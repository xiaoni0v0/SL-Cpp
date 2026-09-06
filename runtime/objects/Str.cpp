#include "Str.h"

#include "../../utils/string_utils.h"
#include "../Runtime.h"

#include <utility>

Str::Str(Type *const type, std::u32string value) : Object{type}, value_{std::move(value)} {}

Str::Str(std::u32string value) : Str{Runtime::type_str(), std::move(value)} {}

Ref<Str> Str::from_utf8(const std::string &utf8) { return make_ref<Str>(utf8_to_u32(utf8)); }

std::string Str::to_utf8() const { return u32_to_utf8(value_); }
