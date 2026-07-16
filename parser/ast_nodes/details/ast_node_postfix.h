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
    Position paren_pos_; // '(' 自己的位置

    explicit AstNodeCall(const Position pos,
                         AstNodePtr object,
                         std::vector<AstNodePtr> args,
                         std::vector<std::pair<std::u32string, AstNodePtr>> kwargs,
                         const Position paren_pos)
        : AstNode{pos}, object_{std::move(object)}, args_{std::move(args)}, kwargs_{std::move(kwargs)},
          paren_pos_{paren_pos} {
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
    Position bracket_pos_; // '[' 自己的位置

    explicit AstNodeIndex(const Position pos,
                          AstNodePtr object,
                          std::vector<AstNodePtr> args,
                          const Position bracket_pos)
        : AstNode{pos}, object_{std::move(object)}, args_{std::move(args)}, bracket_pos_{bracket_pos} {
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
    Position dot_pos_; // '.' 自己的位置

    explicit AstNodeAttr(const Position pos,
                         AstNodePtr object,
                         std::u32string attr,
                         const Position dot_pos)
        : AstNode{pos}, object_{std::move(object)}, attr_{std::move(attr)}, dot_pos_{dot_pos} {
    }

    [[nodiscard]] json to_json() const override {
        return json{{"type", "Attr"}, {"object", object_->to_json()}, {"attr", u32_to_utf8(attr_)}};
    }
};
