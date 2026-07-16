#pragma once

#include "ast_node.h"
#include "../../../utils/string_utils.h"

#include <memory>
#include <string>
#include <vector>


// None
struct AstNodeLiteralNone : AstNode {
    AstNodeLiteralNone(const int row, const int col)
        : AstNode{row, col} {
    }

    [[nodiscard]] json to_json() const override {
        return json{{"type", "LiteralNone"}};
    }
};

// bool
struct AstNodeLiteralBool : AstNode {
    bool value_;

    AstNodeLiteralBool(const int row, const int col,
                       const bool value)
        : AstNode{row, col}, value_{value} {
    }

    [[nodiscard]] json to_json() const override {
        json j{{"type", "LiteralBool"}};
        j["value"] = value_;
        return j;
    }
};

struct AstNodeLiteralGL : AstNode {
    enum class GLType { G, L } value_;

    AstNodeLiteralGL(const int row, const int col,
                     const GLType value)
        : AstNode{row, col}, value_{value} {
    }

    [[nodiscard]] json to_json() const override {
        json j{{"type", "LiteralGL"}};
        j["value"] = (value_ == GLType::G) ? "_G" : "_L";
        return j;
    }
};

// int
struct AstNodeLiteralInt : AstNode {
    // 保留原文，解释器按需转换（"123" -> 123）
    std::u32string raw_;

    AstNodeLiteralInt(const int row, const int col,
                      std::u32string raw)
        : AstNode{row, col}, raw_{std::move(raw)} {
    }

    [[nodiscard]] json to_json() const override {
        json j{{"type", "LiteralInt"}};
        j["raw"] = u32_to_utf8(raw_);
        return j;
    }
};

// float
struct AstNodeLiteralFloat : AstNode {
    std::u32string raw_;

    AstNodeLiteralFloat(const int row, const int col,
                        std::u32string raw)
        : AstNode{row, col}, raw_{std::move(raw)} {
    }

    [[nodiscard]] json to_json() const override {
        json j{{"type", "LiteralFloat"}};
        j["raw"] = u32_to_utf8(raw_);
        return j;
    }
};

// str
struct AstNodeLiteralStr : AstNode {
    // Lexer 已处理转义，value 为最终字符串内容
    std::u32string value_;

    AstNodeLiteralStr(const int row, const int col, std::u32string value)
        : AstNode{row, col}, value_{std::move(value)} {
    }

    [[nodiscard]] json to_json() const override {
        json j{{"type", "LiteralStr"}};
        j["value"] = u32_to_utf8(value_);
        return j;
    }
};

// (1, 2, 3)，单元素元组须有尾逗号
struct AstNodeLiteralTuple : AstNode {
    std::vector<AstNodePtr> items_; // 可空

    AstNodeLiteralTuple(const int row, const int col, std::vector<AstNodePtr> items)
        : AstNode{row, col}, items_{std::move(items)} {
    }

    [[nodiscard]] json to_json() const override {
        json j{{"type", "LiteralTuple"}};
        auto items = json::array();
        for (const auto &item : items_) items.push_back(item->to_json());
        j["items"] = std::move(items);
        return j;
    }
};

// [1, 2, 3]
struct AstNodeLiteralList : AstNode {
    std::vector<AstNodePtr> items_; // 可空

    AstNodeLiteralList(const int row, const int col,
                       std::vector<AstNodePtr> items)
        : AstNode{row, col}, items_{std::move(items)} {
    }

    [[nodiscard]] json to_json() const override {
        json j{{"type", "LiteralList"}};
        auto items = json::array();
        for (const auto &item : items_) items.push_back(item->to_json());
        j["items"] = std::move(items);
        return j;
    }
};

// ['k1': 'v1', 'k2': 'v2']
struct AstNodeLiteralDict : AstNode {
    std::vector<std::pair<AstNodePtr, AstNodePtr>> items_; // 可空

    AstNodeLiteralDict(const int row, const int col,
                       std::vector<std::pair<AstNodePtr, AstNodePtr>> items)
        : AstNode{row, col}, items_{std::move(items)} {
    }

    [[nodiscard]] json to_json() const override {
        json j{{"type", "LiteralDict"}};
        auto items = json::array();
        for (const auto &[key, val] : items_)
            items.push_back({
                {"key", key->to_json()},
                {"val", val ? val->to_json() : json(nullptr)}
            });
        j["items"] = std::move(items);
        return j;
    }
};

// ...
struct AstNodeLiteralEllipsis : AstNode {
    AstNodeLiteralEllipsis(const int row, const int col)
        : AstNode{row, col} {
    }

    [[nodiscard]] json to_json() const override {
        return json{{"type", "LiteralEllipsis"}};
    }
};
