#pragma once

#include "ast_node.h"

#include <memory>
#include <utility>

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
};

// **expr（字典展开，具体含义由语义层确定）
struct AstNodeDoubleStar : AstNode {
    AstNodePtr operand_;

    AstNodeDoubleStar(const int row, const int col,
                      AstNodePtr operand)
        : AstNode{row, col}, operand_{std::move(operand)} {
    }
};

// 一元运算符：++x  --x  +x  -x  ~x  not x  x?  x!
struct AstNodeOpUnary : AstNode {
    enum class OpType {
        // 150
        Question, // x?
        Exclaim, // x!
        // 130
        Inc,    // ++x
        Dec,    // --x
        Pos,    // +x
        Neg,    // -x
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
};

// 二元运算符：+  -  *  /  //  %  **  &  |  ^  <<  >>  ==  !=  <  <=  >  >=  is  and  or  ..
struct AstNodeOpBinary : AstNode {
    enum class OpType {
        // 算术
        Add, Sub, Mul, Div, DivFloor, Mod, Pow,
        // 位运算
        BitAnd, BitOr, BitXor, LShift, RShift,
        // 比较
        Eq, Ne, Lt, Le, Gt, Ge, Is,
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
};
