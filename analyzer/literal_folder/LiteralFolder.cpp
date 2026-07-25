#include "LiteralFolder.h"

#include "StaticEvaler.h"

#include <cassert>
#include <utility>

void LiteralFolder::visit_and_replace(AstNodePtr &node) const {
    if (!node) return;
    visit(*node);
    if (AstNodePtr folded{StaticEvaler::fold(*node)}) node = std::move(folded);
}

void LiteralFolder::visit(AstNode &node) const {
    AstNode *const p{&node}; // 变成指针再 dynamic_cast

#define X(nt)                                                                                      \
    if (auto *n{dynamic_cast<nt *>(p)}) {                                                          \
        return visit(*n);                                                                          \
    }
#include "../../parser/ast_nodes/x_ast_nodes.h"
#undef X

    assert(!"Unknown node type");
}

void LiteralFolder::visit(AstNodeClass &node) const {
    for (auto &deco : node.decorators_) visit_and_replace(deco);
    for (auto &base : node.bases_) visit_and_replace(base);
    for (auto &capture : node.captures_) visit_and_replace(capture.value_expr_);
    visit_and_replace(node.doc_);
    visit(*node.body_);
}

void LiteralFolder::visit(AstNodeIf &node) const {
    for (auto &clause : node.clauses_) {
        visit_and_replace(clause.cond_);
        visit_and_replace(clause.body_);
    }
    visit_and_replace(node.else_expr_);
}

void LiteralFolder::visit(AstNodeForCond &node) const {
    visit_and_replace(node.init_);
    visit_and_replace(node.cond_);
    visit_and_replace(node.inc_);
    visit_and_replace(node.body_);
}

void LiteralFolder::visit(AstNodeForIter &node) const {
    visit_and_replace(node.target_);
    visit_and_replace(node.iterable_);
    visit_and_replace(node.body_);
}

void LiteralFolder::visit(AstNodeBreak &) const {}

void LiteralFolder::visit(AstNodeContinue &) const {}

void LiteralFolder::visit(AstNodeReturn &node) const { visit_and_replace(node.value_); }

void LiteralFolder::visit(AstNodeTry &node) const {
    visit_and_replace(node.try_expr_);
    for (auto &clause : node.except_clauses_) {
        for (auto &exc : clause.exceptions_) visit_and_replace(exc);
        visit_and_replace(clause.body_);
    }
    visit_and_replace(node.finally_expr_);
}

void LiteralFolder::visit(AstNodeRaise &node) const { visit_and_replace(node.value_); }

void LiteralFolder::visit(AstNodeDecorator &node) const {
    visit_and_replace(node.decorator_);
    visit_and_replace(node.target_);
}

void LiteralFolder::visit(AstNodeFunc &node) const {
    for (auto &deco : node.decorators_) visit_and_replace(deco);
    for (auto &capture : node.captures_) visit_and_replace(capture.value_expr_);
    for (auto &param : node.params_.positional_) {
        visit_and_replace(param.type_annotation_);
        visit_and_replace(param.default_value_);
    }
    for (auto &param : node.params_.kw_only_) {
        visit_and_replace(param.type_annotation_);
        visit_and_replace(param.default_value_);
    }
    visit_and_replace(node.return_type_);
    visit_and_replace(node.doc_);
    visit(*node.body_);
}

void LiteralFolder::visit(AstNodeLiteralNone &) const {}

void LiteralFolder::visit(AstNodeLiteralBool &) const {}

void LiteralFolder::visit(AstNodeLiteralGL &) const {}

void LiteralFolder::visit(AstNodeLiteralInt &) const {}

void LiteralFolder::visit(AstNodeLiteralFloat &) const {}

void LiteralFolder::visit(AstNodeLiteralStr &) const {}

void LiteralFolder::visit(AstNodeLiteralTuple &node) const {
    for (auto &item : node.items_) visit_and_replace(item);
}

void LiteralFolder::visit(AstNodeLiteralList &node) const {
    for (auto &item : node.items_) visit_and_replace(item);
}

void LiteralFolder::visit(AstNodeLiteralDict &node) const {
    for (auto &[key, val] : node.items_) {
        visit_and_replace(key);
        visit_and_replace(val);
    }
}

void LiteralFolder::visit(AstNodeLiteralEllipsis &) const {}

void LiteralFolder::visit(AstNodeProgram &node) const {
    for (auto &e : node.exprs_) visit_and_replace(e);
}

void LiteralFolder::visit(AstNodeCompound &node) const {
    for (auto &e : node.exprs_) visit_and_replace(e);
}

void LiteralFolder::visit(AstNodeStar &node) const { visit_and_replace(node.operand_); }

void LiteralFolder::visit(AstNodeDoubleStar &node) const { visit_and_replace(node.operand_); }

void LiteralFolder::visit(AstNodeOpUnary &node) const { visit_and_replace(node.operand_); }

void LiteralFolder::visit(AstNodeOpBinary &node) const {
    visit_and_replace(node.left_);
    visit_and_replace(node.right_);
}

void LiteralFolder::visit(AstNodeCompare &node) const {
    for (auto &operand : node.operands_) visit_and_replace(operand);
}

void LiteralFolder::visit(AstNodeIs &node) const {
    for (auto &operand : node.operands_) visit_and_replace(operand);
}

void LiteralFolder::visit(AstNodeAssign &node) const {
    visit_and_replace(node.target_);
    visit_and_replace(node.value_);
}

void LiteralFolder::visit(AstNodeCompoundAssign &node) const {
    visit_and_replace(node.target_);
    visit_and_replace(node.value_);
}

void LiteralFolder::visit(AstNodeCall &node) const {
    visit_and_replace(node.object_);
    for (auto &arg : node.positional_args_) visit_and_replace(arg);
    for (auto &kw : node.keyword_args_) visit_and_replace(kw.value_);
}

void LiteralFolder::visit(AstNodeIndex &node) const {
    visit_and_replace(node.object_);
    for (auto &arg : node.args_) visit_and_replace(arg);
}

void LiteralFolder::visit(AstNodeAttr &node) const { visit_and_replace(node.object_); }

void LiteralFolder::visit(AstNodeIdentifier &) const {}

void LiteralFolder::visit(AstNodeDel &node) const { visit_and_replace(node.target_); }

void LiteralFolder::visit(AstNodeGlobal &) const {}

LiteralFolder::LiteralFolder(AstNodeProgram &root) : root_{root} {}

void LiteralFolder::fold() const && { visit(root_); }
