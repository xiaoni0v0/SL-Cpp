#pragma once

#include "Heap.h"
#include "Type.h"
#include "objects/Bool.h"
#include "objects/NamedSingleton.h"

#include <array>

// 内置类型的编号
enum class BuiltinType : std::size_t {
#define X(id, accessor, name, base) id,
#include "x_builtin_types.inc"

#undef X
    Count,

    // 不是类型，只给 object 用
    NoBase,
};

// 内部对象的编号。同时就是 LOAD_INTERNAL 的参数
enum class InternalObject : std::size_t {
#define X(id, accessor, name) id,
#include "x_internal_objects.inc"

#undef X
    Count,
};

/**
 * 运行时本身。
 *
 * 持有全部内置类型、单例与内部对象，按顺序建好。进程内唯一（全局单例）。
 */
class Runtime final {
    friend class Heap;

    std::array<Ref<Type>, static_cast<std::size_t>(BuiltinType::Count)> types_;

    std::array<ObjectRef, static_cast<std::size_t>(InternalObject::Count)> internals_;

    // 六个单例
    struct {
        Ref<NamedSingleton> none_;
        Ref<NamedSingleton> ellipsis_;
        Ref<NamedSingleton> not_implemented_;
        Ref<NamedSingleton> stop_iteration_;
        Ref<Bool> true_;
        Ref<Bool> false_;
    } singletons_;

    Runtime() = default;

    // bootstrap 的三步，按声明顺序执行
    void build_types();
    void build_singletons();
    void build_internals();

    // 自己拆掉 Runtime，init / shutdown 调用
    static void tear_down();

    // 取单例
    [[nodiscard]] static Runtime &instance();
    // 下边各类型访问器共用的实现
    [[nodiscard]] static Type *builtin_type(BuiltinType id);
    // 下边各内部对象访问器共用的实现
    [[nodiscard]] static Object *internal_object(InternalObject id);

    // 遍历全部引用根。只有 Heap::collect() 会调
    static void visit_roots(RefVisitor &visitor);

  public:
    // init() 中途抛异常时会回滚
    static void init();
    static void shutdown();

    // 编译器/虚拟机的入口在动手之前该断言这个
    [[nodiscard]] static bool ready();

    // 类
    // 下面这些返回的都是借用的裸指针，shutdown 之后一律失效
#define X(id, accessor, name, base) [[nodiscard]] static Type *accessor();
#include "x_builtin_types.inc"

#undef X

    // 内部对象
#define X(id, accessor, name) [[nodiscard]] static Object *accessor();
#include "x_internal_objects.inc"

#undef X

    [[nodiscard]] static NamedSingleton *singleton_none();
    [[nodiscard]] static NamedSingleton *singleton_ellipsis();
    [[nodiscard]] static NamedSingleton *singleton_not_implemented();
    [[nodiscard]] static NamedSingleton *singleton_stop_iteration();
    [[nodiscard]] static Bool *singleton_bool(bool value);
};
