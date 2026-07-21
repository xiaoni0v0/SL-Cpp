#pragma once

// 测试专用工具：解析一条顶层表达式、跑字面量折叠，转成方便比对的 JSON。

#include "../../analyzer/literal_folder/LiteralFolder.h"
#include "../parser/test_utils.h"

#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

// 解析恰好一条顶层表达式，对它跑一遍字面量折叠，返回折叠后的 JSON。
inline nlohmann::json fold_json(const std::u32string &source) {
    AstNodeProgramPtr program{parse_program(source)};
    if (program->exprs_.size() != 1) {
        throw std::runtime_error(
            "fold_json: expected exactly 1 top-level expr, got " + std::to_string(program->exprs_.size()));
    }
    LiteralFolder{*program}.fold();
    return nlohmann::json(program->exprs_[0]->to_json());
}

inline nlohmann::json int_lit(const std::string &raw) {
    return nlohmann::json{{"type", "LiteralInt"}, {"raw", raw}};
}

inline nlohmann::json float_lit(const std::string &raw) {
    return nlohmann::json{{"type", "LiteralFloat"}, {"raw", raw}};
}

inline nlohmann::json bool_lit(const bool value) {
    return nlohmann::json{{"type", "LiteralBool"}, {"value", value}};
}

inline nlohmann::json str_lit(const std::string &value) {
    return nlohmann::json{{"type", "LiteralStr"}, {"value", value}};
}

inline nlohmann::json none_lit() {
    return nlohmann::json{{"type", "LiteralNone"}};
}
