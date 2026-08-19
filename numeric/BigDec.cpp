#include "BigDec.h"

#include <algorithm>
#include <cassert>
#include <stdexcept>

namespace {

// 10^k。调用方保证 k >= 0
BigInt pow10(const int64_t k) {
    assert(k >= 0);
    return BigInt(10).pow(BigInt(k));
}

bool is_ascii_digit(const char c) { return c >= '0' && c <= '9'; }

// ASCII 大写化，只用来比对 Inf/Infinity/NaN/sNaN 这几个固定名字
std::string ascii_upper(const std::string &s) {
    std::string result{s};
    for (char &c : result)
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    return result;
}

// 加法对齐用的工作表示：符号 + 非负系数 + 指数
struct WorkRep {
    bool sign{false};
    BigInt coeff;
    int64_t exp{0};
};

// 把两个操作数调成同一个指数，好逐位相加。指数小的那个若小到"再怎么加也影响不了舍入后的结果"，
// 就换成一个 1 × 10^exp 的粘滞值——否则为了对齐，另一个可能要乘上 10^(几百万)。
// 调用方保证两边都是有限非零数
std::pair<WorkRep, WorkRep> align_for_add(const BigDec &a, const BigDec &b, const int64_t prec) {
    WorkRep op1{a.is_negative(), a.coefficient(), a.exponent()};
    WorkRep op2{b.is_negative(), b.coefficient(), b.exponent()};
    // tmp 是指数大的那个（有效数字更靠左），要被放大到跟 other 同一个指数
    WorkRep &tmp{op1.exp < op2.exp ? op2 : op1};
    WorkRep &other{op1.exp < op2.exp ? op1 : op2};

    const int64_t tmp_len{static_cast<int64_t>(tmp.coeff.num_decimal_digits())};
    const int64_t other_len{static_cast<int64_t>(other.coeff.num_decimal_digits())};
    // 给 tmp 加上 10^sticky_exp、和加上任何比它更小的正数，舍入之后的结果一样（减法同理）；
    // other 比它还小就直接换成这个粘滞值
    const int64_t sticky_exp{tmp.exp + std::min<int64_t>(-1, tmp_len - prec - 2)};
    if (other_len + other.exp - 1 < sticky_exp) {
        other.coeff = BigInt(1);
        other.exp = sticky_exp;
    }
    tmp.coeff = tmp.coeff * pow10(tmp.exp - other.exp);
    tmp.exp = other.exp;
    return {op1, op2};
}

} // namespace

void BigDec::check_invariant() const {
    assert(!coeff_.is_negative()); // 系数恒非负，符号单独存在 sign_ 里
    // 特殊值的系数/指数不参与运算，一律归一成 0，这样 identical() 只比字段就够了
    assert(kind_ == Kind::Finite || (coeff_.is_zero() && exp_ == 0));
}

BigDec BigDec::make_finite(const bool sign, BigInt coeff, const int64_t exp) {
    BigDec result;
    result.kind_ = Kind::Finite;
    result.sign_ = sign;
    result.coeff_ = std::move(coeff);
    result.exp_ = exp;
    result.check_invariant();
    return result;
}

BigDec BigDec::from_bigint(const BigInt &value) {
    return make_finite(value.is_negative(), value.abs(), 0);
}

BigDec BigDec::from_parts(const bool sign, BigInt coeff, const int64_t exp) {
    if (coeff.is_negative())
        throw std::invalid_argument("BigDec::from_parts: negative coefficient");
    if (exp > kMaxExponent || exp < -kMaxExponent)
        throw std::invalid_argument("BigDec::from_parts: exponent out of range");
    return make_finite(sign, std::move(coeff), exp);
}

BigDec BigDec::infinity(const bool sign) {
    BigDec result;
    result.kind_ = Kind::Infinity;
    result.sign_ = sign;
    return result;
}

BigDec BigDec::quiet_nan(const bool sign) {
    BigDec result;
    result.kind_ = Kind::NaN;
    result.sign_ = sign;
    return result;
}

BigDec BigDec::signaling_nan(const bool sign) {
    BigDec result;
    result.kind_ = Kind::SignalingNaN;
    result.sign_ = sign;
    return result;
}

