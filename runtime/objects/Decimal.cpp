#include "Decimal.h"

#include "../Runtime.h"

#include <utility>

Decimal::Decimal(BigDec value) : Object{Runtime::type_decimal()}, value_{std::move(value)} {}
