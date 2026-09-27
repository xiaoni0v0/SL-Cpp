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
