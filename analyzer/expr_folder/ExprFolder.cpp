#include "ExprFolder.h"

#include "StaticEvaler.h"

#include <utility>

void ExprFolder::visit_and_replace(AstNodePtr &node) {
    if (!node) return;
    visit_any(*node);
    // 折到不动为止
    while (AstNodePtr folded{StaticEvaler::fold(*node)}) node = std::move(folded);
}

void ExprFolder::visit_any(AstNode &node) { node.accept(*this); }

void ExprFolder::visit(AstNodeClass &node) {
    for (auto &deco : node.decorators_) visit_and_replace(deco);
    for (auto &base : node.bases_) visit_and_replace(base);
    for (auto &capture : node.captures_) visit_and_replace(capture.value_expr_);
    visit_and_replace(node.doc_);
    visit(*node.body_);
}

void ExprFolder::visit(AstNodeIf &node) {
    for (auto &clause : node.clauses_) {
        visit_and_replace(clause.cond_);
        visit_and_replace(clause.body_);
    }
    visit_and_replace(node.else_expr_);
}

void ExprFolder::visit(AstNodeForCond &node) {
    visit_and_replace(node.init_);
    visit_and_replace(node.cond_);
    visit_and_replace(node.inc_);
    visit_and_replace(node.body_);
}

void ExprFolder::visit(AstNodeForIter &node) {
    visit_and_replace(node.target_);
    visit_and_replace(node.iterable_);
    visit_and_replace(node.body_);
}

void ExprFolder::visit(AstNodeBreak &) {}

void ExprFolder::visit(AstNodeContinue &) {}

void ExprFolder::visit(AstNodeReturn &node) { visit_and_replace(node.value_); }

void ExprFolder::visit(AstNodeTry &node) {
    visit_and_replace(node.try_expr_);
    for (auto &clause : node.except_clauses_) {
        for (auto &exc : clause.exceptions_) visit_and_replace(exc);
        visit_and_replace(clause.body_);
    }
    visit_and_replace(node.finally_expr_);
}

void ExprFolder::visit(AstNodeRaise &node) { visit_and_replace(node.value_); }

void ExprFolder::visit(AstNodeDecorator &node) {
    visit_and_replace(node.decorator_);
    visit_and_replace(node.target_);
}

void ExprFolder::visit(AstNodeFunc &node) {
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

void ExprFolder::visit(AstNodeEval &node) { visit_and_replace(node.code_); }

void ExprFolder::visit(AstNodeLiteralNone &) {}

void ExprFolder::visit(AstNodeLiteralBool &) {}

void ExprFolder::visit(AstNodeLiteralGL &) {}

void ExprFolder::visit(AstNodeLiteralInt &) {}

void ExprFolder::visit(AstNodeLiteralDecimal &) {}

void ExprFolder::visit(AstNodeLiteralStr &) {}

void ExprFolder::visit(AstNodeLiteralTuple &node) {
    for (auto &item : node.items_) visit_and_replace(item);
}

void ExprFolder::visit(AstNodeLiteralList &node) {
    for (auto &item : node.items_) visit_and_replace(item);
}

void ExprFolder::visit(AstNodeLiteralDict &node) {
    for (auto &[key, val] : node.items_) {
        visit_and_replace(key);
        visit_and_replace(val);
    }
}

void ExprFolder::visit(AstNodeLiteralEllipsis &) {}

void ExprFolder::visit(AstNodeProgram &node) {
    for (auto &e : node.exprs_) visit_and_replace(e);
    StaticEvaler::prune_program(node);
}

void ExprFolder::visit(AstNodeCompound &node) {
    for (auto &e : node.exprs_) visit_and_replace(e);
}

void ExprFolder::visit(AstNodeStar &node) { visit_and_replace(node.operand_); }

void ExprFolder::visit(AstNodeDoubleStar &node) { visit_and_replace(node.operand_); }

void ExprFolder::visit(AstNodeOpUnary &node) { visit_and_replace(node.operand_); }

void ExprFolder::visit(AstNodeOpBinary &node) {
    visit_and_replace(node.left_);
    visit_and_replace(node.right_);
}

void ExprFolder::visit(AstNodeCompare &node) {
    for (auto &operand : node.operands_) visit_and_replace(operand);
}

void ExprFolder::visit(AstNodeIs &node) {
    for (auto &operand : node.operands_) visit_and_replace(operand);
}

void ExprFolder::visit(AstNodeAssign &node) {
    visit_and_replace(node.target_);
    visit_and_replace(node.value_);
}

void ExprFolder::visit(AstNodeCompoundAssign &node) {
    visit_and_replace(node.target_);
    visit_and_replace(node.value_);
}

void ExprFolder::visit(AstNodeCall &node) {
    visit_and_replace(node.object_);
    for (auto &arg : node.positional_args_) visit_and_replace(arg);
    for (auto &kw : node.keyword_args_) visit_and_replace(kw.value_);
}

void ExprFolder::visit(AstNodeIndex &node) {
    visit_and_replace(node.object_);
    for (auto &arg : node.args_) visit_and_replace(arg);
}

void ExprFolder::visit(AstNodeAttr &node) { visit_and_replace(node.object_); }

void ExprFolder::visit(AstNodeIdentifier &) {}

void ExprFolder::visit(AstNodeDel &node) { visit_and_replace(node.target_); }

void ExprFolder::visit(AstNodeGlobal &) {}

void ExprFolder::visit(AstNodeImportKw &) {}

void ExprFolder::visit(AstNodeImportCall &node) {
    for (auto &arg : node.positional_args_) visit_and_replace(arg);
    for (auto &kw : node.keyword_args_) visit_and_replace(kw.value_);
}

void ExprFolder::fold(AstNodeProgram &root) { ExprFolder{}.visit(root); }

void ExprFolder::fold_expr(AstNodePtr &node) { ExprFolder{}.visit_and_replace(node); }
