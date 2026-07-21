#pragma once

#include "../../parser/ast_nodes/ast_nodes.h"


/**
 * 遍历整棵 AST，把每一处纯字面量组合的子表达式换成折叠后的字面量节点
 * 只负责处理纯字面量组合的子表达式走到每个可能可折的位置、调 StaticEvaler、换掉，具体怎么折是 StaticEvaler 的事
 */
class LiteralFolder {
    AstNodeProgram &root_;

    // node 为空表示这个槽位本来就没有，什么都不做
    void visit_and_replace(AstNodePtr &node) const;

    // 按节点类型分派
    // 要求 node 非空
    void visit(AstNode &node) const;

#define X(nt) void visit(nt &node) const;
#include "../../parser/ast_nodes/x_ast_nodes.h"
#undef X

public:
    explicit LiteralFolder(AstNodeProgram &root);

    void fold() const &&;
};
