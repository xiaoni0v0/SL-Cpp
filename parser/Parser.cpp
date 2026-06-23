#include "Parser.h"

#include "../builtins/classes/exceptions/SyntaxError.h"
#include "../lexer/Lexer.h"
#include "../utils/string_utils.h"

#include <algorithm>
#include <cassert>
#include <format>
#include <memory>
#include <optional>

// token 类型转换为一元运算符类型
static AstNodeOpUnary::OpType token_type_to_unary_op_type(const TokenType t) {
    switch (t) {
    case TokenType::SIGN_DOUBLEPLUS: return AstNodeOpUnary::OpType::Inc;
    case TokenType::SIGN_DOUBLEMINUS: return AstNodeOpUnary::OpType::Dec;
    case TokenType::SIGN_PLUS: return AstNodeOpUnary::OpType::Pos;
    case TokenType::SIGN_MINUS: return AstNodeOpUnary::OpType::Neg;
    case TokenType::SIGN_TILDE: return AstNodeOpUnary::OpType::BitNot;
    case TokenType::KW_NOT: return AstNodeOpUnary::OpType::Not;
    case TokenType::SIGN_QUESTION: return AstNodeOpUnary::OpType::Question;
    case TokenType::SIGN_EXCLAIM: return AstNodeOpUnary::OpType::Exclaim;
    default:
        assert(false && "not a unary op token");
    }
}

// token 类型转换为二元运算符类型
static AstNodeOpBinary::OpType token_type_to_binary_op_type(const TokenType t) {
    switch (t) {
    case TokenType::SIGN_PLUS: return AstNodeOpBinary::OpType::Add;
    case TokenType::SIGN_MINUS: return AstNodeOpBinary::OpType::Sub;
    case TokenType::SIGN_STAR: return AstNodeOpBinary::OpType::Mul;
    case TokenType::SIGN_SLASH: return AstNodeOpBinary::OpType::Div;
    case TokenType::SIGN_DOUBLESLASH: return AstNodeOpBinary::OpType::DivFloor;
    case TokenType::SIGN_PERCENT: return AstNodeOpBinary::OpType::Mod;
    case TokenType::SIGN_DOUBLESTAR: return AstNodeOpBinary::OpType::Pow;
    case TokenType::SIGN_AMPERSAND: return AstNodeOpBinary::OpType::BitAnd;
    case TokenType::SIGN_PIPE: return AstNodeOpBinary::OpType::BitOr;
    case TokenType::SIGN_CARET: return AstNodeOpBinary::OpType::BitXor;
    case TokenType::SIGN_LSHIFT: return AstNodeOpBinary::OpType::LShift;
    case TokenType::SIGN_RSHIFT: return AstNodeOpBinary::OpType::RShift;
    case TokenType::SIGN_EQ: return AstNodeOpBinary::OpType::Eq;
    case TokenType::SIGN_NEQ: return AstNodeOpBinary::OpType::Ne;
    case TokenType::SIGN_LT: return AstNodeOpBinary::OpType::Lt;
    case TokenType::SIGN_LE: return AstNodeOpBinary::OpType::Le;
    case TokenType::SIGN_GT: return AstNodeOpBinary::OpType::Gt;
    case TokenType::SIGN_GE: return AstNodeOpBinary::OpType::Ge;
    case TokenType::KW_IS: return AstNodeOpBinary::OpType::Is;
    case TokenType::KW_AND: return AstNodeOpBinary::OpType::And;
    case TokenType::KW_OR: return AstNodeOpBinary::OpType::Or;
    case TokenType::SIGN_DOTDOT: return AstNodeOpBinary::OpType::Range;
    default:
        assert(false && "not a binary op token");
    }
}

