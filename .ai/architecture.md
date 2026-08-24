# 项目结构 / 代码索引

新会话、或者接手这个项目的另一个 AI，从这份文件开始：这里是"东西在哪、大概长什么样"的地图，不重复
代码本身写了什么。语言的行为契约见 [SL.md](../SL.md)；设计决策的"为什么"见 [context.md](context.md)；
具体的代码风格约定/踩过的坑见 [notes/](notes/README.md)。

## 整体流水线

```
源码 (.sl)
  → Lexer      (lexer/)              词法分析，产出 Token 序列
  → Parser     (parser/)             语法分析，产出 AST
  → Analyzer   (analyzer/)           语义检查 + 编译期常量折叠，原地改 AST
  → Executor   (executor/)           空壳，还没写
```

`main.cpp` 目前只是个手工调用这条流水线、把每一步中间结果打印出来的调试入口，不是真正的解释器入口。

**当前完成度**：Lexer/Parser 已实现且有完整测试；Analyzer 的两个子系统（语义检查、常量折叠）已实现
且有完整测试；`numeric/` 的 `BigInt`（`int` 的底层）和 `BigDec`+`DecContext`（`decimal` 的底层）已实现
且有完整测试；`executor/Executor.{h,cpp}` 只是个占位空壳，虚拟机/求值器还没有任何代码。**在
`executor/` 落地之前，SL.md 里"运行时"相关的条文（对象模型、GC、异常传播的具体机制等）都还没有对应
实现可以参照，只能靠 SL.md 文本本身。**

**`BigDec` 的完成度**：SL.md 里 decimal 参与的**运算符全都实现了**——四则、`//`/`%`、比较、`**`，
外加 `sqrt`/`exp`/`ln`/`log10`（`**` 的一般情形要靠它们）。还没做的是 `hash`（要跟数值相等的 `int`
一致，得先归一标度）和 `int(decimal)`（取整方向 SL.md 还没定），见 [context.md](context.md) 的
悬而未决一节。

## 目录一览

| 目录 | 内容 |
|---|---|
| `lexer/` | `Lexer.{h,cpp}`：分词器。`token.h` 定义 `Token`；`x_token_type.h`/`x_keyword.h`/`x_reservedword.h` 是 X-macro 列表（见下）。 |
| `parser/` | `Parser.{h,cpp}`：递归下降 + Pratt 解析器，产出 `parser/ast_nodes/` 里定义的 AST。 |
| `parser/ast_nodes/` | AST 节点类型定义。`ast_nodes.h` 是汇总头（引入 `details/` 下所有节点头）；`x_ast_nodes.h` 是全部节点类型的 X-macro 列表；`to_json.cpp` 实现每个节点的 `to_json_impl`（调试/测试用，不是语言语义的一部分）。 |
| `parser/ast_nodes/details/` | 具体节点定义，按语法范畴分文件（`ast_node_class.h`、`ast_node_control_flows.h`、`ast_node_func.h`、`ast_node_import.h`、`ast_node_literals.h`、`ast_node_multi_exprs.h`（Program/Compound）、`ast_node_operators.h`、`ast_node_postfix.h`（call/index/attr）、`ast_node_var.h`（del/global/标识符）、`ast_node_decorators.h`）。`ast_node_misc.h` 放**不是** `AstNode`、但被多个节点类型共用的小聚合体（`OneCapture`、`OneKwArg`）。 |
| `analyzer/` | `Analyzer.{h,cpp}`：入口，依次跑 `SemanticChecker` 和 `ExprFolder`。 |
| `analyzer/semantic_checker/` | `SemanticChecker.{h,cpp}`：语义检查（作用域规则、lvalue 合法性、`*`/`**` 位置合法性、func/class 约束、AST 结构防御性校验……），只读不改 AST，违规抛 `SyntaxError`（真实语义错误）或 `InternalError`（AST 结构本身违反 Parser 的保证，代表实现自己有 bug）。 |
| `analyzer/expr_folder/` | `ExprFolder.{h,cpp}`：遍历 + 原地替换 AST 的调度层，拥有 `AstNodePtr` 槽位的所有权。`StaticEvaler.{h,cpp}`：纯函数式的"给一个节点判断能不能折、折成什么"，不遍历树、不拥有节点。 |
| `numeric/` | `BigInt.{h,cpp}`：手写高精度整数，`int` 的底层实现（`小路径 int64_t` / `大路径 limbs` 双表示）。`BigDec.{h,cpp}`：十进制浮点数，`decimal` 的底层实现（`BigInt 系数 + int64_t 指数 + 独立符号位 + 特殊值 tag`）。`DecContext.{h,cpp}`：`decimal.Context` 的底层实现——舍入方式、精度、指数范围、信号的陷阱/标志位，以及陷阱触发时抛的 `DecTrapped`。`dec_math.{h,cpp}`：`ln`/`log10`/`exp`/`**` 用的整数层定点算法（`ilog`/`iexp`/`dlog`/`dexp`/`dpower` 等），只跟 BigInt 打交道，不认识上下文和信号。 |
| `builtins/exceptions/` | 前端自己用的 C++ 异常类型（`SyntaxError`/`InternalError`/`EncodingError`/`FileNotFoundError`，都继承 `SLException`）——跟 SL.md 文档化的、暴露给 SL 用户代码的异常类同名但不是同一个东西，是两层，见 [context.md](context.md) 的架构边界一节。 |
| `utils/` | 自由函数工具：`string_utils`（UTF-8/UTF-32 互转等）、`file_utils`（读文件）。 |
| `executor/` | 空壳，还没写。 |
| `test/` | 目录结构镜像 `lexer/`/`parser/`/`analyzer/`/`numeric/`，见下。 |

