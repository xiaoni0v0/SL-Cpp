# Parser.cpp：辅助逻辑该当类成员还是局部 lambda

宁愿某个函数稍微长一丢丢，也不要把过多不同层级、逻辑上不并列的东西堆进类的成员列表。一个私有方法要
够格单独占一行成员，必须满足以下之一：

1. 被多个调用者真正复用；
2. 即便只有一个调用者，本身也是一个独立、有名字的语法产生式/通用原语（比如链式比较、链式 `is`、
   字典与集合字面量、复合表达式这种在 SL.md 里明确成节的概念，或者 `skip_paren_newline`、
   `assign_allowed` 这种对括号栈的查询）。

否则——哪怕这段逻辑语法上完全自洽、哪怕它对应 SL.md 里一个正儿八经的产生式（如 `ONE_PARAM`/
`ONE_CAPTURE`）——只要离开唯一的调用者就没有独立意义，就应该收进调用者内部当局部 lambda（`[&]`
捕获），不要单独提成 `Parser::parse_X()`。

## why

类成员列表本质是这个 parser 的"词汇表"。读者扫一眼应该只看到真正独立的语法产生式或者真正被复用的
原语，不用反查"这个私有方法是不是只在另一个私有方法里用了一次"这种内部管道细节。

`parse_func_params`、`parse_captures` 各自管理完整的列表语法；单个普通形参、单个捕获项只服务于所在的列表，
保留为局部 lambda。复合表达式的解析直接写在 `parse_compound` 中，不拆成只有一个调用者的转发层。

而 `finish_chain_compare`/`finish_chain_is`/`parse_dict_or_set` 虽然也只有一个调用点，但因为体量大（带自己的
局部状态、循环）且对应 SL.md 里明确成节的语法概念，仍然保留为独立方法：判断标尺不是单纯数调用点个数，
而是"这个方法在类的词汇表里有没有资格单独占一行"。这类拿不准的个案，用户让 AI 自己判断，倾向是保留
独立方法，不为了"贯彻内联原则"牺牲 `parse_expr_pratt` 这个核心调度函数的精简。

`finish_assign` 统一处理赋值、声明和复合赋值；`parse_annotation` 读取冒号及其后的 `const`、类型，
供声明、形参和绑定共用。控制头部先解析主表达式，再调用 `parse_binding` 读取可选的 `as binding` 部分。
`parse_binding` 返回 `Binding`，只处理目标和注解；外层决定赋给目标的值。普通声明的初始化值直接在
`finish_assign` 中读取，不拆出只有一个调用者的声明头尾函数。
`finish_assign` 和 `parse_binding` 各自保留带注解时的标识符检查，注解的解析已经共用；这几行检查留在调用处。

完整读取一个语法成分的函数用 `parse_`，接收已解析的前半部分并继续构造节点的函数用 `finish_`。
捕获、形参、实参列表分别由 `parse_captures`、`parse_func_params`、`parse_call_args` 读取；
`finish_assign`、`finish_call`、`finish_index` 需要已解析的表达式或对象，保留 `finish_`。
调用与索引用 `finish_call`、`finish_index` 作为对称的后缀构造入口。`finish_call` 将对象与实参表组成调用节点，
实参语法仍由 `parse_call_args` 管理，供 `import`、`eval` 共用；索引保留独立函数管理自己的实参规则。
链式比较和 is 链也接收已解析的左操作数与首个运算符，分别用 `finish_chain_compare`、`finish_chain_is`。

只有转发、没有独立逻辑的方法，不因调用点多就保留。类型注解直接调用 `parse_expr_pratt(kAboveAssignBp)`。

## how to apply

往这个项目的 Parser.cpp 加辅助解析逻辑时，默认先问"这段逻辑有没有第二个调用者，或者它本身是不是一个
独立的语法概念（体量大、带自己的局部状态/循环也算数）"——都没有就直接写成局部 lambda，不要急着提成
类方法。`parse_paren_or_tuple` 里先手动 `check`+报错、再调用 `expect()` 去消耗 token 这种"看起来重复
检查"的写法，是刻意的，见 [parser-prefer-expect.md](parser-prefer-expect.md)。
