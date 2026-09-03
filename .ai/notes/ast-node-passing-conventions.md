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

同一套取舍延伸到类本身：

- **无状态的纯函数集合**（`Analyzer`、`StaticEvaler`）：全静态成员 + 删掉所有构造/拷贝/移动，
  外部构造不出来。
- **有遍历状态的**（`SemanticChecker`：`root_`/`file_path_`/`ctx_`）：正常实例，一次性用完就扔
  （`check()` 加了 `&&` 限定）。
- **`ExprFolder` 是中间态**：它没状态，但改成 `AstVisitor` 之后当 visitor 得有个实例，所以对外
  仍是静态门面（`ExprFolder::fold(root)`），默认构造函数收成 private 只给自己的静态入口用。

## how to apply

新写一个吃 AST 的函数时，按上表对号入座；拿不准就问自己两句：

1. 调用方要不要在调用之后继续把这个节点当完整的树用？要 → `const`。
2. 这个位置有没有可能是空的？有 → 收指针；没有 → 收引用。