std::optional<BigDec> BigDec::try_from_string(const std::string &s) {
    size_t i{0};
    bool sign{false};
    if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
        sign = s[i] == '-';
        ++i;
    }

    // 特殊值：符号之后剩下的部分必须整个就是这几个名字之一
    if (const size_t rest_len{s.size() - i}; rest_len >= 3 && rest_len <= 8) {
        const std::string name{ascii_upper(s.substr(i))};
        if (name == "INF" || name == "INFINITY") return infinity(sign);
        if (name == "NAN") return quiet_nan(sign);
        if (name == "SNAN") return signaling_nan(sign);
    }

    const size_t int_begin{i};
    while (i < s.size() && is_ascii_digit(s[i])) ++i;
    const size_t int_end{i};

    size_t frac_begin{i};
    size_t frac_end{i};
    if (i < s.size() && s[i] == '.') {
        ++i;
        frac_begin = i;
        while (i < s.size() && is_ascii_digit(s[i])) ++i;
        frac_end = i;
    }
    // 整数部分和小数部分至少得有一位数字："."、""、"E5" 都不合法
    if (int_begin == int_end && frac_begin == frac_end) return std::nullopt;

    int64_t exp{0};
    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
        ++i;
        bool exp_negative{false};
        if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
            exp_negative = s[i] == '-';
            ++i;
        }
        const size_t exp_begin{i};
        while (i < s.size() && is_ascii_digit(s[i])) ++i;
        if (i == exp_begin) return std::nullopt;
        // 先跳过前导 0，再按剩下的位数挡掉大到会撑爆 int64_t 的输入（19 位就可能溢出）
        size_t digits_begin{exp_begin};
        while (digits_begin + 1 < i && s[digits_begin] == '0') ++digits_begin;
        if (i - digits_begin > 18) return std::nullopt;
        for (size_t k{digits_begin}; k < i; ++k) exp = exp * 10 + (s[k] - '0');
        if (exp_negative) exp = -exp;
    }
    if (i != s.size()) return std::nullopt; // 尾部还剩别的字符

    // 系数就是整数部分和小数部分的数字直接拼起来，前导 0 由 BigInt 自然丢掉；小数部分有几位，
    // 指数就往下挪几位（末尾零因此得以保留：1.50 是 150 × 10^-2）
    std::string digits{s.substr(int_begin, int_end - int_begin)};
    digits += s.substr(frac_begin, frac_end - frac_begin);
    exp -= static_cast<int64_t>(frac_end - frac_begin);
    if (exp > kMaxExponent || exp < -kMaxExponent) return std::nullopt;
    return make_finite(sign, BigInt::from_decimal_string(digits), exp);
}

BigDec BigDec::from_string(const std::string &s, DecContext &ctx) {
    if (std::optional<BigDec> parsed{try_from_string(s)}) return *std::move(parsed);
    return raise_invalid(ctx, DecCondition::ConversionSyntax);
}

size_t BigDec::digit_count() const { return coeff_.num_decimal_digits(); }

int64_t BigDec::adjusted() const {
    assert(is_finite());
    return exp_ + static_cast<int64_t>(digit_count()) - 1;
}

std::string BigDec::to_string() const {
    const std::string sign_str{sign_ ? "-" : ""};
    switch (kind_) {
    case Kind::Infinity:
        return sign_str + "Infinity";
    case Kind::NaN:
        return sign_str + "NaN";
    case Kind::SignalingNaN:
        return sign_str + "sNaN";
    case Kind::Finite:
        break;
    }

    const std::string digits{coeff_.to_decimal_string()}; // 恒非负、无多余前导 0
    const int64_t digit_len{static_cast<int64_t>(digits.size())};
    // 小数点落在系数的第几位之后（可以 <= 0 或者 >= 位数，下面分三种情形各自补零）
    const int64_t left_digits{exp_ + digit_len};
    // 指数为正、或者调整后的指数小于 -6，就改用科学计数法（小数点固定放在第一位之后）
    const int64_t dot_place{exp_ <= 0 && left_digits > -6 ? left_digits : 1};

    std::string int_part;
    std::string frac_part;
    if (dot_place <= 0) {
        int_part = "0";
        frac_part = "." + std::string(static_cast<size_t>(-dot_place), '0') + digits;
    } else if (dot_place >= digit_len) {
        int_part = digits + std::string(static_cast<size_t>(dot_place - digit_len), '0');
    } else {
        int_part = digits.substr(0, static_cast<size_t>(dot_place));
        frac_part = "." + digits.substr(static_cast<size_t>(dot_place));
    }

    std::string exp_part;
    if (left_digits != dot_place) {
        const int64_t e{left_digits - dot_place};
        exp_part = "E" + std::string(e < 0 ? "-" : "+") + std::to_string(e < 0 ? -e : e);
    }
    return sign_str + int_part + frac_part + exp_part;
}

