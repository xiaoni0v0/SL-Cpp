#pragma once

#include <compare>
#include <cstdint>
#include <string>
#include <vector>

// 高精度整数：小路径 int64_t / 大路径 limbs 双表示。大数算法只求正确，不追求渐进最优
// （乘法朴素竖式、除法二进制逐位长除）
class BigInt {
    bool is_small_{true};
    int64_t small_{0};
    std::vector<uint32_t> limbs_; // 小端，limbs_[0] 最低 32 位；大路径专用
    bool negative_{false};        // 大路径专用；值为 0 时恒 false（不存在负零）

    // 去掉多余的最高位 0；若结果为空则把 negative_ 归位成 false。不负责收缩回小路径
    void normalize();

    // 转成大路径表示（已经是大路径则原样拷贝）
    [[nodiscard]] BigInt promoted() const;
    // 能装进 int64_t 就收缩回小路径，否则原样返回。所有慢路径算完的地方都要过这一步
    [[nodiscard]] static BigInt shrink(BigInt big);
    // Debug 断言：核实"能装进 int64_t 就必然是小路径"等内部不变量。
    // 只在依赖它的入口调用，不放进 shrink() 内部（会互相递归）
    void check_invariant() const;
    // 用一段大小（可能带多余高位 0）+ 符号构造大路径值（内部 normalize）
    [[nodiscard]] static BigInt from_magnitude(std::vector<uint32_t> limbs, bool negative);

    // 以下均只处理大小（不管符号），调用方保证参数已经 normalize（不带多余高位 0，
    // 值为 0 则是空 vector）。|a| 与 |b| 的比较：<0/=0/>0；减法要求 a >= b；
    // 除法是二进制逐位长除，要求 b 不为 0
    [[nodiscard]] static std::strong_ordering
    compare_magnitude(const std::vector<uint32_t> &a, const std::vector<uint32_t> &b);
    [[nodiscard]] static std::vector<uint32_t>
    add_magnitude(const std::vector<uint32_t> &a, const std::vector<uint32_t> &b);
    [[nodiscard]] static std::vector<uint32_t>
    sub_magnitude(const std::vector<uint32_t> &a, const std::vector<uint32_t> &b);
    [[nodiscard]] static std::vector<uint32_t>
    mul_magnitude(const std::vector<uint32_t> &a, const std::vector<uint32_t> &b);
    [[nodiscard]] static std::pair<std::vector<uint32_t>, std::vector<uint32_t>>
    div_mod_magnitude(const std::vector<uint32_t> &a, const std::vector<uint32_t> &b);
    // 左移 bits 位（bits 可以很大，用于 * 2^bits）
    [[nodiscard]] static std::vector<uint32_t>
    shift_left_magnitude(const std::vector<uint32_t> &a, uint64_t bits);
    // 右移 bits 位（向零截断）。返回截断后的大小 + 被移出的位是否有 1
    // （负数右移由调用方按这个标志决定要不要把截断商多减 1）
    [[nodiscard]] static std::pair<std::vector<uint32_t>, bool>
    shift_right_magnitude(const std::vector<uint32_t> &a, uint64_t bits);

    // 无穷位补码视角下前 limb_count 个 32 位 limb（负数高位补 1）。调用方保证走大路径、
    // limb_count > limbs_.size()（留一个安全 limb，否则全 1 的最高位可能被误读成符号位）
    [[nodiscard]] std::vector<uint32_t> to_twos_complement(size_t limb_count) const;
    // to_twos_complement 的逆操作：补码 limb（最高位决定符号）转回符号-大小表示（大路径）
    [[nodiscard]] static BigInt from_twos_complement(std::vector<uint32_t> limbs);

    // 向负无穷取整的除法+取模（大路径）。调用方保证 divisor 不为 0、两边都走大路径
    [[nodiscard]] std::pair<BigInt, BigInt> divmod_floor_big(const BigInt &divisor) const;

  public:
    BigInt() = default;
    explicit BigInt(long long value);

    // 十进制字符串构造，允许前导 '-'/'+'，其余字符不合法则抛 std::invalid_argument
    [[nodiscard]] static BigInt from_decimal_string(const std::string &s);

    // 转十进制字符串，负数带前导 '-'，恒无多余前导 0（0 输出 "0"）
    [[nodiscard]] std::string to_decimal_string() const;

    // |x| 的十进制位数，0 算 1 位
    [[nodiscard]] size_t num_decimal_digits() const;

    // |x| 的二进制位数，0 算 0 位。同 Python 的 int.bit_length()
    [[nodiscard]] size_t bit_length() const;

    // 转 double；超出 double 表示范围按 IEEE 溢出语义返回 ±infinity
    [[nodiscard]] double to_double() const;

    [[nodiscard]] bool is_zero() const { return is_small_ ? small_ == 0 : limbs_.empty(); }
    [[nodiscard]] bool is_negative() const { return is_small_ ? small_ < 0 : negative_; }
    // -1 / 0 / 1
    [[nodiscard]] int sign() const;
    [[nodiscard]] bool is_odd() const {
        return is_small_ ? (small_ % 2 != 0) : (!limbs_.empty() && (limbs_[0] & 1u));
    }

    [[nodiscard]] BigInt abs() const;

    [[nodiscard]] BigInt operator-() const;
    [[nodiscard]] BigInt operator+() const { return *this; }
    // ~x == -x - 1（补码按位取反）
    [[nodiscard]] BigInt operator~() const;

    [[nodiscard]] BigInt operator+(const BigInt &rhs) const;
    [[nodiscard]] BigInt operator-(const BigInt &rhs) const;
    [[nodiscard]] BigInt operator*(const BigInt &rhs) const;

    // 向负无穷取整的除法/取模（SL 的 //、%，语义同 Python）。
    // 除数为 0 抛 std::domain_error（转换 SL 的 MathError 是调用方的事）
    [[nodiscard]] BigInt floor_div(const BigInt &divisor) const;
    [[nodiscard]] BigInt mod(const BigInt &divisor) const;

    // exponent >= 0（SL 里 int ** 负数是 decimal，不归 BigInt 管），否则抛 std::domain_error；
    // 结果不设规模上限
    [[nodiscard]] BigInt pow(const BigInt &exponent) const;

    [[nodiscard]] BigInt operator&(const BigInt &rhs) const;
    [[nodiscard]] BigInt operator|(const BigInt &rhs) const;
    [[nodiscard]] BigInt operator^(const BigInt &rhs) const;
    // x << k 恒等于 x * 2^k。k < 0 抛 std::domain_error；k 很大时结果同样很大
    [[nodiscard]] BigInt operator<<(long long k) const;
    // x >> k 恒等于 x // 2^k（负数右移永远不会"变正"）。k < 0 抛 std::domain_error
    [[nodiscard]] BigInt operator>>(long long k) const;

    [[nodiscard]] std::strong_ordering operator<=>(const BigInt &rhs) const;
    [[nodiscard]] bool operator==(const BigInt &rhs) const;
};