// 运算符绑定力表
// 对中缀/后缀运算符，返回 {lbp, rbp}
// {-1,-1} 表示不是中缀/后缀运算符
static std::pair<int, int> infix_bp(const TokenType type) {
    switch (type) {
    case TokenType::SIGN_DOT: return {160, 160};
    case TokenType::SIGN_LPAREN:
    case TokenType::SIGN_LBRACKET: return {160, -1}; // 函数调用、索引
    case TokenType::SIGN_QUESTION:
    case TokenType::SIGN_EXCLAIM: return {150, -1}; // ? !
    case TokenType::SIGN_DOUBLESTAR: return {140, 139}; // **
    case TokenType::SIGN_STAR:
    case TokenType::SIGN_SLASH:
    case TokenType::SIGN_DOUBLESLASH:
    case TokenType::SIGN_PERCENT: return {120, 120}; // * / // %
    case TokenType::SIGN_PLUS:
    case TokenType::SIGN_MINUS: return {110, 110}; // + -
    case TokenType::SIGN_DOTDOT: return {100, 100}; // ..
    case TokenType::SIGN_LSHIFT:
    case TokenType::SIGN_RSHIFT: return {90, 90}; // << >>
    case TokenType::SIGN_AMPERSAND: return {80, 80}; // &
    case TokenType::SIGN_CARET: return {70, 70}; // ^
    case TokenType::SIGN_PIPE: return {60, 60}; // |
    case TokenType::SIGN_LT:
    case TokenType::SIGN_LE:
    case TokenType::SIGN_GT:
    case TokenType::SIGN_GE:
    case TokenType::SIGN_EQ:
    case TokenType::SIGN_NEQ:
    case TokenType::KW_IS: return {50, 50}; // < <= > >= == != is
    case TokenType::KW_AND: return {30, 30}; // and
    case TokenType::KW_OR: return {20, 20}; // or
    // 赋值（右结合）
    case TokenType::SIGN_ASSIGN:
    case TokenType::SIGN_PLUS_ASSIGN:
    case TokenType::SIGN_MINUS_ASSIGN:
    case TokenType::SIGN_STAR_ASSIGN:
    case TokenType::SIGN_DOUBLESTAR_ASSIGN:
    case TokenType::SIGN_SLASH_ASSIGN:
    case TokenType::SIGN_DOUBLESLASH_ASSIGN:
    case TokenType::SIGN_PERCENT_ASSIGN:
    case TokenType::SIGN_AMPERSAND_ASSIGN:
    case TokenType::SIGN_PIPE_ASSIGN:
    case TokenType::SIGN_CARET_ASSIGN:
    case TokenType::SIGN_LSHIFT_ASSIGN:
    case TokenType::SIGN_RSHIFT_ASSIGN: return {10, 9};
    default: return {-1, -1};
    }
}

// 返回是否是赋值运算符
static bool is_assign_op(const TokenType type) {
    switch (type) {
    case TokenType::SIGN_ASSIGN:
    case TokenType::SIGN_PLUS_ASSIGN:
    case TokenType::SIGN_MINUS_ASSIGN:
    case TokenType::SIGN_STAR_ASSIGN:
    case TokenType::SIGN_DOUBLESTAR_ASSIGN:
    case TokenType::SIGN_SLASH_ASSIGN:
    case TokenType::SIGN_DOUBLESLASH_ASSIGN:
    case TokenType::SIGN_PERCENT_ASSIGN:
    case TokenType::SIGN_AMPERSAND_ASSIGN:
    case TokenType::SIGN_PIPE_ASSIGN:
    case TokenType::SIGN_CARET_ASSIGN:
    case TokenType::SIGN_LSHIFT_ASSIGN:
    case TokenType::SIGN_RSHIFT_ASSIGN: return true;
    default: return false;
    }
}

// 复合赋值 op= 对应的二元运算符
static AstNodeOpBinary::OpType assign_compound_to_binary(const TokenType op) {
    switch (op) {
    case TokenType::SIGN_PLUS_ASSIGN: return AstNodeOpBinary::OpType::Add;
    case TokenType::SIGN_MINUS_ASSIGN: return AstNodeOpBinary::OpType::Sub;
    case TokenType::SIGN_STAR_ASSIGN: return AstNodeOpBinary::OpType::Mul;
    case TokenType::SIGN_DOUBLESTAR_ASSIGN: return AstNodeOpBinary::OpType::Pow;
    case TokenType::SIGN_SLASH_ASSIGN: return AstNodeOpBinary::OpType::Div;
    case TokenType::SIGN_DOUBLESLASH_ASSIGN: return AstNodeOpBinary::OpType::DivFloor;
    case TokenType::SIGN_PERCENT_ASSIGN: return AstNodeOpBinary::OpType::Mod;
    case TokenType::SIGN_AMPERSAND_ASSIGN: return AstNodeOpBinary::OpType::BitAnd;
    case TokenType::SIGN_PIPE_ASSIGN: return AstNodeOpBinary::OpType::BitOr;
    case TokenType::SIGN_CARET_ASSIGN: return AstNodeOpBinary::OpType::BitXor;
    case TokenType::SIGN_LSHIFT_ASSIGN: return AstNodeOpBinary::OpType::LShift;
    case TokenType::SIGN_RSHIFT_ASSIGN: return AstNodeOpBinary::OpType::RShift;
    default:
        assert(false && "not a compound assign op");
    }
}

const Token &Parser::peek() const {
    return pos_ < tokens_.size() ? tokens_[pos_] : tokens_.back();
}

const Token &Parser::advance() {
    assert(pos_ < tokens_.size());
    return tokens_[pos_++];
}

bool Parser::check(const TokenType type) const {
    return pos_ < tokens_.size() && tokens_[pos_].type == type;
}

bool Parser::check_over_newline(const TokenType type) const {
    const size_t len{tokens_.size()};
    for (size_t i{pos_}; i < len; i++) {
        if (tokens_[i].type != TokenType::NEWLINE) {
            return tokens_[i].type == type;
        }
    }
    return false;
}

const Token &Parser::expect(const TokenType expected_type) {
    if (const Token &token{peek()}; token.type != expected_type) {
        error(std::format("expected {} but got {}",
                          Lexer::get_typename_by_tokentype(expected_type),
                          Lexer::get_typename_by_tokentype(token.type)),
              token.row, token.col);
    }
    return advance();
}

