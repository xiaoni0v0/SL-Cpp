#include "Type.h"

#include "../cpp_exceptions/InternalError.h"

#include <algorithm>
#include <utility>

Type::Type(Type *const meta, std::string name, std::vector<Ref<Type>> bases)
    : Object{meta}, name_{std::move(name)}, bases_{std::move(bases)} {
    if (bases_.size() > 1) {
        // C3 线性化要等 SL 层能自己定义类（class 表达式）时再写
        throw InternalError{std::format("Type '{}': multiple bases (C3 not implemented)", name_)};
    }

    mro_.push_back(this);
    if (!bases_.empty()) {
        const std::vector<Type *> &base_mro{bases_.front()->mro_};
        mro_.insert(mro_.end(), base_mro.begin(), base_mro.end());
    }
}

void Type::visit_own_refs(RefVisitor &visitor) {
    // 只报 bases_
    for (RefBase &ref : bases_) visitor.visit(ref);
}

bool Type::is_subtype_of(const Type *const other) const {
    return std::ranges::find(mro_, other) != mro_.end();
}

std::size_t Type::size_bytes() const {
    return sizeof(*this) + name_.capacity() + bases_.capacity() * sizeof(Ref<Type>) +
           mro_.capacity() * sizeof(Type *);
}
