# 对象模型：所有权与继承的两条约定

写 `runtime/` 下任何新对象类型之前先读这两条，写完之后照着自查。

## 一、C++ 的类继承 ≠ SL 的类继承

`runtime/` 里的 C++ 类**只表达存储形状**（这个对象有哪些字段、怎么析构）；SL 那边的继承关系
完全存在 `Type` 的 `bases_`/`mro_` 里，两套互不牵连。

**How to apply**：新增一个 SL 类型时，先分别回答两个问题，不要让一个答案顺带决定另一个。

- "它在 SL 里的父类是谁" → 只影响 `x_builtin_types.inc` 里那一行的最后一列。
- "它需要什么字段" → 决定要不要新开一个 C++ 类。

三种情况都真实存在，缺一不可：

- **有 SL 类、没有 C++ 类**：`numbers.Number`/`numbers.Real` 是抽象类，没有实例，只是两个
  `Type` 对象；
- **一个 C++ 类、多个 SL 类**：`Singleton` 同时被 `None`（类型是 `NoneType`）和
  `Ellipsis`/`NotImplemented`/`StopIteration`（类型是 `SingletonType`）使用——它们的差别只有
  "类型是谁、打印成什么"，没有任何字段差异，不值得各开一个空类；
- **SL 里不是子类、C++ 里也不该是子类**：`bool` 的父类是 `numbers.Real` 而**不是** `int`
  （SL.md 4.2.5/4.4，跟 Python 不同），所以 `Bool` 也不是 `Int` 的 C++ 子类。反过来说，就算
  哪天某两个类型在 SL 里是父子，也不意味着 C++ 这边就该继承——那是独立判断。

**Why**：这两套一旦被"顺手对齐"，以后每次改 SL 的继承图都会连带逼着改 C++ 的类层次（或者反过来），
而它们的变化原因根本不同。

## 二、所有权：`Ref<T>` 是唯一的所有权表达，裸指针恒是借用

`Ref<T>`（`runtime/Object.h`）是侵入式引用计数句柄，**构造恒 +1、析构恒 -1**，没有
adopt/borrow 两套入口。裸 `Object *`/`Type *` 一律不带所有权。

**How to apply**：

- 新对象一律 `make_ref<T>(...)` 建，不要裸 `new`——裸 `new` 出来的对象引用计数是 0、没人管，必漏；
- 存进字段/容器的用 `Ref<T>`；只在函数内看一眼、不跨越可能触发回收的操作的，用裸指针；
- `Runtime::xxx_type()` / `Runtime::none()` 这类访问器返回的是**借用**（那些对象由 `Runtime`
  永久持有），要长期存下来自己包一层 `Ref`；
- 新类型必须实现 `visit_own_refs()`，如实报出自己强引用的每个槽位。它是纯虚的，漏写是编译期
  错误；但**报漏一个字段不是**——GC 会把那条边指向的对象当成不可达提前回收，这是这一层最难查的
  bug 类型。加字段时同步改 `visit_own_refs()`，跟 AST 那边"加槽位要过三个消费方"是同一个自查动作。

**两个刻意的例外，别照抄成"引用就得用 Ref"**：

- **`Object::type_` 由基类统一报告**：它是 `Ref<Type>`，但归 `Object::visit_all_refs()` 管，
  子类的 `visit_own_refs()` 不用（也不该）报它；
- **`Type::mro_` 存裸指针，是真·借用**：`mro_[0] == this`，如果用 `Ref` 就是每个类都自成一个环、
  引用计数永远归不了零。安全性由 `bases_` 兜着——MRO 里的每个类都能沿 `bases_` 链到达，本来就被
  强引用着，GC 顺着 `bases_` 也扫得到，所以 `Type::visit_own_refs()` 只报 `bases_`。

## 三、`Heap::collect()` 只能在安全点调用

**硬前提：调用 `collect()` 时，当前 C++ 调用栈上不能有任何活的 `Ref`。**

根集合只包含注册进 `Heap` 的那些 `GcRootSource`（现在是 `Runtime`，以后还有帧栈、模块表、
每份 `Code` 的常量表）。C++ 栈上的局部 `Ref` 谁也扫不到，扫描时它们指向的对象会被判成不可达、
当场清掉，句柄立刻悬垂。

**How to apply**：

- 回收只发生在**主循环两条指令之间的安全点**——主循环问 `Heap::should_collect()`，为真就
  `Heap::collect()`。
- **分配动作不许顺手触发回收**，`make_ref` 里没有、也不要加这种钩子；
- codegen、内置函数实现、任何"一路 C++ 调下去"的代码半途都不许调 `collect()`，那些地方栈上全是
  局部 `Ref`。

这条前提能成立不是运气，是 `bytecode.md` 那条"C++ 调用栈深度不得随 SL 帧栈深度增长"的约束换来的：
执行状态全在堆上的帧对象里，安全点上 C++ 栈本来就是空的。**改动这条约束等于改动 GC 的正确性前提。**

## 四、加初始化步骤：按依赖选相位，不是往 bootstrap 末尾追

`BootPhase` 是**有序**的：`Uninitialized` → `Types`（全部内置类型对象）→ `Values`（单例；到这里
常量表要的一切都能造了）→ `Ready`（编译器与虚拟机可以跑）。

**How to apply**：

- 新的初始化步骤先问"它依赖已经建好的什么"，据此决定插在哪两个相位之间，并在 `Runtime::init()` 里
  相应地推进 `phase_`。异常类树、内置函数表、内置模块表都该落在 `Values` 和 `Ready` 之间。
- **新访问器必须在 `instance(...)` 里如实声明自己要求的最低相位。** 这是"bootstrap 顺序写错了"
  唯一的自动拦截点——不声明的话，早一步拿到的是个空类型指针，错误会飘到很远才炸。
