#pragma once

#include "ast_node.h"
#include "ast_node_misc.h"

#include <string>
#include <utility>
#include <vector>

// ============================================================
// 导入
// ============================================================

// 关键字形态的 import a  /  import a.b.c ...
struct AstNodeImportKw : AstNode {
    // 按 '.' 拆开的各段名字，至少一段
    std::vector<std::u32string> segments_;

    explicit AstNodeImportKw(const Position pos, std::vector<std::u32string> segments)
        : AstNode{pos}, segments_{std::move(segments)} {}

    SL_AST_NODE_ACCEPT

  private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};

// 调用形态的 import(name, lazy=..., force=...)
struct AstNodeImportCall : AstNode {
    CallArgs args_;

    explicit AstNodeImportCall(const Position pos, CallArgs args)
        : AstNode{pos}, args_{std::move(args)} {}

    SL_AST_NODE_ACCEPT

  private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};
