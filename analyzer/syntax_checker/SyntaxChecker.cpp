// ReSharper disable CppMemberFunctionMayBeStatic
// ReSharper disable CppMemberFunctionMayBeConst

#include "SyntaxChecker.h"

#include "../../builtins/classes/exceptions/SyntaxError.h"

#include <cassert>
#include <ranges>
#include <unordered_set>

void SyntaxChecker::error(const std::string &msg, const Position pos) const {
    throw SyntaxError{file_path_, pos.row, pos.col, msg};
}

void SyntaxChecker::require_not_null(const AstNodePtr &node, const Position pos) const {
    if (!node) error("Bad AstNode: unexpected null node", pos);
}

void SyntaxChecker::require_not_null(const std::u32string &name, const Position pos) const {
    if (name.empty()) error("Bad AstNode: unexpected empty name", pos);
}

void SyntaxChecker::check(const AstNode &node) {
    const AstNode *const p{&node}; // 变成指针再 dynamic_cast

#define X(nt) if (const auto *n{dynamic_cast<const nt *>(p)}) { return check(*n); }
#include "../../parser/ast_nodes/x_ast_nodes.h"
#undef X

    assert(!"Unknown node type");
}

void SyntaxChecker::check(const AstNodeClass &node) {
    const Position pos{node.pos_};

    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    for (const auto &deco : node.decorators_) check_not_null(deco, pos);
    require_same_size(node.decorators_, node.decorator_positions_, pos);
    if (node.name_) require_not_null(*node.name_, pos);
    for (const auto &base : node.bases_) check_not_null(base, pos);

    std::unordered_set<std::u32string> names;
    for (const auto &capture : node.captures_) {
        // 如果是已经存在
        if (!names.insert(capture.identifier_).second) error("duplicate name in capture list", pos);
        check_nullable(capture.value_expr_);
    }

    check_doc(node.doc_);

    ctx_.local_scope_depth++;
    ctx_.loop_depth = 0;
    check_not_null(node.body_, pos);

    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeIf &node) {
    const Position pos{node.pos_};

    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    require_not_null(node.clauses_, 1, pos);
    for (const auto &clause : node.clauses_) {
        check_not_null(clause.cond_, pos);
        check_not_null(clause.body_, pos);
    }
    check_nullable(node.else_expr_);

    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeForCond &node) {
    const Position pos{node.pos_};

    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    check_nullable(node.init_);
    check_nullable(node.cond_);
    check_nullable(node.inc_);
    ctx_.loop_depth++;
    check_not_null(node.body_, pos);

    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeForIter &node) {
    const Position pos{node.pos_};

    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    require_not_null(node.target_, pos), check_lvalue(*node.target_);
    check_not_null(node.iterable_, pos);
    ctx_.loop_depth++;
    require_not_null(node.body_, node.pos_), check(*node.body_);

    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeBreak &node) {
    if (ctx_.loop_depth == 0) error("break outside loop", node.pos_);
}

void SyntaxChecker::check(const AstNodeContinue &node) {
    if (ctx_.loop_depth == 0) error("continue outside loop", node.pos_);
}

