#pragma once

#include "../../../utils/string_utils.h"
#include "ast_node.h"

#include <string>
#include <vector>

// ============================================================
// 捕获列表：func、class 共用
// ============================================================

// 单个捕获项：identifier（值捕获，读当前值） / identifier = expr（值捕获，读 expr） /
// &identifier（引用捕获）
struct OneCapture {
    enum class CaptureType { Value, Reference } capture_type_;

    std::u32string identifier_;
    AstNodePtr value_expr_; // 仅 CaptureType::Value 且显式写了 "= expr"
                            // 时非空；裸标识符或引用捕获均为 nullptr
};

json captures_to_json(const std::vector<OneCapture> &captures);
