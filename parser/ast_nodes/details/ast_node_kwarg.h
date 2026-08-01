#pragma once

#include "ast_node.h"

#include <string>

// ============================================================
// 关键字实参：函数调用、import 调用形态共用
// ============================================================

// 关键字组的一项：identifier = expr（Keyword）或 **expr（DoubleStar）
struct OneKwArg {
    enum class Kind { Keyword, DoubleStar } kind_;

    std::u32string keyword_; // 仅 Kind::Keyword 时有意义
    AstNodePtr value_;
};
