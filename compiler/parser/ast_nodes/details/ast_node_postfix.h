#pragma once

#include "ast_node.h"
#include "ast_node_misc.h"

#include <string>
#include <utility>
#include <vector>

// ============================================================
// 后缀表达式
// ============================================================

// f(arg1, arg2, kw=val, ...)
struct AstNodeCall : AstNode {
    AstNodePtr object_;
    CallArgs args_;

    explicit AstNodeCall(const Position pos, AstNodePtr object, CallArgs args)
        : AstNode{pos}, object_{std::move(object)}, args_{std::move(args)} {}

    SL_AST_NODE_ACCEPT
};

// x[i]  x[i, j, ...]
struct AstNodeIndex : AstNode {
    AstNodePtr object_;
    std::vector<AstNodePtr> args_;
    Position pos_bracket_; // '[' 自己的位置

    explicit AstNodeIndex(
        const Position pos, AstNodePtr object, std::vector<AstNodePtr> args,
        const Position pos_bracket
    )
        : AstNode{pos}, object_{std::move(object)}, args_{std::move(args)},
          pos_bracket_{pos_bracket} {}

    SL_AST_NODE_ACCEPT
};

// x.attr
struct AstNodeAttr : AstNode {
    AstNodePtr object_;
    std::u32string attr_;
    Position pos_dot_; // '.' 自己的位置

    explicit AstNodeAttr(
        const Position pos, AstNodePtr object, std::u32string attr, const Position pos_dot
    )
        : AstNode{pos}, object_{std::move(object)}, attr_{std::move(attr)}, pos_dot_{pos_dot} {}

    SL_AST_NODE_ACCEPT
};
