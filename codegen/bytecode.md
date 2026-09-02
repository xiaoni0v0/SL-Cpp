# 字节码与虚拟机设计

`codegen/`（AST → `Code`）和 `executor/`（跑 `Code`）之间的实现契约，不是语言规范，涉及语义处以
`SL.md` 为准。

## 整体模型

- 帧栈。帧分三种： **`ByteCodeFrame`** 执行一份 `Code`； **`NativeFrame`** 执行一段 C++ 逻辑（内置函数、
  运算符分派、`import` 加载算法等）； **`EvalFrame`** 是 `eval` 专用的第三种帧，见下。三者共同实现
  `Frame` 这个纯接口：推进一步、接收子帧正常返回的值、接收沿栈传播的异常。主循环只认这个接口。
- 一个 `Code` 是一个 `ByteCodeFrame` 的边界：一份文件、一个函数体、一个类体各一个 `Code`。
- 任何帧结束时把结果交给下面那帧。压帧只换栈顶， **主循环不递归**。

**硬约束：C++ 调用栈深度不得随 SL 帧栈深度增长。** 任何可能回调 SL 代码的地方一律压 `NativeFrame`、
交回主循环，禁止在 C++ 层递归跑一轮子循环。例：`a == b` 压一个 `NativeFrame`，它再压
`type(a).__op_eq__(a, b)` 的调用帧，结果交回后据此决定返回还是继续试 `__rop_eq__`/`__op_cmp__`/身份
兜底。同类还有：属性读写删的描述器、真值 `__bool__`、迭代协议、`C(x)` 的 `__new__`+`__init__`、
`import` 逐段加载、内置函数回调用户函数。纯 C++、不回调 SL 的实现直接原地算完，不压 `NativeFrame`
（`1 + 2` 查到 int 的内置 `__op_add__` 直接算）。

**同一条约束决定了异常传播也不能靠 C++ 原生异常跨帧展开**：SL 帧是 `frames_` 里的堆对象，不是嵌套的
C++ 调用记录，C++ 的自动栈展开够不到别的 SL 帧。跨帧传播必须是主循环手写驱动的显式算法，见「异常与
finally」。

**局部变量走字典查找，不编译成槽位下标**：`_L` 恒为真字典，`eval` 能往任意帧现插代码。

## 指令编码

一条指令固定 2 字节：高字节操作码、低字节参数，无参指令塞 0。字节码即 `std::vector<std::uint16_t>`，
计数器是它的下标。跳转目标一律绝对、以指令为单位。

**`EXTENDED_ARG`**：前缀 `EXTENDED_ARG a` 把待用参数累积成 `(已累积 << 8) | a`，可连用最多 4 条，只对
紧随其后那条真指令有效。加前缀会让后面指令下标右移、可能又逼出新前缀，codegen 先产出带标签的伪指令流，
反复扫描到不再变化为止（前缀只增不减，必收敛）。

## `Frame` 的三种实现

`Frame` 不存数据字段，只是接口：推进一步、接收子帧返回值、接收沿栈传播的异常。

### `ByteCodeFrame`

| 字段             | 内容                                                                                      |
|------------------|-------------------------------------------------------------------------------------------|
| `code_`          | 正在执行的 `Code`（不拥有）                                                               |
| `pc_`            | 指令计数器                                                                                |
| `stack_`         | 操作数栈，容量按 `Code` 的栈深上限一次分配                                                |
| `blocks_`        | 块栈：异常处理器的展开信息，见「异常与 finally」                                          |
| `locals_`        | 局部字典引用，即 `_L`。帧死亡时置为 `None`（不是清空、不是换对象），判活看它是否为 `None` |
| `globals_frame_` | 所属全局帧。全局帧指向自己，**帧靠 `globals_frame_ == this` 自判是不是全局帧**            |
| `global_marks_`  | global 标记集                                                                             |
| `owner_`         | 建立这一帧的东西：函数对象 / 类构建器 / 模块对象                                          |

`owner_` 提供三样东西，不用再开字段： **定义帧**（引用捕获据此路由）； **`RETURN_VALUE` 的收尾**（函数
对象 → 类型检查后交给下面那帧；类构建器 → 收属性、算 MRO/`__abstractmethods__`、造类对象；模块对象 → 记
`__return__` 或当退出码）；返回类型注解、形参注解（`TypeVar` 检查用）。

**引用捕获名集合、值捕获名字表都不在帧上，在 `Code` 上**（见下），查自己的 `code_` 即可。

### `NativeFrame`

