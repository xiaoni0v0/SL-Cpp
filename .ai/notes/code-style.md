# C++ 码风约定（超出 `.clang-format` 管得到的那部分）

`.clang-format` 管缩进/花括号/换行/include 分组排序（`format.py` 一键跑）。下面是它管不到、靠自觉的规则，
以及**每条规则自己有的陷阱**——只照字面套用会改出 bug。

## 头文件

- **分四组，组内连续排、组间空一行**：这个 `.cpp` 自己对应的头 / 项目里的其他头 / C 和 C++ 标准头 / 第三方库的头
  （`nlohmann`、`SafeInt`、`doctest`、`cxxopts`）。由 `.clang-format` 的 `IncludeBlocks: Regroup` +
  `IncludeCategories` 强制，**别手排**。
- **include-what-you-use**：每个文件直接包含它自己用到的头，不多不少（含标准头：用了 `std::make_unique` 就要
  `<memory>`，不能靠别的头传递带进来）。头文件必须自包含（能单独编译）。
- 自动补/删 include 用 `clang-tidy --fix --checks=-*,misc-include-cleaner`，**跑完必须人工过一遍**：它会插入绝对路径、
  把汇总头拆成十几个 `details/*.h`、把 `<windows.h>` 换成内部头、把 `<nlohmann/json.hpp>` 换成 `json_fwd.hpp`
  （成员是完整类型时不行）。

## 类型与转换

- **C 标准库的类型带 `std::`**：`std::size_t`、`std::int64_t`、`std::uint32_t`……（`INT64_MAX` 这类宏不带）。
- **永远不用 C 风格转换** `(T)x`，也不用函数式转换 `int(x)` 做算术转换，一律 `static_cast`/`bit_cast`。
  构造对象用 `T{...}`，不算转换。
- **`int` 还是 `std::size_t`/定宽类型**：容器/字符串的大小、下标、个数 → `std::size_t`；宽度本身是语义
  （字节码字段、limb 运算、哈希、跟 `INT64_MAX` 比较）→ 定宽类型；小循环计数、`argc`、优先级数值、行列号 → 保留 `int`。
  **陷阱**：把 `int` 换成 `std::size_t` 会引入无符号回绕（`for (size_t i = n - 1; i >= 0; --i)` 死循环、
  `a - b` 变成巨大数）；`int` 和 `std::int32_t` 在所有支持的平台上同一类型，互换没有意义。
- `main` 的签名必须是标准的 `main(int, char *[])`：其他形式是**实现定义**的（比如 `const char *argv[]`）。

## 初始化与模板实参

- **变量初始化优先 `T x{...}`**。json 相关的例外（`nlohmann::json{...}` 会把花括号吃成数组），用 `=`。
  **陷阱**：容器/字符串的"大小 + 值"构造必须用圆括号——`std::vector<int> v(n, 0)`、`std::string s(n, 'c')`，
  花括号会选 `initializer_list` 构造，语义完全不同。花括号禁止窄化转换，出编译错误说明原来有隐式窄化，
  先看它是不是 bug，别用强转糊弄。标量一律带初始化（`T x{};`），别留未初始化（`std::int64_t sum;` 作输出参数也一样）。
- **模板实参能省则省**（CTAD）。**陷阱**：`Ref<Base> p{derived_ptr}` 省略后推成 `Ref<Derived>`，语义变了——声明类型
  故意不同于实参推导结果时不能省；`std::vector v{other_vec}` 会推成拷贝；`std::make_unique<T>`/`make_ref<T>` 必须写。

## const、move、static

- **确定不变就 `const`**；指针/引用按含义给**指向物**加 const，**指针本身不加顶层 const**（`const T *p`，不写
  `T *const p`）。`const char *const *argv` 里第二个 const 是指向物的，要保留。
- **const 会阻止自动 move**：被 `return x;` 按值返回的局部不要加 const；会被 `std::move(x)` 的局部/形参不要加 const
  （`std::move` 一个 const 对象是拷贝）；**`const` 数据成员让类失去移动/拷贝赋值，移动构造退化成拷贝**——
  只在"实例从不被赋值/放进需要移动交换排序的容器"时才给成员加（`Lexer`/`Parser`/`SemanticChecker` 这类只在
  静态入口里构造一次的类可以；AST 节点是 `unique_ptr` 独占、不拷贝不移动，所以也可以）。按值形参且函数体不 move 它：
  写 `const T x`。
- 不用 `this` 的成员函数 → `static`；不修改状态的 → `const`；类内常量 `static constexpr`，头里的全局常量
  `inline constexpr`，命名 `kPascalCase`。
- **只在当前 TU 可见的函数/变量放匿名命名空间，不用 `static`**（类内 `static` 成员不在此列）。
  一个 private 辅助函数如果既不用 `this` 也不碰类的私有成员/类型，就放进 `.cpp` 的匿名命名空间，别留成成员。
- 没有可见性的 `const` 成员函数经指针/引用改了别的对象的状态（比如 `CodeGen::gen_*` 经 `builder() const` 返回的
  非 const `CodeBuilder &` 发指令）——这种 `const` 是撒谎，见 `builder()`：目前没改，是否统一去掉是待拍板项。