- 需要"运行时已经完全就位"的入口（编译器门面、虚拟机主循环）断言 `Runtime::ready()`。

## 六、"只给 X 用"就用访问控制表达，别靠注释

`runtime/` 里凡是注释写着"只给 GC 用""只给 bootstrap 用"的成员，一律是 `private` + 精确的
`friend`，没有"public 但请自觉别碰"这种东西。当前的几处：

| 成员 | 谁能碰 | 怎么做到的 |
|---|---|---|
| `Object::gc_prev_/gc_next_/gc_marked_` | `Heap` | `friend class Heap` |
| `Object::visit_all_refs` | `Heap` | 同上 |
| `Object::visit_own_refs` | 只有 `Object::visit_all_refs` | 私有虚函数（NVI），子类照常覆写 |
| `Object::set_type` | `Runtime` | `friend class Runtime` |
| `Object::incref/decref` | `Ref<T>`、`Heap` | `template <typename U> friend class Ref` + `friend class Heap` |
| 各具体对象类型的构造函数 | `make_ref` | `SL_HEAP_ONLY` 宏（放在 private 区） |
| `Heap::link/unlink` | `Object` | `friend class Object` |

**踩过的坑**：`Heap` 的两个访问者（`Marker`/`Clearer`）一开始写在 `Heap.cpp` 的匿名 namespace 里，
它们不是 `Heap` 的成员，`friend class Heap` 罩不到，于是当时把 `Object` 的标记位开了一对公开访问器
将就过去。**正确做法是把它们做成 `Heap` 的嵌套类**——嵌套类跟其他成员一样享有外围类的友元权限
（`[class.access.nest]`），头文件里只留两行前置声明 `class Marker; class Clearer;`，定义照旧在 `.cpp`。
以后再遇到"某个辅助类需要访问被友元保护的东西"，先想这一招，别开公开后门。

**`incref`/`decref` 是成员而不是自由函数**：自由函数（Boost `intrusive_ptr` 那套）的意义在于让
任意类型都能接入侵入式计数，而 `Ref` 只服务 `Object` 一族，用不上那份通用性；做成私有成员之后，
"手动改引用计数"在 `runtime/` 之外直接写不出来。`decref()` 归零时 `delete this`——注意它之后不得
再碰任何成员。

**每个具体对象类型的 private 区都要写 `SL_HEAP_ONLY;`，构造函数也放在 private 区**（宏定义在
`Object.h`）。这样这类对象只能由 `make_ref` 在堆上建。拦的是两件事：

- **栈上/静态存储期的对象**：`Int x{BigInt{1}};` 语法上完全合法、写起来还很自然，可它一旦被 `Ref`
  接管，引用计数归零时 `decref()` 会对它 `delete this`，直接踩烂栈；
- **裸 `new` 之后忘了包 `Ref`**：那种对象引用计数恒为 0、没人管，必漏。

新增对象类型时忘了写这一行不会有任何报错，只是少了这层保护——**加新对象类型时照着现有的抄全**。

**唯一公开的例外是 `Object::refcount()`**，而且它是**只读**的：测试要靠它断言引用计数收支平衡，
这是现阶段抓引用计数 bug 的主要手段。判据是"只读的诊断信息可以公开，可变的内部记账不行"。

## 内置类型之间唯一的环

内置类型之间**唯一**的引用环是"每个类型都强引用元类 `type`，而 `type` 的元类是它自己"——
`bases_` 是自下而上的 DAG，`mro_` 是借用，都不成环。`Runtime::shutdown()` 正是靠这个：放掉全部
引用、摘掉根源、再 `Heap::collect()` 扫一轮，整个堆一次清空，之后 `Heap::live_count()` 应当是 0。

**这也意味着 `shutdown()` 之前必须放掉所有 `Ref`**：还攥在手里的对象同样会被扫掉，句柄随即悬垂。

## 五、异常：一个 C++ 类覆盖整棵树，`.args` 是唯一字段

`Exception`（`runtime/objects/Exception.h`）覆盖 `BaseException` 及其全部子类，只存 `.args`
元组——跟第一条"一个 C++ 类可以覆盖多个 SL 类"是同一个模式（`Singleton` 覆盖 `None`/`Ellipsis` 等
是先例）。跟 CPython 对齐：`BaseException.args` 本来就是结构体槽位，不是 `__dict__`。

**How to apply**：

- 新增异常子类只改 `x_builtin_types.inc` 加一行，**不新增 C++ 类**——除非它需要 `.args` 之外的
  专属字段，而这现在还没出现过（`SyntaxError` 要不要额外挂 file/row/col 是待拍板的语言设计问题，
  见 `.ai/context.md`，没拍板之前不要在 `Exception` 里预先开这个口子）。
- 构造 `Exception` 必须给一个 `BaseException` 的子类当 `type`，构造函数里有 `assert` 兜底，
  别指望它在 Release 下也拦——调用方保证。
- `RaisedException`（`runtime/RaisedException.h`）是 `raise` 用的 C++ 信封，**只能在一段不回调 SL、
  有界的 C++ 代码里 throw/catch**，不是异常跨 SL 帧传播的机制——那条路是主循环手写的显式算法
  （bytecode.md）。写内置操作的 C++ 实现，需要"产出一个异常"时才用它；写任何涉及 SL 帧的代码
  都不该用它。
- `HostErrorConversion.{h,cpp}` 是宿主异常 → SL 异常对象转换的落地实现，`InternalError` 没有对应
  函数、以后也不会有，见 [cpp-layer-vs-sl-layer.md](cpp-layer-vs-sl-layer.md)。
