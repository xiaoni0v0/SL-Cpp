#pragma once

#include "ast_node.h"
#include "ast_node_capture.h"
#include "ast_node_multi_exprs.h"
#include "../../../utils/string_utils.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

// ============================================================
// 函数定义
// ============================================================

// ⟦@decorator ...⟧ func ⟦name⟧ ⟦[captures]⟧ (params) ⟦: return_type⟧ ⟦doc⟧ { body }
struct AstNodeFunc : AstNode {

    // 单个形参
    struct OneParam {
        enum class ParamType {
            Normal, StarArgs, DoubleStarKwargs
        } param_type_; // identifier / *identifier / **identifier

        std::u32string identifier_;
        AstNodePtr type_annotation_; // 类型注解，nullptr 表示无类型注解
        AstNodePtr default_value_; // 默认值，nullptr 表示无默认值
    };

    // 前缀装饰器
    std::vector<AstNodePtr> decorators_; // 可空
    std::vector<Position> decorator_positions_; // 每个装饰器自己的 '@' 位置，跟 decorators_ 一一对应
    std::optional<std::u32string> name_; // nullopt 表示匿名函数
    std::vector<OneCapture> captures_; // 可空
    std::vector<OneParam> params_; // 可空
    AstNodePtr return_type_; // nullptr 表示无 ": type"
    AstNodePtr doc_; // nullptr 表示无文档字符串
    AstNodeProgramPtr body_;

    explicit AstNodeFunc(const Position pos,
                         std::vector<AstNodePtr> decorators,
                         std::vector<Position> decorator_positions,
                         std::optional<std::u32string> name,
                         std::vector<OneCapture> captures,
                         std::vector<OneParam> params,
                         AstNodePtr return_type,
                         AstNodePtr doc,
                         AstNodeProgramPtr body)
        : AstNode{pos}, decorators_{std::move(decorators)}, decorator_positions_{std::move(decorator_positions)},
          name_{std::move(name)}, captures_{std::move(captures)}, params_{std::move(params)},
          return_type_{std::move(return_type)}, doc_{std::move(doc)}, body_{std::move(body)} {
    }

    [[nodiscard]] json to_json() const override {
        auto decorators = json::array();
        for (const auto &d : decorators_) decorators.push_back(d->to_json());

        auto params = json::array();
        for (const auto &p : params_)
            params.push_back({
                {"identifier", u32_to_utf8(p.identifier_)},
                {"param_type", param_type_str(p.param_type_)},
                {"type_annotation", p.type_annotation_ ? p.type_annotation_->to_json() : json(nullptr)},
                {"default_value", p.default_value_ ? p.default_value_->to_json() : json(nullptr)}
            });

        return json{
            {"type", "Func"},
            {"decorators", std::move(decorators)},
            {"name", name_ ? json(u32_to_utf8(*name_)) : json(nullptr)},
            {"captures", captures_to_json(captures_)},
            {"params", std::move(params)},
            {"return_type", return_type_ ? return_type_->to_json() : json(nullptr)},
            {"doc", doc_ ? doc_->to_json() : json(nullptr)},
            {"body", body_->to_json()}
        };
    }

    [[nodiscard]] static constexpr const char *param_type_str(const OneParam::ParamType pt) {
        switch (pt) {
        case OneParam::ParamType::Normal: return "Normal";
        case OneParam::ParamType::StarArgs: return "StarArgs";
        case OneParam::ParamType::DoubleStarKwargs: return "DoubleStarKwargs";
        default: return "<unknown>";
        }
    }
};
