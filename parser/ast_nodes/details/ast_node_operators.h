#pragma once

#include "ast_node.h"

#include <memory>
#include <utility>
#include <vector>

// ============================================================
// 运算符
// ============================================================

// *expr（解包目标 / 实参展开，具体含义由语义层确定）
struct AstNodeStar : AstNode {
    AstNodePtr operand_;

    AstNodeStar(const int row, const int col,
                AstNodePtr operand)
        : AstNode{row, col}, operand_{std::move(operand)} {
    }

    [[nodiscard]] json to_json() const override {
        return json{{"type", "Star"}, {"operand", operand_->to_json()}};
    }
};

// **expr（字典展开，具体含义由语义层确定）
struct AstNodeDoubleStar : AstNode {
    AstNodePtr operand_;

    AstNodeDoubleStar(const int row, const int col,
                      AstNodePtr operand)
        : AstNode{row, col}, operand_{std::move(operand)} {
    }

    [[nodiscard]] json to_json() const override {
        return json{{"type", "DoubleStar"}, {"operand", operand_->to_json()}};
    }
};

// 一元运算符：+x  -x  ~x  not x  x?  x!（不含 ++x/--x）
struct AstNodeOpUnary : AstNode {
    enum class OpType {
        // 160
        Question, // x?
        Exclaim, // x!
        // 140
        Pos, // +x
        Neg, // -x
        BitNot, // ~x
        // 40
        Not // not x
    };

    OpType op_;
    AstNodePtr operand_;

    AstNodeOpUnary(const int row, const int col,
                   const OpType op,
                   AstNodePtr operand)
        : AstNode{row, col}, op_{op}, operand_{std::move(operand)} {
    }

    [[nodiscard]] json to_json() const override {
        return json{{"type", "OpUnary"}, {"op", op_str(op_)}, {"operand", operand_->to_json()}};
    }

    [[nodiscard]] static constexpr const char *op_str(const OpType op) {
        switch (op) {
        case OpType::Question: return "?";
        case OpType::Exclaim: return "!";
        case OpType::Pos: return "+";
        case OpType::Neg: return "-";
        case OpType::BitNot: return "~";
        case OpType::Not: return "not";
        default: return "<unknown>";
        }
    }
};

// ++x  --x（优先级 170，独立于其他一元运算符：target 是被写入的左值，不是纯求值）
struct AstNodeIncDec : AstNode {
    enum class OpType { Inc, Dec };

    OpType op_;
    // target 必须是标识符/属性访问/元素访问，由语义层校验
    AstNodePtr target_;

    AstNodeIncDec(const int row, const int col,
                  const OpType op,
                  AstNodePtr target)
        : AstNode{row, col}, op_{op}, target_{std::move(target)} {
    }

    [[nodiscard]] json to_json() const override {
        return json{{"type", "IncDec"}, {"op", op_ == OpType::Inc ? "++" : "--"}, {"target", target_->to_json()}};
    }
};

// 二元运算符：+  -  *  /  //  %  **  &  |  ^  <<  >>  and  or  ..（比较运算符见 AstNodeCompare）
struct AstNodeOpBinary : AstNode {
    enum class OpType {
        // 算术
        Add, Sub, Mul, Div, DivFloor, Mod, Pow,
        // 位运算
        BitAnd, BitOr, BitXor, LShift, RShift,
        // 逻辑
        And, Or,
        // ..
        Range
    };

    OpType op_;
    AstNodePtr left_, right_;

    AstNodeOpBinary(const int row, const int col,
                    const OpType op,
                    AstNodePtr left,
                    AstNodePtr right)
        : AstNode{row, col}, op_{op}, left_{std::move(left)}, right_{std::move(right)} {
    }

    [[nodiscard]] json to_json() const override {
        return json{{"type", "OpBinary"}, {"op", op_str(op_)}, {"left", left_->to_json()},
                    {"right", right_->to_json()}};
    }

    // AstNodeCompoundAssign 复用同一个 OpType，也复用这个字符串化
    [[nodiscard]] static constexpr const char *op_str(const OpType op) {
        switch (op) {
        case OpType::Add: return "+";
        case OpType::Sub: return "-";
        case OpType::Mul: return "*";
        case OpType::Div: return "/";
        case OpType::DivFloor: return "//";
        case OpType::Mod: return "%";
        case OpType::Pow: return "**";
        case OpType::BitAnd: return "&";
        case OpType::BitOr: return "|";
        case OpType::BitXor: return "^";
        case OpType::LShift: return "<<";
        case OpType::RShift: return ">>";
        case OpType::And: return "and";
        case OpType::Or: return "or";
        case OpType::Range: return "..";
        default: return "<unknown>";
        }
    }
};

// 比较运算：a OP1 b ⟦OP2 c ...⟧（== != < <= > >= 一组，is 自成一组，组间不可链式）
// 单个比较（operands_.size() == 2）也用这个节点表示，不与 AstNodeOpBinary 重复表达同一种语义
struct AstNodeCompare : AstNode {
    enum class OpType { Lt, Le, Gt, Ge, Eq, Ne, Is };

    // operands_.size() == ops_.size() + 1，operands_.size() >= 2
    std::vector<AstNodePtr> operands_;
    std::vector<OpType> ops_;

    AstNodeCompare(const int row, const int col,
                   std::vector<AstNodePtr> operands,
                   std::vector<OpType> ops)
        : AstNode{row, col}, operands_{std::move(operands)}, ops_{std::move(ops)} {
    }

    [[nodiscard]] json to_json() const override {
        auto operands = json::array();
        for (const auto &operand : operands_) operands.push_back(operand->to_json());
        auto ops = json::array();
        for (const auto &op : ops_) ops.push_back(op_str(op));
        return json{{"type", "Compare"}, {"operands", std::move(operands)}, {"ops", std::move(ops)}};
    }

    [[nodiscard]] static constexpr const char *op_str(const OpType op) {
        switch (op) {
        case OpType::Lt: return "<";
        case OpType::Le: return "<=";
        case OpType::Gt: return ">";
        case OpType::Ge: return ">=";
        case OpType::Eq: return "==";
        case OpType::Ne: return "!=";
        case OpType::Is: return "is";
        default: return "<unknown>";
        }
    }
};

// target = expr
struct AstNodeAssign : AstNode {
    AstNodePtr target_;
    AstNodePtr value_;

    AstNodeAssign(const int row, const int col,
                  AstNodePtr target,
                  AstNodePtr value)
        : AstNode{row, col}, target_{std::move(target)}, value_{std::move(value)} {
    }

    [[nodiscard]] json to_json() const override {
        return json{{"type", "Assign"}, {"target", target_->to_json()}, {"value", value_->to_json()}};
    }
};

// target op= expr（op 为算术/位运算，由语义层校验）
struct AstNodeCompoundAssign : AstNode {
    AstNodePtr target_;
    AstNodeOpBinary::OpType op_;
    AstNodePtr value_;

    AstNodeCompoundAssign(const int row, const int col,
                          AstNodePtr target,
                          const AstNodeOpBinary::OpType op,
                          AstNodePtr value)
        : AstNode{row, col}, target_{std::move(target)}, op_{op}, value_{std::move(value)} {
    }

    [[nodiscard]] json to_json() const override {
        return json{
            {"type", "CompoundAssign"}, {"target", target_->to_json()},
            {"op", AstNodeOpBinary::op_str(op_)}, {"value", value_->to_json()}
        };
    }
};
