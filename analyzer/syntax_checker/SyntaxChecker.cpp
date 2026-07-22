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
    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    for (const auto &deco : node.decorators_) check(*deco);
    require_same_size(node.decorators_, node.decorator_positions_, node.pos_);
    for (const auto &base : node.bases_) check(*base);
    check_doc(node.doc_);

    ctx_.local_scope_depth++;
    ctx_.loop_depth = 0;
    check(*node.body_);

    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeIf &node) {
    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    // clauses_ 至少一条：语法上 if 必须有 if (cond) expr 这个基础子句，Parser 结构性保证，这里只是
    // 防御性地断言一下（万一 Parser 出 bug），不是真的有哪种源码能让这个为空
    require_not_null(node.clauses_, 1, node.pos_);
    for (const auto &clause : node.clauses_) {
        check(*clause.cond_);
        check(*clause.body_);
    }
    check_optional(node.else_expr_);
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeForCond &node) {
    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;
    check_optional(node.init_);
    check_optional(node.cond_);
    check_optional(node.inc_);
    ctx_.loop_depth++;
    check(*node.body_);
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeForIter &node) {
    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;
    check_lvalue(*node.target_);
    check(*node.iterable_);
    ctx_.loop_depth++;
    check(*node.body_);
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeBreak &node) {
    if (ctx_.loop_depth == 0) error("break outside for/while loop", node.pos_);
}

void SyntaxChecker::check(const AstNodeContinue &node) {
    if (ctx_.loop_depth == 0) error("continue outside for/while loop", node.pos_);
}

void SyntaxChecker::check(const AstNodeReturn &node) {
    // return 现在处处合法：离它最近的 Program 就是它的作用对象（SL.md 3.4.1/3.4.5.6），哪怕是文件
    // 顶层，最近的 Program 也就是文件自身，不需要判断上下文
    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;
    check_optional(node.value_);
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeTry &node) {
    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;
    if (node.except_clauses_.empty() && !node.finally_expr_)
        error("try must have at least one except or finally",
              node.pos_);
    check(*node.try_expr_);
    for (const auto &clause : node.except_clauses_) {
        for (const auto &exc : clause.exceptions_) check(*exc);
        check(*clause.body_);
    }
    check_optional(node.finally_expr_);
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeRaise &node) {
    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;
    check(*node.value_);
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeDecorator &node) {
    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;
    check(*node.decorator_);
    check(*node.target_);
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeFunc &node) {
    using PT = AstNodeFunc::OneParam::ParamType;

    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    // decorators_/decorator_positions_ 一一对应，Parser 结构性保证，防御性断言一下
    require_same_size(node.decorators_, node.decorator_positions_, node.pos_);
    for (const auto &deco : node.decorators_) check(*deco);

    // 捕获列表、形参列表内部及两者之间标识符均不可重复（2.2.6）
    std::unordered_set<std::u32string> has_seen_names;

    for (const auto &capture : node.captures_) {
        if (!has_seen_names.insert(capture.identifier_).second)
            error("duplicate name in capture/parameter list",
                  node.pos_);
        check_optional(capture.value_expr_);
    }

    // 形参顺序与重复检查
    bool has_seen_star_args{false};
    bool has_seen_double_star{false};
    bool has_seen_default{false};

    for (const auto &param : node.params_) {
        if (!has_seen_names.insert(param.identifier_).second)
            error("duplicate name in capture/parameter list",
                  node.pos_);
        if (has_seen_double_star) error("parameter after **kwargs", node.pos_);

        switch (param.param_type_) {
        case PT::Normal: if (has_seen_star_args) error("normal parameter after *args", node.pos_);
            if (param.default_value_) has_seen_default = true;
            else if (has_seen_default) error("non-default parameter after default parameter", node.pos_);
            break;
        case PT::StarArgs: if (has_seen_star_args) error("duplicate *args", node.pos_);
            has_seen_star_args = true;
            break;
        case PT::DoubleStarKwargs: has_seen_double_star = true;
            break;
        }

        check_optional(param.type_annotation_);
        check_optional(param.default_value_);
    }

    check_optional(node.return_type_);
    check_doc(node.doc_);

    // 进入函数体（新的局部作用域，loop 深度归零——函数体自己不算在循环里）
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
    for (const auto &item : node.items_) check(*item);
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeLiteralList &node) {
    const Context saved{ctx_};
    ctx_.can_star = true;
    ctx_.can_double_star = false;
    for (const auto &item : node.items_) check(*item);
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeLiteralDict &node) {
    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;
    for (const auto &[key, val] : node.items_) {
        ctx_.can_double_star = true;
        check(*key);
        ctx_.can_double_star = false;
        check_optional(val);
    }
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeLiteralEllipsis &) {
}

