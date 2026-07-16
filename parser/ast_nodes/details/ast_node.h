#pragma once

#include <memory>
#include <nlohmann/json.hpp>

using json = nlohmann::ordered_json;

// AST 节点的基类
struct AstNode {
    // 开始的行和列
    int row_;
    int col_;

    AstNode(const int row, const int col) : row_{row}, col_{col} {
    }

    virtual ~AstNode() = default;

    // 序列化为 JSON，用于 dump
    [[nodiscard]] virtual json to_json() const = 0;
};

using AstNodePtr = std::unique_ptr<AstNode>;