void Parser::skip_newline() {
    while (check(TokenType::NEWLINE)) advance();
}

void Parser::skip_paren_newline() {
    while (paren_depth_ > 0 && check(TokenType::NEWLINE)) advance();
}

void Parser::skip_terminator() {
    while (check(TokenType::NEWLINE) || check(TokenType::SIGN_SEMICOLON)) advance();
}

void Parser::error(const std::string &msg) const {
    const Token &token{peek()};
    throw SyntaxError{file_path_, token.row, token.col, msg};
}

void Parser::error(const std::string &msg, const int row, const int col) const {
    throw SyntaxError{file_path_, row, col, msg};
}

std::vector<AstNodePtr> Parser::parse_exprs() {
    std::vector<AstNodePtr> exprs;

    // 跳过前导终止符
    skip_terminator();

    // 不是 EOF 也不是 }
    while (!check(TokenType::END_OF_FILE) && !check(TokenType::SIGN_RBRACE)) {
        exprs.push_back(parse_expr());

        // 如果一个表达式结尾了还不是换行、';'、EOF、}，则语法错误
        if (!(
            check(TokenType::NEWLINE) || check(TokenType::SIGN_SEMICOLON)
            || check(TokenType::END_OF_FILE) || check(TokenType::SIGN_RBRACE)
        )) {
            error("expected newline or ';' after expression (newline recommended)");
        }

        // 消耗剩余终止符
        skip_terminator();
    }

    return exprs;
}

AstNodePtr Parser::parse_expr() {
    // 委托给 parse_expr_pratt
    // 无前缀运算符，故 min_bp = 0
    return parse_expr_pratt(0);
}

AstNodePtr Parser::parse_expr_pratt(const int min_bp) {
    AstNodePtr left{parse_non_op()};

    const int start_row{left->row_}, start_col{left->col_};

    while (true) {
        // 括号内允许运算符前换行（如多行链式调用）
        skip_paren_newline();

        const auto &[op, lexeme, op_row, op_col]{peek()};
        const auto [lbp, rbp]{infix_bp(op)};

        // lbp == -1 表示非中缀/后缀运算符；lbp < min_bp 表示绑定力不足，让上层处理
        if (lbp < min_bp) break;

        advance(); // 消耗运算符

        // 赋值（右结合，rbp = lbp - 1 = 9）
        if (op == TokenType::SIGN_ASSIGN) {
            skip_newline();
            return std::make_unique<AstNodeAssign>(
                start_row, start_col, std::move(left), parse_expr_pratt(rbp)
                );
        }

        // 复合赋值 x op= y
        if (is_assign_op(op)) {
            skip_newline();
            return std::make_unique<AstNodeCompoundAssign>(
                start_row, start_col, std::move(left),
                assign_compound_to_binary(op), parse_expr_pratt(rbp)
                );
        }

        // 后缀 x?  x!
        if (op == TokenType::SIGN_QUESTION || op == TokenType::SIGN_EXCLAIM) {
            left = std::make_unique<AstNodeOpUnary>(op_row, op_col, token_type_to_unary_op_type(op), std::move(left));
            continue;
        }

        // 函数调用 f(...)
        if (op == TokenType::SIGN_LPAREN) {
            paren_depth_++;
            left = finish_call(std::move(left), op_row, op_col);
            continue;
        }

        // 索引 x[...]
        if (op == TokenType::SIGN_LBRACKET) {
            paren_depth_++;
            left = finish_index(std::move(left), op_row, op_col);
            continue;
        }

        // 属性访问 x.attr（'.' 在行尾时 attr 可换行）
        if (op == TokenType::SIGN_DOT) {
            skip_newline();
            left = std::make_unique<AstNodeAttr>(
                op_row, op_col, std::move(left), expect(TokenType::IDENTIFIER).lexeme
                );
            continue;
        }

        // 普通二元运算符（运算符在行尾时右侧可换行）
        skip_newline();
        left = std::make_unique<AstNodeOpBinary>(
            op_row, op_col, token_type_to_binary_op_type(op),
            std::move(left), parse_expr_pratt(rbp)
            );
    }

    return left;
}

