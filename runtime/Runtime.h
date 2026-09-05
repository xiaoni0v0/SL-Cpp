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

    // 把运行时拆干净。跟 init() 一样是对全局槽位本身的操作（末尾要把它置空），所以是 static
    static void tear_down();

    // 取那个唯一的运行时对象；没 init() 或已经 shutdown() 就访问，抛 InternalError
    [[nodiscard]] static Runtime &instance();
    // 各类型访问器共用的实现，省得宏展开出一堆同样的函数体
    [[nodiscard]] static Type *builtin_type(BuiltinType id);

  public:
    // 覆写基类 GcRootSource 的公开接口，可见性跟基类保持一致（收紧会被 CLion/编译器警告，
    // 而且这本来就是给 Heap 通过 GcRootSource* 调用的公开契约，不是该收紧的内部实现细节）
    void visit_roots(RefVisitor &visitor) override;

    // init() 中途抛异常时会回滚
    static void init();
    static void shutdown();

    // 编译器/虚拟机的入口在动手之前该断言这个
    [[nodiscard]] static bool ready();

    // 下面这些返回的都是**借用**的裸指针：运行时活着期间它们恒有效，要长期持有请自己包 Ref。
    // shutdown 之后一律失效
#define X(id, accessor, name, base) [[nodiscard]] static Type *accessor();
#include "x_builtin_types.inc"

#undef X

    [[nodiscard]] static NamedSingleton *none();
    [[nodiscard]] static NamedSingleton *ellipsis();
    [[nodiscard]] static NamedSingleton *not_implemented();
    [[nodiscard]] static NamedSingleton *stop_iteration();
    [[nodiscard]] static Bool *boolean(bool value);
};
