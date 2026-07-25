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

    explicit AstNodeDecorator(const Position pos, AstNodePtr decorator, AstNodePtr target)
        : AstNode{pos}, decorator_{std::move(decorator)}, target_{std::move(target)} {}

    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};
