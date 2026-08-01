#include "Parser.h"

#include "../builtins/exceptions/InternalError.h"
#include "../builtins/exceptions/SyntaxError.h"
#include "../lexer/Lexer.h"
#include "../utils/string_utils.h"

#include <algorithm>
#include <cassert>
#include <format>
#include <memory>
#include <optional>
#include <ranges>

// token 类型转换为一元运算符类型
static std::optional<AstNodeOpUnary::OpType> token_type_to_unary_op_type(const TokenType t) {
    // clang-format off
    switch (t) {
    case TokenType::SIGN_PLUS:     return AstNodeOpUnary::OpType::Pos;
    case TokenType::SIGN_MINUS:    return AstNodeOpUnary::OpType::Neg;
    case TokenType::SIGN_TILDE:    return AstNodeOpUnary::OpType::BitInvert;
    case TokenType::KW_NOT:        return AstNodeOpUnary::OpType::Not;
    case TokenType::SIGN_QUESTION: return AstNodeOpUnary::OpType::Question;
    case TokenType::SIGN_EXCLAIM:  return AstNodeOpUnary::OpType::Exclaim;
    // clang-format on
    default:
        return std::nullopt;
    }
}

// token 类型转换为二元运算符类型（不含比较运算符）
static std::optional<AstNodeOpBinary::OpType> token_type_to_binary_op_type(const TokenType t) {
    // clang-format off
    switch (t) {
    case TokenType::SIGN_PLUS:        return AstNodeOpBinary::OpType::Add;
    case TokenType::SIGN_MINUS:       return AstNodeOpBinary::OpType::Sub;
    case TokenType::SIGN_STAR:        return AstNodeOpBinary::OpType::Mul;
    case TokenType::SIGN_SLASH:       return AstNodeOpBinary::OpType::Div;
    case TokenType::SIGN_DOUBLESLASH: return AstNodeOpBinary::OpType::DivFloor;
    case TokenType::SIGN_PERCENT:     return AstNodeOpBinary::OpType::Mod;
    case TokenType::SIGN_DOUBLESTAR:  return AstNodeOpBinary::OpType::Pow;
    case TokenType::SIGN_AMPERSAND:   return AstNodeOpBinary::OpType::BitAnd;
    case TokenType::SIGN_PIPE:        return AstNodeOpBinary::OpType::BitOr;
    case TokenType::SIGN_CARET:       return AstNodeOpBinary::OpType::BitXor;
    case TokenType::SIGN_LSHIFT:      return AstNodeOpBinary::OpType::LShift;
    case TokenType::SIGN_RSHIFT:      return AstNodeOpBinary::OpType::RShift;
    case TokenType::KW_AND:           return AstNodeOpBinary::OpType::And;
    case TokenType::KW_OR:            return AstNodeOpBinary::OpType::Or;
    case TokenType::SIGN_DOTDOT:      return AstNodeOpBinary::OpType::Range;
    // clang-format on
    default:
        return std::nullopt;
    }
}

// token 类型是否属于比较组（== != < <= > >=），是则转换成对应的 AstNodeCompare::OpType，否则
// nullopt
static std::optional<AstNodeCompare::OpType> token_type_to_compare_op_type(const TokenType t) {
    // clang-format off
    switch (t) {
    case TokenType::SIGN_LT: return AstNodeCompare::OpType::Lt;
    case TokenType::SIGN_LE: return AstNodeCompare::OpType::Le;
    case TokenType::SIGN_GT: return AstNodeCompare::OpType::Gt;
    case TokenType::SIGN_GE: return AstNodeCompare::OpType::Ge;
    case TokenType::SIGN_EQ: return AstNodeCompare::OpType::Eq;
    case TokenType::SIGN_NE: return AstNodeCompare::OpType::Ne;
    // clang-format on
    default:
        return std::nullopt;
    }
}

// 运算符绑定力表
// 对中缀/后缀运算符，返回 {lbp, rbp}
// {-1,-1} 表示不是中缀/后缀运算符
static std::pair<int, int> infix_bp(const TokenType type) {
    switch (type) {
    case TokenType::SIGN_DOT:
        return {170, 170};
    case TokenType::SIGN_LPAREN:
    case TokenType::SIGN_LBRACKET:
        return {170, -1}; // 函数调用、索引
    case TokenType::SIGN_QUESTION:
    case TokenType::SIGN_EXCLAIM:
        return {160, -1}; // ? !
    case TokenType::SIGN_DOUBLESTAR:
        return {150, 149}; // **（右结合）
    case TokenType::SIGN_STAR:
    case TokenType::SIGN_SLASH:
    case TokenType::SIGN_DOUBLESLASH:
    case TokenType::SIGN_PERCENT:
        return {130, 131}; // * / // %
    case TokenType::SIGN_PLUS:
    case TokenType::SIGN_MINUS:
        return {120, 121}; // + -
    case TokenType::SIGN_DOTDOT:
        return {110, 111}; // ..
    case TokenType::SIGN_LSHIFT:
    case TokenType::SIGN_RSHIFT:
        return {100, 101}; // << >>
    case TokenType::SIGN_AMPERSAND:
        return {90, 91}; // &
    case TokenType::SIGN_CARET:
        return {80, 81}; // ^
    case TokenType::SIGN_PIPE:
        return {70, 71}; // |
    // < <= > >= == !=（链式比较；rbp 未被使用，链内自行控制操作数的 min_bp）
    case TokenType::SIGN_LT:
    case TokenType::SIGN_LE:
    case TokenType::SIGN_GT:
    case TokenType::SIGN_GE:
    case TokenType::SIGN_EQ:
    case TokenType::SIGN_NE:
        return {60, 61};
    case TokenType::KW_IS:
        return {50, 51}; // is（自成一组的链式比较，不与上面 6 者混链）
    case TokenType::KW_AND:
        return {30, 31}; // and
    case TokenType::KW_OR:
        return {20, 21}; // or
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
    case TokenType::SIGN_RSHIFT_ASSIGN:
        return {10, 9};
    default:
        return {-1, -1};
    }
}

