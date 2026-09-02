# 字节码与虚拟机设计

`codegen/`（AST → `Code`）和 `executor/`（跑 `Code`）之间的实现契约，不是语言规范，涉及语义处以
`SL.md` 为准。

## 整体模型

- 帧栈。
- 帧分三种： **`ByteCodeFrame`** 执行一份 `Code`； **`NativeFrame`** 执行一段 C++ 逻辑（内置函数、运算符
  分派、`import` 加载算法等）； **`EvalFrame`** 是 `eval` 专用的第三种帧，见下。三者共同实现 `Frame`
  这个纯接口：推进一步；接收子帧正常返回的值；接收沿栈传播的异常。主循环只认这个接口，不关心具体是 哪一种帧。
- 一个 `Code` 是一个 `ByteCodeFrame` 的边界：一份文件、一个函数体、一个类体各一个 `Code`。
- 任何帧结束时，把结果交给下面那帧（`Frame` 接口里"接收子帧返回值"那一步）。压帧只换栈顶，
  **主循环不递归**。

**硬约束：C++ 调用栈深度不得随 SL 帧栈深度增长。** 实现里任何可能回调 SL 代码的地方，一律压
`NativeFrame`、交回主循环，禁止在 C++ 层递归跑一轮子循环。例：`a == b` 压一个 `NativeFrame`，它再压
`type(a).__op_eq__(a, b)` 的调用帧；结果交给这个 `NativeFrame`，它据此决定返回还是继续试
`__rop_eq__` / `__op_cmp__` / 身份兜底。同类还有：属性读写删的描述器、真值 `__bool__`、迭代协议、
`C(x)` 的 `__new__` + `__init__`、`import` 逐段加载、内置函数回调用户函数。

纯 C++、不回调 SL 的实现直接原地算完，不压 `NativeFrame`（`1 + 2` 查到 int 的内置 `__op_add__` 就 直接算）。

**同一条约束决定了异常传播不能靠 C++ 原生异常自己跨帧展开**：SL 帧是 `frames_` 里的堆对象，不是嵌套的
C++ 调用记录，C++ 的自动栈展开只够得着"真正嵌套调用出来的"那部分 C++ 栈，够不到别的 SL 帧。跨 SL 帧
的异常传播必须是主循环手写驱动的显式算法，不是甩给 C++ 的 `throw`/`catch` 自动完成，具体机制见「异常 与
finally」。

**局部变量走字典查找，不编译成槽位下标**：`_L` 恒为真字典，`eval` 能往任意帧现插代码。

## 指令编码

一条指令固定 2 字节：高字节操作码、低字节参数，无参指令塞 0。字节码即 `std::vector<std::uint16_t>`，
计数器是它的下标。 **跳转目标一律绝对、以指令为单位**（即计数器的值），不是字节偏移。

**`EXTENDED_ARG`**：前缀 `EXTENDED_ARG a` 把待用参数累积成 `(已累积 << 8) | a`，可连用，最多 4 条，
只对紧随其后的那条真指令有效。加前缀会让后面所有指令下标右移、可能又逼出新前缀，所以 codegen 先产出
带标签的伪指令流，再反复扫描到不再变化为止（前缀只增不减，必收敛）。

## `Frame` 的三种实现

`Frame` 本身不存数据字段，只是一个接口： **推进一步**、 **接收子帧正常返回的值**、 **接收沿栈传播的
异常**。三种具体帧各自决定这三件事怎么做，`Frame` 一律靠虚派发选中对应实现。

### `ByteCodeFrame`：跑一份 `Code`

| 字段             | 内容                                                                                            |
|------------------|-------------------------------------------------------------------------------------------------|
| `code_`          | 正在执行的 `Code`（不拥有，由函数对象/模块持有）                                                |
| `pc_`            | 指令计数器，单位是指令                                                                          |
| `stack_`         | 操作数栈，容量按 `Code` 里的栈深上限一次分配                                                    |
| `blocks_`        | 块栈：异常处理器的展开信息，见下面 try/finally 一节                                             |
| `locals_`        | 局部字典对象的引用，即 `_L`。帧死亡时置为 `None`（不是清空、不是换对象），判活看它是否为 `None` |
| `globals_frame_` | 所属全局帧。全局帧指向自己，**帧靠 `globals_frame_ == this` 自判是不是全局帧**                  |
| `global_marks_`  | global 标记集，一组名字                                                                         |
| `owner_`         | 建立这一帧的东西：函数对象 / 类构建器 / 模块对象                                                |

"推进一步"＝执行下一条指令；"接收子帧返回值"＝压进 `stack_`；"接收异常"＝按 `blocks_` 找处理器，
块栈空了就继续往下一帧传播。

`owner_` 提供三样东西，不用再开字段：

- **定义帧**（函数对象和类构建器都记着），引用捕获据此路由；
- **`RETURN_VALUE` 的收尾**按其种类分派：函数对象 → 返回值类型检查后把值交给下面那帧；类构建器 →
  丢弃值、收属性、算 MRO/`__abstractmethods__`、造出类对象再交给下面那帧；模块对象 → 记进
  `__return__` 或当退出码；
- 返回类型注解、形参注解（`TypeVar` 一致性检查用）。

**引用捕获名集合不在帧上，在 `Code` 上**，查自己的 `code_` 即可。

### `NativeFrame`：跑一段 C++ 逻辑

