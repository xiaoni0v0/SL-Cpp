#include "BaseException.h"

#include "../Runtime.h"

#include <cassert>
#include <utility>

BaseException::BaseException(Type *const type, std::vector<ObjectRef> args)
    : Object{type}, args_{make_ref<Tuple>(std::move(args))} {
    assert(
        type->is_subtype_of(Runtime::base_exception_type()) &&
        "exception type must derive from BaseException"
    );
}

void BaseException::visit_own_refs(RefVisitor &visitor) { visitor.visit(args_); }
