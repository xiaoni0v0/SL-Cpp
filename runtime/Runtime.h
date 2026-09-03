#pragma once

#include "Heap.h"
#include "Type.h"
#include "objects/singletons.h"

#include <array>
#include <cstddef>

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
// 进程内只有一个（SL 没有"多解释器"的概念，import / eval_isolated 建的是新的**全局作用域**
// 而不是新的运行时），所以做成全局单例、访问器是静态的——把一个 Runtime& 穿过每个对象构造函数
// 和每条指令，是在为一个 SL 根本不提供的能力付管道费。
//
// 后续"分阶段 bootstrap"在这个类上展开。它同时是第一个 GC 根源——内置类型与单例都由它攥着，
// 以后帧栈、模块表、每份 Code 的常量表各自再注册一个。
class Runtime final : public GcRootSource {
    std::array<Ref<Type>, static_cast<std::size_t>(BuiltinType::Count)> types_;
    Ref<Singleton> none_;
    Ref<Singleton> ellipsis_;
    Ref<Singleton> not_implemented_;
    Ref<Singleton> stop_iteration_;
    Ref<Bool> true_;
    Ref<Bool> false_;

    Runtime() = default;
    void bootstrap();
    // 放掉全部内置类型与单例的引用。放完它们就只剩内部互相引用，是标准的垃圾环
    void release_all();

    void visit_roots(RefVisitor &visitor) override;

    [[nodiscard]] static Runtime &instance();

  public:
    // 重复 init / 未 init 就 shutdown 都是 InternalError：这种顺序错误只可能是实现自己的 bug
    static void init();
    static void shutdown();
    [[nodiscard]] static bool initialized();

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