没有通用字段——每个原生操作各自定义子类，只装自己需要的状态（"`==` 的分派"记 `lhs`、`rhs`、试到第几
步）。不认识 SL 名字/作用域，没有名字环境；下一步做什么、返回值存哪、异常要不要就地处理，都由子类自己
决定。这是"字节码帧"与"原生帧"唯二的区别：前者的执行状态和名字环境通用、由 `Code` 驱动；后者是手写的 专用
C++ 状态机。

### `EvalFrame`

执行状态跟 `ByteCodeFrame` 一样（`code_`/`pc_`/`stack_`/`blocks_`，`code_` 是编译出的单条表达式），但
没有自己的名字环境——`locals_`/`globals_frame_`/`global_marks_`、引用捕获集合的查询，全部转发给
`target_`（`eval` 所在的调用帧）。`owner_` 恒为空：执行完直接把栈顶的值交给下面那帧。

这是"`Code` 边界 = 帧边界"的唯一例外：`eval` 有自己的执行状态，但名字读写、`global`、引用捕获全部落在
`target_` 上，跟没压新帧效果一致。

## `Code` 存什么

| 字段           | 内容                                                                                                   |
|----------------|--------------------------------------------------------------------------------------------------------|
| 字节码         | 见上                                                                                                   |
| 常量表         | 字面量对象、名字元组（`CALL_KW` 用）、**嵌套 `Code`**（函数体/类体）                                   |
| 名字表         | 标识符、属性名、`import` 的点分名字                                                                    |
| 栈深上限       | 编译期算出，一次性分配操作数栈                                                                         |
| 引用捕获名集合 | 函数体/类体才有，必须持久保留——运行期每次按标识符读写都要查                                            |
| 值捕获名字表   | 函数体/类体才有，按声明顺序对应 `MAKE_FUNC`/`MAKE_CLASS` 弹的那份捕获值元组。函数调用时随实参写进 `_L` |
|                | （跟默认值走同一套注入机制，但不参与实参匹配）；类体是新建帧后直接写入                                 |
| 形参形状       | 函数体才有：四段形参各自的名字表、哪些有默认值/注解、`*args`/`**kwargs` 的名字。实参绑定算法吃它       |
| 行位置表       | 指令下标 → 源码行列 + 来源标识（文件名 / `<eval>`）。纯为报错，非规范                                  |

**不进 `Code`**：装饰器（建立完对象之后的独立步骤，是外层 `Code` 里的普通调用）；形参默认值、各类注解、
`doc`、函数名（外层作用域、建立时求值，挂在函数对象上，`Code` 只需知道"有没有"）；容器字面量的 **值**
（`(1, 2)`、`[1]`、`{1: 2}` 一律运行期构造，否则两次求值拿到同一个对象，`is` 的结果就变了；decimal
字面量例外，可以进，按字符串精确构造不受上下文影响）。

`Code` 对 SL 不可见，不通过 `getattr`/`attrs()` 暴露。

## 栈约定

**每个"编译一个表达式节点"的 codegen 函数带一个 `want_value: bool` 参数，不变量是：编译完这个节点，
操作数栈相对编译前恰好增加 `want_value ? 1 : 0`。** `want_value=false` 时节点自己判断能不能干脆不产出
（赋值、`and`/`or`、`if` 缺分支要补的 `None`……这些"产出"本来就是 codegen 自己加的，不加就是了）；不能
不产出的（`CALL`、`BINARY_OP`、`LOAD_NAME` 这类，产出是指令本身固有的行为），编译完照常补 `POP_TOP`。
判断标准跟折叠器"被丢弃的部分本来就不会被求值就能丢"是同一条：只能省 codegen 自己加的垫值步骤，不能省
任何有副作用/可能抛异常的求值。

`want_value` 往子节点传不是原样转发，按节点语义各自决定：`Compound`/`Program` 的非末尾语句恒 `false`；
`for`/`while` 的 `init`/`inc` 恒 `false`；`and`/`or` 左操作数要不要留值取决于 **整个节点自己收到的**
`want_value`，不是简单继承（见下面 codegen 模式）。`del`/`global`/`break`/`continue` 这类值恒为 `None`
的构造，`want_value=true` 才补 `LOAD_COMMON` 取 `None`。

codegen 必须按实际传下去的 `want_value` 静态跟踪栈深（算栈深上限、`break`/`continue` 裁栈都要用），两
条路径算出来的深度不一样。

## 指令表

参数写作 `n`/`i`/`t`：`n` 是个数或表下标，`t` 是跳转目标。「前」「后」两列是执行前后的操作数栈，最右 是栈顶，
`…` 是不受影响的其余部分。

