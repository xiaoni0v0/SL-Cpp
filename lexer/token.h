#pragma once

#include <string>
#include <utility>

/**
 * token 类型枚举
 */
enum class TokenType {
#define X(name) name,
#include "x_token_type.h"

#undef X
};

/**
 * 代表一个 token
 */
struct Token {
    // token 类型
    TokenType type;
    // token 开始的字符所在的行和列
    int row;
    int col;
    // 该 token 的原始字符串
    // str 字面量除外，它存的是 str 的值
    std::u32string lexeme;

    Token(const TokenType t, const int r, const int c, std::u32string l)
        : type{t}, row{r}, col{c}, lexeme{std::move(l)} {}
};
