#pragma once

#include <compare>
#include <cstdint>
#include <string>
#include <vector>

// 小数快路径 + 大数慢路径
// 大数算法暂不考虑渐进最优（假设内存足够），只保证正确性；
// 乘法是朴素 O(n*m) 竖式乘法，除法是二进制逐位长除法（不是更快的 Knuth Algorithm D）。
class BigInt {
    // 小路径：is_small_ 为 true 时，值就是 small_ 本身，limbs_/negative_ 不使用。
    // 大路径：符号-大小表示，small_ 不使用。limbs_[0] 是最低 32 位，恒无多余最高位 0
    // （值为 0 时 limbs_ 为空，靠 normalize() 维护）；负数只在大路径下由 negative_
    // 表示，值为 0 时恒为 false（不存在"负零"）。
    bool is_small_{true};
    int64_t small_{0};
    std::vector<uint32_t> limbs_;
    bool negative_{false};

    // 去掉多余的最高位 0；若结果为空则连带把 negative_ 归位成 false（只操作大路径的
    // limbs_/negative_， 不负责"是否该整个收缩回小路径"——那是 shrink() 的职责）
    void normalize();

    // 返回一个保证走大路径的等价值（已经是大路径就直接拷贝，是小路径则转换）
    [[nodiscard]] BigInt promoted() const;
    // 已经是小路径就原样返回；大路径则装得下 int64_t 就收缩，否则原样返回。所有慢路径算完
    // 的地方都要过这一步，保证"能装进 int64_t 就一定是小路径"这条不变量（abs()
    // 的大路径分支例外：量级本就严格超出 int64_t 表示范围，取正后不变，这里恒是空转）
    [[nodiscard]] static BigInt shrink(BigInt big);
    // Debug 断言：核实"能装进 int64_t 就必然是小路径"等内部不变量（历史上两次真实 bug 都是
    // 它被静默破坏）。只在依赖它的入口调用，不放进 shrink() 内部——会跟里面用来验证的
    // shrink(*this) 互相递归
    void check_invariant() const;
    // 用一段大小（可能带多余高位 0）+ 符号构造一个大路径 BigInt（内部会 normalize）。
    // 集中在这一处显式设 is_small_ = false，避免各处手写漏设
    [[nodiscard]] static BigInt from_magnitude(std::vector<uint32_t> limbs, bool negative);

    // 以下均只处理大小（不管符号），调用方保证参数已经 normalize 过（不带多余高位 0，
    // 恒无 back() == 0，值为 0 则是空 vector）。|a| 与 |b| 的大小比较：<0/=0/>0
    [[nodiscard]] static std::strong_ordering
    compare_magnitude(const std::vector<uint32_t> &a, const std::vector<uint32_t> &b);
    [[nodiscard]] static std::vector<uint32_t>
    add_magnitude(const std::vector<uint32_t> &a, const std::vector<uint32_t> &b);
    // 调用方保证 a >= b（按 compare_magnitude）
    [[nodiscard]] static std::vector<uint32_t>
    sub_magnitude(const std::vector<uint32_t> &a, const std::vector<uint32_t> &b);
    [[nodiscard]] static std::vector<uint32_t>
    mul_magnitude(const std::vector<uint32_t> &a, const std::vector<uint32_t> &b);
    // 二进制逐位长除法：返回 (商, 余数)。调用方保证 b 不为 0
    [[nodiscard]] static std::pair<std::vector<uint32_t>, std::vector<uint32_t>>
    div_mod_magnitude(const std::vector<uint32_t> &a, const std::vector<uint32_t> &b);
    // 左移 bits 位（bits 可以很大，用于 * 2^bits），仅操作大小
    [[nodiscard]] static std::vector<uint32_t>
    shift_left_magnitude(const std::vector<uint32_t> &a, uint64_t bits);
    // 右移 bits 位（向零截断，仅操作大小）。返回截断后的大小 + 是否有被移出的位为
    // 1（非负数右移直接就是向负无穷取整；负数则调用方按这个标志决定要不要把截断商多减 1）
    [[nodiscard]] static std::pair<std::vector<uint32_t>, bool>
    shift_right_magnitude(const std::vector<uint32_t> &a, uint64_t bits);

    // 按位运算共用：算出 *this 在"无穷位补码"视角下前 limb_count 个 32 位 limb（非负数高位
    // 补 0，负数补 1）。调用方保证走大路径、limb_count > limbs_.size()
    // （留一个安全 limb，否则全 1 的最高位可能被误读成符号位，见 operator&/|/^ 的 "+1"）
    [[nodiscard]] std::vector<uint32_t> to_twos_complement(size_t limb_count) const;
    // to_twos_complement 的逆操作：给一段补码 limb（最高位决定符号），转回符号-大小表示（大路径）
    [[nodiscard]] static BigInt from_twos_complement(std::vector<uint32_t> limbs);

    // 向负无穷取整的除法+取模一起算（除法、取模各自只取其中一半）。调用方保证 divisor 不为 0、
    // *this 和 divisor 都走大路径（小路径在 floor_div/mod 里单独处理，不会走到这里）
    [[nodiscard]] std::pair<BigInt, BigInt> divmod_floor_big(const BigInt &divisor) const;

  public:
    BigInt() = default;
    explicit BigInt(long long value);

    // 十进制字符串构造，允许前导 '-'/'+'，不允许除数字外的其他字符（含千分位分隔符等）。
    // 空串或格式不对则抛 std::invalid_argument
    [[nodiscard]] static BigInt from_decimal_string(const std::string &s);

    // 转成十进制字符串，负数带前导 '-'，恒无多余前导 0（0 本身输出 "0"）
    [[nodiscard]] std::string to_decimal_string() const;

    // |x| 的十进制位数，0 算 1 位（等价于 to_decimal_string() 去掉负号之后的长度）
    [[nodiscard]] size_t num_decimal_digits() const;

    // 转成 double；超出 double 表示范围则返回 ±infinity（标准 IEEE 溢出语义，不抛异常）
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
    // 除数为 0 则抛 std::domain_error——是否转换成 SL 的 MathError 由调用方（StaticEvaler/VM）负责，
    // BigInt 本身不知道、也不该知道 SL 的异常类型体系。
    [[nodiscard]] BigInt floor_div(const BigInt &divisor) const;
    [[nodiscard]] BigInt mod(const BigInt &divisor) const;

    // 要求 exponent >= 0（SL 里 int ** 负数不再是 int，是 float，不归 BigInt 管），否则抛
    // std::domain_error；结果指数级增长，exponent 很大时可能抛 std::bad_alloc（不设上限）
    [[nodiscard]] BigInt pow(const BigInt &exponent) const;

    [[nodiscard]] BigInt operator&(const BigInt &rhs) const;
    [[nodiscard]] BigInt operator|(const BigInt &rhs) const;
    [[nodiscard]] BigInt operator^(const BigInt &rhs) const;
    // x << k 恒等于 x * 2^k（对负数同样成立）。k < 0 抛 std::domain_error；
    // k 很大时结果同样很大（不像 >> 能靠"结果收敛"短路），可能抛 std::bad_alloc
    [[nodiscard]] BigInt operator<<(long long k) const;
    // x >> k 恒等于 x // 2^k（向负无穷取整右移，负数右移永远不会"变正"）。k < 0 抛
    // std::domain_error
    [[nodiscard]] BigInt operator>>(long long k) const;

    [[nodiscard]] std::strong_ordering operator<=>(const BigInt &rhs) const;
    [[nodiscard]] bool operator==(const BigInt &rhs) const;
};
