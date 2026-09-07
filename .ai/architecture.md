# 项目结构 / 代码索引

新会话、或者接手这个项目的另一个 AI，从这份文件开始：这里是"东西在哪、大概长什么样"的地图，不重复
代码本身写了什么。语言的行为契约见 [SL.md](../SL.md)；设计决策的"为什么"见 [context.md](context.md)；
具体的代码风格约定/踩过的坑见 [notes/](notes/README.md)。

## 整体流水线

```
源码 (.sl)
  → Lexer      (compiler/lexer/)     词法分析，产出 Token 序列
  → Parser     (compiler/parser/)    语法分析，产出 AST
  → Analyzer   (compiler/analyzer/)  语义检查 + 编译期常量折叠，原地改 AST
  → CodeGen    (compiler/codegen/)   AST → Code（字节码），还没写
  → Executor   (executor/)           跑字节码的虚拟机，还没写
```

**但这条流水线不是启动顺序**：运行时（`runtime/`：对象模型/GC/基础类型）先于编译器启动，见下面
「运行时先于编译器」。这也是链接依赖的方向——`runtime` 在 `compiler` **下面**（codegen 要造真对象），
而虚拟机主循环因为 `eval` 反过来依赖 `compiler`。

`main.cpp` 目前只是个手工调用这条流水线、把每一步中间结果打印出来的调试入口，不是真正的解释器入口。

**当前完成度**：Lexer/Parser 已实现且有完整测试；Analyzer 的两个子系统（语义检查、常量折叠）已实现
且有完整测试；`numeric/` 的 `BigInt`（`int` 的底层）和 `BigDec`+`DecContext`（`decimal` 的底层）已实现
且有完整测试；`compiler/codegen/` 有了常量池（`ConstPool`）和设计文档 [`bytecode.md`](../compiler/codegen/bytecode.md)、`CodeGen` 本身还没写，
`executor/` 目前只是串流水线的驱动。`runtime/` 已经有对象模型骨架（`Object`+`Ref`、`Type`、
四个基础类型、单例）、GC（引用计数 + 标记清扫）、bootstrap 和异常体系，帧与主循环还没开始——实现顺序见下面「实现路线」。
**在虚拟机落地之前，SL.md 里"运行时"相关的条文（属性协议、GC、异常传播的具体机制等）大多还没有对应
实现可以参照，只能靠 SL.md 文本本身。**

**`BigDec` 的完成度**：SL.md 里 decimal 参与的**运算符全都实现了**——四则、`//`/`%`、比较、`**`，
外加 `sqrt`/`exp`/`ln`/`log10`（`**` 的一般情形要靠它们）。还没做的是 `hash`（要跟数值相等的 `int`
一致，得先归一标度）和 `int(decimal)`（取整方向 SL.md 还没定），见 [context.md](context.md) 的
悬而未决一节。

## 目录一览

