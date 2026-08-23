#pragma once

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "BigInt.h"
#include "DecContext.h"

// 十进制浮点数，SL 的 decimal 的底层实现：值为 (-1)^符号 × 系数 × 10^指数，系数任意精度。
// 算术遵循 IBM 通用十进制算术规范（Python decimal），不一致处逐处注明——目前只有 //、%
// 向负无穷取整。 标度是值的一部分（1.5 与 1.50 数值相等但表示不同），构造不舍入、运算才舍入。
// 运算一律以具名方法为准，operator 只是同名方法配一个默认上下文的转发
class BigDec {
  public:
    enum class Kind : uint8_t { Finite, Infinity, NaN, SignalingNaN };

    // 构造出来的有限数，指数须落在 ±kMaxExponent 内（中间结果可暂时超出）。上界取 Emax 上限
    // + prec 上限，保证任何合法上下文下 fix 出的结果（指数最低到 Etiny）都能被 try_from_string 读回
    static constexpr int64_t kMaxExponent{
        static_cast<int64_t>(DecContext::kMaxExp) + DecContext::kMaxPrec
    };

  private:
    Kind kind_{Kind::Finite};
    // 符号独立于 coeff_（恒非负）存：否则表示不出负零。特殊值也带符号（-Infinity、-NaN）
    bool sign_{false};
    BigInt coeff_;   // 系数，恒非负；特殊值时恒为 0，不参与运算
    int64_t exp_{0}; // 十进制指数；特殊值时恒为 0，不参与运算

    void check_invariant() const;

    // 造一个有限数，不检查指数范围（中间结果可越界，由 fix 收尾）。调用方保证 coeff 非负
    [[nodiscard]] static BigDec make_finite(bool sign, BigInt coeff, int64_t exp);

    // 每个算术结果的最后一步（IBM 的 "fix"）：压到 prec 位有效数字、指数进 [Etiny, Emax]，
    // 并按规范规定的先后顺序触发 Overflow/Underflow/Subnormal/Inexact/Rounded/Clamped
    [[nodiscard]] BigDec fix(DecContext &ctx) const;

    // 补零重表示成指数恰为 exp 的形式，恒精确。调用方保证是有限数、exp <= 自己的指数
    [[nodiscard]] BigDec pad_to_exponent(int64_t exp) const;

    // ln()/log10() 的结果的调整后指数的下界，用来定第一轮该算到小数点后多少位。
    // 调用方保证是有限的正数、且数值不等于 1
    [[nodiscard]] int64_t ln_exp_bound() const;
    [[nodiscard]] int64_t log10_exp_bound() const;

    // 整数的精确值。调用方保证是有限整数且量级不大（pow() 的两个调用点已先把量级夹住）
    [[nodiscard]] BigInt integer_value() const;
    // 调用方保证是有限的整数
    [[nodiscard]] bool is_even_integer() const;

    // 试着精确算出 self ** other 并压进 p 位以内，办不到返回 nullopt。
    // 调用方保证两边有限、self 为正且 ≠1、other ≠0，且外层已用 Emax/Etiny 做过溢出/下溢粗筛
    [[nodiscard]] std::optional<BigDec> power_exact(const BigDec &other, int64_t p) const;

    // IBM 规范那对向零截断的 divide-integer / remainder，是 //、% 的原料。返回 (商的绝对值, 余数)：
    // 余数符号同被除数、指数取两者中较小的。nullopt 表示商位数超过 prec（DivisionImpossible）
    [[nodiscard]] std::optional<std::pair<BigInt, BigDec>>
    trunc_divmod(const BigDec &rhs, int64_t prec) const;

  public:
    BigDec() = default; // +0，指数 0

    // —————————— 构造 ——————————

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
    // 上下文版：不合法则触发 ConversionSyntax，没设陷阱时返回安静 NaN
    [[nodiscard]] static BigDec from_string(const std::string &s, DecContext &ctx);

    // —————————— 转换 ——————————

    // IBM 的 to-scientific-string：完整保留表示（1.5 与 1.50、0 与 -0 都得到不同的串），
    // 因此 try_from_string(x.to_string()) 恒与 x 表示层面完全相同。不查上下文、不触发信号
    [[nodiscard]] std::string to_string() const;

    // —————————— 查询 ——————————

    [[nodiscard]] Kind kind() const { return kind_; }
    [[nodiscard]] bool is_finite() const { return kind_ == Kind::Finite; }
    [[nodiscard]] bool is_infinite() const { return kind_ == Kind::Infinity; }
    [[nodiscard]] bool is_nan() const { return kind_ == Kind::NaN || kind_ == Kind::SignalingNaN; }
    [[nodiscard]] bool is_signaling_nan() const { return kind_ == Kind::SignalingNaN; }
    // 有限且系数为 0（负零也算）
    [[nodiscard]] bool is_zero() const { return kind_ == Kind::Finite && coeff_.is_zero(); }
    // 符号位本身：负零、-Infinity、-NaN 都为 true
    [[nodiscard]] bool is_negative() const { return sign_; }
    // 有限、且没有非零的小数部分
    [[nodiscard]] bool is_integral() const;

