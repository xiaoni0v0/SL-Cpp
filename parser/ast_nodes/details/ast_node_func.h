#pragma once

#include "ast_node.h"
#include "ast_node_capture.h"
#include "ast_node_multi_exprs.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

// ============================================================
// 函数定义
// ============================================================

// ⟦@decorator ...⟧ func ⟦name⟧ ⟦[captures]⟧ (params) ⟦: return_type⟧ ⟦doc⟧ { body }
struct AstNodeFunc : AstNode {

    // 单个形参：identifier ⟦: type⟧ ⟦= expr⟧（*args/**kwargs 各自只有一个裸标识符，不需要这个结构）
    struct OneParam {
        std::u32string identifier_;
        AstNodePtr type_annotation_; // 类型注解，nullptr 表示无类型注解
        AstNodePtr default_value_;   // 默认值，nullptr 表示无默认值
    };

    // 所有形参
    struct AllParams {
        // *args 之前的形参
        std::vector<OneParam> positional_;
        // *identifier，nullopt 表示没有
        std::optional<std::u32string> var_args_name_;
        // *args 之后、**kwargs 之前的形参
        std::vector<OneParam> kw_only_;
        // **identifier，nullopt 表示没有
        std::optional<std::u32string> var_kwargs_name_;
    };

    std::vector<AstNodePtr> decorators_;        // 可空
    std::vector<Position> decorator_positions_; // 每个装饰器自己的 '@' 位置
    std::optional<std::u32string> name_;        // nullopt 表示匿名函数
    std::vector<OneCapture> captures_;          // 可空
    AllParams params_;
    AstNodePtr return_type_; // 可空
    AstNodePtr doc_;         // 可空
    AstNodeProgramPtr body_;

    explicit AstNodeFunc(
        const Position pos, std::vector<AstNodePtr> decorators,
        std::vector<Position> decorator_positions, std::optional<std::u32string> name,
        std::vector<OneCapture> captures, AllParams params, AstNodePtr return_type, AstNodePtr doc,
        AstNodeProgramPtr body
    )
        : AstNode{pos}, decorators_{std::move(decorators)},
          decorator_positions_{std::move(decorator_positions)}, name_{std::move(name)},
          captures_{std::move(captures)}, params_{std::move(params)},
          return_type_{std::move(return_type)}, doc_{std::move(doc)}, body_{std::move(body)} {}

private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};
