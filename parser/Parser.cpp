#include "Parser.h"

#include "../builtins/classes/exceptions/SyntaxError.h"
#include "../lexer/Lexer.h"
#include "../utils/string_utils.h"

#include <algorithm>
#include <cassert>
#include <format>
#include <memory>
#include <optional>
#include <ranges>

// token 类型转换为一元运算符类型
static AstNodeOpUnary::OpType token_type_to_unary_op_type(const TokenType t) {
    switch (t) {
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

// token 类型转换为二元运算符类型（不含比较运算符，见 token_type_to_compare_op_type）
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
    case TokenType::KW_AND: return AstNodeOpBinary::OpType::And;
    case TokenType::KW_OR: return AstNodeOpBinary::OpType::Or;
    case TokenType::SIGN_DOTDOT: return AstNodeOpBinary::OpType::Range;
    default:
        assert(false && "not a binary op token");
    }
}

// 是否是比较组（== != < <= > >=）的运算符 token；is 不属于这一组，也不共用 AstNodeCompare，见 AstNodeIs
static bool is_compare_op(const TokenType t) {
    switch (t) {
    case TokenType::SIGN_LT:
    case TokenType::SIGN_LE:
    case TokenType::SIGN_GT:
    case TokenType::SIGN_GE:
    case TokenType::SIGN_EQ:
    case TokenType::SIGN_NE: return true;
    default: return false;
    }
}

// token 类型转换为比较运算类型
static AstNodeCompare::OpType token_type_to_compare_op_type(const TokenType t) {
    switch (t) {
    case TokenType::SIGN_LT: return AstNodeCompare::OpType::Lt;
    case TokenType::SIGN_LE: return AstNodeCompare::OpType::Le;
    case TokenType::SIGN_GT: return AstNodeCompare::OpType::Gt;
    case TokenType::SIGN_GE: return AstNodeCompare::OpType::Ge;
    case TokenType::SIGN_EQ: return AstNodeCompare::OpType::Eq;
    case TokenType::SIGN_NE: return AstNodeCompare::OpType::Ne;
    default:
        assert(false && "not a compare op token");
    }
}

