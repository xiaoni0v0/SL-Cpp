#pragma once

#include "Heap.h"
#include "Type.h"
#include "objects/singletons.h"

#include <array>

// 内置类型的编号。清单在 x_builtin_types.inc
enum class BuiltinType : std::size_t {
#define X(field, accessor, name, base) field,
#include "x_builtin_types.inc"
#undef X
    Count,
    NoBase, // 只给 object 用：它没有基类
};

// 运行时本身：持有全部内置类型与单例，并按正确的顺序把它们建起来。
//
// **没有分阶段的状态机**：`init()` 内部先建类型、再建单例，看着像"两个阶段"，但这个中间状态
// 从来没有暴露给外部——C++ 单线程同步执行，`init()` 跑到一半时不存在任何别的代码能插进来看到
// "类型建好了、单例还没建好"这个瞬间。真正需要分辨的只有一件事："`init()` 到底有没有跑完"，
// 这就是一个 bool（`g_runtime` 是否为空）。
//
// 早先版本给这个类配了个 `BootPhase` 有序枚举，本意是防"提前访问了还没建好的东西"，
// 但那其实是自己给自己挖的坑：`build_singletons()` 内部曾经调用 `none_type()` 这类**公开的**
// 访问器去拿自己需要的类型，而不是直接碰 `types_` 这个私有字段——`Bool`/`Singleton` 的构造
// 函数也一样，曾经硬编码调用 `Runtime::bool_type()` 而不是接收调用方传来的 `Type*`。绕过公开
// 接口、直接传值/直接访问私有字段之后，"类型建好但单例还没建好"这个阶段根本不会被任何代码
// 观察到，`BootPhase` 也就没有存在的必要了
//
// 进程内只有一个（SL 没有"多解释器"的概念，import / eval_isolated 建的是新的**全局作用域**
// 而不是新的运行时），所以做成全局单例、访问器是静态的——把一个 Runtime& 穿过每个对象构造函数
// 和每条指令，是在为一个 SL 根本不提供的能力付管道费。
//
// 它同时是第一个 GC 根源——内置类型与单例都由它攥着，以后帧栈、模块表、每份 Code 的常量表
// 各自再注册一个。
class Runtime final : public GcRootSource {
    std::array<Ref<Type>, static_cast<std::size_t>(BuiltinType::Count)> types_;
    Ref<Singleton> none_;
    Ref<Singleton> ellipsis_;
    Ref<Singleton> not_implemented_;
    Ref<Singleton> stop_iteration_;
    Ref<Bool> true_;
    Ref<Bool> false_;

    Runtime() = default;

    // —— bootstrap 的两步，按声明顺序执行；都只碰 types_/私有字段，不经过任何公开访问器 ——
    void build_types();
    void build_singletons();

    // 放掉全部内置类型与单例的引用。放完它们就只剩内部互相引用，是标准的垃圾环
    void release_all();
    // 把运行时拆干净：放引用、摘根源、扫一轮。init() 中途失败和正常 shutdown 共用它
    static void dispose();

    // 取运行时；没 init() 或已经 shutdown() 就访问，抛 InternalError
    [[nodiscard]] static Runtime &instance();
    // 各类型访问器共用的实现，省得宏展开出一堆同样的函数体
    [[nodiscard]] static Type *builtin_type(BuiltinType id);

  public:
    // 覆写基类 GcRootSource 的公开接口，可见性跟基类保持一致（收紧会被 CLion/编译器警告，
    // 而且这本来就是给 Heap 通过 GcRootSource* 调用的公开契约，不是该收紧的内部实现细节）
    void visit_roots(RefVisitor &visitor) override;

    // 重复 init / 未 init 就 shutdown 都是 InternalError：这种顺序错误只可能是实现自己的 bug。
    // init() 中途抛异常时会把已经建起来的部分拆干净再把异常放出去，不留半初始化的运行时
    static void init();
    static void shutdown();

    // 编译器/虚拟机的入口在动手之前该断言这个
    [[nodiscard]] static bool ready();

    // 下面这些返回的都是**借用**的裸指针：运行时活着期间它们恒有效，要长期持有请自己包 Ref。
    // shutdown 之后一律失效
#define X(field, accessor, name, base) [[nodiscard]] static Type *accessor();
#include "x_builtin_types.inc"
#undef X

    [[nodiscard]] static Object *none();
    [[nodiscard]] static Object *ellipsis();
    [[nodiscard]] static Object *not_implemented();
    [[nodiscard]] static Object *stop_iteration();
    // True / False。SL 的 bool 恒只有这两个实例，不许再造第三个
    [[nodiscard]] static Bool *boolean(bool value);
};