    [[nodiscard]] const BigInt &coefficient() const { return coeff_; }
    [[nodiscard]] int64_t exponent() const { return exp_; }
    // 系数的十进制位数，系数为 0 时算 1 位
    [[nodiscard]] size_t digit_count() const;
    // 调整后的指数 exp + 位数 - 1（科学计数法 d.dddE±n 里的 n）。调用方保证是有限数
    [[nodiscard]] int64_t adjusted() const;

    // 表示层面完全相同。跟 SL 的 == 不是一回事：1.5 vs 1.50、NaN vs NaN 这里正好都相反
    [[nodiscard]] bool identical(const BigDec &rhs) const;

    // —————————— 一元运算 ——————————

    // 只翻/清符号位，不舍入、不查上下文（IBM 的 copy-negate / copy-abs）
    [[nodiscard]] BigDec copy_negate() const;
    [[nodiscard]] BigDec copy_abs() const;

    // 按 IBM 规范这三个都要按上下文舍入，+x 不是恒等操作（int 的才是）
    [[nodiscard]] BigDec plus(DecContext &ctx) const;
    [[nodiscard]] BigDec minus(DecContext &ctx) const;
    [[nodiscard]] BigDec abs(DecContext &ctx) const;

    // —————————— 二元算术 ——————————

    [[nodiscard]] BigDec add(const BigDec &rhs, DecContext &ctx) const;
    [[nodiscard]] BigDec sub(const BigDec &rhs, DecContext &ctx) const;
    [[nodiscard]] BigDec mul(const BigDec &rhs, DecContext &ctx) const;
    [[nodiscard]] BigDec div(const BigDec &rhs, DecContext &ctx) const;

    // SL 的 //、%：向负无穷取整（不是 IBM 规范的向零截断），非零余数符号跟除数一致。
    // 余数恰为零时符号跟被除数走（-6 % 3 是 -0）。商位数超过 prec 触发 DivisionImpossible
    [[nodiscard]] BigDec floor_div(const BigDec &rhs, DecContext &ctx) const;
    [[nodiscard]] BigDec mod(const BigDec &rhs, DecContext &ctx) const;
    [[nodiscard]] std::pair<BigDec, BigDec> divmod(const BigDec &rhs, DecContext &ctx) const;

    // 底数为负而指数不是整数时结果不是实数，触发 InvalidOperation；0 ** 0 同。
    // 跟下面四个超越函数不同，幂运算尊重上下文的舍入方式，不切成 HalfEven
    [[nodiscard]] BigDec pow(const BigDec &rhs, DecContext &ctx) const;

    // —————————— 超越函数 ——————————

    // 任意精度下算不出精确值：多算几位、不够定夺舍入方向就再多三位，循环到能定夺为止；
    // 最终 fix 固定按 ROUND_HALF_EVEN 走（此时任何舍入方式同解），算完还原上下文的 rounding。
    // 算法对应 Python `_pydecimal` 同名方法，整数层在 dec_math
    [[nodiscard]] BigDec sqrt(DecContext &ctx) const;  // 负数触发 InvalidOperation
    [[nodiscard]] BigDec exp(DecContext &ctx) const;   // e ** self
    [[nodiscard]] BigDec ln(DecContext &ctx) const;    // 负数触发 InvalidOperation
    [[nodiscard]] BigDec log10(DecContext &ctx) const; // 同上

    // —————————— 比较 ——————————

    // SL 的 == / !=：任一方是 NaN 就恒为不等，且不触发信号；只有 sNaN 才触发 InvalidOperation
    [[nodiscard]] bool equals(const BigDec &rhs, DecContext &ctx) const;
    // SL 的 < <= > >=：任一方是 NaN（安静的也算）都触发 InvalidOperation，没设陷阱时返回
    // unordered（于是四个比较全为 False，同 IEEE 754）
    [[nodiscard]] std::partial_ordering compare_ordering(const BigDec &rhs, DecContext &ctx) const;

    // —————————— 运算符 ——————————
    //
    // 转发到同名方法，配默认上下文（prec 28 / HalfEven / 默认三陷阱），因此 1/0、NaN 参与序比较
    // 等会抛 DecTrapped；flags 随临时上下文丢弃、也换不了精度——要这些就直接调具名方法

    [[nodiscard]] BigDec operator+() const;
    [[nodiscard]] BigDec operator-() const;

    [[nodiscard]] BigDec operator+(const BigDec &rhs) const;
    [[nodiscard]] BigDec operator-(const BigDec &rhs) const;
    [[nodiscard]] BigDec operator*(const BigDec &rhs) const;
    [[nodiscard]] BigDec operator/(const BigDec &rhs) const;
    [[nodiscard]] BigDec operator%(const BigDec &rhs) const;

    [[nodiscard]] bool operator==(const BigDec &rhs) const;
    [[nodiscard]] std::partial_ordering operator<=>(const BigDec &rhs) const;
};
