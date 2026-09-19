#pragma once

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
// 原 <stdckdint.h>
// ============================================================

template <std::integral type_result, std::integral type1, std::integral type2>
constexpr bool sl_checked_add_impl(type_result *result, type1 a, type2 b);

template <std::integral type_result, std::integral type1, std::integral type2>
constexpr bool sl_checked_sub_impl(type_result *result, type1 a, type2 b);

template <std::integral type_result, std::integral type1, std::integral type2>
constexpr bool sl_checked_mul_impl(type_result *result, type1 a, type2 b);

#if __has_builtin(__builtin_add_overflow)
#define sl_checked_add(result, a, b) __builtin_add_overflow(a, b, result)
#else
#define sl_checked_add(result, a, b) sl_checked_add_impl(result, a, b)
#endif
#if __has_builtin(__builtin_sub_overflow)
#define sl_checked_sub(result, a, b) __builtin_sub_overflow(a, b, result)
#else
#define sl_checked_sub(result, a, b) sl_checked_sub_impl(result, a, b)
#endif
#if __has_builtin(__builtin_mul_overflow)
#define sl_checked_mul(result, a, b) __builtin_mul_overflow(a, b, result)
#else
#define sl_checked_mul(result, a, b) sl_checked_mul_impl(result, a, b)
#endif
