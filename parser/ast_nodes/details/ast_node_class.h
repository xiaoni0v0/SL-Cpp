#pragma once

#include "ast_node.h"
#include "ast_node_multi_exprs.h"
#include "../../../utils/string_utils.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

// ============================================================
// 类定义
// ============================================================

// ⟦@decorator ...⟧ class ⟦name⟧ ⟦(bases)⟧ ⟦doc⟧ { body }
struct AstNodeClass : AstNode {
    std::vector<AstNodePtr> decorators_; // 可空
    std::optional<std::u32string> name_; // nullopt 表示匿名类
    std::vector<AstNodePtr> bases_; // 可空
    AstNodePtr doc_; // nullptr 表示无文档字符串
    AstNodeProgramPtr body_; // 类体

    AstNodeClass(const int row, const int col,
                 std::vector<AstNodePtr> decorators,
                 std::optional<std::u32string> name,
                 std::vector<AstNodePtr> bases,
                 AstNodePtr doc,
                 AstNodeProgramPtr body)
        : AstNode{row, col}, decorators_{std::move(decorators)}, name_{std::move(name)},
          bases_{std::move(bases)}, doc_{std::move(doc)}, body_{std::move(body)} {
    }

    [[nodiscard]] json to_json() const override {
        auto decorators = json::array();
        for (const auto &d : decorators_) decorators.push_back(d->to_json());
        auto bases = json::array();
        for (const auto &base : bases_) bases.push_back(base->to_json());

        return json{
            {"type", "Class"},
            {"decorators", std::move(decorators)},
            {"name", name_ ? json(u32_to_utf8(*name_)) : json(nullptr)},
            {"bases", std::move(bases)},
            {"doc", doc_ ? doc_->to_json() : json(nullptr)},
            {"body", body_->to_json()}
        };
    }
};