bool BigDec::identical(const BigDec &rhs) const {
    return kind_ == rhs.kind_ && sign_ == rhs.sign_ && exp_ == rhs.exp_ && coeff_ == rhs.coeff_;
}

BigDec BigDec::copy_negate() const {
    BigDec result{*this};
    result.sign_ = !sign_;
    return result;
}

BigDec BigDec::copy_abs() const {
    BigDec result{*this};
    result.sign_ = false;
    return result;
}

std::pair<BigInt, int> BigDec::split_and_decide(
    const BigInt &coeff, const size_t keep, const bool sign, const DecRounding rounding
) {
    const size_t digits{coeff.num_decimal_digits()};
    assert(keep < digits);

    // dropped >= 1，因此 10^(dropped-1) 恒有意义
    const int64_t dropped{static_cast<int64_t>(digits - keep)};
    const BigInt unit{pow10(dropped - 1)};     // 被丢掉的那段里最高位的权重
    const BigInt scale{unit * BigInt(10)};     // 10^dropped
    const BigInt high{coeff.floor_div(scale)}; // 留下的部分
    const BigInt low{coeff - high * scale};    // 丢掉的部分
    const BigInt half{unit * BigInt(5)};       // "恰好一半"

    const bool low_zero{low.is_zero()};
    const bool at_least_half{!(low < half)}; // 被丢掉的最高位数字 >= 5
    const bool exact_half{low == half};
    // 留下的部分的末位数字：是不是偶数（HalfEven 用）、是不是 0 或 5（ZeroFiveUp 用）。keep 为 0
    // 时 high 是 0，两个判断都成立，正好对应规范里"没有前一位可看"时规定的取值
    const bool high_even{!high.is_odd()};
    const bool high_is_0_or_5{high.mod(BigInt(5)).is_zero()};

    const int down{low_zero ? 0 : -1}; // 截断
    const int up{low_zero ? 0 : 1};    // 进位

    // 八种舍入方式各自一行，横着读就是完整的规则表
    // clang-format off
    int decision{0};
    switch (rounding) {
    case DecRounding::Down:       decision = down;                                          break;
    case DecRounding::Up:         decision = up;                                            break;
    case DecRounding::HalfUp:     decision = at_least_half ? 1 : down;                      break;
    case DecRounding::HalfDown:   decision = exact_half ? -1 : (at_least_half ? 1 : down);  break;
    case DecRounding::HalfEven:   decision = exact_half && high_even
                                                 ? -1 : (at_least_half ? 1 : down);         break;
    case DecRounding::Ceiling:    decision = sign ? down : up;                              break;
    case DecRounding::Floor:      decision = sign ? up : down;                              break;
    case DecRounding::ZeroFiveUp: decision = high_is_0_or_5 ? up : down;                    break;
    }
    // clang-format on
    return {high, decision};
}

BigDec BigDec::rescale(const int64_t exp, const DecRounding rounding) const {
    assert(is_finite());
    if (coeff_.is_zero()) return make_finite(sign_, BigInt(0), exp);
    if (exp_ >= exp) return make_finite(sign_, coeff_ * pow10(exp_ - exp), exp); // 补零，精确

    int64_t keep{static_cast<int64_t>(digit_count()) + exp_ - exp};
    BigInt source{coeff_};
    if (keep < 0) {
        // 整个值比 10^(exp-1) 还小，先换成 1 × 10^(exp-1) 再舍——只要保住"非零"这个信息，
        // 具体小到什么程度不影响结果
        source = BigInt(1);
        keep = 0;
    }
    auto [kept, decision]{split_and_decide(source, static_cast<size_t>(keep), sign_, rounding)};
    if (decision > 0) kept = kept + BigInt(1);
    return make_finite(sign_, std::move(kept), exp);
}

