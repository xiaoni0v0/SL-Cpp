# Parser.cpp：辅助逻辑该当类成员还是局部 lambda

宁愿某个函数稍微长一丢丢，也不要把过多不同层级、逻辑上不并列的东西堆进类的成员列表。一个私有方法要
够格单独占一行成员，必须满足以下之一：

1. 被多个调用者真正复用；
2. 即便只有一个调用者，本身也是一个独立、有名字的语法产生式/通用原语（比如链式比较、链式 `is`、
   字典字面量这种在 SL.md 里明确成节的概念）。

否则——哪怕这段逻辑语法上完全自洽、哪怕它对应 SL.md 里一个正儿八经的产生式（如 `ONE_PARAM`/
`ONE_CAPTURE`）——只要离开唯一的调用者就没有独立意义，就应该收进调用者内部当局部 lambda（`[&]`
捕获），不要单独提成 `Parser::parse_X()`。

## why

类成员列表本质是这个 parser 的"词汇表"。读者扫一眼应该只看到真正独立的语法产生式或者真正被复用的
原语，不用反查"这个私有方法是不是只在另一个私有方法里用了一次"这种内部管道细节。用户在实际重构中
两次验证了这条标尺：

- `finish_func_params`/`finish_func_captures` 把原本独立的 `parse_func_param`/`parse_func_capture`
  （各自只有一个调用者）内联成了局部 lambda `parse_one_param`/`parse_one_capture`；
- `finish_brace_block`（此前是拆出的"瘦壳 + 干活函数"两段式，但 `finish_brace_block` 自始至终只有
  一个调用者）被合并回一个 `parse_brace()`。

而 `parse_chain_compare`/`parse_chain_is`/`finish_dict` 虽然也只有一个调用点，但因为体量大（带自己的
局部状态、循环）且对应 SL.md 里明确成节的语法概念，仍然保留为独立方法——这印证了判断标尺不是单纯数
调用点个数，而是"这个方法在类的词汇表里有没有资格单独占一行"。这三个是否该内联成 lambda 曾经拿不准，
问过用户，用户明确说这个具体要不要合是他自己也没想清楚的偶然情况、让 AI 自己判断——判断结果是继续
保留独立方法（体量大 + 独立语法概念这两条本身就够格，不需要为了"贯彻内联原则"而牺牲 `parse_expr_pratt`/
`parse_brace` 这两个核心调度函数的精简）。

## how to apply

往这个项目的 Parser.cpp 加辅助解析逻辑时，默认先问"这段逻辑有没有第二个调用者，或者它本身是不是一个
独立的语法概念（体量大、带自己的局部状态/循环也算数）"——都没有就直接写成局部 lambda，不要急着提成
类方法。`parse_paren_or_tuple` 里先手动 `check`+报错、再调用 `expect()` 去消耗 token 这种"看起来重复
检查"的写法，是刻意的，见 [parser-prefer-expect.md](parser-prefer-expect.md)。
