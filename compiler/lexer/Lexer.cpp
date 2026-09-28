#include "Lexer.h"

#include "../../common/Position.h"
#include "../../common/index_of.h"
#include "../../cppexceptions/SyntaxError.h"
#include "../../utils/string_utils.h"
#include "token.h"

#include <cassert>
#include <cstddef>
#include <format>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

char32_t Lexer::peek(const std::size_t offset) const {
    const std::size_t idx{idx_ + offset};
    return idx < source_.size() ? source_[idx] : U'\0';
}

char32_t Lexer::advance() {
    assert(idx_ < source_.size());

    const char32_t c{source_[idx_++]};
    if (c == U'\n') // 该换行了
        pos_.row++, pos_.col = 1;
    else // 行中间
        pos_.col++;
    return c;
}

bool Lexer::is_eof() const { return idx_ >= source_.size(); }

void Lexer::error(const std::string &msg, const Position pos) const {
    throw SyntaxError{file_path_, pos, msg};
}

void Lexer::skip_spaces() {
    while (!is_eof() && (peek() == U' ' || peek() == U'\t' || peek() == U'\r')) advance();
}

Token Lexer::read_newline() {
    const Position start_pos{pos_};

    advance(); // 消耗 '\n'
    return {TokenType::NEWLINE, start_pos, U"\n"};
}

void Lexer::read_comment_line() {
    while (!is_eof() && peek() != U'\n') advance();
}

void Lexer::read_comment_block() {
    const Position start_pos{pos_};
    advance(), advance(); // 消耗 /*
    while (!is_eof()) {
        if (peek() == U'*' && peek(1) == U'/') {
            advance(), advance(); // 消耗 */
            return;
        }
        advance();
    }
    error("unterminated block comment", start_pos);
}

Token Lexer::read_string(const char32_t quote) {
    assert(quote == U'"' || quote == U'\'');

    // 工具：某些字符到对应转义字符的映射
    // a b f n r t v 0 \ ' "
    static const std::unordered_map<char32_t, char32_t> kEscapeCharMapping{
        {U'a', U'\a'},
        {U'b', U'\b'},
        {U'f', U'\f'},
        {U'n', U'\n'},
        {U'r', U'\r'},
        {U't', U'\t'},
        {U'v', U'\v'},
        {U'0', U'\0'},
        {U'\\', U'\\'},
        {U'\'', U'\''},
        {U'"', U'"'},
    };

    const Position start_pos{pos_};

    advance(); // 消耗开头引号
    std::u32string str_literal;
    while (!is_eof() && peek() != U'\n') {
        const char32_t c{advance()};
        if (c == quote) return {TokenType::LITERAL_STR, start_pos, std::move(str_literal)};

        // 转义字符
        if (c == U'\\') {
            const Position esc_pos{pos_.row, pos_.col - 1}; // 反斜杠自己的位置

            if (is_eof() || peek() == U'\n')
                error("unterminated escape sequence in string literal", esc_pos);

            const char32_t esc{advance()};
            if (const auto it{kEscapeCharMapping.find(esc)}; it != kEscapeCharMapping.end()) {
                str_literal += it->second;
            } else {
                error(std::format("unknown escape sequence '\\{}'", u32_to_utf8(esc)), esc_pos);
            }
        } else {
            str_literal += c;
        }
    }
    error("unterminated string literal", start_pos);
}

Token Lexer::read_raw_string() {
    const Position start_pos{pos_};

    advance(); // 消耗开头反引号
    std::u32string str_literal;
    while (!is_eof()) {
        const char32_t c{advance()};
        if (c == U'`') return {TokenType::LITERAL_STR, start_pos, std::move(str_literal)};
        str_literal += c;
    }
    error("unterminated raw string literal", start_pos);
}

