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
template <typename Stage> decltype(auto) run_stage(const char *const name, Stage &&stage) {
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

AstNodeProgramPtr
Compiler::compile_source(std::u32string source, std::string file_path, const Hooks &hooks) {
    // 1. 分词器（源代码 -> token 数组）
    std::vector<Token> tokens{run_stage("lexer", [&] {
        return Lexer{std::move(source), file_path}.tokenize();
    })};
    if (hooks.after_tokenize) hooks.after_tokenize(tokens);

    // 2. 解析器（token 数组 -> AST）
    AstNodeProgramPtr ast{run_stage("parser", [&] {
        return Parser{std::move(tokens), file_path}.parse_as_file();
    })};
    if (hooks.after_parse) hooks.after_parse(*ast);

    // 3. 分析器（语义检查 + 折叠，原地改 AST）
    run_stage("analyzer", [&] { Analyzer::analyze_program(*ast, std::move(file_path)); });
    if (hooks.after_analyze) hooks.after_analyze(*ast);

    return ast;
}

AstNodeProgramPtr Compiler::compile_file(const std::string &file_path, const Hooks &hooks) {
    return compile_source(utf8_to_u32(file_read_all(file_path), file_path), file_path, hooks);
}
