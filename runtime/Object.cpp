#include "Object.h"

#include "Heap.h"
#include "Type.h"

#include <cassert>

Object::Object(Type *const type) : type_{type} { Heap::link(this); }

Object::~Object() { Heap::unlink(this); }

void Object::set_type(Type *const type) { type_ = Ref<Type>{type}; }

void Object::visit_all_refs(RefVisitor &visitor) {
    visitor(type_);
    visit_own_refs(visitor);
}

void incref(Object *const obj) { ++obj->refcount_; }

void decref(Object *const obj) {
    assert(obj->refcount_ > 0 && "decref 了一个引用计数已经为 0 的对象");
    if (--obj->refcount_ == 0) delete obj;
}
