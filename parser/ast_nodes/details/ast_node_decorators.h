#pragma once

#include "ast_node.h"

#include <utility>

// ============================================================
// 装饰器
// ============================================================

// @decorator expr（通用形式，2.2.8）
struct AstNodeDecorator : AstNode {
    AstNodePtr decorator_;
    AstNodePtr target_;

    AstNodeDecorator(const int row, const int col,
                     AstNodePtr decorator,
                     AstNodePtr target)
        : AstNode{row, col}, decorator_{std::move(decorator)}, target_{std::move(target)} {
    }

    [[nodiscard]] json to_json() const override {
        json j{{"type", "Decorator"}};
        j["decorator"] = decorator_->to_json();
        j["target"] = target_->to_json();
        return j;
    }
};
