#include "SemanticChecker.h"

#include "../../builtins/exceptions/InternalError.h"
#include "../../builtins/exceptions/SyntaxError.h"

#include <unordered_set>

void SemanticChecker::error(const std::string &msg, const Position pos) const {
    throw SyntaxError{file_path_, pos.row, pos.col, msg};
}

void SemanticChecker::error_internal(const std::string &msg, const Position pos) const {
    throw InternalError{file_path_, pos.row, pos.col, msg};
}

void SemanticChecker::require_not_null(const AstNodePtr &node, const Position pos) const {
    if (!node) error_internal("unexpected null node", pos);
}

void SemanticChecker::require_not_empty(const std::u32string &name, const Position pos) const {
    if (name.empty()) error_internal("unexpected empty name", pos);
}

void SemanticChecker::visit(const AstNode &node) { node.accept(*this); }

void SemanticChecker::visit(const AstNodeClass &node) {
    const Position pos{node.pos_};

    const ContextGuard guard{*this};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    for (const auto &deco : node.decorators_) check_not_null(deco, pos);
    require_same_size(node.decorators_, node.positions_decorator_, pos);
    if (node.name_) require_not_empty(*node.name_, pos);
    for (const auto &base : node.bases_) check_not_null(base, pos);

    std::unordered_set<std::u32string> names;
    for (const auto &capture : node.captures_) {
        require_not_empty(capture.identifier_, pos);
        // 如果是已经存在
        if (!names.insert(capture.identifier_).second) error("duplicate name in capture list", pos);
        check_nullable(capture.value_expr_);
    }

    check_doc(node.doc_);

    ctx_.in_local_scope = true;
    ctx_.loop_depth = 0;
    ctx_.finally_loop_depth = -1;
    check_not_null(node.body_, pos);
}

void SemanticChecker::visit(const AstNodeIf &node) {
    const Position pos{node.pos_};

    const ContextGuard guard{*this};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    require_min_size(node.clauses_, 1, pos);
    for (const auto &clause : node.clauses_) {
        check_not_null(clause.cond_, pos);
        check_not_null(clause.body_, pos);
    }
    check_nullable(node.else_expr_);
}

void SemanticChecker::visit(const AstNodeForCond &node) {
    const Position pos{node.pos_};

    if (node.collect_.container_ == CollectMark::Container::None && node.collect_.expand_)
        error_internal("expand flag without a collect container", pos);

    const ContextGuard guard{*this};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    check_nullable(node.init_);
    check_nullable(node.cond_);
    check_nullable(node.inc_);
    ctx_.loop_depth++;
    check_not_null(node.body_, pos);
}

void SemanticChecker::visit(const AstNodeForIter &node) {
    const Position pos{node.pos_};

    if (node.collect_.container_ == CollectMark::Container::None && node.collect_.expand_)
        error_internal("expand flag without a collect container", pos);

    const ContextGuard guard{*this};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    check_not_null(node.iterable_, pos);
    if (node.target_) check_lvalue(*node.target_);
    ctx_.loop_depth++;
    require_not_null(node.body_, node.pos_), visit(*node.body_);
}

void SemanticChecker::visit(const AstNodeBreak &node) {
    if (ctx_.loop_depth == 0) error("break outside loop", node.pos_);
    // finally 体内禁止 break 跳出 finally 范围
    if (ctx_.finally_loop_depth >= 0 && ctx_.loop_depth == ctx_.finally_loop_depth)
        error("break inside finally is not allowed", node.pos_);
}

void SemanticChecker::visit(const AstNodeContinue &node) {
    if (ctx_.loop_depth == 0) error("continue outside loop", node.pos_);
    // finally 体内禁止 continue 跳出 finally 范围
    if (ctx_.finally_loop_depth >= 0 && ctx_.loop_depth == ctx_.finally_loop_depth)
        error("continue inside finally is not allowed", node.pos_);
}

void SemanticChecker::visit(const AstNodeReturn &node) {
    // return 的作用对象是离它最近的 Program，外层一个 Program 都没有就无处可去
    if (!ctx_.in_program) error("return outside program", node.pos_);

    // finally 体内禁止 return
    if (ctx_.finally_loop_depth >= 0) error("return inside finally is not allowed", node.pos_);

    const ContextGuard guard{*this};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    check_nullable(node.value_);
}

