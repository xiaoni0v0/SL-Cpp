#include "SyntaxChecker.h"

#include "../../builtins/classes/exceptions/SyntaxError.h"

#include <cassert>
#include <ranges>
#include <unordered_set>

void SyntaxChecker::error(const std::string &msg, const int row, const int col) const {
    throw SyntaxError{file_path_, row, col, msg};
}

void SyntaxChecker::require_not_null(const AstNodePtr &node) const {
    if (!node) error("unexpected null node", 0, 0);
}

void SyntaxChecker::require_not_null(const std::u32string &name) const {
    if (name.empty()) error("unexpected null name", 0, 0);
}

void SyntaxChecker::check(const AstNode *node) {
    if (!node) return;

#define X(nt) if (const auto *n{dynamic_cast<const nt *>(node)}) { return check(n); }
#include "../../parser/ast_nodes/x_ast_nodes.h"
#undef X

    assert(!"Unknown node type");
}

void SyntaxChecker::check(const AstNodeCall *node) {
    const Context saved = ctx_;

    // object_
    ctx_.can_star = false;
    ctx_.can_double_star = false;
    check(node->object_.get());

    // 参数们
    bool seen_double_star = false; // ** 之后不能再出现其他参数
    ctx_.can_star = true;
    ctx_.can_double_star = true;
    for (const auto &arg : node->args_) {
        if (dynamic_cast<const AstNodeDoubleStar *>(arg.get())) seen_double_star = true;
        else if (seen_double_star) error("argument after ** spread", arg->row_, arg->col_);
        check(arg.get());
    }

    ctx_.can_star = false;
    ctx_.can_double_star = false;
    for (const auto &val : node->kwargs_ | std::views::values) check(val.get());

    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeIndex *node) {
    const Context saved = ctx_;
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    // object_
    require_not_null(node->object_);
    check(node->object_.get());

    // args_
    ctx_.can_star = true;
    for (const auto &a : node->args_) {
        require_not_null(a);
        check(a.get());
    }

    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeAttr *node) {
    const Context saved = ctx_;
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    // object_
    require_not_null(node->object_);
    check(node->object_.get());

    // attr_
    require_not_null(node->attr_);

    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeIf *node) {
    const Context saved = ctx_;
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    // clauses_
    require_not_null(node->clauses_);
    for (const auto &clause : node->clauses_) {
        check(clause.cond_.get());
        check(clause.body_.get());
    }
    check(node->else_expr_.get());
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeForCond *node) {
    const Context saved = ctx_;
    ctx_.can_star = false;
    ctx_.can_double_star = false;
    check(node->init_.get());
    check(node->cond_.get());
    check(node->inc_.get());
    ctx_.for_depth++;
    check(node->body_.get());
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeForIter *node) {
    const Context saved = ctx_;
    ctx_.can_star = false;
    ctx_.can_double_star = false;
    if (!dynamic_cast<const AstNodeIdentifier *>(node->target_.get()))
        error("for-iter target must be an identifier",
              node->target_->row_, node->target_->col_);
    check(node->iterable_.get());
    ctx_.for_depth++;
    check(node->body_.get());
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeBreak *node) {
    if (ctx_.for_depth == 0) error("break outside for loop", node->row_, node->col_);
}

void SyntaxChecker::check(const AstNodeContinue *node) {
    if (ctx_.for_depth == 0) error("continue outside for loop", node->row_, node->col_);
}

void SyntaxChecker::check(const AstNodeReturn *node) {
    if (ctx_.func_depth == 0) error("return outside function", node->row_, node->col_);
    const Context saved = ctx_;
    ctx_.can_star = false;
    ctx_.can_double_star = false;
    check(node->value_.get());
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeTry *node) {
    const Context saved = ctx_;
    ctx_.can_star = false;
    ctx_.can_double_star = false;
    if (node->except_clauses_.empty() && !node->finally_expr_)
        error("try must have at least one except or finally",
              node->row_, node->col_);
    check(node->try_expr_.get());
    for (const auto &clause : node->except_clauses_) {
        for (const auto &exc : clause.exceptions_) check(exc.get());
        check(clause.body_.get());
    }
    check(node->finally_expr_.get());
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeRaise *node) {
    const Context saved = ctx_;
    ctx_.can_star = false;
    ctx_.can_double_star = false;
    check(node->value_.get());
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeDecorator *node) {
    const Context saved = ctx_;
    ctx_.can_star = false;
    ctx_.can_double_star = false;
    check(node->decorator_.get());
    check(node->target_.get());
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeFunc *node) {
    using PT = AstNodeFunc::Param::ParamType;

    const Context saved = ctx_;
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    // 形参顺序与重复检查
    std::unordered_set<std::u32string> seen_names;
    bool seen_star_args = false;
    bool seen_double_star = false;
    bool seen_default = false;

    for (const auto &param : node->params_) {
        if (!seen_names.insert(param.identifier).second) error("duplicate parameter name", node->row_, node->col_);
        if (seen_double_star) error("parameter after **kwargs", node->row_, node->col_);

        switch (param.param_type) {
        case PT::Normal: if (seen_star_args) error("normal parameter after *args", node->row_, node->col_);
            if (param.default_value) seen_default = true;
            else if (seen_default) error("non-default parameter after default parameter", node->row_, node->col_);
            break;
        case PT::StarArgs: if (seen_star_args) error("duplicate *args", node->row_, node->col_);
            seen_star_args = true;
            break;
        case PT::DoubleStarKwargs: seen_double_star = true;
            break;
        }

        if (param.type_annotation) check(param.type_annotation.get());
        if (param.default_value) check(param.default_value.get());
    }

    // 进入函数体（新上下文，for 深度归零）
    ctx_.func_depth++;
    ctx_.for_depth = 0;
    check(node->body_.get());

    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeLiteralNone *) {
}

