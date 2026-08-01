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

  private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};

// 调用形态的 import(name, lazy=..., force=...)
struct AstNodeImportCall : AstNode {
    std::vector<AstNodePtr> positional_args_; // 位置组：位置实参、*expr 展开，按书写顺序
    std::vector<OneKwArg> keyword_args_;      // 关键字组：关键字实参、**expr 展开，按书写顺序
    Position paren_pos_;                      // '(' 自己的位置

    explicit AstNodeImportCall(
        const Position pos, std::vector<AstNodePtr> positional_args,
        std::vector<OneKwArg> keyword_args, const Position paren_pos
    )
        : AstNode{pos}, positional_args_{std::move(positional_args)},
          keyword_args_{std::move(keyword_args)}, paren_pos_{paren_pos} {}

  private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};
