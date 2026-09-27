#include "BaseException.h"

#include "../Object.h"
#include "../Runtime.h"
#include "../Type.h"
#include "Tuple.h"

#include <cassert>
#include <utility>
#include <vector>

BaseException::BaseException(Type *type, std::vector<ObjectRef> args)
    : Object{type}, args_{make_ref<Tuple>(std::move(args))} {
    assert(type->is_subtype_of(Runtime::type_base_exception()));
}

void BaseException::visit_own_refs(RefVisitor &visitor) { visitor.visit(args_); }
