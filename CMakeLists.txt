cmake_minimum_required(VERSION 3.20)
project(SL_Cpp)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# 允许通过 MSVC_RUNTIME_LIBRARY 属性控制 MSVC 运行时库的链接方式（静态 /MT 还是动态 /MD）而不是被 CMake 默认策略覆盖。
if (POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif ()

# ------------------------------------------------------------
# 前端识别
#   - GCC:           CMAKE_CXX_COMPILER_ID = "GNU",   FRONTEND_VARIANT = ""
#   - Clang:         CMAKE_CXX_COMPILER_ID = "Clang", FRONTEND_VARIANT = "GNU" 或 ""
#   - clang-cl:      CMAKE_CXX_COMPILER_ID = "Clang", FRONTEND_VARIANT = "MSVC"
#   - MSVC（不支持）: CMAKE_CXX_COMPILER_ID = "MSVC",  FRONTEND_VARIANT = ""
# ------------------------------------------------------------
if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR
            "不支持的编译器 CMAKE_CXX_COMPILER_ID=\"${CMAKE_CXX_COMPILER_ID}\"，目前只支持 GCC / Clang / clang-cl")
endif ()

if (NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "" AND
        NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU" AND
        NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    message(FATAL_ERROR
            "不支持的编译器前端命令行风格 CMAKE_CXX_COMPILER_FRONTEND_VARIANT=\"${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}\"")
endif ()

# 是否为 MSVC 命令行风格。上面已经把 cl.exe 挡掉了，所以这里为真只可能是 clang-cl
set(SL_MSVC_LIKE_FRONTEND FALSE)
if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    set(SL_MSVC_LIKE_FRONTEND TRUE)
    add_compile_options(/utf-8 /bigobj)
endif ()

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
    if (SL_MSVC_LIKE_FRONTEND)
        target_compile_options(${target} PRIVATE
                /W4 /permissive- /EHsc -Wshadow
                $<$<CONFIG:Debug>:/WX>
                $<$<CONFIG:Debug>:/Zi>
                $<$<CONFIG:Debug>:/Od>
                $<$<CONFIG:Release>:/O2>
        )
    else ()
        target_compile_options(${target} PRIVATE
                -Wall -Wextra -pedantic -Wshadow -Wpointer-arith -Winvalid-pch
                $<$<CONFIG:Debug>:-Werror>
                $<$<CONFIG:Debug>:-g>
                $<$<CONFIG:Debug>:-O0>
                $<$<CONFIG:Release>:-O2>
        )
    endif ()
endfunction()

function(sl_apply_test_target_options target)
    if (SL_MSVC_LIKE_FRONTEND)
        target_compile_options(${target} PRIVATE /W4 /permissive- /EHsc -Wshadow /WX /Od)
    else ()
        target_compile_options(${target} PRIVATE -Wall -Wextra -pedantic -Wshadow -Wpointer-arith -Werror -O0)
    endif ()
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

# 所有的源文件
add_executable(SL main.cpp
        # utils
        utils/string_utils.cpp
        utils/string_utils.h
        utils/file_utils.cpp
        utils/file_utils.h

        # numeric
        numeric/BigInt.cpp
        numeric/BigInt.h

        # Executor
        executor/Executor.cpp
        executor/Executor.h

        # Lexer
        lexer/Lexer.cpp
        lexer/Lexer.h
        lexer/token.h
        lexer/x_token_type.h
        lexer/x_keyword.h
        lexer/x_reservedword.h

        # builtins
        builtins/exceptions/InternalError.h
        builtins/exceptions/SLException.h
        builtins/exceptions/SyntaxError.h
        builtins/exceptions/EncodingError.h
        builtins/exceptions/FileNotFoundError.h

        # Parser
        parser/Parser.cpp
        parser/Parser.h
        parser/ast_nodes/ast_nodes.h
        parser/ast_nodes/x_ast_nodes.h
        parser/ast_nodes/details/ast_node.h
        parser/ast_nodes/details/ast_node_capture.h
        parser/ast_nodes/details/ast_node_class.h
        parser/ast_nodes/details/ast_node_control_flows.h
        parser/ast_nodes/details/ast_node_decorators.h
        parser/ast_nodes/details/ast_node_func.h
        parser/ast_nodes/details/ast_node_import.h
        parser/ast_nodes/details/ast_node_kwarg.h
        parser/ast_nodes/details/ast_node_literals.h
        parser/ast_nodes/details/ast_node_multi_exprs.h
        parser/ast_nodes/details/ast_node_operators.h
        parser/ast_nodes/details/ast_node_postfix.h
        parser/ast_nodes/details/ast_node_var.h
        parser/ast_nodes/to_json.cpp

        # Analyzer
        analyzer/Analyzer.cpp
        analyzer/Analyzer.h
        analyzer/semantic_checker/SemanticChecker.cpp
        analyzer/semantic_checker/SemanticChecker.h
        analyzer/expr_folder/ExprFolder.cpp
        analyzer/expr_folder/ExprFolder.h
        analyzer/expr_folder/StaticEvaler.cpp
        analyzer/expr_folder/StaticEvaler.h
)

# 编译选项
sl_apply_main_target_options(SL)

# 链接选项
if (SL_MSVC_LIKE_FRONTEND)
    set_property(TARGET SL PROPERTY
            MSVC_RUNTIME_LIBRARY "$<IF:$<CONFIG:Debug>,MultiThreadedDebugDLL,MultiThreaded>"
    )
else ()
    target_link_options(SL PRIVATE
            $<$<CONFIG:Release>:-static-libgcc>
            $<$<CONFIG:Release>:-static-libstdc++>
    )
endif ()

# 链接库
target_link_libraries(SL PRIVATE nlohmann_json::nlohmann_json cxxopts::cxxopts)

# 预编译头
target_precompile_headers(SL PRIVATE
        <algorithm>
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
        <nlohmann/json.hpp>
)

# ============================================================
# 测试
# ============================================================

enable_testing()

add_executable(SL_Cpp_Numeric_Tests
        # 被测代码
        numeric/BigInt.cpp
        numeric/BigInt.h

        # 测试工具
        test/numeric/main_test.cpp

        # 测试代码
        test/numeric/big_int_test.cpp
)
sl_apply_test_target_options(SL_Cpp_Numeric_Tests)
target_link_libraries(SL_Cpp_Numeric_Tests PRIVATE doctest::doctest)
add_test(NAME SL_Cpp_Numeric_Tests COMMAND SL_Cpp_Numeric_Tests)

add_executable(SL_Cpp_Lexer_Tests
        # 被测代码
        lexer/Lexer.cpp
        lexer/Lexer.h
        lexer/token.h
        lexer/x_token_type.h
        lexer/x_keyword.h
        lexer/x_reservedword.h
        utils/string_utils.cpp
        utils/string_utils.h

        # 测试工具
        test/lexer/test_utils.h
        test/lexer/main_test.cpp

        # 测试代码
        # 2.1.1 注释
        test/lexer/2_1_1_comments/comments_test.cpp
        # 2.1.2 关键字与保留字
        test/lexer/2_1_2_keywords_reserved/keywords_test.cpp
        test/lexer/2_1_2_keywords_reserved/reserved_words_test.cpp
        # 2.1.3 标识符
        test/lexer/2_1_3_identifiers/identifiers_test.cpp
        # 2.1.4 字面量
        test/lexer/2_1_4_literals/none_bool_g_l_ellipsis_test.cpp
        test/lexer/2_1_4_literals/int_test.cpp
        test/lexer/2_1_4_literals/float_test.cpp
        test/lexer/2_1_4_literals/string_test.cpp
        test/lexer/2_1_4_literals/raw_string_test.cpp
        # 2.1.5 运算符与符号
        test/lexer/2_1_5_operators/symbols_test.cpp
        test/lexer/2_1_5_operators/dot_disambiguation_test.cpp
        # 2.1.6 空白字符
        test/lexer/2_1_6_whitespace/whitespace_test.cpp
        # 跨章节：位置追踪、报错
        test/lexer/common/position_tracking_test.cpp
        test/lexer/common/error_handling_test.cpp
)
sl_apply_test_target_options(SL_Cpp_Lexer_Tests)
target_link_libraries(SL_Cpp_Lexer_Tests PRIVATE doctest::doctest)
target_precompile_headers(SL_Cpp_Lexer_Tests PRIVATE
        <doctest/doctest.h>
)
set_source_files_properties(test/lexer/main_test.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
add_test(NAME SL_Cpp_Lexer_Tests COMMAND SL_Cpp_Lexer_Tests)

add_executable(SL_Cpp_Parser_Tests
        # 被测代码
        lexer/Lexer.cpp
        lexer/Lexer.h
        lexer/token.h
        lexer/x_token_type.h
        lexer/x_keyword.h
        lexer/x_reservedword.h
        utils/string_utils.cpp
        utils/string_utils.h
        parser/Parser.cpp
        parser/Parser.h
        parser/ast_nodes/to_json.cpp
        parser/ast_nodes/ast_nodes.h
        parser/ast_nodes/x_ast_nodes.h
        parser/ast_nodes/details/ast_node.h
        parser/ast_nodes/details/ast_node_class.h
        parser/ast_nodes/details/ast_node_control_flows.h
        parser/ast_nodes/details/ast_node_decorators.h
        parser/ast_nodes/details/ast_node_func.h
        parser/ast_nodes/details/ast_node_import.h
        parser/ast_nodes/details/ast_node_kwarg.h
        parser/ast_nodes/details/ast_node_literals.h
        parser/ast_nodes/details/ast_node_multi_exprs.h
        parser/ast_nodes/details/ast_node_operators.h
        parser/ast_nodes/details/ast_node_postfix.h
        parser/ast_nodes/details/ast_node_var.h

        # 测试工具
        test/parser/test_utils.h
        test/parser/main_test.cpp

        # 测试代码
        # 2.1.4 字面量
        test/parser/2_1_4_literals/literals_test.cpp
        # 2.1.5 运算符
        test/parser/2_1_5_operators/precedence_test.cpp
        test/parser/2_1_5_operators/compare_is_assign_test.cpp
        # 2.2.1 表达式分隔符
        test/parser/2_2_1_separators/separator_test.cpp
        # 2.2.2 基本表达式（{} 判别规则）
        test/parser/2_2_2_basic_exprs/compound_dict_test.cpp
        # 2.2.3 del
        test/parser/2_2_3_del/del_test.cpp
        # 2.2.4 global
        test/parser/2_2_4_global/global_test.cpp
        # 2.2.5 import
        test/parser/2_2_5_import/import_test.cpp
        # 2.2.5 控制流
        test/parser/2_2_5_control_flow/if_test.cpp
        test/parser/2_2_5_control_flow/for_test.cpp
        test/parser/2_2_5_control_flow/while_test.cpp
        test/parser/2_2_5_control_flow/break_continue_test.cpp
        test/parser/2_2_5_control_flow/return_test.cpp
        test/parser/2_2_5_control_flow/try_test.cpp
        test/parser/2_2_5_control_flow/raise_test.cpp
        # 2.2.6 函数
        test/parser/2_2_6_func/func_test.cpp
        # 2.2.7 类
        test/parser/2_2_7_class/class_test.cpp
        # 2.2.8 装饰器
        test/parser/2_2_8_decorator/decorator_test.cpp
        # 跨章节：位置追踪、报错
        test/parser/common/position_tracking_test.cpp
        test/parser/common/error_handling_test.cpp
        test/parser/common/combination_test.cpp
)
sl_apply_test_target_options(SL_Cpp_Parser_Tests)
target_link_libraries(SL_Cpp_Parser_Tests PRIVATE doctest::doctest nlohmann_json::nlohmann_json)
target_precompile_headers(SL_Cpp_Parser_Tests PRIVATE
        <nlohmann/json.hpp>
        <doctest/doctest.h>
)
set_source_files_properties(test/parser/main_test.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
add_test(NAME SL_Cpp_Parser_Tests COMMAND SL_Cpp_Parser_Tests)

add_executable(SL_Cpp_Analyzer_Tests
        # 被测代码
        lexer/Lexer.cpp
        lexer/Lexer.h
        lexer/token.h
        lexer/x_token_type.h
        lexer/x_keyword.h
        lexer/x_reservedword.h
        utils/string_utils.cpp
        utils/string_utils.h
        numeric/BigInt.cpp
        numeric/BigInt.h
        parser/Parser.cpp
        parser/Parser.h
        parser/ast_nodes/to_json.cpp
        parser/ast_nodes/ast_nodes.h
        parser/ast_nodes/x_ast_nodes.h
        parser/ast_nodes/details/ast_node.h
        parser/ast_nodes/details/ast_node_class.h
        parser/ast_nodes/details/ast_node_control_flows.h
        parser/ast_nodes/details/ast_node_decorators.h
        parser/ast_nodes/details/ast_node_func.h
        parser/ast_nodes/details/ast_node_import.h
        parser/ast_nodes/details/ast_node_kwarg.h
        parser/ast_nodes/details/ast_node_literals.h
        parser/ast_nodes/details/ast_node_multi_exprs.h
        parser/ast_nodes/details/ast_node_operators.h
        parser/ast_nodes/details/ast_node_postfix.h
        parser/ast_nodes/details/ast_node_var.h
        builtins/exceptions/InternalError.h
        analyzer/expr_folder/ExprFolder.cpp
        analyzer/expr_folder/ExprFolder.h
        analyzer/expr_folder/StaticEvaler.cpp
        analyzer/expr_folder/StaticEvaler.h
        analyzer/semantic_checker/SemanticChecker.cpp
        analyzer/semantic_checker/SemanticChecker.h

        # 测试工具（复用 Parser 测试的 parse_program/parse_json 等）
        test/parser/test_utils.h
        test/analyzer/test_utils.h
        test/analyzer/semantic_checker/test_utils.h
        test/analyzer/main_test.cpp

        # 测试代码：expr_folder
        test/analyzer/expr_folder/arithmetic_test.cpp
        test/analyzer/expr_folder/bitwise_test.cpp
        test/analyzer/expr_folder/container_ops_test.cpp
        test/analyzer/expr_folder/compare_is_test.cpp
        test/analyzer/expr_folder/logical_test.cpp
        test/analyzer/expr_folder/dead_branch_test.cpp
        test/analyzer/expr_folder/compound_test.cpp
        test/analyzer/expr_folder/program_prune_test.cpp
        test/analyzer/expr_folder/try_raise_test.cpp
        test/analyzer/expr_folder/func_class_decorator_test.cpp
        test/analyzer/expr_folder/call_index_attr_assign_test.cpp
        test/analyzer/expr_folder/import_test.cpp

        # 测试代码：semantic_checker
        test/analyzer/semantic_checker/scope_test.cpp
        test/analyzer/semantic_checker/func_class_test.cpp
        test/analyzer/semantic_checker/lvalue_test.cpp
        test/analyzer/semantic_checker/dict_call_test.cpp
        test/analyzer/semantic_checker/import_test.cpp
        test/analyzer/semantic_checker/defensive_test.cpp
)
sl_apply_test_target_options(SL_Cpp_Analyzer_Tests)
target_link_libraries(SL_Cpp_Analyzer_Tests PRIVATE doctest::doctest nlohmann_json::nlohmann_json)
target_precompile_headers(SL_Cpp_Analyzer_Tests PRIVATE
        <nlohmann/json.hpp>
        <doctest/doctest.h>
)
set_source_files_properties(test/analyzer/main_test.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
add_test(NAME SL_Cpp_Analyzer_Tests COMMAND SL_Cpp_Analyzer_Tests)
