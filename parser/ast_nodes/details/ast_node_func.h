#pragma once

#include "ast_node.h"
#include "ast_node_multi_exprs.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

// ============================================================
// 函数定义
// ============================================================

// func [name] (params) { body }
struct AstNodeFunc : AstNode {

    // 单个形参
    struct Param {
        std::u32string identifier;
        AstNodePtr type_annotation; // 类型注解，nullptr 表示无类型注解
        AstNodePtr default_value; // 默认值，nullptr 表示无默认值
        enum class ParamType { Normal, StarArgs, DoubleStarKwargs } param_type; // identifier / *identifier / **identifier
    };

    std::optional<std::u32string> name_; // nullopt 表示匿名函数
    std::vector<Param> params_;
    AstNodeProgramPtr body_;

    AstNodeFunc(const int row, const int col,
                std::optional<std::u32string> name,
                std::vector<Param> params,
                AstNodeProgramPtr body)
        : AstNode{row, col}, name_{std::move(name)}, params_{std::move(params)}, body_{std::move(body)} {
    }
};
