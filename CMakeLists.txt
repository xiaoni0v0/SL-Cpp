cmake_minimum_required(VERSION 3.20)
project(SL_Cpp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
        interpreter/Interpreter.cpp
        interpreter/Interpreter.h
        interpreter/dumper/Dumper.cpp
        interpreter/dumper/Dumper.h
        interpreter/evaler/Evaler.cpp
        interpreter/evaler/Evaler.h
        interpreter/frame/Frame.cpp
        interpreter/frame/Frame.h

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
        # $<$<CONFIG:Debug>:-Werror>
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
