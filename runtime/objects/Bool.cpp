#include "Bool.h"

#include "../Object.h"

Bool::Bool(Type *type, const bool value) : Object{type}, value_{value} {}