| 目录 | 内容 |
|---|---|
| `compiler/` | 编译期那几层的收纳目录，本身只有一个 `CMakeLists.txt`。注意 **纯 C++ 层 / SL 层的分界是按模块划的不是按目录**：里面 `lexer`/`parser`/`analyzer` 是纯 C++ 层，`codegen` 要造真 SL 对象、属于 SL 层，见 [notes/cpp-layer-vs-sl-layer.md](notes/cpp-layer-vs-sl-layer.md)。 |
| `compiler/lexer/` | `Lexer.{h,cpp}`：分词器。`token.h` 定义 `Token`；`x_token_type.inc`/`x_keyword.inc`/`x_reservedword.inc` 是 X-macro 列表（见下）。 |
| `compiler/parser/` | `Parser.{h,cpp}`：递归下降 + Pratt 解析器，产出 `ast_nodes/` 里定义的 AST。 |
| `compiler/parser/ast_nodes/` | AST 节点类型定义。`ast_nodes.h` 是汇总头（引入 `details/` 下所有节点头）；`x_ast_nodes.inc` 是全部节点类型的 X-macro 列表；`ast_json_dumper.h`/`.cpp` 定义 `AstJsonDumper : public AstConstVisitor`，把 AST 序列化成 JSON（调试/测试用，不是语言语义的一部分），入口是静态方法 `AstJsonDumper::dump(node, include_pos)`，内部靠每个节点一个 `visit()` + `result_` 成员当通道完成（`.ai/notes/visitor-result-passing.md` 那个模式）；`ast_visitor.h` 定义 `AstVisitor`/`AstConstVisitor`（会改树的、只读的两套）和 `SL_AST_NODE_ACCEPT` 宏，要遍历 AST 的类继承它们，靠 `accept` + `visit` 两次虚调用完成双分派——漏实现某个节点类型是编译期错误，不是运行期 assert。 |
| `compiler/parser/ast_nodes/details/` | 具体节点定义，按语法范畴分文件（`ast_node_class.h`、`ast_node_control_flows.h`、`ast_node_func.h`、`ast_node_import.h`、`ast_node_literals.h`、`ast_node_multi_exprs.h`（Program/Compound）、`ast_node_operators.h`、`ast_node_postfix.h`（call/index/attr）、`ast_node_var.h`（del/global/标识符）、`ast_node_decorators.h`、`ast_node_eval.h`）。`ast_node_misc.h` 放**不是** `AstNode`、但被多个节点类型共用的小聚合体（`OneCapture`、`OneKwArg`、`CallArgs`）。唯一的 `.cpp` 是 `ast_node_literals.cpp`：int/decimal 字面量的构造函数在这里校验 `raw_` 的形状（纯数字/前导零/科学计数法后缀/可选的前导负号），违反即 `InternalError`——常量折叠造出来的字面量节点不会再经过 `SemanticChecker`，只有构造函数拦得住。 |
| `compiler/analyzer/` | `Analyzer.{h,cpp}`：入口，依次跑 `SemanticChecker` 和 `ExprFolder`。 |
| `compiler/analyzer/semantic_checker/` | `SemanticChecker.{h,cpp}`（`AstConstVisitor` 的实现）：语义检查（作用域规则、lvalue 合法性、`*`/`**` 位置合法性、func/class 约束、AST 结构防御性校验……），只读不改 AST，违规抛 `SyntaxError`（真实语义错误）或 `InternalError`（AST 结构本身违反 Parser 的保证，代表实现自己有 bug）。单个节点自己字段的合法性不归这里，归节点构造函数（见 `compiler/parser/ast_nodes/details/`）。 |
| `compiler/analyzer/expr_folder/` | `ExprFolder.{h,cpp}`（`AstVisitor` 的实现）：遍历 + 原地替换 AST 的调度层，拥有 `AstNodePtr` 槽位的所有权。`StaticEvaler.{h,cpp}`：纯函数式的"给一个节点判断能不能折、折成什么"，不遍历树、不拥有节点。 |
| `numeric/` | `BigInt.{h,cpp}`：手写高精度整数，`int` 的底层实现（`小路径 int64_t` / `大路径 limbs` 双表示）。`BigDec.{h,cpp}`：十进制浮点数，`decimal` 的底层实现（`BigInt 系数 + int64_t 指数 + 独立符号位 + 特殊值 tag`）。`DecContext.{h,cpp}`：`decimal.Context` 的底层实现——舍入方式、精度、指数范围、信号的陷阱/标志位，以及陷阱触发时抛的 `DecTrapped`。`dec_math.{h,cpp}`：`ln`/`log10`/`exp`/`**` 用的整数层定点算法（`ilog`/`iexp`/`dlog`/`dexp`/`dpower` 等），只跟 BigInt 打交道，不认识上下文和信号。 |
| `runtime/` | 运行时的地基，**在编译器下面**（`compiler/` 的 codegen 要直接造真 SL 对象）。`Object.h`：一切 SL 对象的基类 + 对象头（引用计数、类型指针、GC 链表与标记位）+ 引用句柄 `RefBase`/`Ref<T>` + 遍历器 `RefVisitor` + `make_ref`（唯一的建对象入口，各类型构造函数私有、只对它开放）。`Type.{h,cpp}`：类型对象（`name_`/`bases_`/`mro_`/`is_subtype_of`）。`Runtime.{h,cpp}`：全局单例，`init()` 两步建全部内置类型与单例并持有它们（都直接读私有字段，不经过公开访问器）、`shutdown()`，也是第一个 `GcRootSource`。`instance()` 只检查"init() 到底跑没跑完"（`g_runtime` 是否为空）——没有分阶段的状态机，见 [context.md](context.md)。`Heap.{h,cpp}`：全堆链表 + STW 标记清扫 `collect()` + 根源注册（`GcRootSource`）+ 按**字节**计的回收触发（`should_collect()`；字节数由 `make_ref` 在对象构造完之后记账，`collect()` 时重算存活量）。**`collect()` 只能在主循环的安全点调用**，理由见笔记。`x_builtin_types.inc`：内置类型清单（X-macro，见下）。写这里的代码前先读 [notes/object-model-conventions.md](notes/object-model-conventions.md)。 |
| `runtime/objects/` | 各具体对象类型：`Int`（裹 `BigInt`）、`Decimal`（裹 `BigDec`）、`Str`（`u32string`，按码点）、`Tuple`（`vector<ObjectRef>`）、`BaseException`（整棵异常树共用，只存 `.args` 元组）、`NamedSingleton`（无负载单例）、`Bool`、`Code`（编译好的一份代码：字节码 + 常量表 + 嵌套 `Code` 表 + 名字表 + 形参形状 + 行位置表；**是 SL 对象但 SL 层完全接触不到**，做成对象是为了让 GC 的追踪能穿过它，见 [context.md](context.md)）。 |
| `runtime/RaisedException.h` | `raise` 用的 C++ 信封（装一个 `ObjectRef`），只在一段不回调 SL、有界的 C++ 代码里 throw/catch，不是异常跨 SL 帧传播的机制——那个仍是主循环手写的显式算法，见 bytecode.md。 |
| `runtime/HostErrorConversion.{h,cpp}` | 宿主异常 → SL 异常对象的转换函数（`SyntaxError`/`EncodingError`/`FileNotFoundError` 各一个），`InternalError` 故意没有对应函数。 |
| `cpp_exceptions/` | 宿主（C++）层的异常类型（`SyntaxError`/`InternalError`/`EncodingError`/`FileNotFoundError`，都继承 `SLException`）加 `SourceLocation.h`，纯头文件。**信息以结构化字段为准，`what()` 只是构造时渲染出来的一种呈现**——转成 SL 异常对象时要的是分开的 file/row/col/message，见 [notes/cpp-layer-vs-sl-layer.md](notes/cpp-layer-vs-sl-layer.md)。跟 SL.md 文档化的、暴露给 SL 用户代码的异常类同名但不是同一个东西，是两层，见 [context.md](context.md) 的架构边界一节。放在顶层而不是 `compiler/` 下，是因为 `utils/` 也要用它，而 `utils/` 是纯 C++ 层、不能反过来依赖 `compiler/`。 |
| `utils/` | 自由函数工具：`string_utils`（UTF-8/UTF-32 互转等）、`file_utils`（读文件）、`memory_utils.h`（`mem::heap_bytes`，各对象类型 `size_bytes()` 的公共零件；**没有兜底重载**，认不出的类型是编译错误，见 [notes/object-model-conventions.md](notes/object-model-conventions.md)）。 |
| `compiler/codegen/` | [`bytecode.md`](../compiler/codegen/bytecode.md)：指令集/帧/`Code` 的完整设计，动这里之前先读它。`ConstPool.{h,cpp}`：常量表的去重池，`intern()` 返回槽号、值相等的常量恒占同一个槽（`a = (1,2)` / `b = (1,2)` 的 `a is b` 因此为真）。**去重判据是「结构标识」，跟 SL 的 `==`/`hash` 好些地方正好相反**——放在 codegen 而不是 runtime 正是为了不让它跟对象模型里的相等混淆，见 [context.md](context.md)。`CodeGen` 本身还没写。 |
| `executor/` | `Executor.{h,cpp}`：目前只是把整条编译流水线串起来、逐步打印中间结果的驱动，`main.cpp` 调它。真正的字节码虚拟机还没写，设计见 [`bytecode.md`](../compiler/codegen/bytecode.md)。 |
| `test/` | 目录结构镜像被测模块（`lexer`/`parser`/`analyzer`/`numeric`），见下。 |

