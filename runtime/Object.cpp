#include "Object.h"

#include "Heap.h"
#include "Type.h"

#include <cassert>

void incref(Object *const obj) { ++obj->refcount_; }

void decref(Object *const obj) {
    assert(obj->refcount_ > 0 && "decref 了一个引用计数已经为 0 的对象");

    if (--obj->refcount_ == 0) delete obj;
}

void Object::set_type(Type *const type) { type_ = Ref{type}; }

void Object::visit_all_refs(RefVisitor &visitor) {
    visitor(type_);
    visit_own_refs(visitor);
}

Object::Object(Type *const type) : type_{type} { Heap::link(this); }

Object::~Object() { Heap::unlink(this); }