### 栈与常量

| 指令             | 前             | 后             | 行为                                             |
|------------------|----------------|----------------|--------------------------------------------------|
| `EXTENDED_ARG a` | `…`            | `…`            | 给下一条指令的参数补高 8 位，可连用              |
| `POP_TOP`        | `… x`          | `…`            | 弹掉栈顶                                         |
| `COPY i`         | `… xᵢ … x₁`    | `… xᵢ … x₁ xᵢ` | 复制从栈顶数第 `i` 项（`i=1` 即栈顶）压栈        |
| `INSERT n`       | `… xₙ … x₂ x₁` | `… x₁ xₙ … x₂` | 弹出栈顶，插回到深度 `n`（`n=2` 即交换栈顶两项） |
| `LOAD_CONST n`   | `…`            | `… c`          | 压常量表第 `n` 项                                |
| `LOAD_COMMON n`  | `…`            | `… v`          | 压全局共享表第 `n` 项，见下                      |

`LOAD_COMMON` 取的是一张 **全局共享、不属于任何单个 `Code`** 的小表：`None`/`True`/`False`、`import`
调用形态背后的函数对象（见「调用与建立」）——极多份 `Code` 反复用到，没必要各自常量表都存一份。命名上 特意不叫
`LOAD_COMMON_CONST`，跟 `LOAD_CONST` 是两张互不相干的表，`n` 不能混用。

**`COPY`/`INSERT` 缺一不可，`INSERT` 去不掉**：`COPY` 只能在栈顶新增副本，不能把已在栈上的项挪到更深
位置，这种下沉只能靠 `INSERT`（等价旋转，`n=2` 即交换）。三处真用到：① 赋值/复合赋值的留值——只在
`want_value=true` 时需要，`false` 时直接省掉；② 链式比较——不管整条链要不要值，前一段的右操作数都必须
留给下一段当左操作数，`want_value` 帮不上忙；③ `for` 的 `$$` 收集模式——`UNPACK 2` 给出的顺序跟
`DICT_PUT` 要的差一次交换（见下）。②③ 是纯操作数重排，`COPY` 换不掉。

### 名字

"按作用域规则"：名字在 global 标记集里作用于 `_G`，是引用捕获就转发到 **定义帧**（定义帧局部字典已是
`None` 则 `NameError`），否则作用于 `_L`；读取时 `_L` → `_G` → 内置表逐级退回。

| 指令                 | 前    | 后               | 行为                                                                                                |
|----------------------|-------|------------------|-----------------------------------------------------------------------------------------------------|
| `LOAD_NAME n`        | `…`   | `… v`            | 按作用域规则读名字表第 `n` 项，查不到 `NameError`                                                   |
| `LOAD_NAME_STRICT n` | `…`   | `… v`            | 复合赋值读旧值专用：不在 global 标记集时**只查 `_L`**，查不到 `NameError`，保证读写落在同一帧       |
| `STORE_NAME n`       | `… v` | `…`              | 弹栈顶，按作用域规则写                                                                              |
| `DELETE_NAME n`      | `…`   | `…`              | 按作用域规则删，不存在 `NameError`                                                                  |
| `DECLARE_GLOBAL n`   | `…`   | `…`              | `global identifier`：是引用捕获则 `NameError`；已在标记集则跳过；否则删 `_L` 同名项、名字加进标记集 |
| `LOAD_GL a`          | `…`   | `… _L` 或 `… _G` | `a=0` 压 `_L`（不是视图/快照），`a=1` 压 `_G`                                                       |

### 属性与索引

| 指令            | 前                  | 后    | 行为                         |
|-----------------|---------------------|-------|------------------------------|
| `GET_ATTR n`    | `… obj`             | `… v` | `obj` → 属性值               |
| `SET_ATTR n`    | `… obj val`         | `…`   | 写属性                       |
| `DELETE_ATTR n` | `… obj`             | `…`   | 删属性                       |
| `GET_INDEX n`   | `… obj a₁ … aₙ`     | `… v` | 走 `__op_get_index__`        |
| `SET_INDEX n`   | `… obj a₁ … aₙ val` | `…`   | 走 `__op_set_index__`        |
| `GET_INDEX_EX`  | `… obj args`        | `… v` | `args` 是下标已打包好的 list |
| `SET_INDEX_EX`  | `… obj args val`    | `…`   | 同上                         |

