#include "Executor.h"

#include "../compiler/analyzer/Analyzer.h"
#include "../compiler/lexer/Lexer.h"
#include "../compiler/parser/Parser.h"
#include "../compiler/parser/ast_nodes/ast_json_dumper.h"
#include "../compiler/parser/ast_nodes/details/ast_node_multi_exprs.h"
#include "../cppexceptions/SLException.h"
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

Executor::Executor(const int argc, const char *argv[]) : argv_{argv, argv + argc} {}

int Executor::run() const {
    const auto t0 = std::chrono::steady_clock::now();

    // 没有输入文件
    if (argv_.size() != 2) {
        std::cerr << "Usage: " << std::filesystem::path{argv_[0]}.filename().string()
                  << " <input_file>" << std::endl;
        return 1;
    }

    const std::string file_path{path_abspath(argv_[1])};

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
        std::cerr << "lexer crashed: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "lexer crashed: unknown error" << std::endl;
        return 1;
    }

    // 2. 解析器（token 数组 -> AST）
    AstNodeProgramPtr ast;
    try {
        ast = Parser{std::move(tokens), file_path}.parse_as_file();
        std::cout << AstJsonDumper::dump(*ast).dump(2) << std::endl << std::endl;
    } catch (SLException &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    } catch (std::exception &e) {
        std::cerr << "parser crashed: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "parser crashed: unknown error" << std::endl;
        return 1;
    }

    // 3. 分析器（检查 AST）
    try {
        Analyzer::analyze_program(*ast, file_path);
        std::cout << AstJsonDumper::dump(*ast).dump(2) << std::endl << std::endl;
    } catch (SLException &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    } catch (std::exception &e) {
        std::cerr << "analyzer crashed: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "analyzer crashed: unknown error" << std::endl;
        return 1;
    }

    std::cout << "elapsed: "
              << std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count()
              << " s" << std::endl;

    return 0;
}