## 运行时先于编译器

**决定：先把运行时（对象模型 + GC + 基础类型 + 单例）立起来，编译器建在它之上。** codegen 直接造真的
SL 对象塞进 `Code` 的常量表，不再有"编译期描述 → 运行期物化"那一层。这是 CPython 的做法：它的编译器
是跑在一个已经 `Py_Initialize()` 完的解释器里的库，所以编译期造对象和运行期造对象走同一套代码。

**决定性理由是 `eval`**：SL 把它做成关键字，要在运行期编译出新 `Code`，所以"编译器必须能在一个正在跑
的 VM 里被调用"本来就是硬需求。既然如此，再维护一条"没有 VM 也能编译"的冷启动路径就是净增成本——两条
路径、两种常量表示、两套测试。

**被否决的方案：编译期常量描述层。** 常量表存一个 `variant`（`BigInt`/`BigDec`/`u32string`/单例/子项
槽号列表），VM 加载 `Code` 时统一物化成对象。当时的理由是"codegen 不依赖对象模型与 GC，能先写完先测，
`Code` 还顺带可序列化"。否决原因：`eval` 让那个依赖无论如何都存在，于是描述层只是在一条必然存在的路径
旁边多养了一条；而且它是一个跟对象模型平行的类型层，每加一种常量都要改两处。已经写出来的 `ConstPool`
一并删掉了——它真正的资产是那套去重规则（先比类型、`decimal` 逐位比、元组比元素身份），规则记在
[`compiler/codegen/bytecode.md`](../compiler/codegen/bytecode.md) 的「常量去重」，代码没了也不丢。

**这个决定不覆盖前端的错误表示**：`SyntaxError`/`InternalError` 等仍然是纯 C++ 异常，lexer/parser/
analyzer 不该反过来依赖对象模型，编译诊断在被 SL 代码 `try` 住之前也不是 SL 值。只在 `eval` 这一个边界
上把编译失败转成 SL 异常对象。`InternalError` 更是永远不能变成 SL 异常——它是唯一没有对应 SL 类、明确
不可被 SL 捕获的那个。

## 构建：每个模块一个静态库

根 `CMakeLists.txt` 只做四件事：编译器前端识别、编译参数/预编译头的封装函数（`sl_apply_*_options`、
`sl_apply_pch`、`sl_add_module`）、拉第三方库、`add_subdirectory`。**每个模块目录有自己的
`CMakeLists.txt`**，用 `sl_add_module(sl_xxx …)` 建一个静态库并声明自己的依赖。

**库目标名 = `sl_` + 从仓库根数下来的目录路径**（`compiler/lexer/` → `sl_compiler_lexer`），
这样光看目标名就知道去哪找它的源码，嵌套模块也不会跟顶层模块重名：

```
sl_cpp_exceptions (INTERFACE，纯头文件)
    ← sl_utils ← sl_compiler_lexer ← sl_compiler_parser ← sl_compiler_analyzer ← sl_executor ← SL
    ← sl_runtime
sl_numeric (不依赖任何模块) ← sl_runtime
```

`sl_runtime` 现在还没有任何模块链接它（codegen 还没写），是根 `CMakeLists.txt` 临时把它链进 `SL`，
免得只构建 main target 时它整个被跳过。这是刻意的：一旦 `sl_compiler_codegen` 出现，它链
`sl_runtime` + `sl_compiler_analyzer`，`sl_executor` 再链 `sl_compiler_codegen`——方向从一开始就是
对的，不用回头改。

`test/CMakeLists.txt` 只定义 `sl_add_test_target(<名字> LIBS … SOURCES …)` 这个辅助函数并
`add_subdirectory` 各套件；**每个测试套件的目标定义放在它自己的目录里**（`test/lexer/CMakeLists.txt`
等），跟各模块的组织方式一致，加测试文件只改它旁边那一份清单。测试目标只列自己的测试文件，被测代码
靠 `LIBS` 链进来。**这是拆库的主要动机**——以前每个测试目标都要把被测模块的源文件清单原样抄一遍，
parser 那份抄了三处，加个文件要改三个地方。

