#include "Lexer.h"

#include "../builtins/exceptions/SyntaxError.h"
#include "../utils/string_utils.h"

#include <format>
#include <unordered_map>
#include <utility>

// 工具，创建一个 token
static Token
make_token(const TokenType type, const int row, const int col, const std::u32string &value) {
    return Token{type, row, col, value};
}

char32_t Lexer::peek(const size_t offset) const {
    const size_t idx{pos_ + offset};
    return idx < source_.size() ? source_[idx] : U'\0';
}

char32_t Lexer::advance() {
    const char32_t c{source_[pos_++]};
    if (c == U'\n') // 该换行了
        row_++, col_ = 1;
    else // 行中间
        col_++;
    return c;
}

bool Lexer::is_eof() const { return pos_ >= source_.size(); }

void Lexer::error(const std::string &msg) const { throw SyntaxError{file_path_, row_, col_, msg}; }

void Lexer::error(const std::string &msg, const int row, const int col) const {
    throw SyntaxError{file_path_, row, col, msg};
}

void Lexer::skip_spaces() {
    while (!is_eof() && (peek() == U' ' || peek() == U'\t' || peek() == U'\r')) advance();
}

Token Lexer::read_newline() {
    const int start_row{row_}, start_col{col_};

    advance(); // 消耗 '\n'
    return make_token(TokenType::NEWLINE, start_row, start_col, U"\n");
}

void Lexer::read_comment_line() {
    while (!is_eof() && peek() != U'\n') advance();
}

void Lexer::read_comment_block() {
    const int start_row{row_}, start_col{col_};
    advance(), advance(); // 消耗 /*
    while (!is_eof()) {
        if (peek() == U'*' && peek(1) == U'/') {
            advance(), advance(); // 消耗 */
            return;
        }
        advance();
    }
    error("unterminated block comment", start_row, start_col);
}

Token Lexer::read_string(const char32_t quote) {
    // 工具：某些字符到对应转义字符的映射
    // a b f n r t v 0 \ ' "
    static const std::unordered_map<char32_t, char32_t> ESCAPE_CHAR_MAPPING{
        {U'a', U'\a'}, {U'b', U'\b'}, {U'f', U'\f'},  {U'n', U'\n'},  {U'r', U'\r'}, {U't', U'\t'},
        {U'v', U'\v'}, {U'0', U'\0'}, {U'\\', U'\\'}, {U'\'', U'\''}, {U'"', U'"'},
    };

    const int start_row{row_}, start_col{col_};

    advance(); // 消耗开头引号
    std::u32string str_literal;
    while (!is_eof() && peek() != U'\n') {
        const char32_t c{advance()};
        if (c == quote)
            return make_token(TokenType::LITERAL_STR, start_row, start_col, str_literal);

        // 转义字符
        if (c == U'\\') {
            const int esc_row{row_}, esc_col{col_ - 1}; // 反斜杠自己的位置

            if (is_eof() || peek() == U'\n')
                error("unterminated escape sequence in string literal", esc_row, esc_col);

            const char32_t esc{advance()};
            if (const auto it{ESCAPE_CHAR_MAPPING.find(esc)}; it != ESCAPE_CHAR_MAPPING.end()) {
                str_literal += it->second;
            } else {
                error(
                    std::format("unknown escape sequence '\\{}'", u32_to_utf8(esc)), esc_row,
                    esc_col
                );
            }
        } else {
            str_literal += c;
        }
    }
    error("unterminated string literal", start_row, start_col);
}

Token Lexer::read_raw_string() {
    const int start_row{row_}, start_col{col_};

    advance(); // 消耗开头反引号
    std::u32string str_literal;
    while (!is_eof()) {
        const char32_t c{advance()};
        if (c == U'`') return make_token(TokenType::LITERAL_STR, start_row, start_col, str_literal);
        str_literal += c;
    }
    error("unterminated raw string literal", start_row, start_col);
}

