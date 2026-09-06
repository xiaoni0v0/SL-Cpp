#include "Int.h"

#include "../Runtime.h"

#include <utility>

Int::Int(Type *const type, BigInt value) : Object{type}, value_{std::move(value)} {}

Int::Int(BigInt value) : Int{Runtime::type_int(), std::move(value)} {}