BigDec BigDec::raise_overflow(DecContext &ctx, const bool sign) {
    ctx.raise(DecCondition::Overflow);
    // 没设陷阱：就近舍入的几种一律给 ±Infinity，带方向的几种看方向跟符号合不合
    switch (ctx.rounding()) {
    case DecRounding::HalfUp:
    case DecRounding::HalfDown:
    case DecRounding::HalfEven:
    case DecRounding::Up:
        return infinity(sign);
    case DecRounding::Ceiling:
        if (!sign) return infinity(sign);
        break;
    case DecRounding::Floor:
        if (sign) return infinity(sign);
        break;
    case DecRounding::Down:
    case DecRounding::ZeroFiveUp:
        break;
    }
    return make_finite(sign, pow10(ctx.prec()) - BigInt(1), ctx.etop()); // prec 个 9
}

BigDec BigDec::raise_invalid(DecContext &ctx, const DecCondition condition) {
    ctx.raise(condition);
    return quiet_nan();
}

std::optional<BigDec> BigDec::check_nans(const BigDec &a, DecContext &ctx) {
    if (a.is_signaling_nan()) {
        ctx.raise(DecCondition::InvalidOperation);
        return quiet_nan(a.sign_); // sNaN 报过之后退化成同号的安静 NaN
    }
    if (a.is_nan()) return a;
    return std::nullopt;
}

std::optional<BigDec> BigDec::check_nans(const BigDec &a, const BigDec &b, DecContext &ctx) {
    // sNaN 优先于安静 NaN，左操作数优先于右操作数
    if (a.is_signaling_nan() || b.is_signaling_nan()) {
        const BigDec &which{a.is_signaling_nan() ? a : b};
        ctx.raise(DecCondition::InvalidOperation);
        return quiet_nan(which.sign_);
    }
    if (a.is_nan()) return a;
    if (b.is_nan()) return b;
    return std::nullopt;
}

BigDec BigDec::fix(DecContext &ctx) const {
    if (!is_finite()) return *this; // Inf/NaN 原样返回（我们的 NaN 没有诊断信息，无需截断）

    const int64_t prec{ctx.prec()};
    const int64_t etiny{ctx.etiny()};
    const int64_t etop{ctx.etop()};

    if (coeff_.is_zero()) {
        // 零只需要把指数夹回 [Etiny, Emax]
        const int64_t new_exp{std::clamp(exp_, etiny, static_cast<int64_t>(ctx.emax()))};
        if (new_exp == exp_) return *this;
        ctx.raise(DecCondition::Clamped);
        return make_finite(sign_, BigInt(0), new_exp);
    }

    const int64_t digits{static_cast<int64_t>(digit_count())};
    // 结果允许的最小指数：再小就说明有效数字超过了 prec 位
    int64_t exp_min{digits + exp_ - prec};
    if (exp_min > etop) {
        // 等价于 adjusted() > Emax
        const BigDec ans{raise_overflow(ctx, sign_)};
        ctx.raise(DecCondition::Inexact);
        ctx.raise(DecCondition::Rounded);
        return ans;
    }

    const bool subnormal{exp_min < etiny};
    if (subnormal) exp_min = etiny;

    if (exp_ < exp_min) {
        int64_t keep{digits + exp_ - exp_min};
        BigInt source{coeff_};
        if (keep < 0) {
            // 同 rescale：整个值比最小可表示的那一位还小，换成 1 再舍，指数由 exp_min 给出
            source = BigInt(1);
            keep = 0;
        }
        auto [kept, decision]{
            split_and_decide(source, static_cast<size_t>(keep), sign_, ctx.rounding())
        };
        if (decision > 0) {
            kept = kept + BigInt(1);
            // 进位可能让位数多出一位（999 -> 1000），这时砍掉末位、指数加一
            if (static_cast<int64_t>(kept.num_decimal_digits()) > prec) {
                kept = kept.floor_div(BigInt(10));
                ++exp_min;
            }
        }
        const BigDec ans{
            exp_min > etop ? raise_overflow(ctx, sign_)
                           : make_finite(sign_, std::move(kept), exp_min)
        };

        // 信号的先后顺序按规范来，别调换：陷阱开着的时候，先抛出来的那个才是用户看到的
        if (decision != 0 && subnormal) ctx.raise(DecCondition::Underflow);
        if (subnormal) ctx.raise(DecCondition::Subnormal);
        if (decision != 0) ctx.raise(DecCondition::Inexact);
        ctx.raise(DecCondition::Rounded);
        if (ans.is_zero()) ctx.raise(DecCondition::Clamped); // 下溢到 0
        return ans;
    }

    if (subnormal) ctx.raise(DecCondition::Subnormal);
    return *this; // 本来就表示得下，原样返回
}

