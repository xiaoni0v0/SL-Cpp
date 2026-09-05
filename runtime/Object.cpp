#include "Object.h"

#include "Heap.h"
#include "Type.h"

#include <cassert>

void Object::incref() { ++refcount_; }

void Object::decref() {
    assert(refcount_ > 0 && "decref on refcount 0");

    if (--refcount_ == 0) delete this; // delete this 之后不得再碰任何成员
}

void Object::visit_refs(RefVisitor &visitor) {
    visitor.visit(type_);
    visit_own_refs(visitor);
}

Object::Object(Type *const type) : type_{type} { Heap::link(this); }

Object::~Object() { Heap::unlink(this); }
