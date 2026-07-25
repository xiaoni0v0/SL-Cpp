#pragma once

#include "ast_node.h"

#include <memory>
#include <string>
#include <vector>

// None
struct AstNodeLiteralNone : AstNode {
    explicit AstNodeLiteralNone(const Position pos) : AstNode{pos} {}

  private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};

// bool
struct AstNodeLiteralBool : AstNode {
    bool value_;

    explicit AstNodeLiteralBool(const Position pos, const bool value)
        : AstNode{pos}, value_{value} {}

  private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};

struct AstNodeLiteralGL : AstNode {
    enum class GLType { G, L } value_;

    explicit AstNodeLiteralGL(const Position pos, const GLType value)
        : AstNode{pos}, value_{value} {}

  private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};

// int
struct AstNodeLiteralInt : AstNode {
    std::u32string raw_;

    explicit AstNodeLiteralInt(const Position pos, std::u32string raw)
        : AstNode{pos}, raw_{std::move(raw)} {}

  private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};

// float
struct AstNodeLiteralFloat : AstNode {
    std::u32string raw_;

    explicit AstNodeLiteralFloat(const Position pos, std::u32string raw)
        : AstNode{pos}, raw_{std::move(raw)} {}

  private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};

// str
struct AstNodeLiteralStr : AstNode {
    // Lexer 已处理转义，value 为最终字符串内容
    std::u32string value_;

    explicit AstNodeLiteralStr(const Position pos, std::u32string value)
        : AstNode{pos}, value_{std::move(value)} {}

  private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};

// (1, 2, 3)，单元素元组须有尾逗号
struct AstNodeLiteralTuple : AstNode {
    std::vector<AstNodePtr> items_; // 可空

    explicit AstNodeLiteralTuple(const Position pos, std::vector<AstNodePtr> items)
        : AstNode{pos}, items_{std::move(items)} {}

  private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};

// [1, 2, 3]
struct AstNodeLiteralList : AstNode {
    std::vector<AstNodePtr> items_; // 可空

    explicit AstNodeLiteralList(const Position pos, std::vector<AstNodePtr> items)
        : AstNode{pos}, items_{std::move(items)} {}

  private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};

// {'k1': 'v1', 'k2': 'v2'}
struct AstNodeLiteralDict : AstNode {
    std::vector<std::pair<AstNodePtr, AstNodePtr>> items_; // 可空

    explicit AstNodeLiteralDict(
        const Position pos, std::vector<std::pair<AstNodePtr, AstNodePtr>> items
    )
        : AstNode{pos}, items_{std::move(items)} {}

  private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};

// ...
struct AstNodeLiteralEllipsis : AstNode {
    explicit AstNodeLiteralEllipsis(const Position pos) : AstNode{pos} {}

  private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};
