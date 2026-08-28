#pragma once

// ============================================================
// AST 的访问者，要遍历 AST 的类继承它
// ============================================================

// 前向声明
#define X(nt) struct nt;
#include "x_ast_nodes.inc"

#undef X

// 会改树的访问者
struct AstVisitor {
    AstVisitor() = default;
    virtual ~AstVisitor() = default;
    AstVisitor(const AstVisitor &) = default;
    AstVisitor(AstVisitor &&) = default;
    AstVisitor &operator=(const AstVisitor &) = default;
    AstVisitor &operator=(AstVisitor &&) = default;

#define X(nt) virtual void visit(nt &node) = 0;
#include "x_ast_nodes.inc"

#undef X
};

// 只读的访问者
struct AstConstVisitor {
    AstConstVisitor() = default;
    virtual ~AstConstVisitor() = default;
    AstConstVisitor(const AstConstVisitor &) = default;
    AstConstVisitor(AstConstVisitor &&) = default;
    AstConstVisitor &operator=(const AstConstVisitor &) = default;
    AstConstVisitor &operator=(AstConstVisitor &&) = default;

#define X(nt) virtual void visit(const nt &node) = 0;
#include "x_ast_nodes.inc"

#undef X
};

#define SL_AST_NODE_ACCEPT                                                                         \
    void accept(AstVisitor &visitor) override { visitor.visit(*this); }                            \
    void accept(AstConstVisitor &visitor) const override { visitor.visit(*this); }