## AST 节点：X-macro 分发 + 双路径职责

新增/修改语言语法时最常触碰的一套机制：

- **`x_ast_nodes.h`** 是唯一的节点类型全集清单，每行 `X(AstNodeXxx)`。`SemanticChecker::check(const
  AstNode&)`、`ExprFolder::visit(AstNode&)`、`AstNode::to_json()` 都是靠 `#define X(nt) ... #include
  "x_ast_nodes.h"` 展开成一串 `dynamic_cast` 试探来分发的（详见这三个文件里对应的实现）。**加一个新节点
  类型，必须在这里加一行**，否则那一串 `dynamic_cast` 试探永远落到 `assert(!"Unknown node type")`。
- 新节点类型还需要：在 `parser/ast_nodes/details/` 某个合适的文件里定义结构体（继承 `AstNode`，私有
  `to_json_impl` 覆写）；`SemanticChecker.h`/`.cpp` 里加对应的 `check(const AstNodeXxx&)` 重载；
  `ExprFolder.h`/`.cpp` 里加对应的 `visit(AstNodeXxx&)` 重载（哪怕只是递归子节点、什么都不折）。
- **节点类型拆分原则**："语义形状不同就不该共用节点类型"——比如比较运算符独立于普通二元运算符
  （`AstNodeCompare`，链式短路语义不同）、`is` 又独立于比较（`AstNodeIs`，不可重载、不跟比较混链）、
  `for` 的步进/迭代两种模式是 `AstNodeForCond`/`AstNodeForIter` 两个节点。不用 `variant`/tag 字段在
  一个节点里区分两种语义——X-macro 分发本来就是一个类型一个重载，`variant` 会在下面再手写一层全项目
  独一份的二级分发。
- **节点位置字段**：只有"产生式里夹着一个不属于任何子节点的裸 token"的节点类型才补一个 `Position` 字段
  （比如 `AstNodeCall::paren_pos_`、`AstNodeAttr::dot_pos_`）——节点的结束位置几乎总能从最右子节点
  递归推出，不需要现在就存;基类 `AstNode::pos_` 只存起始位置。
- **`to_json()` 是 NVI 模式**：基类 `to_json(include_pos=false)` 非虚、转发给各节点私有的
  `to_json_impl(include_pos)`（纯虚，无默认值）——虚函数不能带默认参数（clang-tidy 会拦，且这条规则
  本身是对的：默认值在虚函数场景下按调用点静态类型决定，容易产生跟直觉不符的结果）。

## SemanticChecker 与 ExprFolder 的职责边界

- `SemanticChecker` **只读不改**，靠 `Context`（`can_star`/`can_double_star`/`loop_depth`/
  `finally_loop_depth`/`local_scope_depth` 等）跨节点传递语境限制，违规抛异常。
