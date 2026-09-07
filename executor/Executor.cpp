#include "Executor.h"

#include "../compiler/Compiler.h"
#include "../compiler/lexer/Lexer.h"
#include "../compiler/parser/ast_nodes/ast_json_dumper.h"
#include "../cppexceptions/SLException.h"
#include "../utils/string_utils.h"

#include <chrono>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

std::string path_abspath(const std::string &path) {
    std::error_code ec;
    return std::filesystem::absolute(path, ec).string();
}

// 开发期用的中间产物观察点
Compiler::Hooks make_debug_hooks() {
    const auto dump_ast{[](const AstNodeProgram &ast) {
        std::cout << AstJsonDumper::dump(ast).dump(2) << std::endl << std::endl;
    }};

    return {
        .after_tokenize =
            [](const std::vector<Token> &tokens) {
                for (const auto &token : tokens) {
                    std::cout << Lexer::get_typename_by_tokentype(token.type);
                    if (!(token.type == TokenType::NEWLINE ||
                          token.type == TokenType::END_OF_FILE)) {
                        std::cout << " \"" << u32_to_utf8(token.lexeme) << "\"";
                    }
                    std::cout << std::endl;
                }
                std::cout << std::endl << std::endl;
            },
        .after_parse = dump_ast,
        .after_analyze = dump_ast,
    };
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

    try {
        const AstNodeProgramPtr ast{Compiler::compile_file(file_path, make_debug_hooks())};
    } catch (const SLException &e) {
        // 编译器内部崩了的情况已经在门面里被包成 InternalError，这里只剩一条出口
        std::cerr << e.what() << std::endl;
        return 1;
    } catch (const std::exception &e) {
        std::cerr << "compiler crashed: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "compiler crashed: unknown error" << std::endl;
        return 1;
    }

    std::cout << "elapsed: "
              << std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count()
              << " s" << std::endl;

    return 0;
}