BigDec BigDec::plus(DecContext &ctx) const {
    if (const std::optional<BigDec> nan{check_nans(*this, ctx)}) return *nan;
    // +(-0) 是 0，只有 ROUND_FLOOR 下才保留负号
    if (is_zero() && ctx.rounding() != DecRounding::Floor) return copy_abs().fix(ctx);
    return fix(ctx);
}

BigDec BigDec::minus(DecContext &ctx) const {
    if (const std::optional<BigDec> nan{check_nans(*this, ctx)}) return *nan;
    // -0 得到的是 0 而不是 -0，同样只有 ROUND_FLOOR 例外
    if (is_zero() && ctx.rounding() != DecRounding::Floor) return copy_abs().fix(ctx);
    return copy_negate().fix(ctx);
}

BigDec BigDec::abs(DecContext &ctx) const {
    if (const std::optional<BigDec> nan{check_nans(*this, ctx)}) return *nan;
    return sign_ ? minus(ctx) : plus(ctx);
}

BigDec BigDec::add(const BigDec &rhs, DecContext &ctx) const {
    if (const std::optional<BigDec> nan{check_nans(*this, rhs, ctx)}) return *nan;
    if (is_infinite()) {
        if (rhs.is_infinite() && sign_ != rhs.sign_) return raise_invalid(ctx); // -INF + INF
        return *this;
    }
    if (rhs.is_infinite()) return rhs;

    const int64_t min_exp{std::min(exp_, rhs.exp_)};
    // ROUND_FLOOR 下，一正一负加出零来时，结果规定为负零
    const bool negative_zero{ctx.rounding() == DecRounding::Floor && sign_ != rhs.sign_};

    if (coeff_.is_zero() && rhs.coeff_.is_zero()) {
        const bool result_sign{negative_zero || (sign_ && rhs.sign_)};
        return make_finite(result_sign, BigInt(0), min_exp).fix(ctx);
    }
    // 一方为零：结果就是另一方，但指数要降到两者较小的那个——只是不必降过"另一方再往下
    // prec+1 位"，那以下的位反正会被舍掉
    if (coeff_.is_zero())
        return rhs.rescale(std::max(min_exp, rhs.exp_ - ctx.prec() - 1), ctx.rounding()).fix(ctx);
    if (rhs.coeff_.is_zero())
        return rescale(std::max(min_exp, exp_ - ctx.prec() - 1), ctx.rounding()).fix(ctx);

    auto [op1, op2]{align_for_add(*this, rhs, ctx.prec())};
    if (op1.sign != op2.sign) {
        if (op1.coeff == op2.coeff) // 大小相等、符号相反
            return make_finite(negative_zero, BigInt(0), min_exp).fix(ctx);
        if (op1.coeff < op2.coeff) std::swap(op1, op2); // 保证 |op1| > |op2|，结果符号随 op1
        return make_finite(op1.sign, op1.coeff - op2.coeff, op1.exp).fix(ctx);
    }
    return make_finite(op1.sign, op1.coeff + op2.coeff, op1.exp).fix(ctx);
}

BigDec BigDec::sub(const BigDec &rhs, DecContext &ctx) const {
    // NaN 判断要赶在取负之前：否则 sNaN 传出去的符号会被翻反
    if (const std::optional<BigDec> nan{check_nans(*this, rhs, ctx)}) return *nan;
    return add(rhs.copy_negate(), ctx);
}

BigDec BigDec::mul(const BigDec &rhs, DecContext &ctx) const {
    if (const std::optional<BigDec> nan{check_nans(*this, rhs, ctx)}) return *nan;
    const bool result_sign{sign_ != rhs.sign_};
    if (is_infinite() || rhs.is_infinite()) {
        if (is_zero() || rhs.is_zero()) return raise_invalid(ctx); // 0 × INF
        return infinity(result_sign);
    }
    return make_finite(result_sign, coeff_ * rhs.coeff_, exp_ + rhs.exp_).fix(ctx);
}

