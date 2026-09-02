#pragma once

#include "ast_node.h"
#include "ast_node_misc.h"

#include <utility>

// ============================================================
// eval
// ============================================================

struct AstNodeEval : AstNode {
    CallArgs args_;

    explicit AstNodeEval(const Position pos, CallArgs args)
        : AstNode{pos}, args_{std::move(args)} {}

    SL_AST_NODE_ACCEPT
};
