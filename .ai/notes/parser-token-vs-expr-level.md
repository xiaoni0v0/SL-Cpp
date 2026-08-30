# Parser：某个槽位该收紧到 token 级别，还是走通用表达式再交给语义层判形状

判断标尺，三问：

1. 这个槽位有没有被 SL.md 定义成一条独立、单一、不含表达式递归的产生式？
2. 有没有语法歧义需要拖到更后的 token 才能消解？
3. 这个槽位是不是**整个构造的返回值**（会被 `parse_non_op` 直接交回外层 `parse_expr_pratt` 的
   中缀/后缀循环）？如果是，提前收手（只吃一个 token 就返回）会不会让紧跟着的 `.`/`(`/`+` 等
   被外层误当成对这个构造的运算，拼出一个**保证出错、且这个错本来编译期就能确定**的表达式？

只有 1 成立、2 不成立、3 也不成立（或者这个槽位根本不是"整个构造的返回值"，比如函数名、
关键字实参名这种只是子字段的情形），才直接在 Parser 里 `expect(对应 token)`。2 成立
（当前 token 还不够判断走哪条产生式）→ 只能先按通用表达式解析，形状校验推迟到语义层。3 成立
（提前收手会让外层拼出保证出错的表达式）→ 同样改用 `parse_expr()`，但形状校验直接在 Parser 里做
完、就地析构成最终字段，不留给语义层（因为最终 AST 节点的字段形状本来就该收紧到"不可能是别的
东西"，不是靠语义层拿着一个宽泛的 `AstNodePtr` 反复 `dynamic_cast`）。

## why

`global identifier`（`AstNodeGlobal::identifier_: u32string`）和 `import identifier.identifier
...`（`AstNodeImportKw::segments_: vector<u32string>`）曾经都是"直接 `expect(IDENTIFIER)`"的
反面教材，问题出在第 3 问：只吃一个 token 就返回后，`global x.y` 里的 `.y`、`import a.b(x)` 里的
`(x)` 会被外层 `parse_expr_pratt` 的循环当成对整个 `global x`/`import a.b` 的运算，拼出
`Attr(Global(x),"y")`、`Call(ImportKw([a,b]),[x])`——而 `global` 的值恒为 `None`
（3.4.4）、模块对象恒不可调用（3.4.5 易错提醒），这两种拼法**保证**跑不通，编译期就能确定，
放行到运行期纯属白白推迟报错。现在两者都改成 `parse_expr()` 整条吃完、再校验形状（`global`
必须是纯 `Identifier`；`import` 必须是 `Identifier` 或一路 `Attr` 到底的属性访问链，展开进
`segments_`），这样 `.y`/`(x)` 会被吃进目标本身、校验时直接拒绝，不会泄漏给外层。

对照组：`for` 迭代模式和 `except` 的绑定目标（`for (iterable as lvalue)`、`except (E as
lvalue)`）走 2 那条——目标允许标识符/属性访问/索引/解构元组或列表等多种形状，产生式不单一，
按表达式解析后校验交给语义层 `check_lvalue`（因为这里的目标字段本来就该是宽泛的
`AstNodePtr`，可以是任意左值，不像 `global`/`import` 的字段被收紧成必须是纯标识符/纯路径）。
`del target` 同理（`check_lvalue_pure` 承担这一步）。

反过来，`as` 这个**记号本身**、函数/类的可选名字这类"只是子字段、不是整个构造返回值"的槽位，
不受第 3 问牵连：`as` 不是运算符，只在头部里出现，当前 token 就够判断，`check(KW_AS)` +
`expect(KW_AS)` 直接收紧没问题——它不会被交回外层 Pratt 循环，没有"泄漏"这一说。

## 用 `parse_expr()` 收紧目标形状时，先探路避免报错串味

`global`/`import` 这两处在调用 `parse_expr()` 之前，都先用一次不消耗 token 的 `check(IDENTIFIER)`
探路。原因：目标位置一旦撞见关键字（`global class`、`import as`），直接扔给 `parse_expr()` 会一头
扎进 `parse_class()` 之类的深层产生式，报出跟"这里需要一个标识符"毫不相关的错误。探路一下能把这类
输入挡在最外面，报错跟原来 `expect(IDENTIFIER)` 一样干脆。

## how to apply

往 Parser 加新槽位、或者审查现有槽位"是不是收得太松/太紧"时，先问这三个问题。1、2 不成立且这个
槽位就是整个构造的返回值时，额外确认第 3 问：提前收手会不会被外层 Pratt 循环拼出一个保证出错的
表达式。三问都过了才收紧到 `expect()`；第 2 问成立就走 `parse_expr()` + 语义层校验（字段留
`AstNodePtr`）；第 3 问成立也走 `parse_expr()`，但校验和析构放在 Parser 里、就地收紧成最终字段
形状（不留一个宽泛的 `AstNodePtr` 给语义层反复 `dynamic_cast`），且校验前先探路避免报错串味。
别为了"看起来更严格"而在语法层做本该在语义层才能安全完成的判断，但也别为了"这里看着该用
`expect()`"而忽视第 3 问，放过一个能在编译期截住的必错构造。
