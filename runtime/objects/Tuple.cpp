#include "Tuple.h"

#include "../Runtime.h"

#include <cassert>
#include <utility>

Tuple::Tuple(std::vector<ObjectRef> items)
    : Object{Runtime::type_tuple()}, items_{std::move(items)} {}

void Tuple::visit_own_refs(RefVisitor &visitor) { visitor.visit_each(items_); }

Object *Tuple::at(const std::size_t index) const {
    assert(index < items_.size());
    return items_[index].get();
}
