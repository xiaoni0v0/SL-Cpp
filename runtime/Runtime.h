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

/**
 * 运行时本身。
 *
 * 持有全部内置类型与单例，按顺序建好。
 * 进程内唯一（做成全局单例），也是第一个引用根。
 */
class Runtime final : public GcRootSource {
    std::array<Ref<Type>, static_cast<std::size_t>(BuiltinType::Count)> types_;

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

    // bootstrap 的两步，按声明顺序执行
    void build_types();
    void build_singletons();

    // 自己拆掉 Runtime，init / shutdown 调用
    static void tear_down();

    // 取单例
    [[nodiscard]] static Runtime &instance();
    // 下边各类型访问器共用的实现
    [[nodiscard]] static Type *builtin_type(BuiltinType id);

  public:
    void visit_roots(RefVisitor &visitor) override;

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

    [[nodiscard]] static NamedSingleton *singleton_none();
    [[nodiscard]] static NamedSingleton *singleton_ellipsis();
    [[nodiscard]] static NamedSingleton *singleton_not_implemented();
    [[nodiscard]] static NamedSingleton *singleton_stop_iteration();
    [[nodiscard]] static Bool *singleton_bool(bool value);
};
