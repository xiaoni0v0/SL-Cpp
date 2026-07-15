cmake_minimum_required(VERSION 3.20)
project(SL_Cpp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# 所有的源文件
add_executable(SL_Cpp main.cpp
        # utils
        utils/string_utils.cpp
        utils/string_utils.h
        utils/file_utils.cpp
        utils/file_utils.h

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

        # Interpreter
        interpreter/dumper/Dumper.cpp
        interpreter/dumper/Dumper.h

        # Parser
        parser/Parser.cpp
        parser/Parser.h
        parser/ast_nodes/ast_nodes.h
        parser/ast_nodes/x_ast_nodes.h
        parser/ast_nodes/details/ast_node.h
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
target_compile_options(SL_Cpp PRIVATE
        -Wall -Wextra -pedantic -Wshadow -Wpointer-arith
        $<$<CONFIG:Debug>:-Werror>
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-O0>
        $<$<CONFIG:Release>:-O2>
)

# 链接选项
target_link_options(SL_Cpp PRIVATE
        $<$<CONFIG:Release>:-static>
)

# 预编译头
target_precompile_headers(SL_Cpp PRIVATE
        <string>
        <vector>
        <memory>
        <utility>
        <optional>
        <stdexcept>
        <format>
)

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
FetchContent_MakeAvailable(nlohmann_json cxxopts)
target_link_libraries(SL_Cpp PRIVATE nlohmann_json::nlohmann_json cxxopts::cxxopts)

# ============================================================
# 测试
# ============================================================

enable_testing()

add_executable(SL_Cpp_Tests
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

FetchContent_Declare(
        doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG v2.5.3
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(doctest)
target_link_libraries(SL_Cpp_Tests PRIVATE doctest::doctest)
target_compile_options(SL_Cpp_Tests PRIVATE -Wall -Wextra -pedantic -Wshadow -Wpointer-arith -Werror -O0)

add_test(NAME SL_Cpp_Tests COMMAND SL_Cpp_Tests)
