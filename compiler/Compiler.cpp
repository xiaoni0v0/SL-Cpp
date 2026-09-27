#include "Compiler.h"

#include "../cppexceptions/InternalError.h"
#include "../cppexceptions/SLException.h"
#include "../runtime/Object.h"
#include "../runtime/objects/Code.h"
#include "../utils/file_utils.h"
#include "../utils/string_utils.h"
#include "analyzer/Analyzer.h"
#include "codegen/CodeGen.h"
#include "lexer/Lexer.h"
#include "lexer/token.h"
#include "parser/Parser.h"
#include "parser/ast_nodes/ast_nodes.h"

#include <concepts>
#include <exception>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

/**
 * 跑一个阶段，把它漏出来的非 SLException 包成 InternalError 并标上阶段名。
 */
template <typename Stage>
    requires std::invocable<Stage>
decltype(auto) run_stage(const std::string_view name, Stage &&stage) {
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
        return Lexer::tokenize(std::move(source), std::move(file_path));
    });
}

AstNodeProgramPtr Compiler::parse(std::vector<Token> tokens, std::string file_path) {
    return run_stage("parser", [&] {
        return Parser::parse_program(std::move(tokens), std::move(file_path));
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

Ref<Code> Compiler::compile(AstNodeProgram &ast, std::string file_path) {
    analyze(ast, file_path);

    return run_stage("code generator", [&] {
        return CodeGen::generate_program(ast, std::move(file_path));
    });
}

Ref<Code> Compiler::compile(std::vector<Token> tokens, std::string file_path) {
    const AstNodeProgramPtr ast{parse(std::move(tokens), file_path)};

    return compile(*ast, std::move(file_path));
}

Ref<Code> Compiler::compile(std::u32string source, std::string file_path) {
    const AstNodeProgramPtr ast{parse(std::move(source), file_path)};

    return compile(*ast, std::move(file_path));
}