`GET`/`SET` 前缀标记"走对象自己的协议、可能回调 SL 代码"（描述器、`__op_*__`），跟直接读写帧自身状态 的
`LOAD`/`STORE`（名字/常量表）区分开。`*expr` 合法出现在索引里（`x[*a, b]`），跟调用一样：一旦有
展开参数个数就不是编译期常数，得先拼出 `args` list 再交 `_EX` 版本。索引没有 `**` 展开（`SL.md` 只准
`**` 出现在字典字面量和函数调用），`_EX` 因此不需要 `kwargs`，跟 `CALL_EX` 不对称是故意的。

### 运算符

| 指令           | 前      | 后    | 行为                                                                                                          |
|----------------|---------|-------|---------------------------------------------------------------------------------------------------------------|
| `UNARY_OP op`  | `… x`   | `… v` | `op` 选 `+` `-` `~` `not` `?` `!`                                                                             |
| `BINARY_OP op` | `… x y` | `… v` | `op` 选全部二元运算符，含比较、`in`、`is`、`..`；正/反向方法、`__op_cmp__` 回退、`==`/`!=` 身份兜底都在实现里 |

`not`/`is`（含链）/比较（含链式）本身仍各自是一次 `UNARY_OP`/`BINARY_OP`——"链"不是新语义，只是同一个
运算符连续应用多次。`not` 是唯一不经过 MRO 分派的选项：不可重载，固定是"取真值（可能因 `__bool__` 被
重载而回调 SL）再取反"。真正不落在这两条指令上的只有 `and`/`or`——走 `JUMP_IF_FALSE` 短路跳转（见下面
codegen 模式）；链式比较/`is` 链靠 `COPY`/`INSERT` 把操作数喂给两次 `BINARY_OP`，同样见下。

### 容器

| 指令                                       | 前                                       | 后                | 行为                                                              |
|--------------------------------------------|------------------------------------------|-------------------|-------------------------------------------------------------------|
| `MAKE_TUPLE n`/`MAKE_LIST n`/`MAKE_DICT n` | `… v₁ … vₙ`（dict 是 `… k₁ v₁ … kₙ vₙ`） | `… c`             | 弹 `n`（dict 弹 `2n`）项建容器。无展开项时用这组                  |
| `LIST_APPEND`                              | `… lst x`                                | `… lst`           | 追加到下面的 list                                                 |
| `LIST_EXTEND`                              | `… lst x`                                | `… lst`           | 要求可迭代（否则 `TypeError`），元素依次追加                      |
| `DICT_PUT`                                 | `… d k v`                                | `… d`             | 写入下面的 dict                                                   |
| `DICT_MERGE`                               | `… d x`                                  | `… d`             | 要求满足映射协议（否则 `TypeError`），各项并入                    |
| `LIST_TO_TUPLE`                            | `… lst`                                  | `… tup`           | list 换成 tuple                                                   |
| `UNPACK n`                                 | `… x`                                    | `… vₙ … v₁`       | 要求可迭代，**逆序**压入使第一个元素在栈顶；个数不对 `ValueError` |
| `UNPACK_EX a`                              | `… x`                                    | `… vₖ … *lv … v₁` | 带 `*lv` 的解构，见下                                             |

带 `*`/`**` 展开的容器字面量走 `MAKE_LIST 0`/`MAKE_DICT 0` 加逐项 append/extend/put/merge，元组最后补
`LIST_TO_TUPLE`；收集模式的 `for` 复用同一组指令。

**`UNPACK_EX a` 编码**：`a` 是一次 `EXTENDED_ARG` 累积出的 16 位值，高 8 位（`EXTENDED_ARG` 自己那字节）
是星号后项数，低 8 位（`UNPACK_EX` 自己那字节）是星号前项数。例：`(a, b, *rest, c) = e` 编译成
`EXTENDED_ARG 1` `UNPACK_EX 2`。每侧最多 255 项—— **这条上限必须在语义检查阶段挡住**（
`SemanticChecker`
给带 `*lv` 的解构加检查，超限 `SyntaxError`，`SL.md` 补一句），不能留到 codegen 才发现打包不进两字节； 跟
CPython 对 star-unpacking 的处理一致，是真实语言限制不是内部实现细节。codegen 到手的 AST 已保证在 限内。

### 跳转