`test/numeric/` 一个目录里有两个目标（`sl_test_numeric_bigint`/`sl_test_numeric_bigdec`，共用
`main_test.cpp`），`test/analyzer/` 会引 `../parser/test_utils.h`——这两处是套件与目录不是一一对应的
仅有例外，不必为它们再拆目录。

新增一个模块：建目录 + 写它的 `CMakeLists.txt`（一个 `sl_add_module` + 一个
`target_link_libraries`），在上级 `CMakeLists.txt` 里 `add_subdirectory`。新增一个文件：只改所属模块
那一份清单。测试套件同理。

## 实现路线

从上到下有依赖，不要跳着做。

1. ~~**对象模型骨架**~~ **已完成**，落在 `runtime/`：`Object` + `Ref<T>`；`Type`（`bases_`/`mro_`/
   `is_subtype_of`）；`Runtime` 的 bootstrap 按 `x_builtin_types.inc` 建全部内置类型，object/type
   互为对方类型的结靠"先留空、建完回填"解开（CPython 靠静态分配的类型结构体 + `PyType_Ready` 做同一件
   事，这里的类型对象是普通堆对象，回填就够了）；`None`/`True`/`False`/`Ellipsis`/`NotImplemented`/
   `StopIteration` 六个单例；`int`/`decimal`/`str`/`tuple` 四个类型。`list`/`dict`、属性表/描述器表、
   `object()` 可实例化都还没有——够常量表用即可。
2. ~~**GC**~~ **已完成**，落在 `runtime/Heap.{h,cpp}`：引用计数那半在 `Ref<T>` 里，环靠
   `Heap::collect()` 的 STW 标记清扫。根从注册进来的 `GcRootSource` 出发（现在只有 `Runtime`；
   帧栈、模块表以后各自注册一个——都是解释器在 C++ 里直接持有、没有 `Object` 主人的东西。
   **有主的不算根**：`Code` 的常量表由 `Code` 持有、`Code` 由函数/类/模块对象持有，顺着链走得到）。**`collect()` 只能在主循环的安全点调用**
   ——根集合不含 C++ 栈上的局部 `Ref`，这条前提靠的是"C++ 调用栈深度不随 SL 帧栈增长"，见
   [notes/object-model-conventions.md](notes/object-model-conventions.md)。分代、只跟踪可能成环的
   对象这类优化都还没做，等主循环能量出实际分配速率再说。
   **回收的触发按字节算**：`Object::size_bytes()` 是每个类型自报的纯虚钩子（不能用 `sizeof(T)`
   ——任意精度 `int`/`decimal` 的真实负载在 `BigInt`/`BigDec` 内部另一次堆分配里），`make_ref` 在
   对象构造完之后记一笔（`Heap::link()` 在构造函数里，那时问不到派生类的大小），`collect()` 顺着
   全堆遍历重算存活字节数。**还差**：`-Xmx` 上限参数、"超预算 → 安全点回收 → 仍超则抛
   `MemoryError`"这条流程，要等第 6 步主循环才有地方挂安全点，见 [context.md](context.md)。
3. ~~**bootstrap**~~ **已完成**：`init()` 就两步——`build_types()` 建全部内置类型（含异常类树），
   `build_singletons()` 建六个单例——都只碰 `Runtime` 自己的私有字段 `types_`，不经过任何公开访问器。
   `Runtime::instance()` 只做一件事：`g_runtime` 是否为空。**没有分阶段的状态机**：早先版本给这个类
   配过一个有序的 `BootPhase`（`Uninitialized`→`Types`→`Values`→`Ready`），后来发现那是自己给
   自己挖的坑——之所以"需要"区分"类型建好但单例还没建好"，只是因为 `build_singletons()` 和
   `Bool`/`NamedSingleton` 的构造函数当时绕道调用了公开的、带检查的访问器（`type_none_type()`、
   `Runtime::type_bool()`），而不是直接读私有字段/接收调用方传来的 `Type*`。改成直接传值之后，
   这个中间状态从来没被任何代码观察到过（C++ 单线程同步执行，`init()` 跑到一半时没有别的代码能
   插进来看），分阶段就是纯粹的自我循环论证，见 [context.md](context.md) 对应小节。
   `Runtime::ready()` 现在就是 `g_runtime != nullptr`，供以后的编译器/虚拟机入口断言。内置函数表、
   内置模块表这些还没做的初始化步骤，插进 `init()` 的两步之后即可，不需要为它们预先开新的状态。
   `init()` 中途失败会把已建的部分拆干净再抛，不留半初始化的运行时。
4. ~~**异常体系**~~ **已完成**：`SL.md` 4.2.23 补了 `BaseException(*args)` 的构造与 `.args` 属性
   （参照 CPython——`args` 是结构体槽位不是 `__dict__`，没有 `__cause__`/`__context__`，因为 `raise`
   没有 `from` 子句）；`runtime/objects/Exception.h` 是它的 C++ 落地，`BaseException` 及其全部子类
   共用同一个类；`runtime/RaisedException.h` 是 `raise` 用的 C++ 信封；
   `runtime/HostErrorConversion.{h,cpp}` 把 `SyntaxError`/`EncodingError`/`FileNotFoundError` 转成
   SL 异常对象（`InternalError` 没有、也不会有对应函数）。**没做的**：`.args` 目前只能从 C++ 直接取
   （`BaseException::args()`），要等属性协议落地才能接上 `getattr`；`SyntaxError` 要不要在 `.args` 之外
   再暴露 file/row/col（类似 CPython），是待拍板的语言设计问题，见 [context.md](context.md)。分界与
   转换约定见 [notes/cpp-layer-vs-sl-layer.md](notes/cpp-layer-vs-sl-layer.md)。
