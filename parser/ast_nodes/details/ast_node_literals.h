#pragma once

#include "../../../utils/string_utils.h"
#include "ast_node.h"

#include <memory>
#include <string>
#include <vector>

// None
struct AstNodeLiteralNone : AstNode {
    explicit AstNodeLiteralNone(const Position pos) : AstNode{pos} {}

    [[nodiscard]] json to_json() const override { return json{{"type", "LiteralNone"}}; }
};

// bool
struct AstNodeLiteralBool : AstNode {
    bool value_;

    explicit AstNodeLiteralBool(const Position pos, const bool value)
        : AstNode{pos}, value_{value} {}

    [[nodiscard]] json to_json() const override {
        return json{{"type", "LiteralBool"}, {"value", value_}};
    }
};

struct AstNodeLiteralGL : AstNode {
    enum class GLType { G, L } value_;

    explicit AstNodeLiteralGL(const Position pos, const GLType value)
        : AstNode{pos}, value_{value} {}

    [[nodiscard]] json to_json() const override {
        return json{{"type", "LiteralGL"}, {"value", value_ == GLType::G ? "_G" : "_L"}};
    }
};

// int
struct AstNodeLiteralInt : AstNode {
    std::u32string raw_;

    explicit AstNodeLiteralInt(const Position pos, std::u32string raw)
        : AstNode{pos}, raw_{std::move(raw)} {}

    [[nodiscard]] json to_json() const override {
        return json{{"type", "LiteralInt"}, {"raw", u32_to_utf8(raw_)}};
    }
};

// float
struct AstNodeLiteralFloat : AstNode {
    std::u32string raw_;

    explicit AstNodeLiteralFloat(const Position pos, std::u32string raw)
        : AstNode{pos}, raw_{std::move(raw)} {}

    [[nodiscard]] json to_json() const override {
        return json{{"type", "LiteralFloat"}, {"raw", u32_to_utf8(raw_)}};
    }
};

// str
struct AstNodeLiteralStr : AstNode {
    // Lexer 已处理转义，value 为最终字符串内容
    std::u32string value_;

    explicit AstNodeLiteralStr(const Position pos, std::u32string value)
        : AstNode{pos}, value_{std::move(value)} {}

    [[nodiscard]] json to_json() const override {
        return json{{"type", "LiteralStr"}, {"value", u32_to_utf8(value_)}};
    }
};

// (1, 2, 3)，单元素元组须有尾逗号
struct AstNodeLiteralTuple : AstNode {
    std::vector<AstNodePtr> items_; // 可空

    explicit AstNodeLiteralTuple(const Position pos, std::vector<AstNodePtr> items)
        : AstNode{pos}, items_{std::move(items)} {}

    [[nodiscard]] json to_json() const override {
        auto items = json::array();
        for (const auto &item : items_) items.push_back(item->to_json());
        return json{{"type", "LiteralTuple"}, {"items", std::move(items)}};
    }
};

// [1, 2, 3]
struct AstNodeLiteralList : AstNode {
    std::vector<AstNodePtr> items_; // 可空

    explicit AstNodeLiteralList(const Position pos, std::vector<AstNodePtr> items)
        : AstNode{pos}, items_{std::move(items)} {}

    [[nodiscard]] json to_json() const override {
        auto items = json::array();
        for (const auto &item : items_) items.push_back(item->to_json());
        return json{{"type", "LiteralList"}, {"items", std::move(items)}};
    }
};

// {'k1': 'v1', 'k2': 'v2'}
struct AstNodeLiteralDict : AstNode {
    std::vector<std::pair<AstNodePtr, AstNodePtr>> items_; // 可空

    explicit AstNodeLiteralDict(
        const Position pos, std::vector<std::pair<AstNodePtr, AstNodePtr>> items
    )
        : AstNode{pos}, items_{std::move(items)} {}

    [[nodiscard]] json to_json() const override {
        auto items = json::array();
        for (const auto &[key, val] : items_)
            items.push_back(
                {{"key", key->to_json()}, {"val", val ? val->to_json() : json(nullptr)}}
            );
        return json{{"type", "LiteralDict"}, {"items", std::move(items)}};
    }
};

// ...
struct AstNodeLiteralEllipsis : AstNode {
    explicit AstNodeLiteralEllipsis(const Position pos) : AstNode{pos} {}

    [[nodiscard]] json to_json() const override { return json{{"type", "LiteralEllipsis"}}; }
};
