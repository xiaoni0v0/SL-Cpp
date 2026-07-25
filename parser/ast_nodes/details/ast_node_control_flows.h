#pragma once

#include "ast_node.h"

#include <utility>
#include <vector>

// ============================================================
// 控制流
// ============================================================

// if (cond) expr [elif (cond) expr]* [else expr]
struct AstNodeIf : AstNode {

    // if/elif 子句，作为 AstNodeIf 的组成部分
    struct AstNodeCondAndExpr {
        AstNodePtr cond_;
        AstNodePtr body_;

        AstNodeCondAndExpr(AstNodePtr cond, AstNodePtr body)
            : cond_{std::move(cond)}, body_{std::move(body)} {}
    };

    std::vector<AstNodeCondAndExpr> clauses_; // 非空
    AstNodePtr else_expr_;                    // nullptr 表示无 else

    explicit AstNodeIf(
        const Position pos, std::vector<AstNodeCondAndExpr> clauses, AstNodePtr else_expr
    )
        : AstNode{pos}, clauses_{std::move(clauses)}, else_expr_{std::move(else_expr)} {}

private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};

// for [$] (init cond inc) body
// while [$] (cond) body 等价于 init_/inc_ 均为空的这种形式，语法层直接复用本节点
struct AstNodeForCond : AstNode {
    bool collect_;    // true 表示 for $ 收集模式
    AstNodePtr init_; // nullptr 表示空
    AstNodePtr cond_; // nullptr 无条件，解释器会视为 True
    AstNodePtr inc_;  // nullptr 表示空
    AstNodePtr body_;

    explicit AstNodeForCond(
        const Position pos, const bool collect, AstNodePtr init, AstNodePtr cond, AstNodePtr inc,
        AstNodePtr body
    )
        : AstNode{pos}, collect_{collect}, init_{std::move(init)}, cond_{std::move(cond)},
          inc_{std::move(inc)}, body_{std::move(body)} {}

private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};

// for [$] (target : iterable) body（迭代模式）
// target 必须是左值（标识符/属性访问/元素访问/解构元组或列表），由语义层校验（复用 check_lvalue）
struct AstNodeForIter : AstNode {
    bool collect_; // true 表示 for $ 收集模式
    AstNodePtr target_;
    AstNodePtr iterable_;
    AstNodePtr body_;

    explicit AstNodeForIter(
        const Position pos, const bool collect, AstNodePtr target, AstNodePtr iterable,
        AstNodePtr body
    )
        : AstNode{pos}, collect_{collect}, target_{std::move(target)},
          iterable_{std::move(iterable)}, body_{std::move(body)} {}

private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};

struct AstNodeBreak : AstNode {
    explicit AstNodeBreak(const Position pos) : AstNode{pos} {}

private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};

struct AstNodeContinue : AstNode {
    explicit AstNodeContinue(const Position pos) : AstNode{pos} {}

private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};

// return [expr]
struct AstNodeReturn : AstNode {
    AstNodePtr value_; // nullptr 表示无（等价于 return None）

    explicit AstNodeReturn(const Position pos, AstNodePtr value)
        : AstNode{pos}, value_{std::move(value)} {}

private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};

// try expr [except (Exception, ...) expr]* [finally expr]
struct AstNodeTry : AstNode {
    // except 子句，作为 AstNodeTry 的组成部分
    struct AstNodeExceptAndExpr {
        std::vector<AstNodePtr> exceptions_;
        AstNodePtr body_;

        AstNodeExceptAndExpr(std::vector<AstNodePtr> exception, AstNodePtr body)
            : exceptions_{std::move(exception)}, body_{std::move(body)} {}
    };

    AstNodePtr try_expr_;
    // 以下两者不可同时为空
    std::vector<AstNodeExceptAndExpr> except_clauses_; // 可空
    AstNodePtr finally_expr_;                          // nullptr 表示无 finally

    explicit AstNodeTry(
        const Position pos, AstNodePtr try_expr, std::vector<AstNodeExceptAndExpr> except_clauses,
        AstNodePtr finally_expr
    )
        : AstNode{pos}, try_expr_{std::move(try_expr)}, except_clauses_{std::move(except_clauses)},
          finally_expr_{std::move(finally_expr)} {}

private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};

struct AstNodeRaise : AstNode {
    AstNodePtr value_;

    explicit AstNodeRaise(const Position pos, AstNodePtr value)
        : AstNode{pos}, value_{std::move(value)} {}

private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};
