cmake_minimum_required(VERSION 3.20)
project(SL_Cpp)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

if (NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "" AND
        NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU")
    message(FATAL_ERROR
            "不支持的编译器前端命令行风格 CMAKE_CXX_COMPILER_FRONTEND_VARIANT=\"${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}\"")
endif ()

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
        builtins/object/SlObject.cpp
        builtins/object/SlObject.h
        builtins/classes/exceptions/SLException.h
        builtins/classes/exceptions/SyntaxError.h
        builtins/classes/exceptions/EncodingError.h
        builtins/classes/exceptions/FileNotFoundError.h

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
        parser/ast_nodes/details/ast_node_literals.h
        parser/ast_nodes/details/ast_node_multi_exprs.h
        parser/ast_nodes/details/ast_node_operators.h
        parser/ast_nodes/details/ast_node_postfix.h
        parser/ast_nodes/details/ast_node_var.h

        # Analyzer
        analyzer/Analyzer.cpp
        analyzer/Analyzer.h
        analyzer/syntax_checker/SyntaxChecker.cpp
        analyzer/syntax_checker/SyntaxChecker.h
        analyzer/literal_folder/LiteralFolder.cpp
        analyzer/literal_folder/LiteralFolder.h
        analyzer/literal_folder/StaticEvaler.cpp
        analyzer/literal_folder/StaticEvaler.h
)

# 编译选项
target_compile_options(SL PRIVATE
        -Wall -Wextra -pedantic -Wshadow -Wpointer-arith -Winvalid-pch
        $<$<CONFIG:Debug>:-Werror>
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-O0>
        $<$<CONFIG:Release>:-O2>
)

# 链接选项
target_link_options(SL PRIVATE
        $<$<CONFIG:Release>:-static>
)

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

target_link_libraries(SL PRIVATE nlohmann_json::nlohmann_json cxxopts::cxxopts)

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
target_compile_options(SL_Cpp_Numeric_Tests PRIVATE -Wall -Wextra -pedantic -Wshadow -Wpointer-arith -Werror -O0)
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
target_precompile_headers(SL_Cpp_Lexer_Tests PRIVATE
        <doctest/doctest.h>
)
set_source_files_properties(test/lexer/main_test.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
target_compile_options(SL_Cpp_Lexer_Tests PRIVATE -Wall -Wextra -pedantic -Wshadow -Wpointer-arith -Werror -O0)
target_link_libraries(SL_Cpp_Lexer_Tests PRIVATE doctest::doctest)
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
        parser/ast_nodes/ast_nodes.h
        parser/ast_nodes/x_ast_nodes.h
        parser/ast_nodes/details/ast_node.h
        parser/ast_nodes/details/ast_node_class.h
        parser/ast_nodes/details/ast_node_control_flows.h
        parser/ast_nodes/details/ast_node_decorators.h
        parser/ast_nodes/details/ast_node_func.h
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
target_precompile_headers(SL_Cpp_Parser_Tests PRIVATE
        <nlohmann/json.hpp>
        <doctest/doctest.h>
)
set_source_files_properties(test/parser/main_test.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
target_compile_options(SL_Cpp_Parser_Tests PRIVATE -Wall -Wextra -pedantic -Wshadow -Wpointer-arith -Werror -O0)
target_link_libraries(SL_Cpp_Parser_Tests PRIVATE doctest::doctest nlohmann_json::nlohmann_json)
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
        parser/ast_nodes/ast_nodes.h
        parser/ast_nodes/x_ast_nodes.h
        parser/ast_nodes/details/ast_node.h
        parser/ast_nodes/details/ast_node_class.h
        parser/ast_nodes/details/ast_node_control_flows.h
        parser/ast_nodes/details/ast_node_decorators.h
        parser/ast_nodes/details/ast_node_func.h
        parser/ast_nodes/details/ast_node_literals.h
        parser/ast_nodes/details/ast_node_multi_exprs.h
        parser/ast_nodes/details/ast_node_operators.h
        parser/ast_nodes/details/ast_node_postfix.h
        parser/ast_nodes/details/ast_node_var.h
        analyzer/literal_folder/LiteralFolder.cpp
        analyzer/literal_folder/LiteralFolder.h
        analyzer/literal_folder/StaticEvaler.cpp
        analyzer/literal_folder/StaticEvaler.h
        analyzer/syntax_checker/SyntaxChecker.cpp
        analyzer/syntax_checker/SyntaxChecker.h

        # 测试工具（复用 Parser 测试的 parse_program/parse_json 等）
        test/parser/test_utils.h
        test/analyzer/test_utils.h
        test/analyzer/syntax_checker/test_utils.h
        test/analyzer/main_test.cpp

        # 测试代码：literal_folder
        test/analyzer/literal_folder/arithmetic_test.cpp
        test/analyzer/literal_folder/bitwise_test.cpp
        test/analyzer/literal_folder/container_ops_test.cpp
        test/analyzer/literal_folder/compare_is_test.cpp
        test/analyzer/literal_folder/logical_test.cpp
        test/analyzer/literal_folder/dead_branch_test.cpp

        # 测试代码：syntax_checker
        test/analyzer/syntax_checker/scope_test.cpp
        test/analyzer/syntax_checker/func_class_test.cpp
        test/analyzer/syntax_checker/lvalue_test.cpp
        test/analyzer/syntax_checker/dict_call_test.cpp
        test/analyzer/syntax_checker/defensive_test.cpp
)
target_precompile_headers(SL_Cpp_Analyzer_Tests PRIVATE
        <nlohmann/json.hpp>
        <doctest/doctest.h>
)
set_source_files_properties(test/analyzer/main_test.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
target_compile_options(SL_Cpp_Analyzer_Tests PRIVATE -Wall -Wextra -pedantic -Wshadow -Wpointer-arith -Werror -O0)
target_link_libraries(SL_Cpp_Analyzer_Tests PRIVATE doctest::doctest nlohmann_json::nlohmann_json)
add_test(NAME SL_Cpp_Analyzer_Tests COMMAND SL_Cpp_Analyzer_Tests)
