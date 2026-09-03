#include "Int.h"

#include "../Runtime.h"

#include <utility>

Int::Int(BigInt value) : Object{Runtime::int_type()}, value_{std::move(value)} {}
