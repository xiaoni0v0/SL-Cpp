#pragma once

#include "ast_node.h"

#include <string>
#include <vector>

// ============================================================
// 杂项：本身不是 AstNode、但被多个节点类型共用的小聚合体
// ============================================================

// 单个捕获项：identifier / identifier = expr / &identifier
// func、class 共用
struct OneCapture {
    enum class CaptureType { Value, Reference } capture_type_;

    std::u32string identifier_;
    AstNodePtr value_expr_; // 仅 CaptureType::Value 且显式写了 "= expr" 时非空
};

// 关键字组的一项：identifier = expr 或 **expr
// 普通函数调用、import 调用形态、eval 共用
struct OneKwArg {
    enum class Kind { Keyword, DoubleStar } kind_;

    std::u32string keyword_; // 仅 Kind::Keyword 时有意义
    AstNodePtr value_;
};

// 一次调用的实参形状
struct CallArgs {
    std::vector<AstNodePtr> positional_args_; // 位置组：位置实参、*expr 展开，按书写顺序
    std::vector<OneKwArg> keyword_args_;      // 关键字组：关键字实参、**expr 展开，按书写顺序
    Position paren_pos_;                      // '(' 自己的位置
};
