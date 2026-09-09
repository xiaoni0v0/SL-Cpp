# 字节码与虚拟机设计

`compiler/codegen/`（AST → `Code`）和 `executor/`（跑 `Code`）之间的实现契约。不是语言规范，涉及语义
处以 [SL.md](../../SL.md) 为准。取舍理由见 [context.md](../../.ai/context.md)。

现状：`Code`、`ConstPool`、`CodeGen` 已实现，`executor/` 的虚拟机还没写。虚拟机落地之前，检查 codegen 产物靠 [`disassembler.h`](disassembler.h)。

## 硬约束

1. **C++ 调用栈深度不得随 SL 帧栈深度增长。** 任何可能回调 SL 代码的地方一律压 `NativeFrame` 交回
   主循环，不在 C++ 层递归跑子循环。纯 C++、不回调 SL 的实现原地算完（`1 + 2` 直接算）。
2. **异常跨帧传播不用 C++ 异常。** SL 帧是堆对象、之间没有嵌套的 C++ 调用记录，C++ 的栈展开够不到。
   C++ 异常只用来把一次"推进一步"内部的错误带到主循环那层 `catch`，越过它就转成显式的传播状态。
   运行期异常是 SL 对象（用户还能派生子类），**用一个统一的 C++ 包装类型装一个引用当信封**，不照抄
   前端"一个概念一个 C++ 类"。
3. **局部变量走字典查找，不编译成槽位下标**：`_L` 恒为真字典，`eval` 能往任意帧现插代码。
4. **运行时先于编译器**：codegen 跑起来时对象模型已经在，直接造真 SL 对象放进常量表，没有
   "描述 → 物化"这一层。

## 指令编码

一条指令固定 2 字节：高字节操作码、低字节参数，无参指令塞 0。字节码即 `std::vector<std::uint16_t>`，
计数器是它的下标。跳转目标一律绝对、以指令为单位。

**`EXTENDED_ARG`**：前缀 `EXTENDED_ARG a` 把待用参数累积成 `(已累积 << 8) | a`，最多连用 3 条，只对
紧随其后那条真指令有效。加上指令自己那一字节，参数最宽 32 位，跟表下标的 `std::uint32_t` 对齐。

加前缀会让后面指令下标右移、可能又逼出新前缀，codegen 先产出带标签的伪指令流，反复扫描到不再变化
为止（前缀只增不减，必收敛）。

## `Code`

一份 `Code` 是一个 `ByteCodeFrame` 的边界：一份文件、一个函数体、一个类体各一个。字段见
[`runtime/objects/Code.h`](../../runtime/objects/Code.h) 的 `Code::Parts`：

| 字段              | 内容                                                                                                                    |
| ----------------- | ----------------------------------------------------------------------------------------------------------------------- |
| `bytecode`        | 见上                                                                                                                    |
| `constants_table` | 不可变字面量值，见下                                                                                                    |
| `codes_table`     | 函数体/类体的嵌套 `Code`。跟常量表分开：它不是"值"，也不去重                                                            |
| `names_table`     | 标识符、属性名、`import` 的点分名字                                                                                     |
| `max_stack`       | 栈深上限，一次性分配操作数栈                                                                                            |
| `ref_captured`    | **名字表的位图**：第 `i` 个名字是不是引用捕获。允许短于名字表，末尾缺的算 `false`                                       |
| `value_captured`  | 值捕获的名字，按声明顺序对应 `MAKE_FUNC`/`MAKE_CLASS` 弹的那份捕获值元组                                                |
| `params`          | 函数体才有：四段形参（`positional`/`kw_only`/`var_args`/`var_kwargs`）各自的名字，以及哪些有默认值/注解、有没有返回注解 |
| `lines_table`     | 指令下标 → 行列，按 pc 升序。纯为报错，非规范                                                                           |
| `source_name`     | 文件名，`eval` 出来的那份写 `<eval>`                                                                                    |

值捕获在函数调用时随实参写进 `_L`（跟默认值走同一套注入机制，但不参与实参匹配）；类体是新建帧后
直接写入。

**不进 `Code`**：装饰器（外层 `Code` 里的普通调用）；形参默认值、各类注解、`doc`、函数名（外层作用域
建立时求值，挂在函数对象上，`Code` 只需知道"有没有"）；`list`/`dict` 字面量的**值**（可变，每次求值
必须是新对象）。

**`Code` 是 SL 对象，但 SL 层完全接触不到**：不进内置表，没有任何属性/内置函数会返回它，连
`type(...)` 都没有下手的地方。它是对象只为让 GC 的追踪链穿过去：模块/函数/类对象 → `Ref<Code>` →
常量表、嵌套 `Code` 表、名字表。

**常量表不是 GC 根**——每份 `Code` 本来就有主，顺着上面这条链就能扫到。

### 常量表存什么

