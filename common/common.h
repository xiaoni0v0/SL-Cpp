#pragma once

#include <concepts>
#include <type_traits>

// ============================================================
// 编译器 id，用宏 SL_COMPILER
// ============================================================

#define SL_COMPILER_GCC 1
#define SL_COMPILER_CLANG 2
#define SL_COMPILER_CLANG_CL 3
#define SL_COMPILER_MSVC 4

#if defined(__clang__) && defined(_MSC_VER)
#define SL_COMPILER SL_COMPILER_CLANG_CL
#elif defined(__clang__)
#define SL_COMPILER SL_COMPILER_CLANG
#elif defined(_MSC_VER)
#define SL_COMPILER SL_COMPILER_MSVC
#elif defined(__GNUC__)
#define SL_COMPILER SL_COMPILER_GCC
#else
#error "unsupported compiler. only supports gcc / clang / clang-cl / cl"
#endif

// ============================================================
// 内置函数
// ============================================================

#ifndef __has_builtin
#define __has_builtin(x) false
#endif

// ============================================================
// 原 <stdckdint.h>，返回的 bool 表示是否溢出。
// ============================================================

// 缺任何一个就包含
#if !__has_builtin(__builtin_add_overflow) || !__has_builtin(__builtin_sub_overflow) ||            \
    !__has_builtin(__builtin_mul_overflow)
#include <SafeInt.hpp>
#endif

template <std::integral Result, std::integral T1, std::integral T2>
    requires(
        std::is_same_v<Result, T1> && (!std::is_same_v<T1, bool>) && (!std::is_same_v<T2, bool>)
    )
[[nodiscard]] bool sl_overflow_add(Result *result, T1 a, T2 b) {
#if __has_builtin(__builtin_add_overflow)
    return __builtin_add_overflow(a, b, result);
#else
    return !SafeAdd(a, b, *result);
#endif
}

template <std::integral T1, std::integral T2>
    requires((!std::is_same_v<T1, bool>) && (!std::is_same_v<T2, bool>) )
[[nodiscard]] bool sl_overflow_add(T1 a, T2 b) {
#if __has_builtin(__builtin_add_overflow_p)
    return __builtin_add_overflow_p(a, b, static_cast<T1>(0));
#else
    T1 tmp{};
    return sl_overflow_add(&tmp, a, b);
#endif
}

template <typename... Args> bool sl_overflow_add(Args &&...args) = delete;

template <std::integral Result, std::integral T1, std::integral T2>
    requires(
        std::is_same_v<Result, T1> && (!std::is_same_v<T1, bool>) && (!std::is_same_v<T2, bool>)
    )
[[nodiscard]] bool sl_overflow_sub(Result *result, T1 a, T2 b) {
#if __has_builtin(__builtin_sub_overflow)
    return __builtin_sub_overflow(a, b, result);
#else
    return !SafeSubtract(a, b, *result);
#endif
}

template <std::integral T1, std::integral T2>
    requires((!std::is_same_v<T1, bool>) && (!std::is_same_v<T2, bool>) )
[[nodiscard]] bool sl_overflow_sub(T1 a, T2 b) {
#if __has_builtin(__builtin_sub_overflow_p)
    return __builtin_sub_overflow_p(a, b, static_cast<T1>(0));
#else
    T1 tmp{};
    return sl_overflow_sub(&tmp, a, b);
#endif
}

template <typename... Args> bool sl_overflow_sub(Args &&...args) = delete;

template <std::integral Result, std::integral T1, std::integral T2>
    requires(
        std::is_same_v<Result, T1> && (!std::is_same_v<T1, bool>) && (!std::is_same_v<T2, bool>)
    )
[[nodiscard]] bool sl_overflow_mul(Result *result, T1 a, T2 b) {
#if __has_builtin(__builtin_mul_overflow)
    return __builtin_mul_overflow(a, b, result);
#else
    return !SafeMultiply(a, b, *result);
#endif
}

template <std::integral T1, std::integral T2>
    requires((!std::is_same_v<T1, bool>) && (!std::is_same_v<T2, bool>) )
[[nodiscard]] bool sl_overflow_mul(T1 a, T2 b) {
#if __has_builtin(__builtin_mul_overflow_p)
    return __builtin_mul_overflow_p(a, b, static_cast<T1>(0));
#else
    T1 tmp{};
    return sl_overflow_mul(&tmp, a, b);
#endif
}

template <typename... Args> bool sl_overflow_mul(Args &&...args) = delete;