BigDec BigDec::div(const BigDec &rhs, DecContext &ctx) const {
    if (const std::optional<BigDec> nan{check_nans(*this, rhs, ctx)}) return *nan;
    const bool result_sign{sign_ != rhs.sign_};
    if (is_infinite() && rhs.is_infinite()) return raise_invalid(ctx);
    if (is_infinite()) return infinity(result_sign);
    if (rhs.is_infinite()) {
        // 有限数除以无穷是零，指数取最小可表示的那个；这是一次"指数被迫改动"，规范要求报 Clamped
        ctx.raise(DecCondition::Clamped);
        return make_finite(result_sign, BigInt(0), ctx.etiny());
    }
    if (rhs.coeff_.is_zero()) {
        if (coeff_.is_zero()) return raise_invalid(ctx, DecCondition::DivisionUndefined);
        ctx.raise(DecCondition::DivisionByZero);
        return infinity(result_sign);
    }
    if (coeff_.is_zero()) return make_finite(result_sign, BigInt(0), exp_ - rhs.exp_).fix(ctx);

    // 把被除数放大到"商恰好能算出 prec + 1 位"，多出来的那一位保证后面的 fix 定得了舍入方向
    const int64_t shift{
        static_cast<int64_t>(rhs.digit_count()) - static_cast<int64_t>(digit_count()) + ctx.prec() +
        1
    };
    int64_t exp{exp_ - rhs.exp_ - shift};
    BigInt quotient;
    BigInt remainder;
    if (shift >= 0) {
        const BigInt scaled{coeff_ * pow10(shift)};
        quotient = scaled.floor_div(rhs.coeff_);
        remainder = scaled - quotient * rhs.coeff_;
    } else {
        const BigInt divisor{rhs.coeff_ * pow10(-shift)};
        quotient = coeff_.floor_div(divisor);
        remainder = coeff_ - quotient * divisor;
    }

    if (!remainder.is_zero()) {
        // 除不尽：末位恰好是 0 或 5 的商会让后面的舍入误判成"正好一半"，加 1 把它推离分界点
        // （这一位在 prec 之外，加 1 不影响最终结果的前 prec 位）
        if (quotient.mod(BigInt(5)).is_zero()) quotient = quotient + BigInt(1);
    } else {
        // 除得尽：在不丢信息的前提下，把指数尽量往理想指数（被除数指数 - 除数指数）靠
        const int64_t ideal_exp{exp_ - rhs.exp_};
        while (exp < ideal_exp && quotient.mod(BigInt(10)).is_zero()) {
            quotient = quotient.floor_div(BigInt(10));
            ++exp;
        }
    }
    return make_finite(result_sign, std::move(quotient), exp).fix(ctx);
}

std::optional<std::pair<BigInt, BigDec>>
BigDec::trunc_divmod(const BigDec &rhs, const int64_t prec) const {
    assert(is_finite() && !rhs.is_nan() && !rhs.is_zero());

    // 余数的"理想指数"：两个操作数指数里较小的那个（除数为无穷时就是被除数自己的指数）
    const int64_t ideal_exp{rhs.is_infinite() ? exp_ : std::min(exp_, rhs.exp_)};

    if (coeff_.is_zero() || rhs.is_infinite() || adjusted() - rhs.adjusted() <= -2) {
        // |self| 比 |rhs| 至少小一个数量级（或者被除数是 0、除数是无穷），截断商必然是 0。
        // 这里的 rescale 只会补零，不会真舍掉什么
        return std::pair{BigInt(0), rescale(ideal_exp, DecRounding::Down)};
    }
    if (adjusted() - rhs.adjusted() > prec) return std::nullopt; // 商位数必然超过 prec

    // 两边对齐到同一个指数（也就是 ideal_exp）之后，就是两个整数的带余除法
    BigInt a{coeff_};
    BigInt b{rhs.coeff_};
    if (exp_ >= rhs.exp_)
        a = a * pow10(exp_ - rhs.exp_);
    else
        b = b * pow10(rhs.exp_ - exp_);
    const BigInt magnitude{a.floor_div(b)};
    if (static_cast<int64_t>(magnitude.num_decimal_digits()) > prec) return std::nullopt;
    return std::pair{magnitude, make_finite(sign_, a - magnitude * b, ideal_exp)};
}

