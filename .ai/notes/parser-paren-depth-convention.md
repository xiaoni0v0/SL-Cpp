# Parser.cpp：`finish_*` 函数的括号栈管理约定

Parser.cpp 里所有处理 `()`/`[]` 配对的 `finish_*` 函数（`finish_call_args`、`finish_index`、
`finish_func_params`、`finish_captures`）统一约定：

1. **起始括号和收尾括号都由 `finish_*` 函数自己消耗**，调用方只负责判断"当前是不是起始括号"；
2. **括号栈 `brackets_` 的入栈/出栈完全由 `finish_*` 函数自己管理**——函数一进来就
   `expect_open(kind)`（消耗左括号、把 `kind` 压栈），收尾时 `expect_close(kind)`（消耗右括号、
   弹栈并核对弹出的种类跟传进来的一致），调用方不用也不该直接碰 `brackets_`；
3. **收尾时永远是"先消耗收尾括号、再弹栈"**，这已经是 `expect_close` 自己的实现顺序，`finish_*`
   系列不需要（也没法）拆成两步分别控制。

`finish_dict` 是唯一的例外：`{}` 这一层括号不是由 `finish_dict` 自己管理，而是外层的
`parse_brace` 在调用 `finish_dict` 前后分别 `expect_open(Bracket::Brace)`/
`expect_close(Bracket::Brace)`——这是因为 `{}` 开启的是全新的语句语境，跟"多一层括号嵌套"是不同
的机制，详见 `parser/Parser.cpp` 里 `parse_brace` 的实现注释和 `.ai/context.md` 里对应的设计记录。
`finish_dict` 本身完全不碰 `brackets_`。

## `brackets_` 解决的问题

`brackets_` 是一个 `std::stack<Bracket>`，`Bracket` 有 `Paren`/`Square`/`Brace`/`ForHeader`
四种。它替代了早期版本里的一个整数深度计数器 `paren_depth_`，原因是纯计数器分不清"括号嵌套了几层"
和"最内层到底是哪一种括号"——而这两者的区别恰好是 `skip_paren_newline()`（仅当栈顶是 `Paren` 或
`Square` 时才把 NEWLINE 当空白跳过）能不能正确工作的关键：`{}` 内部（栈顶是 `Brace`）、for 步进头部
内部（栈顶是 `ForHeader`）都不应该被外层任意多少层 `Paren`/`Square` 带偏，栈顶天然就是当前"最内层
生效的语境"，不需要在进入 `{}`/`ForHeader` 时手动"保存/清零/退出时再恢复"计数器——每一层括号各自
独立压栈弹栈，互不干扰。

## why

早期版本 `finish_call`/`finish_index` 是"调用方在调用前 `paren_depth_++`、函数自己只减不增"，
`finish_func_params`/`finish_captures` 是"函数自己全权管理"，两种写法混用；同时约一半的收尾代码是
"先减后消耗"、另一半是"先消耗后减"。这两处不统一都是用户明确要求理顺的（"要统一一下"）。选"函数
自己全权管理 + 消耗在前"这一版是因为：(a) 这样每个 `finish_*` 函数是完全自洽的单元，调用方不用记住
"调用我之前你还得自己 `++`"这种隐藏契约；(b) 跟开头"消耗在前、入栈在后"的顺序对称一致；(c)
`expect()` 抛异常时 Parser 整体废弃（不捕获继续解析），所以两种收尾顺序在行为上完全等价，纯粹是风格
选择，选对称的那个更好记。后来把整数计数器换成 `Bracket` 栈是另一次改动，动机见上一节，跟这里的
"函数自己全权管理"约定是正交的，栈化之后约定本身没变，只是操作对象从"整数加减"变成了"入栈出栈"。

## how to apply

以后往 Parser.cpp 加新的、处理括号配对的 `finish_*` 函数，直接照抄这个约定：起始/收尾括号和
`brackets_` 的入栈出栈全部由函数自己管（`expect_open`/`expect_close` 已经把顺序和核对都做好了，
直接调用即可，不需要手写 push/pop）。

需要"同一段括号内容、包成不同节点"时（`import`/`eval` 的调用形态就是这样），**不要**为此把
`finish_*` 再拆一层出参版本——直接调现成的那个、再把结果重新组装成需要的节点。用户明确要求这样，
理由是只保留一个入口更统一，而且 `finish_*` 一族其余成员都是按值返回、没有出参风格。相应地
`finish_call_args()` 的返回类型是具体的 `CallArgs`（普通调用、`import`/`eval` 三处调用方各自把它
包进 `AstNodeCall`/`AstNodeImportCall`/`AstNodeEval`，不需要 `dynamic_cast`）。