- `ExprFolder` **原地改**，`fold_*` 系列只处理"能不能折成编译期已知的字面量"，死分支/死循环消除、
  `Compound`/`Program` 剪枝也在这里。折叠不追求覆盖每个运算符——`is`、`dict` 的运算、`str` 的 `%`
  格式化等依赖运行时对象同一性/协议判等的场景故意不折，交给以后的执行器。
- 两者的执行顺序固定是 **check 在前、fold 在后**——反过来会有死分支消除把"本该在受限语境里"的表达式
  搬到不受限语境、悄悄让非法写法变合法的风险（具体反例见 [context.md](context.md)）。
- `StaticEvaler` 的折叠范围、编译期"折叠炸弹"防护（`int64_t` 溢出检测代替无限精度折叠、容器折叠结果
  设大小上限）是刻意的工程约束，不是语言语义的一部分——新增折叠点时要留意这条边界。

## 测试组织

`test/` 下每个测试目标目录结构镜像对应源码目录（`test/lexer/`、`test/parser/`、
`test/analyzer/{semantic_checker,expr_folder}/`、`test/numeric/`）。`test/parser/`、`test/lexer/`
内部按主题分子目录，用两位数独立编号（`01_literals`、`08_control_flow`……），**不跟 SL.md 章节号
绑定**，见 [notes/no-section-numbers.md](notes/no-section-numbers.md)。新增测试文件必须手动加进
`CMakeLists.txt` 对应的 `add_executable(...)` 文件列表（不是 glob，漏加不报错、只是静默不参与编译）。

`test/numeric/big_dec_cases.inc` 和 `big_int_cases.inc` 都是**生成产物**，分别由
`gen_big_dec_cases.py`（期望值来自 CPython 自带的 decimal）和 `gen_big_int_cases.py`（期望值来自
Python 内置的 int）产出，`big_dec_test.cpp`/`big_int_test.cpp` 逐条比对结果和触发的信号。改
`BigDec`/`BigInt` 的语义时要连带重新生成（脚本开头写了用法），别手改那两个 `.inc`；生成器的种子
是固定的，同一个 CPython 版本下重新生成应当跟仓库里的逐字节一致，这一点可以当回归检查用。两个
脚本都带一个倍数参数，临时跑几十倍规模的差分测试很方便，提交进仓库的那份用默认倍数。BigDec 那张
表里除了陷阱全关的路径，还有 `kTrapped*` 四张陷阱开启的表（抛不抛、抛哪个条件、抛出时 flags 到
哪一步），值池刻意塞了带非零指数的零；这些表的合并规则见脚本头注释。

**每组用例都拿 CPython 的两套实现（libmpdec 和 `_pydecimal`）各算一遍，不一致就整组跳过**——它们
自己在 `**` 和 `exp` 上就有已知分歧（见 [context.md](context.md)）。这条规则是防呆用的：分歧点随
参数漂移，往池子里加一档 `Emin`/舍入方式就可能生成出一张永远过不了的表。

提交进仓库的这份表是**按跑得动来配的**：`SL_Cpp_Numeric_Tests` 里超越函数和 `**` 那两个用例合起来
就占了十几秒（BigDec 底下的 BigInt 是朴素算法，一次 `exp`/`ln` 要做几十次大数乘除），整个 ctest
现在约 28 秒。要更大覆盖别往表里堆，用倍数参数临时生成一份跑完再换回来——40 倍规模
（约 83 万个断言）跑过，全过。单条最贵的手写用例是 `log10_digits` 那个（约 1.6 秒，见
[context.md](context.md) 里"覆盖率驱动补的窄路径"一节），嫌慢时它是第一个可以砍的。

四个测试可执行目标：`SL_Cpp_Numeric_Tests`、`SL_Cpp_Lexer_Tests`、`SL_Cpp_Parser_Tests`、
`SL_Cpp_Analyzer_Tests`（后者同时覆盖 `semantic_checker/` 和 `expr_folder/` 两个子系统）。怎么构建/
跑测试见 [notes/build-and-test.md](notes/build-and-test.md)。

`test/doc/` 是 `build_doc.py` 从 `SL.md` 生成的带锚点版本（`SL_linked.md`/`.html`），生成产物，
不是手写测试，且经常滞后于 `SL.md` 本身（除非用户明确要求，不需要主动重新生成）。

