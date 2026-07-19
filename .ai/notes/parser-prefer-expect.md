# Parser.cpp：消耗确定 token 一律用 expect，不用裸 advance

只要某次 `advance()` 消耗的 token 类型在那个调用点是**唯一确定**的（不管是靠前面一个 `check(X)`、
外层 `if(check(X))` 分支、还是 `switch(type) { case X: ... }` 的 case 标签本身保证的），就应该写成
`expect(TokenType::X)`，不要写裸 `advance()`——哪怕这意味着同一个条件看起来被判断了两次（比如
`if (!check(X)) error(自定义文案); expect(X);` 这种先手动 check 报自定义错误、再调用 expect 去消耗
的写法）。

只有当一次 `advance()` 消耗的 token 类型本身不唯一（比如 Pratt 循环里消耗"任意一个中缀运算符"、
`parse_expr_as_cond` 里消耗"11 种复合赋值运算符之一"、字面量解析里 `+`/`-`/`~` 三个 case 共用一段
代码时消耗"三者之一"）才继续用裸 `advance()`——这种场景类型不唯一，`expect()` 单参数签名本来就套
不上。

## why

用 `expect()` 而不是裸 `advance()` 能在"我们以为当前 token 一定是 X，但因为别处的 bug 实际不是"这种
情况下立刻抛出清晰的"expected X but got Y"错误，而不是静默吃掉错误的 token、让解析继续跑偏，产生一个
远离真正错误位置的、难以排查的下游报错或者错误的 AST。用户明确认为这点性能损耗（一次 enum 比较）完全
可以忽略，安全性收益更值。

## how to apply

往这个项目的 Parser.cpp 写代码时，任何"消耗一个类型已经确定的 token"都默认用 `expect(TokenType::X)`；
只有消耗"若干种可能类型之一"时才用裸 `advance()`。`expect()`/`skip_newline()`/`skip_paren_newline()`/
`skip_terminator()` 自己内部的 `advance()` 是最底层原语，不在此规则适用范围内（不要把 `expect()`
套进它自己身上）。
