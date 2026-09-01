#pragma once

// 把 Lexer 输出转成单行字符串，方便 CHECK(lex_dump(...) == "...")。

#include "../../lexer/Lexer.h"
#include "../../utils/string_utils.h"

#include <sstream>
#include <string>
#include <vector>

inline std::vector<Token> lex(const std::u32string &source) { return Lexer{source}.tokenize(); }

// 仅用于 dump 展示，跟词法层的字符串转义无关。
inline std::string escape_for_dump(const std::u32string &s) {
    std::string out;
    for (const char32_t c : s) {
        switch (c) {
        case U'\n':
            out += "\\n";
            break;
        case U'\t':
            out += "\\t";
            break;
        case U'\r':
            out += "\\r";
            break;
        default:
            out += u32_to_utf8(c);
        }
    }
    return out;
}

// 格式：`TYPE1 TYPE2(内容) ...`，跳过结尾 EOF。int/decimal/str/标识符带括号内容。
inline std::string lex_dump(const std::u32string &source) {
    const auto tokens{lex(source)};
    std::ostringstream oss;
    bool first{true};
    for (const auto &tok : tokens) {
        if (tok.type == TokenType::END_OF_FILE) continue;
        if (!first) oss << ' ';
        first = false;

        oss << Lexer::get_typename_by_tokentype(tok.type);
        switch (tok.type) {
        case TokenType::LITERAL_INT:
        case TokenType::LITERAL_DECIMAL:
        case TokenType::LITERAL_STR:
        case TokenType::IDENTIFIER:
            oss << '(' << escape_for_dump(tok.lexeme) << ')';
            break;
        default:
            break;
        }
    }
    return oss.str();
}