AstNodePtr Parser::parse_non_op() {
    // 跳过前导换行
    skip_newline();

    switch (const auto &[type, lexeme, row, col]{peek()}; type) {
    // 字面量
    case TokenType::LITERAL_NONE: return advance(), std::make_unique<AstNodeLiteralNone>(row, col);
    case TokenType::LITERAL_TRUE: return advance(), std::make_unique<AstNodeLiteralBool>(row, col, true);
    case TokenType::LITERAL_FALSE: return advance(), std::make_unique<AstNodeLiteralBool>(row, col, false);
    case TokenType::LITERAL_G:
        // _G
        return advance(), std::make_unique<AstNodeLiteralGL>(row, col, AstNodeLiteralGL::GLType::G);
    case TokenType::LITERAL_L:
        // _L
        return advance(), std::make_unique<AstNodeLiteralGL>(row, col, AstNodeLiteralGL::GLType::L);
    case TokenType::LITERAL_ELLIPSIS: return advance(), std::make_unique<AstNodeLiteralEllipsis>(row, col);
    case TokenType::LITERAL_INT: return std::make_unique<AstNodeLiteralInt>(row, col, advance().lexeme);
    case TokenType::LITERAL_FLOAT: return std::make_unique<AstNodeLiteralFloat>(row, col, advance().lexeme);
    case TokenType::LITERAL_STR: return std::make_unique<AstNodeLiteralStr>(row, col, advance().lexeme);

    // 分组 / 元组
    case TokenType::SIGN_LPAREN: return parse_paren_or_tuple();
    // 列表
    case TokenType::SIGN_LBRACKET: return parse_list();
    // 复合表达式
    case TokenType::SIGN_LBRACE: return parse_brace_block();

    // 标识符
    case TokenType::IDENTIFIER: return std::make_unique<AstNodeIdentifier>(row, col, advance().lexeme);

    // 解包 / 展开
    case TokenType::SIGN_STAR:
        // *iterable
        return advance(), std::make_unique<AstNodeStar>(row, col, parse_expr_pratt(130));
    case TokenType::SIGN_DOUBLESTAR:
        // **mapping
        return advance(), std::make_unique<AstNodeDoubleStar>(row, col, parse_expr_pratt(130));

    // 前缀运算符
    case TokenType::SIGN_DOUBLEPLUS:
    case TokenType::SIGN_DOUBLEMINUS:
    case TokenType::SIGN_PLUS:
    case TokenType::SIGN_MINUS:
    case TokenType::SIGN_TILDE: {
        // 单目优先级 130
        const TokenType token_type{advance().type};
        return std::make_unique<AstNodeOpUnary>(
            row, col, token_type_to_unary_op_type(token_type), parse_expr_pratt(130)
            );
    }
    case TokenType::KW_NOT:
        // not 优先级 40
        return advance(), std::make_unique<AstNodeOpUnary>(row, col, AstNodeOpUnary::OpType::Not, parse_expr_pratt(40));

    // del / global
    case TokenType::KW_DEL: return parse_del();
    case TokenType::KW_GLOBAL: return parse_global();

    // 控制流
    case TokenType::KW_IF: return parse_if();
    case TokenType::KW_FOR: return parse_for();
    case TokenType::KW_BREAK: return advance(), std::make_unique<AstNodeBreak>(row, col);
    case TokenType::KW_CONTINUE: return advance(), std::make_unique<AstNodeContinue>(row, col);
    case TokenType::KW_RETURN: return parse_return();
    case TokenType::KW_TRY: return parse_try();
    case TokenType::KW_RAISE: return parse_raise();

    // 函数
    case TokenType::KW_FUNC: return parse_func();

    // 装饰器
    case TokenType::SIGN_AT: return parse_decorator();

    // 类
    case TokenType::KW_CLASS: error("'class' is not yet implemented", row, col);

    // 遇到 EOF：括号内多半是没闭合，否则是缺了表达式
    case TokenType::END_OF_FILE: {
        if (paren_depth_ > 0) error("unexpected end of file (unclosed bracket)", row, col);
        error("unexpected end of file (expected an expression)", row, col);
    }

    // 错误
    default: error(std::format("unexpected token '{}'", u32_to_utf8(lexeme)), row, col);
    }
}

AstNodePtr Parser::parse_paren_or_tuple() {
    const int start_row{peek().row}, start_col{peek().col};

    expect(TokenType::SIGN_LPAREN); // 消耗 '('
    paren_depth_++;

    skip_newline();

    // 空元组 ()
    if (check(TokenType::SIGN_RPAREN)) {
        advance(); // 消耗 ')'
        paren_depth_--;
        return std::make_unique<AstNodeLiteralTuple>(start_row, start_col, std::vector<AstNodePtr>{});
    }

    // 第一个表达式
    AstNodePtr first_item{parse_expr()};

    skip_newline();

    // 分组 (expr)
    if (check(TokenType::SIGN_RPAREN)) {
        advance(); // 消耗 ')'
        paren_depth_--;
        return first_item;
    }

    std::vector<AstNodePtr> items;
    items.push_back(std::move(first_item));

    while (check(TokenType::SIGN_COMMA)) {
        advance(); // 消耗 ','
        skip_newline();
        if (check(TokenType::SIGN_RPAREN)) break; // 尾逗号
        items.push_back(parse_expr());
        skip_newline();
    }

    if (!check(TokenType::SIGN_RPAREN)) error("expected ')' to close tuple");
    advance(); // 消耗 ')'
    paren_depth_--;

    return std::make_unique<AstNodeLiteralTuple>(start_row, start_col, std::move(items));
}