// token 类型是否是复合赋值 op=，是则转换成对应的二元运算符，否则 nullopt
static std::optional<AstNodeOpBinary::OpType> assign_compound_to_binary(const TokenType op) {
    // clang-format off
    switch (op) {
    case TokenType::SIGN_PLUS_ASSIGN:        return AstNodeOpBinary::OpType::Add;
    case TokenType::SIGN_MINUS_ASSIGN:       return AstNodeOpBinary::OpType::Sub;
    case TokenType::SIGN_STAR_ASSIGN:        return AstNodeOpBinary::OpType::Mul;
    case TokenType::SIGN_DOUBLESTAR_ASSIGN:  return AstNodeOpBinary::OpType::Pow;
    case TokenType::SIGN_SLASH_ASSIGN:       return AstNodeOpBinary::OpType::Div;
    case TokenType::SIGN_DOUBLESLASH_ASSIGN: return AstNodeOpBinary::OpType::DivFloor;
    case TokenType::SIGN_PERCENT_ASSIGN:     return AstNodeOpBinary::OpType::Mod;
    case TokenType::SIGN_AMPERSAND_ASSIGN:   return AstNodeOpBinary::OpType::BitAnd;
    case TokenType::SIGN_PIPE_ASSIGN:        return AstNodeOpBinary::OpType::BitOr;
    case TokenType::SIGN_CARET_ASSIGN:       return AstNodeOpBinary::OpType::BitXor;
    case TokenType::SIGN_LSHIFT_ASSIGN:      return AstNodeOpBinary::OpType::LShift;
    case TokenType::SIGN_RSHIFT_ASSIGN:      return AstNodeOpBinary::OpType::RShift;
    // clang-format on
    default:
        return std::nullopt;
    }
}

const Token &Parser::peek() const { return pos_ < tokens_.size() ? tokens_[pos_] : tokens_.back(); }

const Token &Parser::advance() {
    assert(pos_ < tokens_.size());

    return tokens_[pos_++];
}

bool Parser::check(const TokenType type) const {
    return pos_ < tokens_.size() && tokens_[pos_].type == type;
}

bool Parser::check_over_newline(const TokenType type, const std::optional<size_t> start) const {
    const size_t len{tokens_.size()};
    for (size_t i{start.value_or(pos_)}; i < len; i++) {
        if (tokens_[i].type != TokenType::NEWLINE) {
            return tokens_[i].type == type;
        }
    }
    return false;
}

void Parser::check_terminator() const {
    switch (peek().type) {
    case TokenType::NEWLINE:
    case TokenType::SIGN_SEMICOLON:
    case TokenType::END_OF_FILE:
    case TokenType::SIGN_RBRACE:
        break;
    default:
        error("expected newline or ';' after expression (newline recommended)");
    }
}

