#pragma once

// 测试专用工具：解析一份源码并跑一遍 SyntaxChecker。

#include "../../../analyzer/syntax_checker/SyntaxChecker.h"
#include "../../../builtins/classes/exceptions/SyntaxError.h"
#include "../../parser/test_utils.h"

#include <doctest/doctest.h>

#include <string>

// 解析整份源码并跑一遍 SyntaxChecker；不抛异常就是通过，调用方一般配 CHECK_NOTHROW/CHECK_THROWS_AS 用。
inline void check_program(const std::u32string &source) {
    AstNodeProgramPtr program{parse_program(source)};
    SyntaxChecker{*program, "<test>"}.check();
}

// 直接对一棵手工搭出来的 AST 跑 SyntaxChecker——用于测试那些"只有 Parser 出 bug 才会触发"的防御性
// 断言（比如 AstNodeIf::clauses_ 为空、AstNodeCompare 的 operands_/ops_ 数量对不上），
// 这类畸形的树没法通过正常解析源码构造出来，只能手工拼。
inline void check_ast(AstNodeProgram &program) {
    SyntaxChecker{program, "<test>"}.check();
}

// 同上，但额外要求抛出的 SyntaxError 消息里包含指定子串（用于区分"确实是这条规则报的错"，
// 不是恰好被别的规则先一步拦下来）。
inline void check_throws_with(const std::u32string &source, const std::string &message_substring) {
    try {
        check_program(source);
        FAIL("expected SyntaxError containing: " << message_substring);
    } catch (const SyntaxError &e) {
        const std::string what{e.what()};
        CHECK_MESSAGE(what.find(message_substring) != std::string::npos,
                      "expected message to contain \"" << message_substring << "\", got: " << what);
    }
}

// 同上，但直接对一棵手工搭出来的 AST 跑（配 check_ast 用）。
inline void check_throws_with(AstNodeProgram &program, const std::string &message_substring) {
    try {
        check_ast(program);
        FAIL("expected SyntaxError containing: " << message_substring);
    } catch (const SyntaxError &e) {
        const std::string what{e.what()};
        CHECK_MESSAGE(what.find(message_substring) != std::string::npos,
                      "expected message to contain \"" << message_substring << "\", got: " << what);
    }
}