// 运算符绑定力表
// 对中缀/后缀运算符，返回 {lbp, rbp}
// {-1,-1} 表示不是中缀/后缀运算符
// 注：左结合运算符 rbp = lbp + 1（保证 parse_expr_pratt(rbp) 不会把同优先级的下一个算子吞进右操作数，
//    否则会变成事实上的右结合，如 `1 - 2 - 3` 就会被错误地解析成 `1 - (2 - 3)`）；
//    右结合运算符（**、赋值类）则 rbp = lbp - 1，允许同优先级递归吞并。
static std::pair<int, int> infix_bp(const TokenType type) {
    switch (type) {
    case TokenType::SIGN_DOT: return {170, 170}; // rbp 未使用，'.' 后直接 expect(IDENTIFIER)，不递归
    case TokenType::SIGN_LPAREN:
    case TokenType::SIGN_LBRACKET: return {170, -1}; // 函数调用、索引
    case TokenType::SIGN_QUESTION:
    case TokenType::SIGN_EXCLAIM: return {160, -1}; // ? !
    case TokenType::SIGN_DOUBLESTAR: return {150, 149}; // **（右结合）
    case TokenType::SIGN_STAR:
    case TokenType::SIGN_SLASH:
    case TokenType::SIGN_DOUBLESLASH:
    case TokenType::SIGN_PERCENT: return {130, 131}; // * / // %
    case TokenType::SIGN_PLUS:
    case TokenType::SIGN_MINUS: return {120, 121}; // + -
    case TokenType::SIGN_DOTDOT: return {110, 111}; // ..
    case TokenType::SIGN_LSHIFT:
    case TokenType::SIGN_RSHIFT: return {100, 101}; // << >>
    case TokenType::SIGN_AMPERSAND: return {90, 91}; // &
    case TokenType::SIGN_CARET: return {80, 81}; // ^
    case TokenType::SIGN_PIPE: return {70, 71}; // |
    // < <= > >= == !=（链式比较，见 parse_compare_chain；rbp 未被使用，链内自行控制操作数的 min_bp）
    case TokenType::SIGN_LT:
    case TokenType::SIGN_LE:
    case TokenType::SIGN_GT:
    case TokenType::SIGN_GE:
    case TokenType::SIGN_EQ:
    case TokenType::SIGN_NE: return {60, 61};
    case TokenType::KW_IS: return {50, 51}; // is（自成一组的链式比较，不与上面 6 者混链）
    case TokenType::KW_AND: return {30, 31}; // and
    case TokenType::KW_OR: return {20, 21}; // or
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
              Position{token.row, token.col});
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

void Parser::error(const std::string &msg, const Position pos) const {
    throw SyntaxError{file_path_, pos.row, pos.col, msg};
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

    const Position start_pos{left->pos_};

    while (true) {
        // 括号内允许运算符前换行（如多行链式调用）
        skip_paren_newline();

        const auto &[op, op_row, op_col, lexeme]{peek()};
        const Position op_pos{op_row, op_col};
        const auto [lbp, rbp]{infix_bp(op)};

        // lbp == -1 表示非中缀/后缀运算符；lbp < min_bp 表示绑定力不足，让上层处理
        if (lbp < min_bp) break;

        advance(); // 消耗运算符

        // 赋值（右结合，rbp = lbp - 1 = 9）
        if (op == TokenType::SIGN_ASSIGN) {
            skip_newline();
            return std::make_unique<AstNodeAssign>(
                start_pos, std::move(left), parse_expr_pratt(rbp)
                );
        }

        // 复合赋值 x op= y
        if (is_assign_op(op)) {
            skip_newline();
            return std::make_unique<AstNodeCompoundAssign>(
                start_pos, std::move(left), assign_compound_to_binary(op), parse_expr_pratt(rbp), op_pos
                );
        }

        // 后缀 x?  x!（节点位置取整个表达式的起始位置，而非运算符自己的位置，下同；
        // op_pos 是运算符自己的位置，另外单独记）
        if (op == TokenType::SIGN_QUESTION || op == TokenType::SIGN_EXCLAIM) {
            left = std::make_unique<AstNodeOpUnary>(
                start_pos, token_type_to_unary_op_type(op), std::move(left), op_pos
                );
            continue;
        }

        // 函数调用 f(...)（op_pos 即 '(' 自己的位置）
        if (op == TokenType::SIGN_LPAREN) {
            paren_depth_++;
            left = finish_call(std::move(left), start_pos, op_pos);
            continue;
        }

        // 索引 x[...]（op_pos 即 '[' 自己的位置）
        if (op == TokenType::SIGN_LBRACKET) {
            paren_depth_++;
            left = finish_index(std::move(left), start_pos, op_pos);
            continue;
        }

        // 属性访问 x.attr（'.' 在行尾时 attr 可换行；op_pos 即 '.' 自己的位置）
        if (op == TokenType::SIGN_DOT) {
            skip_newline();
            left = std::make_unique<AstNodeAttr>(
                start_pos, std::move(left), expect(TokenType::IDENTIFIER).lexeme, op_pos
                );
            continue;
        }

        // 比较运算（链式）：== != < <= > >= 一组，用 AstNodeCompare
        if (is_compare_op(op)) {
            left = parse_compare_chain(std::move(left), start_pos, op, op_pos);
            continue;
        }

        // is（链式，自成一组，不与上面 6 者混链）：不可重载、不走 AstNodeCompare，用专门的 AstNodeIs
        if (op == TokenType::KW_IS) {
            left = parse_is_chain(std::move(left), start_pos, op_pos);
            continue;
        }

        // 普通二元运算符（运算符在行尾时右侧可换行；节点位置取左操作数的起始位置，op_pos 才是运算符自己的位置）
        skip_newline();
        left = std::make_unique<AstNodeOpBinary>(
            start_pos, token_type_to_binary_op_type(op), std::move(left), parse_expr_pratt(rbp), op_pos
            );
    }

    return left;
}

AstNodePtr Parser::parse_cond() {
    // min_bp=11 卡住裸的赋值类运算符（lbp=10 < 11 会让 Pratt 主循环在它们前面停下）
    AstNodePtr left{parse_expr_pratt(11)};
    const Position start_pos{left->pos_};

    skip_paren_newline();

    if (is_assign_op(peek().type)) {
        if (peek().type == TokenType::SIGN_ASSIGN) {
            error("bare assignment '=' is not allowed directly in a condition "
                  "(did you mean '=='? wrap it in an extra pair of parentheses if intentional, e.g. `if ((x = y))`)",
                  Position{peek().row, peek().col});
        }

        // 复合赋值 x op= y：没有 = 和 == 混淆的手误风险，允许裸写
        const auto &[op, op_row, op_col, lexeme]{advance()};
        const Position op_pos{op_row, op_col};
        const int rbp{infix_bp(op).second};
        skip_newline();
        left = std::make_unique<AstNodeCompoundAssign>(
            start_pos, std::move(left), assign_compound_to_binary(op), parse_expr_pratt(rbp), op_pos
            );
    }

    return left;
}

AstNodePtr Parser::parse_compare_chain(AstNodePtr left, const Position start_pos,
                                       const TokenType first_op, const Position first_op_pos) {
    // first_op 已经被消耗（由调用处的 parse_expr_pratt 主循环 advance），属于比较组（< <= > >= == !=）
    // 后续操作数用"比较组优先级 60 + 1"解析，防止同组递归吞并（链式循环自己处理连续项）
    constexpr int operand_min_bp{61};

    std::vector<AstNodePtr> operands;
    std::vector<AstNodeCompare::OpType> ops;
    std::vector<Position> op_positions;

    operands.push_back(std::move(left));
    ops.push_back(token_type_to_compare_op_type(first_op));
    op_positions.push_back(first_op_pos);

    skip_newline();
    operands.push_back(parse_expr_pratt(operand_min_bp));

    while (true) {
        skip_paren_newline();
        if (!is_compare_op(peek().type)) break;

        const Position next_op_pos{peek().row, peek().col};
        const TokenType next_op{advance().type}; // 消耗运算符
        ops.push_back(token_type_to_compare_op_type(next_op));
        op_positions.push_back(next_op_pos);
        skip_newline();
        operands.push_back(parse_expr_pratt(operand_min_bp));
    }

    return std::make_unique<AstNodeCompare>(
        start_pos, std::move(ops), std::move(operands), std::move(op_positions)
        );
}

AstNodePtr Parser::parse_is_chain(AstNodePtr left, const Position start_pos, const Position first_is_pos) {
    // 第一个 'is' 已经被消耗（由调用处的 parse_expr_pratt 主循环 advance）
    // 后续操作数用"is 优先级 50 + 1"解析，防止递归吞并（链式循环自己处理连续的 is）
    constexpr int operand_min_bp{51};

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
        advance(); // 消耗 'is'
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
    case TokenType::LITERAL_NONE: return advance(), std::make_unique<AstNodeLiteralNone>(pos);
    case TokenType::LITERAL_TRUE: return advance(), std::make_unique<AstNodeLiteralBool>(pos, true);
    case TokenType::LITERAL_FALSE: return advance(), std::make_unique<AstNodeLiteralBool>(pos, false);
    case TokenType::LITERAL_G:
        // _G
        return advance(), std::make_unique<AstNodeLiteralGL>(pos, AstNodeLiteralGL::GLType::G);
    case TokenType::LITERAL_L:
        // _L
        return advance(), std::make_unique<AstNodeLiteralGL>(pos, AstNodeLiteralGL::GLType::L);
    case TokenType::LITERAL_ELLIPSIS: return advance(), std::make_unique<AstNodeLiteralEllipsis>(pos);
    case TokenType::LITERAL_INT: return std::make_unique<AstNodeLiteralInt>(pos, advance().lexeme);
    case TokenType::LITERAL_FLOAT: return std::make_unique<AstNodeLiteralFloat>(pos, advance().lexeme);
    case TokenType::LITERAL_STR: return std::make_unique<AstNodeLiteralStr>(pos, advance().lexeme);

    // 分组、元组
    case TokenType::SIGN_LPAREN: return parse_paren_or_tuple();
    // 列表
    case TokenType::SIGN_LBRACKET: return parse_list();
    // 字典、复合表达式
    case TokenType::SIGN_LBRACE: return parse_brace_block();

    // 标识符
    case TokenType::IDENTIFIER: return std::make_unique<AstNodeIdentifier>(pos, advance().lexeme);

    // 解包 / 展开（操作数按“单目运算符”那一档的优先级 140 解析，与 +x/-x/~x 一致）
    case TokenType::SIGN_STAR:
        // *iterable
        return advance(), std::make_unique<AstNodeStar>(pos, parse_expr_pratt(140));
    case TokenType::SIGN_DOUBLESTAR:
        // **mapping
        return advance(), std::make_unique<AstNodeDoubleStar>(pos, parse_expr_pratt(140));

    // 前缀运算符（运算符自己的位置就是 pos，前缀形式下和整个表达式的起始位置重合）
    case TokenType::SIGN_PLUS:
    case TokenType::SIGN_MINUS:
    case TokenType::SIGN_TILDE: {
        // 单目优先级 140
        const TokenType token_type{advance().type};
        return std::make_unique<AstNodeOpUnary>(
            pos, token_type_to_unary_op_type(token_type), parse_expr_pratt(140), pos
            );
    }
    case TokenType::KW_NOT:
        // not 优先级 40
        return advance(), std::make_unique<AstNodeOpUnary>(
                   pos, AstNodeOpUnary::OpType::Not, parse_expr_pratt(40), pos
                   );

    // del / global
    case TokenType::KW_DEL: return parse_del();
    case TokenType::KW_GLOBAL: return parse_global();

    // 控制流
    case TokenType::KW_IF: return parse_if();
    case TokenType::KW_FOR: return parse_for();
    case TokenType::KW_WHILE: return parse_while();
    case TokenType::KW_BREAK: return advance(), std::make_unique<AstNodeBreak>(pos);
    case TokenType::KW_CONTINUE: return advance(), std::make_unique<AstNodeContinue>(pos);
    case TokenType::KW_RETURN: return parse_return();
    case TokenType::KW_TRY: return parse_try();
    case TokenType::KW_RAISE: return parse_raise();

    // 函数
    case TokenType::KW_FUNC: return parse_func();

    // 装饰器
    case TokenType::SIGN_AT: return parse_decorator();

    // 类
    case TokenType::KW_CLASS: return parse_class();

    // 遇到 EOF：括号内多半是没闭合，否则是缺了表达式
    case TokenType::END_OF_FILE: {
        if (paren_depth_ > 0) error("unexpected end of file (unclosed bracket)", pos);
        error("unexpected end of file (expected an expression)", pos);
    }

    // 错误
    default: error(std::format("unexpected token '{}'", u32_to_utf8(lexeme)), pos);
    }
}

