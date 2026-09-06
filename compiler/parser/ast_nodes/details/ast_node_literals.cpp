#include "ast_node_literals.h"

#include "../../../../cppexceptions/InternalError.h"
#include "../../../../utils/string_utils.h"

namespace {

[[noreturn]] void error_internal(const std::string &msg, const Position pos) {
    throw InternalError{"<file>", pos.row, pos.col, msg};
}

// raw_ 拆出来的某一段必须是非空的纯十进制数字串，否则以 InternalError 报告。
// raw_ 的形状本该已由构造时校验过，走到这里说明内部逻辑本身有 bug
void require_literal_digits(
    const std::u32string_view digits, const bool no_leading_zero, const std::string_view part,
    const Position pos
) {
    if (digits.empty())
        error_internal(std::format("literal raw text is missing digits in {}", part), pos);
    for (const char32_t c : digits)
        if (!is_digit(c))
            error_internal(
                std::format("literal raw text contains a non-digit character in {}", part), pos
            );
    if (no_leading_zero && digits.size() > 1 && digits[0] == U'0')
        error_internal(std::format("literal raw text has a leading zero in {}", part), pos);
}

} // namespace

AstNodeLiteralInt::AstNodeLiteralInt(const Position pos, std::u32string raw)
    : AstNode{pos}, raw_{std::move(raw)} {
    // 0 <= e <= 9999
    const LiteralExponentSplit split{
        strip_literal_exponent(strip_literal_sign(raw_), false, 4, pos)
    };
    require_literal_digits(split.mantissa, true, "the integer part", pos);
}

AstNodeLiteralDecimal::AstNodeLiteralDecimal(const Position pos, std::u32string raw)
    : AstNode{pos}, raw_{std::move(raw)} {
    // ∀ e
    const LiteralExponentSplit split{
        strip_literal_exponent(strip_literal_sign(raw_), true, 0, pos)
    };
    const size_t dot{split.mantissa.find(U'.')};
    if (dot == std::u32string_view::npos)
        throw InternalError{
            "<file>", pos.row, pos.col, "decimal literal raw text is missing a '.'"
        };
    require_literal_digits(split.mantissa.substr(0, dot), true, "the integer part", pos);
    require_literal_digits(split.mantissa.substr(dot + 1), false, "the fractional part", pos);
}

std::u32string_view strip_literal_sign(const std::u32string_view raw) {
    return !raw.empty() && raw[0] == U'-' ? raw.substr(1) : raw;
}

LiteralExponentSplit strip_literal_exponent(
    const std::u32string_view raw, const bool allow_negative_exponent,
    const size_t max_exponent_digits, const Position pos
) {
    const size_t marker{raw.find_first_of(U"eE")};             // 找 e/E
    if (marker == std::u32string_view::npos) return {raw, {}}; // 没有 e/E

    size_t exponent_begin{marker + 1};
    if (exponent_begin < raw.size() &&
        (raw[exponent_begin] == U'+' || raw[exponent_begin] == U'-')) {
        // 不允许负指数
        if (raw[exponent_begin] == U'-' && !allow_negative_exponent)
            error_internal("literal raw text has a negative exponent", pos);
        ++exponent_begin;
    }

    const std::u32string_view exponent{raw.substr(exponent_begin)};
    require_literal_digits(exponent, true, "the exponent", pos);
    // 指数的位数
    if (max_exponent_digits != 0 && exponent.size() > max_exponent_digits)
        error_internal("literal raw text has an out-of-range exponent", pos);

    return {raw.substr(0, marker), exponent};
}
