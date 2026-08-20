#pragma once

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "BigInt.h"
#include "DecContext.h"

// 十进制浮点数，SL 的 decimal 的底层实现：值为 (-1)^符号 × 系数 × 10^指数，系数是任意精度
// 整数。算术遵循 IBM 通用十进制算术规范（Python decimal 实现的那一份），与规范不一致处逐处
// 注明——目前只有 //、% 向负无穷取整（不是规范里向零截断的 divide-integer/remainder）。
//
// 标度是值的一部分：1.5 和 1.50 数值相等但指数不同，判等不能靠结构相等，一律走
// equals()/compare_ordering()。构造不舍入，运算才舍入。
class BigDec {
  public:
    enum class Kind : uint8_t { Finite, Infinity, NaN, SignalingNaN };

    // 构造出来的有限数，指数必须落在 [-kMaxExponent, kMaxExponent] 内（运算的中间结果可以
    // 暂时超出，fix 之后必然回到 [Etiny, Emax] 之内）。上界取 Emax 上限 + prec 上限，为的是
    // "任何合法上下文下 fix 出来的结果都还构造得回去"——次正规结果的指数会被压到
    // Etiny = Emin - prec + 1，卡在 Emax 的量级上就会产出 try_from_string 读不回来的值
    static constexpr int64_t kMaxExponent{
        static_cast<int64_t>(DecContext::kMaxExp) + DecContext::kMaxPrec
    };

  private:
    Kind kind_{Kind::Finite};
    // 符号独立于 coeff_ 存：coeff_ 恒非负，否则表示不出负零（-0 与 +0 数值相等但 to_string()
    // 不同，且决定后续运算结果的符号）。特殊值也带符号（-Infinity、-NaN）
    bool sign_{false};
    BigInt coeff_;   // 系数，恒非负；特殊值时恒为 0，不参与运算
    int64_t exp_{0}; // 十进制指数；特殊值时恒为 0，不参与运算

  public:
    BigDec() = default; // +0，指数 0

    // 精确转换，指数取 0
    [[nodiscard]] static BigDec from_bigint(const BigInt &value);
    // 三元组构造。coeff 为负、或 |exp| > kMaxExponent 抛 std::invalid_argument
    [[nodiscard]] static BigDec from_parts(bool sign, BigInt coeff, int64_t exp);
    [[nodiscard]] static BigDec infinity(bool sign = false);
    [[nodiscard]] static BigDec quiet_nan(bool sign = false);
    [[nodiscard]] static BigDec signaling_nan(bool sign = false);

    // 按 IBM 规范的数字字符串语法解析（Inf/Infinity/NaN/sNaN 及指数记号不区分大小写），
    // 不合法返回 nullopt。跟 Python 的差异：不收首尾空白、数字里的 '_'、NaN 后的诊断信息
    [[nodiscard]] static std::optional<BigDec> try_from_string(const std::string &s);
    // try_from_string 的上下文版：不合法则触发 ConversionSyntax，没设陷阱时返回安静 NaN
    [[nodiscard]] static BigDec from_string(const std::string &s, DecContext &ctx);

    [[nodiscard]] Kind kind() const { return kind_; }
    [[nodiscard]] bool is_finite() const { return kind_ == Kind::Finite; }
    [[nodiscard]] bool is_infinite() const { return kind_ == Kind::Infinity; }
    [[nodiscard]] bool is_nan() const { return kind_ == Kind::NaN || kind_ == Kind::SignalingNaN; }
    [[nodiscard]] bool is_signaling_nan() const { return kind_ == Kind::SignalingNaN; }
    // 有限且系数为 0（负零也算）
    [[nodiscard]] bool is_zero() const { return kind_ == Kind::Finite && coeff_.is_zero(); }
    // 符号位本身：负零、-Infinity、-NaN 都为 true
    [[nodiscard]] bool is_negative() const { return sign_; }

    [[nodiscard]] const BigInt &coefficient() const { return coeff_; }
    [[nodiscard]] int64_t exponent() const { return exp_; }
    // 系数的十进制位数，系数为 0 时算 1 位
    [[nodiscard]] size_t digit_count() const;
    // 调整后的指数 exp + 位数 - 1（科学计数法 d.dddE±n 里的 n）。调用方保证是有限数
    [[nodiscard]] int64_t adjusted() const;

    // IBM 的 to-scientific-string：完整保留表示（1.5 与 1.50、0 与 -0 都得到不同的串），
    // 因此 try_from_string(x.to_string()) 恒与 x 表示层面完全相同。不查上下文、不触发信号
    [[nodiscard]] std::string to_string() const;

