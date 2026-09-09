#include "Executor.h"

#include "../compiler/Compiler.h"
#include "../compiler/codegen/disassembler.h"
#include "../compiler/lexer/Lexer.h"
#include "../compiler/parser/ast_nodes/ast_json_dumper.h"
#include "../cppexceptions/SLException.h"
#include "../runtime/Runtime.h"
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

void dump_tokens(const std::vector<Token> &tokens) {
    for (const Token &token : tokens) {
        std::cout << Lexer::get_typename_by_tokentype(token.type);
        if (!(token.type == TokenType::NEWLINE || token.type == TokenType::END_OF_FILE)) {
            std::cout << " \"" << u32_to_utf8(token.lexeme) << "\"";
        }
        std::cout << std::endl;
    }
    std::cout << std::endl << std::endl;
}

void dump_ast(const AstNodeProgram &ast) {
    std::cout << AstJsonDumper::dump(ast).dump(2) << std::endl << std::endl;
}

void dump_code(const Code &code) { std::cout << disassemble(code) << std::endl; }

// RAII 保护 Runtime 状态正确
struct RuntimeGuard {
    RuntimeGuard() { Runtime::init(); }
    ~RuntimeGuard() { Runtime::shutdown(); }
    RuntimeGuard(const RuntimeGuard &) = delete;
    RuntimeGuard &operator=(const RuntimeGuard &) = delete;
};

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
    const RuntimeGuard runtime;

    try {
        std::vector tokens{Compiler::tokenize(Compiler::read_source(file_path), file_path)};
        dump_tokens(tokens);

        const AstNodeProgramPtr ast{Compiler::parse(std::move(tokens), file_path)};
        dump_ast(*ast);

        Compiler::analyze(*ast, file_path);
        dump_ast(*ast);

        const Ref code{Compiler::compile(*ast, file_path)};
        dump_code(*code);

    } catch (const SLException &e) {
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
