#include "Tuple.h"

#include "../Runtime.h"

#include <cassert>
#include <utility>

Tuple::Tuple(Type *const type, std::vector<ObjectRef> items)
    : Object{type}, items_{std::move(items)} {}

Tuple::Tuple(std::vector<ObjectRef> items) : Tuple{Runtime::type_tuple(), std::move(items)} {}

void Tuple::visit_own_refs(RefVisitor &visitor) {
    for (RefBase &ref : items_) visitor.visit(ref);
}

Object *Tuple::at(const std::size_t index) const {
    assert(index < items_.size());

    return items_[index].get();
}
