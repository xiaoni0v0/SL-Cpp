#pragma once

#include "ast_node.h"

#include <utility>

// ============================================================
// eval
// ============================================================

// eval(code)
struct AstNodeEval : AstNode {
    AstNodePtr code_;

    explicit AstNodeEval(const Position pos, AstNodePtr code)
        : AstNode{pos}, code_{std::move(code)} {}

    SL_AST_NODE_ACCEPT

  private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};