void SemanticChecker::visit(const AstNodeTry &node) {
    const Position pos{node.pos_};

    const ContextGuard guard{*this};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    check_not_null(node.try_expr_, pos);
    // except 和 finally 不能同时不存在
    if (node.except_clauses_.empty() && !node.finally_expr_) {
        error("try must have at least one except or finally", node.pos_);
    }
    for (const auto &clause : node.except_clauses_) {
        require_min_size(clause.exceptions_, 1, pos);
        for (const auto &exc : clause.exceptions_) check_not_null(exc, pos);
        if (clause.target_) check_lvalue(*clause.target_);
        check_not_null(clause.body_, pos);
    }

    // finally 体内拦截 return/break/continue
    // 保存此时的 loop_depth 作为 finally 拦截的基准
    ctx_.finally_loop_depth = ctx_.loop_depth;
    check_nullable(node.finally_expr_);
}

void SemanticChecker::visit(const AstNodeRaise &node) {
    const ContextGuard guard{*this};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    check_not_null(node.value_, node.pos_);
}

void SemanticChecker::visit(const AstNodeDecorator &node) {
    const Position pos{node.pos_};

    const ContextGuard guard{*this};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    check_not_null(node.decorator_, pos);
    check_not_null(node.target_, pos);
}

void SemanticChecker::visit(const AstNodeEval &node) {
    const ContextGuard guard{*this};
    check_call_args(node.args_, node.pos_);
}

void SemanticChecker::visit(const AstNodeFunc &node) {
    const Position pos{node.pos_};

    const ContextGuard guard{*this};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    for (const auto &deco : node.decorators_) check_not_null(deco, pos);
    require_same_size(node.decorators_, node.positions_decorator_, pos);
    if (node.name_) require_not_empty(*node.name_, pos);

    std::unordered_set<std::u32string> names;

    // 排重函数
    auto ensure_unique{[&](const std::u32string &name) {
        // 如果是已经存在
        if (!names.insert(name).second) error("duplicate name in capture/parameter list", pos);
    }};

    // 捕获
    for (const auto &capture : node.captures_) {
        require_not_empty(capture.identifier_, pos), ensure_unique(capture.identifier_);
        check_nullable(capture.value_expr_);
    }

    // 形参
    bool has_seen_default{false};
    for (const auto &param : node.params_.positional_) {
        require_not_empty(param.identifier_, pos), ensure_unique(param.identifier_);
        if (param.default_value_)
            has_seen_default = true;
        else // 如果当前这个没有默认值，且前边的某个有默认值
            if (has_seen_default) error("non-default parameter after default parameter", pos);
        check_nullable(param.type_annotation_);
        check_nullable(param.default_value_);
    }
    if (node.params_.var_args_name_) {
        require_not_empty(*node.params_.var_args_name_, pos),
            ensure_unique(*node.params_.var_args_name_);
    }
    for (const auto &param : node.params_.kw_only_) {
        require_not_empty(param.identifier_, pos), ensure_unique(param.identifier_);
        check_nullable(param.type_annotation_);
        check_nullable(param.default_value_);
    }
    if (node.params_.var_kwargs_name_) {
        require_not_empty(*node.params_.var_kwargs_name_, pos),
            ensure_unique(*node.params_.var_kwargs_name_);
    }

    check_nullable(node.return_type_);
    check_doc(node.doc_);

    ctx_.in_local_scope = true;
    ctx_.loop_depth = 0;
    ctx_.finally_loop_depth = -1;
    visit(*node.body_);
}

void SemanticChecker::visit(const AstNodeImportKw &node) {
    require_min_size(node.segments_, 1, node.pos_);
    for (const auto &segment : node.segments_) require_not_empty(segment, node.pos_);
}

void SemanticChecker::visit(const AstNodeImportCall &node) {
    const ContextGuard guard{*this};
    check_call_args(node.args_, node.pos_);
}

void SemanticChecker::visit(const AstNodeLiteralNone &) {}

void SemanticChecker::visit(const AstNodeLiteralBool &) {}

void SemanticChecker::visit(const AstNodeLiteralGL &) {}

void SemanticChecker::visit(const AstNodeLiteralInt &) {}

void SemanticChecker::visit(const AstNodeLiteralDecimal &) {}

void SemanticChecker::visit(const AstNodeLiteralStr &) {}

void SemanticChecker::visit(const AstNodeLiteralTuple &node) {
    const ContextGuard guard{*this};
    ctx_.can_star = true;
    ctx_.can_double_star = false;

    for (const auto &item : node.items_) check_not_null(item, node.pos_);
}