| 指令              | 前     | 后                          | 行为                                                   |
|-------------------|--------|-----------------------------|--------------------------------------------------------|
| `JUMP t`          | `…`    | `…`                         | 无条件跳                                               |
| `TO_BOOL`         | `… x`  | `… b`                       | `type(x).__bool__(x)` 的结果；不是 bool 则 `TypeError` |
| `JUMP_IF_FALSE t` | `… b`  | `…`                         | 弹栈顶，`b` 假则跳                                     |
| `GET_ITER`        | `… x`  | `… it`                      | 换成迭代器，不满足可迭代协议 `TypeError`               |
| `FOR_ITER t`      | `… it` | `… it v` 或 `…`（耗尽，跳） | 取到元素压栈；耗尽弹掉迭代器并跳 `t`                   |

**`JUMP_IF_FALSE` 只认 bool，自己不做真值转换**：转换单独拆成 `TO_BOOL`，调用方保证操作数已经是 bool。
`if`/`while`/`for` 的 `cond`、`and`/`or`、链式比较每一段都是"先 `TO_BOOL` 再 `JUMP_IF_FALSE`"——比较
运算符不保证返回 bool（用户重载的 `__op_lt__` 可以返回别的类型），`SL.md` 只规定链式比较看"真值"。
`CHECK_EXC_MATCH` 是例外：`isinstance` 是内置判定不走用户重载，结果恒为真 bool，不需要垫 `TO_BOOL`。

**不需要 `JUMP_IF_TRUE`**：`__bool__` 分派只发生一次，`JUMP_IF_FALSE`/假想中的 `JUMP_IF_TRUE` 都只调
一次，完全对称。唯一差别是往哪边加一条无条件 `JUMP`，而这已经是 `or` 的 codegen 自己在做的事。

### 异常与 finally

**C++ 异常只用来把一次"推进一步"内部的错误带出来，不允许跨过这个边界继续往外抛**：主循环每次调用
`Frame` 的"推进一步"都包一层 `catch`——`step()` 内部嵌套多少层纯 C++ 调用（查 MRO、类型转换……）都
正常展开，不对应任何 SL 帧；一旦越过这层 `catch`，就转成"有个 SL 异常对象在传播"的显式状态，交给下面
手写的算法，不指望 C++ 自己继续 `catch` 到更外层（SL 帧之间没有嵌套的 C++ 调用记录）。跟前端
`SyntaxError`/`InternalError` 是同一种用法（内部传错误，不逃出公开入口），但 **运行期不能照抄"一个 SL
概念一个 C++ 类"**：运行期异常是 SL 对象（`SL.md` 4.2 的内置类实例，用户还能派生子类），种类穷举不完， 应该用
**一个统一的 C++ 包装类型**装一个指向 SL 异常对象的引用当"信封"，具体是哪种异常看信封里那个对象 自己的
`type()`。

`return`/`break`/`continue` 不走这条路——纯粹的 PC 跳转（`CALL_FINALLY`/`END_FINALLY`），编译期就确定
目标和要不要经过 `finally`，不需要运行期传播。

块栈一项记 `{处理器目标, 进块时的操作数栈深}`。传播时：弹出最近一项，操作数栈裁到记录的深度，压入异常
对象，跳到处理器；块栈空了就弹帧、往下一帧传播——这是主循环里的显式循环，不是 C++ 栈的自动展开。传到
`NativeFrame` 时给它一次收尾机会（如 `import` 失败撤掉半成品缓存），可以就地处理也可以继续往外传。

| 指令                               | 前               | 后              | 行为                                                                                         |
|------------------------------------|------------------|-----------------|----------------------------------------------------------------------------------------------|
| `SETUP_EXCEPT t`/`SETUP_FINALLY t` | `…`              | `…`（只压块栈） | 压一项块                                                                                     |
| `POP_BLOCK`                        | `…`              | `…`             | 弹一项块（正常路径走完 try 体）                                                              |
| `CALL_FINALLY t`                   | `…`              | `… r`           | "返回地址" `r` 压栈并跳到 `t`；正常完成、`return`/`break`/`continue` 经过 `finally` 时走这条 |
| `END_FINALLY`                      | `… r` 或 `… exc` | `…`             | 是返回地址就跳回去；是异常对象就继续向外传播                                                 |
| `CHECK_EXC_MATCH`                  | `… exc E`        | `… exc b`       | 弹掉 `E`，压 `isinstance(exc, E)`                                                            |
| `RAISE`                            | `… exc`          | ——（抛出）      | 不是 `BaseException` 实例则 `TypeError`                                                      |
| `RERAISE`                          | `… exc`          | ——（抛出）      | 继续传播，不产生新位置信息                                                                   |

"返回地址"是纯内部整数值，SL 层拿不到，只在 `finally` 体执行期间待在操作数栈上。

### 调用与建立

