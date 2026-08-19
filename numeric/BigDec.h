#pragma once

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "BigInt.h"
#include "DecContext.h"

// 十进制浮点数，SL 的 decimal 的底层实现：值为 (-1)^符号 × 系数 × 10^指数，系数是任意精度整数、
// 指数是范围有限的整数。算术遵循 IBM 通用十进制算术规范（Python decimal 实现的那一份），
// 与之不一致的地方逐处注明——目前只有 floor_div/mod 一处（SL 的 //、% 向负无穷取整，
// 不是规范里 divide-integer/remainder 的向零截断）。
//
// **标度是值的一部分**：1.5 和 1.50 数值相等但指数不同，to_string() 得到的串也不同。因此
// BigInt 那条"一个值只有一种表示"的不变量在这里**不成立**，判等不能靠结构相等，一律走
// equals()/compare_ordering()。
//
// 构造不舍入，运算才舍入：字面量、from_string()、from_bigint() 都精确保留全部位数，
// 每个算术运算的结果才按上下文舍入到至多 prec 位有效数字。
class BigDec {
  public:
    enum class Kind : uint8_t { Finite, Infinity, NaN, SignalingNaN };

    // 构造出来的有限数，指数必须落在 [-kMaxExponent, kMaxExponent] 里（运算的中间结果可以暂时
    // 超出，fix 之后必然回到 [Etiny, Emax] 之内）
    static constexpr int64_t kMaxExponent{999999999};

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

    // 按 IBM 规范的数字字符串语法解析（`Inf`/`Infinity`/`NaN`/`sNaN` 以及指数记号 `E` 都不区分
    // 大小写），不合法返回 nullopt。构造不舍入，因此不需要上下文；指数超出 kMaxExponent 也算
    // 不合法。跟 Python 的三处差异：不接受首尾空白、不接受数字里的 `_`、不接受 NaN 后面的诊断
    // 信息（SL 的 decimal 没有 NaN 诊断信息这个概念，静默丢掉比拒绝更糟）
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
    // 调整后的指数 exp + 位数 - 1，即科学计数法写成 d.dddE±n 时的那个 n。调用方保证是有限数
    [[nodiscard]] int64_t adjusted() const;

    // IBM 的 to-scientific-string：完整保留表示（`1.5` 与 `1.50`、`0` 与 `-0` 都得到不同的串），
    // 因此 try_from_string(x.to_string()) 恒与 x 表示层面完全相同。不查上下文、不触发信号
    [[nodiscard]] std::string to_string() const;

    // 表示层面完全相同（类别、符号、系数、指数都一样）。跟 SL 的 `==` **不是**一回事：
    // SL 里 1.5 == 1.50 为真、NaN == NaN 为假，这里正好都相反
    [[nodiscard]] bool identical(const BigDec &rhs) const;

    // 只翻/清符号位，不舍入、不查上下文、不触发任何信号（IBM 的 copy-negate / copy-abs）
    [[nodiscard]] BigDec copy_negate() const;
    [[nodiscard]] BigDec copy_abs() const;

    // 一元 +、-、abs。按 IBM 规范这三个都要按上下文舍入，`+x` 不是恒等操作
    [[nodiscard]] BigDec plus(DecContext &ctx) const;
    [[nodiscard]] BigDec minus(DecContext &ctx) const;
    [[nodiscard]] BigDec abs(DecContext &ctx) const;

    [[nodiscard]] BigDec add(const BigDec &rhs, DecContext &ctx) const;
    [[nodiscard]] BigDec sub(const BigDec &rhs, DecContext &ctx) const;
    [[nodiscard]] BigDec mul(const BigDec &rhs, DecContext &ctx) const;
    [[nodiscard]] BigDec div(const BigDec &rhs, DecContext &ctx) const;

    // SL 的 // 和 %：**向负无穷取整**，不是 IBM 规范里向零截断的 divide-integer/remainder。
    // 因此非零余数的符号跟除数一致（规范里跟被除数一致），恒满足 x % y == x - (x // y) * y。
    // 余数恰好为零时符号仍跟被除数走（`-6 % 3` 是 `-0`）：这一处照抄规范，因为上面那个恒等式
    // 自己在零上也定不出符号（IBM 的加法规定 a - a 得 +0），换成"跟除数走"并不更有理有据。
    // 商的位数超过 prec 时触发 DivisionImpossible
    [[nodiscard]] BigDec floor_div(const BigDec &rhs, DecContext &ctx) const;
    [[nodiscard]] BigDec mod(const BigDec &rhs, DecContext &ctx) const;
    [[nodiscard]] std::pair<BigDec, BigDec> divmod(const BigDec &rhs, DecContext &ctx) const;

    // SL 的 == / !=：任一方是 NaN 就恒为不等，且不触发信号；只有 sNaN 才触发 InvalidOperation
    [[nodiscard]] bool equals(const BigDec &rhs, DecContext &ctx) const;
    // SL 的 < <= > >=：任一方是 NaN（安静的也算）都触发 InvalidOperation，没设陷阱时返回
    // unordered（于是四个比较全为 False，同 IEEE 754）
    [[nodiscard]] std::partial_ordering compare_ordering(const BigDec &rhs, DecContext &ctx) const;

  private:
    void check_invariant() const;

    // 造一个有限数，不检查指数范围（运算的中间结果允许暂时越界，由 fix 收尾）。
    // 调用方保证 coeff 非负
    [[nodiscard]] static BigDec make_finite(bool sign, BigInt coeff, int64_t exp);

    // 舍入 + 指数范围检查，是每个算术结果的最后一步（IBM 的 "fix"）：把结果压到至多 prec 位
    // 有效数字、指数压进 [Etiny, Emax]，并按规范规定的先后顺序触发
    // Overflow/Underflow/Subnormal/Inexact/Rounded/Clamped
    [[nodiscard]] BigDec fix(DecContext &ctx) const;

    // 把自己重新表示成指数恰为 exp 的形式：指数变小就补零（精确），变大就按 rounding 舍掉低位。
    // 安静操作——不触发任何信号、不查上下文。调用方保证是有限数
    [[nodiscard]] BigDec rescale(int64_t exp, DecRounding rounding) const;

    // 把 coeff 截到只保留最高 keep 位，返回 (截断后的系数, 舍入判定)。判定的含义同 IBM 规范：
    // 1 = 该向远离零的方向进位，0 = 被截掉的部分全是 0（值没变），-1 = 被截掉的部分非 0 但不进位。
    // sign 只有 Ceiling/Floor 两种舍入方式用得上。调用方保证 keep < coeff 的十进制位数
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
    // nullopt 表示位数超过 prec——注意要按修正**之后**的商来判，截断商恰好 prec 位时减 1 会多
    // 出一位。% 也要查这一条：商算不出来的话 x % y == x - (x // y) * y 就不成立了
    [[nodiscard]] static std::optional<BigInt> floor_quotient(
        const BigInt &magnitude, const BigDec &dividend, const BigDec &divisor,
        const BigDec &trunc_remainder, int64_t prec
    );
};
