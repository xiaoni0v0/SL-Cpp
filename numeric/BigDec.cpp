#include "BigDec.h"

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <tuple>

#include "dec_math.h"

namespace {

using dec_math::pow10;

bool is_ascii_digit(const char c) { return c >= '0' && c <= '9'; }

// ASCII 大写化，只用来比对 Inf/Infinity/NaN/sNaN 这几个固定名字
std::string ascii_upper(const std::string &s) {
    std::string result{s};
    for (char &c : result)
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    return result;
}

// 临时把上下文的舍入方式换掉，析构时还回去。超越函数的最后一步 fix 可能因为陷阱抛异常，
// 不能靠"算完再赋值回去"这种顺序执行来还原
class RoundingGuard {
    DecContext &ctx_;
    DecRounding saved_;

  public:
    RoundingGuard(DecContext &ctx, const DecRounding rounding) : ctx_{ctx}, saved_{ctx.rounding()} {
        ctx.set_rounding(rounding);
    }
    ~RoundingGuard() { ctx_.set_rounding(saved_); }
    RoundingGuard(const RoundingGuard &) = delete;
    RoundingGuard &operator=(const RoundingGuard &) = delete;
};

// 十进制位数，负数不算符号位。取绝对值走 uint64_t：INT64_MIN 直接取负是 UB
int64_t int64_digits(const int64_t value) {
    uint64_t magnitude{
        value < 0 ? static_cast<uint64_t>(-(value + 1)) + 1 : static_cast<uint64_t>(value)
    };
    int64_t digits{1};
    while (magnitude >= 10) {
        magnitude /= 10;
        ++digits;
    }
    return digits;
}

// 近似值算到位了没有：末尾恰好是 5000…0 就说明它正卡在两个可表示值的正中间，
// 这时候任何舍入方式都定不下方向，得回去多算几位。调用方保证 coeff 的位数 > p
bool is_roundable(const BigInt &coeff, const int64_t p) {
    const int64_t digits{static_cast<int64_t>(coeff.num_decimal_digits())};
    assert(digits - p - 1 >= 0);
    return !coeff.mod(BigInt(5) * pow10(digits - p - 1)).is_zero();
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
    const bool at_least_half{low >= half}; // 被丢掉的最高位数字 >= 5
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

BigDec BigDec::pad_to_exponent(const int64_t exp) const {
    assert(is_finite());
    assert(exp <= exp_);
    if (coeff_.is_zero()) return make_finite(sign_, BigInt(0), exp);
    return make_finite(sign_, coeff_ * pow10(exp_ - exp), exp);
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
            // 整个值比最小可表示的那一位还小，换成 1 再舍，指数由 exp_min 给出
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
        return rhs.pad_to_exponent(std::max(min_exp, rhs.exp_ - ctx.prec() - 1)).fix(ctx);
    if (rhs.coeff_.is_zero())
        return pad_to_exponent(std::max(min_exp, exp_ - ctx.prec() - 1)).fix(ctx);

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
        // ideal_exp 恒不大于自己的指数，所以这里只是补零，不会真舍掉什么
        return std::pair{BigInt(0), pad_to_exponent(ideal_exp)};
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
    // 商必然是整数、位数也不超过 prec，所以这一步 fix 报不出 Inexact/Rounded（进不了舍入分支），
    // 但**不能**因此跳过：位数够不代表指数域也够，商的调整后指数超过 Emax 时得报 Overflow
    // （Emax 小于 prec - 1 的上下文虽然罕见，但设得出来）
    return quotient_to_dec(*quotient, sign_ != rhs.sign_).fix(ctx);
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
    // 商在前、余数在后：陷阱开着时，先算的那个的信号才是用户看到的异常
    BigDec quotient_dec{quotient_to_dec(*quotient, sign_ != rhs.sign_).fix(ctx)};
    BigDec remainder{
        needs_floor_correction(*this, rhs, trunc->second) ? trunc->second.add(rhs, ctx)
                                                          : trunc->second.fix(ctx)
    };
    return {std::move(quotient_dec), std::move(remainder)};
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

bool BigDec::is_integral() const {
    if (!is_finite()) return false;
    if (exp_ >= 0 || coeff_.is_zero()) return true;
    const int64_t drop{-exp_};
    // 小数部分比整个系数还长，那整数部分只能是 0，而系数非零，必然不是整数
    if (drop >= static_cast<int64_t>(digit_count())) return false;
    return coeff_.mod(pow10(drop)).is_zero();
}

int64_t BigDec::ln_exp_bound() const {
    // 0.1 <= x <= 10 时用不等式 1-1/x <= ln(x) <= x-1 卡；出了这个范围光看指数就够了
    const int64_t adj{adjusted()};
    if (adj >= 1) return int64_digits(adj * 23 / 10) - 1; // 23/10 是 ln(10) 的下界
    if (adj <= -2) return int64_digits((-1 - adj) * 23 / 10) - 1;

    // 下面两支里 exp_ 必然 <= 0，10^-exp_ 的规模跟系数本身相当，不会炸开
    if (adj == 0) { // 1 < self < 10
        const std::string num{(coeff_ - pow10(-exp_)).to_decimal_string()};
        const std::string den{coeff_.to_decimal_string()};
        return static_cast<int64_t>(num.size()) - static_cast<int64_t>(den.size()) -
               (num < den ? 1 : 0);
    }
    // adj == -1，也就是 0.1 <= self < 1
    return exp_ + static_cast<int64_t>((pow10(-exp_) - coeff_).to_decimal_string().size()) - 1;
}

int64_t BigDec::log10_exp_bound() const {
    const int64_t adj{adjusted()};
    if (adj >= 1) return int64_digits(adj) - 1;
    if (adj <= -2) return int64_digits(-1 - adj) - 1;

    if (adj == 0) { // 1 < self < 10；2.31 是 1/log10(e) 的上界
        const std::string num{(coeff_ - pow10(-exp_)).to_decimal_string()};
        const std::string den{(coeff_ * BigInt(231)).to_decimal_string()};
        return static_cast<int64_t>(num.size()) - static_cast<int64_t>(den.size()) -
               (num < den ? 1 : 0) + 2;
    }
    const std::string num{(pow10(-exp_) - coeff_).to_decimal_string()};
    return static_cast<int64_t>(num.size()) + exp_ - (num < "231" ? 1 : 0) - 1;
}

BigDec BigDec::sqrt(DecContext &ctx) const {
    if (const std::optional<BigDec> nan{check_nans(*this, ctx)}) return *nan;
    if (is_infinite() && !sign_) return *this;
    // sqrt(-0) 是 -0，符号留着
    if (is_zero()) return make_finite(sign_, BigInt(0), exp_ >> 1).fix(ctx);
    if (sign_) return raise_invalid(ctx); // 负数（-Infinity 也走这里）

    // 把 self 写成 c*100^e：e 取 exp_/2（理想指数），c 调到恰好 prec+1 个百进制位。
    // 多算一位是为了让最后那次 fix 无论什么舍入方式都能给出正确结果
    const int64_t prec{ctx.prec() + 1};
    int64_t e{exp_ >> 1};
    BigInt c{coeff_};
    int64_t l{0};
    if ((exp_ & 1) != 0) { // 指数是奇数，把系数乘 10 换成偶指数
        c = c * BigInt(10);
        l = static_cast<int64_t>(digit_count() >> 1) + 1;
    } else {
        l = static_cast<int64_t>((digit_count() + 1) >> 1);
    }

    const int64_t shift{prec - l};
    bool exact{true};
    if (shift >= 0) {
        c = c * pow10(2 * shift);
    } else {
        const BigInt scale{pow10(-2 * shift)};
        const BigInt quotient{c.floor_div(scale)};
        exact = (c - quotient * scale).is_zero();
        c = quotient;
    }
    e -= shift;

    // 牛顿迭代求 floor(sqrt(c))
    BigInt n{pow10(prec)};
    while (true) {
        const BigInt q{c.floor_div(n)};
        if (n <= q) break;
        n = (n + q) >> 1;
    }
    exact = exact && n * n == c;

    if (exact) {
        // 精确，把指数还原到理想指数
        if (shift >= 0)
            n = n.floor_div(pow10(shift));
        else
            n = n * pow10(-shift);
        e += shift;
    } else if (n.mod(BigInt(5)).is_zero()) {
        // 不精确、末位又恰好是 0 或 5：抬到 1 或 6，免得下一步舍入把它当成"正好一半"
        n = n + BigInt(1);
    }

    const RoundingGuard guard{ctx, DecRounding::HalfEven};
    return make_finite(false, std::move(n), e).fix(ctx);
}

BigDec BigDec::exp(DecContext &ctx) const {
    if (const std::optional<BigDec> nan{check_nans(*this, ctx)}) return *nan;
    if (is_infinite()) return sign_ ? make_finite(false, BigInt(0), 0) : *this;
    if (is_zero()) return make_finite(false, BigInt(1), 0);

    // 结果必然是无理数，也就必然不精确；Inexact/Rounded 由最后那次 fix 统一报，这里不用管
    const int64_t p{ctx.prec()};
    const int64_t adj{adjusted()};

    BigDec ans;
    if (!sign_ && adj > int64_digits((static_cast<int64_t>(ctx.emax()) + 1) * 3)) {
        ans = make_finite(false, BigInt(1), static_cast<int64_t>(ctx.emax()) + 1); // 必然溢出
    } else if (sign_ && adj > int64_digits((-ctx.etiny() + 1) * 3)) {
        ans = make_finite(false, BigInt(1), ctx.etiny() - 1); // 必然下溢成 0
    } else if (!sign_ && adj < -p) {
        // 结果跟 1 已经分不出来了，给 1.00…01（p+1 位），让 fix 去报正确的信号
        ans = make_finite(false, pow10(p) + BigInt(1), -p);
    } else if (sign_ && adj < -p - 1) {
        // 同上，负指数那侧贴着 1 的下方，给 0.99…9
        ans = make_finite(false, pow10(p + 1) - BigInt(1), -p - 1);
    } else {
        const BigInt c{sign_ ? -coeff_ : coeff_};
        // 每次多算三位，直到结果不再卡在两个可表示值正中间
        int64_t extra{3};
        BigInt coeff;
        int64_t exp{0};
        while (true) {
            std::tie(coeff, exp) = dec_math::dexp(c, exp_, p + extra);
            if (is_roundable(coeff, p)) break;
            extra += 3;
        }
        ans = make_finite(false, std::move(coeff), exp);
    }

    const RoundingGuard guard{ctx, DecRounding::HalfEven};
    return ans.fix(ctx);
}

BigDec BigDec::ln(DecContext &ctx) const {
    if (const std::optional<BigDec> nan{check_nans(*this, ctx)}) return *nan;
    if (is_zero()) return infinity(true);      // ln(±0) = -Infinity
    if (is_infinite() && !sign_) return *this; // ln(+Infinity) = +Infinity
    if (sign_) return raise_invalid(ctx);      // 负数、-Infinity
    if (cmp_no_nan(*this, from_bigint(BigInt(1))) == 0) return make_finite(false, BigInt(0), 0);

    const int64_t p{ctx.prec()};
    // 至少算到小数点后 p+3 位；不够定夺就每次再多三位
    int64_t places{p - ln_exp_bound() + 2};
    BigInt coeff;
    while (true) {
        coeff = dec_math::dlog(coeff_, exp_, places);
        if (is_roundable(coeff, p)) break;
        places += 3;
    }
    const BigDec ans{make_finite(coeff.is_negative(), coeff.abs(), -places)};

    const RoundingGuard guard{ctx, DecRounding::HalfEven};
    return ans.fix(ctx);
}

BigDec BigDec::log10(DecContext &ctx) const {
    if (const std::optional<BigDec> nan{check_nans(*this, ctx)}) return *nan;
    if (is_zero()) return infinity(true);
    if (is_infinite() && !sign_) return *this;
    if (sign_) return raise_invalid(ctx);

    BigDec ans;
    if (coeff_ == pow10(static_cast<int64_t>(digit_count()) - 1)) {
        // self 恰好是 10 的整数次幂，答案就是它的调整后指数（还是可能要按 prec 舍入）
        ans = from_bigint(BigInt(adjusted()));
    } else {
        const int64_t p{ctx.prec()};
        int64_t places{p - log10_exp_bound() + 2};
        BigInt coeff;
        while (true) {
            coeff = dec_math::dlog10(coeff_, exp_, places);
            if (is_roundable(coeff, p)) break;
            places += 3;
        }
        ans = make_finite(coeff.is_negative(), coeff.abs(), -places);
    }

    const RoundingGuard guard{ctx, DecRounding::HalfEven};
    return ans.fix(ctx);
}

BigInt BigDec::integer_value() const {
    assert(is_integral());
    if (coeff_.is_zero()) return BigInt(0);
    const BigInt magnitude{exp_ >= 0 ? coeff_ * pow10(exp_) : coeff_.floor_div(pow10(-exp_))};
    return sign_ ? -magnitude : magnitude;
}

bool BigDec::is_even_integer() const {
    assert(is_integral());
    if (coeff_.is_zero() || exp_ > 0) return true; // 指数为正说明末位必然是 0
    return !coeff_.floor_div(pow10(-exp_)).is_odd();
}

std::optional<BigDec> BigDec::power_exact(const BigDec &other, const int64_t p) const {
    // 把 x = xc*10^xe、|y| = yc*10^ye 都化到系数不再被 10 整除，好判断幂次能不能整除
    BigInt xc{coeff_};
    int64_t xe{exp_};
    while (xc.mod(BigInt(10)).is_zero()) {
        xc = xc.floor_div(BigInt(10));
        ++xe;
    }
    BigInt yc{other.coeff_};
    int64_t ye{other.exp_};
    while (yc.mod(BigInt(10)).is_zero()) {
        yc = yc.floor_div(BigInt(10));
        ++ye;
    }

    // 结果的指数要尽量往"理想指数"靠，也就是补上若干个末尾零。只有指数是非负整数时才有理想指数
    const bool has_ideal_exponent{other.is_integral() && !other.sign_};

    if (xc == BigInt(1)) {
        // x 是 10 的整数次幂，结果就是 10^(xe*y)，前提是 xe*y 恰好是整数
        BigInt exponent{BigInt(xe) * yc};
        while (!exponent.is_zero() && exponent.mod(BigInt(10)).is_zero()) {
            exponent = exponent.floor_div(BigInt(10));
            ++ye;
        }
        if (ye < 0) return std::nullopt; // xe*y 带小数，结果不是 10 的整数次幂
        exponent = exponent * pow10(ye);
        if (other.sign_) exponent = -exponent;

        int64_t zeros{0};
        if (has_ideal_exponent) {
            const BigInt diff{exponent - BigInt(exp_) * other.integer_value()};
            assert(!diff.is_negative()); // 剥掉末尾零只会让指数变大，不会小于理想指数
            zeros = diff > BigInt(p - 1) ? p - 1 : dec_math::to_int64(diff).value();
        }
        const std::optional<int64_t> result_exp{dec_math::to_int64(exponent - BigInt(zeros))};
        if (!result_exp) return std::nullopt;
        return make_finite(false, pow10(zeros), *result_exp);
    }

    if (other.sign_) {
        // y < 0：结果是 1/(x^|y|)，要是有限小数，xc 只能是 2 的幂或者 5 的幂
        const BigInt last_digit{xc.mod(BigInt(10))};
        BigInt scaled_e;  // -e*y
        BigInt scaled_xe; // -xe*y
        if (last_digit == BigInt(2) || last_digit == BigInt(4) || last_digit == BigInt(6) ||
            last_digit == BigInt(8)) {
            if ((xc & -xc) != xc) return std::nullopt; // 不是 2 的幂
            const int64_t e{static_cast<int64_t>(xc.bit_length()) - 1};
            // x = 2^e * 10^xe 时结果是 5^(-e*y) * 10^(e*y + xe*y)。
            // 5^emax < 10^p 的最大 emax，93/65 是 log(10)/log(5) 的上界
            const int64_t emax{p * 93 / 65};
            if (ye >= int64_digits(emax)) return std::nullopt; // |y| 大到系数必然放不下
            const std::optional<BigInt> a{dec_math::decimal_lshift_exact(BigInt(e) * yc, ye)};
            const std::optional<BigInt> b{dec_math::decimal_lshift_exact(BigInt(xe) * yc, ye)};
            if (!a || !b) return std::nullopt; // e*y 或 xe*y 带小数
            if (*a > BigInt(emax)) return std::nullopt;
            scaled_e = *a;
            scaled_xe = *b;
            xc = BigInt(5).pow(scaled_e);
        } else if (last_digit == BigInt(5)) {
            // 先按位长估一个 e 的上界（28/65 是 log(2)/log(5) 的下界），再看 5^e 能不能被 xc 整除
            int64_t e{static_cast<int64_t>(xc.bit_length()) * 28 / 65};
            BigInt quotient{BigInt(5).pow(BigInt(e))};
            if (!quotient.mod(xc).is_zero()) return std::nullopt; // 不是 5 的幂
            quotient = quotient.floor_div(xc);
            // 把多估的那几次方除回去。28/65 这个估计一直到 5^2658 都不多不少，所以这个
            // 循环在现实的系数上转不起来；留着是因为再往上就估不准了
            while (quotient.mod(BigInt(5)).is_zero()) {
                quotient = quotient.floor_div(BigInt(5));
                --e;
            }
            const int64_t emax{p * 10 / 3}; // 10/3 是 log(10)/log(2) 的上界
            if (ye >= int64_digits(emax)) return std::nullopt;
            const std::optional<BigInt> a{dec_math::decimal_lshift_exact(BigInt(e) * yc, ye)};
            const std::optional<BigInt> b{dec_math::decimal_lshift_exact(BigInt(xe) * yc, ye)};
            if (!a || !b) return std::nullopt;
            if (*a > BigInt(emax)) return std::nullopt;
            scaled_e = *a;
            scaled_xe = *b;
            xc = BigInt(2).pow(scaled_e);
        } else {
            return std::nullopt;
        }
        if (static_cast<int64_t>(xc.num_decimal_digits()) > p) return std::nullopt;
        const std::optional<int64_t> result_exp{dec_math::to_int64(-scaled_e - scaled_xe)};
        if (!result_exp) return std::nullopt;
        return make_finite(false, std::move(xc), *result_exp);
    }

    // y > 0：写成既约分数 m/n。结果要精确，xc 必须是某个正整数的 n 次幂、xe 必须被 n 整除
    BigInt m;
    BigInt n{1};
    int64_t xc_bits{0};
    if (ye >= 0) {
        m = yc * pow10(ye); // y 本身就是整数，n 取 1
    } else {
        // |y| 小到一定程度结果必然不可表示：xe != 0 时要求 |y| >= 1/|xe|，
        // xc != 1 时要求 |y| >= 1/位长（不然 xc^|y| 连 2 都到不了）
        if (xe != 0 && static_cast<int64_t>((yc * BigInt(xe)).num_decimal_digits()) <= -ye)
            return std::nullopt;
        xc_bits = static_cast<int64_t>(xc.bit_length());
        if (static_cast<int64_t>((yc * BigInt(xc_bits)).num_decimal_digits()) <= -ye)
            return std::nullopt;
        m = yc;
        n = pow10(-ye);
        while (m.mod(BigInt(2)).is_zero() && n.mod(BigInt(2)).is_zero()) {
            m = m.floor_div(BigInt(2));
            n = n.floor_div(BigInt(2));
        }
        while (m.mod(BigInt(5)).is_zero() && n.mod(BigInt(5)).is_zero()) {
            m = m.floor_div(BigInt(5));
            n = n.floor_div(BigInt(5));
        }
    }

    if (n > BigInt(1)) {
        // 1 < xc < 2^n 时 xc 不可能是某个整数的 n 次幂
        if (n >= BigInt(xc_bits)) return std::nullopt;
        const int64_t n_small{dec_math::to_int64(n).value()}; // 上一行保证了装得下

        const BigInt divided{BigInt(xe).floor_div(n)};
        if (!(BigInt(xe) - divided * n).is_zero()) return std::nullopt; // xe 不被 n 整除
        xe = dec_math::to_int64(divided).value();

        // 牛顿迭代求 xc 的 n 次方根
        BigInt a{BigInt(1) << ((xc_bits + n_small - 1) / n_small)};
        BigInt q;
        BigInt r;
        while (true) {
            const BigInt power{a.pow(BigInt(n_small - 1))};
            q = xc.floor_div(power);
            r = xc - q * power;
            if (a <= q) break;
            a = (a * BigInt(n_small - 1) + q).floor_div(n);
        }
        if (!(a == q && r.is_zero())) return std::nullopt; // 开不尽
        xc = a;
    }

    // m > p/log10(xc) 时 xc^m >= 10^p，系数放不下（100/log10_lb 是 1/log10(xc) 的上界）
    if (xc > BigInt(1) && m > BigInt(p * 100 / dec_math::log10_lb(xc))) return std::nullopt;
    xc = xc.pow(m);
    const BigInt result_exp_big{BigInt(xe) * m};
    const int64_t digits{static_cast<int64_t>(xc.num_decimal_digits())};
    if (digits > p) return std::nullopt;

    int64_t zeros{0};
    if (has_ideal_exponent) {
        const BigInt diff{result_exp_big - BigInt(exp_) * other.integer_value()};
        assert(!diff.is_negative());
        zeros = diff > BigInt(p - digits) ? p - digits : dec_math::to_int64(diff).value();
    }
    const std::optional<int64_t> result_exp{dec_math::to_int64(result_exp_big - BigInt(zeros))};
    if (!result_exp) return std::nullopt;
    return make_finite(false, xc * pow10(zeros), *result_exp);
}

BigDec BigDec::pow(const BigDec &rhs, DecContext &ctx) const {
    if (const std::optional<BigDec> nan{check_nans(*this, rhs, ctx)}) return *nan;

    // 0 ** 0 无意义；其余 x ** 0 一律是 1
    if (rhs.is_zero()) {
        if (is_zero()) return raise_invalid(ctx);
        return make_finite(false, BigInt(1), 0);
    }

    // 结果为负，当且仅当底数为负且指数是奇整数
    bool result_sign{false};
    BigDec base{*this};
    if (sign_) {
        if (rhs.is_integral()) {
            if (!rhs.is_even_integer()) result_sign = true;
        } else if (!is_zero()) {
            // 负数的非整数次幂不是实数。(-0) ** 非整数 不算，按 0 ** 非整数 处理
            return raise_invalid(ctx);
        }
        base = copy_negate(); // 底数取绝对值，符号已经单独记在 result_sign 里
    }

    // 0 ** 正数 = 0，0 ** 负数 = Infinity
    if (base.is_zero())
        return rhs.sign_ ? infinity(result_sign) : make_finite(result_sign, BigInt(0), 0);
    // Infinity ** 正数 = Infinity，Infinity ** 负数 = 0
    if (base.is_infinite())
        return rhs.sign_ ? make_finite(result_sign, BigInt(0), 0) : infinity(result_sign);

    const int64_t p{ctx.prec()};
    const BigDec one{from_bigint(BigInt(1))};

    // 1 ** y 的值恒是 1，但结果的标度和触发的信号取决于底数自己的指数和 y 长什么样
    if (cmp_no_nan(base, one) == 0) {
        int64_t exp{0};
        if (rhs.is_integral()) {
            // 直接取 int(rhs) 有风险（可能是 1E+999999999），先跟 prec 比一下再说
            const int64_t multiplier{
                rhs.sign_ ? 0
                : cmp_no_nan(rhs, from_bigint(BigInt(p))) > 0
                    ? p
                    : dec_math::to_int64(rhs.integer_value()).value()
            };
            const BigInt exp_big{BigInt(base.exp_) * BigInt(multiplier)};
            if (exp_big < BigInt(1 - p)) {
                exp = 1 - p;
                ctx.raise(DecCondition::Rounded);
            } else {
                exp = dec_math::to_int64(exp_big).value(); // 不小于 1-p，必然装得下
            }
        } else {
            ctx.raise(DecCondition::Inexact);
            ctx.raise(DecCondition::Rounded);
            exp = 1 - p;
        }
        return make_finite(result_sign, pow10(-exp), exp);
    }

    const int64_t base_adj{base.adjusted()};

    // x ** ±Infinity：看 |x| 在 1 的哪一侧
    if (rhs.is_infinite()) {
        if ((!rhs.sign_) == (base_adj < 0)) return make_finite(result_sign, BigInt(0), 0);
        return infinity(result_sign);
    }

    // 先用一个很粗的界筛掉必然溢出/下溢的情形，免得精确路径去算天文数字
    std::optional<BigDec> ans;
    bool exact{false};
    const int64_t bound{base.log10_exp_bound() + rhs.adjusted()};
    if ((base_adj >= 0) == !rhs.sign_) {
        if (bound >= int64_digits(ctx.emax()))
            ans = make_finite(result_sign, BigInt(1), static_cast<int64_t>(ctx.emax()) + 1);
    } else {
        if (bound >= int64_digits(-ctx.etiny()))
            ans = make_finite(result_sign, BigInt(1), ctx.etiny() - 1);
    }

    // 多算一位去试精确解：结果精确的话，后面就不用走 exp(y*log(x)) 那条又慢又只能逼近的路
    if (!ans) {
        ans = base.power_exact(rhs, p + 1);
        if (ans) {
            if (result_sign) ans = ans->copy_negate();
            exact = true;
        }
    }

    if (!ans) {
        // 一般情形：x**y 按 exp(y*log(x)) 算，同样是每次多算三位直到能定夺舍入方向
        const BigInt yc{rhs.sign_ ? -rhs.coeff_ : rhs.coeff_};
        int64_t extra{3};
        BigInt coeff;
        int64_t exp{0};
        while (true) {
            std::tie(coeff, exp) =
                dec_math::dpower(base.coeff_, base.exp_, yc, rhs.exp_, p + extra);
            if (is_roundable(coeff, p)) break;
            extra += 3;
        }
        ans = make_finite(result_sign, std::move(coeff), exp);
    }

    if (!exact || rhs.is_integral()) return ans->fix(ctx);

    // 指数不是整数时，规范要求即使结果精确也报 Inexact（结果落进次正规区还要报 Underflow）。
    // fix 自己不会报，又不能在 fix 前后直接补——那会打乱规范规定的信号优先级。于是先在一个
    // 陷阱全关、标志位清空的副本上 fix，再按优先级顺序把信号补报到真上下文上
    BigDec padded{*ans};
    if (static_cast<int64_t>(padded.digit_count()) <= p) {
        // 补零补到 prec+1 位，保证 Rounded 一定会被触发
        const int64_t pad{p + 1 - static_cast<int64_t>(padded.digit_count())};
        padded = make_finite(padded.sign_, padded.coeff_ * pow10(pad), padded.exp_ - pad);
    }
    DecContext scratch{ctx};
    scratch.traps().clear();
    scratch.flags().clear();
    const BigDec fixed{padded.fix(scratch)};

    scratch.raise(DecCondition::Inexact);
    if (scratch.flags().has(DecCondition::Subnormal)) scratch.raise(DecCondition::Underflow);

    if (scratch.flags().has(DecCondition::Overflow)) ctx.raise(DecCondition::Overflow);
    for (const DecCondition condition :
         {DecCondition::Underflow,
          DecCondition::Subnormal,
          DecCondition::Inexact,
          DecCondition::Rounded,
          DecCondition::Clamped}) {
        if (scratch.flags().has(condition)) ctx.raise(condition);
    }
    return fixed;
}