5. **`Code` + `CodeGen`**（进行中：`Code` 对象、`ConstPool` 已完成，`CodeGen` 未开始）：按 [`bytecode.md`](../compiler/codegen/bytecode.md) 重启。常量表存真对象，去重按
   那份文档的「常量去重」。
6. **`Frame` + 主循环 + 指令实现**。
7. **`eval` / `import`**。

**目录调整**：`lexer`/`parser`/`analyzer`/`codegen` 已收进 `compiler/`，`builtins/exceptions/` 已挪成
顶层的 `cpp_exceptions/`（`builtins/` 这个名字留给真正的 SL 内置）。还没做的两件：

- **`compiler/` 的门面**——一个把"源码 → `Code`"包起来的对外入口，同时是把 C++ `SyntaxError` 转成 SL
  异常的地方。等 `codegen` 能跑了再写，现在 `executor/Executor.cpp` 手工串着三层。
- **`executor/` 里那段"串流水线的驱动"要跟虚拟机本身分开**——等主循环真写出来（第 6 步）再拆。
  运行时的落脚处已经定了：**新开 `runtime/`，不扩 `executor/`**。这不是口味问题——codegen 要造真
  对象，所以 `codegen → 对象模型`；而虚拟机主循环因为 `eval` 要 `executor → compiler`。对象模型放进
  `executor/` 就成了 `executor ← compiler ← executor` 的环，链接期就过不去。

## AST 节点：X-macro 分发 + 双路径职责

新增/修改语言语法时最常触碰的一套机制：

- **`x_ast_nodes.inc`** 是唯一的节点类型全集清单，每行 `X(AstNodeXxx)`。`ast_visitor.h` 里的
  `AstVisitor`/`AstConstVisitor` 靠它展开出每个节点一个纯虚 `visit`，遍历 AST 的类（`SemanticChecker`
  用 const 版、`ExprFolder` 用非 const 版）继承它们，分派走**双分派**（`node.accept(*this)` →
  节点自己的 `accept` 挑中对应的 `visit`），不是 `dynamic_cast` 瀑布。**加一个新节点类型，必须在
  这里加一行**——加完之后漏实现哪个 `visit` 是**编译期**报错（纯虚函数没覆写），不会拖到运行期。
- 新节点类型还需要：在 `compiler/parser/ast_nodes/details/` 某个合适的文件里定义结构体（继承 `AstNode`，
  类体里写一行 `SL_AST_NODE_ACCEPT` 宏），并把该头文件加进 `ast_nodes.h`；`ast_json_dumper.cpp` 里加
  对应的 `visit(...) override`；`SemanticChecker.h`/`.cpp` 和 `ExprFolder.h`/`.cpp` 里也各加
  对应的 `visit(...) override`（哪怕只是递归子节点、什么都不折）。
- **给已有节点加一个新的子节点槽位（`AstNodePtr` 字段），编译器一个字都不会提醒**——这跟上面
  "加新节点类型漏了 `visit` 会编译期报错"是两回事：X-macro 只保证每个**类型**都有 `visit`，管不到
  某个 `visit` 里面漏读了哪个**字段**。加完新槽位必须手动过一遍三个消费方：`ast_json_dumper.cpp` 的
  `visit`、`SemanticChecker` 的 `visit`、`ExprFolder` 的 `visit`（外加对应的测试）。踩过：
  `as` 那次给 `AstNodeForIter` 和 `AstNodeTry::AstNodeExceptAndExpr` 各加了个 `target_`，前者三处
  都跟上了，后者在 `ExprFolder` 里漏了，于是 `except (E as a[1 + 1])` 的下标一直没被折叠，四套
  测试全绿也照样没发现（测试同样漏写了这个槽位）。
- **节点类型拆分原则**："语义形状不同就不该共用节点类型"——比如比较运算符独立于普通二元运算符
  （`AstNodeCompare`，链式短路语义不同）、`is` 又独立于比较（`AstNodeIs`，不可重载、不跟比较混链）、
  `for` 的步进/迭代两种模式是 `AstNodeForCond`/`AstNodeForIter` 两个节点。不用 `variant`/tag 字段在
  一个节点里区分两种语义——X-macro 分发本来就是一个类型一个重载，`variant` 会在下面再手写一层全项目
  独一份的二级分发。
- **节点位置字段**：只有"产生式里夹着一个不属于任何子节点的裸 token"的节点类型才补一个 `Position` 字段
  （比如 `AstNodeCall::args_.pos_paren_`、`AstNodeAttr::pos_dot_`）——节点的结束位置几乎总能从最右
  子节点递归推出，不需要现在就存;基类 `AstNode::pos_` 只存起始位置。**命名统一是 `pos_` 前缀**
  （`pos_paren_`/`pos_dot_`/`pos_bracket_`/`pos_op_`，多个位置的复数形态是 `positions_` 前缀，比如
  `AstNodeCompare::positions_op_`、`AstNodeFunc::positions_decorator_`）——不是 `xxx_pos_` 后缀，
  这样所有"这是个位置字段"的成员在结构体里一眼能从前缀认出来，也跟基类的 `pos_` 保持同一词根开头。
