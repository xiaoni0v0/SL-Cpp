# SL.md 已改、实现还没跟上的地方

类型系统、`const`、单元、懒注解、范围运算这一轮改的是 SL.md，代码一行没动。这份清单按编译和运行的
各层列出要改的东西，改实现时逐条对照 SL.md 的对应章节，做完一条删一条，全部做完后删掉这个文件。
设计理由不在这里，查 [context.md](../context.md) 里的对应小节。

## 词法

- 关键字新增 `from`、`const`；`as` 从关键字退回保留字（`x_keyword.inc` / `x_reservedword.inc`）。
- 新 token `..=`，优先级、结合性与 `..` 相同（110，左结合）。
- `$` 与 `{` 仍是两个独立 token，字典、集合字面量由语法分析识别这个组合，二者之间可以有空白。

## 语法分析和 AST

- `{}` 一律是复合表达式（`func`、`class` 之后是函数体、类体），删掉"第一个表达式后跟 `:` 就是字典"的判别。
- 字典、集合字面量改成 `${...}`：`${}` 是空字典，`${k: v}` 是字典，`${a, b}` 是集合，分别支持 `**` 与 `*`
  展开。键值对和非键值对混用、`*` 与 `**` 混用都是 `SyntaxError`。项按高于 `=` 的优先级解析。
  `$` 后面不是 `{` 抛 `SyntaxError`。现有的 `AstNodeLiteralDict` 要改，另加集合字面量节点。
- 赋值和声明合成一个"赋值"节点：目标、类型、值三个字段，外加 `const` 标志。类型和值可空但不同时为空；
  类型非空时目标只能是标识符。复合赋值保持独立节点，不带类型。语法是
  `identifier : ⟦const⟧ ⟦type⟧ ⟦= expr⟧`，`const` 与 `type` 至少写一个；`type` 按高于 `=` 的优先级解析。
- 带类型的赋值不能直接作为字典、集合的项和调用实参。`cond` 位置只拒绝裸赋值 `target = expr`，带类型的放行。
- 类型位置（声明、形参注解、返回注解、`for`/`except` 的 `: type`）在解析期按节点类型判断最外层是不是匿名
  函数表达式，带名字的函数表达式在此处是 `SyntaxError`。字符串字面量也在这里特判。
- 形参注解和返回注解前允许写 `const`。
- `for` 迭代模式改成 `for ⟦collect⟧ (⟦target ⟦: type⟧ from⟧ iterable)`，不再是 `iterable as lvalue`。
  带 `: type` 时目标只能是标识符。
- `except` 改成 `except (⟦target ⟦: type⟧ from⟧ exc_type)`，不再有逗号分隔的异常类列表，多个异常类由用户
  用 `|` 拼。带 `: type` 时目标只能是标识符。
- `..=` 作为二元运算符加入 Pratt 表。

## 语义分析和折叠

- 引用捕获的检查规则跟着单元的语义调整（见下），定义帧的概念去掉。
- 折叠器：空字典的写法变成 `${}`；`$$` 零轮循环折出的空字典不变；`..=` 的 range 要么不折，要么带上闭区间
  标志一起折。

## 字节码和 codegen

- 声明需要新指令或扩展存储指令，带上类型和 `const` 标志。无值声明给存储指令推 `Missing` 哨兵，VM 遇到
  `Missing` 只换声明不绑定；表达式要值时不能 `COPY` 这个 `Missing`，要另压 `None`。
- 声明的类型何时求值：函数的形参、返回注解和类体里的声明，类型编译成无形参、无捕获的函数，第一次检查时
  调用并缓存；其余位置就地求值，先 `T` 后 `v`。最外层是匿名函数表达式时直接用它，以无实参调用。
- 引用捕获改成单元：`MAKE_FUNC` / `MAKE_CLASS` 取的是单元而不是名字集合加定义帧；定义处的名字本身是引用
  捕获来的，直接转发那个单元。函数、类对象记住建立时的 `_G`。
- `except` 的匹配就是 `isinstance(异常, exc_type)`，绑定走赋值的通用路径。
- `..=` 对应一个新的 `BINARY_OP` 运算符。
- 类体收集属性时，单元取其中的值；类体帧 `_L` 里的声明变成 `attrs(类)` 的声明。

## 运行时

- `Namespace`：`dict` 的子类，`_L`、`_G`、对象属性表都是它。额外一张"名字 → 声明（类型、是否 const）"的表；
  非声明的写入检查 `const` 和类型，删除遇 `const` 抛 `TypeError`。支持单元：引用捕获把值和声明搬进单元，
  之后读写删都穿过单元，删除只清空单元。对象属性表的声明还要沿 MRO 从所属类继承。
- 帧没有"死亡"了，弹出即可回收；函数不再持有定义帧。
- `isinstance(x, T)` 恒等于 `issubclass(type(x), T)`，删掉 `__instance_check__`，保留 `__subclass_check__`。
- 复合类只剩 `|`、`!`、`?`；按结构比较相等、哈希一致；子类判定按分支包含；所有要求类型的地方 `None` 都当
  `NoneType`，`None | int` 要靠 `type.__rop_or__` 接住。
- 带约束的容器类：`tuple`、`list`、`dict`、`unordered_dict`、`frozendict`、`set`、`frozenset` 的 `[...]`
  动态生成子类。同参数的类相等但不保证同一对象，元素全是 `object` 时得到容器类本身；子类关系按元素类型协变；
  创建和增改元素时检查，派生容器保持同类；各容器加 `typed(*args)` 方法，按值递归转换。
- 删掉 `TypeVar`。
- range 带"是否包含 stop"的标志；`..=`、`__op_range_closed__` / `__rop_range_closed__`；三元形式的第二个运算符
  只能是 `..`；`range(..., closed=True)`。
- 函数调用：带类型注解的形参在局部帧里加上声明（`const` 生效）；注解首次检查时求值并缓存；实参不符是
  `DispatchError`，函数体内违反声明是 `TypeError`；定义时不再检查默认值是否符合注解，调用时随实参一起检查。
- 类：类体里的声明成为属性的声明，子类重声明的子类型检查在新声明首次求值时做。
- `eval_isolated` 的 `globals` 要求是 `Namespace`。

## 跟着代码一起改的文档

- `compiler/codegen/bytecode.md`：`ByteCodeFrame` 的 `locals_` 还写着"帧死亡时置为 `None`"，`owner_` 还提到
  定义帧和 `TypeVar`，作用域那段还在转发到定义帧，`for` 的编译模式还是 `iterable as lvalue`。
- `.ai/architecture.md` 和 `notes/parser-token-vs-expr-level.md` 里描述 `as` 解析方式的几处，代码改成 `from`
  之后同步改。
