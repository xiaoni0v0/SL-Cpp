#include "BaseException.h"

#include "../Runtime.h"

#include <cassert>
#include <utility>

BaseException::BaseException(Type *const type, std::vector<ObjectRef> args)
    : Object{type}, args_{make_ref<Tuple>(std::move(args))} {
    assert(
        type->is_subtype_of(Runtime::base_exception_type()) &&
        "异常对象的类型必须是 BaseException 或它的子类"
    );
}

void BaseException::visit_own_refs(RefVisitor &visitor) { visitor.visit(args_); }