    // 表示层面完全相同（类别、符号、系数、指数都一样）。跟 SL 的 == 不是一回事：
    // SL 里 1.5 == 1.50 为真、NaN == NaN 为假，这里正好都相反
    [[nodiscard]] bool identical(const BigDec &rhs) const;

    // 只翻/清符号位，不舍入、不查上下文（IBM 的 copy-negate / copy-abs）
    [[nodiscard]] BigDec copy_negate() const;
    [[nodiscard]] BigDec copy_abs() const;

    // 一元 +、-、abs。按 IBM 规范这三个都要按上下文舍入，+x 不是恒等操作
    [[nodiscard]] BigDec plus(DecContext &ctx) const;
    [[nodiscard]] BigDec minus(DecContext &ctx) const;
    [[nodiscard]] BigDec abs(DecContext &ctx) const;

    [[nodiscard]] BigDec add(const BigDec &rhs, DecContext &ctx) const;
    [[nodiscard]] BigDec sub(const BigDec &rhs, DecContext &ctx) const;
    [[nodiscard]] BigDec mul(const BigDec &rhs, DecContext &ctx) const;
    [[nodiscard]] BigDec div(const BigDec &rhs, DecContext &ctx) const;

    // SL 的 // 和 %：向负无穷取整（不是 IBM 规范的向零截断），非零余数的符号跟除数一致。
    // 精确算术下 x % y == x - (x // y) * y 成立；两边各自按上下文舍入，返回值不保证逐位相等。
    // 余数恰好为零时符号跟被除数走（-6 % 3 是 -0，照抄规范）。商的位数超过 prec 时触发
    // DivisionImpossible
    [[nodiscard]] BigDec floor_div(const BigDec &rhs, DecContext &ctx) const;
    [[nodiscard]] BigDec mod(const BigDec &rhs, DecContext &ctx) const;
    [[nodiscard]] std::pair<BigDec, BigDec> divmod(const BigDec &rhs, DecContext &ctx) const;

    // 有限、且没有非零的小数部分
    [[nodiscard]] bool is_integral() const;

    // 超越函数：任意精度下算不出精确值，做法是"多算几位 → 看结果够不够定夺舍入方向 →
    // 不够就再多算三位"，循环到能定夺为止；最终那步 fix 固定按 ROUND_HALF_EVEN 走
    // （这时任何舍入方式都会给出同一个答案），算完把上下文的 rounding 原样还回去。
    // 算法逐个对应 Python `_pydecimal` 里的同名方法，整数层在 dec_math 里
    [[nodiscard]] BigDec sqrt(DecContext &ctx) const;  // 负数触发 InvalidOperation
    [[nodiscard]] BigDec exp(DecContext &ctx) const;   // e ** self
    [[nodiscard]] BigDec ln(DecContext &ctx) const;    // 负数触发 InvalidOperation
    [[nodiscard]] BigDec log10(DecContext &ctx) const; // 同上

    // self ** rhs。底数为负而指数不是整数时结果不是实数，触发 InvalidOperation；0 ** 0 同。
    // 跟上面四个不同，幂运算尊重上下文的舍入方式，不切成 HalfEven
    [[nodiscard]] BigDec pow(const BigDec &rhs, DecContext &ctx) const;

    // SL 的 == / !=：任一方是 NaN 就恒为不等，且不触发信号；只有 sNaN 才触发 InvalidOperation
    [[nodiscard]] bool equals(const BigDec &rhs, DecContext &ctx) const;
    // SL 的 < <= > >=：任一方是 NaN（安静的也算）都触发 InvalidOperation，没设陷阱时返回
    // unordered（于是四个比较全为 False，同 IEEE 754）
    [[nodiscard]] std::partial_ordering compare_ordering(const BigDec &rhs, DecContext &ctx) const;

  private:
    void check_invariant() const;

    // ln()/log10() 的结果的调整后指数的下界，用来定第一轮该算到小数点后多少位。
    // 调用方保证是有限的正数、且数值不等于 1
    [[nodiscard]] int64_t ln_exp_bound() const;
    [[nodiscard]] int64_t log10_exp_bound() const;

    // 整数的精确值。调用方保证是有限的整数，且量级不大——指数很大的整数（1E+999999）会让
    // 这里的 10^exp 炸开，pow() 的两个调用点都先把量级夹住了
    [[nodiscard]] BigInt integer_value() const;
    // 调用方保证是有限的整数
    [[nodiscard]] bool is_even_integer() const;