- **调用类节点共用 `CallArgs`**：`AstNodeCall`/`AstNodeImportCall`/`AstNodeEval` 的实参部分
  （位置组/关键字组/`pos_paren_`）形状完全一致，拆成 `ast_node_misc.h` 里的 `CallArgs` 聚合体，三个
  节点各自持有一份 `args_` 成员，不重复三份字段——`OneKwArg`/`OneCapture` 已经是这个模式。注意这
  只是**数据形状**共用，不是给这三个节点类型加公共基类：`AstVisitor`/`AstConstVisitor` 由
  `x_ast_nodes.inc` 生成，每个具体节点类型各自一个 `visit()` 重载，加基类砍不掉这层重复，真正能砍的
  是 `SemanticChecker::check_call_args`/`ExprFolder::fold_call_args`/`AstJsonDumper::dump_call_args`
  这几个吃 `CallArgs` 的共享辅助函数。
- **折叠器（`StaticEvaler`）不依赖 `numeric/` 的高精度库**，int 折叠走 `int64_t`：溢出/装不下就
  不折，这是保守但正确，不追求任意精度。decimal 一律不折——算术、比较、真值都不折，因为它的值依赖
  运行期可变的 `prec`/`rounding`，编译期算出来的东西不保证跟运行期一致。类注释里那张「哪些类型、
  哪些运算符折」的表是这一块的**规范**，改动前先读它。int 的科学计数法写法（`1e5`）先在折叠器内部
  按值展开成普通数字串（指数超过一个较小的内部上限就跳过展开直接不折，纯粹是提前退出的效率阈值，
  不是正确性边界），再走同一条 `int64_t` 路径。判定函数（`truthy`/`literal_equal`）返回
  `std::optional`，`nullopt` 是「判不了」，调用方必须当「不折」处理——**不能给它一个默认值**，
  那会让死分支消除挑错分支。详见 .ai/context.md「折叠器数值折叠」一节。
- **AST 转 JSON 走独立的 visitor `AstJsonDumper`，不是节点自带的 `to_json()`**：`AstNode` 基类没有
  序列化相关的成员，`AstJsonDumper : public AstConstVisitor`（`ast_json_dumper.h`/`.cpp`）像
  `SemanticChecker`/`ExprFolder` 一样按 X-macro 展开一个节点一个 `visit() override`，用私有成员
  `result_` 当产出通道（`.ai/notes/visitor-result-passing.md` 的模式），入口是静态方法
  `AstJsonDumper::dump(node, include_pos)`。

## SemanticChecker 与 ExprFolder 的职责边界

- `SemanticChecker` **只读不改**，靠 `Context`（`can_star`/`can_double_star`/`loop_depth`/
  `finally_loop_depth`/`in_local_scope` 等，配 `ContextGuard` 做 RAII 存还原）跨节点传递语境限制，
  违规抛异常。
- `ExprFolder` **原地改**，`fold_*` 系列只处理"能不能折成编译期已知的字面量"，死分支/死循环消除、
  `Compound`/`Program` 剪枝也在这里。折叠不追求覆盖每个运算符——`is`、`dict` 的运算、`str` 的 `%`
  格式化等依赖运行时对象同一性/协议判等的场景故意不折，交给以后的执行器。
- 两者的执行顺序固定是 **check 在前、fold 在后**——反过来会有死分支消除把"本该在受限语境里"的表达式
  搬到不受限语境、悄悄让非法写法变合法的风险（具体反例见 [context.md](context.md)）。
- `StaticEvaler` 的折叠范围、编译期"折叠炸弹"防护（int 结果位数、str 长度、容器元素数三道上限）
  是刻意的工程约束，不是语言语义的一部分——新增折叠点时要留意这条边界。上限挡的是"算得完但没必要"
  （`2 ** 大数`、`'' * 大数` 这类），不是"算不对"：算不对属于保真性问题，见上面折叠器那一条。

## 测试组织

`test/` 下每个测试目标目录结构镜像对应源码目录（`test/lexer/`、`test/parser/`、
`test/analyzer/{semantic_checker,expr_folder}/`、`test/numeric/`）。`test/parser/`、`test/lexer/`、
`test/numeric/` 内部按主题分子目录，用两位数独立编号（lexer `01_comments`…`07_errors`，parser
`01_literals`…`13_cross`，numeric `01_bigint` / `02_bigdec`），**不跟 SL.md 章节号绑定**，见
[notes/no-section-numbers.md](notes/no-section-numbers.md)。analyzer 按子系统分子目录，文件按被测
规则命名，不再套一层编号——源码本身已经按子系统拆开，测试跟着走比再编一套号更不容易找错地方。
新增测试文件必须手动加进 `CMakeLists.txt` 对应的 `add_executable(...)` 文件列表（不是 glob，漏加
不报错、只是静默不参与编译）。

`test/numeric/big_dec_cases.inc` 和 `big_int_cases.inc` 都是**生成产物**，分别由
`gen_big_dec_cases.py`（期望值来自 CPython 自带的 decimal）和 `gen_big_int_cases.py`（期望值来自
Python 内置的 int）产出，对应的 `02_bigdec/python_cross_test.cpp` /
`01_bigint/python_cross_test.cpp` 逐条比对结果和触发的信号。改 `BigDec`/`BigInt` 的语义时要连带
重新生成（脚本开头写了用法），别手改那两个 `.inc`；生成器的种子是固定的，同一个 CPython 版本下
重新生成应当跟仓库里的逐字节一致，这一点可以当回归检查用。两个脚本都带一个倍数参数，临时跑几十
倍规模的差分测试很方便，提交进仓库的那份用默认倍数。BigDec 那张表里除了陷阱全关的路径，还有
`kTrapped*` 四张陷阱开启的表（抛不抛、抛哪个条件、抛出时 flags 到哪一步），值池刻意塞了带非零
指数的零；这些表的合并规则见脚本头注释。