AstNodePtr Parser::parse_list() {
    const int start_row{peek().row}, start_col{peek().col};

    expect(TokenType::SIGN_LBRACKET); // 消耗 '['
    paren_depth_++;

    skip_newline();

    // 空列表 []
    if (check(TokenType::SIGN_RBRACKET)) {
        advance(); // 消耗 ']'
        paren_depth_--;
        return std::make_unique<AstNodeLiteralList>(start_row, start_col, std::vector<AstNodePtr>{});
    }

    std::vector<AstNodePtr> items;
    items.push_back(parse_expr());

    skip_newline();

    while (check(TokenType::SIGN_COMMA)) {
        advance(); // 消耗 ','
        skip_newline();
        if (check(TokenType::SIGN_RBRACKET)) break; // 尾逗号
        items.push_back(parse_expr());
        skip_newline();
    }

    if (!check(TokenType::SIGN_RBRACKET)) error("expected ']' to close list literal");
    advance(); // 消耗 ']'
    paren_depth_--;

    return std::make_unique<AstNodeLiteralList>(start_row, start_col, std::move(items));
}

AstNodePtr Parser::parse_brace_block() {
    const int start_row{peek().row}, start_col{peek().col};

    expect(TokenType::SIGN_LBRACE); // 消耗 '{'

    // 跳过前导终止符
    skip_terminator();

    // 空 {} → 空的复合表达式
    if (check(TokenType::SIGN_RBRACE)) {
        advance();
        return std::make_unique<AstNodeCompound>(start_row, start_col, std::vector<AstNodePtr>{});
    }

    // ** 开头必定是字典展开项，否则先解析第一个表达式再看 ':'
    const bool first_is_spread{check(TokenType::SIGN_DOUBLESTAR)};
    AstNodePtr first{parse_expr()};

    // 字典字面量 {k: v, ...} 或 {**d, ...}
    if (first_is_spread || check_over_newline(TokenType::SIGN_COLON)) {
        std::vector<std::pair<AstNodePtr, AstNodePtr>> items;

        if (first_is_spread) {
            items.emplace_back(std::move(first), nullptr); // **expr，无 value
        } else {
            skip_newline();
            advance(); // 消耗 ':'
            skip_newline();
            items.emplace_back(std::move(first), parse_expr());
        }

        skip_newline();
        while (check(TokenType::SIGN_COMMA)) {
            advance();
            skip_newline();
            if (check(TokenType::SIGN_RBRACE)) break; // 尾逗号
            AstNodePtr key{parse_expr()};
            skip_newline();
            if (check(TokenType::SIGN_COLON)) {
                advance(); // 消耗 ':'
                skip_newline();
                items.emplace_back(std::move(key), parse_expr());
            } else {
                // **expr 展开项（无冒号），语义层校验 key 必须是 AstNodeDoubleStar
                items.emplace_back(std::move(key), nullptr);
            }
            skip_newline();
        }

        skip_newline();
        expect(TokenType::SIGN_RBRACE);
        return std::make_unique<AstNodeLiteralDict>(start_row, start_col, std::move(items));
    }

    // 复合表达式 {expr; ...}
    std::vector<AstNodePtr> exprs;
    exprs.push_back(std::move(first));
    for (AstNodePtr &e : parse_exprs()) exprs.push_back(std::move(e));
    expect(TokenType::SIGN_RBRACE);
    return std::make_unique<AstNodeCompound>(start_row, start_col, std::move(exprs));
}

AstNodePtr Parser::parse_del() {
    const int start_row{peek().row}, start_col{peek().col};
    expect(TokenType::KW_DEL);
    skip_newline();
    // 语法层只解析一个表达式，target 是否为标识符由语义层校验
    return std::make_unique<AstNodeDel>(start_row, start_col, parse_expr());
}

AstNodePtr Parser::parse_global() {
    const int start_row{peek().row}, start_col{peek().col};
    expect(TokenType::KW_GLOBAL);
    skip_newline();
    // 语法层只解析一个表达式，target 是否为标识符由语义层校验
    return std::make_unique<AstNodeGlobal>(start_row, start_col, parse_expr());
}

AstNodePtr Parser::parse_if() {
    const int start_row{peek().row}, start_col{peek().col};
    expect(TokenType::KW_IF);

    std::vector<AstNodeIf::AstCondAndExpr> clauses;

    // 解析一个 if/elif 子句的条件和主体
    auto parse_cond_and_body{
        [&]() -> AstNodeIf::AstCondAndExpr {
            skip_newline();
            expect(TokenType::SIGN_LPAREN); // 消耗 '('
            paren_depth_++;
            AstNodePtr cond{parse_expr()};
            paren_depth_--;
            expect(TokenType::SIGN_RPAREN); // 消耗 ')'
            skip_newline();
            AstNodePtr body{parse_expr()};
            return {std::move(cond), std::move(body)};
        }
    };

    clauses.push_back(parse_cond_and_body());

    // 解析 elif 子句（可在下一行）
    while (check_over_newline(TokenType::KW_ELIF)) {
        skip_newline();
        expect(TokenType::KW_ELIF);
        clauses.push_back(parse_cond_and_body());
    }

    // 解析可选的 else（可在下一行）
    if (check_over_newline(TokenType::KW_ELSE)) {
        skip_newline();
        expect(TokenType::KW_ELSE);
        skip_newline();

        return std::make_unique<AstNodeIf>(start_row, start_col, std::move(clauses), parse_expr());
    }

    // 无 else
    return std::make_unique<AstNodeIf>(start_row, start_col, std::move(clauses), nullptr);
}