    // 试着精确算出 self ** other 并压进 p 位有效数字以内，办不到就返回 nullopt。
    // 调用方保证两边都是有限数、self 为正且数值不等于 1、other 不为 0，而且外层已经用
    // Emax/Etiny 做过溢出/下溢的粗筛（否则这里的 10^ye、xc^m 之类会炸开）
    [[nodiscard]] std::optional<BigDec> power_exact(const BigDec &other, int64_t p) const;

    // 造一个有限数，不检查指数范围（运算的中间结果允许暂时越界，由 fix 收尾）。
    // 调用方保证 coeff 非负
    [[nodiscard]] static BigDec make_finite(bool sign, BigInt coeff, int64_t exp);

    // 舍入 + 指数范围检查，是每个算术结果的最后一步（IBM 的 "fix"）：把结果压到至多 prec 位
    // 有效数字、指数压进 [Etiny, Emax]，并按规范规定的先后顺序触发
    // Overflow/Underflow/Subnormal/Inexact/Rounded/Clamped
    [[nodiscard]] BigDec fix(DecContext &ctx) const;

    // 把自己重新表示成指数恰为 exp 的形式，靠补零，因此恒精确。安静操作——不触发任何信号、
    // 不查上下文。调用方保证是有限数、且 exp <= 自己的指数
    [[nodiscard]] BigDec pad_to_exponent(int64_t exp) const;

    // 把 coeff 截到只保留最高 keep 位，返回 (截断后的系数, 舍入判定)。判定的含义同 IBM 规范：
    // 1 = 向远离零进位，0 = 被截掉的部分全是 0，-1 = 被截掉的部分非 0 但不进位。
    // sign 只有 Ceiling/Floor 用得上。调用方保证 keep < coeff 的十进制位数
    [[nodiscard]] static std::pair<BigInt, int>
    split_and_decide(const BigInt &coeff, size_t keep, bool sign, DecRounding rounding);

    // 触发 Overflow 并给出没设陷阱时的结果：按 rounding 决定是 ±Infinity 还是当前精度下最大的
    // 有限数
    [[nodiscard]] static BigDec raise_overflow(DecContext &ctx, bool sign);
    // 触发 InvalidOperation（或它的某个细分条件），没设陷阱时结果是安静 NaN
    [[nodiscard]] static BigDec
    raise_invalid(DecContext &ctx, DecCondition condition = DecCondition::InvalidOperation);

    // NaN 传播：任一方是 NaN 就返回该 NaN（sNaN 先触发 InvalidOperation，再退化成安静 NaN），
    // 都不是 NaN 则返回 nullopt
    [[nodiscard]] static std::optional<BigDec> check_nans(const BigDec &a, DecContext &ctx);
    [[nodiscard]] static std::optional<BigDec>
    check_nans(const BigDec &a, const BigDec &b, DecContext &ctx);

    // 数值大小比较，-1/0/1。调用方保证两边都不是 NaN
    [[nodiscard]] static int cmp_no_nan(const BigDec &a, const BigDec &b);

    // IBM 规范里那对向零截断的 divide-integer / remainder，是 //、% 的原料。返回 (商的绝对值,
    // 余数)：余数是精确值、符号同被除数、指数取两个操作数里较小的那个。调用方保证两边都不是
    // NaN、self 有限、rhs 非零。nullopt 表示商的位数超过 prec（对应 DivisionImpossible）
    [[nodiscard]] std::optional<std::pair<BigInt, BigDec>>
    trunc_divmod(const BigDec &rhs, int64_t prec) const;

    // 把 // 算出来的整数商装回 BigDec。商为 0 时 BigInt 记不住符号，用 sign_if_zero 补上
    [[nodiscard]] static BigDec quotient_to_dec(const BigInt &quotient, bool sign_if_zero);

    // 向零截断的商/余数要不要往负无穷方向修正一格。余数的符号跟被除数一致，所以"余数与除数
    // 异号"就是"两个操作数异号"
    [[nodiscard]] static bool needs_floor_correction(
        const BigDec &dividend, const BigDec &divisor, const BigDec &trunc_remainder
    );

    // 把 trunc_divmod 给的商的绝对值修正成向负无穷取整的商（带符号）。
    // nullopt 表示位数超过 prec——要按修正之后的商来判（截断商恰好 prec 位时减 1 会多出一位）
    [[nodiscard]] static std::optional<BigInt> floor_quotient(
        const BigInt &magnitude, const BigDec &dividend, const BigDec &divisor,
        const BigDec &trunc_remainder, int64_t prec
    );
};