void SemanticChecker::visit(const AstNodeLiteralList &node) {
    const ContextGuard guard{*this};
    ctx_.can_star = true;
    ctx_.can_double_star = false;

    for (const auto &item : node.items_) check_not_null(item, node.pos_);
}

void SemanticChecker::visit(const AstNodeLiteralDict &node) {
    const Position pos{node.pos_};

    const ContextGuard guard{*this};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    for (const auto &[k, v] : node.items_) {
        ctx_.can_double_star = true;
        check_not_null(k, pos);
        ctx_.can_double_star = false;

        // 是 **dict
        if (dynamic_cast<const AstNodeDoubleStar *>(k.get())) {
            if (v) error_internal("** dict-spread entry must not have a value", pos);
        }
        // 是 k: v
        else
            check_not_null(v, pos);
    }
}

void SemanticChecker::visit(const AstNodeLiteralEllipsis &) {}

void SemanticChecker::visit(const AstNodeProgram &node) {
    const ContextGuard guard{*this};
    ctx_.can_star = false;
    ctx_.can_double_star = false;
    ctx_.in_program = true;

    for (const auto &e : node.exprs_) check_not_null(e, node.pos_);
}

void SemanticChecker::visit(const AstNodeCompound &node) {
    const ContextGuard guard{*this};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    for (const auto &e : node.exprs_) check_not_null(e, node.pos_);
}

void SemanticChecker::visit(const AstNodeStar &node) {
    const Position pos{node.pos_};

    if (!ctx_.can_star)
        error("* can only appear in tuple, list, index, or function call arguments", pos);

    const ContextGuard guard{*this};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    check_not_null(node.operand_, pos);
}

void SemanticChecker::visit(const AstNodeDoubleStar &node) {
    const Position pos{node.pos_};

    if (!ctx_.can_double_star)
        error("** can only appear in dict literal or function call arguments", pos);

    const ContextGuard guard{*this};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    check_not_null(node.operand_, pos);
}

void SemanticChecker::visit(const AstNodeOpUnary &node) {
    const ContextGuard guard{*this};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    check_not_null(node.operand_, node.pos_);
}

void SemanticChecker::visit(const AstNodeOpBinary &node) {
    const Position pos{node.pos_};

    const ContextGuard guard{*this};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    check_not_null(node.left_, pos);
    check_not_null(node.right_, pos);
}

void SemanticChecker::visit(const AstNodeCompare &node) {
    const Position pos{node.pos_};

    const ContextGuard guard{*this};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    if (node.operands_.size() != node.ops_.size() + 1)
        error_internal("operands/ops count mismatch", pos);
    require_min_size(node.operands_, 2, pos);
    require_same_size(node.ops_, node.positions_op_, pos);
    for (const auto &operand : node.operands_) check_not_null(operand, pos);
}

void SemanticChecker::visit(const AstNodeIs &node) {
    const Position pos{node.pos_};

    const ContextGuard guard{*this};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    require_min_size(node.operands_, 2, node.pos_);
    if (node.operands_.size() != node.positions_op_.size() + 1)
        error_internal("operands/op positions count mismatch", pos);
    for (const auto &operand : node.operands_) check_not_null(operand, pos);
}

void SemanticChecker::visit(const AstNodeAssign &node) {
    const Position pos{node.pos_};

    const ContextGuard guard{*this};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    require_not_null(node.target_, pos), check_lvalue(*node.target_);
    check_not_null(node.value_, pos);
}

void SemanticChecker::visit(const AstNodeCompoundAssign &node) {
    const Position pos{node.pos_};

    const ContextGuard guard{*this};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    require_not_null(node.target_, pos), check_lvalue_pure(*node.target_);
    check_not_null(node.value_, pos);
}

void SemanticChecker::visit(const AstNodeCall &node) {
    const Position pos{node.pos_};

    const ContextGuard guard{*this};

    ctx_.can_star = false;
    ctx_.can_double_star = false;
    check_not_null(node.object_, pos);

    check_call_args(node.args_, pos);
}

void SemanticChecker::visit(const AstNodeIndex &node) {
    const Position pos{node.pos_};

    const ContextGuard guard{*this};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    check_not_null(node.object_, pos);

    require_min_size(node.args_, 1, pos);
    ctx_.can_star = true;
    for (const auto &a : node.args_) check_not_null(a, pos);
}

