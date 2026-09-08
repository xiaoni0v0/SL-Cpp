#include "Compiler.h"

#include "../cppexceptions/InternalError.h"
#include "../cppexceptions/SLException.h"
#include "../utils/file_utils.h"
#include "../utils/string_utils.h"
#include "analyzer/Analyzer.h"
#include "lexer/Lexer.h"
#include "parser/Parser.h"

#include <exception>
#include <format>
#include <utility>

namespace {

/**
 * 跑一个阶段，把它漏出来的非 SLException 包成 InternalError 并标上阶段名。
 */
template <typename Stage>
    requires std::invocable<Stage>
decltype(auto) run_stage(const char *name, Stage &&stage) {
    try {
        return std::forward<Stage>(stage)();
    } catch (const SLException &) {
        throw;
    } catch (const std::exception &e) {
        throw InternalError{std::format("{} crashed: {}", name, e.what())};
    } catch (...) {
        throw InternalError{std::format("{} crashed: unknown error", name)};
    }
}

} // namespace

std::u32string Compiler::read_source(const std::string &file_path) {
    return run_stage("source reader", [&] {
        return utf8_to_u32(file_read_all(file_path), file_path);
    });
}

std::vector<Token> Compiler::tokenize(std::u32string source, std::string file_path) {
    return run_stage("lexer", [&] {
        return Lexer{std::move(source), std::move(file_path)}.tokenize();
    });
}

AstNodeProgramPtr Compiler::parse(std::vector<Token> tokens, std::string file_path) {
    return run_stage("parser", [&] {
        return Parser{std::move(tokens), std::move(file_path)}.parse_as_file();
    });
}

AstNodeProgramPtr Compiler::parse(std::u32string source, std::string file_path) {
    std::vector tokens{tokenize(std::move(source), file_path)};

    return parse(std::move(tokens), std::move(file_path));
}

void Compiler::analyze(AstNodeProgram &ast, std::string file_path) {
    run_stage("analyzer", [&] { Analyzer::analyze_program(ast, std::move(file_path)); });
}

AstNodeProgramPtr Compiler::analyze(std::vector<Token> tokens, std::string file_path) {
    AstNodeProgramPtr ast{parse(std::move(tokens), file_path)};
    analyze(*ast, std::move(file_path));

    return ast;
}

AstNodeProgramPtr Compiler::analyze(std::u32string source, std::string file_path) {
    AstNodeProgramPtr ast{parse(std::move(source), file_path)};
    analyze(*ast, std::move(file_path));

    return ast;
}
