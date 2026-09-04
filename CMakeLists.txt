cmake_minimum_required(VERSION 3.20)
project(SL_Cpp)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# 允许通过 MSVC_RUNTIME_LIBRARY 属性控制 MSVC 运行时库的链接方式（静态 /MT 还是动态 /MD）而不是被 CMake 默认策略覆盖。
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

# ------------------------------------------------------------
# 前端识别
#   - GCC:           CMAKE_CXX_COMPILER_ID = "GNU",   FRONTEND_VARIANT = ""
#   - Clang:         CMAKE_CXX_COMPILER_ID = "Clang", FRONTEND_VARIANT = "GNU" 或 ""
#   - clang-cl:      CMAKE_CXX_COMPILER_ID = "Clang", FRONTEND_VARIANT = "MSVC"
#   - MSVC（不支持）: CMAKE_CXX_COMPILER_ID = "MSVC",  FRONTEND_VARIANT = ""
# ------------------------------------------------------------

if(
    NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
    AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
)
    message(
        FATAL_ERROR
        "不支持的编译器 CMAKE_CXX_COMPILER_ID=\"${CMAKE_CXX_COMPILER_ID}\"，目前只支持 GCC / Clang / clang-cl"
    )
endif()

if(
    NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL ""
    AND NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU"
    AND NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC"
)
    message(
        FATAL_ERROR
        "不支持的编译器前端命令行风格 CMAKE_CXX_COMPILER_FRONTEND_VARIANT=\"${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}\""
    )
endif()

# 是否为 MSVC 命令行风格。上面已经把 cl.exe 挡掉了，所以这里为真只可能是 clang-cl
set(SL_MSVC_LIKE_FRONTEND FALSE)
if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    set(SL_MSVC_LIKE_FRONTEND TRUE)
    add_compile_options(/utf-8 /bigobj)
    # 所有目标（含各静态库）统一运行时库
    set(CMAKE_MSVC_RUNTIME_LIBRARY
        "$<IF:$<CONFIG:Debug>,MultiThreadedDebugDLL,MultiThreaded>"
    )
endif()

# ------------------------------------------------------------
# 编译参数统一封装
# 对照关系：
#   -Wall -Wextra -pedantic  ->  /W4 /permissive-
#   -Werror                  ->  /WX
#   -g                       ->  /Zi
#   -O0 / -O2                ->  /Od / /O2
#   -Wshadow -Wpointer-arith -Winvalid-pch 省略。
#   /EHsc 是 MSVC 下开启标准 C++ 异常处理语义所必需的。
# ------------------------------------------------------------

function(sl_apply_main_target_options target)
    if(SL_MSVC_LIKE_FRONTEND)
        target_compile_options(
            ${target}
            PRIVATE
                /W4
                /permissive-
                /EHsc
                -Wshadow
                $<$<CONFIG:Debug>:/WX>
                $<$<CONFIG:Debug>:/Zi>
                $<$<CONFIG:Debug>:/Od>
                $<$<CONFIG:Release>:/O2>
        )
    else()
        target_compile_options(
            ${target}
            PRIVATE
                -Wall
                -Wextra
                -pedantic
                -Wshadow
                -Wpointer-arith
                -Winvalid-pch
                $<$<CONFIG:Debug>:-Werror>
                $<$<CONFIG:Debug>:-g>
                $<$<CONFIG:Debug>:-O0>
                $<$<CONFIG:Release>:-O2>
        )
    endif()
endfunction()

function(sl_apply_test_target_options target)
    if(SL_MSVC_LIKE_FRONTEND)
        target_compile_options(
            ${target}
            PRIVATE /W4 /permissive- /EHsc -Wshadow /WX /Od
        )
    else()
        target_compile_options(
            ${target}
            PRIVATE -Wall -Wextra -pedantic -Wshadow -Wpointer-arith -Werror -O0
        )
    endif()
endfunction()

# 各模块共用的预编译头（只放标准库）
function(sl_apply_pch target)
    target_precompile_headers(
        ${target}
        PRIVATE
            <algorithm>
            <charconv>
            <filesystem>
            <format>
            <iostream>
            <memory>
            <optional>
            <ranges>
            <stdexcept>
            <string>
            <utility>
            <vector>
    )
endfunction()

# 建一个模块静态库
function(sl_add_module target)
    add_library(${target} STATIC ${ARGN})
    sl_apply_main_target_options(${target})
    sl_apply_pch(${target})
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
FetchContent_MakeAvailable(nlohmann_json cxxopts doctest)

# ------------------------------------------------------------
# 各模块。依赖方向：cpp_exceptions <- utils/numeric <- runtime <- compiler <- executor
# ------------------------------------------------------------
add_subdirectory(cpp_exceptions)
add_subdirectory(utils)
add_subdirectory(numeric)
add_subdirectory(runtime)
add_subdirectory(compiler)
add_subdirectory(executor)

# ------------------------------------------------------------
# 可执行文件
# ------------------------------------------------------------
add_executable(SL main.cpp)
sl_apply_main_target_options(SL)
# 这个 sl_runtime 是临时的，防止只构建 main target 时跳过 sl_runtime 的编译
target_link_libraries(SL PRIVATE sl_executor cxxopts::cxxopts sl_runtime)

if(MINGW)
    target_link_options(
        SL
        PRIVATE
            $<$<CONFIG:Release>:-static-libgcc>
            $<$<CONFIG:Release>:-static-libstdc++>
    )
endif()

# ------------------------------------------------------------
# 测试
# ------------------------------------------------------------
enable_testing()
add_subdirectory(test)
