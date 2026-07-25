#pragma once

#include "ast_node.h"

#include <memory>
#include <utility>
#include <vector>

// 整个文件、函数体、类体
struct AstNodeProgram : AstNode {
    std::vector<AstNodePtr> exprs_;

    explicit AstNodeProgram(const Position pos, std::vector<AstNodePtr> exprs)
        : AstNode{pos}, exprs_{std::move(exprs)} {}

private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};

using AstNodeProgramPtr = std::unique_ptr<AstNodeProgram>;

// 复合表达式 { expr1; expr2; ... }，值为最后一条 expr 的值
struct AstNodeCompound : AstNode {
    std::vector<AstNodePtr> exprs_;

    explicit AstNodeCompound(const Position pos, std::vector<AstNodePtr> exprs)
        : AstNode{pos}, exprs_{std::move(exprs)} {}

private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};
