#pragma once

// 解析后跑 SemanticChecker。check_throws_with 断言消息子串，避免被别的规则先拦下来。

#include "../../../analyzer/semantic_checker/SemanticChecker.h"
#include "../../../builtins/exceptions/InternalError.h"
#include "../../../builtins/exceptions/SyntaxError.h"
#include "../../parser/test_utils.h"

#include <doctest/doctest.h>

#include <string>

inline void check_program(const std::u32string &source) {
    AstNodeProgramPtr program{parse_as_file(source)};
    SemanticChecker{*program, "<test>"}.check();
}

// eval 入口：恰好一条表达式，外层没有 Program / 循环。
inline void check_single_expr(const std::u32string &source) {
    const AstNodePtr expr{parse_as_single_expr(source)};
    SemanticChecker{*expr, "<test>"}.check();
}

// 手工 AST：测 Parser 保证过的结构被破坏时的 InternalError / SyntaxError。
inline void check_ast(AstNodeProgram &program) { SemanticChecker{program, "<test>"}.check(); }

inline void check_throws_with(const std::u32string &source, const std::string &message_substring) {
    try {
        check_program(source);
        FAIL("expected SyntaxError containing: " << message_substring);
    } catch (const SyntaxError &e) {
        const std::string what{e.what()};
        CHECK_MESSAGE(
            what.find(message_substring) != std::string::npos,
            "expected message to contain \"" << message_substring << "\", got: " << what
        );
    }
}

inline void
check_single_expr_throws_with(const std::u32string &source, const std::string &message_substring) {
    try {
        check_single_expr(source);
        FAIL("expected SyntaxError containing: " << message_substring);
    } catch (const SyntaxError &e) {
        const std::string what{e.what()};
        CHECK_MESSAGE(
            what.find(message_substring) != std::string::npos,
            "expected message to contain \"" << message_substring << "\", got: " << what
        );
    }
}

inline void check_throws_with(AstNodeProgram &program, const std::string &message_substring) {
    try {
        check_ast(program);
        FAIL("expected SyntaxError containing: " << message_substring);
    } catch (const SyntaxError &e) {
        const std::string what{e.what()};
        CHECK_MESSAGE(
            what.find(message_substring) != std::string::npos,
            "expected message to contain \"" << message_substring << "\", got: " << what
        );
    }
}

inline void
check_throws_internal_error_with(AstNodeProgram &program, const std::string &message_substring) {
    try {
        check_ast(program);
        FAIL("expected InternalError containing: " << message_substring);
    } catch (const InternalError &e) {
        const std::string what{e.what()};
        CHECK_MESSAGE(
            what.find(message_substring) != std::string::npos,
            "expected message to contain \"" << message_substring << "\", got: " << what
        );
    }
}