BigDec BigDec::quotient_to_dec(const BigInt &quotient, const bool sign_if_zero) {
    // 商为 0 时 BigInt 记不住符号，按两个操作数的符号异或补上——同 * 和 / 的规矩
    // （0 / -3 是 -0），别让 // 成为唯一丢掉零符号的那个
    return make_finite(
        quotient.is_zero() ? sign_if_zero : quotient.is_negative(), quotient.abs(), 0
    );
}

bool BigDec::needs_floor_correction(
    const BigDec &dividend, const BigDec &divisor, const BigDec &trunc_remainder
) {
    return !trunc_remainder.is_zero() && dividend.sign_ != divisor.sign_;
}

std::optional<BigInt> BigDec::floor_quotient(
    const BigInt &magnitude, const BigDec &dividend, const BigDec &divisor,
    const BigDec &trunc_remainder, const int64_t prec
) {
    BigInt quotient{dividend.sign_ != divisor.sign_ ? -magnitude : magnitude};
    if (needs_floor_correction(dividend, divisor, trunc_remainder)) quotient = quotient - BigInt(1);
    if (static_cast<int64_t>(quotient.num_decimal_digits()) > prec) return std::nullopt;
    return quotient;
}

BigDec BigDec::floor_div(const BigDec &rhs, DecContext &ctx) const {
    if (const std::optional<BigDec> nan{check_nans(*this, rhs, ctx)}) return *nan;
    if (is_infinite()) {
        if (rhs.is_infinite()) return raise_invalid(ctx);
        return infinity(sign_ != rhs.sign_);
    }
    if (rhs.is_zero()) {
        if (is_zero()) return raise_invalid(ctx, DecCondition::DivisionUndefined);
        ctx.raise(DecCondition::DivisionByZero);
        return infinity(sign_ != rhs.sign_);
    }
    const std::optional<std::pair<BigInt, BigDec>> trunc{trunc_divmod(rhs, ctx.prec())};
    if (!trunc) return raise_invalid(ctx, DecCondition::DivisionImpossible);
    const std::optional<BigInt> quotient{
        floor_quotient(trunc->first, *this, rhs, trunc->second, ctx.prec())
    };
    if (!quotient) return raise_invalid(ctx, DecCondition::DivisionImpossible);
    // 商必然是整数、位数也不超过 prec，不需要再 fix（跟着 fix 只会白报 Inexact/Rounded——
    // 那是余数被舍入才该有的信号，而 // 的结果跟余数无关）
    return quotient_to_dec(*quotient, sign_ != rhs.sign_);
}

BigDec BigDec::mod(const BigDec &rhs, DecContext &ctx) const {
    if (const std::optional<BigDec> nan{check_nans(*this, rhs, ctx)}) return *nan;
    if (is_infinite()) return raise_invalid(ctx); // INF % x 无意义
    if (rhs.is_zero())
        return raise_invalid(
            ctx, is_zero() ? DecCondition::DivisionUndefined : DecCondition::InvalidOperation
        );
    const std::optional<std::pair<BigInt, BigDec>> trunc{trunc_divmod(rhs, ctx.prec())};
    if (!trunc) return raise_invalid(ctx, DecCondition::DivisionImpossible);
    // 商本身用不上，但商算不出来的话 x % y == x - (x // y) * y 就不成立了，这里只取它的成败
    if (!floor_quotient(trunc->first, *this, rhs, trunc->second, ctx.prec()))
        return raise_invalid(ctx, DecCondition::DivisionImpossible);
    if (!needs_floor_correction(*this, rhs, trunc->second)) return trunc->second.fix(ctx);
    // 修正走带上下文的 add，而不是"先精确相加再 fix"：结果一样（add 对齐时的粘滞位就是为了
    // 保证这一点），但精确相加在 |rhs| 远大于 |self| 时会先造出一个几百万位的中间值
    return trunc->second.add(rhs, ctx);
}