**每组用例都拿 CPython 的两套实现（libmpdec 和 `_pydecimal`）各算一遍，不一致就整组跳过**——它们
自己在 `**` 和 `exp` 上就有已知分歧（见 [context.md](context.md)）。这条规则是防呆用的：分歧点随
参数漂移，往池子里加一档 `Emin`/舍入方式就可能生成出一张永远过不了的表。

提交进仓库的这份表是**按跑得动来配的**：`sl_test_numeric_bigdec` 里超越函数和 `**` 那两个用例
合起来就占了十几秒（BigDec 底下的 BigInt 是朴素算法，一次 `exp`/`ln` 要做几十次大数乘除），整个
ctest 现在约 28 秒。要更大覆盖别往表里堆，用倍数参数临时生成一份跑完再换回来——40 倍规模
（约 83 万个断言）跑过，全过。单条最贵的手写用例是 `log10_digits` 那个（约 1.6 秒，见
[context.md](context.md) 里"覆盖率驱动补的窄路径"一节），嫌慢时它是第一个可以砍的。

五个测试可执行目标：`sl_test_numeric_bigint`、`sl_test_numeric_bigdec`、
`sl_test_lexer`、`sl_test_parser`、`sl_test_analyzer`（最后这个同时覆盖
`semantic_checker/` 和 `expr_folder/` 两个子系统）。怎么构建/跑测试见
[notes/build-and-test.md](notes/build-and-test.md)。

`test/doc/` 是 `build_doc.py` 从 `SL.md` 生成的带锚点版本（`SL_linked.md`/`.html`），生成产物，
不是手写测试，且经常滞后于 `SL.md` 本身（除非用户明确要求，不需要主动重新生成）。

## 工程原则（写代码时套用，不要重新发明）

这些是在这个代码库里反复被验证过的取舍标准，加新功能/改现有代码时默认套用：

- **非法状态在数据形状层面就不可表达，优于"用状态机扫描去挡"**。例：`AstNodeFunc` 的形参列表拆成
  `AllParams` 聚合体里对应形参列表 4 段的 4 个字段（`positional_`/`var_args_name_`/`kw_only_`/
  `var_kwargs_name_`），不是一个打了 tag 的扁平 vector 靠布尔标志区分区域；`AstNodeCall`/
  `AstNodeImportCall`/`AstNodeEval` 共用的 `CallArgs` 聚合体拆成 `positional_args_`/
  `keyword_args_` 两个字段各自保持书写顺序；`CollectMark`（`for`/`while` 的收集模式记号）拆成
  `Container` 枚举 + `expand_` 布尔两个正交字段，而不是四个字符串常量。这类打了 tag 的扁平结构是
  最容易漏边界情况的地方——状态机代码里"这个组合出现了但没被处理"的分支永远比看起来的多。
- **节点类型/数据结构该拆分的信号是"语义形状不同"，不是"结构相似"**：`AstNodeCompare`/`AstNodeIs`、
  `AstNodeForCond`/`AstNodeForIter`、`AstNodeImportKw`/`AstNodeImportCall` 都是这条原则的应用，
  没有用 `variant`/tag 字段在一个节点里区分。
- **槽为空表示另一种形状，是整棵 AST 通用的既有惯例**（`AstNodeIf::else_expr_`、
  `AstNodeReturn::value_`、字典字面量里 `**expr` 展开项存成 `{expr, nullptr}`……）。新增字段要表达
  "这个东西可能不存在"时，默认复用这个惯例，不要为单个场景新引入 `std::variant`/`std::optional`
  包一层——除非这个字段是全 AST 唯一需要这种表达力的地方，那种情况下 `variant` 才划算。
- **各层入口的形态按"有没有可变状态"选，不要凭手感**：带跨节点遍历状态、必须保证"同一个对象只跑
  一次"的，做成**一次性对象 + 右值限定方法**，用 `&&` 让类型系统挡住第二次调用（`Parser` 的
  `pos_`/`brackets_`、`SemanticChecker` 的 `ctx_`）；完全无状态、所需信息全在参数里的，做成
  **静态函数**（`ExprFolder` 的所有 `visit` 本来就是静态的，`Analyzer` 只是把两步串起来）。
  别给无状态的东西套一个只存了个引用的构造函数——那是实例的外壳、静态的内里，同一个类里迟早出现
  一半入口是实例一半是静态的分裂。
- **函数的隐含前提统一写"调用方保证 X"**（不是"要求 X"这种含糊说法，消除"这是函数自己检查的还是靠
  调用方保证的"这层歧义），配的校验方式看这个前提要不要在 Release 构建里也生效：只在开发期兜底的
  用 `assert(...)`（在函数开头，`NDEBUG` 下会被优化掉，所以不能拿它实现真正的校验）；`SemanticChecker`/
  `string_utils`/`cpp_exceptions` 这类需要在 Release 也生效的真实校验，走抛异常
  （`SyntaxError`/`InternalError`/`EncodingError`），不能用 `assert` 顶替。
- **避免"同一产物在多个出口分头构造"**：同一种产物如果在多处提前返回、各自构造，等于制造了多份
  测试压不到的独立状态空间，历史上好几个换行容错类的 bug 都是这个模式孵出来的。能合并"同一产物多处
  构造"就合并，只保留"产物本质不同"的分叉（比如字典字面量 vs 复合表达式、分组 vs 元组，这些才是
  真正的二选一）。
