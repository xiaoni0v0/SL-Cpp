#pragma once

#include "ast_node.h"

#include <memory>
#include <utility>
#include <vector>


// 整个文件、函数体，值为 return 后的值
struct AstNodeProgram : AstNode {
    std::vector<AstNodePtr> exprs_;

    AstNodeProgram(const int row, const int col,
                   std::vector<AstNodePtr> exprs)
        : AstNode{row, col}, exprs_{std::move(exprs)} {
    }
};

using AstNodeProgramPtr = std::unique_ptr<AstNodeProgram>;

// 复合表达式 { expr1; expr2; ... }，值为最后一条 expr 的值
struct AstNodeCompound : AstNode {
    std::vector<AstNodePtr> exprs_;

    AstNodeCompound(const int row, const int col,
                    std::vector<AstNodePtr> exprs)
        : AstNode{row, col}, exprs_{std::move(exprs)} {
    }
};
