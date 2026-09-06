#include "Decimal.h"

#include "../Runtime.h"

#include <utility>

Decimal::Decimal(Type *const type, BigDec value) : Object{type}, value_{std::move(value)} {}

Decimal::Decimal(BigDec value) : Decimal{Runtime::type_decimal(), std::move(value)} {}
