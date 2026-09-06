#pragma once

#include <climits>
#include <concepts>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

/**
 * 算一个值额外占了多少堆内存的一组重载。不含 `sizeof(值)` 本身。典型用法是：
 *     sizeof(*this) + mem::heap_bytes(字段a) + mem::heap_bytes(字段b) + ...;
 */
namespace mem {

// 自己知道怎么算的类型
template <typename T>
concept SelfReportsHeapBytes = requires(const T &value) {
    { value.heap_bytes() } -> std::convertible_to<std::size_t>;
};

// 平凡可复制 = 没有析构函数 = 不可能拥有需要释放的堆内存
template <typename T>
concept NoOwnedHeap = std::is_trivially_copyable_v<T> && !SelfReportsHeapBytes<T>;

template <SelfReportsHeapBytes T> [[nodiscard]] constexpr std::size_t heap_bytes(const T &value);

template <NoOwnedHeap T> [[nodiscard]] constexpr std::size_t heap_bytes(const T &value);

template <typename C, typename Tr, typename A>
[[nodiscard]] constexpr std::size_t heap_bytes(const std::basic_string<C, Tr, A> &value);

template <typename T, typename A>
[[nodiscard]] constexpr std::size_t heap_bytes(const std::vector<T, A> &value);

template <typename A>
[[nodiscard]] constexpr std::size_t heap_bytes(const std::vector<bool, A> &value);

template <typename T> [[nodiscard]] constexpr std::size_t heap_bytes(const std::optional<T> &value);

template <SelfReportsHeapBytes T> constexpr std::size_t heap_bytes(const T &value) {
    return value.heap_bytes();
}

template <NoOwnedHeap T> constexpr std::size_t heap_bytes(const T &) { return 0; }

// 重载 string
template <typename C, typename Tr, typename A>
constexpr std::size_t heap_bytes(const std::basic_string<C, Tr, A> &value) {
    const auto *const begin{reinterpret_cast<const std::byte *>(&value)};
    const auto *const end{begin + sizeof(std::basic_string<C, Tr, A>)};
    const auto *const at{reinterpret_cast<const std::byte *>(value.data())};

    if (constexpr std::less<> before; !before(at, begin) && before(at, end)) return 0;
    return (value.capacity() + 1) * sizeof(C); // + 1 for '\0'
}

// 重载 vector
template <typename T, typename A> constexpr std::size_t heap_bytes(const std::vector<T, A> &value) {
    std::size_t bytes{value.capacity() * sizeof(T)};
    for (const T &item : value) bytes += heap_bytes(item);
    return bytes;
}

// 重载 vector<bool>
template <typename A> constexpr std::size_t heap_bytes(const std::vector<bool, A> &value) {
    return (value.capacity() + CHAR_BIT - 1) / CHAR_BIT;
}

// 重载 optional
template <typename T> constexpr std::size_t heap_bytes(const std::optional<T> &value) {
    return value ? heap_bytes(*value) : 0;
}

} // namespace mem
