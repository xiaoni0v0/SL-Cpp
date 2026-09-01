#pragma once

#include "ast_node.h"

#include <memory>
#include <string>
#include <vector>

// None
struct AstNodeLiteralNone : AstNode {
    explicit AstNodeLiteralNone(const Position pos) : AstNode{pos} {}

    SL_AST_NODE_ACCEPT

  private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};

// bool
struct AstNodeLiteralBool : AstNode {
    bool value_;

    explicit AstNodeLiteralBool(const Position pos, const bool value)
        : AstNode{pos}, value_{value} {}

    SL_AST_NODE_ACCEPT

  private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};

struct AstNodeLiteralGL : AstNode {
    enum class GLType { G, L } value_;

    explicit AstNodeLiteralGL(const Position pos, const GLType value)
        : AstNode{pos}, value_{value} {}

    SL_AST_NODE_ACCEPT

  private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};

// int。raw_ 的形状在构造时校验，不合法即 InternalError
struct AstNodeLiteralInt : AstNode {
    const std::u32string raw_;

    explicit AstNodeLiteralInt(Position pos, std::u32string raw);

    SL_AST_NODE_ACCEPT

  private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};

// decimal。raw_ 的形状在构造时校验，不合法即 InternalError
struct AstNodeLiteralDecimal : AstNode {
    const std::u32string raw_;

    explicit AstNodeLiteralDecimal(Position pos, std::u32string raw);

    SL_AST_NODE_ACCEPT

  private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};

// str
struct AstNodeLiteralStr : AstNode {
    // Lexer 已处理转义，value 为最终字符串内容
    std::u32string value_;

    explicit AstNodeLiteralStr(const Position pos, std::u32string value)
        : AstNode{pos}, value_{std::move(value)} {}

    SL_AST_NODE_ACCEPT

  private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};

// (1, 2, 3)，单元素元组须有尾逗号
struct AstNodeLiteralTuple : AstNode {
    std::vector<AstNodePtr> items_; // 可空

    explicit AstNodeLiteralTuple(const Position pos, std::vector<AstNodePtr> items)
        : AstNode{pos}, items_{std::move(items)} {}

    SL_AST_NODE_ACCEPT

  private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};

// [1, 2, 3]
struct AstNodeLiteralList : AstNode {
    std::vector<AstNodePtr> items_; // 可空

    explicit AstNodeLiteralList(const Position pos, std::vector<AstNodePtr> items)
        : AstNode{pos}, items_{std::move(items)} {}

    SL_AST_NODE_ACCEPT

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

    SL_AST_NODE_ACCEPT

  private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};

// ...
struct AstNodeLiteralEllipsis : AstNode {
    explicit AstNodeLiteralEllipsis(const Position pos) : AstNode{pos} {}

    SL_AST_NODE_ACCEPT

  private:
    [[nodiscard]] json to_json_impl(bool include_pos) const override;
};

// 剥掉开头可能有的负号
[[nodiscard]] std::u32string_view strip_literal_sign(std::u32string_view raw);

// 数字字面量科学计数法后缀 `[eE][+-]?digits` 的拆分结果
struct LiteralExponentSplit {
    std::u32string_view mantissa; // e/E 前面的部分；没有该后缀时就是整个输入
    std::u32string_view exponent; // 指数的数字部分（不含符号）；没有该后缀时为空
};

/**
 * 校验并拆出末尾的科学计数法后缀
 * @param allow_negative_exponent 指数能不能带负号（int 不行，decimal 行）
 * @param max_exponent_digits     指数位数上限，0 表示不限
 */
[[nodiscard]] LiteralExponentSplit strip_literal_exponent(
    std::u32string_view raw, bool allow_negative_exponent, size_t max_exponent_digits, Position pos
);