AstNodePtr Parser::parse_paren_or_tuple() {
    const Position start_pos{peek().row, peek().col};

    expect(TokenType::SIGN_LPAREN); // 消耗 '('
    paren_depth_++;

    skip_newline();

    // 空元组 ()
    if (check(TokenType::SIGN_RPAREN)) {
        advance(); // 消耗 ')'
        paren_depth_--;
        return std::make_unique<AstNodeLiteralTuple>(start_pos, std::vector<AstNodePtr>{});
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

    return std::make_unique<AstNodeLiteralTuple>(start_pos, std::move(items));
}

AstNodePtr Parser::parse_list() {
    const Position start_pos{peek().row, peek().col};

    expect(TokenType::SIGN_LBRACKET); // 消耗 '['
    paren_depth_++;

    skip_newline();

    // 空列表 []
    if (check(TokenType::SIGN_RBRACKET)) {
        advance(); // 消耗 ']'
        paren_depth_--;
        return std::make_unique<AstNodeLiteralList>(start_pos, std::vector<AstNodePtr>{});
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

    return std::make_unique<AstNodeLiteralList>(start_pos, std::move(items));
}

AstNodePtr Parser::parse_brace_block() {
    const Position start_pos{peek().row, peek().col};

    expect(TokenType::SIGN_LBRACE); // 消耗 '{'

    // 跳过前导终止符
    skip_terminator();

    // 空 {} → 空的复合表达式
    if (check(TokenType::SIGN_RBRACE)) {
        advance();
        return std::make_unique<AstNodeCompound>(start_pos, std::vector<AstNodePtr>{});
    }

    // ** 开头必定是字典展开项，否则先解析第一个表达式再看 ':'
    const bool first_is_doublestar{check(TokenType::SIGN_DOUBLESTAR)};
    AstNodePtr first{parse_expr()};

    // 字典字面量 {k: v, ...} 或 {**d, ...}
    if (first_is_doublestar || check_over_newline(TokenType::SIGN_COLON)) {
        std::vector<std::pair<AstNodePtr, AstNodePtr>> items;

        if (first_is_doublestar) {
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
            // 与第一项一致：直接看是不是以 '**' 开头来判断是否为展开项，
            // 而不是"解析完键之后看有没有冒号"来反推——否则 {k: v, x}（x 既非 **expr 也没有冒号）
            // 会被误判成合法的展开项，把校验漏过去
            if (check(TokenType::SIGN_DOUBLESTAR)) {
                items.emplace_back(parse_expr(), nullptr); // **expr，无 value
            } else {
                AstNodePtr key{parse_expr()};
                skip_newline();
                expect(TokenType::SIGN_COLON);
                skip_newline();
                items.emplace_back(std::move(key), parse_expr());
            }
            skip_newline();
        }

        skip_newline();
        expect(TokenType::SIGN_RBRACE);
        return std::make_unique<AstNodeLiteralDict>(start_pos, std::move(items));
    }

    // 复合表达式 {expr; ...}
    std::vector<AstNodePtr> exprs;
    exprs.push_back(std::move(first));
    for (AstNodePtr &e : parse_exprs()) exprs.push_back(std::move(e));
    expect(TokenType::SIGN_RBRACE);
    return std::make_unique<AstNodeCompound>(start_pos, std::move(exprs));
}

AstNodePtr Parser::parse_del() {
    const Position start_pos{peek().row, peek().col};
    expect(TokenType::KW_DEL);
    skip_newline();
    // 语法层只解析一个表达式，target 是否为标识符由语义层校验
    return std::make_unique<AstNodeDel>(start_pos, parse_expr());
}

AstNodePtr Parser::parse_global() {
    const Position start_pos{peek().row, peek().col};
    expect(TokenType::KW_GLOBAL);
    skip_newline();
    // 语法本身就是 identifier（2.2.4），不是表达式，直接要求一个标识符 token，没有什么好交给语义层判形状的
    return std::make_unique<AstNodeGlobal>(start_pos, expect(TokenType::IDENTIFIER).lexeme);
}

AstNodePtr Parser::parse_if() {
    const Position start_pos{peek().row, peek().col};
    expect(TokenType::KW_IF);

    std::vector<AstNodeIf::AstNodeCondAndExpr> clauses;

    // 解析一个 if/elif 子句的条件和主体
    auto parse_cond_and_body{
        [&]() -> AstNodeIf::AstNodeCondAndExpr {
            skip_newline();
            expect(TokenType::SIGN_LPAREN); // 消耗 '('
            paren_depth_++;
            AstNodePtr cond{parse_cond()};
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

        return std::make_unique<AstNodeIf>(start_pos, std::move(clauses), parse_expr());
    }

    // 无 else
    return std::make_unique<AstNodeIf>(start_pos, std::move(clauses), nullptr);
}

AstNodePtr Parser::parse_for() {
    const Position start_pos{peek().row, peek().col};
    expect(TokenType::KW_FOR);
    skip_newline();

    const bool collect{check(TokenType::SIGN_DOLLAR)};
    if (collect) advance(); // 消耗 '$'
    skip_newline();

    expect(TokenType::SIGN_LPAREN); // 消耗 '('
    paren_depth_++;
    skip_newline();

    // 解析 for 头部的一个槽：遇到 ';', NEWLINE, ')' 则槽为空，返回 nullptr；
    // restrict_assign 为 true 时该槽走 parse_cond（禁止裸的普通赋值 =），用于三槽形式的中间 cond 槽；
    // init/inc 槽本身就是为赋值而生（如 for (i = 0; ...; i += 1)），不加此限制
    auto parse_slot{
        [&](const bool restrict_assign) -> AstNodePtr {
            if (check(TokenType::SIGN_SEMICOLON) ||
                check(TokenType::NEWLINE) ||
                check(TokenType::SIGN_RPAREN))
                return nullptr;
            return restrict_assign ? parse_cond() : parse_expr();
        }
    };

    // 先读第一个槽（可能为空）。它要么是迭代目标（后跟 ':'），要么是步进模式的 init
    AstNodePtr first{parse_slot(false)};

    // 1. 第一个槽后紧跟 ':' → 迭代模式：for [$] (target : iterable) body
    if (check_over_newline(TokenType::SIGN_COLON)) {
        skip_newline();
        advance(); // 消耗 ':'
        skip_newline();
        AstNodePtr iterable{parse_expr()};
        skip_newline();
        paren_depth_--;
        expect(TokenType::SIGN_RPAREN);
        skip_newline();
        AstNodePtr body{parse_expr()};
        return std::make_unique<AstNodeForIter>(
            start_pos, collect,
            std::move(first), std::move(iterable), std::move(body)
            );
    }

    // 2. for ()：第一个槽为空、且直接紧跟 ')'，即整个头部彻底为空。
    //    SL.md 2.2.5.2 只有步进模式（三槽用 ';'/换行分隔，空槽也须显式 ';'）和迭代模式两种语法，
    //    没有"裸单表达式当条件"的第三种写法；无限循环请用 for (;;)，纯条件循环请用 while (cond)。
    if (!first && check_over_newline(TokenType::SIGN_RPAREN)) {
        error("empty for header (for an infinite loop use `for (;;)`; for a plain condition use `while (cond)`)");
    }

    // 3. 否则为步进模式：for [$] (init SEP cond SEP inc) body，first 即 init
    //    SEP（分隔符）为 ';' 或至少一个换行；两个槽之间必须有 SEP，否则无法无歧义地分割
    //    注：括号内 paren_depth_ > 0，槽末尾的换行可能已经被上一个槽内部 Pratt 循环的边界检查
    //    （skip_paren_newline）提前吃掉，此时再直接 check(NEWLINE) 会误判为"没有分隔符"，
    //    所以改为比较"当前 token 所在行"与"上一个已消耗 token 所在行"是否不同来判断换行分隔符是否存在
    auto consume_sep{
        [&] {
            if (check(TokenType::SIGN_SEMICOLON)) {
                advance(); // 消耗 ';'
                skip_newline();
                return;
            }
            if (tokens_[pos_ - 1].row == peek().row) {
                error("expected ';' or newline to separate the expressions in a for header");
            }
            skip_newline();
        }
    };

    consume_sep();
    AstNodePtr cond{parse_slot(true)};
    consume_sep();
    AstNodePtr inc{parse_slot(false)};
    skip_newline();

    paren_depth_--;
    expect(TokenType::SIGN_RPAREN); // 消耗 ')'
    skip_newline();
    AstNodePtr body{parse_expr()};

    return std::make_unique<AstNodeForCond>(
        start_pos, collect,
        std::move(first), std::move(cond), std::move(inc), std::move(body)
        );
}

AstNodePtr Parser::parse_while() {
    // while [$] (cond) body 语义上就是 init/inc 皆空的 for，直接复用 AstNodeForCond，不单独建节点类型
    const Position start_pos{peek().row, peek().col};
    expect(TokenType::KW_WHILE);
    skip_newline();

    const bool collect{check(TokenType::SIGN_DOLLAR)};
    if (collect) advance(); // 消耗 '$'
    skip_newline();

    expect(TokenType::SIGN_LPAREN); // 消耗 '('
    paren_depth_++;
    AstNodePtr cond{parse_cond()};
    paren_depth_--;
    expect(TokenType::SIGN_RPAREN); // 消耗 ')'
    skip_newline();

    AstNodePtr body{parse_expr()};

    return std::make_unique<AstNodeForCond>(
        start_pos, collect, nullptr, std::move(cond), nullptr, std::move(body)
        );
}

AstNodePtr Parser::parse_try() {
    const Position start_pos{peek().row, peek().col};
    expect(TokenType::KW_TRY);

    // 与 if 不同：try 后没有 '()'，直接跟主体
    skip_newline();
    AstNodePtr try_expr{parse_expr()};

    // 解析一个 except 子句的异常列表和主体：except (Exc1, Exc2, ...) body
    // 与 if 的 parse_cond_and_body 平行，区别是括号内为一个或多个表达式
    auto parse_excs_and_body{
        [&]() -> AstNodeTry::AstNodeExceptAndExpr {
            skip_newline();
            expect(TokenType::SIGN_LPAREN); // 消耗 '('
            paren_depth_++;
            skip_newline();

            // 解析 Exception1, ...
            std::vector<AstNodePtr> excs;
            excs.push_back(parse_expr());
            skip_newline();
            while (check(TokenType::SIGN_COMMA)) {
                advance(); // 消耗 ','
                skip_newline();
                if (check(TokenType::SIGN_RPAREN)) break; // 尾逗号
                excs.push_back(parse_expr());
                skip_newline();
            }

            skip_newline();
            paren_depth_--;
            expect(TokenType::SIGN_RPAREN); // 消耗 ')'
            skip_newline();
            AstNodePtr body{parse_expr()};
            return {std::move(excs), std::move(body)};
        }
    };

    std::vector<AstNodeTry::AstNodeExceptAndExpr> except_clauses;

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
            start_pos,
            std::move(try_expr), std::move(except_clauses), parse_expr()
            );
    }

    // 注：except / finally 至少有一个，这一约束交由语义层检查
    return std::make_unique<AstNodeTry>(
        start_pos,
        std::move(try_expr), std::move(except_clauses), nullptr
        );
}

AstNodePtr Parser::parse_return() {
    const Position start_pos{peek().row, peek().col};
    expect(TokenType::KW_RETURN);
    // 若紧跟终止符则为裸 return（值为 None）
    // 终止符 = 语句终止符 NEWLINE, ';', EOF, '}'
    //        + 括号语境的闭合 / 分隔符 ')', ']', ','（如 for(;;return)、(return,)、[return]、f(return)）
    if (check(TokenType::NEWLINE) || check(TokenType::SIGN_SEMICOLON) ||
        check(TokenType::END_OF_FILE) || check(TokenType::SIGN_RBRACE) ||
        check(TokenType::SIGN_RPAREN) || check(TokenType::SIGN_RBRACKET) ||
        check(TokenType::SIGN_COMMA))
        return std::make_unique<AstNodeReturn>(start_pos, nullptr);

    return std::make_unique<AstNodeReturn>(start_pos, parse_expr());
}

AstNodePtr Parser::parse_raise() {
    const Position start_pos{peek().row, peek().col};
    expect(TokenType::KW_RAISE);
    skip_newline();
    return std::make_unique<AstNodeRaise>(start_pos, parse_expr());
}

AstNodePtr Parser::parse_func(std::vector<AstNodePtr> decorators, std::vector<Position> decorator_positions,
                              const Position deco_pos) {
    const Position start_pos{decorators.empty() ? Position{peek().row, peek().col} : deco_pos};
    expect(TokenType::KW_FUNC);
    skip_newline();

    // 可选函数名（无名即匿名函数）
    std::optional<std::u32string> name;
    if (check(TokenType::IDENTIFIER)) {
        name = advance().lexeme;
        skip_newline();
    }

    // 可选捕获列表 [captures]
    std::vector<AstNodeFunc::OneCapture> captures;
    if (check(TokenType::SIGN_LBRACKET)) {
        advance(); // 消耗 '['
        captures = finish_func_captures();
        skip_newline();
    }

    // 形参列表（合法性由语义层检查）
    expect(TokenType::SIGN_LPAREN);
    paren_depth_++;
    skip_newline();

    std::vector<AstNodeFunc::OneParam> params{};
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

    // 可选返回类型 : type
    AstNodePtr return_type;
    if (check(TokenType::SIGN_COLON)) {
        advance(); // 消耗 ':'
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

    // 函数体：{ ... }
    const Position body_pos{peek().row, peek().col};
    expect(TokenType::SIGN_LBRACE);
    std::vector body_exprs{parse_exprs()};
    expect(TokenType::SIGN_RBRACE);

    return std::make_unique<AstNodeFunc>(
        start_pos, std::move(decorators), std::move(decorator_positions), std::move(name),
        std::move(captures), std::move(params), std::move(return_type), std::move(doc),
        std::make_unique<AstNodeProgram>(body_pos, std::move(body_exprs))
        );
}

AstNodePtr Parser::parse_class(std::vector<AstNodePtr> decorators, std::vector<Position> decorator_positions,
                               const Position deco_pos) {
    const Position start_pos{decorators.empty() ? Position{peek().row, peek().col} : deco_pos};
    expect(TokenType::KW_CLASS);
    skip_newline();

    // 可选类名（无名即匿名类）
    std::optional<std::u32string> name;
    if (check(TokenType::IDENTIFIER)) {
        name = advance().lexeme;
        skip_newline();
    }

    // 可选基类列表 (BaseClass1, ...)
    std::vector<AstNodePtr> bases;
    if (check(TokenType::SIGN_LPAREN)) {
        advance(); // 消耗 '('
        paren_depth_++;
        bases = finish_class_bases();
        skip_newline();
    }

    // 可选文档字符串（下一个 token 不是 '{' 则视为 doc）
    AstNodePtr doc;
    if (!check(TokenType::SIGN_LBRACE)) {
        doc = parse_expr();
        skip_newline();
    }

    // 类体：{ ... }
    const Position body_pos{peek().row, peek().col};
    expect(TokenType::SIGN_LBRACE);
    std::vector body_exprs{parse_exprs()};
    expect(TokenType::SIGN_RBRACE);

    return std::make_unique<AstNodeClass>(
        start_pos, std::move(decorators), std::move(decorator_positions), std::move(name),
        std::move(bases), std::move(doc),
        std::make_unique<AstNodeProgram>(body_pos, std::move(body_exprs))
        );
}

AstNodePtr Parser::parse_decorator() {
    // 先把连续的前缀 @decorator 全部收集起来（不预先假设后面接的是 func/class 还是任意表达式），
    // 每项记住自己的位置，供后面（通用形式分支）构造对应节点时使用
    struct DecoratorEntry {
        Position pos;
        AstNodePtr expr;
    };
    std::vector<DecoratorEntry> entries;

    while (check(TokenType::SIGN_AT)) {
        const Position deco_pos{peek().row, peek().col};
        advance(); // 消耗 '@'
        skip_newline();
        // 解析装饰器表达式（Pratt 在遇到 func / class / @ 前会自然停止，因为它们都不是中缀运算符）
        AstNodePtr decorator{parse_expr()};
        skip_newline();
        entries.push_back({deco_pos, std::move(decorator)});
    }

    // 紧邻 func/class：这些装饰器是函数/类表达式自己产生式的一部分，
    // 直接挂到对应节点的 decorators_ 上，不包一层 AstNodeDecorator；节点自己的起始位置也相应地
    // 从第一个 '@' 算起（parse_decorator 只在当前 token 就是 '@' 时才会被调用，entries 必然非空）
    if (check(TokenType::KW_FUNC)) {
        const Position deco_pos{entries.front().pos};
        std::vector<AstNodePtr> decorators;
        std::vector<Position> decorator_positions;
        decorators.reserve(entries.size());
        decorator_positions.reserve(entries.size());
        for (auto &e : entries) {
            decorators.push_back(std::move(e.expr));
            decorator_positions.push_back(e.pos);
        }
        return parse_func(std::move(decorators), std::move(decorator_positions), deco_pos);
    }
    if (check(TokenType::KW_CLASS)) {
        const Position deco_pos{entries.front().pos};
        std::vector<AstNodePtr> decorators;
        std::vector<Position> decorator_positions;
        decorators.reserve(entries.size());
        decorator_positions.reserve(entries.size());
        for (auto &e : entries) {
            decorators.push_back(std::move(e.expr));
            decorator_positions.push_back(e.pos);
        }
        return parse_class(std::move(decorators), std::move(decorator_positions), deco_pos);
    }

    // 通用形式（2.2.8）：@d1 @d2 ... expr ≡ d1(d2(...(expr)))，从最贴近 expr 的装饰器开始向外包裹
    AstNodePtr target{parse_expr()};
    for (auto &[pos, expr] : std::views::reverse(entries)) {
        target = std::make_unique<AstNodeDecorator>(pos, std::move(expr), std::move(target));
    }
    return target;
}

AstNodePtr Parser::finish_call(AstNodePtr callee, const Position pos, const Position paren_pos) {
    // '(' 已消耗，paren_depth_ 已自增
    std::vector<AstNodePtr> args;
    std::vector<std::pair<std::u32string, AstNodePtr>> kwargs;

    skip_newline();
    while (!check(TokenType::SIGN_RPAREN)) {
        skip_newline();

        if (at_kwarg()) {
            // 关键字参数 name = value
            auto name = advance().lexeme; // IDENTIFIER
            skip_newline();
            expect(TokenType::SIGN_ASSIGN);
            skip_newline();
            kwargs.emplace_back(std::move(name), parse_expr());
        } else {
            // 位置参数，含 *expr / **expr 展开（参数顺序合法性由语义层校验）
            args.push_back(parse_expr());
        }

        skip_newline();
        if (check(TokenType::SIGN_COMMA)) {
            advance();
            skip_newline();
        } else {
            break;
        }
    }

    skip_newline();
    if (!check(TokenType::SIGN_RPAREN)) error("expected ')' to close function call", Position{peek().row, peek().col});
    advance();
    paren_depth_--;

    return std::make_unique<AstNodeCall>(pos, std::move(callee), std::move(args), std::move(kwargs), paren_pos);
}

AstNodePtr Parser::finish_index(AstNodePtr obj, const Position pos, const Position bracket_pos) {
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
              Position{peek().row, peek().col});

    advance();
    paren_depth_--;

    return std::make_unique<AstNodeIndex>(pos, std::move(obj), std::move(args), bracket_pos);
}

std::vector<AstNodeFunc::OneCapture> Parser::finish_func_captures() {
    // '[' 已消耗
    paren_depth_++;
    skip_newline();

    std::vector<AstNodeFunc::OneCapture> captures;

    // 空捕获列表 []
    if (check(TokenType::SIGN_RBRACKET)) {
        advance(); // 消耗 ']'
        paren_depth_--;
        return captures;
    }

    captures.push_back(parse_func_capture());
    skip_newline();

    while (check(TokenType::SIGN_COMMA)) {
        advance(); // 消耗 ','
        skip_newline();
        if (check(TokenType::SIGN_RBRACKET)) break; // 尾逗号
        captures.push_back(parse_func_capture());
        skip_newline();
    }

    if (!check(TokenType::SIGN_RBRACKET)) error("expected ']' to close capture list");
    advance(); // 消耗 ']'
    paren_depth_--;

    return captures;
}

AstNodeFunc::OneCapture Parser::parse_func_capture() {
    AstNodeFunc::OneCapture c{};

    skip_newline();

    // &identifier（引用捕获）
    if (check(TokenType::SIGN_AMPERSAND)) {
        advance(); // 消耗 '&'
        skip_newline();
        c.capture_type_ = AstNodeFunc::OneCapture::CaptureType::Reference;
        c.identifier_ = expect(TokenType::IDENTIFIER).lexeme;
        return c;
    }

    // identifier ⟦= expr⟧（值捕获）
    c.capture_type_ = AstNodeFunc::OneCapture::CaptureType::Value;
    c.identifier_ = expect(TokenType::IDENTIFIER).lexeme;
    skip_newline();

    if (check(TokenType::SIGN_ASSIGN)) {
        advance(); // 消耗 '='
        skip_newline();
        c.value_expr_ = parse_expr();
    }

    return c;
}

std::vector<AstNodePtr> Parser::finish_class_bases() {
    // '(' 已消耗，paren_depth_ 已自增
    skip_newline();

    std::vector<AstNodePtr> bases;

    // 空基类列表 ()
    if (check(TokenType::SIGN_RPAREN)) {
        advance(); // 消耗 ')'
        paren_depth_--;
        return bases;
    }

    bases.push_back(parse_expr());
    skip_newline();

    while (check(TokenType::SIGN_COMMA)) {
        advance(); // 消耗 ','
        skip_newline();
        if (check(TokenType::SIGN_RPAREN)) break; // 尾逗号
        bases.push_back(parse_expr());
        skip_newline();
    }

    if (!check(TokenType::SIGN_RPAREN)) error("expected ')' to close base class list");
    advance(); // 消耗 ')'
    paren_depth_--;

    return bases;
}

bool Parser::at_kwarg() const {
    if (!check(TokenType::IDENTIFIER)) return false;
    // 跳过 IDENTIFIER 之后可能的 NEWLINE，看是否为 '='
    size_t i = pos_ + 1;
    while (i < tokens_.size() && tokens_[i].type == TokenType::NEWLINE) ++i;
    return i < tokens_.size() && tokens_[i].type == TokenType::SIGN_ASSIGN;
}

AstNodeFunc::OneParam Parser::parse_func_param() {
    AstNodeFunc::OneParam p{};

    skip_newline();

    // **kwargs
    if (check(TokenType::SIGN_DOUBLESTAR)) {
        advance(); // 消耗 '**'
        skip_newline();
        p.param_type_ = AstNodeFunc::OneParam::ParamType::DoubleStarKwargs;
        p.identifier_ = expect(TokenType::IDENTIFIER).lexeme;
    }
    // *args
    else if (check(TokenType::SIGN_STAR)) {
        advance(); // 消耗 '*'
        skip_newline();
        p.param_type_ = AstNodeFunc::OneParam::ParamType::StarArgs;
        p.identifier_ = expect(TokenType::IDENTIFIER).lexeme;
    }
    // arg
    else {
        p.param_type_ = AstNodeFunc::OneParam::ParamType::Normal;
        p.identifier_ = expect(TokenType::IDENTIFIER).lexeme;
    }

    skip_newline();

    // 类型注解
    if (check(TokenType::SIGN_COLON)) {
        advance(); // 消耗 ':'
        skip_newline();
        p.type_annotation_ = parse_expr_pratt(11); // 停在 '=' 之前（lbp=10 < 11）
        skip_newline();
    }

    // 默认值
    if (check(TokenType::SIGN_ASSIGN)) {
        advance(); // 消耗 '='
        skip_newline();
        p.default_value_ = parse_expr();
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

    // 3. 前边不能有 END_OF_FILE
    const auto it{std::find_if(tokens_.begin(), tokens_.end() - 1,
                               [](const Token &t) { return t.type == TokenType::END_OF_FILE; })};
    if (it != tokens_.end() - 1) throw std::runtime_error("Bad tokens: multiple END_OF_FILE.");
}

AstNodeProgramPtr Parser::parse() && {
    const Position start_pos{peek().row, peek().col};

    // 解析一个若干个表达式
    std::vector exprs{parse_exprs()};

    // 必须是解析完了，否则肯定是语法错误
    expect(TokenType::END_OF_FILE);

    return std::make_unique<AstNodeProgram>(start_pos, std::move(exprs));
}
