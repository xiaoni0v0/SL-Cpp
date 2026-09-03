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

void Runtime::bootstrap() {
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

    // 单例。None 的类型是 NoneType，另外三个是 SingletonType（SL.md 4.2.17）
    none_ = make_ref<Singleton>(none_type(), "None");
    ellipsis_ = make_ref<Singleton>(singleton_type(), "Ellipsis");
    not_implemented_ = make_ref<Singleton>(singleton_type(), "NotImplemented");
    stop_iteration_ = make_ref<Singleton>(singleton_type(), "StopIteration");
    true_ = make_ref<Bool>(true);
    false_ = make_ref<Bool>(false);
}

void Runtime::visit_roots(RefVisitor &visitor) {
    visitor.visit_each(types_);
    visitor(none_);
    visitor(ellipsis_);
    visitor(not_implemented_);
    visitor(stop_iteration_);
    visitor(true_);
    visitor(false_);
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

Runtime &Runtime::instance() {
    if (!g_runtime) throw InternalError{"运行时还没初始化就被访问了"};
    return *g_runtime;
}

void Runtime::init() {
    if (g_runtime) throw InternalError{"运行时被初始化了两次"};
    // 先装好全局指针再 bootstrap：各对象的构造函数要经由访问器拿自己的类型
    g_runtime.reset(new Runtime{});
    Heap::add_root_source(g_runtime.get());
    g_runtime->bootstrap();
}

void Runtime::shutdown() {
    if (!g_runtime) throw InternalError{"运行时没初始化就被关闭了"};

    // 顺序不能反：先放引用、再摘根源、最后扫一轮。
    // 内置类型之间那个环（每个类型都强引用元类 type，而 type 的元类是它自己）引用计数解不开，
    // 只能靠这一轮标记清扫——此时没有任何根，于是整个堆都是垃圾
    g_runtime->release_all();
    Heap::remove_root_source(g_runtime.get());
    Heap::collect();

    g_runtime.reset();
}

bool Runtime::initialized() { return g_runtime != nullptr; }

#define X(field, accessor, name, base)                                                             \
    Type *Runtime::accessor() { return instance().types_[index_of(BuiltinType::field)].get(); }
#include "x_builtin_types.inc"
#undef X

Object *Runtime::none() { return instance().none_.get(); }
Object *Runtime::ellipsis() { return instance().ellipsis_.get(); }
Object *Runtime::not_implemented() { return instance().not_implemented_.get(); }
Object *Runtime::stop_iteration() { return instance().stop_iteration_.get(); }

Bool *Runtime::boolean(const bool value) {
    const Runtime &self{instance()};
    return value ? self.true_.get() : self.false_.get();
}
