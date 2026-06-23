#pragma once

#include "ast_node.h"

#include <utility>

// ============================================================
// 装饰器
// ============================================================

// @decorator expr
struct AstNodeDecorator : AstNode {
    AstNodePtr decorator_;
    AstNodePtr target_; // 被装饰的 AstNodeFunc 或 AstNodeClass

    AstNodeDecorator(const int row, const int col,
                     AstNodePtr decorator,
                     AstNodePtr target)
        : AstNode{row, col}, decorator_{std::move(decorator)}, target_{std::move(target)} {
    }
};
