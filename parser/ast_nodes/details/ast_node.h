#pragma once

#include "../ast_visitor.h"

#include <compare>
#include <memory>
#include <nlohmann/json.hpp>

using json = nlohmann::ordered_json;

// 源码里的一个位置（行、列）
struct Position {
    int row;
    int col;
    std::strong_ordering operator<=>(const Position &) const = default;
};

// AST 节点的基类
struct AstNode {
    // 开始的位置
    Position pos_;

    explicit AstNode(const Position pos) : pos_{pos} {}

    virtual ~AstNode() = default;

    /**
     * 序列化为 JSON
     * @param include_pos 是否把节点自身的位置信息也 dump 进去
     */
    [[nodiscard]] json to_json(const bool include_pos = false) const {
        return to_json_impl(include_pos);
    }

    // 双分派的前半程
    virtual void accept(AstVisitor &visitor) = 0;
    virtual void accept(AstConstVisitor &visitor) const = 0;

  private:
    [[nodiscard]] virtual json to_json_impl(bool include_pos) const = 0;
};

using AstNodePtr = std::unique_ptr<AstNode>;
