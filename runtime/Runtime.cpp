#include "Runtime.h"

#include "../cpp_exceptions/InternalError.h"

#include <memory>

namespace {

std::unique_ptr<Runtime> g_runtime;

// 每个内置类型的静态描述，编号 + 名字
struct BuiltinTypeSpec {
    const char *name;
    BuiltinType base;
};

constexpr BuiltinTypeSpec kBuiltinTypeSpecs[]{
#define X(id, accessor, name, base) {name, BuiltinType::base},
#include "x_builtin_types.inc"

#undef X
};

constexpr std::size_t index_of(const BuiltinType type) { return static_cast<std::size_t>(type); }

} // namespace

void Runtime::build_types() {
    for (std::size_t i{0}; i < index_of(BuiltinType::Count); ++i) {
        const auto &[name, base]{kBuiltinTypeSpecs[i]};

        std::vector<Ref<Type>> bases;
        if (base != BuiltinType::NoBase) bases.push_back(types_[index_of(base)]); // 父类

        // 清单里 type 排在第二位，所以只有 object 和 type 自己会拿到空元类
        types_[i] = make_ref<Type>(types_[index_of(BuiltinType::Type)].get(), name, bases);
    }

    // 回填 Object 和 Type 的类型为 Type
    Type *const meta{types_[index_of(BuiltinType::Type)].get()};
    types_[index_of(BuiltinType::Object)]->type_ = Ref{meta};
    meta->type_ = Ref{meta};
}

void Runtime::build_singletons() {
    // 全部直接读 types_，不经过 none_type()/bool_type() 这些公开访问器——那些访问器要求
    // 运行时已经建好，而这里恰恰是"正在建"，绕道就成了自我循环
    Type *const none_type{types_[index_of(BuiltinType::NoneType)].get()};
    Type *const singleton_type{types_[index_of(BuiltinType::SingletonType)].get()};
    Type *const bool_type{types_[index_of(BuiltinType::Bool)].get()};

    // None 的类型是 NoneType，另外三个是 SingletonType
    singletons_.none_ = make_ref<NamedSingleton>(none_type, "None");
    singletons_.ellipsis_ = make_ref<NamedSingleton>(singleton_type, "Ellipsis");
    singletons_.not_implemented_ = make_ref<NamedSingleton>(singleton_type, "NotImplemented");
    singletons_.stop_iteration_ = make_ref<NamedSingleton>(singleton_type, "StopIteration");
    singletons_.true_ = make_ref<Bool>(bool_type, true);
    singletons_.false_ = make_ref<Bool>(bool_type, false);
}

void Runtime::tear_down() {
    g_runtime->singletons_ = {};
    for (Ref<Type> &type : g_runtime->types_) type.reset();

    Heap::remove_root_source(g_runtime.get());
    Heap::collect();

    g_runtime.reset();
}

Runtime &Runtime::instance() {
    if (!g_runtime) throw InternalError{"Runtime: use before init"};
    return *g_runtime;
}

Type *Runtime::builtin_type(const BuiltinType id) { return instance().types_[index_of(id)].get(); }

void Runtime::visit_roots(RefVisitor &visitor) {
    visitor.visit_each(types_);
    visitor.visit(singletons_.none_);
    visitor.visit(singletons_.ellipsis_);
    visitor.visit(singletons_.not_implemented_);
    visitor.visit(singletons_.stop_iteration_);
    visitor.visit(singletons_.true_);
    visitor.visit(singletons_.false_);
}

void Runtime::init() {
    if (g_runtime) throw InternalError{"Runtime: double init"};
    g_runtime.reset(new Runtime{});
    Heap::add_root_source(g_runtime.get());

    try {
        g_runtime->build_types();
        g_runtime->build_singletons();
    } catch (...) {
        tear_down();
        throw;
    }
}

void Runtime::shutdown() {
    if (!g_runtime) throw InternalError{"Runtime: shutdown before init"};
    tear_down();
}

bool Runtime::ready() { return g_runtime != nullptr; }

#define X(id, accessor, name, base)                                                                \
    Type *Runtime::accessor() { return builtin_type(BuiltinType::id); }
#include "x_builtin_types.inc"
#undef X

NamedSingleton *Runtime::none() { return instance().singletons_.none_.get(); }

NamedSingleton *Runtime::ellipsis() { return instance().singletons_.ellipsis_.get(); }

NamedSingleton *Runtime::not_implemented() { return instance().singletons_.not_implemented_.get(); }

NamedSingleton *Runtime::stop_iteration() { return instance().singletons_.stop_iteration_.get(); }

Bool *Runtime::boolean(const bool value) {
    const Runtime &self{instance()};
    return value ? self.singletons_.true_.get() : self.singletons_.false_.get();
}