void SyntaxChecker::check(const AstNodeReturn &node) {
    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    check_nullable(node.value_);

    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeTry &node) {
    const Position pos{node.pos_};

    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    check_not_null(node.try_expr_, pos);
    // except 和 finally 不能同时不存在
    if (node.except_clauses_.empty() && !node.finally_expr_) {
        error("try must have at least one except or finally", node.pos_);
    }
    for (const auto &clause : node.except_clauses_) {
        for (const auto &exc : clause.exceptions_) check_not_null(exc, pos);
        check_not_null(clause.body_, pos);
    }
    check_nullable(node.finally_expr_);

    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeRaise &node) {
    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    check_not_null(node.value_, node.pos_);

    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeDecorator &node) {
    const Position pos{node.pos_};

    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    check_not_null(node.decorator_, pos);
    check_not_null(node.target_, pos);

    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeFunc &node) {
    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    for (const auto &deco : node.decorators_) check(*deco);
    require_same_size(node.decorators_, node.decorator_positions_, node.pos_);
    if (node.name_) require_not_null(*node.name_, node.pos_);

    std::unordered_set<std::u32string> names;
    for (const auto &capture : node.captures_) {
        // 如果是已经存在
        if (!names.insert(capture.identifier_).second) {
            error("duplicate name in capture/parameter list", node.pos_);
        }
        check_nullable(capture.value_expr_);
    }
    // SL.md 2.2.6："以上形参若出现，必须遵循以下顺序：1. 无默认值的形参；2. 有默认值的形参、
    // 可变长位置形参（这两种之间顺序不限）；3. 可变长关键字形参"——这条顺序规则只管 *args 之前
    // 的"位置形参"部分；一旦见过 *args，后面的普通形参就是仅关键字形参（3.5："出现在可变长位置
    // 形参之后的槽位标记为仅关键字"），按名字匹配、不按位置，彼此之间有没有默认值不受这条顺序
    // 约束（同 Python：func f(*x, y) {} 合法，y 是必须以关键字方式传入的仅关键字形参）
    bool has_seen_star{false}, has_seen_double_star{false}, has_seen_default{false};
    for (const auto &param : node.params_) {
        // 如果是已经存在
        if (!names.insert(param.identifier_).second) {
            error("duplicate name in capture/parameter list", node.pos_);
        }
        if (has_seen_double_star) error("parameter after **kwargs", node.pos_);

        switch (param.param_type_) {
            using PT = AstNodeFunc::OneParam::ParamType;
        case PT::Normal: if (!has_seen_star) {
                // 仍在位置形参部分：无默认值的形参必须先于有默认值的形参
                if (param.default_value_) has_seen_default = true;
                else if (has_seen_default) error("non-default parameter after default parameter", node.pos_);
            }
            break;
        case PT::StarArgs: if (has_seen_star) error("duplicate *args", node.pos_);
            has_seen_star = true;
            break;
        case PT::DoubleStarKwargs: has_seen_double_star = true;
            break;
        }

        check_nullable(param.type_annotation_);
        check_nullable(param.default_value_);
    }

    check_nullable(node.return_type_);
    check_doc(node.doc_);

    ctx_.local_scope_depth++;
    ctx_.loop_depth = 0;
    check(*node.body_);

    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeLiteralNone &) {
}

void SyntaxChecker::check(const AstNodeLiteralBool &) {
}

void SyntaxChecker::check(const AstNodeLiteralGL &) {
}

void SyntaxChecker::check(const AstNodeLiteralInt &) {
}

void SyntaxChecker::check(const AstNodeLiteralFloat &) {
}

void SyntaxChecker::check(const AstNodeLiteralStr &) {
}

void SyntaxChecker::check(const AstNodeLiteralTuple &node) {
    const Context saved{ctx_};
    ctx_.can_star = true;
    ctx_.can_double_star = false;

    for (const auto &item : node.items_) check_not_null(item, node.pos_);

    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeLiteralList &node) {
    const Context saved{ctx_};
    ctx_.can_star = true;
    ctx_.can_double_star = false;

    for (const auto &item : node.items_) check_not_null(item, node.pos_);

    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeLiteralDict &node) {
    const Position pos{node.pos_};

    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    for (const auto &[k, v] : node.items_) {
        ctx_.can_double_star = true;
        check_not_null(k, pos);
        ctx_.can_double_star = false;

        // 是 **dict
        if (dynamic_cast<const AstNodeDoubleStar *>(k.get())) {
            if (v) error("Bad AstNode: ** dict-spread entry must not have a value", pos);
        }
        // 是 k: v
        else check_not_null(v, pos);
    }

    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeLiteralEllipsis &) {
}

