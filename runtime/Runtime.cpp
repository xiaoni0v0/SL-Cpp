#include "Runtime.h"

#include "../cppexceptions/InternalError.h"

#include <memory>
#include <vector>

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
    Type *meta{types_[index_of(BuiltinType::Type)].get()};
    types_[index_of(BuiltinType::Object)]->set_meta(meta);
    meta->set_meta(meta);
}

void Runtime::build_singletons() {
    std::size_t slot{0};

#define X(id, accessor, cpp_type, type_id, arg)                                                    \
    singletons_[slot++] = make_ref<cpp_type>(types_[index_of(BuiltinType::type_id)].get(), arg);
#include "x_singletons.inc"

#undef X
}

void Runtime::build_internals() {
    Type *singleton_type{types_[index_of(BuiltinType::SingletonType)].get()};

    std::size_t slot{0};
#define X(id, accessor, name) internals_[slot++] = make_ref<NamedSingleton>(singleton_type, name);
#include "x_internal_objects.inc"

#undef X
}

void Runtime::tear_down() {
    // 跟建的顺序相反
    for (ObjectRef &internal : g_runtime->internals_) internal.reset();
    for (ObjectRef &singleton : g_runtime->singletons_) singleton.reset();
    for (Ref<Type> &type : g_runtime->types_) type.reset();

    Heap::collect();

    g_runtime.reset();
}

Runtime &Runtime::instance() {
    if (!g_runtime) throw InternalError{"Runtime: use before init"};
    return *g_runtime;
}

Type *Runtime::builtin_type(const BuiltinType id) { return instance().types_[index_of(id)].get(); }

Object *Runtime::singleton(const Singleton id) {
    return instance().singletons_[static_cast<std::size_t>(id)].target();
}

Object *Runtime::internal_object(const InternalObject id) {
    return instance().internals_[static_cast<std::size_t>(id)].target();
}

void Runtime::visit_roots(RefVisitor &visitor) {
    Runtime &self{instance()};

    for (RefBase &ref : self.types_) visitor.visit(ref);
    for (RefBase &ref : self.singletons_) visitor.visit(ref);
    for (RefBase &ref : self.internals_) visitor.visit(ref);
}

void Runtime::init() {
    if (g_runtime) throw InternalError{"Runtime: double init"};
    g_runtime.reset(new Runtime{});

    try {
        g_runtime->build_types();
        g_runtime->build_singletons();
        g_runtime->build_internals();
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

#define X(id, accessor, cpp_type, type_id, arg)                                                    \
    cpp_type *Runtime::accessor() { return static_cast<cpp_type *>(singleton(Singleton::id)); }
#include "x_singletons.inc"
#undef X

Bool *Runtime::singleton_bool(const bool value) {
    return value ? singleton_true() : singleton_false();
}

#define X(id, accessor, name)                                                                      \
    Object *Runtime::accessor() { return internal_object(InternalObject::id); }
#include "x_internal_objects.inc"
#undef X
