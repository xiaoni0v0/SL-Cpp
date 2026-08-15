#include "Executor.h"

#include "../analyzer/Analyzer.h"
#include "../builtins/exceptions/SLException.h"
#include "../lexer/Lexer.h"
#include "../parser/Parser.h"
#include "../parser/ast_nodes/details/ast_node_multi_exprs.h"
#include "../utils/file_utils.h"
#include "../utils/string_utils.h"

#include <filesystem>
#include <iostream>
#include <string>

static std::string path_abspath(const std::string &path) {
    std::error_code ec;
    return std::filesystem::absolute(path, ec).string();
}

Executor::Executor(const std::string &s) : file_path{path_abspath(s)} {}

int Executor::run() const {
    // 输入文件不存在
    if (!std::filesystem::exists(file_path)) {
        std::cerr << "Source code file \"" << file_path << "\" does not exist." << std::endl;
        return 1;
    }

    // 1. 分词器（源代码 -> token 数组）
    std::vector<Token> tokens;
    try {
        tokens = Lexer{utf8_to_u32(file_read_all(file_path), file_path), file_path}.tokenize();
        for (const auto &token : tokens) {
            std::cout << Lexer::get_typename_by_tokentype(token.type);
            if (!(token.type == TokenType::NEWLINE || token.type == TokenType::END_OF_FILE)) {
                std::cout << " \"" << u32_to_utf8(token.lexeme) << "\"";
            }
            std::cout << std::endl;
        }
        std::cout << std::endl << std::endl;
    } catch (SLException &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    } catch (std::exception &e) {
        std::cerr << "分词器崩溃了: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "分词器崩溃了: 未知错误" << std::endl;
        return 1;
    }

    // 2. 解析器（token 数组 -> AST）
    AstNodeProgramPtr ast;
    try {
        ast = Parser{std::move(tokens), file_path}.parse_program();
        std::cout << ast->to_json().dump(2) << std::endl << std::endl;
    } catch (SLException &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    } catch (std::exception &e) {
        std::cerr << "解析器崩溃了: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "解析器崩溃了: 未知错误" << std::endl;
        return 1;
    }

    // 3. 分析器（检查 AST）
    try {
        Analyzer::analyze_program(*ast, file_path);
        std::cout << ast->to_json().dump(2) << std::endl << std::endl;
    } catch (SLException &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    } catch (std::exception &e) {
        std::cerr << "分析器崩溃了: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "分析器崩溃了: 未知错误" << std::endl;
        return 1;
    }

    return 0;
}