AstNodePtr Parser::parse_for() {
    const int start_row{peek().row}, start_col{peek().col};
    expect(TokenType::KW_FOR);
    skip_newline();

    const bool collect{check(TokenType::SIGN_DOLLAR)};
    if (collect) advance(); // 消耗 '$'
    skip_newline();

    expect(TokenType::SIGN_LPAREN); // 消耗 '('
    skip_newline();

    // 解析 for 头部的一个槽：遇到 ';', NEWLINE, ')' 则槽为空，返回 nullptr
    auto parse_slot{
        [&]() -> AstNodePtr {
            if (check(TokenType::SIGN_SEMICOLON) ||
                check(TokenType::NEWLINE) ||
                check(TokenType::SIGN_RPAREN))
                return nullptr;
            return parse_expr();
        }
    };

    // 先读第一个槽（可能为空）。它要么是迭代目标（后跟 ':'），要么是条件形式的 init
    AstNodePtr first{parse_slot()};

    // 1. 第一个槽后紧跟 ':' → 迭代形式：for [$] (target : iterable) body
    if (check_over_newline(TokenType::SIGN_COLON)) {
        skip_newline();
        advance(); // 消耗 ':'
        skip_newline();
        AstNodePtr iterable{parse_expr()};
        skip_newline();
        expect(TokenType::SIGN_RPAREN);
        skip_newline();
        AstNodePtr body{parse_expr()};
        return std::make_unique<AstNodeForIter>(
            start_row, start_col, collect,
            std::move(first), std::move(iterable), std::move(body)
            );
    }

    // 2. 第一个槽后紧跟 ')' → 条件形式：for [$] (cond) body，first 即 cond
    if (check_over_newline(TokenType::SIGN_RPAREN)) {
        // 条件形式要求括号内有一个表达式；空括号 for () 非法（无限循环请用 for (;;)）
        if (!first) error("empty for header");
        skip_newline();
        expect(TokenType::SIGN_RPAREN);
        skip_newline();
        AstNodePtr body{parse_expr()};
        return std::make_unique<AstNodeForCond>(
            start_row, start_col, collect,
            nullptr, std::move(first), nullptr, std::move(body)
            );
    }

    // 3. 否则为完整的计数-条件形式：for [$] (init SEP cond SEP inc) body，first 即 init
    //    SEP（分隔符）为 ';' 或至少一个换行；两个槽之间必须有 SEP，否则无法无歧义地分割
    auto consume_sep{
        [&] {
            if (!check(TokenType::NEWLINE) && !check(TokenType::SIGN_SEMICOLON)) {
                error("expected ';' or newline to separate the expressions in a for header");
            }
            skip_newline();
            if (check(TokenType::SIGN_SEMICOLON)) advance(); // 消耗 ';'
            skip_newline();
        }
    };

    consume_sep();
    AstNodePtr cond{parse_slot()};
    consume_sep();
    AstNodePtr inc{parse_slot()};
    skip_newline();

    expect(TokenType::SIGN_RPAREN); // 消耗 ')'
    skip_newline();
    AstNodePtr body{parse_expr()};

    return std::make_unique<AstNodeForCond>(
        start_row, start_col, collect,
        std::move(first), std::move(cond), std::move(inc), std::move(body)
        );
}

AstNodePtr Parser::parse_try() {
    const int start_row{peek().row}, start_col{peek().col};
    expect(TokenType::KW_TRY);

    // 与 if 不同：try 后没有 '()'，直接跟主体
    skip_newline();
    AstNodePtr try_expr{parse_expr()};

    // 解析一个 except 子句的异常列表和主体：except (Exc1, Exc2, ...) body
    // 与 if 的 parse_cond_and_body 平行，区别是括号内为一个或多个表达式
    auto parse_excs_and_body{
        [&]() -> AstNodeTry::AstExceptAndExpr {
            skip_newline();
            expect(TokenType::SIGN_LPAREN); // 消耗 '('
            paren_depth_++;
            skip_paren_newline();

            // 解析 Exception1, ...
            std::vector<AstNodePtr> excs;
            excs.push_back(parse_expr());
            skip_paren_newline();
            while (check(TokenType::SIGN_COMMA)) {
                advance(); // 消耗 ','
                skip_paren_newline();
                if (check(TokenType::SIGN_RPAREN)) break; // 尾逗号
                excs.push_back(parse_expr());
                skip_paren_newline();
            }

            skip_paren_newline();
            paren_depth_--;
            expect(TokenType::SIGN_RPAREN); // 消耗 ')'
            skip_newline();
            AstNodePtr body{parse_expr()};
            return {std::move(excs), std::move(body)};
        }
    };

    std::vector<AstNodeTry::AstExceptAndExpr> except_clauses;

    // 解析 except 子句（可在下一行）
    while (check_over_newline(TokenType::KW_EXCEPT)) {
        skip_newline();
        expect(TokenType::KW_EXCEPT);
        except_clauses.push_back(parse_excs_and_body());
    }

    // 解析可选的 finally（可在下一行）
    if (check_over_newline(TokenType::KW_FINALLY)) {
        skip_newline();
        expect(TokenType::KW_FINALLY);
        skip_newline();

        return std::make_unique<AstNodeTry>(
            start_row, start_col,
            std::move(try_expr), std::move(except_clauses), parse_expr()
            );
    }

    // 注：except / finally 至少有一个，这一约束交由语义层检查
    return std::make_unique<AstNodeTry>(
        start_row, start_col,
        std::move(try_expr), std::move(except_clauses), nullptr
        );
}