Token Lexer::read_number() {
    const Position start_pos{pos_};

    // 读一串数字
    const auto read_digits{
        [&](const bool allow_leading_zeros, const std::string_view part) -> std::u32string {
            std::u32string digits;
            while (!is_eof() && is_digit(peek())) digits += advance();
            if (!allow_leading_zeros && digits.size() > 1 && digits[0] == U'0') {
                error(std::format("leading zeros in {} are not permitted", part), start_pos);
            }
            return digits;
        }
    };

    std::u32string num_literal{read_digits(false, "the integer part")};
    bool is_decimal{false}; // 带小数点即为 decimal，否则是 int

    // 只有小数点后紧跟数字才当作小数部分；否则不消耗这个 '.'，留给下一个 token
    if (peek() == U'.' && is_digit(peek(1))) {
        is_decimal = true;
        num_literal += advance();
        num_literal += read_digits(true, "the fractional part"); // 小数部分不限前导零
    }

    // 科学计数法后缀 `[eE][+-]?digits`
    if (peek() == U'e' || peek() == U'E') {
        num_literal += advance();

        bool exponent_negative{false};
        if (peek() == U'+' || peek() == U'-') {
            exponent_negative = peek() == U'-';
            num_literal += advance();
        }

        const std::u32string exponent_digits{read_digits(false, "the exponent")};
        if (exponent_digits.empty()) {
            error("missing exponent digits in numeric literal", pos_);
        }
        num_literal += exponent_digits;

        // int 的 e 有范围
        if (!is_decimal) {
            // >= 0
            if (exponent_negative) {
                error("a negative exponent needs a fractional part", start_pos);
            }
            // <= 9999，也即位数 <= 4
            if (exponent_digits.size() > 4) {
                error("exponent of an integer literal may not exceed 9999", start_pos);
            }
        }
    }

    // 数字后面紧跟字母或下划线，非法
    if (is_alpha(peek()) || peek() == U'_') {
        error(
            std::format("unexpected character '{}' after a numeric literal", u32_to_utf8(peek())),
            pos_
        );
    }

    return {
        is_decimal ? TokenType::LITERAL_DECIMAL : TokenType::LITERAL_INT,
        start_pos,
        std::move(num_literal)
    };
}

Token Lexer::read_identifier_keyword_reservedword() {
    // 工具：关键字字符串到对应 TokenType 的映射
    static const std::unordered_map<std::u32string, TokenType> kKeywordsMapping{
#define X(a, b) {U"" #a, TokenType::b},
#include "x_keyword.inc"

#undef X
    };

    // 工具：保留字字符串到对应 TokenType 的映射
    static const std::unordered_map<std::u32string, TokenType> kReservedWordsMapping{
#define X(a, b) {U"" #a, TokenType::b},
#include "x_reservedword.inc"

#undef X
    };

    const Position start_pos{pos_};

    std::u32string word;
    while (!is_eof() && (is_alpha_digit(peek()) || peek() == U'_')) word += advance();

    // 保留字：直接报 SyntaxError
    if (kReservedWordsMapping.contains(word)) {
        error(std::format("'{}' is a reserved word", u32_to_utf8(word)), start_pos);
    }
    if (const auto it{kKeywordsMapping.find(word)}; it != kKeywordsMapping.end()) {
        return {it->second, start_pos, std::move(word)};
    }

    return {TokenType::IDENTIFIER, start_pos, std::move(word)};
}