没有通用字段——每个原生操作各自定义一个 `NativeFrame` 子类，只装它自己需要的状态（比如"`==` 的分派"
这个子类记 `lhs`、`rhs`、试到第几步）。不认识 SL 的名字/作用域，因此没有名字环境；下一步做什么由具体
子类的控制流决定，不需要通用的 `pc_`；接的子帧返回值该存到哪个字段、异常要不要就地处理，也由子类自己
决定。这正是"字节码帧"与"原生帧"唯二的区别：前者的执行状态和名字环境是通用的、由 `Code` 驱动；后者
是手写的、专用的 C++ 状态机。

### `EvalFrame`：`eval` 专用

执行状态字段跟 `ByteCodeFrame` 一样（`code_`/`pc_`/`stack_`/`blocks_`，`code_` 是编译出的单条表达式），
但没有自己的名字环境——`locals_`/`globals_frame_`/`global_marks_`，以及引用捕获集合的查询，全部转发 给
`target_`（`eval` 所在的调用帧）。`owner_` 恒为空：执行完直接把栈顶的值交给下面那帧，没有类型检查、 没有
`__return__` 记录。

这就是"`Code` 边界 = 帧边界"的唯一例外：`eval` 有自己的执行状态（一条独立的指令流、独立的操作数栈），
但名字读写、`global` 声明、引用捕获，全部落在 `target_` 上，跟"没有压新帧"效果一致。

## `Code` 存什么

| 字段           | 内容                                                                                                     |
|----------------|----------------------------------------------------------------------------------------------------------|
| 字节码         | 见上                                                                                                     |
| 常量表         | 字面量对象、名字字符串组成的元组（`CALL_KW` 用）、**嵌套 `Code`**（函数体/类体）                         |
| 名字表         | 标识符、属性名、`import` 的点分名字                                                                      |
| 栈深上限       | 编译期算出，用来一次性分配操作数栈                                                                       |
| 引用捕获名集合 | 函数体/类体 `Code` 才有。运行期每次按标识符读写都要查它，必须持久保留                                    |
| 形参形状       | 函数体 `Code` 才有：四段形参各自的名字表、哪些有默认值/注解、`*args`/`**kwargs` 的名字。实参绑定算法吃它 |
| 行位置表       | 指令下标 → 源码行列，外加来源标识（文件名 / `<eval>`）。纯为报错，非规范                                 |

**不进 `Code`**：

- 装饰器——建立完对象之后的独立步骤，是外层 `Code` 里的普通调用指令；
- 形参默认值、各类注解、`doc`、函数名——外层作用域、建立时求值，挂在函数对象上，`Code` 只需知道
  "有没有"，而这已经在形参形状里；
- 值捕获——同上，调用时随实参写进 `_L`；
- 容器字面量的 **值**——`(1, 2)`、`[1]`、`{1: 2}` 一律运行期构造，否则两次求值拿到同一个对象，`is` 的
  结果就变了。decimal 字面量则可以进：按字符串精确构造，不受上下文 `prec`/`rounding` 影响。

`Code` 对 SL 不可见，不通过 `getattr`/`attrs()` 暴露。

## 栈约定

**每个"编译一个表达式节点"的 codegen 函数都带一个 `want_value: bool` 参数，不变量是：编译完这个节点，
操作数栈相对编译前恰好增加 `want_value ? 1 : 0`。** 不是无脑地"先都留一个值、外层不要再 `POP_TOP`"：
`want_value=false` 时，节点自己判断能不能干脆不产出（赋值、`and`/`or`、`if` 缺分支要补的 `None`……
这些"产出"本来就是 codegen 自己加的，不加就是了，省掉 `COPY`/`LOAD_COMMON` 这类垫值指令）；不能不产出 的（
`CALL`、`BINARY_OP`、`LOAD_NAME` 这类——产出是指令本身固有的行为，没有"别产出"这个开关），编译完 照常补一条
`POP_TOP`。判断"能不能不产出"的标准跟折叠器"被丢弃的部分本来就不会被求值就能丢"是同一条： 省掉的只能是
codegen 自己额外加的垫值步骤，不能省掉任何有副作用/可能抛异常的求值本身。

`want_value` 怎么往子节点传不是无脑原样转发，每种节点按自己的语义定：`Compound`/`Program` 的非末尾
语句恒为 `false`，末尾语句/整体值继承外层给这个节点的 `want_value`；`for`/`while` 的 `init`/`inc`
恒为 `false`（"对其求值实为跳过"）；`and`/`or` 左操作数要不要垫一份留到短路分支，取决于 **整个
`and`/`or` 节点自己收到的** `want_value`，不是简单地也传 `true` 给左操作数（具体见下面 codegen 模式）。

`del`/`global`/`break`/`continue` 这类值恒为 `None` 的纯副作用构造，`want_value=true` 时才补
`LOAD_COMMON`（取 `None`，不是 `LOAD_CONST`——`None` 走全局共享表，理由见「栈与常量」），`false` 时 什么也不用补。

codegen 必须按实际传下去的 `want_value` 静态跟踪每个点的栈深：算栈深上限要用，`break`/`continue`
的裁栈也要用——两条路径（想要值/不想要值）算出来的深度不一样，不能只按其中一条算。

## 指令表

