#pragma once

// 测试专用工具：把 Lexer 的输出转成人类可读、方便在测试里当字符串字面量写的形式。

#include "../../lexer/Lexer.h"
#include "../../utils/string_utils.h"

#include <sstream>
#include <string>
#include <vector>

// 直接跑词法分析，拿到原始 token 序列（含结尾 EOF）。
// 用于需要检查 row/col、或者需要检查 EOF 本身的用例。
inline std::vector<Token> lex(const std::u32string &source) { return Lexer{source}.tokenize(); }

// 把一个 u32string 里的控制字符转成可见的转义序列，纯粹是为了让转储结果保持单行、方便阅读/比对，
// 跟词法分析本身的转义处理（Lexer::read_string）无关，只是测试展示层面的东西。
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

// 词法分析 + 转储成一行字符串，格式：`TYPE1 TYPE2(内容) TYPE3 ...`，token 间用单个空格分隔。
// 会跳过结尾的 END_OF_FILE。
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
