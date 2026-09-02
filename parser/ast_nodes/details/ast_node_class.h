#pragma once

#include "ast_node.h"
#include "ast_node_misc.h"
#include "ast_node_multi_exprs.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

// ============================================================
// 类定义
// ============================================================

// ⟦@decorator ...⟧ class ⟦name⟧ ⟦(bases)⟧ ⟦[captures]⟧ ⟦doc⟧ { body }
struct AstNodeClass : AstNode {
    std::vector<AstNodePtr> decorators_;        // 可空
    std::vector<Position> positions_decorator_; // 每个装饰器自己的 '@' 位置
    std::optional<std::u32string> name_;        // nullopt 表示匿名类
    std::vector<AstNodePtr> bases_;             // 可空
    std::vector<OneCapture> captures_;          // 可空，语法/语义与 AstNodeFunc 的捕获列表完全一致
    AstNodePtr doc_;                            // nullptr 表示无文档字符串
    AstNodeProgramPtr body_;                    // 类体

    explicit AstNodeClass(
        const Position pos, std::vector<AstNodePtr> decorators,
        std::vector<Position> positions_decorator, std::optional<std::u32string> name,
        std::vector<AstNodePtr> bases, std::vector<OneCapture> captures, AstNodePtr doc,
        AstNodeProgramPtr body
    )
        : AstNode{pos}, decorators_{std::move(decorators)},
          positions_decorator_{std::move(positions_decorator)}, name_{std::move(name)},
          bases_{std::move(bases)}, captures_{std::move(captures)}, doc_{std::move(doc)},
          body_{std::move(body)} {}

    SL_AST_NODE_ACCEPT
};
