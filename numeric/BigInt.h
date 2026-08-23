#pragma once

#include <compare>
#include <cstdint>
#include <string>
#include <vector>

// 高精度整数：小路径 int64_t / 大路径 limbs 双表示。大数算法只求正确、不追求渐进最优
// （乘法朴素竖式，除法二进制逐位长除）。
// 运算一律以具名方法为准（add/sub/...），operator 只是同名方法的转发
class BigInt {
    bool is_small_{true};
    int64_t small_{0};
    std::vector<uint32_t> limbs_; // 小端，limbs_[0] 最低 32 位；大路径专用
    bool negative_{false};        // 大路径专用；值为 0 时恒 false（不存在负零）

  public:
    BigInt() = default;
    explicit BigInt(long long value);

    // —————————— 构造 ——————————

    // 语法 `[+-]?digits([eE][+-]?digits)?`，不合法抛 std::invalid_argument。
    // 指数必须非负（`1e-9` 不是整数；`100e-1` 值虽是整数 10 也不收，只看写法不看值），
    // 且不设上限——`1e999999999` 会真去造一个十亿位的数。SL 字面量另有 65536 的指数上限，
    // 那条归 lexer 管：它只约束 e 记法、不约束手写的等长字面量，是源码形态的政策
    // （"不允许前导零"同理，这里 "007" 照常给 7）
    [[nodiscard]] static BigInt from_decimal_string(const std::string &s);

    // —————————— 转换 ——————————

    // 负数带前导 '-'，恒无多余前导 0（0 输出 "0"）
    [[nodiscard]] std::string to_decimal_string() const;
    // 超出 double 表示范围按 IEEE 溢出语义返回 ±infinity
    [[nodiscard]] double to_double() const;

    // —————————— 查询 ——————————

    [[nodiscard]] bool is_zero() const { return is_small_ ? small_ == 0 : limbs_.empty(); }
    [[nodiscard]] bool is_negative() const { return is_small_ ? small_ < 0 : negative_; }
    [[nodiscard]] bool is_odd() const {
        return is_small_ ? (small_ % 2 != 0) : (!limbs_.empty() && (limbs_[0] & 1u));
    }
    // -1 / 0 / 1
    [[nodiscard]] int sign() const;
    // |x| 的十进制位数，0 算 1 位
    [[nodiscard]] size_t num_decimal_digits() const;
    // |x| 的二进制位数，0 算 0 位。同 Python 的 int.bit_length()
    [[nodiscard]] size_t bit_length() const;

    // —————————— 一元运算 ——————————

    // int 的一元 + 是恒等操作（decimal 的不是，那个要按上下文舍入）
    [[nodiscard]] BigInt plus() const { return *this; }
    [[nodiscard]] BigInt minus() const;
    [[nodiscard]] BigInt abs() const;
    // ~x == -x - 1（补码按位取反）
    [[nodiscard]] BigInt bit_not() const;

    // —————————— 二元算术 ——————————

    [[nodiscard]] BigInt add(const BigInt &rhs) const;
    [[nodiscard]] BigInt sub(const BigInt &rhs) const;
    [[nodiscard]] BigInt mul(const BigInt &rhs) const;
    // SL 的 //、%：向负无穷取整，语义同 Python。除数为 0 抛 std::domain_error
    // （转成 SL 的 MathError 是调用方的事，BigInt 不认识 SL 的异常体系）
    [[nodiscard]] BigInt floor_div(const BigInt &divisor) const;
    [[nodiscard]] BigInt mod(const BigInt &divisor) const;
    // exponent < 0 抛 std::domain_error（SL 里 int ** 负数是 decimal）；结果不设规模上限
    [[nodiscard]] BigInt pow(const BigInt &exponent) const;

    // —————————— 位运算：按无穷位补码，负数高位视为全 1 ——————————

    [[nodiscard]] BigInt bit_and(const BigInt &rhs) const;
    [[nodiscard]] BigInt bit_or(const BigInt &rhs) const;
    [[nodiscard]] BigInt bit_xor(const BigInt &rhs) const;
    // 恒等于 * 2^k、// 2^k（负数右移不会"变正"）。k < 0 抛 std::domain_error
    [[nodiscard]] BigInt shift_left(long long k) const;
    [[nodiscard]] BigInt shift_right(long long k) const;

    // —————————— 比较 ——————————

    [[nodiscard]] bool equals(const BigInt &rhs) const;
    [[nodiscard]] std::strong_ordering compare_ordering(const BigInt &rhs) const;

    // —————————— 运算符：全部转发到上面的同名方法 ——————————

    [[nodiscard]] BigInt operator+() const { return plus(); }
    [[nodiscard]] BigInt operator-() const { return minus(); }
    [[nodiscard]] BigInt operator~() const { return bit_not(); }

    [[nodiscard]] BigInt operator+(const BigInt &rhs) const { return add(rhs); }
    [[nodiscard]] BigInt operator-(const BigInt &rhs) const { return sub(rhs); }
    [[nodiscard]] BigInt operator*(const BigInt &rhs) const { return mul(rhs); }

    [[nodiscard]] BigInt operator&(const BigInt &rhs) const { return bit_and(rhs); }
    [[nodiscard]] BigInt operator|(const BigInt &rhs) const { return bit_or(rhs); }
    [[nodiscard]] BigInt operator^(const BigInt &rhs) const { return bit_xor(rhs); }
    [[nodiscard]] BigInt operator<<(const long long k) const { return shift_left(k); }
    [[nodiscard]] BigInt operator>>(const long long k) const { return shift_right(k); }

    [[nodiscard]] bool operator==(const BigInt &rhs) const { return equals(rhs); }
    [[nodiscard]] std::strong_ordering operator<=>(const BigInt &rhs) const {
        return compare_ordering(rhs);
    }

  private:
    // 去掉多余的最高位 0；若结果为空则把 negative_ 归位成 false。不负责收缩回小路径
    void normalize();
    // 用一段大小（可能带多余高位 0）+ 符号构造大路径值（内部 normalize）
    [[nodiscard]] static BigInt from_magnitude(std::vector<uint32_t> limbs, bool negative);
    // 转成大路径表示（已经是大路径则原样拷贝）
    [[nodiscard]] BigInt promoted() const;
    // 能装进 int64_t 就收缩回小路径，否则原样返回。所有慢路径算完的地方都要过这一步
    [[nodiscard]] static BigInt shrink(BigInt big);
    // Debug 断言：核实"能装进 int64_t 就必然是小路径"等内部不变量。
    // 只在依赖它的入口调用，不放进 shrink() 内部（会互相递归）
    void check_invariant() const;

    // 无穷位补码视角下前 limb_count 个 limb（负数高位补 1）。调用方保证走大路径、
    // limb_count > limbs_.size()（留一个安全 limb，否则全 1 的最高位会被误读成符号位）
    [[nodiscard]] std::vector<uint32_t> to_twos_complement(size_t limb_count) const;
    // 逆操作：补码 limb（最高位决定符号）转回符号-大小表示（大路径）
    [[nodiscard]] static BigInt from_twos_complement(std::vector<uint32_t> limbs);
    // bit_and/bit_or/bit_xor 的公共骨架：两边取补码、逐 limb 施加 op、再转回来
    [[nodiscard]] BigInt bitwise_big(const BigInt &rhs, uint32_t (*op)(uint32_t, uint32_t)) const;

    // 向负无穷取整的除法+取模。调用方保证 divisor 不为 0、两边都走大路径
    [[nodiscard]] std::pair<BigInt, BigInt> divmod_floor_big(const BigInt &divisor) const;
};