Token Lexer::read_symbol() {
    using enum TokenType;

    const Position start_pos{pos_};

    switch (const char32_t c{advance()}) {

    // 对于严格单个字符符号
    case U'(':
        return {SIGN_LPAREN, start_pos, U"("};
    case U')':
        return {SIGN_RPAREN, start_pos, U")"};
    case U'[':
        return {SIGN_LBRACKET, start_pos, U"["};
    case U']':
        return {SIGN_RBRACKET, start_pos, U"]"};
    case U'{':
        return {SIGN_LBRACE, start_pos, U"{"};
    case U'}':
        return {SIGN_RBRACE, start_pos, U"}"};
    case U',':
        return {SIGN_COMMA, start_pos, U","};
    case U';':
        return {SIGN_SEMICOLON, start_pos, U";"};
    case U':':
        return {SIGN_COLON, start_pos, U":"};
    case U'@':
        return {SIGN_AT, start_pos, U"@"};
    case U'~':
        return {SIGN_TILDE, start_pos, U"~"};
    case U'?':
        return {SIGN_QUESTION, start_pos, U"?"};

    // 对于可能的多字符符号
    case U'+':
        if (peek() == U'=') {
            advance();
            return {SIGN_PLUS_ASSIGN, start_pos, U"+="};
        }
        return {SIGN_PLUS, start_pos, U"+"};

    case U'-':
        if (peek() == U'=') {
            advance();
            return {SIGN_MINUS_ASSIGN, start_pos, U"-="};
        }
        return {SIGN_MINUS, start_pos, U"-"};

    case U'*':
        if (peek() == U'*') {
            advance();
            if (peek() == U'=') {
                advance();
                return {SIGN_DOUBLESTAR_ASSIGN, start_pos, U"**="};
            }
            return {SIGN_DOUBLESTAR, start_pos, U"**"};
        }
        if (peek() == U'=') {
            advance();
            return {SIGN_STAR_ASSIGN, start_pos, U"*="};
        }
        return {SIGN_STAR, start_pos, U"*"};

    case U'/':
        if (peek() == U'/') {
            advance();
            if (peek() == U'=') {
                advance();
                return {SIGN_DOUBLESLASH_ASSIGN, start_pos, U"//="};
            }
            return {SIGN_DOUBLESLASH, start_pos, U"//"};
        }
        if (peek() == U'=') {
            advance();
            return {SIGN_SLASH_ASSIGN, start_pos, U"/="};
        }
        return {SIGN_SLASH, start_pos, U"/"};

    case U'%':
        if (peek() == U'=') {
            advance();
            return {SIGN_PERCENT_ASSIGN, start_pos, U"%="};
        }
        return {SIGN_PERCENT, start_pos, U"%"};

    case U'&':
        if (peek() == U'=') {
            advance();
            return {SIGN_AMPERSAND_ASSIGN, start_pos, U"&="};
        }
        return {SIGN_AMPERSAND, start_pos, U"&"};

    case U'|':
        if (peek() == U'=') {
            advance();
            return {SIGN_PIPE_ASSIGN, start_pos, U"|="};
        }
        return {SIGN_PIPE, start_pos, U"|"};

    case U'^':
        if (peek() == U'=') {
            advance();
            return {SIGN_CARET_ASSIGN, start_pos, U"^="};
        }
        return {SIGN_CARET, start_pos, U"^"};

    case U'<':
        if (peek() == U'<') {
            advance();
            if (peek() == U'=') {
                advance();
                return {SIGN_LSHIFT_ASSIGN, start_pos, U"<<="};
            }
            return {SIGN_LSHIFT, start_pos, U"<<"};
        }
        if (peek() == U'=') {
            advance();
            return {SIGN_LE, start_pos, U"<="};
        }
        return {SIGN_LT, start_pos, U"<"};

    case U'>':
        if (peek() == U'>') {
            advance();
            if (peek() == U'=') {
                advance();
                return {SIGN_RSHIFT_ASSIGN, start_pos, U">>="};
            }
            return {SIGN_RSHIFT, start_pos, U">>"};
        }
        if (peek() == U'=') {
            advance();
            return {SIGN_GE, start_pos, U">="};
        }
        return {SIGN_GT, start_pos, U">"};

    case U'=':
        if (peek() == U'=') {
            advance();
            return {SIGN_EQ, start_pos, U"=="};
        }
        return {SIGN_ASSIGN, start_pos, U"="};

    case U'!':
        if (peek() == U'=') {
            advance();
            return {SIGN_NE, start_pos, U"!="};
        }
        return {SIGN_EXCLAIM, start_pos, U"!"};

    case U'$':
        if (peek() == U'$') {
            advance();
            return {SIGN_DOUBLEDOLLAR, start_pos, U"$$"};
        }
        return {SIGN_DOLLAR, start_pos, U"$"};

    case U'.': {
        // 贪婪匹配：尽可能多吃连续的点，最多 3 个
        // 别忘了此时已经消耗了第一个点
        int dot_count{1};
        while (dot_count < 3 && peek() == U'.') {
            advance();
            dot_count++;
        }
        switch (dot_count) {
        case 3:
            return {LITERAL_ELLIPSIS, start_pos, U"..."};
        case 2:
            return {SIGN_DOTDOT, start_pos, U".."};
        default:
            return {SIGN_DOT, start_pos, U"."};
        }
    }

    default:
        error(std::format("unexpected character '{}'", u32_to_utf8(c)), start_pos);
    }
}

