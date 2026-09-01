#pragma once

// 词法 + 语法分析，把 AST 转成按 key 比较的 json，方便断言。
// 把 parse_json / parse_program_json 的返回值存进局部变量时必须用 `=`，不能用 `{}`，
// 否则 nlohmann 会走 initializer_list 构造函数，对象被包成单元素数组。见
// .ai/notes/json-test-brace-init-trap.md。

#include "../../builtins/exceptions/SyntaxError.h"
#include "../../lexer/Lexer.h"
#include "../../parser/Parser.h"

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <utility>

inline AstNodeProgramPtr
parse_as_file(const std::u32string &source, const std::string &file_path = "<test>") {
    return Parser{Lexer{source}.tokenize(), file_path}.parse_as_file();
}

// eval 入口：整份输入必须恰好一条表达式，条数由 Parser 自己判。
inline AstNodePtr
parse_as_single_expr(const std::u32string &source, const std::string &file_path = "<test>") {
    return Parser{Lexer{source}.tokenize(), file_path}.parse_as_single_expr();
}

// 走文件入口再取出唯一一条顶层表达式。条数不对说明用例写错了，抛 runtime_error。
inline AstNodePtr parse_single(const std::u32string &source) {
    AstNodeProgramPtr program{parse_as_file(source)};
    if (program->exprs_.size() != 1) {
        throw std::runtime_error(
            "parse_single: expected exactly 1 top-level expr, got " +
            std::to_string(program->exprs_.size())
        );
    }
    return std::move(program->exprs_[0]);
}

inline nlohmann::json parse_json(const std::u32string &source) {
    return nlohmann::json(parse_single(source)->to_json());
}

inline nlohmann::json parse_program_json(const std::u32string &source) {
    return nlohmann::json(parse_as_file(source)->to_json());
}

inline void check_parse_as_single_expr_throws_with(
    const std::u32string &source, const std::string &message_substring
) {
    try {
        parse_as_single_expr(source);
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
check_parse_throws_with(const std::u32string &source, const std::string &message_substring) {
    try {
        parse_as_file(source);
        FAIL("expected SyntaxError containing: " << message_substring);
    } catch (const SyntaxError &e) {
        const std::string what{e.what()};
        CHECK_MESSAGE(
            what.find(message_substring) != std::string::npos,
            "expected message to contain \"" << message_substring << "\", got: " << what
        );
    }
}

inline nlohmann::json ident(const char *name) {
    return nlohmann::json{{"type", "Identifier"}, {"identifier", name}};
}

inline nlohmann::json int_lit(const char *raw) {
    return nlohmann::json{{"type", "LiteralInt"}, {"raw", raw}};
}

inline nlohmann::json str_lit(const char *value) {
    return nlohmann::json{{"type", "LiteralStr"}, {"value", value}};
}