| 指令           | 前                                         | 后         | 行为                                                                                                                      |
|----------------|--------------------------------------------|------------|---------------------------------------------------------------------------------------------------------------------------|
| `CALL n`       | `… f a₁ … aₙ`                              | `… v`      | 压被调对象的帧（SL 函数 → `ByteCodeFrame`，内置 → `NativeFrame`）。只有位置实参                                           |
| `CALL_KW n`    | `… f a₁ … aₙ names`                        | `… v`      | 同上，`names` 是常量表里的名字元组，末尾 `len(names)` 个实参按名字传                                                      |
| `CALL_EX`      | `… f args kwargs`                          | `… v`      | 同上，`*`/`**` 展开时用                                                                                                   |
| `MAKE_FUNC`    | `… captures params ret_type doc code name` | `… f`      | 弹 6 项建函数对象，见下                                                                                                   |
| `MAKE_CLASS`   | `… captures bases doc code name`           | `… c`      | 弹 5 项，新建局部帧执行类体，见下                                                                                         |
| `RETURN_VALUE` | `… v`                                      | ——（弹帧） | 弹栈顶为值，弹帧，按 `owner_` 种类收尾                                                                                    |
| `IMPORT n`     | `…`                                        | `… m`      | 关键字形态 `import a.b.c`：名字表第 `n` 项是完整点分名，压 `NativeFrame` 跑加载算法，压入**第一段**模块对象               |
| `EVAL`         | `… args kwargs`                            | `… v`      | 绑出 `code`（失败 `DispatchError`，非 str `TypeError`），解析成恰好一条表达式（否则 `SyntaxError`），编译，压 `EvalFrame` |

实参绑定算法（槽位填充、`*args`/`**kwargs` 收集、类型检查、函数族逐个试）不摊成字节码，在 `CALL` 系列
指令实现里。`EVAL` 编译 `code` 时交给语义检查的 `in_local_scope` 取
`target_.globals_frame_ != target_`。

**没有 `IMPORT_CALL`**：调用形态 `import(expr, kwarg=v, ...)` 的实参形状本来就跟普通调用一致，没必要
另写绑定逻辑——加载算法包成一个内部函数对象，`LOAD_COMMON` 取它接一次普通 `CALL`/`CALL_KW`/`CALL_EX`
即可。这个对象只出现在这一种 codegen 产出的字节码里，`import` 仍是关键字，SL 层拿不到它。

**`eval` 不能走这条路**：`import` 是参数绑完才干活，能塞进普通内置函数；`eval` 得先拿到 `code` 字符串、
现场编译出新 `Code`（可能 `SyntaxError`），编译要用的 `in_local_scope` 来自 **发起调用的那一帧**——这
信息在参数绑定阶段不存在，`CALL` 没有"先编译一份 Code 再决定压哪种帧"这一步。更根本的是：`eval` 关键
字化就是为了让"这份 `Code` 有没有 `eval` 点"纯静态可判定（给未来局部变量槽位化铺路，见
`.ai/context.md`）——若也编译成 `LOAD_COMMON`+`CALL`，字节码层面就分不清普通调用和 `eval` 调用了。

## 关键构造的 codegen 模式

**`and`/`or`**：只用消费型的 `JUMP_IF_FALSE`。`want_value=true` 时先复制一份操作数走 `TO_BOOL` 测试，
原件留在栈上不受影响；短路时原件就是结果，不短路就弹掉原件再求右操作数：

```
a and b (want=true) : <a> COPY 1 TO_BOOL JUMP_IF_FALSE end POP_TOP <b(want=true)> end:
a or b  (want=true) : <a> COPY 1 TO_BOOL JUMP_IF_FALSE rhs JUMP end   rhs: POP_TOP <b(want=true)>   end:
```

`want_value=false` 时不需要保留 `a`，只要真值，`TO_BOOL` 直接吃掉 `a`；右操作数继承 `want_value=false`：

```
a and b (want=false) : <a> TO_BOOL JUMP_IF_FALSE end <b(want=false)> end:
a or b  (want=false) : <a> TO_BOOL JUMP_IF_FALSE rhs JUMP end   rhs: <b(want=false)>   end:
```

`or` 只是把"假才跳"倒过来加一条无条件 `JUMP`，不需要"真才跳"的指令。

**链式比较 `a < b < c < d`**（`is` 链同理，`op` 换 `is`）：中间段把右操作数复制一份垫到左操作数下面 再比较，
`t` 不保证是 bool（用户重载的比较方法可以返回别的类型），测试前先垫 `TO_BOOL`（只转换测试用 的副本，`t`
原件不受影响）； **所有中间段的失败分支共享同一个 `fail`**——清理动作都是同一句"交换、弹掉
左操作数，留下比较结果"：

