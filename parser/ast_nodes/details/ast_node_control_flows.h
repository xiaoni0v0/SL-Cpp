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

    SL_AST_NODE_ACCEPT
};

// 收集模式记号 $ / $ * / $$ / $$ **，两个 for 节点共用
struct CollectMark {
    enum class Container { None, List, Dict } container_; // 无 / $ 出 list / $$ 出 dict
    bool expand_; // 是否带 * / **（把每轮的值展开）；container_ 为 None 时必须为 false
};

// for [collect] (init cond inc) body
struct AstNodeForCond : AstNode {
    CollectMark collect_;
    AstNodePtr init_; // nullptr 表示空
    AstNodePtr cond_; // nullptr 无条件，解释器会视为 True
    AstNodePtr inc_;  // nullptr 表示空
    AstNodePtr body_;

    explicit AstNodeForCond(
        const Position pos, const CollectMark collect, AstNodePtr init, AstNodePtr cond,
        AstNodePtr inc, AstNodePtr body
    )
        : AstNode{pos}, collect_{collect}, init_{std::move(init)}, cond_{std::move(cond)},
          inc_{std::move(inc)}, body_{std::move(body)} {}

    SL_AST_NODE_ACCEPT
};

// for [collect] (iterable [as target]) body
struct AstNodeForIter : AstNode {
    CollectMark collect_;
    AstNodePtr iterable_;
    AstNodePtr target_; // nullptr 表示没有 as
    AstNodePtr body_;

    explicit AstNodeForIter(
        const Position pos, const CollectMark collect, AstNodePtr iterable, AstNodePtr target,
        AstNodePtr body
    )
        : AstNode{pos}, collect_{collect}, iterable_{std::move(iterable)},
          target_{std::move(target)}, body_{std::move(body)} {}

    SL_AST_NODE_ACCEPT
};

struct AstNodeBreak : AstNode {
    explicit AstNodeBreak(const Position pos) : AstNode{pos} {}

    SL_AST_NODE_ACCEPT
};

struct AstNodeContinue : AstNode {
    explicit AstNodeContinue(const Position pos) : AstNode{pos} {}

    SL_AST_NODE_ACCEPT
};

// return [expr]
struct AstNodeReturn : AstNode {
    AstNodePtr value_; // nullptr 表示无（等价于 return None）

    explicit AstNodeReturn(const Position pos, AstNodePtr value)
        : AstNode{pos}, value_{std::move(value)} {}

    SL_AST_NODE_ACCEPT
};

// try expr [except (Exception, ... [as target]) expr]* [finally expr]
struct AstNodeTry : AstNode {
    // except 子句，作为 AstNodeTry 的组成部分
    struct AstNodeExceptAndExpr {
        std::vector<AstNodePtr> exceptions_;
        AstNodePtr target_; // nullptr 表示没有 as
        AstNodePtr body_;

        AstNodeExceptAndExpr(std::vector<AstNodePtr> exception, AstNodePtr target, AstNodePtr body)
            : exceptions_{std::move(exception)}, target_{std::move(target)},
              body_{std::move(body)} {}
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

    SL_AST_NODE_ACCEPT
};

struct AstNodeRaise : AstNode {
    AstNodePtr value_;

    explicit AstNodeRaise(const Position pos, AstNodePtr value)
        : AstNode{pos}, value_{std::move(value)} {}

    SL_AST_NODE_ACCEPT
};