## 工程原则（写代码时套用，不要重新发明）

这些是在这个代码库里反复被验证过的取舍标准，加新功能/改现有代码时默认套用：

- **非法状态在数据形状层面就不可表达，优于"用状态机扫描去挡"**。例：`AstNodeFunc` 的形参列表拆成
  4 个字段对应形参列表的 4 段（`params_`/`var_args_name_`/`kw_only_params_`/`var_kwargs_name_`），
  不是一个打了 tag 的扁平 vector 靠布尔标志区分区域；`AstNodeCall` 拆成 `positional_args_`/
  `keyword_args_` 两个字段各自保持书写顺序；`CollectMark`（`for`/`while` 的收集模式记号）拆成
  `Container` 枚举 + `expand_` 布尔两个正交字段，而不是四个字符串常量。这类打了 tag 的扁平结构是
  最容易漏边界情况的地方——状态机代码里"这个组合出现了但没被处理"的分支永远比看起来的多。
- **节点类型/数据结构该拆分的信号是"语义形状不同"，不是"结构相似"**：`AstNodeCompare`/`AstNodeIs`、
  `AstNodeForCond`/`AstNodeForIter`、`AstNodeImportKw`/`AstNodeImportCall` 都是这条原则的应用，
  没有用 `variant`/tag 字段在一个节点里区分。
- **槽为空表示另一种形状，是整棵 AST 通用的既有惯例**（`AstNodeIf::else_expr_`、
  `AstNodeReturn::value_`、字典字面量里 `**expr` 展开项存成 `{expr, nullptr}`……）。新增字段要表达
  "这个东西可能不存在"时，默认复用这个惯例，不要为单个场景新引入 `std::variant`/`std::optional`
  包一层——除非这个字段是全 AST 唯一需要这种表达力的地方，那种情况下 `variant` 才划算。
- **各层入口的形态按"有没有可变状态"选，不要凭手感**：带跨节点遍历状态、必须保证"同一个对象只跑
  一次"的，做成**一次性对象 + 右值限定方法**，用 `&&` 让类型系统挡住第二次调用（`Parser` 的
  `pos_`/`paren_depth_`、`SemanticChecker` 的 `ctx_`）；完全无状态、所需信息全在参数里的，做成
  **静态函数**（`ExprFolder` 的所有 `visit` 本来就是静态的，`Analyzer` 只是把两步串起来）。
  别给无状态的东西套一个只存了个引用的构造函数——那是实例的外壳、静态的内里，同一个类里迟早出现
  一半入口是实例一半是静态的分裂。
- **函数的隐含前提统一写"调用方保证 X"**（不是"要求 X"这种含糊说法，消除"这是函数自己检查的还是靠
  调用方保证的"这层歧义），配的校验方式看这个前提要不要在 Release 构建里也生效：只在开发期兜底的
  用 `assert(...)`（在函数开头，`NDEBUG` 下会被优化掉，所以不能拿它实现真正的校验）；`SemanticChecker`/
  `string_utils`/`builtins/exceptions` 这类需要在 Release 也生效的真实校验，走抛异常
  （`SyntaxError`/`InternalError`/`EncodingError`），不能用 `assert` 顶替。
- **避免"同一产物在多个出口分头构造"**：同一种产物如果在多处提前返回、各自构造，等于制造了多份
  测试压不到的独立状态空间，历史上好几个换行容错类的 bug 都是这个模式孵出来的。能合并"同一产物多处
  构造"就合并，只保留"产物本质不同"的分叉（比如字典字面量 vs 复合表达式、分组 vs 元组，这些才是
  真正的二选一）。
- **折叠（`StaticEvaler`/`ExprFolder`）的正确性标准是"被丢弃的部分本来就不会被求值"，不是"被丢弃
  的部分本身可以折成字面量"**。例：`True and f()` 能折成 `f()`，不需要 `f()` 本身可折；链式比较
  `1 < 2 < 'a'` 已经确定为 `True` 的前缀可以丢，即使后面的 `2 < 'a'` 类型不可比没法继续折。新增
  折叠规则时按这条判断"能不能丢"，不要跟"能不能折成字面量"混为一谈。
