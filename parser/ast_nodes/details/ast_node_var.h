#pragma once

#include "ast_node.h"

#include <string>
#include <utility>


// 标识符
struct AstNodeIdentifier : AstNode {
    std::u32string identifier_;

    AstNodeIdentifier(const int row, const int col,
                      std::u32string identifier)
        : AstNode{row, col}, identifier_{std::move(identifier)} {
    }
};

// del target（target 必须是标识符，由语义层校验）
struct AstNodeDel : AstNode {
    AstNodePtr target_;

    AstNodeDel(const int row, const int col,
               AstNodePtr target)
        : AstNode{row, col}, target_{std::move(target)} {
    }
};

// global target（target 必须是标识符，由语义层校验）
struct AstNodeGlobal : AstNode {
    AstNodePtr target_;

    AstNodeGlobal(const int row, const int col,
                  AstNodePtr target)
        : AstNode{row, col}, target_{std::move(target)} {
    }
};
