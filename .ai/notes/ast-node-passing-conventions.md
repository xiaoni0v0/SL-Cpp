# AST 节点怎么传：引用 / 非 const 引用 / 智能指针引用 各管什么

`compiler/analyzer/` 里同时出现 `const AstNode &`、`AstNode &`、`const AstNodePtr &`、`AstNodePtr &`，
乍看像是"混着用"。其实是一套规则，只是以前没写下来。

## 规则

**看你要对这个节点做什么，不看它当时恰好是什么形式**：

| 想做的事                             | 参数类型                              | 例子                                       |
| ------------------------------------ | ------------------------------------- | ------------------------------------------ |
| 只读地看                             | `const AstNode &` / `const AstNodeXxx &` | `StaticEvaler::is_int`、`SemanticChecker::visit` |
| 原地改它，或者把它的孩子搬走         | `AstNode &` / `AstNodeXxx &`          | `StaticEvaler::fold_if`、`ExprFolder::visit` |
| 把整个槽位换成另一个节点             | `AstNodePtr &`                        | `ExprFolder::visit_and_replace`            |
| 判断槽位空不空                       | `const AstNodePtr &`                  | `SemanticChecker::check_nullable`          |
| 交出所有权                           | 返回 `AstNodePtr`（按值）             | `StaticEvaler::fold*`（`nullptr` = 折不动）|

**不用**：裸指针 `AstNode *`（除了 `dynamic_cast` 的即时结果）、`AstNodePtr` 按值当入参、
`std::shared_ptr`。

## why

**为什么 `const AstNodePtr &` 不是"多此一举地传智能指针"**：只有它能表达"这个槽位可能是空的"。
一旦写成 `const AstNode &`，就已经默认非空了。所以可空的槽位（`else_expr_`、`doc_`、`value_`、
`init_`/`inc_`）必须按指针传，`check_not_null`/`check_nullable`/`check_doc` 收的都是指针，
不是随手写的。

**为什么 `StaticEvaler::fold_unary` 是 const、`fold_binary` 不是**：也不是随手写的。
`fold_if`/`fold_compound`/`fold_and_or`/`fold_add`/`fold_compare`/`fold_for_cond` 会把 node 的孩子
`std::move` 出来当返回值——调用之后那个 node 就是个空壳了；`fold_unary`/`fold_arithmetic`/
`fold_bitwise`/`fold_mul` 只读。**签名上的 const 是在告诉调用方"这次调用之后这个节点还完不完整"**，
是有效信息，不是装饰。所以加新的 `fold_*` 时别图省事一律写非 const。

**为什么槽位替换必须收 `AstNodePtr &` 而不是 `AstNode &`**：折叠经常整个换掉一个节点
（`2 + 3` → 字面量 `5`），拿到节点引用改不了"父节点的那个 `unique_ptr` 指向谁"。

## 类的形态

**每一层的「阶段入口」一律是静态门面：构造函数收进 private，对外只有静态方法。**

```cpp
Lexer::tokenize(source, file_path)              -> std::vector<Token>
Parser::parse_program(tokens, file_path)        -> AstNodeProgramPtr
Parser::parse_single_expr(tokens, file_path)    -> AstNodePtr
SemanticChecker::check_program(root, file_path)
SemanticChecker::check_single_expr(expr, file_path, in_local_scope)
ExprFolder::fold_program(root)
CodeGen::generate_program(root, file_path)      -> Ref<Code>
AstJsonDumper::dump(node, include_pos)          -> json
```

这些类里大多有一堆遍历状态（`Parser` 的 `pos_`/`brackets_`、`SemanticChecker` 的 `ctx_`、`CodeGen`
的 `builders_`/`loops_`/`blocks_`），但**那是实现细节，不该漏给调用方**：构造和执行之间没有任何有用
的中间态，没人会构造完一个 `Parser` 之后不 parse，也没人会在 parse 之前对它做什么。

曾经用「公开构造 + `&&` 限定方法」来表达"只能跑一次"（`Parser{tokens, file}.parse_as_file()`）。
那是给一个本不该暴露出去的实例打补丁——构造函数私有之后，"第二次调用"在类型层面根本不可表达，比 `&&`
彻底，调用点也少一层噪音。内部实现方法统一叫 `run` / `run_xxx`，跟对外的入口名区分开。

`Analyzer`、`StaticEvaler` 更彻底一点：它们连实例都不需要（纯函数集合），构造/拷贝/移动全 `= delete`。

**例外是"被别人持有、要多次调用"的组件**：`ConstPool`（`CodeBuilder` 的成员，反复 `intern`）、
`CodeBuilder`（`CodeGen` 的成员，反复 `emit`）。它们不是阶段入口，是正常实例。

## 入口的命名

同一层同时提供"整份 Program"和"单独一条表达式"两个入口时，用 `_program` / `_single_expr` 配套：

| 层 | 整份 Program | 单条表达式 |
|---|---|---|
| `Parser` | `parse_program` | `parse_single_expr` |
| `SemanticChecker` | `check_program` | `check_single_expr` |
| `ExprFolder` | `fold_program` | `fold_single_expr` |
| `Analyzer` | `analyze_program` | `analyze_single_expr` |
| `CodeGen` | `generate_program` | （等 `eval` 落地再补） |

`in_local_scope` 这类只对单条表达式有意义的参数，就只挂在 `_single_expr` 那个入口上——整份 Program
恒是模块顶层，给它一个 `in_local_scope` 参数只会让人以为可以传 `true`。

## how to apply

新写一个吃 AST 的函数时，按上表对号入座；拿不准就问自己两句：

1. 调用方要不要在调用之后继续把这个节点当完整的树用？要 → `const`。
2. 这个位置有没有可能是空的？有 → 收指针；没有 → 收引用。
