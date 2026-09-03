#include "Decimal.h"

#include "../Runtime.h"

#include <utility>

Decimal::Decimal(BigDec value) : Object{Runtime::decimal_type()}, value_{std::move(value)} {}
