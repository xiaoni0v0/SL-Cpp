#include "Decimal.h"

#include "../../numeric/BigDec.h"
#include "../Object.h"
#include "../Runtime.h"
#include "../Type.h"

#include <utility>

Decimal::Decimal(Type *type, BigDec value) : Object{type}, value_{std::move(value)} {}

Decimal::Decimal(BigDec value) : Decimal{Runtime::type_decimal(), std::move(value)} {}