void SyntaxChecker::check(const AstNodeLiteralBool *) {
}

void SyntaxChecker::check(const AstNodeLiteralGL *) {
}

void SyntaxChecker::check(const AstNodeLiteralInt *) {
}

void SyntaxChecker::check(const AstNodeLiteralFloat *) {
}

void SyntaxChecker::check(const AstNodeLiteralStr *) {
}

void SyntaxChecker::check(const AstNodeLiteralTuple *node) {
    const Context saved = ctx_;
    ctx_.can_star = true;
    ctx_.can_double_star = false;
    for (const auto &item : node->items_) check(item.get());
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeLiteralList *node) {
    const Context saved = ctx_;
    ctx_.can_star = true;
    ctx_.can_double_star = false;
    for (const auto &item : node->items_) check(item.get());
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeLiteralDict *node) {
    const Context saved = ctx_;
    ctx_.can_star = false;
    ctx_.can_double_star = false;
    for (const auto &[key, val] : node->items_) {
        ctx_.can_double_star = true;
        check(key.get());
        ctx_.can_double_star = false;
        if (val) check(val.get());
    }
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeLiteralEllipsis *) {
}

void SyntaxChecker::check(const AstNodeProgram *node) {
    const Context saved = ctx_;
    ctx_.can_star = false;
    ctx_.can_double_star = false;
    for (const auto &e : node->exprs_) check(e.get());
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeCompound *node) {
    const Context saved = ctx_;
    ctx_.can_star = false;
    ctx_.can_double_star = false;
    for (const auto &e : node->exprs_) check(e.get());
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeStar *node) {
    if (!ctx_.can_star)
        error("* can only appear in tuple, list, or function call arguments",
              node->row_, node->col_);
    const Context saved = ctx_;
    ctx_.can_star = false;
    ctx_.can_double_star = false;
    check(node->operand_.get());
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeDoubleStar *node) {
    if (!ctx_.can_double_star)
        error("** can only appear in dict literal or function call arguments",
              node->row_, node->col_);
    const Context saved = ctx_;
    ctx_.can_star = false;
    ctx_.can_double_star = false;
    check(node->operand_.get());
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeOpUnary *node) {
    const Context saved = ctx_;
    ctx_.can_star = false;
    ctx_.can_double_star = false;
    check(node->operand_.get());
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeOpBinary *node) {
    const Context saved = ctx_;
    ctx_.can_star = false;
    ctx_.can_double_star = false;
    check(node->left_.get());
    check(node->right_.get());
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeAssign *node) {
    check_lvalue(node->target_.get());
    const Context saved = ctx_;
    ctx_.can_star = false;
    ctx_.can_double_star = false;
    check(node->value_.get());
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeCompoundAssign *node) {
    // check_simple_lvalue(node->target_.get());
    const Context saved = ctx_;
    ctx_.can_star = false;
    ctx_.can_double_star = false;
    check(node->value_.get());
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeIdentifier *) {
}

void SyntaxChecker::check(const AstNodeDel *node) {
    if (!dynamic_cast<const AstNodeIdentifier *>(node->target_.get()))
        error("del target must be an identifier",
              node->target_->row_, node->target_->col_);
}

void SyntaxChecker::check(const AstNodeGlobal *node) {
    if (ctx_.func_depth == 0) error("global outside function", node->row_, node->col_);
    if (!dynamic_cast<const AstNodeIdentifier *>(node->target_.get()))
        error("global target must be an identifier",
              node->target_->row_, node->target_->col_);
}

void SyntaxChecker::check_lvalue(const AstNode *node) const {
    // a  a[ind]  a.x
    if (dynamic_cast<const AstNodeIdentifier *>(node)) return;
    if (dynamic_cast<const AstNodeIndex *>(node)) return;
    if (dynamic_cast<const AstNodeAttr *>(node)) return;

    // (a, b)  [a, b]
    if (const auto *n{dynamic_cast<const AstNodeLiteralTuple *>(node)}) {
        for (const auto &item : n->items_) check_lvalue(item.get());
        return;
    }
    if (const auto *n{dynamic_cast<const AstNodeLiteralList *>(node)}) {
        for (const auto &item : n->items_) check_lvalue(item.get());
        return;
    }

    error("lvalue expected before assignment", node->row_, node->col_);
}

SyntaxChecker::SyntaxChecker(AstNodeProgram *root, std::string file_path)
    : root_{root}, file_path_{std::move(file_path)} {
}

void SyntaxChecker::check() && {
    check(root_);
}
