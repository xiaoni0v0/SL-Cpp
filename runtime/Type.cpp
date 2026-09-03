#include "Type.h"

#include "../cpp_exceptions/InternalError.h"

#include <algorithm>
#include <utility>

Type::Type(Type *const meta, std::string name, std::vector<Ref<Type>> bases)
    : Object{meta}, name_{std::move(name)}, bases_{std::move(bases)} {
    if (bases_.size() > 1) {
        // C3 线性化要等 SL 层能自己定义类（class 表达式）时再写；内置类型全是单继承
        // （SL.md 4.4 那张图里每个类都只有一个父类），现在到不了这里
        throw InternalError{std::format("类 {} 有多个基类，C3 线性化还没实现", name_)};
    }

    mro_.push_back(this);
    if (!bases_.empty()) {
        const std::vector<Type *> &base_mro{bases_.front()->mro_};
        mro_.insert(mro_.end(), base_mro.begin(), base_mro.end());
    }
}

void Type::visit_own_refs(RefVisitor &visitor) {
    // 只报 bases_。mro_ 是借用，且其中每个类都能沿 bases_ 到达，不用重复报。
    // 清理阶段把 bases_ 的槽位抹空之后 mro_ 里的裸指针就可能悬垂了——那时这个对象已经是
    // 待释放的垃圾，没人会再读它
    visitor.visit_each(bases_);
}

bool Type::is_subtype_of(const Type *const other) const {
    return std::ranges::find(mro_, other) != mro_.end();
}
