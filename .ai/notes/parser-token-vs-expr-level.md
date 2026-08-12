# Parser：某个槽位该收紧到 token 级别，还是走通用表达式再交给语义层判形状

判断标尺，两问：

1. 这个槽位有没有被 SL.md 定义成一条独立、单一、不含表达式递归的产生式？
2. 有没有语法歧义需要拖到更后的 token 才能消解？

第 1 条成立、第 2 条不成立 → 直接在 Parser 里 `expect(对应 token)`，不用走 `parse_expr()` 再在
语义层反查形状。第 2 条成立（当前位置的 token 还不够判断走哪条产生式）→ 只能先按通用表达式解析，
形状校验推迟到语义层。

## why

`global identifier` 属于前者：`identifier` 就是紧跟着的一个标识符 token，没有任何歧义，直接
`expect(IDENTIFIER)` 最省事也最明确——语义层不需要再费一次 `dynamic_cast` 去确认这是不是标识符。

`for` 迭代模式的 `lvalue`（`for (lvalue : iterable) ...`）属于后者：当前 token 只是槽位开头，得
读到 `:`/`;`/`)` 才能判断这一段到底是迭代模式的 `lvalue` 还是步进模式的 `init`，没法提前收紧。
`del target` 语法上允许标识符或属性访问/索引等多种形状，也是"语法先按表达式解析、语义层限定形状"
的合理场景（`check_lvalue`/`check_lvalue_pure` 承担这一步）。

## how to apply

往 Parser 加新槽位、或者审查现有槽位"是不是收得太松/太紧"时，先问这两个问题；只有两条都满足
（产生式单一 + 当前 token 已足够判断）才收紧到 `expect()`，否则老实用 `parse_expr()` 通用解析，
把形状校验留给 `SemanticChecker`。别为了"看起来更严格"而在语法层做本该在语义层才能安全完成的判断。