- **折叠（`StaticEvaler`/`ExprFolder`）的正确性标准是"被丢弃的部分本来就不会被求值"，不是"被丢弃
  的部分本身可以折成字面量"**。例：`True and f()` 能折成 `f()`，不需要 `f()` 本身可折；链式比较
  `1 < 2 < 'a'` 已经确定为 `True` 的前缀可以丢，即使后面的 `2 < 'a'` 类型不可比没法继续折。新增
  折叠规则时按这条判断"能不能丢"，不要跟"能不能折成字面量"混为一谈。
- **折叠涉及"复制值 vs 共享引用"时要考虑 `is`**：SL 的 `is` 不可重载、纯粹判断对象同一性，跟内容
  是否可变无关，"深度不可变就能安全地用 clone 代替共享"这个直觉是错的——哪怕内容完全不可变，
  克隆出来的两份和真正共享的同一份在 `is` 面前也能被区分出来。这是 `tuple`/`list` 的 `*` 重复恒
  不折的原因（AST 是纯 `unique_ptr` 独占树，没有"共享子树"的表示能力，折叠成克隆等于悄悄改变了
  `is` 语义，只能不折）。

## ExprFolder / SemanticChecker 的非显然算法事实

代码本身能看出这些行为，但不看注释容易假设错，写新折叠规则前先确认没有踩这几条：

- **`visit_and_replace` 是 fixpoint 循环**：对同一节点反复调 `StaticEvaler::fold` 直到返回
  `nullptr` 为止，不是只折一次。新写的 `fold_xxx` 不需要自己操心"我拼出来的新节点还能不能再折"——
  只要新节点的子节点都已经被 `visit` 过，上层循环会兜底继续折。
- **`prune_program`（`AstNodeProgram` 语句列表精简）不走 `fold()` 替换节点这条路，是原地精简**：
  `AstNodeProgram` 的槽位在 `ExprFolder::root_`、`AstNodeFunc::body_`、`AstNodeClass::body_` 里
  都是具体类型 `AstNodeProgram(Ptr)` 而不是通用 `AstNodePtr`，节点自身的地址/类型不能变，跟
  `AstNodeCompound`（永远挂在通用 `AstNodePtr` 槽位上，`fold_compound` 能把整个节点换成别的类型）
  是两种不同的机制，不是随意不统一。剪枝规则：`exprs_` 里每个位置统一判断，纯字面量
  （`is_literal_pure`）就丢、非字面量就留、相对顺序不变——**不特殊保留最后一条**，因为 `Program`
  的值由 `return`（或 `None`）决定，跟"最后一条表达式的值"无关（这点跟 `AstNodeCompound` 不同，
  `Compound` 的值确实是最后一条，`fold_compound` 保留最后一条是对的、两者不能类比）。
- **`for`/`while` 死分支消除**：`init_` 不管 `cond` 折出来是什么都会先无条件求值一次，折叠时不能
  丢掉这个副作用——`init_` 非空时结果包成 `Compound{init_, 退化值}`（不带 `$` 退化成 `0`，`$`/`$ *`
  退化成 `[]`，`$$`/`$$ **` 退化成空 dict）。空 dict 在**源码**里写不出来（`{}` 归复合表达式），但
  折叠器产出的是 AST 节点而不是源码，`AstNodeLiteralDict` 的 `items_` 本来就可空——"折叠结果必须是
  parser 也能产出的形状"不是本项目的约束，别把它当理由。`cond` 折成确定 `True` 时刻意不折——只能
  确定"不会提前退出"，不像 `if` 折 `True` 那样能确定具体是哪个分支、有"换成什么"的答案。
- **`check`（`SemanticChecker`）必须在 `fold`（`ExprFolder`）之前跑，不能反过来**：反例
  `f(True and *args)`，check 先跑时 `*args` 作为 `and` 的右操作数永远在 `can_star=false` 语境下
  被检查、正确报错；但如果 fold 先跑，死分支消除会把 `*args` 从 `and` 表达式里挪出来直接顶到调用
  参数位置（`can_star=true` 的合法语境），一段本该永远语法非法的写法就这样被折叠悄悄变合法了。
- **`SemanticChecker::Context::finally_loop_depth`**：记录进入 `finally` 时的 `loop_depth`（`-1`
  表示不在 `finally` 内）。`return` 只要它 `>= 0` 就拦；`break`/`continue` 要求
  `loop_depth == finally_loop_depth`（相等说明这层循环在 `finally` 外面，不相等说明是 `finally`
  内部自己新开的循环，不该拦）；进入 `func`/`class` 的新 `Program` 时必须重置成 `-1`（否则
  `finally` 内定义的函数体自己的 `return` 会被误杀）。

## X-macro 清单文件

除了 `x_ast_nodes.inc`，还有 `../runtime/x_builtin_types.inc`（全部内置类型：枚举名、访问器名、
SL 层类名、基类；顺序即建立顺序，同时驱动 `BuiltinType` 枚举、`Runtime::type_xxx()` 访问器和
bootstrap 的建立循环，加一个内置类型只改这一个文件）、`../compiler/lexer/x_token_type.inc`（全部 `TokenType` 枚举值）、
`../compiler/lexer/x_keyword.inc`（关键字文本 → `TokenType` 映射）、
`../compiler/lexer/x_reservedword.inc`（保留字但非关键字，如 `_G`/`_L`）。加新
关键字/token 类型时这几个文件要一起改，具体加在哪由这个 token 的性质决定（是不是关键字、是不是保留字）。