const Token &Parser::expect(const TokenType expected_type) {
    if (const Token &token{peek()}; token.type != expected_type) {
        error(
            std::format(
                "expected {} but got {}",
                Lexer::get_displayname_by_tokentype(expected_type),
                Lexer::get_displayname_by_tokentype(token.type)
            )
        );
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

void Parser::error_internal(const std::string &msg, const Position pos) const {
    throw InternalError{file_path_, pos.row, pos.col, msg};
}

std::vector<AstNodePtr> Parser::parse_exprs() {
    std::vector<AstNodePtr> exprs;

    // 跳过前导终止符
    skip_terminator();
    // 不是 EOF 也不是 }
    while (!check(TokenType::END_OF_FILE) && !check(TokenType::SIGN_RBRACE)) {
        exprs.push_back(parse_expr());
        check_terminator();
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

    const Position start_pos{left->pos_};

    while (true) {
        // 括号内允许运算符前换行（如多行链式调用）
        skip_paren_newline();
        const TokenType op{peek().type};
        const auto [lbp, rbp]{infix_bp(op)};

        // lbp == -1 表示非中缀/后缀运算符；lbp < min_bp 表示绑定力不足，让上层处理
        if (lbp < min_bp) break;

        // 全部交给 finish_call / finish_index 处理
        if (op == TokenType::SIGN_LPAREN) {
            left = finish_call(std::move(left), start_pos);
            continue;
        }
        if (op == TokenType::SIGN_LBRACKET) {
            left = finish_index(std::move(left), start_pos);
            continue;
        }

        const Token &op_token{advance()}; // 消耗运算符
        const Position op_pos{op_token.row, op_token.col};

        // 赋值（右结合，rbp = lbp - 1 = 9）
        if (op == TokenType::SIGN_ASSIGN) {
            skip_newline();
            return std::make_unique<AstNodeAssign>(
                start_pos, std::move(left), parse_expr_pratt(rbp)
            );
        }

        // 复合赋值 x op= y
        if (const auto compound_op{assign_compound_to_binary(op)}) {
            skip_newline();
            return std::make_unique<AstNodeCompoundAssign>(
                start_pos, std::move(left), *compound_op, parse_expr_pratt(rbp), op_pos
            );
        }

        // 后缀 x?  x!
        if (op == TokenType::SIGN_QUESTION || op == TokenType::SIGN_EXCLAIM) {
            left = std::make_unique<AstNodeOpUnary>(
                start_pos, *token_type_to_unary_op_type(op), std::move(left), op_pos
            );
            continue;
        }

        // 属性访问 x.attr
        if (op == TokenType::SIGN_DOT) {
            skip_newline();
            left = std::make_unique<AstNodeAttr>(
                start_pos,
                std::move(left),
                expect(TokenType::IDENTIFIER).lexeme,
                op_pos // 消耗标识符
            );
            continue;
        }

        // 比较运算（链式，== != < <= > >= 一组）
        if (const auto compare_op{token_type_to_compare_op_type(op)}) {
            left = parse_chain_compare(std::move(left), start_pos, *compare_op, op_pos);
            continue;
        }

        // is（链式，自成一组）
        if (op == TokenType::KW_IS) {
            left = parse_chain_is(std::move(left), start_pos, op_pos);
            continue;
        }

        // 普通二元运算符
        skip_newline();
        left = std::make_unique<AstNodeOpBinary>(
            start_pos,
            *token_type_to_binary_op_type(op),
            std::move(left),
            parse_expr_pratt(rbp),
            op_pos
        );
    }

    return left;
}

AstNodePtr Parser::parse_chain_compare(
    AstNodePtr left, const Position start_pos, const AstNodeCompare::OpType first_op,
    const Position first_op_pos
) {
    constexpr int operand_min_bp{61}; // (比较组优先级 60) + 1，防止同组递归吞并

    std::vector<AstNodePtr> operands;
    std::vector<AstNodeCompare::OpType> ops;
    std::vector<Position> op_positions;

    operands.push_back(std::move(left));
    ops.push_back(first_op);
    op_positions.push_back(first_op_pos);

    skip_newline();
    operands.push_back(parse_expr_pratt(operand_min_bp));

    while (true) {
        skip_paren_newline();
        const auto next_op{token_type_to_compare_op_type(peek().type)};
        if (!next_op) break;

        const Position next_op_pos{peek().row, peek().col};
        advance(); // 消耗 '<' 或 '<=' 或 '>' 或 '>=' 或 '==' 或 '!='
        ops.push_back(*next_op);
        op_positions.push_back(next_op_pos);
        skip_newline();
        operands.push_back(parse_expr_pratt(operand_min_bp));
    }

    return std::make_unique<AstNodeCompare>(
        start_pos, std::move(ops), std::move(operands), std::move(op_positions)
    );
}

AstNodePtr
Parser::parse_chain_is(AstNodePtr left, const Position start_pos, const Position first_is_pos) {
    constexpr int operand_min_bp{51}; // (is 优先级 50) + 1，防止同组递归吞并

    std::vector<AstNodePtr> operands;
    std::vector<Position> op_positions;
    operands.push_back(std::move(left));
    op_positions.push_back(first_is_pos);

    skip_newline();
    operands.push_back(parse_expr_pratt(operand_min_bp));

    while (true) {
        skip_paren_newline();
        if (!check(TokenType::KW_IS)) break;

        op_positions.emplace_back(peek().row, peek().col);
        expect(TokenType::KW_IS); // 消耗 'is'
        skip_newline();
        operands.push_back(parse_expr_pratt(operand_min_bp));
    }

    return std::make_unique<AstNodeIs>(start_pos, std::move(operands), std::move(op_positions));
}

AstNodePtr Parser::parse_non_op() {
    // 跳过前导换行
    skip_newline();
    const auto &[type, row, col, lexeme]{peek()};
    const Position pos{row, col};

    switch (type) {
    // 字面量
    case TokenType::LITERAL_NONE:
        // None
        expect(TokenType::LITERAL_NONE); // 消耗 'None'
        return std::make_unique<AstNodeLiteralNone>(pos);
    case TokenType::LITERAL_TRUE:
        // True
        expect(TokenType::LITERAL_TRUE); // 消耗 'True'
        return std::make_unique<AstNodeLiteralBool>(pos, true);
    case TokenType::LITERAL_FALSE:
        // False
        expect(TokenType::LITERAL_FALSE); // 消耗 'False'
        return std::make_unique<AstNodeLiteralBool>(pos, false);
    case TokenType::LITERAL_G:
        // _G
        expect(TokenType::LITERAL_G); // 消耗 '_G'
        return std::make_unique<AstNodeLiteralGL>(pos, AstNodeLiteralGL::GLType::G);
    case TokenType::LITERAL_L:
        // _L
        expect(TokenType::LITERAL_L); // 消耗 '_L'
        return std::make_unique<AstNodeLiteralGL>(pos, AstNodeLiteralGL::GLType::L);
    case TokenType::LITERAL_ELLIPSIS:
        // ...
        expect(TokenType::LITERAL_ELLIPSIS); // 消耗 '...'
        return std::make_unique<AstNodeLiteralEllipsis>(pos);
    case TokenType::LITERAL_INT:
        // int
        expect(TokenType::LITERAL_INT); // 消耗整数字面量
        return std::make_unique<AstNodeLiteralInt>(pos, lexeme);
    case TokenType::LITERAL_FLOAT:
        // float
        expect(TokenType::LITERAL_FLOAT); // 消耗浮点数字面量
        return std::make_unique<AstNodeLiteralFloat>(pos, lexeme);
    case TokenType::LITERAL_STR:
        // str
        expect(TokenType::LITERAL_STR); // 消耗字符串字面量
        return std::make_unique<AstNodeLiteralStr>(pos, lexeme);

    // 分组、元组
    case TokenType::SIGN_LPAREN:
        return parse_paren_or_tuple();
    // 列表
    case TokenType::SIGN_LBRACKET:
        return parse_list();
    // 字典、复合表达式
    case TokenType::SIGN_LBRACE:
        return parse_brace();

    // 标识符
    case TokenType::IDENTIFIER:
        return std::make_unique<AstNodeIdentifier>(pos, expect(TokenType::IDENTIFIER).lexeme);

    // 解包 / 展开（操作数按“单目运算符”那一档的优先级 140 解析，与 +x/-x/~x 一致）
    case TokenType::SIGN_STAR:
        // *obj
        expect(TokenType::SIGN_STAR); // 消耗 '*'
        return std::make_unique<AstNodeStar>(pos, parse_expr_pratt(140));
    case TokenType::SIGN_DOUBLESTAR:
        // **obj
        expect(TokenType::SIGN_DOUBLESTAR); // 消耗 '**'
        return std::make_unique<AstNodeDoubleStar>(pos, parse_expr_pratt(140));

    // 前缀运算符
    case TokenType::SIGN_PLUS:
    case TokenType::SIGN_MINUS:
    case TokenType::SIGN_TILDE:
        // 单目优先级 140
        advance(); // 消耗 '+' 或 '-' 或 '~'
        return std::make_unique<AstNodeOpUnary>(
            pos, *token_type_to_unary_op_type(type), parse_expr_pratt(140), pos
        );
    case TokenType::KW_NOT:
        // not 优先级 40
        expect(TokenType::KW_NOT); // 消耗 'not'
        return std::make_unique<AstNodeOpUnary>(
            pos, AstNodeOpUnary::OpType::Not, parse_expr_pratt(40), pos
        );

    // del / global / import
    case TokenType::KW_DEL:
        return parse_del();
    case TokenType::KW_GLOBAL:
        return parse_global();
    case TokenType::KW_IMPORT:
        return parse_import();

    // 控制流
    case TokenType::KW_IF:
        return parse_if();
    case TokenType::KW_FOR:
        return parse_for();
    case TokenType::KW_WHILE:
        return parse_while();
    case TokenType::KW_BREAK:
        // break
        expect(TokenType::KW_BREAK); // 消耗 'break'
        return std::make_unique<AstNodeBreak>(pos);
    case TokenType::KW_CONTINUE:
        // continue
        expect(TokenType::KW_CONTINUE); // 消耗 'continue'
        return std::make_unique<AstNodeContinue>(pos);
    case TokenType::KW_RETURN:
        return parse_return();
    case TokenType::KW_TRY:
        return parse_try();
    case TokenType::KW_RAISE:
        return parse_raise();

    // 函数
    case TokenType::KW_FUNC:
        return parse_func();
    // 类
    case TokenType::KW_CLASS:
        return parse_class();
    // 装饰器
    case TokenType::SIGN_AT:
        return parse_decorator();

    case TokenType::END_OF_FILE:
        // 括号内多半是没闭合
        if (paren_depth_ > 0) error("unexpected end of file (unclosed bracket)");
        // 否则是缺了表达式
        error("unexpected end of file (expected an expression)");

    // 错误
    default:
        error(std::format("unexpected token '{}'", u32_to_utf8(lexeme)));
    }
}

AstNodePtr Parser::parse_expr_as_cond() {
    // min_bp=11 卡住裸的赋值类运算符
    AstNodePtr left{parse_expr_pratt(11)};
    const Position start_pos{left->pos_};

    skip_paren_newline();
    if (peek().type == TokenType::SIGN_ASSIGN) {
        error(
            "bare assignment '=' is not allowed directly here "
            "(did you mean '=='? or wrap it in an extra pair of parentheses if intentional)"
        );
    }

    // 复合赋值 x op= y 允许裸写
    if (const auto compound_op{assign_compound_to_binary(peek().type)}) {
        const auto &[op, op_row, op_col, lexeme]{advance()};
        skip_newline();
        left = std::make_unique<AstNodeCompoundAssign>(
            start_pos,
            std::move(left),
            *compound_op,
            parse_expr_pratt(infix_bp(op).second),
            Position{op_row, op_col}
        );
    }

    return left;
}

AstNodePtr Parser::parse_class(
    std::vector<AstNodePtr> decorators, std::vector<Position> decorator_positions,
    const Position deco_pos
) {
    const Position start_pos{decorators.empty() ? Position{peek().row, peek().col} : deco_pos};
    expect(TokenType::KW_CLASS); // 消耗 'class'
    skip_newline();

    // 可选类名（无名即匿名类）
    std::optional<std::u32string> name;
    if (check(TokenType::IDENTIFIER)) {
        name = expect(TokenType::IDENTIFIER).lexeme; // 消耗标识符
        skip_newline();
    }

    // 可选基类列表 (BaseClass1, ...)
    std::vector<AstNodePtr> bases;
    if (check(TokenType::SIGN_LPAREN)) {
        expect(TokenType::SIGN_LPAREN), paren_depth_++; // 消耗 '('

        finish_comma_batch(TokenType::SIGN_RPAREN, [&] { bases.push_back(parse_expr()); });

        if (!check(TokenType::SIGN_RPAREN)) error("expected ')' to close base class list");
        expect(TokenType::SIGN_RPAREN), paren_depth_--; // 消耗 ')'

        skip_newline();
    }

    // 可选捕获列表 [captures]
    std::vector<OneCapture> captures;
    if (check(TokenType::SIGN_LBRACKET)) {
        captures = finish_captures();
        skip_newline();
    }

    // 可选文档字符串（下一个 token 不是 '{' 则视为 doc）
    AstNodePtr doc;
    if (!check(TokenType::SIGN_LBRACE)) {
        doc = parse_expr();
        skip_newline();
    }

    // 类体：{ ... }（同函数体：暂存并清零 paren_depth_，块内换行重新充当语句分隔符）
    const Position body_pos{peek().row, peek().col};
    const int outer_paren_depth{paren_depth_};
    expect(TokenType::SIGN_LBRACE), paren_depth_ = 0; // 消耗 '{'
    std::vector body{parse_exprs()};
    expect(TokenType::SIGN_RBRACE), paren_depth_ = outer_paren_depth; // 消耗 '}'

    return std::make_unique<AstNodeClass>(
        start_pos,
        std::move(decorators),
        std::move(decorator_positions),
        std::move(name),
        std::move(bases),
        std::move(captures),
        std::move(doc),
        std::make_unique<AstNodeProgram>(body_pos, std::move(body))
    );
}

AstNodePtr Parser::parse_if() {
    const Position start_pos{peek().row, peek().col};
    expect(TokenType::KW_IF); // 消耗 'if'

    std::vector<AstNodeIf::AstNodeCondAndExpr> clauses;

    // 解析一个 if/elif 子句的条件和主体：if/elif (cond) body
    auto parse_cond_and_body{[&]() -> AstNodeIf::AstNodeCondAndExpr {
        skip_newline();
        expect(TokenType::SIGN_LPAREN), paren_depth_++; // 消耗 '('
        AstNodePtr cond{parse_expr_as_cond()};
        expect(TokenType::SIGN_RPAREN), paren_depth_--; // 消耗 ')'
        skip_newline();
        AstNodePtr body{parse_expr()};
        return {std::move(cond), std::move(body)};
    }};

    clauses.push_back(parse_cond_and_body());

    // 解析 elif 子句（可在下一行）
    while (check_over_newline(TokenType::KW_ELIF)) {
        skip_newline();
        expect(TokenType::KW_ELIF); // 消耗 'elif'
        clauses.push_back(parse_cond_and_body());
    }

    // 解析可选的 else（可在下一行），无 else 则保持 nullptr
    AstNodePtr else_expr;
    if (check_over_newline(TokenType::KW_ELSE)) {
        skip_newline();
        expect(TokenType::KW_ELSE); // 消耗 'else'
        skip_newline();
        else_expr = parse_expr();
    }

    return std::make_unique<AstNodeIf>(start_pos, std::move(clauses), std::move(else_expr));
}

AstNodePtr Parser::parse_for() {
    const Position start_pos{peek().row, peek().col};
    expect(TokenType::KW_FOR); // 消耗 'for'
    skip_newline();
    const bool collect{check(TokenType::SIGN_DOLLAR)};
    if (collect) expect(TokenType::SIGN_DOLLAR); // 消耗 '$'
    skip_newline();
    expect(TokenType::SIGN_LPAREN), paren_depth_++; // 消耗 '('
    skip_newline();

    // 解析 for 头部的一个槽
    auto parse_slot{[&](const bool as_cond) -> AstNodePtr {
        if (check(TokenType::SIGN_SEMICOLON) || check(TokenType::NEWLINE) ||
            check(TokenType::SIGN_RPAREN))
            // 遇到 ';', NEWLINE, ')' 则槽为空，返回 nullptr
            return nullptr;
        return as_cond ? parse_expr_as_cond() : parse_expr();
    }};

    // 先读第一个槽（可能为空）。它要么是迭代目标（后跟 ':'），要么是步进模式的 init
    AstNodePtr first{parse_slot(false)};

    // 第一个槽后紧跟 ':' → 迭代模式：for [$] (target : iterable) body
    if (check_over_newline(TokenType::SIGN_COLON)) {
        skip_newline();
        expect(TokenType::SIGN_COLON); // 消耗 ':'
        skip_newline();
        AstNodePtr iterable{parse_expr()};
        skip_newline();
        expect(TokenType::SIGN_RPAREN), paren_depth_--; // 消耗 ')'
        skip_newline();
        AstNodePtr body{parse_expr()};
        return std::make_unique<AstNodeForIter>(
            start_pos, collect, std::move(first), std::move(iterable), std::move(body)
        );
    }

    // 第一个槽为空、且直接紧跟 ')'，即整个头部彻底为空：for ()
    if (!first && check_over_newline(TokenType::SIGN_RPAREN)) {
        error(
            "empty for header (for an infinite loop use `for (;;)`; for a plain condition use "
            "`while (cond)`)"
        );
    }

    // 否则为步进模式：for [$] (init SEP cond SEP inc) body，first 即 init
    auto consume_sep{[&] {
        if (check(TokenType::SIGN_SEMICOLON)) {
            expect(TokenType::SIGN_SEMICOLON); // 消耗 ';'
            skip_newline();
            return;
        }
        // 上一个已消耗的 token 和当前 token 之间是不是隔着至少一次真实换行（比较两者的行号）
        if (tokens_[pos_ - 1].row == peek().row) {
            error("expected ';' or newline to separate the expressions in a for header");
        }
        skip_newline();
        // 换行分隔（不是显式 ';'）之后如果直接是 ')'，说明后面这一槽整个是空的
        if (check(TokenType::SIGN_RPAREN)) {
            error(
                "an empty slot in a for header must be marked with ';', a newline alone is not "
                "enough"
            );
        }
    }};

    consume_sep();
    AstNodePtr cond{parse_slot(true)};
    consume_sep();
    AstNodePtr inc{parse_slot(false)};
    skip_newline();
    expect(TokenType::SIGN_RPAREN), paren_depth_--; // 消耗 ')'
    skip_newline();
    AstNodePtr body{parse_expr()};

    return std::make_unique<AstNodeForCond>(
        start_pos, collect, std::move(first), std::move(cond), std::move(inc), std::move(body)
    );
}

AstNodePtr Parser::parse_while() {
    const Position start_pos{peek().row, peek().col};
    expect(TokenType::KW_WHILE); // 消耗 'while'
    skip_newline();
    const bool collect{check(TokenType::SIGN_DOLLAR)};
    if (collect) expect(TokenType::SIGN_DOLLAR); // 消耗 '$'
    skip_newline();
    expect(TokenType::SIGN_LPAREN), paren_depth_++; // 消耗 '('
    AstNodePtr cond{parse_expr_as_cond()};
    expect(TokenType::SIGN_RPAREN), paren_depth_--; // 消耗 ')'
    skip_newline();
    AstNodePtr body{parse_expr()};

    return std::make_unique<AstNodeForCond>(
        start_pos, collect, nullptr, std::move(cond), nullptr, std::move(body)
    );
}

AstNodePtr Parser::parse_return() {
    const Position start_pos{peek().row, peek().col};
    expect(TokenType::KW_RETURN); // 消耗 'return'
    // 若紧跟终止符则为裸 return（值为 None）
    // 语句终止符 NEWLINE, ';', EOF, '}' + 括号语境的闭合 / 分隔符 ')', ']', ','
    const bool bare{
        check(TokenType::NEWLINE) || check(TokenType::SIGN_SEMICOLON) ||
        check(TokenType::END_OF_FILE) || check(TokenType::SIGN_RBRACE) ||
        check(TokenType::SIGN_RPAREN) || check(TokenType::SIGN_RBRACKET) ||
        check(TokenType::SIGN_COMMA)
    };
    return std::make_unique<AstNodeReturn>(start_pos, bare ? nullptr : parse_expr());
}

AstNodePtr Parser::parse_try() {
    const Position start_pos{peek().row, peek().col};
    expect(TokenType::KW_TRY); // 消耗 'try'

    // try 后没有 '()'，直接跟主体
    skip_newline();
    AstNodePtr try_expr{parse_expr()};

    // 解析一个 except 子句的异常列表和主体：except (Exc1, Exc2, ...) body
    auto parse_excs_and_body{[&]() -> AstNodeTry::AstNodeExceptAndExpr {
        skip_newline();
        expect(TokenType::SIGN_LPAREN), paren_depth_++; // 消耗 '('

        // 解析 Exception1, ...
        std::vector<AstNodePtr> excs;
        finish_comma_batch(TokenType::SIGN_RPAREN, [&] { excs.push_back(parse_expr()); });
        if (excs.empty()) error("except requires at least one exception type");

        expect(TokenType::SIGN_RPAREN), paren_depth_--; // 消耗 ')'
        skip_newline();
        AstNodePtr body{parse_expr()};
        return {std::move(excs), std::move(body)};
    }};

    std::vector<AstNodeTry::AstNodeExceptAndExpr> except_clauses;

    // 解析 except 子句（可在下一行）
    while (check_over_newline(TokenType::KW_EXCEPT)) {
        skip_newline();
        expect(TokenType::KW_EXCEPT); // 消耗 'except'
        except_clauses.push_back(parse_excs_and_body());
    }

    // 解析可选的 finally（可在下一行），无 finally 则保持 nullptr
    AstNodePtr finally_expr;
    if (check_over_newline(TokenType::KW_FINALLY)) {
        skip_newline();
        expect(TokenType::KW_FINALLY); // 消耗 'finally'
        skip_newline();
        finally_expr = parse_expr();
    }

    // 注：except / finally 至少有一个，这一约束交由语义层检查
    return std::make_unique<AstNodeTry>(
        start_pos, std::move(try_expr), std::move(except_clauses), std::move(finally_expr)
    );
}

AstNodePtr Parser::parse_raise() {
    const Position start_pos{peek().row, peek().col};
    expect(TokenType::KW_RAISE); // 消耗 'raise'
    skip_newline();
    return std::make_unique<AstNodeRaise>(start_pos, parse_expr());
}

AstNodePtr Parser::parse_decorator() {
    // 先把连续的前缀 @decorator 全部收集起来（不预先假设后面接的是 func/class 还是任意表达式），
    std::vector<AstNodePtr> decorators;
    std::vector<Position> decorator_positions;

    while (check(TokenType::SIGN_AT)) {
        const Position deco_pos{peek().row, peek().col};
        expect(TokenType::SIGN_AT); // 消耗 '@'
        skip_newline();
        // 解析装饰器表达式（Pratt 在遇到 func / class / @ 前会自然停止，因为它们都不是中缀运算符）
        AstNodePtr decorator{parse_expr()};
        skip_newline();
        decorators.push_back(std::move(decorator));
        decorator_positions.push_back(deco_pos);
    }

    // 紧邻 func/class
    if (check(TokenType::KW_FUNC) || check(TokenType::KW_CLASS)) {
        const Position deco_pos{decorator_positions.front()};
        return check(TokenType::KW_FUNC)
                   ? parse_func(std::move(decorators), std::move(decorator_positions), deco_pos)
                   : parse_class(std::move(decorators), std::move(decorator_positions), deco_pos);
    }

    // 通用形式：@d1 @d2 ... expr ≡ d1(d2(...(expr)))
    AstNodePtr target{parse_expr()};
    for (auto &&[expr, pos] :
         std::views::zip(decorators, decorator_positions) | std::views::reverse) {
        target = std::make_unique<AstNodeDecorator>(pos, std::move(expr), std::move(target));
    }
    return target;
}

AstNodePtr Parser::parse_func(
    std::vector<AstNodePtr> decorators, std::vector<Position> decorator_positions,
    const Position deco_pos
) {
    const Position start_pos{decorators.empty() ? Position{peek().row, peek().col} : deco_pos};
    expect(TokenType::KW_FUNC); // 消耗 'func'
    skip_newline();

    // 可选函数名（无名即匿名函数）
    std::optional<std::u32string> name;
    if (check(TokenType::IDENTIFIER)) {
        name = expect(TokenType::IDENTIFIER).lexeme; // 消耗标识符
        skip_newline();
    }

    // 可选捕获列表 [captures]
    std::vector<OneCapture> captures;
    if (check(TokenType::SIGN_LBRACKET)) {
        captures = finish_captures();
        skip_newline();
    }

    // 形参列表
    AstNodeFunc::AllParams params{finish_func_params()};
    skip_newline();

    // 可选返回类型 : type
    AstNodePtr return_type;
    if (check(TokenType::SIGN_COLON)) {
        expect(TokenType::SIGN_COLON); // 消耗 ':'
        skip_newline();
        return_type = parse_expr();
        skip_newline();
    }

    // 可选文档字符串（下一个 token 不是 '{' 则视为 doc；是否为合法的常量折叠字符串由语义层校验）
    AstNodePtr doc;
    if (!check(TokenType::SIGN_LBRACE)) {
        doc = parse_expr();
        skip_newline();
    }

    // 函数体：{ ... }（块内换行重新充当语句分隔符，暂存并清零 paren_depth_）
    const Position body_pos{peek().row, peek().col};
    const int outer_paren_depth{paren_depth_};
    expect(TokenType::SIGN_LBRACE), paren_depth_ = 0; // 消耗 '{'
    std::vector body{parse_exprs()};
    expect(TokenType::SIGN_RBRACE), paren_depth_ = outer_paren_depth; // 消耗 '}'

    return std::make_unique<AstNodeFunc>(
        start_pos,
        std::move(decorators),
        std::move(decorator_positions),
        std::move(name),
        std::move(captures),
        std::move(params),
        std::move(return_type),
        std::move(doc),
        std::make_unique<AstNodeProgram>(body_pos, std::move(body))
    );
}

AstNodePtr Parser::parse_import() {
    const Position start_pos{peek().row, peek().col};
    expect(TokenType::KW_IMPORT); // 消耗 'import'

    skip_newline();

    // 调用形态 import(...)
    if (check(TokenType::SIGN_LPAREN)) {
        const std::unique_ptr call{finish_call(nullptr, start_pos)}; // 消耗 '(' ... ')'
        return std::make_unique<AstNodeImportCall>(
            start_pos,
            std::move(call->positional_args_),
            std::move(call->keyword_args_),
            call->paren_pos_
        );
    }

    // 关键字形态 import a.b.c
    // 点号在这里一路吃干净，换行规则跟属性访问 x.y 完全一致
    std::vector segments{expect(TokenType::IDENTIFIER).lexeme}; // 消耗标识符
    skip_paren_newline();
    while (check(TokenType::SIGN_DOT)) {
        expect(TokenType::SIGN_DOT); // 消耗 '.'
        skip_newline();
        segments.push_back(expect(TokenType::IDENTIFIER).lexeme); // 消耗标识符
        skip_paren_newline();
    }

    return std::make_unique<AstNodeImportKw>(start_pos, std::move(segments));
}

AstNodePtr Parser::parse_paren_or_tuple() {
    const Position start_pos{peek().row, peek().col};

    expect(TokenType::SIGN_LPAREN), paren_depth_++; // 消耗 '('

    std::vector<AstNodePtr> items;
    const bool has_seen_comma{finish_comma_batch(TokenType::SIGN_RPAREN, [&] {
        items.push_back(parse_expr());
    })};

    if (!check(TokenType::SIGN_RPAREN)) {
        error(
            has_seen_comma ? "expected ')' to close tuple" : "expected ')' to close the parentheses"
        );
    }
    expect(TokenType::SIGN_RPAREN), paren_depth_--; // 消耗 ')'

    // 恰好一项且没见过 ','：(expr) 是分组，不是元组，直接返回内部表达式本身
    if (items.size() == 1 && !has_seen_comma) return std::move(items[0]);

    return std::make_unique<AstNodeLiteralTuple>(start_pos, std::move(items));
}

AstNodePtr Parser::parse_list() {
    const Position start_pos{peek().row, peek().col};

    expect(TokenType::SIGN_LBRACKET), paren_depth_++; // 消耗 '['

    std::vector<AstNodePtr> items;
    finish_comma_batch(TokenType::SIGN_RBRACKET, [&] { items.push_back(parse_expr()); });

    if (!check(TokenType::SIGN_RBRACKET)) error("expected ']' to close list literal");
    expect(TokenType::SIGN_RBRACKET), paren_depth_--; // 消耗 ']'

    return std::make_unique<AstNodeLiteralList>(start_pos, std::move(items));
}

AstNodePtr Parser::parse_brace() {
    const Position start_pos{peek().row, peek().col};

    expect(TokenType::SIGN_LBRACE); // 消耗 '{'

    const int outer_paren_depth{paren_depth_};
    paren_depth_ = 0;

    skip_newline();
    // 一见到 '}'（空块）或 ';' 就已经确定是复合表达式
    if (check(TokenType::SIGN_RBRACE) || check(TokenType::SIGN_SEMICOLON)) {
        std::vector exprs{parse_exprs()};
        expect(TokenType::SIGN_RBRACE); // 消耗 '}'

        paren_depth_ = outer_paren_depth;
        return std::make_unique<AstNodeCompound>(start_pos, std::move(exprs));
    }

    // 先解析第一个表达式再判别是字典还是复合表达式
    AstNodePtr first{parse_expr()};

    // 是 ** 或者后边有冒号 -> 一定是字典
    if (dynamic_cast<AstNodeDoubleStar *>(first.get()) ||
        check_over_newline(TokenType::SIGN_COLON)) {
        AstNodePtr dict{finish_dict(start_pos, std::move(first))};
        expect(TokenType::SIGN_RBRACE), paren_depth_ = outer_paren_depth; // 消耗 '}'
        return dict;
    }

    // 否则一定是复合表达式
    // 它没走 parse_exprs 的循环体，这里手动过一遍同一道终止符检查，防止 first
    // 和后续表达式之间没有分隔符
    check_terminator();
    std::vector<AstNodePtr> exprs;
    exprs.push_back(std::move(first));

    for (AstNodePtr &expr : parse_exprs()) exprs.push_back(std::move(expr));
    expect(TokenType::SIGN_RBRACE), paren_depth_ = outer_paren_depth; // 消耗 '}'
    return std::make_unique<AstNodeCompound>(start_pos, std::move(exprs));
}

AstNodePtr Parser::parse_del() {
    const Position start_pos{peek().row, peek().col};
    expect(TokenType::KW_DEL); // 消耗 'del'
    skip_newline();
    return std::make_unique<AstNodeDel>(start_pos, parse_expr());
}

AstNodePtr Parser::parse_global() {
    const Position start_pos{peek().row, peek().col};
    expect(TokenType::KW_GLOBAL); // 消耗 'global'
    skip_newline();
    return std::make_unique<AstNodeGlobal>(
        start_pos, expect(TokenType::IDENTIFIER).lexeme
    ); // 消耗标识符
}

bool Parser::finish_comma_batch(const TokenType close, const std::function<void()> &parse_item) {
    skip_newline();
    bool has_seen_comma{false};
    if (!check(close)) {
        parse_item(); // 委托给回调函数
        skip_newline();
        while (check(TokenType::SIGN_COMMA)) {
            has_seen_comma = true;
            expect(TokenType::SIGN_COMMA); // 消耗 ','
            skip_newline();
            if (check(close)) break; // 尾逗号
            parse_item();
            skip_newline();
        }
    }
    return has_seen_comma;
}

std::vector<OneCapture> Parser::finish_captures() {
    // 解析一个捕获
    auto parse_one_capture{[&]() -> OneCapture {
        OneCapture c{};

        // &identifier（引用捕获）
        if (check(TokenType::SIGN_AMPERSAND)) {
            expect(TokenType::SIGN_AMPERSAND); // 消耗 '&'
            skip_newline();
            c.capture_type_ = OneCapture::CaptureType::Reference;
            c.identifier_ = expect(TokenType::IDENTIFIER).lexeme; // 消耗标识符
            return c;
        }

        // identifier ⟦= expr⟧（值捕获）
        c.capture_type_ = OneCapture::CaptureType::Value;
        c.identifier_ = expect(TokenType::IDENTIFIER).lexeme; // 消耗标识符
        skip_newline();
        if (check(TokenType::SIGN_ASSIGN)) {
            expect(TokenType::SIGN_ASSIGN); // 消耗 '='
            skip_newline();
            c.value_expr_ = parse_expr();
        }

        return c;
    }};

    expect(TokenType::SIGN_LBRACKET), paren_depth_++; // 消耗 '['

    std::vector<OneCapture> captures;
    finish_comma_batch(TokenType::SIGN_RBRACKET, [&] { captures.push_back(parse_one_capture()); });

    if (!check(TokenType::SIGN_RBRACKET)) error("expected ']' to close capture list");
    expect(TokenType::SIGN_RBRACKET), paren_depth_--; // 消耗 ']'

    // 留给语义层检查的：捕获列表的标识符查重
    return captures;
}

AstNodeFunc::AllParams Parser::finish_func_params() {
    // 解析一个普通形参：identifier ⟦: type⟧ ⟦= expr⟧（*args/**kwargs 各自只是裸标识符，不走这里）
    auto parse_one_normal_param{[&]() -> AstNodeFunc::OneParam {
        AstNodeFunc::OneParam p{};
        p.identifier_ = expect(TokenType::IDENTIFIER).lexeme; // 消耗标识符
        skip_newline();

        // 类型注解
        if (check(TokenType::SIGN_COLON)) {
            expect(TokenType::SIGN_COLON); // 消耗 ':'
            skip_newline();
            p.type_annotation_ = parse_expr_pratt(11); // 停在 '=' 之前（lbp=10 < 11）
            skip_newline();
        }

        // 默认值
        if (check(TokenType::SIGN_ASSIGN)) {
            expect(TokenType::SIGN_ASSIGN); // 消耗 '='
            skip_newline();
            p.default_value_ = parse_expr();
            skip_newline();
        }

        return p;
    }};

    expect(TokenType::SIGN_LPAREN), paren_depth_++; // 消耗 '('

    // 这里只保证至多一个 *args、至多一个 **kwargs 且必须是最后一项
    // 留给语义层检查的：
    // 1. 捕获列表/形参列表内部及两者之间的标识符查重（含 var_args_name_/var_kwargs_name_ 自己）；
    // 2. positional_ 段内"无默认值形参必须排在有默认值形参之前"（kw_only_ 段不受此约束）；
    AstNodeFunc::AllParams result;
    finish_comma_batch(TokenType::SIGN_RPAREN, [&] {
        // 当前是 **kwargs -> 要求是前边不能有 **kwargs
        if (check(TokenType::SIGN_DOUBLESTAR)) {
            if (result.var_kwargs_name_) error("at most one **kwargs parameter is allowed");
            expect(TokenType::SIGN_DOUBLESTAR); // 消耗 '**'
            skip_newline();
            result.var_kwargs_name_ = expect(TokenType::IDENTIFIER).lexeme; // 消耗标识符
            return;
        }

        // 当前不是 **kwargs -> 要求是前边不能有 **kwargs
        if (result.var_kwargs_name_) error("no parameter is allowed after **kwargs");

        // 当前是 *args -> 要求是前边不能有 *args
        if (check(TokenType::SIGN_STAR)) {
            if (result.var_args_name_) error("at most one *args parameter is allowed");
            expect(TokenType::SIGN_STAR); // 消耗 '*'
            skip_newline();
            result.var_args_name_ = expect(TokenType::IDENTIFIER).lexeme; // 消耗标识符
            return;
        }

        // 前边无 *args、无 **kwargs，当前是普通形参 -> 判断看看到底是位置形参还是
        (result.var_args_name_ ? result.kw_only_ : result.positional_)
            .push_back(parse_one_normal_param());
    });

    if (!check(TokenType::SIGN_RPAREN)) error("expected ')' to close parameter list");

    expect(TokenType::SIGN_RPAREN), paren_depth_--; // 消耗 ')'

    return result;
}

AstNodePtr Parser::finish_dict(const Position start_pos, AstNodePtr first) {
    std::vector<std::pair<AstNodePtr, AstNodePtr>> items;

    // 放置一个已解析出来的项
    auto emplace_item{[&](AstNodePtr item) {
        if (dynamic_cast<AstNodeDoubleStar *>(item.get())) {
            items.emplace_back(std::move(item), nullptr);
        } else {
            skip_newline();
            expect(TokenType::SIGN_COLON); // 消耗 ':'
            skip_newline();
            items.emplace_back(std::move(item), parse_expr());
        }
    }};

    emplace_item(std::move(first));
    skip_newline();
    while (check(TokenType::SIGN_COMMA)) {
        expect(TokenType::SIGN_COMMA); // 消耗 ','
        skip_newline();
        if (check(TokenType::SIGN_RBRACE)) break; // 尾逗号
        emplace_item(parse_expr());
        skip_newline();
    }

    return std::make_unique<AstNodeLiteralDict>(start_pos, std::move(items));
}

std::unique_ptr<AstNodeCall> Parser::finish_call(AstNodePtr obj, const Position start_pos) {
    const Position paren_pos{peek().row, peek().col};
    expect(TokenType::SIGN_LPAREN), paren_depth_++; // 消耗 '('

    // 实参
    std::vector<AstNodePtr> positional_args;
    std::vector<OneKwArg> keyword_args;

    // 传参分：位置组（位置传参、*expr 展开）、关键字组（关键字传参、**expr 展开）两阶段
    enum class ArgsGroup { Positional, Keyword } group{ArgsGroup::Positional};
    finish_comma_batch(TokenType::SIGN_RPAREN, [&] {
        // 关键字传参的判定：当前是 IDENTIFIER，且跳过其后可能的换行紧跟 '='
        if (check(TokenType::IDENTIFIER) && check_over_newline(TokenType::SIGN_ASSIGN, pos_ + 1)) {
            group = ArgsGroup::Keyword;
            std::u32string name{expect(TokenType::IDENTIFIER).lexeme}; // 消耗标识符
            skip_newline();
            expect(TokenType::SIGN_ASSIGN); // 消耗 '='
            skip_newline();
            keyword_args.push_back({OneKwArg::Kind::Keyword, std::move(name), parse_expr()});
            return;
        }

        // 不是关键字传参
        AstNodePtr value{parse_expr()};
        // **expr
        if (dynamic_cast<AstNodeDoubleStar *>(value.get())) {
            group = ArgsGroup::Keyword;
            keyword_args.push_back({OneKwArg::Kind::DoubleStar, U"", std::move(value)});
            return;
        }
        // 位置参数 / *expr
        if (group == ArgsGroup::Keyword)
            error("positional argument cannot appear after keyword argument");
        positional_args.push_back(std::move(value));
    });

    if (!check(TokenType::SIGN_RPAREN)) error("expected ')' to close function call");
    expect(TokenType::SIGN_RPAREN), paren_depth_--; // 消耗 ')'

    return std::make_unique<AstNodeCall>(
        start_pos, std::move(obj), std::move(positional_args), std::move(keyword_args), paren_pos
    );
}

AstNodePtr Parser::finish_index(AstNodePtr obj, const Position start_pos) {
    const Position bracket_pos{peek().row, peek().col};
    expect(TokenType::SIGN_LBRACKET), paren_depth_++; // 消耗 '['

    std::vector<AstNodePtr> args;
    finish_comma_batch(TokenType::SIGN_RBRACKET, [&] { args.push_back(parse_expr()); });

    // a[]：不允许，索引至少需要一个下标
    if (args.empty()) error("expected at least one argument for indexing");

    if (!check(TokenType::SIGN_RBRACKET)) error("expected ']' to close index expression");

    expect(TokenType::SIGN_RBRACKET), paren_depth_--; // 消耗 ']'

    return std::make_unique<AstNodeIndex>(start_pos, std::move(obj), std::move(args), bracket_pos);
}

Parser::Parser(std::vector<Token> tokens, std::string file_path)
    : tokens_{std::move(tokens)}, file_path_{std::move(file_path)} {
    // 1. 空的肯定不行
    if (tokens_.empty()) error_internal("Bad tokens: empty token list");

    // 2. 最后必须是 END_OF_FILE
    if (tokens_.back().type != TokenType::END_OF_FILE) {
        error_internal(
            "Bad tokens: missing END_OF_FILE token at the end",
            {tokens_.back().row, tokens_.back().col}
        );
    }

    // 3. 前边不能有 END_OF_FILE
    if (const auto it{std::find_if(
            tokens_.begin(),
            tokens_.end() - 1,
            [](const Token &t) -> bool { return t.type == TokenType::END_OF_FILE; }
        )};
        it != tokens_.end() - 1) {
        error_internal("Bad tokens: unexpected END_OF_FILE", {it->row, it->col});
    }
}

AstNodeProgramPtr Parser::parse() && {
    const Position start_pos{peek().row, peek().col};

    // 解析一个若干个表达式
    std::vector exprs{parse_exprs()};

    // 必须是解析完了，否则肯定是语法错误
    expect(TokenType::END_OF_FILE); // 消耗 EOF

    return std::make_unique<AstNodeProgram>(start_pos, std::move(exprs));
}