- **折叠涉及"复制值 vs 共享引用"时要考虑 `is`**：SL 的 `is` 不可重载、纯粹判断对象同一性，跟内容
  是否可变无关，"深度不可变就能安全地用 clone 代替共享"这个直觉是错的——哪怕内容完全不可变，
  克隆出来的两份和真正共享的同一份在 `is` 面前也能被区分出来。这是 `tuple`/`list` 的 `*` 重复恒
  不折的原因（AST 是纯 `unique_ptr` 独占树，没有"共享子树"的表示能力，折叠成克隆等于悄悄改变了
  `is` 语义，只能不折）。

## ExprFolder / SemanticChecker 的非显然算法事实

代码本身能看出这些行为，但不看注释容易假设错，写新折叠规则前先确认没有踩这几条：

- **`visit_and_replace` 是 fixpoint 循环**：对同一节点反复调 `StaticEvaler::fold` 直到返回
  `nullptr` 为止，不是只折一次。新写的 `fold_xxx` 不需要自己操心"我拼出来的新节点还能不能再折"——
  只要新节点的子节点都已经被 `visit` 过，上层循环会兜底继续折。
- **`prune_program`（`AstNodeProgram` 语句列表精简）不走 `fold()` 替换节点这条路，是原地精简**：
  `AstNodeProgram` 的槽位在 `ExprFolder::root_`、`AstNodeFunc::body_`、`AstNodeClass::body_` 里
  都是具体类型 `AstNodeProgram(Ptr)` 而不是通用 `AstNodePtr`，节点自身的地址/类型不能变，跟
  `AstNodeCompound`（永远挂在通用 `AstNodePtr` 槽位上，`fold_compound` 能把整个节点换成别的类型）
  是两种不同的机制，不是随意不统一。剪枝规则：`exprs_` 里每个位置统一判断，纯字面量
  （`is_literal_pure`）就丢、非字面量就留、相对顺序不变——**不特殊保留最后一条**，因为 `Program`
  的值由 `return`（或 `None`）决定，跟"最后一条表达式的值"无关（这点跟 `AstNodeCompound` 不同，
  `Compound` 的值确实是最后一条，`fold_compound` 保留最后一条是对的、两者不能类比）。
- **`for`/`while` 死分支消除**：`init_` 不管 `cond` 折出来是什么都会先无条件求值一次，折叠时不能
  丢掉这个副作用——`init_` 非空时结果包成 `Compound{init_, 退化值}`（不带 `$` 退化成 `0`，`$` 退化
  成 `[]`，`$$`/`$$ **` 退化成空 dict、没有对应字面量写不出来，所以这两种收集模式的 `for`/`while`
  死循环消除干脆不折）。`cond` 折成确定 `True` 时刻意不折——只能确定"不会提前退出"，不像 `if` 折
  `True` 那样能确定具体是哪个分支、有"换成什么"的答案。
- **`check`（`SemanticChecker`）必须在 `fold`（`ExprFolder`）之前跑，不能反过来**：反例
  `f(True and *args)`，check 先跑时 `*args` 作为 `and` 的右操作数永远在 `can_star=false` 语境下
  被检查、正确报错；但如果 fold 先跑，死分支消除会把 `*args` 从 `and` 表达式里挪出来直接顶到调用
  参数位置（`can_star=true` 的合法语境），一段本该永远语法非法的写法就这样被折叠悄悄变合法了。
- **`SemanticChecker::Context::finally_loop_depth`**：记录进入 `finally` 时的 `loop_depth`（`-1`
  表示不在 `finally` 内）。`return` 只要它 `>= 0` 就拦；`break`/`continue` 要求
  `loop_depth == finally_loop_depth`（相等说明这层循环在 `finally` 外面，不相等说明是 `finally`
  内部自己新开的循环，不该拦）；进入 `func`/`class` 的新 `Program` 时必须重置成 `-1`（否则
  `finally` 内定义的函数体自己的 `return` 会被误杀）。

## X-macro 清单文件

除了 `x_ast_nodes.h`，还有 `../lexer/x_token_type.inc`（全部 `TokenType` 枚举值）、`../lexer/x_keyword.inc`
（关键字文本 → `TokenType` 映射）、`../lexer/x_reservedword.inc`（保留字但非关键字，如 `_G`/`_L`）。加新
关键字/token 类型时这几个文件要一起改，具体加在哪由这个 token 的性质决定（是不是关键字、是不是保留字）。