参数写作 `n`/`i`/`t`：`n` 是个数或表下标，`t` 是跳转目标。「前」「后」两列是执行前后的操作数栈，最 右边是栈顶，
`…` 代表不受影响的其余部分；字母含义看「行为」列。

### 栈与常量

| 指令             | 前             | 后             | 行为                                                    |
|------------------|----------------|----------------|---------------------------------------------------------|
| `EXTENDED_ARG a` | `…`            | `…`            | 给下一条指令的参数补高 8 位，可连用                     |
| `POP_TOP`        | `… x`          | `…`            | 弹掉栈顶                                                |
| `COPY i`         | `… xᵢ … x₁`    | `… xᵢ … x₁ xᵢ` | 把从栈顶数第 `i` 项（`i=1` 即栈顶）复制一份压栈         |
| `INSERT n`       | `… xₙ … x₂ x₁` | `… x₁ xₙ … x₂` | 弹出栈顶，插回到深度 `n` 的位置（`n=2` 即交换栈顶两项） |
| `LOAD_CONST n`   | `…`            | `… c`          | 压常量表第 `n` 项                                       |
| `LOAD_COMMON n`  | `…`            | `… v`          | 压全局共享表第 `n` 项，见下                             |

`LOAD_COMMON` 取的是一张 **全局共享、不属于任何单个 `Code`** 的小表：`None`/`True`/`False`、`import`
调用形态背后的函数对象（见「调用与建立」）……这些值被极多份 `Code` 反复用到，没必要每份 `Code` 自己的
常量表都存一份——叫 `LOAD_COMMON` 不叫 `LOAD_COMMON_CONST`，就是要跟 `LOAD_CONST` 的"这个常量属于 这份
`Code`"区分开：两个指令的 `n` 是两张完全不同的表，不能混用下标。

**`COPY` 和 `INSERT` 缺一不可，`INSERT` 去不掉**：`COPY` 只能在栈顶新增副本，不能把已经在栈上的项
挪到更深的位置；这种下沉只能靠 `INSERT`（等价于旋转，`n=2` 时就是交换）完成。三处真用到、且互相换不掉：

1. 赋值表达式的值、复合赋值——`want_value=true` 时才用得上（见「栈约定」），`false` 时这两处直接不 需要
   `INSERT`，这是 `want_value` 带来的实打实的省法，但省不掉 `INSERT` 这条指令本身，只是省掉它
   在这两处的出现次数；
2. 链式比较——不管整条链的值要不要，`b`（前一段的右操作数）都必须留到下一段当左操作数用，`want_value`
   在这里帮不上忙；
3. `for` 的 `$$` 收集模式——`UNPACK 2` 按"第一个元素在栈顶"的规则把 `(k, v)` 拆开，跟 `DICT_PUT` 要的
   `d k v` 顺序正好差一次交换，得靠 `INSERT 2` 补上（见下面 codegen 模式）。

第 2、3 条不是"赋值要留值"这种能靠 `want_value` 绕开的场景，是纯粹的操作数重排，`COPY` 替代不了，
`INSERT` 留在指令集里是必要的。

### 名字

"按作用域规则"指：名字在 global 标记集里就作用于 `_G`，是引用捕获就转发到 **定义帧**（定义帧的局部 字典已是
`None` 则 `NameError`），否则作用于 `_L`；读取时 `_L` → `_G` → 内置表逐级退回。

| 指令                 | 前    | 后               | 行为                                                                                                                          |
|----------------------|-------|------------------|-------------------------------------------------------------------------------------------------------------------------------|
| `LOAD_NAME n`        | `…`   | `… v`            | 按作用域规则读名字表第 `n` 项，查不到 `NameError`                                                                             |
| `LOAD_NAME_STRICT n` | `…`   | `… v`            | 复合赋值读旧值专用：不在 global 标记集时**只查 `_L`**、不退回 `_G`/内置表，查不到 `NameError`。保证这次读和随后的写落在同一帧 |
| `STORE_NAME n`       | `… v` | `…`              | 弹栈顶，按作用域规则写                                                                                                        |
| `DELETE_NAME n`      | `…`   | `…`              | 按作用域规则删，不存在 `NameError`                                                                                            |
| `DECLARE_GLOBAL n`   | `…`   | `…`              | `global identifier`：名字是引用捕获则 `NameError`；已在标记集则跳过；否则删掉 `_L` 里的同名项、把名字加进标记集               |
| `LOAD_GL a`          | `…`   | `… _L` 或 `… _G` | `a=0` 压 `_L`（不是视图、不是快照），`a=1` 压 `_G`                                                                            |

### 属性与索引

| 指令            | 前                  | 后    | 行为                                            |
|-----------------|---------------------|-------|-------------------------------------------------|
| `GET_ATTR n`    | `… obj`             | `… v` | `obj` → 属性值                                  |
| `SET_ATTR n`    | `… obj val`         | `…`   | `obj val` → 弹两项，写属性                      |
| `DELETE_ATTR n` | `… obj`             | `…`   | `obj` → 弹掉，删属性                            |
| `GET_INDEX n`   | `… obj a₁ … aₙ`     | `… v` | `obj a1 … an` → 结果，走 `__op_get_index__`     |
| `SET_INDEX n`   | `… obj a₁ … aₙ val` | `…`   | `obj a1 … an val` → 弹光，走 `__op_set_index__` |
| `GET_INDEX_EX`  | `… obj args`        | `… v` | `obj args` → 结果。`args` 是下标已打包好的 list |
| `SET_INDEX_EX`  | `… obj args val`    | `…`   | `obj args val` → 弹光                           |

