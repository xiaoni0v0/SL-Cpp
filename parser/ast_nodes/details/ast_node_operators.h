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

    explicit AstNodeStar(const Position pos,
                         AstNodePtr operand)
        : AstNode{pos}, operand_{std::move(operand)} {
    }

    [[nodiscard]] json to_json() const override {
        return json{{"type", "Star"}, {"operand", operand_->to_json()}};
    }
};

// **expr（字典展开，具体含义由语义层确定）
struct AstNodeDoubleStar : AstNode {
    AstNodePtr operand_;

    explicit AstNodeDoubleStar(const Position pos,
                               AstNodePtr operand)
        : AstNode{pos}, operand_{std::move(operand)} {
    }

    [[nodiscard]] json to_json() const override {
        return json{{"type", "DoubleStar"}, {"operand", operand_->to_json()}};
    }
};

// 一元运算符：+x  -x  ~x  not x  x?  x!
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
    // 运算符自己的位置
    Position op_pos_;

    explicit AstNodeOpUnary(const Position pos,
                            const OpType op,
                            AstNodePtr operand,
                            const Position op_pos)
        : AstNode{pos}, op_{op}, operand_{std::move(operand)}, op_pos_{op_pos} {
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
    // 运算符自己的位置
    Position op_pos_;

    explicit AstNodeOpBinary(const Position pos,
                             const OpType op,
                             AstNodePtr left,
                             AstNodePtr right,
                             const Position op_pos)
        : AstNode{pos}, op_{op}, left_{std::move(left)}, right_{std::move(right)}, op_pos_{op_pos} {
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

// 比较运算：a OP1 b ⟦OP2 c ...⟧（== != < <= > >= 一组）
struct AstNodeCompare : AstNode {
    enum class OpType { Lt, Le, Gt, Ge, Eq, Ne };

    // operands_.size() == ops_.size() + 1，operands_.size() >= 2
    std::vector<OpType> ops_;
    std::vector<AstNodePtr> operands_;
    // 链中每个运算符自己的位置，跟 ops_ 一一对应
    std::vector<Position> op_positions_;

    explicit AstNodeCompare(const Position pos,
                            std::vector<OpType> ops,
                            std::vector<AstNodePtr> operands,
                            std::vector<Position> op_positions)
        : AstNode{pos}, ops_{std::move(ops)}, operands_{std::move(operands)},
          op_positions_{std::move(op_positions)} {
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
        default: return "<unknown>";
        }
    }
};

// a is b ⟦is c ...⟧（is）
struct AstNodeIs : AstNode {
    // operands_.size() >= 2
    std::vector<AstNodePtr> operands_;
    // 链中每个 'is' 自己的位置，op_positions_.size() == operands_.size() - 1
    std::vector<Position> op_positions_;

    explicit AstNodeIs(const Position pos,
                       std::vector<AstNodePtr> operands,
                       std::vector<Position> op_positions)
        : AstNode{pos}, operands_{std::move(operands)}, op_positions_{std::move(op_positions)} {
    }

    [[nodiscard]] json to_json() const override {
        auto operands = json::array();
        for (const auto &operand : operands_) operands.push_back(operand->to_json());
        return json{{"type", "Is"}, {"operands", std::move(operands)}};
    }
};

// target = expr
struct AstNodeAssign : AstNode {
    AstNodePtr target_;
    AstNodePtr value_;

    explicit AstNodeAssign(const Position pos,
                           AstNodePtr target,
                           AstNodePtr value)
        : AstNode{pos}, target_{std::move(target)}, value_{std::move(value)} {
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
    // op= 这个运算符自己的位置（不同于 pos_，后者是整个赋值表达式的起始位置，即 target_ 的起始位置）
    Position op_pos_;

    explicit AstNodeCompoundAssign(const Position pos,
                                   AstNodePtr target,
                                   const AstNodeOpBinary::OpType op,
                                   AstNodePtr value,
                                   const Position op_pos)
        : AstNode{pos}, target_{std::move(target)}, op_{op}, value_{std::move(value)}, op_pos_{op_pos} {
    }

    [[nodiscard]] json to_json() const override {
        return json{
            {"type", "CompoundAssign"}, {"target", target_->to_json()},
            {"op", AstNodeOpBinary::op_str(op_)}, {"value", value_->to_json()}
        };
    }
};