void SyntaxChecker::check(const AstNodeProgram &node) {
    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;
    for (const auto &e : node.exprs_) check(*e);
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeCompound &node) {
    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;
    for (const auto &e : node.exprs_) check(*e);
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeStar &node) {
    if (!ctx_.can_star) error("* can only appear in tuple, list, or function call arguments", node.pos_);
    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;
    check(*node.operand_);
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeDoubleStar &node) {
    if (!ctx_.can_double_star) error("** can only appear in dict literal or function call arguments", node.pos_);
    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;
    check(*node.operand_);
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeOpUnary &node) {
    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;
    check(*node.operand_);
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeOpBinary &node) {
    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;
    check(*node.left_);
    check(*node.right_);
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeCompare &node) {
    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    // operands_.size() == ops_.size() + 1 且 >= 2，ops_/op_positions_ 一一对应
    // （ast_node_operators.h 里的注释），Parser 结构性保证，防御性断言一下
    require_not_null(node.operands_, 2, node.pos_);
    if (node.operands_.size() != node.ops_.size() + 1) error("Bad AstNode: mismatched operands/ops count", node.pos_);
    require_same_size(node.ops_, node.op_positions_, node.pos_);

    for (const auto &operand : node.operands_) check(*operand);
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeIs &node) {
    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    // operands_.size() >= 2，op_positions_.size() == operands_.size() - 1
    // （ast_node_operators.h 里的注释），Parser 结构性保证，防御性断言一下
    require_not_null(node.operands_, 2, node.pos_);
    if (node.op_positions_.size() != node.operands_.size() - 1) error(
        "Bad AstNode: mismatched operands/op_positions count", node.pos_);

    for (const auto &operand : node.operands_) check(*operand);
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeAssign &node) {
    check_lvalue(*node.target_);
    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;
    check(*node.value_);
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeCompoundAssign &node) {
    check_lvalue_pure(*node.target_);
    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;
    check(*node.value_);
    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeCall &node) {
    const Context saved{ctx_};

    // object_
    ctx_.can_star = false;
    ctx_.can_double_star = false;
    check(*node.object_);

    // 参数们
    bool has_seen_double_star{false}; // ** 之后不能再出现其他参数
    ctx_.can_star = true;
    ctx_.can_double_star = true;
    for (const auto &arg : node.args_) {
        if (dynamic_cast<const AstNodeDoubleStar *>(arg.get())) has_seen_double_star = true;
        else if (has_seen_double_star) error("argument after ** spread", arg->pos_);
        check(*arg);
    }

    ctx_.can_star = false;
    ctx_.can_double_star = false;
    for (const auto &val : node.kwargs_ | std::views::values) check(*val);

    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeIndex &node) {
    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    // object_：语法上 x[...] 的 x 永远是解析出来的前一个表达式，Parser 结构性保证非空，
    // 这里只是防御性地断言一下（万一 Parser 出 bug）
    require_not_null(node.object_, node.pos_);
    check(*node.object_);

    // args_：同理，每个下标参数都是 Parser 循环里实际解析出来的表达式，结构性保证非空
    ctx_.can_star = true;
    for (const auto &a : node.args_) {
        require_not_null(a, node.pos_);
        check(*a);
    }

    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeAttr &node) {
    const Context saved{ctx_};
    ctx_.can_star = false;
    ctx_.can_double_star = false;

    // object_：同 AstNodeIndex，Parser 结构性保证非空
    require_not_null(node.object_, node.pos_);
    check(*node.object_);

    // attr_：语法上是 '.' 后面必须紧跟的一个 IDENTIFIER token，Lexer 的标识符正则要求至少 1 个字符，
    // Parser 用 expect(IDENTIFIER) 拿到的 text 结构性保证非空
    require_not_null(node.attr_, node.pos_);

    ctx_ = saved;
}

