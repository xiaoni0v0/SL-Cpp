#pragma once

#include "../../common/Position.h"
#include "../lexer/token.h"
#include "ast_nodes/ast_nodes.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stack>
#include <string>
#include <type_traits>
#include <vector>

class Parser {
    const std::vector<Token> tokens_;
    std::size_t pos_{0}; // 当前 token 的索引
    const std::string file_path_;

    // 一层未闭合的括号
    enum class Bracket : std::uint8_t {
        Paren,     // ( )
        Square,    // [ ]
        Brace,     // 复合表达式、函数体、类体的 { }
        ForHeader, // for 头部的 ( )
        Call,      // 调用的 ( )
        DictSet,   // 字典、集合字面量 ${ } 里的 { }
    };

    std::stack<Bracket> brackets_; // 所有未闭合的括号，栈顶是最内层

    // 往后看 token
    [[nodiscard]] const Token &peek() const;
    // 消耗 token。调用方保证 pos_ < tokens_.size()
    const Token &advance();
    // 检查下一个 token 类型是否为 type
    [[nodiscard]] bool check(TokenType type) const;
    // 检查从 pos_ + offset 开始，跳过 NEWLINE 之后的第一个 token 是否为 type
    [[nodiscard]] bool check_over_newline(TokenType type, std::size_t offset = 0) const;
    // 检查当前位置是不是一条表达式合法的终止符（换行、';'、EOF、'}'），不是则抛语法错误
    void check_terminator() const;
    // 消耗对应类型 token，否则抛出异常
    const Token &expect(TokenType expected_type);
    // 消耗一个左括号并入栈。每个左括号都得走这里、每个右括号走 expect_close，配对关系一眼可见
    const Token &expect_open(Bracket kind);
    // 消耗一个右括号并出栈，顺便核对弹出的跟传进来的是同一种
    const Token &expect_close(Bracket kind);
    // 无条件跳过 NEWLINE
    void skip_newline();
    // 仅当最内层那对括号是 Paren、Square、Call、DictSet 时跳过 NEWLINE（ForHeader、Brace 不跳）
    void skip_paren_newline();
    // 无条件跳过 NEWLINE 和 ';'
    void skip_terminator();

    [[noreturn]] void
    error(const std::string &msg, std::optional<Position> pos = std::nullopt) const;
    [[noreturn]] void error_internal(const std::string &msg) const;
    [[noreturn]] void error_internal(const std::string &msg, Position pos) const;

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
     * 继续解析链式比较（< <= > >= == !=），左操作数和首个运算符已读取
     * @param left         左操作数
     * @param start_pos    整个表达式开始的位置
     * @param first_op     第一个运算符的类型
     * @param first_op_pos 第一个运算符的位置
     */
    [[nodiscard]] AstNodePtr finish_chain_compare(
        AstNodePtr left, Position start_pos, AstNodeCompare::OpType first_op, Position first_op_pos
    );

    /**
     * 继续解析 is 链，左操作数和首个 is 已读取
     * @param left         左操作数
     * @param start_pos    整个表达式开始的位置
     * @param first_is_pos 第一个 is 的位置
     */
    [[nodiscard]] AstNodePtr
    finish_chain_is(AstNodePtr left, Position start_pos, Position first_is_pos);

    /**
     * 解析一个无运算符的表达式
     * 不依赖左侧值
     * @return 节点
     */
    [[nodiscard]] AstNodePtr parse_non_op();

    // 解析 if / for 中间槽 / while 的条件表达式
    [[nodiscard]] AstNodePtr parse_expr_as_cond();

    // 解析 for / while 紧跟在关键字之后的收集模式记号（可以没有）
    [[nodiscard]] CollectMark parse_collect_mark();

    // 分组 (expr) 或者元组 (expr1, expr2)
    [[nodiscard]] AstNodePtr parse_paren_or_tuple();
    // 列表 [expr1, expr2]
    [[nodiscard]] AstNodePtr parse_list();
    // 字典 ${k1 -> v1, ...} 或集合 ${a, b, ...}
    [[nodiscard]] AstNodePtr parse_dict_or_set();
    // 复合表达式 { expr1; expr2 }
    [[nodiscard]] AstNodePtr parse_compound();

