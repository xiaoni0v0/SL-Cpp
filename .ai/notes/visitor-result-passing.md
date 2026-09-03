# 访问者要返回值/带参数时，手搓成员通道，不做封装

`AstVisitor`/`AstConstVisitor` 的 `visit` 一律 `void`。想要返回值（`AstJsonDumper` 要 `json`）或者往
下传参数（`CodeGen` 要 `want_value`）时，就在 visitor 自己身上开成员当通道，**不要把这套机制抽成模板
基类**。

## 为什么不抽

- 模板化 `AstVisitor` 走不通：`accept` 是 `AstNode` 的虚函数，签名带 `AstVisitor&`，跟着模板化就成了
  虚函数模板，C++ 不允许。只能给每个返回类型在基类里各加一个 `accept`，等于让 `AstNode` 提前知道全世界
  的返回类型。
- 可预见的 visitor 就 `AstJsonDumper`、`CodeGen` 这么几个。为两三个使用者引进
  `std::tuple`/`std::apply`/void 偏特化那套机器，读任何一个 visitor 都得先看懂适配器，是负收益。

## 怎么手搓

按参数性质分两种，别混：

1. **整趟遍历不变的参数** —— 构造时定成 `const` 成员，根本不参与传递。`AstJsonDumper::include_pos_`
   就是这种。
2. **每次调用都不同的参数** —— 成员 + 一个入口函数赋值再 `accept`。`CodeGen` 的 `want_value` 是这种：
   同一个父节点的不同子节点值不一样，塞进"整趟不变"那类里就得手工存取，容易漏。

返回值同理：一个 `result_` 成员，配一个入口函数。范本见
[AstJsonDumper](../../compiler/parser/ast_nodes/ast_json_dumper.h) 的 `dump_node`。

## 两条硬约束（手搓唯一会出 bug 的地方）

- **每次调用都不同的那类参数，进 `visit` 第一行就搬到局部常量**——一旦分派了任何子节点，成员就被覆盖了；
- **`result_` 只在入口函数里「`accept` 完立刻 move 走」**，绝不允许跨越另一次 `accept` 继续存活。

本质是"成员当传参通道，通道的读写必须紧挨着分派点配对"。写新 visitor 时照抄这条纪律就不会串。