`*expr` 合法出现在索引里（`x[*a, b]`），跟函数调用一样：只要索引参数里有一个 `*` 展开，参数个数就不是
编译期常数，必须先在栈上拼出 `args` 这个 list 再交给 `_EX` 版本；没有展开的普通索引走上面固定参数个数
的 `GET_INDEX`/`SET_INDEX`。索引没有 `**` 展开（`SL.md` 只允许 `**` 出现在字典字面量和函数调用）， 所以
`_EX` 版本不需要 `kwargs`，跟 `CALL_EX` 不对称是故意的。

### 运算符

| 指令           | 前      | 后    | 行为                                                                                                                                          |
|----------------|---------|-------|-----------------------------------------------------------------------------------------------------------------------------------------------|
| `UNARY_OP op`  | `… x`   | `… v` | `x` → 结果。`op` 选 `+` `-` `~` `not` `?` `!`                                                                                                 |
| `BINARY_OP op` | `… x y` | `… v` | `x y` → 结果。`op` 选全部二元运算符，含六个比较、`in`、`is`、`..`。正向/反向方法、`__op_cmp__` 回退、`==`/`!=` 的身份兜底全在这条指令的实现里 |

`not`、`is`（含 `is` 链）、六个比较（含链式比较）本身仍然各自是一次 `UNARY_OP`/`BINARY_OP`——"链"
不是新语义，只是同一个二元运算符被连续应用多次。`not` 是 `UNARY_OP` 里唯一 **不经过 MRO 分派**的 选择项：
`SL.md` 明确 `not` 不可重载，它的行为固定是"对操作数取真值（这一步可能因为 `__bool__` 被 重载而回调 SL
代码）然后取反"，不需要像 `+x`/`-x`/`~x`/`x?`/`x!` 那样先查 `__op_*__`。真正不落在
`UNARY_OP`/`BINARY_OP` 这两条指令上的只有 `and`/`or`：它们不产出"某个运算符的结果"，而是"保留左值
还是求右值"的控制流分支，走 `JUMP_IF_FALSE` 短路跳转，见下面 codegen 模式；链式比较/`is` 链要的只是
"把同一个操作数喂给两次 `BINARY_OP`"，靠 `COPY`/`INSERT` 拼接，同样见下面 codegen 模式。

### 容器

| 指令                                           | 前                                       | 后                | 行为                                                                                     |
|------------------------------------------------|------------------------------------------|-------------------|------------------------------------------------------------------------------------------|
| `MAKE_TUPLE n` / `MAKE_LIST n` / `MAKE_DICT n` | `… v₁ … vₙ`（dict 是 `… k₁ v₁ … kₙ vₙ`） | `… c`             | 弹 `n` 项（dict 弹 `2n` 项、键值交替）建容器。无展开项时用这组                           |
| `LIST_APPEND`                                  | `… lst x`                                | `… lst`           | 弹栈顶，追加到下面的 list                                                                |
| `LIST_EXTEND`                                  | `… lst x`                                | `… lst`           | 弹栈顶（要求可迭代，否则 `TypeError`），元素依次追加到下面的 list                        |
| `DICT_PUT`                                     | `… d k v`                                | `… d`             | 弹 `k v` 两项写入下面的 dict                                                             |
| `DICT_MERGE`                                   | `… d x`                                  | `… d`             | 弹栈顶（要求满足映射协议，否则 `TypeError`），各项并入下面的 dict                        |
| `LIST_TO_TUPLE`                                | `… lst`                                  | `… tup`           | 把栈顶的 list 换成 tuple                                                                 |
| `UNPACK n`                                     | `… x`                                    | `… vₙ … v₁`       | 弹栈顶（要求可迭代），**逆序**压入各元素使第一个元素在栈顶；个数不是 `n` 则 `ValueError` |
| `UNPACK_EX a`                                  | `… x`                                    | `… vₖ … *lv … v₁` | 带 `*lv` 的解构，见下                                                                    |

带 `*`/`**` 展开的元组/列表/字典字面量走 `MAKE_LIST 0` / `MAKE_DICT 0` 加逐项
append/extend/put/merge，元组最后补 `LIST_TO_TUPLE`；收集模式的 `for` 复用同一组指令。

**`UNPACK_EX a` 的参数编码**：`a` 是一次 `EXTENDED_ARG` 累积出的 16 位值， **高 8 位**是星号后的项数、
**低 8 位**是星号前的项数——跟本指令自己那一字节参数的位置对应：`EXTENDED_ARG (星号后项数)` 在前，
`UNPACK_EX (星号前项数)` 在后，累积规则见前面「指令编码」一节，两半正好各占一字节。例如
`(a, b, *rest, c) = e`（星号前 2 项、星号后 1 项）编译成 `EXTENDED_ARG 1` `UNPACK_EX 2`，累积值
`a = (1 << 8) | 2`。这个编码天然只支持每侧至多 255 项。 **这条上限必须在语义检查阶段就拦下来，不能留到
codegen 发现 参数打包不进两个字节才出错**：`SemanticChecker` 要给带 `*lv` 的解构加一条检查，星号前、星号后的项数
任一超过 255 就当场 `SyntaxError`，`SL.md` 的解构/lvalue 一节要补一句。这不是内部实现细节，是会拒绝
用户代码的真实语言限制——CPython 对 star-unpacking 就是这么处理的（超限直接 `SyntaxError:
too many expressions in star-unpacking assignment`），不是运行时才发现装不下的静默 bug。 codegen 到手的
AST 已经保证在限内，`UNPACK_EX` 自己不用再防这一步。

