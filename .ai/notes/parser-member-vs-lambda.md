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
原语，不用反查"这个私有方法是不是只在另一个私有方法里用了一次"这种内部管道细节。用户在实际重构中
两次验证了这条标尺：

- `finish_func_params`/`finish_func_captures` 把原本独立的 `parse_func_param`/`parse_func_capture`
  （各自只有一个调用者）内联成了局部 lambda `parse_one_param`/`parse_one_capture`；
- 曾经拆成"瘦壳 + 干活函数"两段的 `{}` 解析（干活的那个自始至终只有一个调用者）被合并回一个函数。

而 `parse_chain_compare`/`parse_chain_is`/`parse_dict_or_set` 虽然也只有一个调用点，但因为体量大（带自己的
局部状态、循环）且对应 SL.md 里明确成节的语法概念，仍然保留为独立方法：判断标尺不是单纯数调用点个数，
而是"这个方法在类的词汇表里有没有资格单独占一行"。这类拿不准的个案，用户让 AI 自己判断，倾向是保留
独立方法，不为了"贯彻内联原则"牺牲 `parse_expr_pratt` 这个核心调度函数的精简。

赋值一档的几个成员（`finish_assign`、`parse_annotation`、`parse_decl_head`、`finish_decl`、`parse_type`、
`parse_from_clause`）都有两个以上调用者：Pratt 循环、条件、for 头部、except 头部、形参注解、返回值注解各取所需。

## how to apply

往这个项目的 Parser.cpp 加辅助解析逻辑时，默认先问"这段逻辑有没有第二个调用者，或者它本身是不是一个
独立的语法概念（体量大、带自己的局部状态/循环也算数）"——都没有就直接写成局部 lambda，不要急着提成
类方法。`parse_paren_or_tuple` 里先手动 `check`+报错、再调用 `expect()` 去消耗 token 这种"看起来重复
检查"的写法，是刻意的，见 [parser-prefer-expect.md](parser-prefer-expect.md)。
