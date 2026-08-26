#pragma once

// 测试专用工具：词法 + 语法分析，拿到 AST，并转成方便比对的形式。

#include "../../builtins/exceptions/SyntaxError.h"
#include "../../lexer/Lexer.h"
#include "../../parser/Parser.h"

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <utility>

// 词法 + 语法分析一整份源码，返回顶层 Program 节点。
// file_path 默认 "<test>"，只在需要检查报错信息里的文件名时才需要显式传。
inline AstNodeProgramPtr
parse_as_file(const std::u32string &source, const std::string &file_path = "<test>") {
    return Parser{Lexer{source}.tokenize(), file_path}.parse_as_file();
}

// 词法 + 语法分析一整份源码，要求它恰好是一条表达式（Parser::parse_as_single_expr，即 eval
// 的入口）， 返回这条表达式自己的节点。注意跟下面的 parse_single
// 不是一回事：那个走整份文件的入口再挖第一条， 这个走的是另一个入口，"多于一条"由 Parser
// 自己判而不是测试判。
inline AstNodePtr
parse_as_single_expr(const std::u32string &source, const std::string &file_path = "<test>") {
    return Parser{Lexer{source}.tokenize(), file_path}.parse_as_single_expr();
}

// 解析恰好一条顶层表达式，返回这条表达式自己的节点（多数用例只关心单条表达式解析出的树，
// 用这个可以省掉每次都要挖 Program::exprs_[0] 的样板）。
// 顶层表达式条数不是恰好 1 条时抛
// std::runtime_error（说明测试用例本身写错了，不是被测代码的问题）。
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

// 解析恰好一条顶层表达式，转成 JSON 用于结构性比对。
// 注意：AstNode::to_json() 返回的是 nlohmann::ordered_json（按插入顺序存字段），这里特意转成普通的
// nlohmann::json（按 key 比较，内部用 std::map），比较两个 json
// 对象时才不会因为“测试里字面量里字段的 书写顺序”和“to_json()
// 里实际插入顺序”不一致而误判为不相等——只关心值，不关心顺序。
inline nlohmann::json parse_json(const std::u32string &source) {
    return nlohmann::json(parse_single(source)->to_json());
}

// 同上，但保留 Program 这一层（需要检查多条顶层表达式的场景使用）。
inline nlohmann::json parse_program_json(const std::u32string &source) {
    return nlohmann::json(parse_as_file(source)->to_json());
}

// 解析整份源码（只到 Parser 这一步，不跑 SemanticChecker），要求抛出的 SyntaxError 消息里包含指定
// 子串（用于区分"确实是这条规则报的错"，不是恰好被别的规则先一步拦下来）。跟
// test/analyzer/semantic_checker/test_utils.h 里同名但语义不同的 check_throws_with（那个还会跑
// SemanticChecker） 故意区分开名字，避免两边都被包含时产生重定义。
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
