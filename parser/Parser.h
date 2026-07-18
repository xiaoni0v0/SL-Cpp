#pragma once

#include "../lexer/token.h"
#include "ast_nodes/ast_nodes.h"

#include <functional>
#include <string>
#include <vector>


class Parser {
    const std::vector<Token> tokens_;
    size_t pos_{0}; // 当前 token 的索引
    int paren_depth_{0}; // 未闭合的 '(' 和 '[' 深度（不含 '{'）
    const std::string file_path_;

    // 往后看 token
    [[nodiscard]] const Token &peek() const;
    // 消耗 token
    const Token &advance();
    // 检查下一个 token 类型是否为 type
    [[nodiscard]] bool check(TokenType type) const;
    // 检查洗一个 token 类型是否为 type，但是忽略 NEWLINE
    [[nodiscard]] bool check_over_newline(TokenType type) const;
    // 消耗对应类型 token，否则抛出异常
    const Token &expect(TokenType expected_type);
    // 无条件跳过 NEWLINE
    void skip_newline();
    // 仅在 paren_depth_ > 0（括号内）时跳过 NEWLINE
    void skip_paren_newline();
    // 无条件跳过 NEWLINE 和 ';'
    void skip_terminator();

    [[noreturn]] void error(const std::string &msg) const;
    [[noreturn]] void error(const std::string &msg, Position pos) const;

    // 检查当前位置是不是一条表达式合法的终止符（换行、';'、EOF、'}'），不是则抛语法错误
    void check_expr_terminator() const;

    // 括号内 "item (',' item)* [',']" 形式的逗号列表的公共部分：起始括号已消耗、paren_depth_ 已自增后调用；
    // 允许整个列表为空（不检测、不报错——是否允许空、空时该干什么由调用方自己决定）；
    // 每解析一项调用一次 parse_item，具体怎么解析、解析结果塞进哪个容器都由调用方的闭包决定；
    // 不消耗、不检查收尾的右括号——同样交给调用方（不同调用处的"未闭合"错误文案不一样）
    // @return 循环期间是否真的消耗过至少一个 ','（分组 (expr) 与单元素元组 (expr,) 靠这个区分）
    bool parse_comma_list(TokenType close, const std::function<void()> &parse_item);

    /**
     * 尽可能多地解析表达式，直到 EOF 或 '}'
     * 不消耗 EOF 或 '}'
     * @return 节点数组
     */
    std::vector<AstNodePtr> parse_exprs();

    /**
     * 解析一个表达式
     * 其实就是无前缀的 Pratt 解析
     * @return 节点
     */
    AstNodePtr parse_expr();

    /**
     * Pratt 解析
     * @param min_bp 最小绑定力
     * @return
     */
    AstNodePtr parse_expr_pratt(int min_bp);

    /**
     * 解析 if / while / for 中间槽的条件表达式：
     * 禁止裸的普通赋值 =
     * @return 节点
     */
    AstNodePtr parse_cond();

    /**
     * 解析一个无运算符的表达式
     * 不依赖左侧值
     * @return 节点
     */
    AstNodePtr parse_non_op();

    // 分组 (expr) 或者元组 (expr1, expr2)
    AstNodePtr parse_paren_or_tuple();
    // 列表 [expr1, expr2]
    AstNodePtr parse_list();
    // 字典 {k1: v1, ...} 或复合表达式
    AstNodePtr parse_brace_block();
    // parse_brace_block 的内部：'{' 已消耗、paren_depth_ 已清零后调用，解析到并消耗 '}'
    AstNodePtr finish_brace_block(Position start_pos);
    // 字典分支：finish_brace_block 已判别为字典、第一项的 key（或整个 '**' 展开项）已解析为
    // first 后调用，解析剩余部分到并消耗 '}'
    AstNodePtr finish_dict(Position start_pos, AstNodePtr first, bool is_first_doublestar);
    // del
    AstNodePtr parse_del();
    // global
    AstNodePtr parse_global();
    // if-elif-else
    AstNodePtr parse_if();
    // for
    AstNodePtr parse_for();
    // while
    AstNodePtr parse_while();
    // try-except-finally
    AstNodePtr parse_try();
    // return
    AstNodePtr parse_return();
    // raise
    AstNodePtr parse_raise();
    // 函数
    // decorators 是已经解析好、紧邻在 func 前面的前缀装饰器
    AstNodePtr parse_func(std::vector<AstNodePtr> decorators = {},
                          std::vector<Position> decorator_positions = {},
                          Position deco_pos = {});
    // 类
    AstNodePtr parse_class(std::vector<AstNodePtr> decorators = {},
                           std::vector<Position> decorator_positions = {},
                           Position deco_pos = {});
    // 装饰器
    // 先收集连续的前缀 @decorator，再看紧跟的是 func/class 还是任意表达式
    AstNodePtr parse_decorator();

    // '(' 已消耗、paren_depth_ 已自增后调用；paren_pos 是这个 '(' 自己的位置
    AstNodePtr finish_call(AstNodePtr callee, Position pos, Position paren_pos);
    // '[' 已消耗、paren_depth_ 已自增后调用；bracket_pos 是这个 '[' 自己的位置
    AstNodePtr finish_index(AstNodePtr obj, Position pos, Position bracket_pos);
    // 检测当前位置是否为关键字参数（IDENTIFIER 之后跳过 NEWLINE 见到 '='）
    [[nodiscard]] bool at_kwarg() const;

    // 单个函数形参：*args / **kwargs / 普通形参（可选类型注解、可选默认值）
    AstNodeFunc::OneParam parse_func_param();
    // 单个捕获项：identifier / identifier = expr / &identifier
    AstNodeFunc::OneCapture parse_func_capture();
    // '[' 已消耗后调用，解析到并消耗 ']'
    std::vector<AstNodeFunc::OneCapture> finish_func_captures();
    // '(' 已消耗、paren_depth_ 已自增后调用，解析到并消耗 ')'（基类列表，仅位置参数）
    std::vector<AstNodePtr> finish_class_bases();

    // 链式比较（< <= > >= == !=）：left 已解析完毕，first_op 是刚 advance 掉的第一个比较运算符
    // 对应的 AstNodeCompare::OpType（调用处已经用 token_type_to_compare_op_type 转换过），
    // first_op_pos 是这个运算符自己的位置（存进 AstNodeCompare::op_positions_）
    AstNodePtr parse_compare_chain(AstNodePtr left, Position start_pos, AstNodeCompare::OpType first_op,
                                   Position first_op_pos);
    // 链式 is：left 已解析完毕，第一个 'is' 已被 advance 掉；is 不可重载，不与上面共用 AstNodeCompare
    // first_is_pos 是第一个 'is' 自己的位置（存进 AstNodeIs::op_positions_）
    AstNodePtr parse_is_chain(AstNodePtr left, Position start_pos, Position first_is_pos);

public:
    /**
     * 构造 Lexer 对象
     * @param tokens    由 Lexer 输出的 tokens
     * @param file_path 文件路径，默认为 "<unknown>"
     */
    explicit Parser(std::vector<Token> tokens, std::string file_path);

    /**
     * 将 tokens 解析成 AST，只能调用一次（右值限定）
     * @return 解析后的 AST 的根节点
     */
    AstNodeProgramPtr parse() &&;
};
