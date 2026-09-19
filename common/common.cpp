#include "common.h"

template <std::integral type_result, std::integral type1, std::integral type2>
constexpr bool sl_checked_add_impl(type_result *result, type1 a, type2 b) {
    return false;
}

template <std::integral type_result, std::integral type1, std::integral type2>
constexpr bool sl_checked_sub_impl(type_result *result, type1 a, type2 b) {
    return false;
}

template <std::integral type_result, std::integral type1, std::integral type2>
constexpr bool sl_checked_mul_impl(type_result *result, type1 a, type2 b) {
    return false;
}
