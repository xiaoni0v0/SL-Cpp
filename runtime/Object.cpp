#include "Object.h"

#include "Heap.h"
#include "Type.h"

#include <cassert>

void Object::incref() { ++refcount_; }

void Object::decref() {
    assert(refcount_ > 0 && "decref 了一个引用计数已经为 0 的对象");

    if (--refcount_ == 0) delete this; // delete this 之后不得再碰任何成员
}

void Object::set_type(Type *const type) { type_ = Ref{type}; }

void Object::visit_all_refs(RefVisitor &visitor) {
    visitor.visit(type_);
    visit_own_refs(visitor);
}

Object::Object(Type *const type) : type_{type} { Heap::link(this); }

Object::~Object() { Heap::unlink(this); }
