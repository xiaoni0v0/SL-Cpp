#pragma once

#include <cstddef>
#include <type_traits>

// 枚举值的底层数值
template <typename Enum>
    requires std::is_enum_v<Enum>
constexpr std::size_t index_of(const Enum id) {
    return static_cast<std::size_t>(id);
}
