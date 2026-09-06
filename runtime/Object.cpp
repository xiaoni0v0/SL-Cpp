#include "Object.h"

#include "Heap.h"
#include "Type.h"

#include <cassert>

RefBase::RefBase(Object *const ptr) : ptr_{ptr} {
    if (ptr_) ptr_->incref();
}

RefBase::~RefBase() {
    if (ptr_) ptr_->decref();
}

void RefBase::reset() {
    if (ptr_) ptr_->decref();
    ptr_ = nullptr;
}

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

void Object::set_type(Type *const type) { type_ = Ref{type}; }

Object::~Object() { Heap::unlink(this); }

Type *Object::type() const { return type_.get(); }
