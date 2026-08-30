#pragma once

// 测试专用工具：解析一条顶层表达式、跑表达式折叠，转成方便比对的 JSON。

#include "../../analyzer/expr_folder/ExprFolder.h"
#include "../parser/test_utils.h"

#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

// 解析恰好一条顶层表达式，对它单独跑一遍表达式折叠（ExprFolder::fold_expr，不是
// ExprFolder{...}.fold()），返回折叠后的 JSON。故意不走整份 Program 的折叠入口：
// AstNodeProgram 级别还会做 StaticEvaler::prune_program 那步剪枝（哪怕只有一条、折成纯字面量
// 也会被剪掉，因为 Program 的值只看 return（见 SL.md 程序与函数体的值由 return
// 决定），不看最后一条表达式的值）， 这里只关心"这一条表达式本身折成了什么"，不想被剪掉。
inline nlohmann::json fold_json(const std::u32string &source) {
    AstNodeProgramPtr program{parse_as_file(source)};
    if (program->exprs_.size() != 1) {
        throw std::runtime_error(
            "fold_json: expected exactly 1 top-level expr, got " +
            std::to_string(program->exprs_.size())
        );
    }
    ExprFolder::fold_single_expr(program->exprs_[0]);
    return nlohmann::json(program->exprs_[0]->to_json());
}

// 解析整份源码、跑一遍表达式折叠，返回折叠后整个 Program 节点（含 exprs_）的 JSON。
// fold_json 只看恰好一条顶层表达式折出来的样子；这个用来测多条顶层表达式之间的折叠交互
// （比如 AstNodeProgram::prune_program 原地精简 exprs_）。
inline nlohmann::json fold_program_json(const std::u32string &source) {
    AstNodeProgramPtr program{parse_as_file(source)};
    ExprFolder::fold_program(*program);
    return nlohmann::json(program->to_json());
}

inline nlohmann::json int_lit(const std::string &raw) {
    return nlohmann::json{{"type", "LiteralInt"}, {"raw", raw}};
}

inline nlohmann::json decimal_lit(const std::string &raw) {
    return nlohmann::json{{"type", "LiteralDecimal"}, {"raw", raw}};
}

inline nlohmann::json bool_lit(const bool value) {
    return nlohmann::json{{"type", "LiteralBool"}, {"value", value}};
}

inline nlohmann::json str_lit(const std::string &value) {
    return nlohmann::json{{"type", "LiteralStr"}, {"value", value}};
}

inline nlohmann::json none_lit() { return nlohmann::json{{"type", "LiteralNone"}}; }
