#pragma once

#include "ast_node.h"
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

    // 单个捕获项：identifier（值捕获，读当前值） / identifier = expr（值捕获，读 expr） / &identifier（引用捕获）
    struct OneCapture {
        enum class CaptureType { Value, Reference } capture_type_;

        std::u32string identifier_;
        AstNodePtr value_expr_; // 仅 CaptureType::Value 且显式写了 "= expr" 时非空；裸标识符或引用捕获均为 nullptr
    };

    // 前缀装饰器
    std::vector<AstNodePtr> decorators_; // 可空
    std::optional<std::u32string> name_; // nullopt 表示匿名函数
    std::vector<OneCapture> captures_; // 可空
    std::vector<OneParam> params_; // 可空
    AstNodePtr return_type_; // nullptr 表示无 ": type"
    AstNodePtr doc_; // nullptr 表示无文档字符串
    AstNodeProgramPtr body_;

    AstNodeFunc(const int row, const int col,
                std::vector<AstNodePtr> decorators,
                std::optional<std::u32string> name,
                std::vector<OneCapture> captures,
                std::vector<OneParam> params,
                AstNodePtr return_type,
                AstNodePtr doc,
                AstNodeProgramPtr body)
        : AstNode{row, col}, decorators_{std::move(decorators)}, name_{std::move(name)},
          captures_{std::move(captures)}, params_{std::move(params)}, return_type_{std::move(return_type)},
          doc_{std::move(doc)}, body_{std::move(body)} {
    }

    [[nodiscard]] json to_json() const override {
        json j{{"type", "Func"}};
        auto decorators = json::array();
        for (const auto &d : decorators_) decorators.push_back(d->to_json());
        j["decorators"] = std::move(decorators);
        j["name"] = name_ ? json(u32_to_utf8(*name_)) : json(nullptr);

        auto captures = json::array();
        for (const auto &c : captures_)
            captures.push_back({
                {"kind", c.capture_type_ == OneCapture::CaptureType::Value ? "Value" : "Reference"},
                {"identifier", u32_to_utf8(c.identifier_)},
                {"value_expr", c.value_expr_ ? c.value_expr_->to_json() : json(nullptr)}
            });
        j["captures"] = std::move(captures);

        auto params = json::array();
        for (const auto &p : params_) {
            const char *pt_str = p.param_type_ == OneParam::ParamType::Normal
                                     ? "Normal"
                                     : p.param_type_ == OneParam::ParamType::StarArgs
                                     ? "StarArgs"
                                     : "DoubleStarKwargs";
            params.push_back({
                {"identifier", u32_to_utf8(p.identifier_)},
                {"param_type", pt_str},
                {"type_annotation", p.type_annotation_ ? p.type_annotation_->to_json() : json(nullptr)},
                {"default_value", p.default_value_ ? p.default_value_->to_json() : json(nullptr)}
            });
        }
        j["params"] = std::move(params);

        j["return_type"] = return_type_ ? return_type_->to_json() : json(nullptr);
        j["doc"] = doc_ ? doc_->to_json() : json(nullptr);
        j["body"] = body_->to_json();
        return j;
    }
};
