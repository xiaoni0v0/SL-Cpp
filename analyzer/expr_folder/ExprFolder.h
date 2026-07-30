#pragma once

#include "../../parser/ast_nodes/ast_nodes.h"

/**
 * 遍历 AST，折叠/精简表达式：常量折叠、死分支/死循环消除、复合表达式与 Program 的死语句剪枝
 */
class ExprFolder {
    AstNodeProgram &root_;

    /**
     * 先把 node 子节点递归处理好，再把 node 自己反复送给 StaticEvaler 折到不能再折为止
     * @param node 可空
     */
    static void visit_and_replace(AstNodePtr &node);

    /**
     * 各种 visit 的入口，按节点类型分派
     */
    static void visit(AstNode &node);

#define X(nt) static void visit(nt &node);
#include "../../parser/ast_nodes/x_ast_nodes.h"
#undef X

  public:
    /**
     * 构造 ExprFolder 对象
     * @param root AST 的根节点
     */
    explicit ExprFolder(AstNodeProgram &root);

    /**
     * 入口：折整份 Program
     */
    void fold() const &&;

    /**
     * 对单个表达式节点递归折到不能再折为止；
     * 不按 Program 语句处理，不会触发 AstNodeProgram 级别的剪枝。
     * 主要给测试用。
     * @param node 可空
     */
    static void fold_expr(AstNodePtr &node);
};
