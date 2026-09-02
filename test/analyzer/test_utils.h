#pragma once

// 解析后跑 ExprFolder，转成按 key 比较的 json。
// 存返回值必须用 `=`，不能用 `{}`。见 .ai/notes/json-test-brace-init-trap.md。

#include "../../analyzer/expr_folder/ExprFolder.h"
#include "../parser/test_utils.h"

#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

// 只折这一条表达式，不走 Program 剪枝（纯字面量在 Program 里会被丢掉）。
inline nlohmann::json fold_json(const std::u32string &source) {
    AstNodeProgramPtr program{parse_as_file(source)};
    if (program->exprs_.size() != 1) {
        throw std::runtime_error(
            "fold_json: expected exactly 1 top-level expr, got " +
            std::to_string(program->exprs_.size())
        );
    }
    ExprFolder::fold_single_expr(program->exprs_[0]);
    return nlohmann::json(AstJsonDumper::dump(*program->exprs_[0]));
}

inline nlohmann::json fold_program_json(const std::u32string &source) {
    AstNodeProgramPtr program{parse_as_file(source)};
    ExprFolder::fold_program(*program);
    return nlohmann::json(AstJsonDumper::dump(*program));
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
