#include "ast_node_literals.h"

#include "../../../builtins/exceptions/InternalError.h"
#include "../../../utils/string_utils.h"

#include <format>
#include <string_view>

namespace {

[[noreturn]] void error_internal(const std::string &msg, const Position pos) {
    throw InternalError{"<file>", pos.row, pos.col, msg};
}

/**
 * raw_ 拆出来的某一段必须是非空的纯十进制数字串
 * @param no_leading_zero 是否禁止前导零（单独一个 "0" 除外）
 * @param part            这一段的名字，只用来拼报错信息
 */
void require_digits(
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

// 剥掉开头可能有的负号
[[nodiscard]] std::u32string_view strip_sign(const std::u32string_view raw) {
    return !raw.empty() && raw[0] == U'-' ? raw.substr(1) : raw;
}

/**
 * 校验并剥掉末尾的科学计数法后缀 `[eE][+-]?digits`
 * @param allow_negative_exponent 指数能不能带负号（int 不行，decimal 行）
 * @param max_exponent_digits     指数位数上限，0 表示不限
 * @return 尾数（没有后缀就原样返回）
 */
[[nodiscard]] std::u32string_view strip_exponent(
    const std::u32string_view raw, const bool allow_negative_exponent,
    const size_t max_exponent_digits, const Position pos
) {
    const size_t marker{raw.find_first_of(U"eE")};       // 找 e/E
    if (marker == std::u32string_view::npos) return raw; // 没有 e/E

    size_t exponent_begin{marker + 1};
    if (exponent_begin < raw.size() &&
        (raw[exponent_begin] == U'+' || raw[exponent_begin] == U'-')) {
        // 不允许负指数
        if (raw[exponent_begin] == U'-' && !allow_negative_exponent)
            error_internal("literal raw text has a negative exponent", pos);
        ++exponent_begin;
    }

    const std::u32string_view exponent{raw.substr(exponent_begin)};
    require_digits(exponent, true, "the exponent", pos);
    // 指数的位数
    if (max_exponent_digits != 0 && exponent.size() > max_exponent_digits)
        error_internal("literal raw text has an out-of-range exponent", pos);

    return raw.substr(0, marker);
}

} // namespace

AstNodeLiteralInt::AstNodeLiteralInt(const Position pos, std::u32string raw)
    : AstNode{pos}, raw_{std::move(raw)} {
    // 0 <= e <= 9999
    const std::u32string_view mantissa{strip_exponent(strip_sign(raw_), false, 4, pos)};
    require_digits(mantissa, true, "the integer part", pos);
}

AstNodeLiteralFloat::AstNodeLiteralFloat(const Position pos, std::u32string raw)
    : AstNode{pos}, raw_{std::move(raw)} {
    // ∀ e
    const std::u32string_view mantissa{strip_exponent(strip_sign(raw_), true, 0, pos)};
    const size_t dot{mantissa.find(U'.')};
    if (dot == std::u32string_view::npos)
        error_internal("float literal raw text is missing a '.'", pos);
    require_digits(mantissa.substr(0, dot), true, "the integer part", pos);
    require_digits(mantissa.substr(dot + 1), false, "the fractional part", pos);
}
