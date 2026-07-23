#pragma once

#include "ast_node.h"
#include "../../../utils/string_utils.h"

#include <string>
#include <utility>
#include <vector>

// ============================================================
// 后缀表达式
// ============================================================

// f(arg1, arg2, kw=val, ...)
struct AstNodeCall : AstNode {
    // 关键字组的一项：identifier = expr（Keyword）或 **expr（DoubleStar）
    struct OneKwArg {
        enum class Kind { Keyword, DoubleStar } kind_;

        std::u32string keyword_; // 仅 Kind::Keyword 时有意义
        AstNodePtr value_;
    };

    AstNodePtr object_;
    std::vector<AstNodePtr> positional_args_; // 位置组：位置实参、*expr 展开，按书写顺序
    std::vector<OneKwArg> keyword_args_; // 关键字组：关键字实参、**expr 展开，按书写顺序
    Position paren_pos_; // '(' 自己的位置

    explicit AstNodeCall(const Position pos,
                         AstNodePtr object,
                         std::vector<AstNodePtr> positional_args,
                         std::vector<OneKwArg> keyword_args,
                         const Position paren_pos)
        : AstNode{pos}, object_{std::move(object)}, positional_args_{std::move(positional_args)},
          keyword_args_{std::move(keyword_args)}, paren_pos_{paren_pos} {
    }

    [[nodiscard]] json to_json() const override {
        auto args = json::array();
        for (const auto &arg : positional_args_) args.push_back(arg->to_json());
        auto kwargs = json::array();
        for (const auto &kw : keyword_args_)
            kwargs.push_back({
                {"key", kw.kind_ == OneKwArg::Kind::Keyword ? json(u32_to_utf8(kw.keyword_)) : json(nullptr)},
                {"value", kw.value_->to_json()}
            });
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
