#pragma once

#include "../../parser/ast_nodes/ast_nodes.h"

/**
 * 遍历 AST，字面量折叠
 */
class LiteralFolder {
    AstNodeProgram &root_;

    /**
     * 先把 node 子节点递归处理好，再把 node 自己反复送给 StaticEvaler 折到不能再折为止
     * @param node 可空
     */
    void visit_and_replace(AstNodePtr &node) const;

    /**
     * 各种 visit 的入口，按节点类型分派
     */
    void visit(AstNode &node) const;

#define X(nt) void visit(nt &node) const;
#include "../../parser/ast_nodes/x_ast_nodes.h"
#undef X

  public:
    /**
     * 构造 LiteralFolder 对象
     * @param root AST 的根节点
     */
    explicit LiteralFolder(AstNodeProgram &root);

    /**
     * 入口
     */
    void fold() const &&;
};
