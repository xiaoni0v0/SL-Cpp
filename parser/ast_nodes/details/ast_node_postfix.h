#pragma once

#include "ast_node.h"
#include "../../../utils/string_utils.h"

#include <string>
#include <utility>
#include <vector>

// ============================================================
// 访问表达式
// ============================================================

// f(arg1, arg2, kw=val, ...)
struct AstNodeCall : AstNode {
    AstNodePtr object_;
    std::vector<AstNodePtr> args_;
    std::vector<std::pair<std::u32string, AstNodePtr>> kwargs_;

    AstNodeCall(const int row, const int col,
                AstNodePtr object,
                std::vector<AstNodePtr> args,
                std::vector<std::pair<std::u32string, AstNodePtr>> kwargs)
        : AstNode{row, col}, object_{std::move(object)}, args_{std::move(args)}, kwargs_{std::move(kwargs)} {
    }

    [[nodiscard]] json to_json() const override {
        auto args = json::array();
        for (const auto &arg : args_) args.push_back(arg->to_json());
        auto kwargs = json::array();
        for (const auto &[key, val] : kwargs_) kwargs.push_back({{"key", u32_to_utf8(key)}, {"value", val->to_json()}});
        return json{
            {"type", "Call"}, {"object", object_->to_json()},
            {"args", std::move(args)}, {"kwargs", std::move(kwargs)}
        };
    }
};

// x[i]  x[i, j, ...]
struct AstNodeIndex : AstNode {
    AstNodePtr object_;
    std::vector<AstNodePtr> args_;

    AstNodeIndex(const int row, const int col,
                 AstNodePtr object,
                 std::vector<AstNodePtr> args)
        : AstNode{row, col}, object_{std::move(object)}, args_{std::move(args)} {
    }

    [[nodiscard]] json to_json() const override {
        auto args = json::array();
        for (const auto &arg : args_) args.push_back(arg->to_json());
        return json{{"type", "Index"}, {"object", object_->to_json()}, {"args", std::move(args)}};
    }
};

// x.attr
struct AstNodeAttr : AstNode {
    AstNodePtr object_;
    std::u32string attr_;
    // '.' 自己的位置
    int dot_row_, dot_col_;

    AstNodeAttr(const int row, const int col,
                AstNodePtr object,
                std::u32string attr,
                const int dot_row, const int dot_col)
        : AstNode{row, col}, object_{std::move(object)}, attr_{std::move(attr)},
          dot_row_{dot_row}, dot_col_{dot_col} {
    }

    [[nodiscard]] json to_json() const override {
        return json{{"type", "Attr"}, {"object", object_->to_json()}, {"attr", u32_to_utf8(attr_)}};
    }
};