Token Lexer::read_number() {
    const int start_row{row_}, start_col{col_};

    std::u32string num_literal;
    bool is_float{false};

    while (!is_eof() && is_digit(peek())) num_literal += advance();

    // 只有小数点后紧跟数字才当作 float 的一部分；否则不消耗这个 '.'，留给下一个 token
    if (peek() == U'.' && is_digit(peek(1))) {
        is_float = true;
        num_literal += advance();
        while (!is_eof() && is_digit(peek())) num_literal += advance();
    }

    // 数字后面紧跟字母或下划线，非法
    if (is_alpha(peek()) || peek() == U'_') {
        error(
            std::format("invalid numeric literal, unexpected character '{}'", u32_to_utf8(peek()))
        );
    }

    return make_token(
        is_float ? TokenType::LITERAL_FLOAT : TokenType::LITERAL_INT, start_row, start_col,
        num_literal
    );
}

Token Lexer::read_identifier_keyword_reservedword() {
    // 工具：关键字字符串到对应 TokenType 的映射
    static const std::unordered_map<std::u32string, TokenType> KEYWORDS_MAPPING{
#define X(a, b) {U"" #a, TokenType::b},
#include "x_keyword.h"
#undef X
    };

    // 工具：保留字字符串到对应 TokenType 的映射
    static const std::unordered_map<std::u32string, TokenType> RESERVEDWORDS_MAPPING{
#define X(a, b) {U"" #a, TokenType::b},
#include "x_reservedword.h"
#undef X
    };

    const int start_row{row_}, start_col{col_};

    std::u32string word;
    while (!is_eof() && (is_alpha_digit(peek()) || peek() == U'_')) word += advance();

    // 保留字：直接报 SyntaxError
    if (RESERVEDWORDS_MAPPING.contains(word)) {
        error(std::format("'{}' is a reserved word", u32_to_utf8(word)), start_row, start_col);
    }
    if (const auto it{KEYWORDS_MAPPING.find(word)}; it != KEYWORDS_MAPPING.end()) {
        return make_token(it->second, start_row, start_col, word);
    }

    return make_token(TokenType::IDENTIFIER, start_row, start_col, word);
}

