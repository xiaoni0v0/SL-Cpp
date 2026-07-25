#pragma once

#include "ast_node.h"

#include <string>

// ============================================================
// 捕获列表：func、class 共用
// ============================================================

// 单个捕获项：identifier（值捕获，读当前值） / identifier = expr（值捕获，读 expr） /
// &identifier（引用捕获）
struct OneCapture {
    enum class CaptureType { Value, Reference } capture_type_;

    std::u32string identifier_;
    AstNodePtr value_expr_; // 仅 CaptureType::Value 且显式写了 "= expr" 时非空
};