### 跳转

| 指令              | 前     | 后                          | 行为                                                         |
|-------------------|--------|-----------------------------|--------------------------------------------------------------|
| `JUMP t`          | `…`    | `…`                         | 无条件跳                                                     |
| `TO_BOOL`         | `… x`  | `… b`                       | `x` → `type(x).__bool__(x)` 的结果；不是 bool 则 `TypeError` |
| `JUMP_IF_FALSE t` | `… b`  | `…`                         | 弹栈顶，`b` 假则跳                                           |
| `GET_ITER`        | `… x`  | `… it`                      | 栈顶换成它的迭代器，不满足可迭代协议则 `TypeError`           |
| `FOR_ITER t`      | `… it` | `… it v` 或 `…`（耗尽，跳） | 取到下一个元素就压栈；耗尽则弹掉迭代器并跳 `t`               |

**`JUMP_IF_FALSE` 只认 bool，自己不做真值转换**：真值转换单独拆成 `TO_BOOL`，调用方（codegen）保证 每次
`JUMP_IF_FALSE` 之前操作数已经是 bool。这不是随意拆分——`if`/`while`/`for` 的 `cond`、`and`/`or`、
链式比较的每一段，一律 **先 `TO_BOOL` 再 `JUMP_IF_FALSE`**，因为它们测的东西不一定已经是 bool （`SL.md`
只规定链式比较测"`t` 的真值"，没规定比较运算符必须返回 bool；用户重载的 `__op_lt__` 等完全 可以返回别的类型）。
`CHECK_EXC_MATCH` 是唯一的例外：`isinstance` 是解释器内置判定，不走用户重载， 结果恒为真 bool，它后面的
`JUMP_IF_FALSE` 不需要先垫一条 `TO_BOOL`。

**不需要 `JUMP_IF_TRUE`**：`__bool__` 分派只应该发生一次，`JUMP_IF_FALSE`/假想中的 `JUMP_IF_TRUE`
无论哪个都只会调用一次，两者在这一点上完全对称，没有效率差异。唯一的差别是"该往哪边加一条无条件
`JUMP`"，而这已经是 `or` 的 codegen 模式自己在做的事（见下）——多一条指令换来少一种指令，指令集更小。

### 异常与 finally

**C++ 异常只用来把一次"驱动一步"内部的错误带出来，绝不允许带着跨过这个边界继续往外抛**——主循环每次
调用某个 `Frame` 的"推进一步"，外面都包一层 `catch`：`step()` 内部不管嵌套了多少层纯 C++ 调用（查
MRO、类型转换、字符串处理……这些调用本身不对应任何 SL 帧，`throw`/`catch` 在这段范围内正常展开完全
没问题），一旦异常越过这一层 `catch`，主循环就不再把它当 C++ 异常处理，而是转成"有一个 SL 异常对象正在
传播"这个显式状态，交给下面这套手写算法——不是让 C++ 自己继续 `catch` 到更外层，SL 帧之间没有嵌套的 C++
调用记录可供 C++ 展开。跟前端 `SyntaxError`/`InternalError` 那批 C++ 异常类是同一个用法（内部传递
错误、从不逃出各自的公开入口），但 **运行期这层不能照抄"一个 SL 概念对应一个 C++ 异常类"**：运行期异常
本身是 SL 对象（`SL.md` 4.2 定义的内置类的实例，用户还能继续派生子类），种类不固定、C++ 层穷举不完； 应该用
**一个统一的 C++ 包装类型**（装一个指向 SL 异常对象的引用）当这个"信封"，具体是哪种异常由信封里 那个 SL
对象自己的 `type()` 决定，不是由信封的 C++ 类型决定。

`return`/`break`/`continue` 不走这条路——它们是纯粹的 PC 跳转（`CALL_FINALLY`/`END_FINALLY` 那套），
编译期就能确定目标和要不要经过 `finally`，不需要运行期传播，也不需要用到上面这套异常信封。

块栈的一项记 `{处理器目标, 进块时的操作数栈深}`。异常传播时（信封已经在主循环手里）：从当前帧块栈弹出
最近一项，把操作数栈裁到它记的深度，压入异常对象，跳到它的处理器；块栈空了就弹帧、往下一帧传播——这一步
步都是主循环里的显式循环，不是 C++ 调用栈的自动展开。传到 `NativeFrame` 时给它一次收尾的机会（例如
`import` 失败要撤掉半成品的缓存），它可以就地处理掉，也可以继续往外传。

