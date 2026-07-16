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
    std::vector<Position> decorator_positions_; // 每个装饰器自己的位置，跟 decorators_ 一一对应
    std::optional<std::u32string> name_; // nullopt 表示匿名类
    std::vector<AstNodePtr> bases_; // 可空
    AstNodePtr doc_; // nullptr 表示无文档字符串
    AstNodeProgramPtr body_; // 类体

    explicit AstNodeClass(const Position pos,
                          std::vector<AstNodePtr> decorators,
                          std::vector<Position> decorator_positions,
                          std::optional<std::u32string> name,
                          std::vector<AstNodePtr> bases,
                          AstNodePtr doc,
                          AstNodeProgramPtr body)
        : AstNode{pos}, decorators_{std::move(decorators)}, decorator_positions_{std::move(decorator_positions)},
          name_{std::move(name)}, bases_{std::move(bases)}, doc_{std::move(doc)}, body_{std::move(body)} {
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
