# SL.md 已改、实现还没跟上的地方

类型系统、`const`、单元、懒注解、范围运算这一轮改的是 SL.md。词法和语法分析已经跟上，后面各层还停在旧
AST 上，暂时编不过，`run_test.py` 构建 all 目标会失败；在那之前只单独构建 `sl_test_lexer`、`sl_test_parser`
两个目标来验证。这份清单按层列出剩下要改的东西，改实现时逐条对照 SL.md 的对应章节，做完一条删一条，全部
做完后删掉这个文件。设计理由不在这里，查 [context.md](../context.md) 里的对应小节。

## AST 已经变了的地方

Analyzer、CodeGen 和它们的测试都要按下面这些跟上（测试源码里的旧写法 `as`、`{k: v}`、`except (A, B)` 也要换成 `from`、`${k -> v}`、`A | B`）：

- 注解是 `ast_node_misc.h` 里的 `Annotation { is_const_, type_ }`，四处共用，没写注解时是 `{false, nullptr}`。
  消费方各写一个吃 `Annotation` 的辅助函数（JSON dumper 已有 `dump_annotation`），跟 `CallArgs` 一样。
- `AstNodeAssign` 字段是 `target_`、`annotation_`、`value_`，`value_` 可空。写了注解即为声明，此时 Parser 已保证
  `target_` 是标识符；没写注解就是裸赋值，`value_` 非空。
- 新节点 `AstNodeLiteralSet`，`items_` 非空，项里可以有 `*expr`，不会有 `**expr`。`AstNodeLiteralDict` 的
  `items_` 可空（`${}`）。
- `for`/`except` 的绑定是 `ast_node_misc.h` 里的 `Binding { target_, annotation_ }`，节点里放
  `std::optional<Binding> binding_`，没写 `from` 时是 `nullopt`，连注解一起没有。`target_` 非空，写了注解时是标识符。
- `AstNodeForIter` 字段按源码顺序变成 `collect_`、`binding_`、`iterable_`、`body_`。
- `AstNodeTry::AstNodeExceptAndExpr` 的 `exceptions_` 列表换成单个 `exc_type_`，字段顺序 `binding_`、
  `exc_type_`、`body_`。
- `AstNodeFunc::OneParam` 字段是 `identifier_`、`annotation_`、`default_value_`，注解可以只有 `const`
  （`func f(x: const)`）。返回值注解不能写 `const`，Parser 直接报错。
- `AstNodeOpBinary::OpType` 多了 `RangeClosed`（`..=`）。
- 类型位置上最外层是函数表达式时，Parser 已保证它是匿名的。最外层是匿名函数表达式还是字符串字面量，后面
  各层按节点类型自己判断（`dynamic_cast`），AST 上没有额外标记。

## 语义分析和折叠

- 上面每个新字段在 `SemanticChecker`、`ExprFolder` 里都要访问到（新槽位编译器不会提醒，见
  [architecture.md](../architecture.md) 的 AST 节点一节）。声明的目标不必再查左值；`for`、`except` 带类型时
  目标同理。集合字面量的项允许 `*`。
- 引用捕获的检查规则跟着单元的语义调整（见下），定义帧的概念去掉。
- 折叠器：`$$` 零轮循环折出的空字典不变；`..=` 的 range 要么不折，要么带上闭区间标志一起折；集合字面量
  每次求值都新建对象，跟 `list` 一样不能进常量表。

## 字节码和 codegen

- 声明需要新指令或扩展存储指令，带上类型和 `const` 标志。无值声明给存储指令推 `Missing` 哨兵，VM 遇到
  `Missing` 只换声明不绑定；表达式要值时不能 `COPY` 这个 `Missing`，要另压 `None`。
- 声明的类型何时求值：函数的形参、返回注解和类体里的声明，类型编译成无形参、无捕获的函数，第一次检查时
  调用并缓存；其余位置就地求值，先 `T` 后 `v`。最外层是匿名函数表达式时直接用它，以无实参调用。
- 引用捕获改成单元：`MAKE_FUNC` / `MAKE_CLASS` 取的是单元而不是名字集合加定义帧；定义处的名字本身是引用
  捕获来的，直接转发那个单元。函数、类对象记住建立时的 `_G`。
- `except` 先检查 `issubclass(exc_type, BaseException)`，不满足抛 `TypeError`；匹配就是
  `isinstance(异常, exc_type)`，绑定走赋值的通用路径。
- `..=` 对应一个新的 `BINARY_OP` 运算符。
- 类体收集属性时，单元取其中的值；类体帧 `_L` 里的声明变成 `attrs(类)` 的声明。

## 运行时

- `Namespace`：`dict` 的子类，`_L`、`_G`、对象属性表都是它。额外一张"名字 → 声明（类型、是否 const）"的表；
  非声明的写入检查类型，声明带 `const` 且名字已有值时抛 `TypeError`（没值时可以写一次），删除遇 `const` 抛
  `TypeError`。类型是字符串时照常记录声明（类型就存这个字符串），只是检查一律通过，`const` 照常。支持单元：
  引用捕获把值和声明搬进单元，之后读写删都穿过单元，删除清空值和声明，保留单元及名字与单元的关联。
  只有值作为字典项存在；只声明未绑定的名字也可以删除，带 `const` 时仍报 `TypeError`。对象属性表的声明和"已有值"都要沿 MRO 查：
  自己的表，再所属类 MRO 上各类的表。
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
- 函数调用：带注解的形参在局部帧里加上声明（`const` 生效）；注解里的类型首次检查时求值并缓存；实参不符是
  `DispatchError`，函数体内违反声明是 `TypeError`；定义时不再检查默认值是否符合注解，调用时随实参一起检查。
- 类：收集属性时，类体里声明过的名字连同声明原样存入 `attrs(类)`，不再检查，也不看 MRO 上的同名声明；其余名字
  按普通写入，检查继承来的类型和 `const`（基类 `n: const = 1` 时子类类体写 `n = 2` 报错）。没有子类型检查。
- 声明：新声明整个替换旧声明，旧声明的 `const` 不影响声明本身；只写声明时若名字已有值，按新类型检查。
- `eval_isolated` 的 `globals` 要求是 `Namespace`。

## 跟着代码一起改的文档

- `compiler/codegen/bytecode.md`：`ByteCodeFrame` 的 `locals_` 还写着"帧死亡时置为 `None`"，`owner_` 还提到
  定义帧和 `TypeVar`，作用域那段还在转发到定义帧，`for`、`except` 的编译模式还写着 `as`。
- `.ai/architecture.md` 的"当前完成度"在 Analyzer、CodeGen 跟上之后去掉"停在旧 AST 上"那句。
