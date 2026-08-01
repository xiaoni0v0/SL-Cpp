#pragma once

#include "ast_node.h"

#include <string>
#include <utility>
#include <vector>

// ============================================================
// 导入
// ============================================================

// 关键字形态的 import a  /  import a.b.c ...
struct AstNodeImport : AstNode {
    // 按 '.' 拆开的各段名字，至少一段
    std::vector<std::u32string> segments_;

    explicit AstNodeImport(const Position pos, std::vector<std::u32string> segments)
        : AstNode{pos}, segments_{std::move(segments)} {}

  private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};
