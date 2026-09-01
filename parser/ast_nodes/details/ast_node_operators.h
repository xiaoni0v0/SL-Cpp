#pragma once

#include "ast_node.h"

#include <memory>
#include <utility>
#include <vector>

// ============================================================
// 运算符
// ============================================================

// *expr（解包目标 / 实参展开，不包括可变长位置形参）
struct AstNodeStar : AstNode {
    AstNodePtr operand_;

    explicit AstNodeStar(const Position pos, AstNodePtr operand)
        : AstNode{pos}, operand_{std::move(operand)} {}

    SL_AST_NODE_ACCEPT

  private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};

// **expr（字典展开，不包括可变长关键字形参）
struct AstNodeDoubleStar : AstNode {
    AstNodePtr operand_;

    explicit AstNodeDoubleStar(const Position pos, AstNodePtr operand)
        : AstNode{pos}, operand_{std::move(operand)} {}

    SL_AST_NODE_ACCEPT

  private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};

// 一元运算符：+x  -x  ~x  not x  x?  x!
struct AstNodeOpUnary : AstNode {
    enum class OpType {
        // 160
        Question, // x?
        Exclaim,  // x!
        // 140
        Pos,       // +x
        Neg,       // -x
        BitInvert, // ~x
        // 40
        Not // not x
    };

    OpType op_;
    AstNodePtr operand_;
    Position pos_op_; // 运算符自己的位置

    explicit AstNodeOpUnary(
        const Position pos, const OpType op, AstNodePtr operand, const Position pos_op
    )
        : AstNode{pos}, op_{op}, operand_{std::move(operand)}, pos_op_{pos_op} {}

    SL_AST_NODE_ACCEPT

  private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};

// 二元运算符：+  -  *  /  //  %  **  &  |  ^  <<  >>  and  or  ..  in
struct AstNodeOpBinary : AstNode {
    enum class OpType {
        // 算术
        Add,
        Sub,
        Mul,
        Div,
        DivFloor,
        Mod,
        Pow,
        // 位运算
        BitAnd,
        BitOr,
        BitXor,
        LShift,
        RShift,
        // 逻辑
        And,
        Or,
        // ..
        Range,
        // 成员测试
        In
    };

    OpType op_;
    AstNodePtr left_, right_;
    Position pos_op_; // 运算符自己的位置

    explicit AstNodeOpBinary(
        const Position pos, const OpType op, AstNodePtr left, AstNodePtr right,
        const Position pos_op
    )
        : AstNode{pos}, op_{op}, left_{std::move(left)}, right_{std::move(right)}, pos_op_{pos_op} {
    }

    SL_AST_NODE_ACCEPT

  private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};

// 比较运算：a OP1 b ⟦OP2 c ...⟧（== != < <= > >= 一组）
struct AstNodeCompare : AstNode {
    enum class OpType { Lt, Le, Gt, Ge, Eq, Ne };

    // operands_.size() == ops_.size() + 1，operands_.size() >= 2
    std::vector<OpType> ops_;
    std::vector<AstNodePtr> operands_;
    // 链中每个运算符自己的位置，跟 ops_ 一一对应
    std::vector<Position> positions_op_;

    explicit AstNodeCompare(
        const Position pos, std::vector<OpType> ops, std::vector<AstNodePtr> operands,
        std::vector<Position> positions_op
    )
        : AstNode{pos}, ops_{std::move(ops)}, operands_{std::move(operands)},
          positions_op_{std::move(positions_op)} {}

    SL_AST_NODE_ACCEPT

  private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};

// a is b ⟦is c ...⟧（is）
struct AstNodeIs : AstNode {
    // operands_.size() >= 2
    std::vector<AstNodePtr> operands_;
    // 链中每个 'is' 自己的位置，positions_op_.size() == operands_.size() - 1
    std::vector<Position> positions_op_;

    explicit AstNodeIs(
        const Position pos, std::vector<AstNodePtr> operands, std::vector<Position> positions_op
    )
        : AstNode{pos}, operands_{std::move(operands)}, positions_op_{std::move(positions_op)} {}

    SL_AST_NODE_ACCEPT

  private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};

// target = expr
struct AstNodeAssign : AstNode {
    AstNodePtr target_;
    AstNodePtr value_;

    explicit AstNodeAssign(const Position pos, AstNodePtr target, AstNodePtr value)
        : AstNode{pos}, target_{std::move(target)}, value_{std::move(value)} {}

    SL_AST_NODE_ACCEPT

  private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};

// target op= expr
// （op 为算术/位运算，由 Parser::assign_compound_to_binary 的白名单保证，不是语义层校验的）
struct AstNodeCompoundAssign : AstNode {
    AstNodePtr target_;
    AstNodeOpBinary::OpType op_;
    AstNodePtr value_;
    // op= 这个运算符自己的位置（不同于 pos_，后者是整个赋值表达式的起始位置，即 target_
    // 的起始位置）
    Position pos_op_;

    explicit AstNodeCompoundAssign(
        const Position pos, AstNodePtr target, const AstNodeOpBinary::OpType op, AstNodePtr value,
        const Position pos_op
    )
        : AstNode{pos}, target_{std::move(target)}, op_{op}, value_{std::move(value)},
          pos_op_{pos_op} {}

    SL_AST_NODE_ACCEPT

  private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};