Lexer::Lexer(std::u32string source, std::string file_path)
    : source_{std::move(source)}, file_path_{std::move(file_path)} {}

std::vector<Token> Lexer::run() {
    std::vector<Token> all_tokens;

    while (true) {
        skip_spaces();

        if (is_eof()) {
            all_tokens.push_back({TokenType::END_OF_FILE, pos_, U""});
            break;
        }

        // 换行
        if (const char32_t c{peek()}; c == U'\n') {
            // 如果是开头或者上一个 token 是换行/分号，这个换行省了
            if (all_tokens.empty() || all_tokens.back().type == TokenType::NEWLINE ||
                all_tokens.back().type == TokenType::SIGN_SEMICOLON)
                advance(); // 还是要消耗掉这个'\n'
            else
                all_tokens.push_back(read_newline());
        }

        // 单行注释
        else if (c == U'#') {
            read_comment_line();
        }
        // 多行注释
        else if (c == U'/' && peek(1) == U'*') {
            read_comment_block();
        }
        // 字符串
        else if (c == U'"' || c == U'\'') {
            all_tokens.push_back(read_string(c));
        }
        // 反引号原始字符串
        else if (c == U'`') {
            all_tokens.push_back(read_raw_string());
        }
        // 数字
        else if (is_digit(c)) {
            all_tokens.push_back(read_number());
        }
        // 标识符 / 关键字 / 保留字
        else if (is_alpha(c) || c == U'_') {
            all_tokens.push_back(read_identifier_keyword_reservedword());
        }
        // 符号
        else {
            all_tokens.push_back(read_symbol());
        }
    }

    return all_tokens;
}

std::vector<Token> Lexer::tokenize(std::u32string source, std::string file_path) {
    return Lexer{std::move(source), std::move(file_path)}.run();
}

std::string Lexer::get_typename_by_tokentype(const TokenType type) {
    static constexpr const char *kTokenTypeMapping[]{
#define X(name) #name,
#include "x_token_type.inc"

#undef X
    };

    const std::size_t ind{index_of(type)};
    if (ind >= std::size(kTokenTypeMapping)) return "<unknown token type>";
    return kTokenTypeMapping[ind];
}

