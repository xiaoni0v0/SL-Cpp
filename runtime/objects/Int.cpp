#include "Int.h"

#include "../../numeric/BigInt.h"
#include "../Object.h"
#include "../Runtime.h"
#include "../Type.h"

#include <utility>

Int::Int(Type *type, BigInt value) : Object{type}, value_{std::move(value)} {}

Int::Int(BigInt value) : Int{Runtime::type_int(), std::move(value)} {}