void SemanticChecker::visit(const AstNodeAttr &node) {
    const Position pos{node.pos_};

    const ContextGuard guard{*this};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    check_not_null(node.object_, pos);
    require_not_empty(node.attr_, pos);
}

void SemanticChecker::visit(const AstNodeIdentifier &node) {
    require_not_empty(node.identifier_, node.pos_);
}

void SemanticChecker::visit(const AstNodeDel &node) {
    const ContextGuard guard{*this};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    check_not_null(node.target_, node.pos_);

    // target 必须是标识符或属性访问
    if (!dynamic_cast<const AstNodeIdentifier *>(node.target_.get()) &&
        !dynamic_cast<const AstNodeAttr *>(node.target_.get()))
        error("del target must be an identifier or attribute access", node.target_->pos_);
}

void SemanticChecker::visit(const AstNodeGlobal &node) {
    require_not_empty(node.identifier_, node.pos_);
    if (!ctx_.in_local_scope) error("global outside function/class body", node.pos_);
}

void SemanticChecker::check_not_null(const AstNodePtr &node, const Position pos) {
    if (!node) error_internal("unexpected null node", pos);
    visit(*node);
}

void SemanticChecker::check_not_null(const AstNodeProgramPtr &node, const Position pos) {
    if (!node) error_internal("unexpected null node", pos);
    visit(*node);
}

void SemanticChecker::check_nullable(const AstNodePtr &node) {
    if (node) visit(*node);
}

void SemanticChecker::check_lvalue(const AstNode &node) {
    // a  a[ind]  a.x
    if (dynamic_cast<const AstNodeIdentifier *>(&node) ||
        dynamic_cast<const AstNodeIndex *>(&node) || dynamic_cast<const AstNodeAttr *>(&node)) {
        return visit(node);
    }

    // (a, b)  [a, b]
    if (const auto *n{dynamic_cast<const AstNodeLiteralTuple *>(&node)}) {
        return check_lvalue_items(n->items_, node.pos_);
    }
    if (const auto *n{dynamic_cast<const AstNodeLiteralList *>(&node)}) {
        return check_lvalue_items(n->items_, node.pos_);
    }

    error("lvalue expected before assignment", node.pos_);
}

void SemanticChecker::check_lvalue_items(const std::vector<AstNodePtr> &items, const Position pos) {
    bool has_seen_star{false};
    for (const auto &item : items) {
        require_not_null(item, pos);
        if (const auto *star{dynamic_cast<const AstNodeStar *>(item.get())}) {
            // 解构时每一层至多一个左值可以带 * 前缀（收集剩余元素）；* 后面本身仍是一个左值，可嵌套
            if (has_seen_star)
                error("at most one starred lvalue allowed in destructuring", star->pos_);
            has_seen_star = true;
            require_not_null(star->operand_, pos);
            check_lvalue(*star->operand_);
        } else {
            check_lvalue(*item);
        }
    }
}

void SemanticChecker::check_lvalue_pure(const AstNode &node) {
    // a  a[ind]  a.x
    if (dynamic_cast<const AstNodeIdentifier *>(&node) ||
        dynamic_cast<const AstNodeIndex *>(&node) || dynamic_cast<const AstNodeAttr *>(&node)) {
        return visit(node);
    }

    error("identifier, attribute access, or index expression expected before op=", node.pos_);
}

void SemanticChecker::check_doc(const AstNodePtr &doc) const {
    if (doc && !dynamic_cast<const AstNodeLiteralStr *>(doc.get()))
        error("doc must be a string literal", doc->pos_);
}

void SemanticChecker::check_call_args(const CallArgs &args, const Position pos) {
    // 位置组：位置传参、*expr
    ctx_.can_star = true;
    ctx_.can_double_star = false;
    for (const auto &arg : args.positional_args_) check_not_null(arg, pos);

    // 关键字组：关键字实参、**expr
    for (const auto &kw : args.keyword_args_) {
        if (kw.kind_ == OneKwArg::Kind::Keyword) require_not_empty(kw.keyword_, pos);

        ctx_.can_star = false;
        ctx_.can_double_star = kw.kind_ == OneKwArg::Kind::DoubleStar;
        check_not_null(kw.value_, pos);
    }
}

SemanticChecker::SemanticChecker(const AstNode &root, std::string file_path)
    : root_{root}, file_path_{std::move(file_path)} {}

void SemanticChecker::check() && { visit(root_); }
