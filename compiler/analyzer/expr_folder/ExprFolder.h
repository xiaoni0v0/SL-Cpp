#pragma once

#include "../../parser/ast_nodes/ast_nodes.h"

/**
 * 遍历 AST，折叠/精简表达式：常量折叠、死分支/死循环消除、复合表达式与 Program 的死语句剪枝
 */
class ExprFolder : public AstVisitor {
    explicit ExprFolder() = default;

    /**
     * 先把 node 子节点递归处理好，再把 node 自己反复送给 StaticEvaler 折到不能再折为止
     * @param node 可空
     */
    void visit_and_replace(AstNodePtr &node);

    void visit(AstNode &node);

#define X(nt) void visit(nt &node) override;
#include "../../parser/ast_nodes/x_ast_nodes.inc"

#undef X

    // 折一份 CallArgs 里的每个实参
    void fold_call_args(CallArgs &args);

  public:
    /**
     * 折整份 Program：除了逐条折叠，还会做 Program 级别的死语句剪枝
     * @param root Program 根节点，原地修改
     */
    static void fold_program(AstNodeProgram &root);

    /**
     * 对单个表达式节点递归折到不能再折为止；
     * 不按 Program 语句处理，不会触发 AstNodeProgram 级别的剪枝。
     * @param node 可空
     */
    static void fold_single_expr(AstNodePtr &node);
};
