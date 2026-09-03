#include "Executor.h"

#include "../compiler/analyzer/Analyzer.h"
#include "../compiler/lexer/Lexer.h"
#include "../compiler/parser/Parser.h"
#include "../compiler/parser/ast_nodes/ast_json_dumper.h"
#include "../compiler/parser/ast_nodes/details/ast_node_multi_exprs.h"
#include "../cpp_exceptions/SLException.h"
#include "../utils/file_utils.h"
#include "../utils/string_utils.h"

#include <filesystem>
#include <iostream>
#include <string>

namespace {

std::string path_abspath(const std::string &path) {
    std::error_code ec;
    return std::filesystem::absolute(path, ec).string();
}

} // namespace

Executor::Executor(const std::string &s) : file_path_{path_abspath(s)} {}

int Executor::run() const {
    const auto t0 = std::chrono::steady_clock::now();

    // 输入文件不存在
    if (!std::filesystem::exists(file_path_)) {
        std::cerr << "Source code file \"" << file_path_ << "\" does not exist." << std::endl;
        return 1;
    }

    // 1. 分词器（源代码 -> token 数组）
    std::vector<Token> tokens;
    try {
        tokens = Lexer{utf8_to_u32(file_read_all(file_path_), file_path_), file_path_}.tokenize();
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
        ast = Parser{std::move(tokens), file_path_}.parse_as_file();
        std::cout << AstJsonDumper::dump(*ast).dump(2) << std::endl << std::endl;
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
        Analyzer::analyze_program(*ast, file_path_);
        std::cout << AstJsonDumper::dump(*ast).dump(2) << std::endl << std::endl;
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

    std::cout << "耗时: "
              << std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count()
              << " 秒" << std::endl;

    return 0;
}
