#pragma once

#include "Heap.h"
#include "Type.h"
#include "objects/singletons.h"

#include <array>

// 内置类型的编号
enum class BuiltinType : std::size_t {
#define X(field, accessor, name, base) field,
#include "x_builtin_types.inc"

#undef X
    Count,
    NoBase, // 只给 object 用：它没有基类
};

/**
 * 运行时本身。
 *
 *持有全部内置类型与单例，按顺序建好。
 * 进程内唯一（做成全局单例），也是第一个 GC 根源。
 */
class Runtime final : public GcRootSource {
    std::array<Ref<Type>, static_cast<std::size_t>(BuiltinType::Count)> types_;

    // 六个单例。凑成一个聚合体，放掉它们只要 `singletons_ = {}` 一句
    struct {
        Ref<Singleton> none_;
        Ref<Singleton> ellipsis_;
        Ref<Singleton> not_implemented_;
        Ref<Singleton> stop_iteration_;
        Ref<Bool> true_;
        Ref<Bool> false_;
    } singletons_;

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
