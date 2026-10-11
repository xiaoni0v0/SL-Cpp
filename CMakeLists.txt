cmake_minimum_required(VERSION 3.20)

# 允许通过 MSVC_RUNTIME_LIBRARY 属性控制 MSVC 运行时库的链接方式（静态 /MT 还是动态 /MD）
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(SL_Cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ------------------------------------------------------------
# 前端识别
#   - GCC:      CMAKE_CXX_COMPILER_ID = "GNU",   FRONTEND_VARIANT = ""
#   - Clang:    CMAKE_CXX_COMPILER_ID = "Clang", FRONTEND_VARIANT = "GNU" 或 ""
#   - clang-cl: CMAKE_CXX_COMPILER_ID = "Clang", FRONTEND_VARIANT = "MSVC"
#   - MSVC:     CMAKE_CXX_COMPILER_ID = "MSVC",  FRONTEND_VARIANT = ""
# ------------------------------------------------------------

if(
    NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
    AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
    AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC"
)
    message(
        FATAL_ERROR
        "unsupported compiler CMAKE_CXX_COMPILER_ID=\"${CMAKE_CXX_COMPILER_ID}\", only gcc / clang / clang-cl / cl are supported"
    )
endif()

if(
    NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL ""
    AND NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU"
    AND NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC"
)
    message(
        FATAL_ERROR
        "unsupported compiler frontend CMAKE_CXX_COMPILER_FRONTEND_VARIANT=\"${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}\""
    )
endif()

# 是否为 MSVC 命令行风格。包括 clang-cl 和 cl
set(SL_MSVC_LIKE_FRONTEND FALSE)
if(
    CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC"
    OR CMAKE_CXX_COMPILER_ID STREQUAL "MSVC"
)
    set(SL_MSVC_LIKE_FRONTEND TRUE)
    add_compile_options(/utf-8 /bigobj)
    # 静态链接
    set(CMAKE_MSVC_RUNTIME_LIBRARY
        "$<IF:$<CONFIG:Debug>,MultiThreadedDebugDLL,MultiThreaded>"
    )
elseif(MINGW)
    # 静态链接
    add_link_options(
        $<$<CONFIG:Release>:-static-libgcc>
        $<$<CONFIG:Release>:-static-libstdc++>
    )
endif()

# 构建类型
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
endif()

# ------------------------------------------------------------
# 编译参数统一封装
# 对照关系：
#   -Wall -Wextra -pedantic  ->  /W4 /permissive-
#   -Werror                  ->  /WX
#   -g                       ->  /Zi
#   -O0 / -O3                ->  /Od / /O2
#   -Wpointer-arith -Winvalid-pch 省略。
#   /EHsc 是 MSVC 下开启标准 C++ 异常处理语义所必需的。
#
# SL_NATIVE=ON：在 Release 基础上再加探测本机 CPU 才能定的参数。cl 没有。
# ------------------------------------------------------------

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

option(SL_NATIVE "在 Release 基础上加本机 CPU 专属优化" OFF)

function(sl_apply_target_options target)
    if(SL_MSVC_LIKE_FRONTEND)
        target_compile_options(
            ${target}
            PRIVATE
                /W4
                /WX
                /permissive-
                /EHsc
                $<$<CONFIG:Debug>:/Zi>
                $<$<CONFIG:Debug>:/Od>
                $<$<CONFIG:Release>:/O2>
        )
        if(SL_NATIVE AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            target_compile_options(
                ${target}
                PRIVATE
                    $<$<CONFIG:Release>:/clang:-march=native>
                    $<$<CONFIG:Release>:/clang:-mtune=native>
            )
        endif()

    else()
        target_compile_options(
            ${target}
            PRIVATE
                -Wall
                -Wextra
                -Werror
                -pedantic
                -Wpointer-arith
                -Winvalid-pch
                $<$<CONFIG:Debug>:-g>
                $<$<CONFIG:Debug>:-O0>
                $<$<CONFIG:Release>:-O3>
        )
        if(SL_NATIVE)
            target_compile_options(
                ${target}
                PRIVATE
                    $<$<CONFIG:Release>:-march=native>
                    $<$<CONFIG:Release>:-mtune=native>
            )
        endif()
    endif()
endfunction()

add_library(sl_pch_stub OBJECT common/pch_stub.cpp) # 一个空 cpp
sl_apply_target_options(sl_pch_stub)
target_precompile_headers(
    sl_pch_stub
    PRIVATE
        <algorithm>
        <concepts>
        <format>
        <memory>
        <optional>
        <stdexcept>
        <string>
        <string_view>
        <utility>
        <vector>
)

# 建一个模块静态库
function(sl_add_module target)
    add_library(${target} STATIC ${ARGN})
    sl_apply_target_options(${target})
    target_precompile_headers(${target} REUSE_FROM sl_pch_stub)
endfunction()

# 第三方库
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
    GIT_SHALLOW TRUE
)
FetchContent_Declare(
    cxxopts
    GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
    GIT_TAG v3.3.1
    GIT_SHALLOW TRUE
)
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.5.3
    GIT_SHALLOW TRUE
)
FetchContent_Declare(
    safeint
    GIT_REPOSITORY https://github.com/dcleblanc/SafeInt.git
    GIT_TAG 3.24
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nlohmann_json cxxopts doctest safeint)
add_library(SafeInt INTERFACE)
target_include_directories(SafeInt SYSTEM INTERFACE ${safeint_SOURCE_DIR})

# ------------------------------------------------------------
# 各模块。依赖方向：cppexceptions <- utils/numeric <- runtime <- compiler <- executor
# ------------------------------------------------------------
add_subdirectory(common)
add_subdirectory(cppexceptions)
add_subdirectory(utils)
add_subdirectory(numeric)
add_subdirectory(runtime)
add_subdirectory(compiler)
add_subdirectory(executor)

# ------------------------------------------------------------
# 可执行文件
# ------------------------------------------------------------
add_executable(SL main.cpp)
sl_apply_target_options(SL)
target_link_libraries(SL PRIVATE sl_executor cxxopts::cxxopts)

# ------------------------------------------------------------
# 测试
# ------------------------------------------------------------
enable_testing()
add_subdirectory(test)
