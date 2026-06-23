#pragma once

#include <memory>


// AST 节点的基类
struct AstNode {
    // 开始的行和列
    int row_;
    int col_;

    AstNode(const int row, const int col) : row_{row}, col_{col} {
    }

    virtual ~AstNode() = default;
};

using AstNodePtr = std::unique_ptr<AstNode>;
