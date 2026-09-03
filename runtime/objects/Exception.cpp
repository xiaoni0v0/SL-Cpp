#include "Exception.h"

#include "../Runtime.h"

#include <cassert>
#include <utility>

Exception::Exception(Type *const type, std::vector<ObjectRef> args)
    : Object{type}, args_{make_ref<Tuple>(std::move(args))} {
    assert(
        type->is_subtype_of(Runtime::base_exception_type()) &&
        "Exception 的类型必须是 BaseException 的子类"
    );
}

void Exception::visit_own_refs(RefVisitor &visitor) { visitor(args_); }