std::string Lexer::get_displayname_by_tokentype(const TokenType type) {
    using enum TokenType;

    // clang-format off
    switch (type) {
    case LITERAL_NONE:            return "'None'";
    case LITERAL_TRUE:            return "'True'";
    case LITERAL_FALSE:           return "'False'";
    case LITERAL_G:               return "'_G'";
    case LITERAL_L:               return "'_L'";
    case LITERAL_ELLIPSIS:        return "'...'";
    case LITERAL_INT:             return "an integer literal";
    case LITERAL_DECIMAL:         return "a decimal literal";
    case LITERAL_STR:             return "a string literal";

    case IDENTIFIER:              return "an identifier";

    case KW_NOT:                  return "'not'";
    case KW_AND:                  return "'and'";
    case KW_OR:                   return "'or'";
    case KW_IS:                   return "'is'";
    case KW_IN:                   return "'in'";
    case KW_AS:                   return "'as'";
    case KW_DEL:                  return "'del'";
    case KW_GLOBAL:               return "'global'";
    case KW_IF:                   return "'if'";
    case KW_ELIF:                 return "'elif'";
    case KW_ELSE:                 return "'else'";
    case KW_FOR:                  return "'for'";
    case KW_WHILE:                return "'while'";
    case KW_BREAK:                return "'break'";
    case KW_CONTINUE:             return "'continue'";
    case KW_FUNC:                 return "'func'";
    case KW_RETURN:               return "'return'";
    case KW_RAISE:                return "'raise'";
    case KW_TRY:                  return "'try'";
    case KW_EXCEPT:               return "'except'";
    case KW_FINALLY:              return "'finally'";
    case KW_CLASS:                return "'class'";
    case KW_IMPORT:               return "'import'";
    case KW_EVAL:                 return "'eval'";

    case RW_ASSERT:               return "'assert'";
    case RW_WHEN:                 return "'when'";
    case RW_CASE:                 return "'case'";
    case RW_YIELD:                return "'yield'";
    case RW_WITH:                 return "'with'";
    case RW_ASYNC:                return "'async'";
    case RW_AWAIT:                return "'await'";
    case RW_DEFINE:               return "'define'";
    case RW_CONST:                return "'const'";
    case RW_STATIC:               return "'static'";
    case RW_LOCAL:                return "'local'";

    case SIGN_LPAREN:             return "'('";
    case SIGN_RPAREN:             return "')'";
    case SIGN_LBRACKET:           return "'['";
    case SIGN_RBRACKET:           return "']'";
    case SIGN_LBRACE:             return "'{'";
    case SIGN_RBRACE:             return "'}'";

    case SIGN_COMMA:              return "','";
    case SIGN_SEMICOLON:          return "';'";
    case SIGN_COLON:              return "':'";
    case SIGN_AT:                 return "'@'";
    case SIGN_DOLLAR:             return "'$'";
    case SIGN_DOUBLEDOLLAR:       return "'$$'";

    case SIGN_PLUS:               return "'+'";
    case SIGN_MINUS:              return "'-'";
    case SIGN_STAR:               return "'*'";
    case SIGN_DOUBLESTAR:         return "'**'";
    case SIGN_SLASH:              return "'/'";
    case SIGN_DOUBLESLASH:        return "'//'";
    case SIGN_PERCENT:            return "'%'";

    case SIGN_AMPERSAND:          return "'&'";
    case SIGN_PIPE:               return "'|'";
    case SIGN_CARET:              return "'^'";
    case SIGN_TILDE:              return "'~'";
    case SIGN_LSHIFT:             return "'<<'";
    case SIGN_RSHIFT:             return "'>>'";

    case SIGN_EQ:                 return "'=='";
    case SIGN_NE:                 return "'!='";
    case SIGN_LT:                 return "'<'";
    case SIGN_LE:                 return "'<='";
    case SIGN_GT:                 return "'>'";
    case SIGN_GE:                 return "'>='";

    case SIGN_ASSIGN:             return "'='";

    case SIGN_PLUS_ASSIGN:        return "'+='";
    case SIGN_MINUS_ASSIGN:       return "'-='";
    case SIGN_STAR_ASSIGN:        return "'*='";
    case SIGN_DOUBLESTAR_ASSIGN:  return "'**='";
    case SIGN_SLASH_ASSIGN:       return "'/='";
    case SIGN_DOUBLESLASH_ASSIGN: return "'//='";
    case SIGN_PERCENT_ASSIGN:     return "'%='";
    case SIGN_AMPERSAND_ASSIGN:   return "'&='";
    case SIGN_PIPE_ASSIGN:        return "'|='";
    case SIGN_CARET_ASSIGN:       return "'^='";
    case SIGN_LSHIFT_ASSIGN:      return "'<<='";
    case SIGN_RSHIFT_ASSIGN:      return "'>>='";

    case SIGN_DOT:                return "'.'";
    case SIGN_DOTDOT:             return "'..'";
    case SIGN_QUESTION:           return "'?'";
    case SIGN_EXCLAIM:            return "'!'";

    case NEWLINE:                 return "a newline";
    case END_OF_FILE:             return "EOF";
    // clang-format on
    default:
        return "<unknown token type>";
    }
}