Token Lexer::read_symbol() {
    const int start_row{row_}, start_col{col_};

    switch (const char32_t c{advance()}) {

    // 对于严格单个字符符号
    case U'(':
        return make_token(TokenType::SIGN_LPAREN, start_row, start_col, U"(");
    case U')':
        return make_token(TokenType::SIGN_RPAREN, start_row, start_col, U")");
    case U'[':
        return make_token(TokenType::SIGN_LBRACKET, start_row, start_col, U"[");
    case U']':
        return make_token(TokenType::SIGN_RBRACKET, start_row, start_col, U"]");
    case U'{':
        return make_token(TokenType::SIGN_LBRACE, start_row, start_col, U"{");
    case U'}':
        return make_token(TokenType::SIGN_RBRACE, start_row, start_col, U"}");
    case U',':
        return make_token(TokenType::SIGN_COMMA, start_row, start_col, U",");
    case U';':
        return make_token(TokenType::SIGN_SEMICOLON, start_row, start_col, U";");
    case U':':
        return make_token(TokenType::SIGN_COLON, start_row, start_col, U":");
    case U'@':
        return make_token(TokenType::SIGN_AT, start_row, start_col, U"@");
    case U'$':
        return make_token(TokenType::SIGN_DOLLAR, start_row, start_col, U"$");
    case U'~':
        return make_token(TokenType::SIGN_TILDE, start_row, start_col, U"~");
    case U'?':
        return make_token(TokenType::SIGN_QUESTION, start_row, start_col, U"?");

    // 对于可能的多字符符号
    case U'+':
        if (peek() == U'=') {
            advance();
            return make_token(TokenType::SIGN_PLUS_ASSIGN, start_row, start_col, U"+=");
        }
        return make_token(TokenType::SIGN_PLUS, start_row, start_col, U"+");

    case U'-':
        if (peek() == U'=') {
            advance();
            return make_token(TokenType::SIGN_MINUS_ASSIGN, start_row, start_col, U"-=");
        }
        return make_token(TokenType::SIGN_MINUS, start_row, start_col, U"-");

    case U'*':
        if (peek() == U'*') {
            advance();
            if (peek() == U'=') {
                advance();
                return make_token(TokenType::SIGN_DOUBLESTAR_ASSIGN, start_row, start_col, U"**=");
            }
            return make_token(TokenType::SIGN_DOUBLESTAR, start_row, start_col, U"**");
        }
        if (peek() == U'=') {
            advance();
            return make_token(TokenType::SIGN_STAR_ASSIGN, start_row, start_col, U"*=");
        }
        return make_token(TokenType::SIGN_STAR, start_row, start_col, U"*");

    case U'/':
        if (peek() == U'/') {
            advance();
            if (peek() == U'=') {
                advance();
                return make_token(TokenType::SIGN_DOUBLESLASH_ASSIGN, start_row, start_col, U"//=");
            }
            return make_token(TokenType::SIGN_DOUBLESLASH, start_row, start_col, U"//");
        }
        if (peek() == U'=') {
            advance();
            return make_token(TokenType::SIGN_SLASH_ASSIGN, start_row, start_col, U"/=");
        }
        return make_token(TokenType::SIGN_SLASH, start_row, start_col, U"/");

    case U'%':
        if (peek() == U'=') {
            advance();
            return make_token(TokenType::SIGN_PERCENT_ASSIGN, start_row, start_col, U"%=");
        }
        return make_token(TokenType::SIGN_PERCENT, start_row, start_col, U"%");

    case U'&':
        if (peek() == U'=') {
            advance();
            return make_token(TokenType::SIGN_AMPERSAND_ASSIGN, start_row, start_col, U"&=");
        }
        return make_token(TokenType::SIGN_AMPERSAND, start_row, start_col, U"&");

    case U'|':
        if (peek() == U'=') {
            advance();
            return make_token(TokenType::SIGN_PIPE_ASSIGN, start_row, start_col, U"|=");
        }
        return make_token(TokenType::SIGN_PIPE, start_row, start_col, U"|");

    case U'^':
        if (peek() == U'=') {
            advance();
            return make_token(TokenType::SIGN_CARET_ASSIGN, start_row, start_col, U"^=");
        }
        return make_token(TokenType::SIGN_CARET, start_row, start_col, U"^");

    case U'<':
        if (peek() == U'<') {
            advance();
            if (peek() == U'=') {
                advance();
                return make_token(TokenType::SIGN_LSHIFT_ASSIGN, start_row, start_col, U"<<=");
            }
            return make_token(TokenType::SIGN_LSHIFT, start_row, start_col, U"<<");
        }
        if (peek() == U'=') {
            advance();
            return make_token(TokenType::SIGN_LE, start_row, start_col, U"<=");
        }
        return make_token(TokenType::SIGN_LT, start_row, start_col, U"<");

    case U'>':
        if (peek() == U'>') {
            advance();
            if (peek() == U'=') {
                advance();
                return make_token(TokenType::SIGN_RSHIFT_ASSIGN, start_row, start_col, U">>=");
            }
            return make_token(TokenType::SIGN_RSHIFT, start_row, start_col, U">>");
        }
        if (peek() == U'=') {
            advance();
            return make_token(TokenType::SIGN_GE, start_row, start_col, U">=");
        }
        return make_token(TokenType::SIGN_GT, start_row, start_col, U">");

    case U'=':
        if (peek() == U'=') {
            advance();
            return make_token(TokenType::SIGN_EQ, start_row, start_col, U"==");
        }
        return make_token(TokenType::SIGN_ASSIGN, start_row, start_col, U"=");

    case U'!':
        if (peek() == U'=') {
            advance();
            return make_token(TokenType::SIGN_NE, start_row, start_col, U"!=");
        }
        return make_token(TokenType::SIGN_EXCLAIM, start_row, start_col, U"!");

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
            return make_token(TokenType::LITERAL_ELLIPSIS, start_row, start_col, U"...");
        case 2:
            return make_token(TokenType::SIGN_DOTDOT, start_row, start_col, U"..");
        default:
            return make_token(TokenType::SIGN_DOT, start_row, start_col, U".");
        }
    }

    default:
        error(std::format("unexpected character '{}'", u32_to_utf8(c)), start_row, start_col);
    }
}

