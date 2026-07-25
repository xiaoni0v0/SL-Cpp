#pragma once

#include "token.h"

#include <string>
#include <vector>

class Lexer {
    const std::u32string source_;
    size_t pos_{0};
    int row_{1};
    int col_{1};
    const std::string file_path_;

    // 往后看字符
    [[nodiscard]] char32_t peek(size_t offset = 0) const;
    // 消耗字符
    char32_t advance();
    // 是否读完了
    [[nodiscard]] bool is_eof() const;
    // 抛出 SyntaxError 异常
    [[noreturn]] void error(const std::string &msg) const;
    // 抛出 SyntaxError 异常，提供行列
    [[noreturn]] void error(const std::string &msg, int row, int col) const;

    // 跳过空白（不含换行）
    void skip_spaces();

    // 读 \n
    [[nodiscard]] Token read_newline();
    // 读单行注释
    void read_comment_line();
    // 读多行注释
    void read_comment_block();
    // 读字符串字面量。quote 为 ' 或者 "
    [[nodiscard]] Token read_string(char32_t quote);
    // 读反引号原始字符串字面量：不处理转义，天然支持多行
    [[nodiscard]] Token read_raw_string();
    // 读数字字面量
    [[nodiscard]] Token read_number();
    // 读标识符、关键字、保留字
    [[nodiscard]] Token read_identifier_keyword_reservedword();
    // 读符号
    [[nodiscard]] Token read_symbol();

  public:
    /**
     * 构造 Lexer 对象
     * @param source    源代码
     * @param file_path 文件路径，默认为 "<unknown>"
     */
    explicit Lexer(std::u32string source, std::string file_path = "<unknown>");

    /**
     * 对源代码词法分析，只能调用一次（右值限定）
     * @return token 序列
     */
    [[nodiscard]] std::vector<Token> tokenize() &&;

    /**
     * TokenType::SIGN_LPAREN -> "SIGN_LPAREN"
     * @param type token 类型
     * @return     token 类型的字符串表示
     */
    [[nodiscard]] static std::string get_typename_by_tokentype(TokenType type);

    /**
     * TokenType::SIGN_LPAREN -> "("
     * @param type token 类型
     * @return     token 类型的用户可读名称
     */
    [[nodiscard]] static std::string get_displayname_by_tokentype(TokenType type);
};
