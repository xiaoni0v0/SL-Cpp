#pragma once

#include "../ast_visitor.h"

#include <compare>
#include <memory>

// 源码里的一个位置（行、列）
struct Position {
    int row;
    int col;
    std::strong_ordering operator<=>(const Position &) const = default;
};

// AST 节点的基类
struct AstNode {
    // 开始的位置
    Position pos_;

    explicit AstNode(const Position pos) : pos_{pos} {}

    // 多态基类，AST 是 unique_ptr 独占树、节点从不被拷贝或移动，删掉以防切片
    AstNode(const AstNode &) = delete;
    AstNode(AstNode &&) = delete;
    AstNode &operator=(const AstNode &) = delete;
    AstNode &operator=(AstNode &&) = delete;
    virtual ~AstNode() = default;

    // 双分派的前半程
    virtual void accept(AstVisitor &visitor) = 0;
    virtual void accept(AstConstVisitor &visitor) const = 0;
};

using AstNodePtr = std::unique_ptr<AstNode>;