判据是**不可变**，即折叠器的 `StaticEvaler::is_literal_const`：`None`/`bool`/`int`/`decimal`/`str`/
`Ellipsis`/**全常量元组**。**不是 `is_literal_pure`**——后者含 `list`，而 `list` 可变，进了常量表就是
每次求值拿到同一个对象。

元组不可变（"不可变"指引用关系不可变），所以 `(1, (2, 'a'))` 是一条 `LOAD_CONST` 而非 `MAKE_TUPLE`；
只要有一个元素不是常量（`(x, 1)`、`([1],)`）整个就走 `MAKE_TUPLE`。`CALL_KW` 的名字元组就是个常量
元组，不单独算一类。`decimal` 能进是因为构造不舍入、精确保留全部位数，不受运行期上下文影响。

**嵌套 `Code` 单独一张表**：`Code` 没有"结构相等"可言（两个函数体不去重），而且 `LOAD_CONST` 压的应当
恒是"用户代码能拿到的值"。这样常量表每一项都是 SL 对象。

### 常量去重

实现是 [`ConstPool`](ConstPool.h)。**加入时立刻按结构去重，值相等的常量恒是同一个槽、因而恒是同一个
对象**：`a = (1, 2)` / `b = (1, 2)` 的 `a is b` 为真，顶层的 `(1,2)` 和 `((1,2),3)` 里那个也是同一个。
元组自底向上建，元素先去重、父项再拿元素的对象身份当键。

去重键是**结构标识，不是 SL 的 `==`/`hash`**：

- **先比类型**：`1` 和 `True`、`1` 和 `1.0` 都 `==` 为真且 hash 相同，但必须占不同的槽，否则
  `True is 1` 变真；
- **`decimal` 逐位比 `sign`/`coeff`/`exp`**（`BigDec::identical()`）。**绝不能用
  `BigDec::operator==`**——`1.5 == 1.50` 为真、`NaN == NaN` 为假，两边都跟这里要的相反；
- **元组比元素的对象身份**，不递归比值。

**单例（`None`/`True`/`False`/`Ellipsis`）取全局那一个对象**，不新建。槽号只决定去重，不决定对象身份。

## `Frame`

`Frame` 不存数据字段，只是接口：推进一步、接收子帧返回值、接收沿栈传播的异常。任何帧结束时把结果交给
下面那帧，压帧只换栈顶，**主循环不递归**。

### `ByteCodeFrame`

| 字段             | 内容                                                                                |
| ---------------- | ----------------------------------------------------------------------------------- |
| `code_`          | `Ref<Code>`，正在执行的 `Code`                                                      |
| `pc_`            | 指令计数器                                                                          |
| `stack_`         | 操作数栈，按 `max_stack` 一次分配                                                   |
| `blocks_`        | 块栈，每项记 `{处理器目标, 进块时的操作数栈深}`                                     |
| `returns_`       | `finally` 返回栈，`CALL_FINALLY` 压、`END_FINALLY` 弹。不在操作数栈上，见下         |
| `locals_`        | 局部字典即 `_L`。帧死亡时置为 `None`（不是清空、不是换对象），判活看它是否为 `None` |
| `globals_frame_` | 所属全局帧。全局帧指向自己，**帧靠 `globals_frame_ == this` 自判是不是全局帧**      |
| `global_marks_`  | global 标记集                                                                       |
| `owner_`         | 建立这一帧的东西：函数对象 / 类构建器 / 模块对象                                    |

`owner_` 提供三样，不用再开字段：**定义帧**（引用捕获据此路由）；**`RETURN_VALUE` 的收尾**（函数对象
→ 类型检查后交给下面那帧；类构建器 → 收属性、算 MRO/`__abstractmethods__`、造类对象；模块对象 → 记
`__return__` 或当退出码）；返回类型注解、形参注解（`TypeVar` 检查用）。

引用捕获位图、值捕获名字表都在 `Code` 上，查 `code_` 即可。

### `NativeFrame`

执行一段 C++ 逻辑（内置函数、运算符分派、`import` 加载算法等）。没有通用字段——每个原生操作各自定义
子类，只装自己需要的状态（"`==` 的分派"记 `lhs`、`rhs`、试到第几步）。不认识 SL 名字/作用域，没有名字
环境；下一步做什么、返回值存哪、异常要不要就地处理都由子类决定。

### `EvalFrame`

执行状态跟 `ByteCodeFrame` 一样（`code_` 是编译出的单条表达式），但**没有自己的名字环境**——
`locals_`/`globals_frame_`/`global_marks_`、引用捕获的查询全部转发给 `target_`（`eval` 所在的调用帧）。
`owner_` 恒为空，执行完直接把栈顶交给下面那帧。

这是"`Code` 边界 = 帧边界"的唯一例外：有自己的执行状态，但名字读写、`global`、引用捕获全落在 `target_`
上，跟没压新帧效果一致。

## 栈约定

**每个"编译一个表达式节点"的 codegen 函数带一个 `want_value: bool`，不变量是：编译完这个节点，操作数栈
相对编译前恰好增加 `want_value ? 1 : 0`。**

`want_value=false` 时节点自己判断能不能干脆不产出（赋值、`and`/`or`、`if` 缺分支要补的 `None`——这些
"产出"本来就是 codegen 自己加的）；不能不产出的（`CALL`、`BINARY_OP`、`LOAD_NAME` 这类，产出是指令固有
行为）编译完补 `POP_TOP`。判断标准同折叠器：**只能省 codegen 自己加的垫值步骤，不能省任何有副作用/
可能抛异常的求值。**

往子节点传不是原样转发，按节点语义各自决定：**`Program` 的每一条都恒 `false`**（见下）；`Compound`
只有非末尾语句恒 `false`，末尾那条是整块的值；`for`/`while` 的 `init`/`inc` 恒 `false`；`and`/`or`
左操作数取决于整个节点自己收到的 `want_value`。`del`/`global`/`break`/`continue` 这类值恒为 `None`
的构造，`want_value=true` 才补 `LOAD_CONST` 取 `None`。

**`Program` 和 `Compound` 的值是两套规则，别混**：复合表达式的值是块里最后一条；而 Program（整个
文件、函数体、类体）的值**只由 `return` 决定**，一路没触发 `return` 走到末尾，值就是 `None`。所以
Program 里每一条都不产出值，codegen 在末尾补一条 `LOAD_CONST <None>` + `RETURN_VALUE`——那正是
`SL.md` 说的"等价于末尾隐含一条 `return None`"。已经 `return`/`raise` 收尾的不再补，那两条走不到。

于是**每份 `Code` 的字节码必然以终止指令结尾**，虚拟机的 pc 不会走出末尾。这条不变量值得 assert：
真走出去了就是 codegen 漏发了收尾指令，让主循环在那里静默返回 `None` 只会把 bug 盖住。

codegen 按实际传下去的 `want_value` 静态跟踪栈深（算 `max_stack`、`break`/`continue` 裁栈都要用）。

## 指令表

`n`/`i` 是个数或表下标，`t` 是跳转目标。「前」「后」是执行前后的操作数栈，最右是栈顶，`…` 是不受影响的
其余部分。

### 栈与常量

| 指令              | 前             | 后             | 行为                                             |
| ----------------- | -------------- | -------------- | ------------------------------------------------ |
| `EXTENDED_ARG a`  | `…`            | `…`            | 给下一条指令的参数补高 8 位，可连用              |
| `POP_TOP`         | `… x`          | `…`            | 弹掉栈顶                                         |
| `COPY i`          | `… xᵢ … x₁`    | `… xᵢ … x₁ xᵢ` | 复制从栈顶数第 `i` 项（`i=1` 即栈顶）压栈        |
| `INSERT n`        | `… xₙ … x₂ x₁` | `… x₁ xₙ … x₂` | 弹出栈顶，插回到深度 `n`（`n=2` 即交换栈顶两项） |
| `LOAD_CONST n`    | `…`            | `… c`          | 压常量表第 `n` 项                                |
| `LOAD_INTERNAL n` | `…`            | `… v`          | 压全局共享表第 `n` 项                            |

`LOAD_INTERNAL` 取的是一张**全局共享、不属于任何单个 `Code`** 的小表，只装 **SL 层拿不到的内部对象**：
`import` 调用形态背后的函数对象、`MAKE_FUNC` 缺省参数用的哨兵值。它没有 SL 主人，**挂在 `Runtime` 上
当 GC 根**（帧栈、模块表同理）。清单在
[`runtime/x_internal_objects.inc`](../../runtime/x_internal_objects.inc)，枚举值就是这条指令的参数，
所以那张表的顺序是字节码格式的一部分，只许往后追加。
字面量一律走 `LOAD_CONST`，`None`/`True`/`False`/`Ellipsis` 也不例外。两张表互不相干，`n` 不能混用。

`COPY` 只能在栈顶新增副本，把已在栈上的项下沉只能靠 `INSERT`，两者缺一不可。

### 名字

"按作用域规则"：名字在 global 标记集里作用于 `_G`，是引用捕获就转发到**定义帧**（定义帧局部字典已是
`None` 则 `NameError`），否则作用于 `_L`；读取时 `_L` → `_G` → 内置表逐级退回。

| 指令                 | 前    | 后               | 行为                                                                                          |
| -------------------- | ----- | ---------------- | --------------------------------------------------------------------------------------------- |
| `LOAD_NAME n`        | `…`   | `… v`            | 按作用域规则读名字表第 `n` 项，查不到 `NameError`                                             |
| `LOAD_NAME_STRICT n` | `…`   | `… v`            | 复合赋值读旧值专用：不在 global 标记集时**只查 `_L`**，查不到 `NameError`，保证读写落在同一帧 |
| `STORE_NAME n`       | `… v` | `…`              | 弹栈顶，按作用域规则写                                                                        |
| `DELETE_NAME n`      | `…`   | `…`              | 按作用域规则删，不存在 `NameError`                                                            |
| `DECLARE_GLOBAL n`   | `…`   | `…`              | 是引用捕获则 `NameError`；已在标记集则跳过；否则删 `_L` 同名项、名字加进标记集                |
| `LOAD_GL a`          | `…`   | `… _L` 或 `… _G` | `a=0` 压 `_L`（不是视图/快照），`a=1` 压 `_G`                                                 |

### 属性与索引

| 指令            | 前                  | 后    | 行为                       |
| --------------- | ------------------- | ----- | -------------------------- |
| `GET_ATTR n`    | `… obj`             | `… v` | `obj` → 属性值             |
| `SET_ATTR n`    | `… obj val`         | `…`   | 写属性                     |
| `DELETE_ATTR n` | `… obj`             | `…`   | 删属性                     |
| `GET_INDEX n`   | `… obj a₁ … aₙ`     | `… v` | 走 `__op_get_index__`      |
| `SET_INDEX n`   | `… obj a₁ … aₙ val` | `…`   | 走 `__op_set_index__`      |
| `GET_INDEX_EX`  | `… obj args`        | `… v` | `args` 是下标打包成的 list |
| `SET_INDEX_EX`  | `… obj args val`    | `…`   | 同上                       |

`GET`/`SET` 前缀标记"走对象自己的协议、可能回调 SL 代码"（描述器、`__op_*__`），跟直接读写帧自身状态的
`LOAD`/`STORE` 区分开。索引里出现 `*expr`（`x[*a, b]`）时参数个数不再是编译期常数，先拼出 list 再交
`_EX` 版。索引没有 `**` 展开，所以 `_EX` 不带 `kwargs`，跟 `CALL_EX` 不对称是故意的。

### 运算符

| 指令           | 前      | 后    | 行为                                            |
| -------------- | ------- | ----- | ----------------------------------------------- |
| `UNARY_OP op`  | `… x`   | `… v` | `op` 选 `+x` `-x` `~x` `not x` `x?` `x!`        |
| `BINARY_OP op` | `… x y` | `… v` | `op` 选全部二元运算符，含比较、`in`、`is`、`..` |

正/反向方法、`__op_cmp__` 回退、`==`/`!=` 身份兜底都在指令实现里。`not` 是唯一不经过 MRO 分派的：不可
重载，固定是"取真值再取反"（`__bool__` 可能回调 SL）。

比较链和 `is` 链在 AST 里各是一个节点，但**链不是新语义**，编译成同一运算符连续应用多次。不落在这两条
指令上的只有 `and`/`or`，走 `JUMP_IF_FALSE` 短路。

### 容器

| 指令                                       | 前                                       | 后                | 行为                                                              |
| ------------------------------------------ | ---------------------------------------- | ----------------- | ----------------------------------------------------------------- |
| `MAKE_TUPLE n`/`MAKE_LIST n`/`MAKE_DICT n` | `… v₁ … vₙ`（dict 是 `… k₁ v₁ … kₙ vₙ`） | `… c`             | 弹 `n`（dict 弹 `2n`）项建容器。无展开项时用这组                  |
| `LIST_APPEND i`                            | `… lst … x`                              | `… lst …`         | 弹栈顶，追加到 list                                               |
| `LIST_EXTEND i`                            | `… lst … x`                              | `… lst …`         | 弹栈顶（要求可迭代，否则 `TypeError`），元素依次追加              |
| `DICT_PUT i`                               | `… d … k v`                              | `… d …`           | 弹 `k`/`v`，写入 dict                                             |
| `DICT_MERGE i`                             | `… d … x`                                | `… d …`           | 弹栈顶（要求映射协议，否则 `TypeError`），各项并入                |
| `COUNT_INC i`                              | `…`                                      | `…`               | 把目标位置的 int 加 1，不弹任何东西                               |
| `LIST_TO_TUPLE`                            | `… lst`                                  | `… tup`           | list 换成 tuple                                                   |
| `UNPACK n`                                 | `… x`                                    | `… vₙ … v₁`       | 要求可迭代，**逆序**压入使第一个元素在栈顶；个数不对 `ValueError` |
| `UNPACK_EX a`                              | `… x`                                    | `… vₖ … *lv … v₁` | 带 `*lv` 的解构，见下                                             |

**这五条的 `i` 是目标容器/累加器的位置**：弹掉自己那些操作数之后，从栈顶数第 `i` 项（`i=1` 即紧邻栈顶）。
容器字面量拼装时目标就在栈顶下面，恒是 `i=1`；迭代模式的 `for` 因为迭代器压在结果槽上面，用 `i=2`。
没有这个参数，迭代 `for` 的收集/计数就够不着夹在迭代器下面的结果槽。

带 `*`/`**` 展开的容器字面量：第一个展开项**之前**的那些项照常逐个求值，用 `MAKE_LIST k`/`MAKE_DICT k`
一次建好，其后逐项 `LIST_APPEND 1`/`LIST_EXTEND 1`/`DICT_PUT 1`/`DICT_MERGE 1` 并入；元组最后补
`LIST_TO_TUPLE`。第一个元素就是展开项时 `k=0`。求值顺序与源码顺序一致。

无展开时直接 `MAKE_TUPLE n`/`MAKE_LIST n`/`MAKE_DICT n`——个数是编译期常数，不必逐项并入。

**全常量元组不用 `MAKE_TUPLE`**，直接 `LOAD_CONST`。`list`/`dict` 不管元素是不是全常量都必须每次现建；
元素全为常量且个数 ≥3 时可选的更短形态是 `MAKE_LIST 0` + `LOAD_CONST <常量元组>` + `LIST_EXTEND`
（`n+1` 条压到 3 条），列表本身仍是新对象。

**`UNPACK_EX a` 编码**：`a` 是一条 `EXTENDED_ARG` 累积出的 16 位值，高 8 位是星号后项数，低 8 位是星号
前项数。例：`(a, b, *rest, c) = e` 编译成 `EXTENDED_ARG 1` `UNPACK_EX 2`。每侧最多 255 项。

这条上限由 `SemanticChecker::check_lvalue_items` 实施（超限 `SyntaxError`），`SL.md` 的解构赋值一节
也写了——它是真实的语言限制，不是内部实现细节。codegen 到手的 AST 已保证在限内。不带 `*` 的解构走
`UNPACK`，项数能靠 `EXTENDED_ARG` 扩到 32 位，不受这条限制。

### 跳转

| 指令              | 前     | 后                          | 行为                                                   |
| ----------------- | ------ | --------------------------- | ------------------------------------------------------ |
| `JUMP t`          | `…`    | `…`                         | 无条件跳                                               |
| `TO_BOOL`         | `… x`  | `… b`                       | `type(x).__bool__(x)` 的结果；不是 bool 则 `TypeError` |
| `JUMP_IF_FALSE t` | `… b`  | `…`                         | 弹栈顶，`b` 假则跳                                     |
| `GET_ITER`        | `… x`  | `… it`                      | 换成迭代器，不满足可迭代协议 `TypeError`               |
| `FOR_ITER t`      | `… it` | `… it v` 或 `…`（耗尽，跳） | 取到元素压栈；耗尽弹掉迭代器并跳 `t`                   |

**`JUMP_IF_FALSE` 只认 bool，自己不做真值转换**。`if`/`while`/`for` 的 `cond`、`and`/`or`、链式比较
每一段都是"先 `TO_BOOL` 再 `JUMP_IF_FALSE`"——比较运算符不保证返回 bool（用户重载的 `__op_lt__` 可以
返回别的类型）。唯一例外是 `CHECK_EXC_MATCH`，`isinstance` 结果恒为真 bool。

没有 `JUMP_IF_TRUE`：`or` 把"假才跳"倒过来再加一条无条件 `JUMP` 即可。

### 异常与 finally

传播算法（主循环里的显式循环，不是 C++ 栈的自动展开）：弹出块栈最近一项，操作数栈裁到记录的深度，压入
异常对象，跳到处理器；块栈空了就弹帧、往下一帧传播。传到 `NativeFrame` 时给它一次收尾机会（如 `import`
失败撤掉半成品缓存），可以就地处理也可以继续往外传。

`return`/`break`/`continue` 不走这条路——纯 PC 跳转，编译期就确定目标和要不要经过 `finally`。

| 指令                               | 前        | 后               | 行为                                                                                                    |
| ---------------------------------- | --------- | ---------------- | ------------------------------------------------------------------------------------------------------- |
| `SETUP_EXCEPT t`/`SETUP_FINALLY t` | `…`       | `…`（只压块栈）  | 压一项块                                                                                                |
| `POP_BLOCK`                        | `…`       | `…`              | 弹一项块（正常路径走完 try 体）                                                                         |
| `CALL_FINALLY t`                   | `…`       | `…`              | "返回地址"压进帧的 `returns_` 并跳到 `t`；正常完成、`return`/`break`/`continue` 经过 `finally` 时走这条 |
| `END_FINALLY`                      | `…`       | ——（不顺序往下） | `returns_` 非空就弹一项跳回去，否则把栈顶那个异常对象继续往外传播                                       |
| `CHECK_EXC_MATCH`                  | `… exc E` | `… exc b`        | 弹掉 `E`，压 `isinstance(exc, E)`，**`exc` 留在栈上**                                                   |
| `RAISE`                            | `… exc`   | ——（抛出）       | 不是 `BaseException` 实例则 `TypeError`                                                                 |
| `RERAISE`                          | `… exc`   | ——（抛出）       | 继续传播，不产生新位置信息                                                                              |

**进入 `finally` 体时，操作数栈顶恒是"这条路径要带出去的那个值"**：正常路径是 `try` 的值，
`return` 是返回值，异常路径是异常对象，`break`/`continue` 没有值可带就压个 `None` 占位、回来再弹掉。
四条路径的栈深因此一致，同一段 `finally` 体才编得出来。

**返回地址走帧自己的 `returns_`，不占操作数栈**，这是上面那条能成立的前提。反过来（照 CPython 2 把
返回地址压操作数栈）会把正常路径和 `return` 路径各顶高一格、跟异常路径对不上，那时"只编译一份"就只
剩两条出路：给每条路径各复制一份 `finally` 体（CPython 3.11 的做法，代码膨胀），或者另开指令把返回值
挪出操作数栈。挪一个纯内部的返回地址进帧，比这两条都便宜。"返回地址"是纯内部整数值，SL 层拿不到。

### 调用与建立

| 指令           | 前                                    | 后         | 行为                                                                                                                      |
| -------------- | ------------------------------------- | ---------- | ------------------------------------------------------------------------------------------------------------------------- |
| `CALL n`       | `… f a₁ … aₙ`                         | `… v`      | 分派被调对象，见下。只有位置实参                                                                                          |
| `CALL_KW n`    | `… f a₁ … aₙ names`                   | `… v`      | 同上，`names` 是常量表里的名字元组，末尾 `len(names)` 个实参按名字传                                                      |
| `CALL_EX`      | `… f args kwargs`                     | `… v`      | 同上，`*`/`**` 展开时用；`args` 是 list、`kwargs` 是 dict（就是拼装用的那两种容器）                                       |
| `MAKE_FUNC c`  | `… captures params ret_type doc name` | `… f`      | 弹 5 项 + **嵌套 `Code` 表**第 `c` 项，建函数对象                                                                         |
| `MAKE_CLASS c` | `… bases captures doc name`           | `… cls`    | 弹 4 项 + **嵌套 `Code` 表**第 `c` 项，新建局部帧执行类体。空 `bases` 视为 `(object,)`                                    |
| `RETURN_VALUE` | `… v`                                 | ——（弹帧） | 弹栈顶为值，弹帧，按 `owner_` 种类收尾                                                                                    |
| `IMPORT n`     | `…`                                   | `… m`      | 关键字形态 `import a.b.c`：名字表第 `n` 项是完整点分名，压 `NativeFrame` 跑加载算法，压入**第一段**模块对象               |
| `EVAL`         | `… args kwargs`                       | `… v`      | 绑出 `code`（失败 `DispatchError`，非 str `TypeError`），解析成恰好一条表达式（否则 `SyntaxError`），编译，压 `EvalFrame` |

**`CALL` 系列的分派**：比被调对象的 `type()` **指针**（这几个类型都是 final，指针相等就够，不走 MRO）
——`Function` 压 `ByteCodeFrame`；`BuiltinFunction` 纯计算就地算完、要回调 SL 就压 `NativeFrame`；
`Method` 把绑定的对象插到实参最前面重新分派；`FuncGroup` 逐个试。都不是就压一个 `NativeFrame` 去查
`type(f)` 的 `__op_call__` 再重新分派——这一步必须压帧，查找会走描述器、可能回调 SL，而且链可以无限长
（靠帧栈深度吃 `RecursionError`）。

**三条调用指令是刻意的性能分层，不是冗余**：功能上 `CALL_EX` 一条就够（`CALL n` 等价于先
`MAKE_LIST n` + `MAKE_DICT 0`），但那样每次调用都要凭空建两个容器对象；`CALL_KW` 用一个空名字元组也能
代替 `CALL`，代价是最常见的无关键字调用平白多压一项、指令实现多一条分支。所以按出现频率分三档：
`CALL` 零额外对象，`CALL_KW` 多一个常量表里的名字元组（不新建），`CALL_EX` 才建两个新容器。

实参绑定（槽位填充、`*args`/`**kwargs` 收集、类型检查、函数族逐个试）不摊成字节码，在 `CALL` 系列的
指令实现里；**内置函数和 SL 函数共用这一套**，所以内置函数也要声明形参与类型注解。

`EVAL` 编译 `code` 时交给语义检查的 `in_local_scope` 取 `target_.globals_frame_ != target_`。

**没有 `IMPORT_CALL`**：`import(...)` 的实参形状跟普通调用一致，加载算法包成一个内部函数对象，
`LOAD_INTERNAL` 取它接一次普通 `CALL`/`CALL_KW`/`CALL_EX` 即可。`eval` 不能照办，必须留成独立指令。

## codegen 模式

**`and`/`or`**：只用消费型的 `JUMP_IF_FALSE`。`want_value=true` 时先复制一份走 `TO_BOOL` 测试，原件留在
栈上；短路时原件就是结果，不短路就弹掉原件再求右操作数：

```
a and b (want=true) : <a> COPY 1 TO_BOOL JUMP_IF_FALSE end POP_TOP <b(want=true)> end:
a or b  (want=true) : <a> COPY 1 TO_BOOL JUMP_IF_FALSE rhs JUMP end   rhs: POP_TOP <b(want=true)>   end:
a and b (want=false): <a> TO_BOOL JUMP_IF_FALSE end <b(want=false)> end:
a or b  (want=false): <a> TO_BOOL JUMP_IF_FALSE rhs JUMP end   rhs: <b(want=false)>   end:
```

**链式比较 `a < b < c < d`**（`is` 链同理）：中间段把右操作数复制一份垫到左操作数下面再比较；`t` 不保证
是 bool，测试前垫 `TO_BOOL`（只转换副本）；**所有中间段的失败分支共享同一个 `fail`**：

```
<a> <b>
COPY 1 INSERT 3 BINARY_OP Lt   ; 栈: b t   (t = a < b)
COPY 1 TO_BOOL JUMP_IF_FALSE fail
POP_TOP                         ; 真：留 b，继续
<c>
COPY 1 INSERT 3 BINARY_OP Lt   ; 栈: c t
COPY 1 TO_BOOL JUMP_IF_FALSE fail
POP_TOP
<d> BINARY_OP Lt                ; 末段：不用再留操作数
JUMP end
fail: INSERT 2 POP_TOP          ; [y, t] 交换成 [t, y] 再弹掉 y
end:
```

**赋值**：`want_value=true` 才垫一份再让指令弹光：

```
x = e         (want=true)  : <e> COPY 1 STORE_NAME x
x = e         (want=false) : <e> STORE_NAME x
x.a = e       (want=true)  : <x> <e> COPY 1 INSERT 3 SET_ATTR a
x[i] = e      (want=true)  : <x> <i> <e> COPY 1 INSERT 4 SET_INDEX 1
(a, b) = e    (want=true)  : <e> COPY 1 UNPACK 2 STORE_NAME a STORE_NAME b
```

**复合赋值**：目标只求值一次，靠 `COPY` 复制已在栈上的那份；多下标用 `n+1` 条 `COPY n+1` 整体复制：

```
x op= e       (want=true)  : LOAD_NAME_STRICT x <e> BINARY_OP op COPY 1 STORE_NAME x
x.a op= e     (want=true)  : <x> COPY 1 GET_ATTR a <e> BINARY_OP op COPY 1 INSERT 3 SET_ATTR a
x[i] op= e    (want=true)  : <x> <i> COPY 2 COPY 2 GET_INDEX 1 <e> BINARY_OP op COPY 1 INSERT 4 SET_INDEX 1
```

**循环**：进循环前先压**结果槽**（计数模式压 `0`，`$` 压空 list，`$$` 压空 dict），整个循环期间待在栈
上，结束时就是整条表达式的值。循环体恒以 `want_value=true` 编译——每轮的值都要被收尾逻辑读一次。

```
步进模式 for ⟦collect⟧ (init; cond; inc) expr：
  <init>
  loop:
    <cond> TO_BOOL JUMP_IF_FALSE end   ; cond 为空则省掉这行（视为 True）
    <expr>                             ; want_value=true
    <收尾>
  inc:
    <inc>                              ; want_value=false
    JUMP loop
  end:

迭代模式 for ⟦collect⟧ (iterable ⟦as lvalue⟧) expr：
  <iterable> GET_ITER
  loop:
    FOR_ITER end                       ; 取到元素压栈；耗尽弹迭代器、跳 end
    <写入 lvalue，弹光；没有 as 就 POP_TOP>
    <expr>                             ; want_value=true
    <收尾>
    JUMP loop
  end:

收尾（i = 步进模式 1 / 迭代模式 2，因为迭代模式的迭代器压在结果槽上面）：
  计数模式: POP_TOP  count_continue: COUNT_INC i
  $       : LIST_APPEND i
  $ *     : LIST_EXTEND i
  $$      : UNPACK 2 INSERT 2 DICT_PUT i   ; UNPACK 2 给出 v k，DICT_PUT 要 d k v
  $$ **   : DICT_MERGE i
```

**迭代模式的栈序是「结果槽在下、迭代器在上」**，没得选：`FOR_ITER` 要求迭代器在栈顶，而结果槽要活过
整个循环。于是每轮循环体的值压在迭代器**上面**，收尾指令够不着下面的结果槽——这正是那五条要带 `i`
的原因。计数模式的 `COUNT_INC` 不弹栈顶，是为了让 `continue` 能跳过 `POP_TOP` 直接进来（它自己的裁栈
已经清掉本轮的值了），仍然计入。

`break` 统一跳 `end:`（两种模式都不计入/不放入）。**`continue` 的目标按收集模式分**：计数模式跳
`count_continue:`（跳过 `POP_TOP`，它自己的裁栈已经清掉了那个值），仍要计入；收集模式跳过整个收尾块，
直接到 `inc:`（步进）或 `loop:`（迭代）。迭代模式下 `break` 提前退出时迭代器还留在栈上，裁栈要算上它。

**`want_value=false` 时结果槽能不能省**：计数模式能，退化成裸循环；`$`/`$ *`/`$$`/`$$ **` 不能——收尾
指令本身可能抛 `TypeError`/`ValueError`，跳过就是悄悄吞异常。每轮折叠照常做，只有最后要不要把结果槽交
给外层由 `want_value` 决定。

**`break`/`continue`/`return` 不用运行期块栈找目标**：跳到哪、裁掉几层栈 codegen 静态就知道，编译成
若干 `POP_TOP` + `JUMP`。

但**跨出去的每一层 `try` 都得逐层 `POP_BLOCK`**：块栈本身是运行期的，跳走时不弹干净，跳到外面之后再
抛异常就会撞进这些已经失效的处理器。`return` 也一样要弹——它虽然马上弹帧、块栈跟着整个没掉，可
`finally` 体是在弹帧**之前**跑的，不先弹掉自己这层，体内抛的异常会被它自己接住，转圈。

隔着 `finally` 的那几层，`POP_BLOCK` 之后还要走一遍 `finally` 体：先把栈裁到进 `try` 时的深度，
`break`/`continue` 再压一个 `None` 占位（回来弹掉），`return` 则用 `INSERT` 把返回值沉到该在的位置、
弹掉夹在中间的临时值——进 `finally` 体时栈顶恒是"这条路径要带出去的那个值"。

**`try`**：处理器入口栈上只有异常对象，`CHECK_EXC_MATCH` 自己会把它留下，不需要额外复制：

```
SETUP_EXCEPT h    <expr1>   POP_BLOCK   JUMP after
h:     <E1> CHECK_EXC_MATCH JUMP_IF_FALSE next1
       (有 as 就把栈顶异常对象赋给 lvalue，否则 POP_TOP) <expr2> JUMP after
next1: <E2> CHECK_EXC_MATCH JUMP_IF_FALSE next2
       …
nextN: RERAISE
after:
```

`finally` 在外面再包一层 `SETUP_FINALLY`：正常路径走完 `POP_BLOCK` + `CALL_FINALLY`（`want_value=false`
时栈上没有值可带，先压个 `None` 占位、回来再弹掉），异常路径展开时直接跳进同一段 `finally` 体，末尾统一
`END_FINALLY` 分派——**只编译一份**，`finally` 体不可能跳出这段范围
（编译期已查）。

**建立函数**：装饰器最先求值；名字是常量放最后压。形参段按 `positional`、`kw_only` 的声明顺序走一遍，
每个形参**写了注解就压注解、写了默认值就压默认值**，谁对应谁由 `Code::ParamShape` 上的两个标志还原
（`*args`/`**kwargs` 不带注解和默认值，不占位置）：

```
<deco1> … <decoN>
<各值捕获> MAKE_TUPLE k
<各形参注解/默认值> MAKE_TUPLE m
<返回类型> <doc> LOAD_CONST <名字>
MAKE_FUNC <嵌套 Code 表下标>
CALL 1                 ; 由近到远，一个装饰器一条
COPY 1 STORE_NAME f    ; 命名函数才有，绑定只发生一次
```

`MAKE_FUNC` 固定弹 5 项，缺的注解/默认值/名字/`doc` 用 `LOAD_INTERNAL` 压一个纯内部哨兵值（不能用
`None`，它是合法默认值）。引用捕获不产生任何指令：`MAKE_FUNC` 在当前帧执行，直接抓一份当前帧的引用挂到
新对象上。

**建立类**：基类元组代替形参段，弹 4 项。**捕获段和函数那边的位置不一样**——函数是
`captures params ret_type doc name`（捕获最先），类是 `bases captures doc name`（基类先于捕获）。
这不是笔误：`SL.md` 规定类先对各基类求值，再新建类体那一帧，捕获是建帧之后才处理的，所以基类必须先
求值。未写基类时压空元组，运行期视为 `(object,)`。

`MAKE_CLASS` 不直接产出类对象——新建局部帧、写入值捕获、压栈执行类体；类体 `RETURN_VALUE` 收尾时才由
`owner_`（类构建器）完成属性收集、MRO、`__abstractmethods__`，把类对象压回。装饰器调用在这之后。

**一份 `Code` 可复用多次，但每次"建立"都必须造出全新的函数/类对象**——`is` 比的是对象身份。

## 明确留给以后

- 窥孔优化、超级指令、内联缓存：一概不做。
- 局部变量槽位化：前提是"这份 `Code` 里有没有 `eval` 点"的静态判定。
- 异常表取代块栈：等块栈成为瓶颈再换，对 codegen 之外不可见。
- `RecursionError` 的阈值未定，只需要卡帧栈深度一个指标。