| 指令                                 | 前               | 后              | 行为                                                                                           |
|--------------------------------------|------------------|-----------------|------------------------------------------------------------------------------------------------|
| `SETUP_EXCEPT t` / `SETUP_FINALLY t` | `…`              | `…`（只压块栈） | 压一项块                                                                                       |
| `POP_BLOCK`                          | `…`              | `…`             | 弹一项块（正常路径走完 try 体时）                                                              |
| `CALL_FINALLY t`                     | `…`              | `… r`           | 把"返回地址" `r` 压栈并跳到 `t`。正常完成、`return`/`break`/`continue` 经过 `finally` 时走这条 |
| `END_FINALLY`                        | `… r` 或 `… exc` | `…`             | 弹栈顶：是返回地址 `r` 就跳回去；是异常对象 `exc` 就继续向外传播                               |
| `CHECK_EXC_MATCH`                    | `… exc E`        | `… exc b`       | 弹掉 `E`、压 `isinstance(exc, E)` 的结果 `b`                                                   |
| `RAISE`                              | `… exc`          | ——（抛出）      | 弹栈顶并抛出；不是 `BaseException` 的实例则 `TypeError`                                        |
| `RERAISE`                            | `… exc`          | ——（抛出）      | 弹栈顶异常对象，继续传播（不产生新的位置信息）                                                 |

"返回地址"是纯内部的整数值，SL 层拿不到，只在 `finally` 体执行期间待在操作数栈上。

### 调用与建立

| 指令           | 前                                         | 后         | 行为                                                                                                                             |
|----------------|--------------------------------------------|------------|----------------------------------------------------------------------------------------------------------------------------------|
| `CALL n`       | `… f a₁ … aₙ`                              | `… v`      | 压被调对象的帧（SL 函数 → `ByteCodeFrame`，内置 → `NativeFrame`）。只有位置实参                                                  |
| `CALL_KW n`    | `… f a₁ … aₙ names`                        | `… v`      | 同上。`names` 是常量表里的名字元组，末尾 `len(names)` 个实参按名字传                                                             |
| `CALL_EX`      | `… f args kwargs`                          | `… v`      | 同上。实参里有 `*`/`**` 展开时用，`args` 是 list、`kwargs` 是 dict                                                               |
| `MAKE_FUNC`    | `… captures params ret_type doc code name` | `… f`      | 弹 6 项建函数对象，见下                                                                                                          |
| `MAKE_CLASS`   | `… captures bases doc code name`           | `… c`      | 弹 5 项，新建局部帧执行类体，见下                                                                                                |
| `RETURN_VALUE` | `… v`                                      | ——（弹帧） | 弹本帧栈顶作为值，弹帧，按 `owner_` 的种类收尾                                                                                   |
| `IMPORT n`     | `…`                                        | `… m`      | 关键字形态 `import a.b.c`：名字表第 `n` 项是完整点分名，压 `NativeFrame` 跑加载算法，最终压入**第一段**的模块对象                |
| `EVAL`         | `… args kwargs`                            | `… v`      | 绑出 `code`（绑定失败 `DispatchError`，非 str 则 `TypeError`），解析成恰好一条表达式（否则 `SyntaxError`），编译，压 `EvalFrame` |

实参与形参的绑定算法（槽位填充、`*args`/`**kwargs` 收集、类型检查、函数族逐个试）不摊成字节码，在
`CALL` 系列指令的实现里。`EVAL` 固定走打包好的 `args`/`kwargs` 形状，不配快路径。`EVAL` 编译 `code`
时交给语义检查的 `in_local_scope` 取 `target_.globals_frame_ != target_`。

**没有 `IMPORT_CALL`**：调用形态 `import(expr, kwarg=v, ...)` 的实参形状本来就跟普通调用完全一致 （
`SL.md` 自己也是这么定义的），没必要再写一套专用的实参绑定逻辑——直接把加载算法包成一个内部函数 对象，
`LOAD_COMMON` 取它、后面接一次普通 `CALL`/`CALL_KW`/`CALL_EX`，复用通用调用的绑定机制。这个 函数对象只出现在这一种
codegen 模式产出的字节码里，`import` 仍是关键字，SL 层没有任何办法引用到它、 更不能把它存起来传来传去。

**`eval` 不能走同一条路，即使它的实参形状同样"跟普通调用一致"**：`import` 的调用形态在参数绑定完
之后才开始干活（加载算法），这一步完全可以塞进一个普通的内置函数实现；`eval` 不同，它必须先拿到
`code` 这个字符串、现场解析+编译出一份新 `Code`（可能 `SyntaxError`），而编译这一步需要的
`in_local_scope` 来自 **发起调用的这一帧**——这个信息在参数绑定阶段根本不存在，`CALL` 的通用流程
完全没有"先编译一份新 `Code` 再决定压哪种帧"这个步骤，硬塞等于给 `CALL` 开洞。更根本的是：`eval`
关键字化的原因就是让"这份 `Code` 里有没有 `eval` 点"变成纯静态可判定的性质（见 `.ai/context.md`），
为将来没有 `eval` 的函数把局部变量装进槽位铺路——如果 `eval` 也编译成 `LOAD_COMMON` + `CALL`，
字节码层面就再分不清一次普通调用和一次 `eval`，直接废掉这条前提。`EVAL` 必须留着专用指令。

## 关键构造的 codegen 模式

**`and`/`or`**：只用一个消费型的 `JUMP_IF_FALSE`（不需要额外的 `JUMP_IF_TRUE`）。`want_value=true`
时，先复制一份操作数，拿这份副本走 `TO_BOOL` 再测试，原件全程留在栈上不受影响；短路时原件就是结果，
不短路时先弹掉原件再求右操作数（右操作数照样 `want_value=true`）：

