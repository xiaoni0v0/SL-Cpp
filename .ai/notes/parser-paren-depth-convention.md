# Parser.cpp：括号栈 `brackets_` 的管理约定和用途

## 谁压栈、谁弹栈

Parser.cpp 里所有处理括号配对的函数（`parse_call_args`、`finish_index`、`parse_func_params`、
`parse_captures`，`parse_paren_or_tuple`、`parse_list`、`parse_dict_or_set`、`parse_compound`，以及
`if`/`for`/`while`/`except` 的头部）统一约定：

1. 起始括号和收尾括号都由这个函数自己消耗，调用方只负责判断"当前是不是起始括号"；
2. 一进来就 `expect_open(kind)`（消耗左括号、压栈），收尾时 `expect_close(kind)`（消耗右括号、弹栈并核对
   弹出的种类跟传进来的一致），调用方不碰 `brackets_`；
3. 收尾永远是"先消耗收尾括号、再弹栈"，这是 `expect_close` 自己的实现顺序。

这样每个函数是自洽的单元，调用方不用记"调用我之前你还得自己压栈"这种隐藏契约。`expect()` 抛异常时整个
Parser 作废，不存在"弹了一半"的状态。

需要"同一段括号内容、包成不同节点"时（`import`/`eval` 的调用形态），直接调现成的 `parse_call_args`，再把
返回的 `CallArgs` 包进 `AstNodeImportCall`/`AstNodeEval`，不要为此拆一个出参版本。

## 栈顶决定两件事

`Bracket` 有六种：`Paren`、`Square`、`Brace`（复合表达式、函数体、类体）、`ForHeader`、`Call`（实参表，含
`import(...)`、`eval(...)`）、`DictSet`（`${ }`）。用栈而不用深度计数器，是因为下面两条规则只看最内层是
哪一种括号，外面嵌了多少层都不该有影响，每层各自压栈弹栈，进入 `{}` 时不用手动保存、清零、恢复什么。

1. **换行是不是空白**（`skip_paren_newline`）：栈顶是 `Brace` 或 `ForHeader` 时换行有意义（分隔语句、分隔
   for 头部的槽），其余四种括号里换行是空白。
2. **能不能写赋值一档**（`assign_allowed`，管 `=`、`:`、op=）：栈顶是 `Call` 时不能。SL.md 规定调用的实参里只有开头的
   `identifier =` 表示按关键字传参，其余赋值写法都要被实参内的括号包住。按关键字传参的写法由 `parse_call_args` 在实参开头
   单独认，不经过 Pratt；Pratt 循环在 `Call` 栈顶遇到赋值一档直接停，交给 `parse_call_args` 报"赋值不能当实参"。
   加一层括号、进函数体、进列表，栈顶就变了，赋值照常可写。这条只看栈顶，所以 `f(if (c) x = 1)` 里 `if` 分支的
   `x` 也不会吃掉 `=`。字典的键值分隔是 `->`，不是赋值一档，`DictSet` 里赋值照常可写。

索引实参按普通表达式解析，赋值一档照常可写，不使用 `Call` 的括号限制。
实参列表可以为空，允许多个 `*expr` 展开项，不单独识别按关键字传参的写法。

## how to apply

加处理括号配对的新函数，照抄 `expect_open`/`expect_close`，不手写 push/pop。新增一种括号时，想清楚它在上面
两条规则里各属于哪一边；语义上跟已有某种完全一样（比如另一种"实参表"）就复用那一种，不另开。
