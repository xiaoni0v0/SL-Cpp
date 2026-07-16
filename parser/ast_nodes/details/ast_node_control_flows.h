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
            : cond_{std::move(cond)}, body_{std::move(body)} {
        }
    };

    std::vector<AstNodeCondAndExpr> clauses_; // 非空
    AstNodePtr else_expr_; // nullptr 表示无 else

    AstNodeIf(const int row, const int col,
              std::vector<AstNodeCondAndExpr> clauses,
              AstNodePtr else_expr)
        : AstNode{row, col}, clauses_{std::move(clauses)}, else_expr_{std::move(else_expr)} {
    }

    [[nodiscard]] json to_json() const override {
        auto clauses = json::array();
        for (const auto &clause : clauses_)
            clauses.push_back({{"cond", clause.cond_->to_json()},
                               {"body", clause.body_->to_json()}});
        return json{
            {"type", "If"}, {"clauses", std::move(clauses)},
            {"else_expr", else_expr_ ? else_expr_->to_json() : json(nullptr)}
        };
    }
};

// for [$] (init cond inc) body
// while [$] (cond) body 等价于 init_/inc_ 均为空的这种形式，语法层直接复用本节点
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

    [[nodiscard]] json to_json() const override {
        return json{
            {"type", "ForCond"}, {"collect", collect_},
            {"init", init_ ? init_->to_json() : json(nullptr)},
            {"cond", cond_ ? cond_->to_json() : json(nullptr)},
            {"inc", inc_ ? inc_->to_json() : json(nullptr)},
            {"body", body_->to_json()}
        };
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

    [[nodiscard]] json to_json() const override {
        return json{
            {"type", "ForIter"}, {"collect", collect_},
            {"target", target_->to_json()}, {"iterable", iterable_->to_json()}, {"body", body_->to_json()}
        };
    }
};

struct AstNodeBreak : AstNode {
    AstNodeBreak(const int row, const int col)
        : AstNode{row, col} {
    }

    [[nodiscard]] json to_json() const override {
        return json{{"type", "Break"}};
    }
};

struct AstNodeContinue : AstNode {
    AstNodeContinue(const int row, const int col)
        : AstNode{row, col} {
    }

    [[nodiscard]] json to_json() const override {
        return json{{"type", "Continue"}};
    }
};

// return [expr]
struct AstNodeReturn : AstNode {
    AstNodePtr value_; // nullptr 表示无（等价于 return None）

    AstNodeReturn(const int row, const int col,
                  AstNodePtr value)
        : AstNode{row, col}, value_{std::move(value)} {
    }

    [[nodiscard]] json to_json() const override {
        return json{{"type", "Return"}, {"value", value_ ? value_->to_json() : json(nullptr)}};
    }
};

// try expr [except (Exception, ...) expr]* [finally expr]
struct AstNodeTry : AstNode {
    // except 子句，作为 AstNodeTry 的组成部分
    struct AstNodeExceptAndExpr {
        std::vector<AstNodePtr> exceptions_;
        AstNodePtr body_;

        AstNodeExceptAndExpr(std::vector<AstNodePtr> exception, AstNodePtr body)
            : exceptions_{std::move(exception)}, body_{std::move(body)} {
        }
    };

    AstNodePtr try_expr_;
    // 以下两者不可同时为空
    std::vector<AstNodeExceptAndExpr> except_clauses_; // 可空
    AstNodePtr finally_expr_; // nullptr 表示无 finally

    AstNodeTry(const int row, const int col,
               AstNodePtr try_expr,
               std::vector<AstNodeExceptAndExpr> except_clauses,
               AstNodePtr finally_expr)
        : AstNode{row, col}, try_expr_{std::move(try_expr)},
          except_clauses_{std::move(except_clauses)}, finally_expr_{std::move(finally_expr)} {
    }

    [[nodiscard]] json to_json() const override {
        auto except_clauses = json::array();
        for (const auto &clause : except_clauses_) {
            auto exceptions = json::array();
            for (const auto &exc : clause.exceptions_) exceptions.push_back(exc->to_json());
            except_clauses.push_back({{"exceptions", std::move(exceptions)}, {"body", clause.body_->to_json()}});
        }
        return json{
            {"type", "Try"}, {"try_expr", try_expr_->to_json()},
            {"except_clauses", std::move(except_clauses)},
            {"finally_expr", finally_expr_ ? finally_expr_->to_json() : json(nullptr)}
        };
    }
};

struct AstNodeRaise : AstNode {
    AstNodePtr value_;

    AstNodeRaise(const int row, const int col,
                 AstNodePtr value)
        : AstNode{row, col}, value_{std::move(value)} {
    }

    [[nodiscard]] json to_json() const override {
        return json{{"type", "Raise"}, {"value", value_->to_json()}};
    }
};
