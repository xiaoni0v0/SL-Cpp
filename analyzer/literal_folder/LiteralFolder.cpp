// ReSharper disable CppMemberFunctionMayBeStatic

#include "LiteralFolder.h"

#include "StaticEvaler.h"

#include <cassert>
#include <utility>

void LiteralFolder::visit_and_replace(AstNodePtr &node) const {
    if (!node) return;
    visit(node.get());
    if (AstNodePtr folded{StaticEvaler::fold(node.get())}) node = std::move(folded);
}

void LiteralFolder::visit(AstNode *node) const {
#define X(nt) if (auto *n{dynamic_cast<nt *>(node)}) { return visit(n); }
#include "../../parser/ast_nodes/x_ast_nodes.h"
#undef X

    assert(!"Unknown node type");
}

void LiteralFolder::visit(AstNodeClass *node) const {
    for (auto &deco : node->decorators_) visit_and_replace(deco);
    for (auto &base : node->bases_) visit_and_replace(base);
    for (auto &capture : node->captures_) visit_and_replace(capture.value_expr_);
    visit_and_replace(node->doc_);
    visit(node->body_.get());
}

void LiteralFolder::visit(AstNodeIf *node) const {
    for (auto &clause : node->clauses_) {
        visit_and_replace(clause.cond_);
        visit_and_replace(clause.body_);
    }
    visit_and_replace(node->else_expr_);
}

void LiteralFolder::visit(AstNodeForCond *node) const {
    visit_and_replace(node->init_);
    visit_and_replace(node->cond_);
    visit_and_replace(node->inc_);
    visit_and_replace(node->body_);
}

void LiteralFolder::visit(AstNodeForIter *node) const {
    // target_ 语法上是左值（标识符/属性/索引/解构），折不动，但递归一遍无妨（不会被误折）
    visit_and_replace(node->target_);
    visit_and_replace(node->iterable_);
    visit_and_replace(node->body_);
}

void LiteralFolder::visit(AstNodeBreak *) const {
}

void LiteralFolder::visit(AstNodeContinue *) const {
}

void LiteralFolder::visit(AstNodeReturn *node) const {
    visit_and_replace(node->value_);
}

void LiteralFolder::visit(AstNodeTry *node) const {
    visit_and_replace(node->try_expr_);
    for (auto &clause : node->except_clauses_) {
        for (auto &exc : clause.exceptions_) visit_and_replace(exc);
        visit_and_replace(clause.body_);
    }
    visit_and_replace(node->finally_expr_);
}

void LiteralFolder::visit(AstNodeRaise *node) const {
    visit_and_replace(node->value_);
}

void LiteralFolder::visit(AstNodeDecorator *node) const {
    visit_and_replace(node->decorator_);
    visit_and_replace(node->target_);
}

void LiteralFolder::visit(AstNodeFunc *node) const {
    for (auto &deco : node->decorators_) visit_and_replace(deco);
    for (auto &capture : node->captures_) visit_and_replace(capture.value_expr_);
    for (auto &param : node->params_) {
        visit_and_replace(param.type_annotation_);
        visit_and_replace(param.default_value_);
    }
    visit_and_replace(node->return_type_);
    visit_and_replace(node->doc_);
    // 函数体是 AstNodeProgramPtr，不是 AstNodePtr，本身没有"整体折成字面量"这回事，直接递归进去就行
    visit(node->body_.get());
}

void LiteralFolder::visit(AstNodeLiteralNone *) const {
}

void LiteralFolder::visit(AstNodeLiteralBool *) const {
}

void LiteralFolder::visit(AstNodeLiteralGL *) const {
}

void LiteralFolder::visit(AstNodeLiteralInt *) const {
}

void LiteralFolder::visit(AstNodeLiteralFloat *) const {
}

void LiteralFolder::visit(AstNodeLiteralStr *) const {
}

void LiteralFolder::visit(AstNodeLiteralTuple *node) const {
    for (auto &item : node->items_) visit_and_replace(item);
}

void LiteralFolder::visit(AstNodeLiteralList *node) const {
    for (auto &item : node->items_) visit_and_replace(item);
}

void LiteralFolder::visit(AstNodeLiteralDict *node) const {
    for (auto &[key, val] : node->items_) {
        visit_and_replace(key);
        if (val) visit_and_replace(val);
    }
}

void LiteralFolder::visit(AstNodeLiteralEllipsis *) const {
}

void LiteralFolder::visit(AstNodeProgram *node) const {
    for (auto &e : node->exprs_) visit_and_replace(e);
}

void LiteralFolder::visit(AstNodeCompound *node) const {
    for (auto &e : node->exprs_) visit_and_replace(e);
}

void LiteralFolder::visit(AstNodeStar *node) const {
    visit_and_replace(node->operand_);
}

void LiteralFolder::visit(AstNodeDoubleStar *node) const {
    visit_and_replace(node->operand_);
}

void LiteralFolder::visit(AstNodeOpUnary *node) const {
    visit_and_replace(node->operand_);
}

void LiteralFolder::visit(AstNodeOpBinary *node) const {
    // and/or 的短路折叠现在还没做（见 StaticEvaler::fold_and/fold_or 的注释），但两个操作数各自
    // 内部更深处能折的地方，不受这个限制，一样正常递归下去
    visit_and_replace(node->left_);
    visit_and_replace(node->right_);
}

void LiteralFolder::visit(AstNodeCompare *node) const {
    for (auto &operand : node->operands_) visit_and_replace(operand);
}

void LiteralFolder::visit(AstNodeIs *node) const {
    for (auto &operand : node->operands_) visit_and_replace(operand);
}

void LiteralFolder::visit(AstNodeAssign *node) const {
    // target_ 是左值，折不动，但递归一遍无妨（比如 a[1+2] = x 里 1+2 这部分可以折）
    visit_and_replace(node->target_);
    visit_and_replace(node->value_);
}

void LiteralFolder::visit(AstNodeCompoundAssign *node) const {
    visit_and_replace(node->target_);
    visit_and_replace(node->value_);
}

void LiteralFolder::visit(AstNodeCall *node) const {
    visit_and_replace(node->object_);
    for (auto &arg : node->args_) visit_and_replace(arg);
    for (auto &val : node->kwargs_ | std::views::values) visit_and_replace(val);
}

void LiteralFolder::visit(AstNodeIndex *node) const {
    visit_and_replace(node->object_);
    for (auto &arg : node->args_) visit_and_replace(arg);
}

void LiteralFolder::visit(AstNodeAttr *node) const {
    visit_and_replace(node->object_);
}

void LiteralFolder::visit(AstNodeIdentifier *) const {
}

void LiteralFolder::visit(AstNodeDel *node) const {
    visit_and_replace(node->target_);
}

void LiteralFolder::visit(AstNodeGlobal *) const {
}

LiteralFolder::LiteralFolder(AstNodeProgram *const root) : root_{root} {
}

void LiteralFolder::fold() const && {
    visit(root_);
}