```
<a> <b>
COPY 1 INSERT 3 BINARY_OP Lt   ; 栈: b t   (t = a < b)
COPY 1 TO_BOOL JUMP_IF_FALSE fail
POP_TOP                         ; 真：留 b，继续
<c>
COPY 1 INSERT 3 BINARY_OP Lt   ; 栈: c t   (t = b < c)
COPY 1 TO_BOOL JUMP_IF_FALSE fail
POP_TOP                         ; 真：留 c，继续
<d> BINARY_OP Lt                ; 末段：不用再留操作数
JUMP end
fail: INSERT 2 POP_TOP          ; [y, t] 交换成 [t, y] 再弹掉 y
end:
```

两个操作符（`a<b<c`）只有一段中间段，三个以上重复中间那段即可。

**赋值表达式的值**：`want_value=true` 才垫一份再让指令弹光；`false`（赋值当独立语句，最常见）直接省 掉
`COPY`/`INSERT`：

```
x = e         (want=true)  : <e> COPY 1 STORE_NAME x
x = e         (want=false) : <e> STORE_NAME x
x.a = e       (want=true)  : <x> <e> COPY 1 INSERT 3 SET_ATTR a
x.a = e       (want=false) : <x> <e> SET_ATTR a
x[i] = e      (want=true)  : <x> <i> <e> COPY 1 INSERT 4 SET_INDEX 1
x[i] = e      (want=false) : <x> <i> <e> SET_INDEX 1
(a, b) = e    (want=true)  : <e> COPY 1 UNPACK 2 STORE_NAME a STORE_NAME b
(a, b) = e    (want=false) : <e> UNPACK 2 STORE_NAME a STORE_NAME b
```

**复合赋值**：目标只求值一次，靠 `COPY` 复制已在栈上的那份；多下标用 `n+1` 条 `COPY n+1` 整体复制。 同样
`want_value=false` 时省掉留值用的 `COPY`/`INSERT`：

```
x op= e       (want=true)  : LOAD_NAME_STRICT x <e> BINARY_OP op COPY 1 STORE_NAME x
x op= e       (want=false) : LOAD_NAME_STRICT x <e> BINARY_OP op STORE_NAME x
x.a op= e     (want=true)  : <x> COPY 1 GET_ATTR a <e> BINARY_OP op COPY 1 INSERT 3 SET_ATTR a
x.a op= e     (want=false) : <x> COPY 1 GET_ATTR a <e> BINARY_OP op SET_ATTR a
x[i] op= e    (want=true)  : <x> <i> COPY 2 COPY 2 GET_INDEX 1 <e> BINARY_OP op COPY 1 INSERT 4 SET_INDEX 1
x[i] op= e    (want=false) : <x> <i> COPY 2 COPY 2 GET_INDEX 1 <e> BINARY_OP op SET_INDEX 1
```

**循环**：`for`/`while` 步进/迭代两种子模式的骨架，外加收集模式共用的收尾逻辑。

进循环前先压 **结果槽**（计数模式压 `0`，`$` 压空 list，`$$` 压空 dict），整个循环期间待在栈上，结束时
就是整条表达式的值。循环体 `expr` 恒以 `want_value=true` 编译——每一轮的值都要被收尾逻辑读一次：

```
计数模式收尾:
  POP_TOP
count_continue:
  LOAD_CONST 1 BINARY_OP Add
$   收尾: LIST_APPEND
$ * 收尾: LIST_EXTEND
$$  收尾: UNPACK 2 INSERT 2 DICT_PUT   ; UNPACK 2 给出的是 v k，DICT_PUT 要 d k v，INSERT 2 补一次交换
$$ ** 收尾: DICT_MERGE
```

**`continue` 的目标按收集模式分**（`break` 不区分，统一跳 `end:`——`SL.md` 明确 `break` 两种模式都不
计入/不放入，`continue` 只有收集模式不放入、计数模式仍要计入）：计数模式跳到 `count_continue:`（跳过
`POP_TOP`，因为 `continue` 自己的裁栈已经清空了栈上原本要弹的值，直接从加 1 开始）；`$`/`$ *`/`$$`/
`$$ **` 跳过整个收尾块，直接到 `inc:`（步进模式）或 `loop:`（迭代模式）。

