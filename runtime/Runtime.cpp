#include "Runtime.h"

#include "../cpp_exceptions/InternalError.h"

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
    // 全部直接读 types_，不经过 none_type()/bool_type() 这类公开访问器——
    // 这正是让 BootPhase 变得没必要的那处改动，见类注释
    Type *const none_type{types_[index_of(BuiltinType::NoneType)].get()};
    Type *const singleton_type{types_[index_of(BuiltinType::SingletonType)].get()};
    Type *const bool_type{types_[index_of(BuiltinType::Bool)].get()};

    // None 的类型是 NoneType，另外三个是 SingletonType（SL.md 4.2.17）
    singletons_.none_ = make_ref<Singleton>(none_type, "None");
    singletons_.ellipsis_ = make_ref<Singleton>(singleton_type, "Ellipsis");
    singletons_.not_implemented_ = make_ref<Singleton>(singleton_type, "NotImplemented");
    singletons_.stop_iteration_ = make_ref<Singleton>(singleton_type, "StopIteration");
    singletons_.true_ = make_ref<Bool>(bool_type, true);
    singletons_.false_ = make_ref<Bool>(bool_type, false);
}

void Runtime::visit_roots(RefVisitor &visitor) {
    visitor.visit_each(types_);
    visitor.visit(singletons_.none_);
    visitor.visit(singletons_.ellipsis_);
    visitor.visit(singletons_.not_implemented_);
    visitor.visit(singletons_.stop_iteration_);
    visitor.visit(singletons_.true_);
    visitor.visit(singletons_.false_);
}

void Runtime::release_all() {
    singletons_ = {};
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

Runtime &Runtime::instance() {
    if (!g_runtime) throw InternalError{"运行时还没初始化就被访问了"};
    return *g_runtime;
}

void Runtime::init() {
    if (g_runtime) throw InternalError{"运行时被初始化了两次"};
    g_runtime.reset(new Runtime{});
    Heap::add_root_source(g_runtime.get());

    try {
        g_runtime->build_types(); // 内置类型（含异常类树）
        g_runtime->build_singletons();
        // 内置函数表、内置模块表将来插在这里
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

bool Runtime::ready() { return g_runtime != nullptr; }

Type *Runtime::builtin_type(const BuiltinType id) { return instance().types_[index_of(id)].get(); }

#define X(field, accessor, name, base)                                                             \
    Type *Runtime::accessor() { return builtin_type(BuiltinType::field); }
#include "x_builtin_types.inc"
#undef X

Object *Runtime::none() { return instance().singletons_.none_.get(); }
Object *Runtime::ellipsis() { return instance().singletons_.ellipsis_.get(); }
Object *Runtime::not_implemented() { return instance().singletons_.not_implemented_.get(); }
Object *Runtime::stop_iteration() { return instance().singletons_.stop_iteration_.get(); }

Bool *Runtime::boolean(const bool value) {
    const Runtime &self{instance()};
    return value ? self.singletons_.true_.get() : self.singletons_.false_.get();
}