    // del
    [[nodiscard]] AstNodePtr parse_del();
    // global
    [[nodiscard]] AstNodePtr parse_global();
    // import（关键字形态出 AstNodeImportKw，调用形态出 AstNodeImportCall）
    [[nodiscard]] AstNodePtr parse_import();

    // if-elif-else
    [[nodiscard]] AstNodePtr parse_if();
    // for（步进模式出 AstNodeForCond，迭代模式出 AstNodeForIter）
    [[nodiscard]] AstNodePtr parse_for();
    // while（出 AstNodeForCond，只是 init/inc 恒空）
    [[nodiscard]] AstNodePtr parse_while();
    // return
    [[nodiscard]] AstNodePtr parse_return();
    // try-except-finally
    [[nodiscard]] AstNodePtr parse_try();
    // raise
    [[nodiscard]] AstNodePtr parse_raise();
    // 函数
    [[nodiscard]] AstNodePtr parse_func(
        std::vector<AstNodePtr> decorators = {}, std::vector<Position> positions_decorator = {},
        Position deco_pos = {}
    );
    // 类
    [[nodiscard]] AstNodePtr parse_class(
        std::vector<AstNodePtr> decorators = {}, std::vector<Position> positions_decorator = {},
        Position deco_pos = {}
    );
    // 装饰器表达式 / 函数 / 类
    [[nodiscard]] AstNodePtr parse_decorator();
    // eval(code)
    [[nodiscard]] AstNodePtr parse_eval();

    /**
     * 解析逗号分隔的一批项，允许空批和尾逗号。不消耗括号、不动括号栈。
     * @param closing_type 结束括号的 token 类型
     * @param parse_item   解析一项的回调函数
     * @return             列表中是否出现过 ','
     */
    template <typename F>
        requires std::is_invocable_r_v<void, F>
    bool parse_comma_batch(TokenType closing_type, const F &parse_item);

    // 解析 `: ⟦const⟧ ⟦type⟧`，const 和类型至少一个；消耗 ':'
    [[nodiscard]] Annotation parse_annotation();
    // left 之后若紧跟 '='、':' 或复合赋值运算符，解析出整个赋值表达式；否则原样返回 left
    // 调用方按高于赋值的优先级解析出 left
    [[nodiscard]] AstNodePtr finish_assign(AstNodePtr left);
    // 解析 `as binding`；带注解时目标必须是标识符
    [[nodiscard]] Binding parse_binding();

    // 解析捕获列表。消耗括号、管理括号栈
    [[nodiscard]] std::vector<OneCapture> parse_captures();
    // 解析形参列表。消耗括号、管理括号栈
    [[nodiscard]] AstNodeFunc::AllParams parse_func_params();
    // 解析实参表 (...)：位置组 + 关键字组 + '(' 位置。消耗括号、管理括号栈
    [[nodiscard]] CallArgs parse_call_args();
    // object 已解析，继续解析调用 (...)
    [[nodiscard]] AstNodePtr finish_call(AstNodePtr object, Position start_pos);
    // object 已解析，继续解析索引 [...]。消耗括号、管理括号栈
    [[nodiscard]] AstNodePtr finish_index(AstNodePtr object, Position start_pos);

    explicit Parser(std::vector<Token> tokens, std::string file_path);

    // 两种解析主体。一个实例只跑其中一个、只跑一次
    [[nodiscard]] AstNodeProgramPtr run_program();
    [[nodiscard]] AstNodePtr run_single_expr();

  public:
    /**
     * 把整个输入当作一整份 Program 解析（`eval_isolated` 现构造出来的那份文件也走这里）
     * @param tokens    由 Lexer 输出的 tokens
     * @param file_path 文件路径，默认为 "<unknown>"
     * @return Program 根节点
     */
    [[nodiscard]] static AstNodeProgramPtr
    parse_program(std::vector<Token> tokens, std::string file_path = "<unknown>");

    /**
     * 把整个输入当作恰好一条表达式解析（`eval` 的解析入口）
     * @param tokens    由 Lexer 输出的 tokens
     * @param file_path 文件路径，默认为 "<unknown>"
     * @return 这条表达式的节点
     */
    [[nodiscard]] static AstNodePtr
    parse_single_expr(std::vector<Token> tokens, std::string file_path = "<unknown>");
};