std::pair<BigDec, BigDec> BigDec::divmod(const BigDec &rhs, DecContext &ctx) const {
    if (const std::optional<BigDec> nan{check_nans(*this, rhs, ctx)}) return {*nan, *nan};
    if (is_infinite()) {
        if (rhs.is_infinite()) {
            const BigDec ans{raise_invalid(ctx)};
            return {ans, ans};
        }
        const BigDec quotient{infinity(sign_ != rhs.sign_)};
        return {quotient, raise_invalid(ctx)}; // INF % x 无意义
    }
    if (rhs.is_zero()) {
        if (is_zero()) {
            const BigDec ans{raise_invalid(ctx, DecCondition::DivisionUndefined)};
            return {ans, ans};
        }
        ctx.raise(DecCondition::DivisionByZero);
        return {infinity(sign_ != rhs.sign_), raise_invalid(ctx)};
    }
    const std::optional<std::pair<BigInt, BigDec>> trunc{trunc_divmod(rhs, ctx.prec())};
    const std::optional<BigInt> quotient{
        trunc ? floor_quotient(trunc->first, *this, rhs, trunc->second, ctx.prec()) : std::nullopt
    };
    if (!quotient) {
        const BigDec ans{raise_invalid(ctx, DecCondition::DivisionImpossible)};
        return {ans, ans};
    }
    BigDec remainder{
        needs_floor_correction(*this, rhs, trunc->second) ? trunc->second.add(rhs, ctx)
                                                          : trunc->second.fix(ctx)
    };
    return {quotient_to_dec(*quotient, sign_ != rhs.sign_), std::move(remainder)};
}

int BigDec::cmp_no_nan(const BigDec &a, const BigDec &b) {
    assert(!a.is_nan() && !b.is_nan());

    if (a.is_infinite() || b.is_infinite()) {
        const int a_inf{a.is_infinite() ? (a.sign_ ? -1 : 1) : 0};
        const int b_inf{b.is_infinite() ? (b.sign_ ? -1 : 1) : 0};
        if (a_inf != b_inf) return a_inf < b_inf ? -1 : 1;
        return 0; // 同号的两个无穷
    }

    // 零跟符号无关：-0 == 0
    if (a.coeff_.is_zero()) return b.coeff_.is_zero() ? 0 : (b.sign_ ? 1 : -1);
    if (b.coeff_.is_zero()) return a.sign_ ? -1 : 1;

    if (a.sign_ != b.sign_) return a.sign_ ? -1 : 1;

    // 同号且都非零，比量级；符号为负时结论反过来
    const int sign_factor{a.sign_ ? -1 : 1};
    const int64_t a_adjusted{a.adjusted()};
    const int64_t b_adjusted{b.adjusted()};
    if (a_adjusted != b_adjusted) return a_adjusted > b_adjusted ? sign_factor : -sign_factor;

    // 调整后的指数相同，说明两个系数补零对齐之后位数也相同，直接比。补的零数量等于两者位数之差，
    // 不会因为指数相差很大而炸开
    const BigInt a_aligned{a.coeff_ * pow10(std::max<int64_t>(a.exp_ - b.exp_, 0))};
    const BigInt b_aligned{b.coeff_ * pow10(std::max<int64_t>(b.exp_ - a.exp_, 0))};
    if (a_aligned == b_aligned) return 0;
    return a_aligned > b_aligned ? sign_factor : -sign_factor;
}

bool BigDec::equals(const BigDec &rhs, DecContext &ctx) const {
    // 安静 NaN 参与判等不报信号，直接不等；sNaN 才触发 InvalidOperation
    if (is_signaling_nan() || rhs.is_signaling_nan()) {
        ctx.raise(DecCondition::InvalidOperation);
        return false;
    }
    if (is_nan() || rhs.is_nan()) return false;
    return cmp_no_nan(*this, rhs) == 0;
}

std::partial_ordering BigDec::compare_ordering(const BigDec &rhs, DecContext &ctx) const {
    // 序比较里安静 NaN 也要报信号——排序时静默返回"不小于"会得到无声的错误结果
    if (is_nan() || rhs.is_nan()) {
        ctx.raise(DecCondition::InvalidOperation);
        return std::partial_ordering::unordered;
    }
    const int result{cmp_no_nan(*this, rhs)};
    if (result < 0) return std::partial_ordering::less;
    if (result > 0) return std::partial_ordering::greater;
    return std::partial_ordering::equivalent;
}