步进模式 `for ⟦collect⟧ (init; cond; inc) expr`：`init`/`inc` 恒 `want_value=false`；`cond` 为空按
`SL.md` 视为 `True`，省掉测试直接落入循环体；`continue`（收集模式）落到 `inc:`——`SL.md` 明确"仍会对
`inc` 求值进而对 `cond` 求值"：

```
<init>
loop:
  <cond> TO_BOOL JUMP_IF_FALSE end   ; cond 为空则省掉这行
  <expr>                             ; want_value=true
  <收尾>
inc:
  <inc>
  JUMP loop
end:
```

迭代模式 `for ⟦collect⟧ (iterable ⟦as lvalue⟧) expr`：没有 `inc`，`continue`（收集模式）落到 `loop:`。
`FOR_ITER` 耗尽自己弹迭代器，但 `break` 提前退出时迭代器还留在栈上——是循环体额外压出来的一层，裁栈
时要算上：

```
<iterable> GET_ITER
loop:
  FOR_ITER end                  ; 取到元素压栈；耗尽弹迭代器、跳 end
  <写入 lvalue，弹光；没有 as 就 POP_TOP>
  <expr>                        ; want_value=true
  <收尾>
  JUMP loop
end:
```

**`want_value=false` 时结果槽能不能省**：计数模式能——纯计数，`want_value=false` 时连 `LOAD_CONST 0`
和每轮加 1 都不用发，退化成裸循环。`$`/`$ *`/`$$`/`$$ **` 不能——`UNPACK`/`DICT_PUT` 等收尾指令本身可能
抛 `TypeError`/`ValueError`，跳过就是悄悄吞异常，违反「栈约定」那条判断标准；每轮折叠必须照常做，只有
最后要不要把结果槽交给外层由 `want_value` 决定。

**`break`/`continue` 不用运行期块栈**：跳到哪、裁掉几层栈 codegen 静态就知道，编译成若干 `POP_TOP` +
`JUMP`；隔着 `finally` 时跳之前由内到外补 `CALL_FINALLY`。`return` 同理但不用裁栈（弹帧时整个操作数栈
一起没了），返回值在 `finally` 体执行期间待在栈上不受影响。

**`try`**：

```
SETUP_EXCEPT h    <expr1>   POP_BLOCK   JUMP after
h:  COPY 1 <E1> CHECK_EXC_MATCH JUMP_IF_FALSE next1
    (有 as 就把栈顶异常对象赋给 lvalue，否则 POP_TOP) <expr2> JUMP after
next1: …          RERAISE
after:
```

`finally` 在外面再包一层 `SETUP_FINALLY`：正常路径走完 `POP_BLOCK` + `CALL_FINALLY`，异常路径展开时
直接跳进同一段 `finally` 体，末尾统一 `END_FINALLY` 分派—— **只编译一份**，`finally` 体不可能跳出这段
范围（编译期已查），不用处理跳转穿过它。

**建立函数**：装饰器最先求值，先压装饰器；`Code`/名字是常量放最后压：

```
<deco1> … <decoN>
<各值捕获> MAKE_TUPLE k
<各形参注解/默认值> MAKE_TUPLE m
<返回类型> <doc> LOAD_CONST <Code> LOAD_CONST <名字>
MAKE_FUNC
CALL 1  (由近到远，一个装饰器一条)
COPY 1 STORE_NAME f   (命名函数才有，绑定只发生一次)
```

`MAKE_FUNC` 固定弹 6 项，缺的注解/默认值/名字/`doc` 用 `LOAD_COMMON` 压一个纯内部哨兵值（不能用
`None`，它是合法默认值）。引用捕获不产生任何指令：`MAKE_FUNC` 在当前帧执行，直接抓一份当前帧的引用
挂到新对象上。

**建立类**：形状同上，基类元组代替形参段，弹 5 项。`MAKE_CLASS` 不直接产出类对象——新建局部帧、写入
值捕获、压栈执行类体；类体 `RETURN_VALUE` 收尾时才由 `owner_`（类构建器）完成属性收集、MRO、
`__abstractmethods__`，把类对象压回。装饰器调用在这之后。

**一份 `Code` 可复用多次，但每次"建立"都必须造出全新的函数/类对象**，`is` 比的是对象身份，不能缓存。

## 明确留给以后

- 窥孔优化、超级指令、内联缓存：一概不做。
- 局部变量槽位化：前提是"这份 `Code` 里有没有 `eval` 点"的静态判定。
- 异常表取代块栈：等块栈成为瓶颈再换，对 codegen 之外不可见。
- `RecursionError` 的阈值未定，只需要卡帧栈深度一个指标。