void SyntaxChecker::check(const AstNodeIdentifier &) {
}

void SyntaxChecker::check(const AstNodeDel &node) {
    // 2.2.3：target 必须是标识符或属性访问
    if (!dynamic_cast<const AstNodeIdentifier *>(node.target_.get()) &&
        !dynamic_cast<const AstNodeAttr *>(node.target_.get()))
        error("del target must be an identifier or attribute access", node.target_->pos_);
}

void SyntaxChecker::check(const AstNodeGlobal &node) {
    // identifier_ 语法上就是 token，没有形状可校验，只需要检查作用域限制
    // （SL.md 2.2.4/3.4.4：只能在局部作用域——函数体或类体——中使用）
    if (ctx_.local_scope_depth == 0) error("global outside function/class body", node.pos_);
}

void SyntaxChecker::check_optional(const AstNodePtr &node) {
    if (node) check(*node);
}

void SyntaxChecker::check_lvalue(const AstNode &node) const {
    // a  a[ind]  a.x
    if (dynamic_cast<const AstNodeIdentifier *>(&node)) return;
    if (dynamic_cast<const AstNodeIndex *>(&node)) return;
    if (dynamic_cast<const AstNodeAttr *>(&node)) return;

    // (a, b)  [a, b]
    if (const auto *n{dynamic_cast<const AstNodeLiteralTuple *>(&node)}) {
        return check_lvalue_items(n->items_);
    }
    if (const auto *n{dynamic_cast<const AstNodeLiteralList *>(&node)}) {
        return check_lvalue_items(n->items_);
    }

    error("lvalue expected before assignment", node.pos_);
}

void SyntaxChecker::check_lvalue_items(const std::vector<AstNodePtr> &items) const {
    bool has_seen_star{false};
    for (const auto &item : items) {
        if (const auto *star{dynamic_cast<const AstNodeStar *>(item.get())}) {
            // 解构时至多一个左值可以带 * 前缀
            if (has_seen_star) error("at most one starred lvalue allowed in destructuring", star->pos_);
            has_seen_star = true;
            check_lvalue(*star->operand_);
        } else {
            check_lvalue(*item);
        }
    }
}

void SyntaxChecker::check_lvalue_pure(const AstNode &node) const {
    // a  a[ind]  a.x（不含解构，用于复合赋值）
    if (dynamic_cast<const AstNodeIdentifier *>(&node)) return;
    if (dynamic_cast<const AstNodeIndex *>(&node)) return;
    if (dynamic_cast<const AstNodeAttr *>(&node)) return;

    error("identifier, attribute access, or index expression expected before op=", node.pos_);
}

void SyntaxChecker::check_doc(const AstNodePtr &doc) const {
    if (doc && !dynamic_cast<const AstNodeLiteralStr *>(doc.get())) error("doc must be a string literal", doc->pos_);
}

SyntaxChecker::SyntaxChecker(AstNodeProgram &root, std::string file_path)
    : root_{root}, file_path_{std::move(file_path)} {
}

void SyntaxChecker::check() && {
    check(root_);
}
