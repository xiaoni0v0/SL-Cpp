#pragma once

#include "Heap.h"
#include "Type.h"
#include "objects/singletons.h"

#include <array>
#include <cstdint>

// 内置类型的编号。清单在 x_builtin_types.inc
enum class BuiltinType : std::size_t {
#define X(field, accessor, name, base) field,
#include "x_builtin_types.inc"
#undef X
    Count,
    NoBase, // 只给 object 用：它没有基类
};

// bootstrap 的相位。**取值有序**，"某个东西现在能不能用"一律表达成"当前相位 >= 某个相位"。
//
// 每一相位的承诺（后面的相位包含前面的）：
//
// | 相位            | 走完之后什么可用                                                       |
// |-----------------|------------------------------------------------------------------------|
// | `Uninitialized` | 只有 Heap。此时建任何 SL 对象都是错的——它拿不到自己的类型             |
// | `Types`         | 全部内置类型对象；object/type 互为对方类型的结已解开                   |
// | `Values`        | 六个单例。**到这里为止，常量表要的一切都能造了**（见 bytecode.md）      |
// | `Ready`         | 全部内置就位，编译器与虚拟机可以跑                                     |
//
// 现在 `Values` 和 `Ready` 之间是空的；异常类树（SL 层）、内置函数表、内置模块表都会插在这中间，
// 各自一个相位。**新加的初始化步骤要按它依赖谁来决定插在哪，不是往 bootstrap 末尾一追了事。**
enum class BootPhase : std::uint8_t {
    Uninitialized,
    Types,
    Values,
    Ready,
};

// 运行时本身：持有全部内置类型与单例，并按正确的顺序把它们建起来。
//
// 进程内只有一个（SL 没有"多解释器"的概念，import / eval_isolated 建的是新的**全局作用域**
// 而不是新的运行时），所以做成全局单例、访问器是静态的——把一个 Runtime& 穿过每个对象构造函数
// 和每条指令，是在为一个 SL 根本不提供的能力付管道费。
//
// 它同时是第一个 GC 根源——内置类型与单例都由它攥着，以后帧栈、模块表、每份 Code 的常量表
// 各自再注册一个。
class Runtime final : public GcRootSource {
    BootPhase phase_{BootPhase::Uninitialized};
    std::array<Ref<Type>, static_cast<std::size_t>(BuiltinType::Count)> types_;
    Ref<Singleton> none_;
    Ref<Singleton> ellipsis_;
    Ref<Singleton> not_implemented_;
    Ref<Singleton> stop_iteration_;
    Ref<Bool> true_;
    Ref<Bool> false_;

    Runtime() = default;

    // —— bootstrap 的各个相位，按声明顺序执行；每个跑完由 init() 推进 phase_ ——
    void build_types();
    void build_singletons();

    // 放掉全部内置类型与单例的引用。放完它们就只剩内部互相引用，是标准的垃圾环
    void release_all();
    // 把运行时拆干净：放引用、摘根源、扫一轮。init() 中途失败和正常 shutdown 共用它
    static void dispose();

    // 取运行时，并核实当前相位够不够 required。**每个访问器都要如实报出自己要求的相位**——
    // 这是"bootstrap 顺序写错了"唯一的自动拦截点：不查的话，早了一步拿到的是个空类型指针，
    // 错误会一路飘到很远的地方才炸
    [[nodiscard]] static Runtime &instance(BootPhase required);
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

    [[nodiscard]] static BootPhase phase();
    // 编译器/虚拟机的入口在动手之前该断言这个
    [[nodiscard]] static bool ready() { return phase() == BootPhase::Ready; }

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
