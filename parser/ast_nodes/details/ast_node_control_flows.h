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
    struct AstCondAndExpr {
        AstNodePtr cond_;
        AstNodePtr body_;

        AstCondAndExpr(AstNodePtr cond, AstNodePtr body)
            : cond_{std::move(cond)}, body_{std::move(body)} {
        }
    };

    std::vector<AstCondAndExpr> clauses_; // 非空
    AstNodePtr else_expr_; // nullptr 表示无 else

    AstNodeIf(const int row, const int col,
              std::vector<AstCondAndExpr> clauses,
              AstNodePtr else_expr)
        : AstNode{row, col}, clauses_{std::move(clauses)}, else_expr_{std::move(else_expr)} {
    }
};

// for [$] (init cond inc) body（计数/条件模式）
struct AstNodeForCond : AstNode {
    bool collect_; // true 表示 for $ 收集模式
    AstNodePtr init_; // nullptr 表示空
    AstNodePtr cond_; // nullptr 无条件，解释器会视为 True
    AstNodePtr inc_; // nullptr 表示空
    AstNodePtr body_;

    AstNodeForCond(const int row, const int col,
                   const bool collect,
                   AstNodePtr init,
                   AstNodePtr cond,
                   AstNodePtr inc,
                   AstNodePtr body)
        : AstNode{row, col}, collect_{collect},
          init_{std::move(init)}, cond_{std::move(cond)}, inc_{std::move(inc)}, body_{std::move(body)} {
    }
};

// for [$] (target : iterable) body（迭代模式）
// target 必须是标识符，由语义层校验
struct AstNodeForIter : AstNode {
    bool collect_; // true 表示 for $ 收集模式
    AstNodePtr target_;
    AstNodePtr iterable_;
    AstNodePtr body_;

    AstNodeForIter(const int row, const int col,
                   const bool collect,
                   AstNodePtr target,
                   AstNodePtr iterable,
                   AstNodePtr body)
        : AstNode{row, col}, collect_{collect},
          target_{std::move(target)}, iterable_{std::move(iterable)}, body_{std::move(body)} {
    }
};

struct AstNodeBreak : AstNode {
    AstNodeBreak(const int row, const int col)
        : AstNode{row, col} {
    }
};

struct AstNodeContinue : AstNode {
    AstNodeContinue(const int row, const int col)
        : AstNode{row, col} {
    }
};

// return [expr]
struct AstNodeReturn : AstNode {
    AstNodePtr value_; // nullptr 表示无（等价于 return None）

    AstNodeReturn(const int row, const int col,
                  AstNodePtr value)
        : AstNode{row, col}, value_{std::move(value)} {
    }
};

// try expr [except (Exception, ...) expr]+ [finally expr]
struct AstNodeTry : AstNode {
    // except 子句，作为 AstNodeTry 的组成部分
    struct AstExceptAndExpr {
        std::vector<AstNodePtr> exceptions_;
        AstNodePtr body_;

        AstExceptAndExpr(std::vector<AstNodePtr> exception, AstNodePtr body)
            : exceptions_{std::move(exception)}, body_{std::move(body)} {
        }
    };

    AstNodePtr try_expr_;

    // 以下两者不可同时为空
    std::vector<AstExceptAndExpr> except_clauses_; // 可空
    AstNodePtr finally_expr_; // nullptr 表示无 finally

    AstNodeTry(const int row, const int col,
               AstNodePtr try_expr,
               std::vector<AstExceptAndExpr> except_clauses,
               AstNodePtr finally_expr)
        : AstNode{row, col}, try_expr_{std::move(try_expr)},
          except_clauses_{std::move(except_clauses)}, finally_expr_{std::move(finally_expr)} {
    }
};

struct AstNodeRaise : AstNode {
    AstNodePtr value_;

    AstNodeRaise(const int row, const int col,
                 AstNodePtr value)
        : AstNode{row, col}, value_{std::move(value)} {
    }
};