AstNodePtr Parser::parse_return() {
    const int start_row{peek().row}, start_col{peek().col};
    expect(TokenType::KW_RETURN);
    // 若紧跟终止符则为裸 return（值为 None）
    // 终止符 = 语句终止符 NEWLINE, ';', EOF, '}'
    //        + 括号语境的闭合 / 分隔符 ')', ']', ','（如 for(;;return)、(return,)、[return]、f(return)）
    if (check(TokenType::NEWLINE) || check(TokenType::SIGN_SEMICOLON) ||
        check(TokenType::END_OF_FILE) || check(TokenType::SIGN_RBRACE) ||
        check(TokenType::SIGN_RPAREN) || check(TokenType::SIGN_RBRACKET) ||
        check(TokenType::SIGN_COMMA))
        return std::make_unique<AstNodeReturn>(start_row, start_col, nullptr);

    return std::make_unique<AstNodeReturn>(start_row, start_col, parse_expr());
}

AstNodePtr Parser::parse_raise() {
    const int start_row{peek().row}, start_col{peek().col};
    expect(TokenType::KW_RAISE);
    skip_newline();
    return std::make_unique<AstNodeRaise>(start_row, start_col, parse_expr());
}

AstNodePtr Parser::parse_func() {
    const int start_row{peek().row}, start_col{peek().col};
    expect(TokenType::KW_FUNC);
    skip_newline();

    // 可选函数名（无名即匿名函数）
    std::optional<std::u32string> name;
    if (check(TokenType::IDENTIFIER)) {
        name = advance().lexeme;
        skip_newline();
    }

    // 形参列表（合法性由语义层检查）
    expect(TokenType::SIGN_LPAREN);
    paren_depth_++;
    skip_newline();

    std::vector<AstNodeFunc::Param> params{};
    params.push_back(parse_func_param());
    skip_newline();

    while (check(TokenType::SIGN_COMMA)) {
        advance(); // 消耗 ','
        skip_newline();
        if (check(TokenType::SIGN_RPAREN)) break; // 允许尾逗号
        params.push_back(parse_func_param());
        skip_newline();
    }

    paren_depth_--;
    expect(TokenType::SIGN_RPAREN);
    skip_newline();

    // 函数体：{ ... }
    const int body_row{peek().row}, body_col{peek().col};
    expect(TokenType::SIGN_LBRACE);
    std::vector body_exprs{parse_exprs()};
    expect(TokenType::SIGN_RBRACE);

    return std::make_unique<AstNodeFunc>(
        start_row, start_col, std::move(name), std::move(params),
        std::make_unique<AstNodeProgram>(body_row, body_col, std::move(body_exprs))
        );
}

AstNodePtr Parser::parse_decorator() {
    const int start_row{peek().row}, start_col{peek().col};
    expect(TokenType::SIGN_AT);
    skip_newline();

    // 解析装饰器表达式（Pratt 在遇到 func / @ 前会自然停止）
    AstNodePtr decorator = parse_expr();
    skip_newline();

    // 装饰在函数上
    if (check(TokenType::KW_FUNC)) {
        AstNodePtr target = parse_func();
        return std::make_unique<AstNodeDecorator>(start_row, start_col, std::move(decorator), std::move(target));
    }
    // 装饰在装饰器上
    if (check(TokenType::SIGN_AT)) {
        AstNodePtr target = parse_decorator(); // 链式装饰器
        return std::make_unique<AstNodeDecorator>(start_row, start_col, std::move(decorator), std::move(target));
    }

    error("expected 'func' or '@' after decorator", peek().row, peek().col);
}

