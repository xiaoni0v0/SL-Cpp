#include "Runtime.h"

#include "../cpp_exceptions/InternalError.h"

#include <format>
#include <memory>

namespace {

std::unique_ptr<Runtime> g_runtime;

// 每个内置类型的静态描述，跟 x_builtin_types.inc 一一对应
struct BuiltinTypeSpec {
    const char *name;
    BuiltinType base;
};

constexpr BuiltinTypeSpec kBuiltinTypeSpecs[]{
#define X(field, accessor, name, base) {name, BuiltinType::base},
#include "x_builtin_types.inc"
#undef X
};

constexpr std::size_t index_of(const BuiltinType type) { return static_cast<std::size_t>(type); }

// 相位名，只用于报错
constexpr const char *phase_name(const BootPhase phase) {
    switch (phase) {
    case BootPhase::Uninitialized:
        return "Uninitialized";
    case BootPhase::Types:
        return "Types";
    case BootPhase::Values:
        return "Values";
    case BootPhase::Ready:
        return "Ready";
    }
    return "<未知相位>";
}

} // namespace

void Runtime::build_types() {
    // object 和 type 互为对方需要的东西：type 的基类是 object，而 object 的类型是 type。
    // 这个结只能这么解——先按清单顺序建，元类还不存在的就先留空，建完立刻回填。
    // CPython 靠静态分配的类型结构体 + PyType_Ready 回填，是同一个套路；这里的类型对象
    // 是普通堆对象，于是"留空再回填"就够了，不用为它们另开一套静态存储
    for (std::size_t i{0}; i < index_of(BuiltinType::Count); ++i) {
        const BuiltinTypeSpec &spec{kBuiltinTypeSpecs[i]};

        std::vector<Ref<Type>> bases;
        if (spec.base != BuiltinType::NoBase) bases.push_back(types_[index_of(spec.base)]);

        // 清单里 type 排在第二位，所以只有 object 和 type 自己会拿到空元类
        types_[i] = make_ref<Type>(types_[index_of(BuiltinType::Type)].get(), spec.name, bases);
    }

    Type *const meta{types_[index_of(BuiltinType::Type)].get()};
    types_[index_of(BuiltinType::Object)]->set_type(meta);
    meta->set_type(meta);
}

void Runtime::build_singletons() {
    // None 的类型是 NoneType，另外三个是 SingletonType（SL.md 4.2.17）
    none_ = make_ref<Singleton>(none_type(), "None");
    ellipsis_ = make_ref<Singleton>(singleton_type(), "Ellipsis");
    not_implemented_ = make_ref<Singleton>(singleton_type(), "NotImplemented");
    stop_iteration_ = make_ref<Singleton>(singleton_type(), "StopIteration");
    true_ = make_ref<Bool>(true);
    false_ = make_ref<Bool>(false);
}

void Runtime::visit_roots(RefVisitor &visitor) {
    visitor.visit_each(types_);
    visitor.visit(none_);
    visitor.visit(ellipsis_);
    visitor.visit(not_implemented_);
    visitor.visit(stop_iteration_);
    visitor.visit(true_);
    visitor.visit(false_);
}

void Runtime::release_all() {
    false_.reset();
    true_.reset();
    stop_iteration_.reset();
    not_implemented_.reset();
    ellipsis_.reset();
    none_.reset();
    for (Ref<Type> &type : types_) type.reset();
}

void Runtime::dispose() {
    // 顺序不能反：先放引用、再摘根源、最后扫一轮。
    // 内置类型之间那个环（每个类型都强引用元类 type，而 type 的元类是它自己）引用计数解不开，
    // 只能靠这一轮标记清扫——此时没有任何根，于是整个堆都是垃圾
    g_runtime->release_all();
    Heap::remove_root_source(g_runtime.get());
    Heap::collect();

    g_runtime.reset();
}

Runtime &Runtime::instance(const BootPhase required) {
    if (!g_runtime) throw InternalError{"运行时还没初始化就被访问了"};
    if (g_runtime->phase_ < required) {
        throw InternalError{std::format(
            "bootstrap 相位不足：这里要求 {}，而当前只到 {}。多半是某个初始化步骤排错了位置",
            phase_name(required),
            phase_name(g_runtime->phase_)
        )};
    }
    return *g_runtime;
}

void Runtime::init() {
    if (g_runtime) throw InternalError{"运行时被初始化了两次"};
    // 先装好全局指针再建东西：各对象的构造函数要经由访问器拿自己的类型。
    // 此时相位还是 Uninitialized，任何"早了一步"的访问都会被 instance() 挡下
    g_runtime.reset(new Runtime{});
    Heap::add_root_source(g_runtime.get());

    try {
        g_runtime->build_types();
        g_runtime->phase_ = BootPhase::Types;

        g_runtime->build_singletons();
        g_runtime->phase_ = BootPhase::Values;

        // 异常类树、内置函数表、内置模块表将来插在这里，各自推进一个相位
        g_runtime->phase_ = BootPhase::Ready;
    } catch (...) {
        // 半初始化的运行时比没有运行时更难查：拆干净再把异常放出去
        dispose();
        throw;
    }
}

void Runtime::shutdown() {
    if (!g_runtime) throw InternalError{"运行时没初始化就被关闭了"};
    dispose();
}

BootPhase Runtime::phase() { return g_runtime ? g_runtime->phase_ : BootPhase::Uninitialized; }

Type *Runtime::builtin_type(const BuiltinType id) {
    return instance(BootPhase::Types).types_[index_of(id)].get();
}

// 类型对象在 Types 相位就位
#define X(field, accessor, name, base)                                                             \
    Type *Runtime::accessor() { return builtin_type(BuiltinType::field); }
#include "x_builtin_types.inc"
#undef X

// 单例在 Values 相位才有
Object *Runtime::none() { return instance(BootPhase::Values).none_.get(); }
Object *Runtime::ellipsis() { return instance(BootPhase::Values).ellipsis_.get(); }
Object *Runtime::not_implemented() { return instance(BootPhase::Values).not_implemented_.get(); }
Object *Runtime::stop_iteration() { return instance(BootPhase::Values).stop_iteration_.get(); }

Bool *Runtime::boolean(const bool value) {
    const Runtime &self{instance(BootPhase::Values)};
    return value ? self.true_.get() : self.false_.get();
}