```
a and b (want=true) : <a> COPY 1 TO_BOOL JUMP_IF_FALSE end POP_TOP <b(want=true)> end:
a or b  (want=true) : <a> COPY 1 TO_BOOL JUMP_IF_FALSE rhs JUMP end   rhs: POP_TOP <b(want=true)>   end:
```

`want_value=false` 时不需要保留 `a` 本身——只要它的真值，不需要 `COPY`，`TO_BOOL` 直接吃掉 `a`；
短路那条路径上什么也不用留（`a` 已经被 `TO_BOOL` 转换消耗掉，`JUMP_IF_FALSE` 又把转换结果弹了），右
操作数继承外层的 `want_value=false`：

```
a and b (want=false) : <a> TO_BOOL JUMP_IF_FALSE end <b(want=false)> end:
a or b  (want=false) : <a> TO_BOOL JUMP_IF_FALSE rhs JUMP end   rhs: <b(want=false)>   end:
```

`or` 只是把"假才跳"倒过来，用一条无条件 `JUMP` 换向即可，不需要"真才跳"的指令。

**链式比较 `a < b < c < d`**（`is` 链同理，`op` 换成 `is`）。中间段把右操作数复制一份垫到左操作数下面
再比较，`t`（比较结果）不保证已经是 bool（用户重载的比较方法可以返回别的类型），所以测试前先垫一条
`TO_BOOL`——跟 `and`/`or` 一样，只转换用来测试的那份副本，`t` 原件不受影响； **所有中间段的失败分支都
跳向同一个 `fail`**——不管哪一段先假，清理动作都是同一句"交换、弹掉左操作数，留下比较结果"：

```
<a> <b>
COPY 1 INSERT 3 BINARY_OP Lt   ; 栈: b t   (t = a < b)
COPY 1 TO_BOOL JUMP_IF_FALSE fail
POP_TOP                         ; 真：留 b，继续
<c>
COPY 1 INSERT 3 BINARY_OP Lt   ; 栈: c t   (t = b < c)
COPY 1 TO_BOOL JUMP_IF_FALSE fail
POP_TOP                         ; 真：留 c，继续
<d> BINARY_OP Lt                ; 末段：t = c < d，不用再留操作数
JUMP end
fail: INSERT 2 POP_TOP          ; 栈 [y, t] 交换成 [t, y] 再弹掉 y，留 t
end:
```

末段（最后一个操作符）不需要保留操作数，直接 `BINARY_OP`。两个操作符（`a<b<c`）时只有一段中间段，
三个以上重复中间那段即可。

**赋值表达式的值**：`want_value=true` 才需要先垫一份再让指令弹光；`want_value=false`（赋值当一条
独立语句用，最常见的情形）直接省掉 `COPY`/`INSERT`，`SET_ATTR`/`SET_INDEX` 本来就弹光不留值，正好 匹配：

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

**复合赋值**：目标只求值一次，靠 `COPY` 复制已在栈上的那份；多下标的用 `n+1` 条 `COPY n+1` 把 `obj`
和各下标整体复制一遍。跟简单赋值一样，`want_value=false` 时省掉留值用的 `COPY`/`INSERT`：

```
x op= e       (want=true)  : LOAD_NAME_STRICT x <e> BINARY_OP op COPY 1 STORE_NAME x
x op= e       (want=false) : LOAD_NAME_STRICT x <e> BINARY_OP op STORE_NAME x
x.a op= e     (want=true)  : <x> COPY 1 GET_ATTR a <e> BINARY_OP op COPY 1 INSERT 3 SET_ATTR a
x.a op= e     (want=false) : <x> COPY 1 GET_ATTR a <e> BINARY_OP op SET_ATTR a
x[i] op= e    (want=true)  : <x> <i> COPY 2 COPY 2 GET_INDEX 1 <e> BINARY_OP op COPY 1 INSERT 4 SET_INDEX 1
x[i] op= e    (want=false) : <x> <i> COPY 2 COPY 2 GET_INDEX 1 <e> BINARY_OP op SET_INDEX 1
```

**循环**：`for`/`while` 两种子模式（步进/迭代）各自的骨架，外加收集模式共用的收尾逻辑。

进循环前先压 **结果槽**（计数模式压 `0`，`$` 压空 list，`$$` 压空 dict），它整个循环期间待在栈上，
循环结束时就是整条表达式的值。循环体 `expr` 恒以 `want_value=true` 编译——不管外层要不要这个 `for`
表达式整体的值，循环体每一轮的值都要被下面的收尾逻辑读一次（判真值/取出来 append/解构）。收尾完把
这一轮的值从栈上换成对结果槽的更新：

```
计数模式   : POP_TOP LOAD_CONST 1 BINARY_OP Add
$         : LIST_APPEND
$ *       : LIST_EXTEND
$$        : UNPACK 2 INSERT 2 DICT_PUT   ; UNPACK 2 按"第一个元素在栈顶"给出 v k，跟 DICT_PUT 要的
                                          ; d k v 差一次交换，INSERT 2 补上（这是 INSERT 去不掉的
                                          ; 第三处，见「栈与常量」）
$$ **     : DICT_MERGE
```

**步进模式** `for ⟦collect⟧ (init; cond; inc) expr`：`init`/`inc` 恒 `want_value=false`（"对其求值
实为跳过"，本来就不产出东西，不用刻意置 `false` 省什么）；`cond` 为空按 `SL.md` 视为 `True`，直接
`JUMP loop` 不用测；`continue` 的目标是 `inc:`，不是 `loop:`——`SL.md` 明确"仍然会对 `inc` 求值进而 对
`cond` 求值"：

