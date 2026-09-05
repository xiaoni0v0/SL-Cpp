#include "Int.h"

#include "../Runtime.h"

#include <utility>

Int::Int(BigInt value) : Object{Runtime::type_int()}, value_{std::move(value)} {}
