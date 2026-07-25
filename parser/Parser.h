#pragma once

#include "../lexer/token.h"
#include "ast_nodes/ast_nodes.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

class Parser {
    const std::vector<Token> tokens_;
    size_t pos_{0};      // 当前 token 的索引
    int paren_depth_{0}; // 未闭合的 '(' 和 '[' 深度（不含 '{'）
    const std::string file_path_;

    // 往后看 token
    [[nodiscard]] const Token &peek() const;
    // 消耗 token
    const Token &advance();
    // 检查下一个 token 类型是否为 type
    [[nodiscard]] bool check(TokenType type) const;
    // 检查从 start（缺省为当前位置 pos_）开始，跳过 NEWLINE 之后的第一个 token 是否为 type
    [[nodiscard]] bool
    check_over_newline(TokenType type, std::optional<size_t> start = std::nullopt) const;
    // 检查当前位置是不是一条表达式合法的终止符（换行、';'、EOF、'}'），不是则抛语法错误
    void check_terminator() const;
    // 消耗对应类型 token，否则抛出异常
    const Token &expect(TokenType expected_type);
    // 无条件跳过 NEWLINE
    void skip_newline();
    // 仅在 paren_depth_ > 0（括号内）时跳过 NEWLINE
    void skip_paren_newline();
    // 无条件跳过 NEWLINE 和 ';'
    void skip_terminator();

    [[noreturn]] void error(const std::string &msg) const;

    /**
     * 尽可能多地解析表达式，直到 EOF 或 '}'
     * 不消耗 EOF 或 '}'
     * @return 节点数组
     */
    [[nodiscard]] std::vector<AstNodePtr> parse_exprs();

    /**
     * 解析一个表达式
     * 其实就是无前缀的 Pratt 解析
     * @return 节点
     */
    [[nodiscard]] AstNodePtr parse_expr();

    /**
     * Pratt 解析
     * @param min_bp 最小绑定力
     * @return
     */
    [[nodiscard]] AstNodePtr parse_expr_pratt(int min_bp);

    /**
     * 链式比较（< <= > >= == !=）
     * @param left         左操作数
     * @param start_pos    整个表达式开始的位置
     * @param first_op     第一个运算符的类型
     * @param first_op_pos 第一个运算符的位置
     */
    [[nodiscard]] AstNodePtr parse_chain_compare(
        AstNodePtr left, Position start_pos, AstNodeCompare::OpType first_op, Position first_op_pos
    );

    /**
     * 链式 is
     * @param left         左操作数
     * @param start_pos    整个表达式开始的位置
     * @param first_is_pos 第一个 is 的位置
     */
    [[nodiscard]] AstNodePtr
    parse_chain_is(AstNodePtr left, Position start_pos, Position first_is_pos);

    /**
     * 解析一个无运算符的表达式
     * 不依赖左侧值
     * @return 节点
     */
    [[nodiscard]] AstNodePtr parse_non_op();

    // 解析 if / for 中间槽 / while 的条件表达式
    [[nodiscard]] AstNodePtr parse_expr_as_cond();
    // 分组 (expr) 或者元组 (expr1, expr2)
    [[nodiscard]] AstNodePtr parse_paren_or_tuple();
    // 列表 [expr1, expr2]
    [[nodiscard]] AstNodePtr parse_list();
    // 字典 {k1: v1, ...} 或复合表达式
    [[nodiscard]] AstNodePtr parse_brace();
    // del
    [[nodiscard]] AstNodePtr parse_del();
    // global
    [[nodiscard]] AstNodePtr parse_global();
    // if-elif-else
    [[nodiscard]] AstNodePtr parse_if();
    // for
    [[nodiscard]] AstNodePtr parse_for();
    // while
    [[nodiscard]] AstNodePtr parse_while();
    // return
    [[nodiscard]] AstNodePtr parse_return();
    // try-except-finally
    [[nodiscard]] AstNodePtr parse_try();
    // raise
    [[nodiscard]] AstNodePtr parse_raise();
    // 函数
    [[nodiscard]] AstNodePtr parse_func(
        std::vector<AstNodePtr> decorators = {}, std::vector<Position> decorator_positions = {},
        Position deco_pos = {}
    );
    // 类
    [[nodiscard]] AstNodePtr parse_class(
        std::vector<AstNodePtr> decorators = {}, std::vector<Position> decorator_positions = {},
        Position deco_pos = {}
    );
    // 装饰器表达式 / 函数 / 类
    [[nodiscard]] AstNodePtr parse_decorator();

    /**
     * 完成一堆逗号连成的一串的剩余部分，可能空。不消耗括号、不涉及 paren_depth_。
     * 说白了它的功能就是跳过逗号并控制何时结束，不管每一项怎么解析。
     * @param close      结束括号 token 类型
     * @param parse_item 回调函数，对每一项怎么解析
     * @return           是否真的消耗过至少一个 ','
     */
    bool finish_comma_batch(TokenType close, const std::function<void()> &parse_item);
    // 完成字典剩余部分。当前已被判为字典、第一项已解析为 first。不消耗括号、不涉及 paren_depth_
    [[nodiscard]] AstNodePtr finish_dict(Position start_pos, AstNodePtr first);
    // 完成解析形参列表。消耗括号、管理 paren_depth_
    [[nodiscard]] AstNodeFunc::AllParams finish_func_params();
    // 完成解析捕获列表。消耗括号、管理 paren_depth_
    [[nodiscard]] std::vector<OneCapture> finish_captures();
    // 完成函数调用 f(...)。消耗括号、管理 paren_depth_
    [[nodiscard]] AstNodePtr finish_call(AstNodePtr obj, Position start_pos);
    // 完成索引 x[...]。消耗括号、管理 paren_depth_
    [[nodiscard]] AstNodePtr finish_index(AstNodePtr obj, Position start_pos);

  public:
    /**
     * 构造 Lexer 对象
     * @param tokens    由 Lexer 输出的 tokens
     * @param file_path 文件路径，默认为 "<unknown>"
     */
    explicit Parser(std::vector<Token> tokens, std::string file_path = "<unknown>");

    /**
     * 将 tokens 解析成 AST，只能调用一次（右值限定）
     * @return 解析后的 AST 的根节点
     */
    [[nodiscard]] AstNodeProgramPtr parse() &&;
};