## 类：可实例化与否、explicit、访问控制

- **不可实例化的类（所有成员 static，且没有任何地方创建它的实例）**：`final` + 删 6 个特殊成员函数
  （默认构造、拷贝构造、移动构造、拷贝赋值、移动赋值、析构）。现有 `Analyzer`、`StaticEvaler`、`Compiler`、`Heap`。
  **不适用**：私有构造 + 在自己的静态入口里创建实例的"静态门面"类（`Parser`、`Lexer`、`SemanticChecker`、
  `ExprFolder`、`CodeGen`）和单例（`Runtime`，但它删拷贝/移动）。
- **多态基类要防切片**：有虚函数的基类拷贝/移动 `= delete`（`AstNode`、`AstVisitor`、`Object`、`RefVisitor`），
  除非确有拷贝需求。**RAII 守卫**（`RoundingGuard`、`RuntimeGuard`、`ContextGuard`）拷贝/移动也 delete。
- **`explicit`**：单参数构造函数（含"除第一个外都有默认值"）和转换运算符默认 `explicit`；确实要隐式转换的写
  `explicit(false)` 并留一行中文注释说明理由（`Ref<T>` 的 `nullptr`/派生类转换）。
  **多参数构造函数不要一律加 explicit**——它会挡住 `return {a, b};`，项目里 `Token`、`AstNodeCondAndExpr` 等在用。
- **struct/class 二选一的判据是"有没有封装"，双向都要满足**：`struct` 不带任何 `private`/`protected`
  （公开字段的纯聚合体：AST 节点、`Token`、`Position`）；`class` 至少带一个 `private`/`protected` 成员
  ——数据或函数都算，且默认访问区（`class` 开头到第一个 `public:` 之前）里有成员也算，不一定要写出
  `private:` 这个标签。**反过来，`class` 如果检查下来全是 public（常见于"纯静态门面"类
  `Compiler`/`Analyzer`，和没有数据的 RAII 触发器 `RuntimeGuard`），要改回 `struct`。** 前向声明
  （`class X;`/`struct X;`）的关键字必须跟定义一致，MSVC ABI 下不一致是真实的链接风险，不只是警告。
  **"藏了东西"顺着继承链传递，判断要看整条链，不能只看这个类自己的类体**：基类是 `class`（链上任意
  一层有封装）→ 派生类必须是 `class`，哪怕派生类自己一个 private 都没加（`Ref<T>` 派生自
  `class RefBase`，`RefBase` 把指针设成 private，`Ref` 自己的类体全是 public，也必须是 `class`）；
  反过来，基类是 `struct`（链上没有任何封装）时派生类才可能是 `struct`——但如果派生类自己也没加
  private，那就真是 `struct`（`Heap::RefDropper` 派生自 `struct RefVisitor`，自己也全是 public，
  是 `struct`，不是 `class`）。类外定义的嵌套类（`class Outer::Inner : Base { ... }`）同样要查，
  容易漏。
  能 `friend` 精确开口的就别放宽访问（`object-model-conventions.md` 第六节）。
- **改小可见性/重命名前，先 `grep` 全仓库含 `test/`**——公开成员几乎都有测试在用。

## 现代 C++，别过度

`[[nodiscard]]` 给"忽略返回值必是 bug"的函数；能 `constexpr` 的；`enum class : 底层类型`；`= default`/`override`/`final`；
`std::optional` 代替哨兵值。**别把清晰的手写循环强行改成算法，别为了"现代"改设计**；项目有意不开命名空间；
`std::endl` 换成 `'\n'`，要 flush 就显式写 `std::flush`。

## 怎么自查

- **clang-tidy**（LLVM 22，随 VS Build Tools）：用过的检查集覆盖 `bugprone-*`、`clang-analyzer-*`、
  `cppcoreguidelines-{init-variables,special-member-functions,pro-type-*cast,...}`、`google-explicit-constructor`、
  `misc-{const-correctness,include-cleaner,use-anonymous-namespace}`、`modernize-*`、`performance-*`、
  `readability-{convert-member-functions-to-static,make-member-function-const,...}`。`misc-const-correctness` 要设
  `WarnPointersAsValues: false`（不给指针加顶层 const）。**这些检查项里有一批对本项目不适用**，直接忽略：
  `modernize-use-designated-initializers`（跟位置花括号初始化的风格冲突）、`performance-enum-size`（有填充的枚举缩小没收益）、
  `modernize-macro-to-enum`（预处理器要用宏）、`bugprone-macro-parentheses`（X-macro 的参数是类型名）、
  `bugprone-branch-clone`（X-macro 生成）。
- **真运行的 UB 检测**：ASan+UBSan 版本跑全部测试，做法见 [build-and-test.md](build-and-test.md)。
- 没有 PCH 的全量构建能抓出"被 PCH 掩盖的缺头文件"；每个头文件单独 `clang++ -fsyntax-only` 能验证自包含。
