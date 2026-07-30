#include "LiteralFolder.h"

#include "StaticEvaler.h"

#include <cassert>
#include <utility>

void LiteralFolder::visit_and_replace(AstNodePtr &node) {
    if (!node) return;
    visit(*node);
    // 折到不动为止
    while (AstNodePtr folded{StaticEvaler::fold(*node)}) node = std::move(folded);
}

void LiteralFolder::visit(AstNode &node) {
    AstNode *const p{&node}; // 变成指针再 dynamic_cast

#define X(nt)                                                                                      \
    if (auto *n{dynamic_cast<nt *>(p)}) return visit(*n);
#include "../../parser/ast_nodes/x_ast_nodes.h"
#undef X

    assert(!"Unknown node type");
}

void LiteralFolder::visit(AstNodeClass &node) {
    for (auto &deco : node.decorators_) visit_and_replace(deco);
    for (auto &base : node.bases_) visit_and_replace(base);
    for (auto &capture : node.captures_) visit_and_replace(capture.value_expr_);
    visit_and_replace(node.doc_);
    visit(*node.body_);
}

void LiteralFolder::visit(AstNodeIf &node) {
    for (auto &clause : node.clauses_) {
        visit_and_replace(clause.cond_);
        visit_and_replace(clause.body_);
    }
    visit_and_replace(node.else_expr_);
}

void LiteralFolder::visit(AstNodeForCond &node) {
    visit_and_replace(node.init_);
    visit_and_replace(node.cond_);
    visit_and_replace(node.inc_);
    visit_and_replace(node.body_);
}

void LiteralFolder::visit(AstNodeForIter &node) {
    visit_and_replace(node.target_);
    visit_and_replace(node.iterable_);
    visit_and_replace(node.body_);
}

void LiteralFolder::visit(AstNodeBreak &) {}

void LiteralFolder::visit(AstNodeContinue &) {}

void LiteralFolder::visit(AstNodeReturn &node) { visit_and_replace(node.value_); }

void LiteralFolder::visit(AstNodeTry &node) {
    visit_and_replace(node.try_expr_);
    for (auto &clause : node.except_clauses_) {
        for (auto &exc : clause.exceptions_) visit_and_replace(exc);
        visit_and_replace(clause.body_);
    }
    visit_and_replace(node.finally_expr_);
}

void LiteralFolder::visit(AstNodeRaise &node) { visit_and_replace(node.value_); }

void LiteralFolder::visit(AstNodeDecorator &node) {
    visit_and_replace(node.decorator_);
    visit_and_replace(node.target_);
}

void LiteralFolder::visit(AstNodeFunc &node) {
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

void LiteralFolder::visit(AstNodeLiteralNone &) {}

void LiteralFolder::visit(AstNodeLiteralBool &) {}

void LiteralFolder::visit(AstNodeLiteralGL &) {}

void LiteralFolder::visit(AstNodeLiteralInt &) {}

void LiteralFolder::visit(AstNodeLiteralFloat &) {}

void LiteralFolder::visit(AstNodeLiteralStr &) {}

void LiteralFolder::visit(AstNodeLiteralTuple &node) {
    for (auto &item : node.items_) visit_and_replace(item);
}

void LiteralFolder::visit(AstNodeLiteralList &node) {
    for (auto &item : node.items_) visit_and_replace(item);
}

void LiteralFolder::visit(AstNodeLiteralDict &node) {
    for (auto &[key, val] : node.items_) {
        visit_and_replace(key);
        visit_and_replace(val);
    }
}

void LiteralFolder::visit(AstNodeLiteralEllipsis &) {}

void LiteralFolder::visit(AstNodeProgram &node) {
    for (auto &e : node.exprs_) visit_and_replace(e);
    StaticEvaler::prune_program(node);
}

void LiteralFolder::visit(AstNodeCompound &node) {
    for (auto &e : node.exprs_) visit_and_replace(e);
}

void LiteralFolder::visit(AstNodeStar &node) { visit_and_replace(node.operand_); }

void LiteralFolder::visit(AstNodeDoubleStar &node) { visit_and_replace(node.operand_); }

void LiteralFolder::visit(AstNodeOpUnary &node) { visit_and_replace(node.operand_); }

void LiteralFolder::visit(AstNodeOpBinary &node) {
    visit_and_replace(node.left_);
    visit_and_replace(node.right_);
}

void LiteralFolder::visit(AstNodeCompare &node) {
    for (auto &operand : node.operands_) visit_and_replace(operand);
}

void LiteralFolder::visit(AstNodeIs &node) {
    for (auto &operand : node.operands_) visit_and_replace(operand);
}

void LiteralFolder::visit(AstNodeAssign &node) {
    visit_and_replace(node.target_);
    visit_and_replace(node.value_);
}

void LiteralFolder::visit(AstNodeCompoundAssign &node) {
    visit_and_replace(node.target_);
    visit_and_replace(node.value_);
}

void LiteralFolder::visit(AstNodeCall &node) {
    visit_and_replace(node.object_);
    for (auto &arg : node.positional_args_) visit_and_replace(arg);
    for (auto &kw : node.keyword_args_) visit_and_replace(kw.value_);
}

void LiteralFolder::visit(AstNodeIndex &node) {
    visit_and_replace(node.object_);
    for (auto &arg : node.args_) visit_and_replace(arg);
}

void LiteralFolder::visit(AstNodeAttr &node) { visit_and_replace(node.object_); }

void LiteralFolder::visit(AstNodeIdentifier &) {}

void LiteralFolder::visit(AstNodeDel &node) { visit_and_replace(node.target_); }

void LiteralFolder::visit(AstNodeGlobal &) {}

LiteralFolder::LiteralFolder(AstNodeProgram &root) : root_{root} {}

void LiteralFolder::fold() const && { visit(root_); }

void LiteralFolder::fold_expr(AstNodePtr &node) { visit_and_replace(node); }