```
<init>
loop:
  <cond> TO_BOOL JUMP_IF_FALSE end   ; cond 为空则省掉这行，直接落到 <expr>
  <expr>                             ; want_value=true，留一个值
  <收尾>                             ; 见上，并入结果槽
inc:
  <inc>
  JUMP loop
end:
```

**迭代模式** `for ⟦collect⟧ (iterable ⟦as lvalue⟧) expr`：`continue` 的目标直接是 `loop:`，没有
`inc` 这一步。`FOR_ITER` 耗尽时自己弹掉迭代器，但 `break` 提前退出时迭代器还留在栈上——它是循环体
在栈上多压出来的一层，`break` 裁栈时要算上它（codegen 本来就在静态跟踪栈深，这层不会漏，跟结果槽是
同一件事的两个例子）：

```
<iterable> GET_ITER
loop:
  FOR_ITER end                  ; 取到元素压栈；耗尽则弹掉迭代器、跳 end
  <写入 lvalue，弹光；没有 as 就 POP_TOP 丢弃>
  <expr>                        ; want_value=true
  <收尾>
  JUMP loop
end:
```

**`want_value=false` 时能不能连结果槽一起省掉**：计数模式能——它只是个纯计数，没有 `want_value` 时 连
`LOAD_CONST 0`/每轮的 `POP_TOP LOAD_CONST 1 BINARY_OP Add` 都不用发，退化成一个不产值的裸循环。
`$`/`$ *`/`$$`/`$$ **` 不能——`UNPACK`/`DICT_PUT`/`LIST_EXTEND`/`DICT_MERGE` 这些收尾指令本身可能抛
`TypeError`/`ValueError`（个数不对、不可迭代、不满足映射协议……），跳过它们就是悄悄吞掉这些异常，
不满足"只能省掉 codegen 自己加的垫值步骤，不能省掉有副作用/可能抛异常的求值"这条判断标准（「栈约定」
一节）。收集模式的每一轮折叠必须照常做，只是最后要不要把整个结果槽的值留给外层，才由 `want_value`
决定。

**`break`/`continue` 不用运行期的块栈**：跳到哪、裁掉几层操作数栈，codegen 静态就知道，编译成"若干
`POP_TOP` + `JUMP`"；中间隔着 `finally` 时在跳之前由内到外补 `CALL_FINALLY`。`return` 同理，只是不用
裁栈（弹帧时整个操作数栈一起没了），返回值在 `finally` 体执行期间待在栈上不受影响。

**`try`**：

```
SETUP_EXCEPT h    <expr1>   POP_BLOCK   JUMP after
h:  COPY 1 <E1> CHECK_EXC_MATCH JUMP_IF_FALSE next1
    (有 as 就把栈顶异常对象赋给 lvalue，否则 POP_TOP) <expr2> JUMP after
next1: …          RERAISE
after:
```

`finally` 在外面再包一层 `SETUP_FINALLY`：正常路径走完后 `POP_BLOCK` + `CALL_FINALLY`，异常路径由 VM
展开时直接跳进同一段 `finally` 体，末尾统一 `END_FINALLY` 分派—— **`finally` 体只编译一份**。
`finally` 体不可能跳出这段范围（编译期已查），不用处理跳转穿过 `finally`。

**建立函数**：装饰器最先求值，所以先压装饰器；`Code` 和名字是常量、无副作用，放最后压。

```
<deco1> … <decoN>
<各值捕获> MAKE_TUPLE k
<各形参注解/默认值> MAKE_TUPLE m
<返回类型> <doc> LOAD_CONST <Code> LOAD_CONST <名字>
MAKE_FUNC
CALL 1  (由近到远，一个装饰器一条)
COPY 1 STORE_NAME f   (命名函数才有，绑定只发生一次、绑最终值)
```

`MAKE_FUNC` 固定弹 6 项，缺的注解/默认值/名字/`doc` 一律用 `LOAD_COMMON` 压一个 **纯内部的哨兵值**
（不能用 `None`，它本身是合法默认值；这个哨兵也是"极多份 `Code` 反复用到的同一个值"，符合
`LOAD_COMMON` 的定位）。引用捕获不产生任何指令：`MAKE_FUNC` 就在当前帧里执行，直接抓一份当前帧的
引用挂到新对象上。

**建立类**：形状同上，基类元组代替形参那段，弹 5 项。`MAKE_CLASS` 不直接产出类对象——它新建局部帧、
写入值捕获、压栈执行类体；类体的 `RETURN_VALUE` 收尾时才由 `owner_`（类构建器）完成属性收集、MRO、
`__abstractmethods__`，把类对象压回。装饰器调用在这之后。

**一份 `Code` 可以复用多次，但每次"建立"都必须造出全新的函数/类对象**，不能缓存。

## 明确留给以后

- 窥孔优化、超级指令、内联缓存：一概不做。
- 局部变量槽位化：前提是"这份 `Code` 里有没有 `eval` 点"的静态判定。
- 异常表取代块栈：等块栈成为瓶颈再换，对 codegen 之外不可见。
- `RecursionError` 的阈值未定。只需要卡帧栈深度一个指标。
