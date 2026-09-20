#pragma once

#include <concepts>

#include <SafeInt.hpp>

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
// 原 <stdckdint.h>，返回的 bool 表示是否溢出
// ============================================================

template <std::integral type_result, std::integral type1, std::integral type2>
    requires std::is_same_v<type_result, type1> &&
             ((!std::is_same_v<type1, bool>) && (!std::is_same_v<type2, bool>) )
bool sl_overflow_add(type_result &result, type1 a, type2 b) {
#if __has_builtin(__builtin_add_overflow)
    return __builtin_add_overflow(a, b, &result);
#else
    return !SafeAdd(a, b, result);
#endif
}

template <typename... Args> bool sl_overflow_add(Args &&...args) = delete;

template <std::integral type_result, std::integral type1, std::integral type2>
    requires std::is_same_v<type_result, type1> &&
             ((!std::is_same_v<type1, bool>) && (!std::is_same_v<type2, bool>) )
bool sl_overflow_sub(type_result &result, type1 a, type2 b) {
#if __has_builtin(__builtin_sub_overflow)
    return __builtin_sub_overflow(a, b, &result);
#else
    return !SafeSubtract(a, b, result);
#endif
}

template <typename... Args> bool sl_overflow_sub(Args &&...args) = delete;

template <std::integral type_result, std::integral type1, std::integral type2>
    requires std::is_same_v<type_result, type1> &&
             ((!std::is_same_v<type1, bool>) && (!std::is_same_v<type2, bool>) )
bool sl_overflow_mul(type_result &result, type1 a, type2 b) {
#if __has_builtin(__builtin_mul_overflow)
    return __builtin_mul_overflow(a, b, &result);
#else
    return !SafeMultiply(a, b, result);
#endif
}

template <typename... Args> bool sl_overflow_mul(Args &&...args) = delete;