void SyntaxChecker::check(const AstNodeProgram &node) {
    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    for (const auto &e : node.exprs_) check_not_null(e, node.pos_);

    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeCompound &node) {
    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    for (const auto &e : node.exprs_) check_not_null(e, node.pos_);

    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeStar &node) {
    const Position pos{node.pos_};

    if (!ctx_.can_star) error("* can only appear in tuple, list, or function call arguments", pos);

    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    check_not_null(node.operand_, pos);

    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeDoubleStar &node) {
    const Position pos{node.pos_};

    if (!ctx_.can_double_star) error("** can only appear in dict literal or function call arguments", pos);

    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    check_not_null(node.operand_, pos);

    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeOpUnary &node) {
    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    check_not_null(node.operand_, node.pos_);

    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeOpBinary &node) {
    const Position pos{node.pos_};

    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    check_not_null(node.left_, pos);
    check_not_null(node.right_, pos);

    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeCompare &node) {
    const Position pos{node.pos_};

    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    if (node.operands_.size() != node.ops_.size() + 1) error("Bad AstNode: mismatched count", pos);
    require_not_null(node.operands_, 2, pos);
    require_same_size(node.ops_, node.op_positions_, pos);
    for (const auto &operand : node.operands_) check_not_null(operand, pos);

    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeIs &node) {
    const Position pos{node.pos_};

    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    require_not_null(node.operands_, 2, node.pos_);
    if (node.operands_.size() != node.op_positions_.size() + 1) error("Bad AstNode: mismatched count", pos);
    for (const auto &operand : node.operands_) check_not_null(operand, pos);

    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeAssign &node) {
    const Position pos{node.pos_};

    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    require_not_null(node.target_, pos), check_lvalue(*node.target_);
    check_not_null(node.value_, pos);

    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeCompoundAssign &node) {
    const Position pos{node.pos_};

    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    require_not_null(node.target_, pos), check_lvalue_pure(*node.target_);
    check_not_null(node.value_, pos);

    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeCall &node) {
    const Context saved{ctx_};

    ctx_.can_star = false;
    ctx_.can_double_star = false;
    check(*node.object_);

    bool has_seen_double_star{false};
    // 位置组
    ctx_.can_star = true;
    ctx_.can_double_star = true;
    for (const auto &arg : node.args_) {
        if (dynamic_cast<const AstNodeDoubleStar *>(arg.get())) has_seen_double_star = true;
        else if (has_seen_double_star) error("argument after ** spread", arg->pos_);
        check(*arg);
    }
    // 关键字组
    ctx_.can_star = false;
    ctx_.can_double_star = false;
    for (const auto &val : node.kwargs_ | std::views::values) check(*val);

    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeIndex &node) {
    const Position pos{node.pos_};

    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    check_not_null(node.object_, pos);

    ctx_.can_star = true;
    for (const auto &a : node.args_) check_not_null(a, pos);

    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeAttr &node) {
    const Position pos{node.pos_};

    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    check_not_null(node.object_, pos);
    require_not_null(node.attr_, pos);

    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeIdentifier &node) {
    require_not_null(node.identifier_, node.pos_);
}

void SyntaxChecker::check(const AstNodeDel &node) {
    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    check_not_null(node.target_, node.pos_);

    // target 必须是标识符或属性访问
    if (!dynamic_cast<const AstNodeIdentifier *>(node.target_.get()) &&
        !dynamic_cast<const AstNodeAttr *>(node.target_.get()))
        error("del target must be an identifier or attribute access", node.target_->pos_);

    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeGlobal &node) {
    if (ctx_.local_scope_depth == 0) error("global outside function/class body", node.pos_);
}

void SyntaxChecker::check_not_null(const AstNodePtr &node, const Position pos) {
    if (!node) error("Bad AstNode: unexpected null node", pos);
    check(*node);
}

void SyntaxChecker::check_not_null(const AstNodeProgramPtr &node, const Position pos) {
    if (!node) error("Bad AstNode: unexpected null node", pos);
    check(*node);
}

void SyntaxChecker::check_nullable(const AstNodePtr &node) {
    if (node) check(*node);
}

void SyntaxChecker::check_lvalue(const AstNode &node) {
    // a  a[ind]  a.x
    if (dynamic_cast<const AstNodeIdentifier *>(&node) ||
        dynamic_cast<const AstNodeIndex *>(&node) ||
        dynamic_cast<const AstNodeAttr *>(&node)) {
        return check(node);
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

void SyntaxChecker::check_lvalue_items(const std::vector<AstNodePtr> &items, const Position pos) {
    bool has_seen_star{false};
    for (const auto &item : items) {
        require_not_null(item, pos);
        if (const auto *star{dynamic_cast<const AstNodeStar *>(item.get())}) {
            // 解构时至多一个左值可以带 * 前缀
            if (has_seen_star) error("at most one starred lvalue allowed in destructuring", star->pos_);
            has_seen_star = true;
            require_not_null(star->operand_, pos), check_lvalue(*star->operand_);
        } else {
            check_lvalue(*item);
        }
    }
}

void SyntaxChecker::check_lvalue_pure(const AstNode &node) {
    // a  a[ind]  a.x
    if (dynamic_cast<const AstNodeIdentifier *>(&node) ||
        dynamic_cast<const AstNodeIndex *>(&node) ||
        dynamic_cast<const AstNodeAttr *>(&node)) {
        return check(node);
    }

    error("identifier, attribute access, or index expression expected before op=", node.pos_);
}

void SyntaxChecker::check_doc(const AstNodePtr &doc) const {
    if (doc && !dynamic_cast<const AstNodeLiteralStr *>(doc.get())) error("doc must be a string literal", doc->pos_);
}

SyntaxChecker::SyntaxChecker(const AstNodeProgram &root, std::string file_path)
    : root_{root}, file_path_{std::move(file_path)} {
}

void SyntaxChecker::check() && {
    check(root_);
}