Lexer::Lexer(std::u32string source, std::string file_path)
    : source_{std::move(source)}, file_path_{std::move(file_path)} {}

std::vector<Token> Lexer::tokenize() && {
    std::vector<Token> all_tokens;

    while (true) {
        skip_spaces();

        if (is_eof()) {
            all_tokens.push_back(make_token(TokenType::END_OF_FILE, row_, col_, U""));
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

std::string Lexer::get_typename_by_tokentype(const TokenType type) {
    static constexpr const char *const TOKEN_TYPE_MAPPING[]{
#define X(name) #name,
#include "x_token_type.h"
#undef X
    };

    const size_t ind{static_cast<size_t>(type)};
    if (ind >= std::size(TOKEN_TYPE_MAPPING)) return "<unknown token type>";
    return TOKEN_TYPE_MAPPING[ind];
}

std::string Lexer::get_displayname_by_tokentype(const TokenType type) {
    switch (type) {
        // clang-format off
    case TokenType::LITERAL_NONE:            return "'None'";
    case TokenType::LITERAL_TRUE:            return "'True'";
    case TokenType::LITERAL_FALSE:           return "'False'";
    case TokenType::LITERAL_G:               return "'_G'";
    case TokenType::LITERAL_L:               return "'_L'";
    case TokenType::LITERAL_ELLIPSIS:        return "'...'";
    case TokenType::LITERAL_INT:             return "an integer literal";
    case TokenType::LITERAL_FLOAT:           return "a float literal";
    case TokenType::LITERAL_STR:             return "a string literal";

    case TokenType::IDENTIFIER:              return "an identifier";

    case TokenType::KW_NOT:                  return "'not'";
    case TokenType::KW_AND:                  return "'and'";
    case TokenType::KW_OR:                   return "'or'";
    case TokenType::KW_IS:                   return "'is'";
    case TokenType::KW_DEL:                  return "'del'";
    case TokenType::KW_GLOBAL:               return "'global'";
    case TokenType::KW_IF:                   return "'if'";
    case TokenType::KW_ELIF:                 return "'elif'";
    case TokenType::KW_ELSE:                 return "'else'";
    case TokenType::KW_FOR:                  return "'for'";
    case TokenType::KW_WHILE:                return "'while'";
    case TokenType::KW_BREAK:                return "'break'";
    case TokenType::KW_CONTINUE:             return "'continue'";
    case TokenType::KW_FUNC:                 return "'func'";
    case TokenType::KW_RETURN:               return "'return'";
    case TokenType::KW_RAISE:                return "'raise'";
    case TokenType::KW_TRY:                  return "'try'";
    case TokenType::KW_EXCEPT:               return "'except'";
    case TokenType::KW_FINALLY:              return "'finally'";
    case TokenType::KW_CLASS:                return "'class'";

    case TokenType::RW_ASSERT:               return "'assert'";
    case TokenType::RW_IN:                   return "'in'";
    case TokenType::RW_WHEN:                 return "'when'";
    case TokenType::RW_CASE:                 return "'case'";
    case TokenType::RW_YIELD:                return "'yield'";
    case TokenType::RW_WITH:                 return "'with'";
    case TokenType::RW_ASYNC:                return "'async'";
    case TokenType::RW_AWAIT:                return "'await'";
    case TokenType::RW_DEFINE:               return "'define'";
    case TokenType::RW_AS:                   return "'as'";
    case TokenType::RW_CONST:                return "'const'";
    case TokenType::RW_STATIC:               return "'static'";
    case TokenType::RW_LOCAL:                return "'local'";

    case TokenType::SIGN_LPAREN:             return "'('";
    case TokenType::SIGN_RPAREN:             return "')'";
    case TokenType::SIGN_LBRACKET:           return "'['";
    case TokenType::SIGN_RBRACKET:           return "']'";
    case TokenType::SIGN_LBRACE:             return "'{'";
    case TokenType::SIGN_RBRACE:             return "'}'";

    case TokenType::SIGN_COMMA:              return "','";
    case TokenType::SIGN_SEMICOLON:          return "';'";
    case TokenType::SIGN_COLON:              return "':'";
    case TokenType::SIGN_AT:                 return "'@'";
    case TokenType::SIGN_DOLLAR:             return "'$'";

    case TokenType::SIGN_PLUS:               return "'+'";
    case TokenType::SIGN_MINUS:              return "'-'";
    case TokenType::SIGN_STAR:               return "'*'";
    case TokenType::SIGN_DOUBLESTAR:         return "'**'";
    case TokenType::SIGN_SLASH:              return "'/'";
    case TokenType::SIGN_DOUBLESLASH:        return "'//'";
    case TokenType::SIGN_PERCENT:            return "'%'";

    case TokenType::SIGN_AMPERSAND:          return "'&'";
    case TokenType::SIGN_PIPE:               return "'|'";
    case TokenType::SIGN_CARET:              return "'^'";
    case TokenType::SIGN_TILDE:              return "'~'";
    case TokenType::SIGN_LSHIFT:             return "'<<'";
    case TokenType::SIGN_RSHIFT:             return "'>>'";

    case TokenType::SIGN_EQ:                 return "'=='";
    case TokenType::SIGN_NE:                 return "'!='";
    case TokenType::SIGN_LT:                 return "'<'";
    case TokenType::SIGN_LE:                 return "'<='";
    case TokenType::SIGN_GT:                 return "'>'";
    case TokenType::SIGN_GE:                 return "'>='";

    case TokenType::SIGN_ASSIGN:             return "'='";

    case TokenType::SIGN_PLUS_ASSIGN:        return "'+='";
    case TokenType::SIGN_MINUS_ASSIGN:       return "'-='";
    case TokenType::SIGN_STAR_ASSIGN:        return "'*='";
    case TokenType::SIGN_DOUBLESTAR_ASSIGN:  return "'**='";
    case TokenType::SIGN_SLASH_ASSIGN:       return "'/='";
    case TokenType::SIGN_DOUBLESLASH_ASSIGN: return "'//='";
    case TokenType::SIGN_PERCENT_ASSIGN:     return "'%='";
    case TokenType::SIGN_AMPERSAND_ASSIGN:   return "'&='";
    case TokenType::SIGN_PIPE_ASSIGN:        return "'|='";
    case TokenType::SIGN_CARET_ASSIGN:       return "'^='";
    case TokenType::SIGN_LSHIFT_ASSIGN:      return "'<<='";
    case TokenType::SIGN_RSHIFT_ASSIGN:      return "'>>='";

    case TokenType::SIGN_DOT:                return "'.'";
    case TokenType::SIGN_DOTDOT:             return "'..'";
    case TokenType::SIGN_QUESTION:           return "'?'";
    case TokenType::SIGN_EXCLAIM:            return "'!'";

    case TokenType::NEWLINE:                 return "a newline";
    case TokenType::END_OF_FILE:             return "EOF";
    // clang-format on
    default:
        return "<unknown token type>";
    }
}