AstNodePtr Parser::finish_call(AstNodePtr callee, int row, int col) {
    // '(' 已消耗，paren_depth_ 已自增
    std::vector<AstNodePtr> args;
    std::vector<std::pair<std::u32string, AstNodePtr>> kwargs;

    skip_newline();
    while (!check(TokenType::SIGN_RPAREN)) {
        skip_paren_newline();

        if (at_kwarg()) {
            // 关键字参数 name = value
            auto name = advance().lexeme; // IDENTIFIER
            skip_paren_newline();
            expect(TokenType::SIGN_ASSIGN);
            skip_paren_newline();
            kwargs.emplace_back(std::move(name), parse_expr());
        } else {
            // 位置参数，含 *expr / **expr 展开（参数顺序合法性由语义层校验）
            args.push_back(parse_expr());
        }

        skip_paren_newline();
        if (check(TokenType::SIGN_COMMA)) {
            advance();
            skip_paren_newline();
        } else {
            break;
        }
    }

    skip_newline();
    if (!check(TokenType::SIGN_RPAREN)) error("expected ')' to close function call", peek().row, peek().col);
    advance();
    paren_depth_--;

    return std::make_unique<AstNodeCall>(row, col, std::move(callee), std::move(args), std::move(kwargs));
}

AstNodePtr Parser::finish_index(AstNodePtr obj, int row, int col) {
    // '[' 已消耗，paren_depth_ 已自增
    skip_newline();

    // 遇到了 a[]，不允许，需要至少一个参数
    if (check(TokenType::SIGN_RBRACKET)) {
        error("expected at least one argument for indexing");
    }

    std::vector<AstNodePtr> args;
    args.push_back(parse_expr());

    skip_newline();

    while (check(TokenType::SIGN_COMMA)) {
        advance(); // 消耗 ','
        skip_newline();
        if (check(TokenType::SIGN_RBRACKET)) break; // 尾逗号
        args.push_back(parse_expr());
        skip_newline();
    }

    skip_newline();
    if (!check(TokenType::SIGN_RBRACKET))
        error("expected ']' to close index expression",
              peek().row, peek().col);

    advance();
    paren_depth_--;

    return std::make_unique<AstNodeIndex>(row, col, std::move(obj), std::move(args));
}

bool Parser::at_kwarg() const {
    if (!check(TokenType::IDENTIFIER)) return false;
    // 跳过 IDENTIFIER 之后可能的 NEWLINE，看是否为 '='
    size_t i = pos_ + 1;
    while (i < tokens_.size() && tokens_[i].type == TokenType::NEWLINE) ++i;
    return i < tokens_.size() && tokens_[i].type == TokenType::SIGN_ASSIGN;
}

AstNodeFunc::Param Parser::parse_func_param() {
    AstNodeFunc::Param p{};

    skip_newline();

    // **kwargs
    if (check(TokenType::SIGN_DOUBLESTAR)) {
        advance(); // 消耗 '**'
        skip_newline();
        p.param_type = AstNodeFunc::Param::ParamType::DoubleStarKwargs;
        p.identifier = expect(TokenType::IDENTIFIER).lexeme;
    }
    // *args
    else if (check(TokenType::SIGN_STAR)) {
        advance(); // 消耗 '*'
        skip_newline();
        p.param_type = AstNodeFunc::Param::ParamType::StarArgs;
        p.identifier = expect(TokenType::IDENTIFIER).lexeme;
    }
    // arg
    else {
        p.param_type = AstNodeFunc::Param::ParamType::Normal;
        p.identifier = expect(TokenType::IDENTIFIER).lexeme;
    }

    skip_newline();

    // 类型注解
    if (check(TokenType::SIGN_COLON)) {
        advance(); // 消耗 ':'
        skip_newline();
        p.type_annotation = parse_expr_pratt(11); // 停在 '=' 之前（lbp=10 < 11）
        skip_newline();
    }

    // 默认值
    if (check(TokenType::SIGN_ASSIGN)) {
        advance(); // 消耗 '='
        skip_newline();
        p.default_value = parse_expr();
        skip_newline();
    }

    return p;
}

Parser::Parser(std::vector<Token> tokens, std::string file_path)
    : tokens_{std::move(tokens)}, file_path_{std::move(file_path)} {
    // 1. 空的肯定不行
    if (tokens_.empty()) throw std::runtime_error{"Bad tokens: empty token list."};

    // 2. 最后必须是 END_OF_FILE
    if (tokens_.back().type != TokenType::END_OF_FILE) {
        throw std::runtime_error{"Bad tokens: missing END_OF_FILE token at the end."};
    }

    // 前边不能有 END_OF_FILE
    const auto it{std::find_if(tokens_.begin(), tokens_.end() - 1,
                               [](const Token &t) { return t.type == TokenType::END_OF_FILE; })};
    if (it != tokens_.end() - 1) throw std::runtime_error("Bad tokens: multiple END_OF_FILE.");
}

AstNodeProgramPtr Parser::parse() && {
    const int start_row{peek().row}, start_col{peek().col};

    // 解析一个若干个表达式
    std::vector exprs{parse_exprs()};

    // 必须是解析完了，否则肯定是语法错误
    expect(TokenType::END_OF_FILE);

    return std::make_unique<AstNodeProgram>(start_row, start_col, std::move(exprs));
}
