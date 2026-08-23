#include "BigInt.h"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cmath>
#include <stdckdint.h>
#include <stdexcept>
#include <utility>

namespace {

// 以下均只处理"大小"（不管符号），调用方保证参数已 normalize：不带多余高位 0，值为 0 则是空
// vector。返回值同样满足这条

std::strong_ordering
compare_magnitude(const std::vector<uint32_t> &a, const std::vector<uint32_t> &b) {
    if (a.size() != b.size()) return a.size() <=> b.size();
    for (size_t i{a.size()}; i-- > 0;) {
        if (a[i] != b[i]) return a[i] <=> b[i];
    }
    return std::strong_ordering::equal;
}

std::vector<uint32_t>
add_magnitude(const std::vector<uint32_t> &a, const std::vector<uint32_t> &b) {
    const std::vector<uint32_t> &longer{a.size() >= b.size() ? a : b};
    const std::vector<uint32_t> &shorter{a.size() >= b.size() ? b : a};

    std::vector<uint32_t> result;
    result.reserve(longer.size() + 1);
    uint64_t carry{0};
    for (size_t i{0}; i < longer.size(); ++i) {
        const uint64_t sum{carry + longer[i] + (i < shorter.size() ? shorter[i] : 0)};
        result.push_back(static_cast<uint32_t>(sum));
        carry = sum >> 32;
    }
    if (carry) result.push_back(static_cast<uint32_t>(carry));
    while (!result.empty() && result.back() == 0) result.pop_back();
    return result;
}

// 调用方保证 a >= b
std::vector<uint32_t>
sub_magnitude(const std::vector<uint32_t> &a, const std::vector<uint32_t> &b) {
    assert(compare_magnitude(a, b) >= 0);

    std::vector<uint32_t> result;
    result.reserve(a.size());
    int64_t borrow{0};
    for (size_t i{0}; i < a.size(); ++i) {
        int64_t diff{
            static_cast<int64_t>(a[i]) - borrow - (i < b.size() ? static_cast<int64_t>(b[i]) : 0)
        };
        if (diff < 0) {
            diff += (static_cast<int64_t>(1) << 32);
            borrow = 1;
        } else {
            borrow = 0;
        }
        result.push_back(static_cast<uint32_t>(diff));
    }
    while (!result.empty() && result.back() == 0) result.pop_back();
    return result;
}

std::vector<uint32_t>
mul_magnitude(const std::vector<uint32_t> &a, const std::vector<uint32_t> &b) {
    if (a.empty() || b.empty()) return {};

    std::vector<uint32_t> result(a.size() + b.size(), 0);
    for (size_t i{0}; i < a.size(); ++i) {
        uint64_t carry{0};
        for (size_t j{0}; j < b.size(); ++j) {
            const uint64_t cur{static_cast<uint64_t>(a[i]) * b[j] + result[i + j] + carry};
            result[i + j] = static_cast<uint32_t>(cur);
            carry = cur >> 32;
        }
        size_t k{i + b.size()};
        while (carry) {
            const uint64_t cur{static_cast<uint64_t>(result[k]) + carry};
            result[k] = static_cast<uint32_t>(cur);
            carry = cur >> 32;
            ++k;
        }
    }
    while (!result.empty() && result.back() == 0) result.pop_back();
    return result;
}

// 左移 bits 位（bits 可以很大，用于 * 2^bits）
std::vector<uint32_t> shift_left_magnitude(const std::vector<uint32_t> &a, const uint64_t bits) {
    if (a.empty() || bits == 0) return a;

    const size_t limb_shift{static_cast<size_t>(bits / 32)};
    const unsigned bit_shift{static_cast<unsigned>(bits % 32)};

    std::vector<uint32_t> result(a.size() + limb_shift + 1, 0);
    for (size_t i{0}; i < a.size(); ++i) {
        const uint64_t shifted{static_cast<uint64_t>(a[i]) << bit_shift};
        result[i + limb_shift] |= static_cast<uint32_t>(shifted);
        if (bit_shift != 0) result[i + limb_shift + 1] |= static_cast<uint32_t>(shifted >> 32);
    }
    while (!result.empty() && result.back() == 0) result.pop_back();
    return result;
}

// 右移 bits 位（向零截断）。返回截断后的大小 + 被移出的位是否有 1
// （负数右移由调用方按这个标志决定要不要把截断商多减 1）
std::pair<std::vector<uint32_t>, bool>
shift_right_magnitude(const std::vector<uint32_t> &a, const uint64_t bits) {
    if (bits == 0) return {a, false};

    const size_t limb_shift{static_cast<size_t>(bits / 32)};
    const unsigned bit_shift{static_cast<unsigned>(bits % 32)};

    // 被移出的位是否有非 0：按整 limb 判断，跟 to_double 的 sticky 位同理
    bool dropped_nonzero{false};
    for (size_t i{0}; i < limb_shift && i < a.size() && !dropped_nonzero; ++i)
        if (a[i] != 0) dropped_nonzero = true;
    if (!dropped_nonzero && bit_shift != 0 && limb_shift < a.size()) {
        const uint32_t mask{(uint32_t{1} << bit_shift) - 1};
        if ((a[limb_shift] & mask) != 0) dropped_nonzero = true;
    }

    if (limb_shift >= a.size()) return {{}, dropped_nonzero};

    std::vector<uint32_t> result(a.size() - limb_shift, 0);
    for (size_t i{0}; i < result.size(); ++i) {
        const uint64_t low{a[i + limb_shift]};
        const uint64_t high{i + limb_shift + 1 < a.size() ? a[i + limb_shift + 1] : 0};
        const uint64_t combined{
            bit_shift == 0 ? low : (low >> bit_shift) | (high << (32 - bit_shift))
        };
        result[i] = static_cast<uint32_t>(combined);
    }
    while (!result.empty() && result.back() == 0) result.pop_back();
    return {std::move(result), dropped_nonzero};
}

// 二进制逐位长除法：从高位到低位边移边比较边减。慢但正确性显然，不用处理猜商修正。
// 返回 (商, 余数)，调用方保证 b 不为 0
std::pair<std::vector<uint32_t>, std::vector<uint32_t>>
div_mod_magnitude(const std::vector<uint32_t> &a, const std::vector<uint32_t> &b) {
    assert(!b.empty()); // 参数已 normalize，非空即非零

    if (a.empty()) return {{}, {}};

    std::vector<uint32_t> quotient(a.size(), 0);
    std::vector<uint32_t> remainder;

    const size_t total_bits{a.size() * 32};
    for (size_t bit_from_top{0}; bit_from_top < total_bits; ++bit_from_top) {
        const size_t bit_index{total_bits - 1 - bit_from_top};
        remainder = shift_left_magnitude(remainder, 1);

        const size_t limb_i{bit_index / 32};
        const size_t bit_in_limb{bit_index % 32};
        if ((a[limb_i] >> bit_in_limb) & 1u) {
            if (remainder.empty())
                remainder.push_back(1);
            else
                remainder[0] |= 1u;
        }

        if (compare_magnitude(remainder, b) >= 0) {
            remainder = sub_magnitude(remainder, b);
            quotient[limb_i] |= (1u << bit_in_limb);
        }
    }

    while (!quotient.empty() && quotient.back() == 0) quotient.pop_back();
    while (!remainder.empty() && remainder.back() == 0) remainder.pop_back();
    return {std::move(quotient), std::move(remainder)};
}

} // namespace

void BigInt::normalize() {
    while (!limbs_.empty() && limbs_.back() == 0) limbs_.pop_back();
    if (limbs_.empty()) negative_ = false;
}

BigInt BigInt::from_magnitude(std::vector<uint32_t> limbs, const bool negative) {
    BigInt result;
    result.is_small_ = false;
    result.limbs_ = std::move(limbs);
    result.negative_ = negative;
    result.normalize();
    return result;
}

BigInt BigInt::promoted() const {
    if (!is_small_) return *this;

    const bool negative{small_ < 0};
    // 分两步取 magnitude，避免 small_ == INT64_MIN 时 -small_ 溢出
    const uint64_t magnitude{
        negative ? static_cast<uint64_t>(-(small_ + 1)) + 1 : static_cast<uint64_t>(small_)
    };
    std::vector<uint32_t> limbs;
    uint64_t remaining{magnitude};
    while (remaining != 0) {
        limbs.push_back(static_cast<uint32_t>(remaining & 0xFFFFFFFFu));
        remaining >>= 32;
    }
    return from_magnitude(std::move(limbs), negative);
}

BigInt BigInt::shrink(BigInt big) {
    // 已经是小路径就原样返回（小路径对象的 limbs_/negative_ 恒为空/false）
    if (big.is_small_) return big;
    // 2 个 limb 已覆盖 int64_t 全部范围，更多 limb 的值必然装不下
    if (big.limbs_.size() > 2) return big;

    uint64_t magnitude{0};
    for (size_t i{big.limbs_.size()}; i-- > 0;) magnitude = (magnitude << 32) | big.limbs_[i];

    if (!big.negative_) {
        if (magnitude > static_cast<uint64_t>(INT64_MAX)) return big;
        BigInt result;
        result.is_small_ = true;
        result.small_ = static_cast<int64_t>(magnitude);
        return result;
    }

    constexpr uint64_t kMinMagnitude{static_cast<uint64_t>(INT64_MAX) + 1}; // |INT64_MIN| == 2^63
    if (magnitude > kMinMagnitude) return big;
    BigInt result;
    result.is_small_ = true;
    result.small_ = magnitude == kMinMagnitude ? INT64_MIN : -static_cast<int64_t>(magnitude);
    return result;
}

void BigInt::check_invariant() const {
    if (is_small_) return;
    assert(limbs_.empty() || limbs_.back() != 0); // 无多余最高位 0
    assert(!limbs_.empty() || !negative_);        // 值为 0 时不该带负号
    assert(!shrink(*this).is_small_);             // 大路径的量级不该是能收缩回小路径的
}

std::vector<uint32_t> BigInt::to_twos_complement(const size_t limb_count) const {
    assert(!is_small_);
    assert(limb_count > limbs_.size()); // 至少留一个 limb 的安全余量，见头文件

    std::vector<uint32_t> result(limb_count, 0);
    if (!negative_) {
        for (size_t i{0}; i < limbs_.size() && i < limb_count; ++i) result[i] = limbs_[i];
        return result; // 非负数：高位补 0
    }

    // 负数：结果 = ~magnitude + 1（在 limb_count * 32 位宽度内计算）
    uint64_t carry{1}; // "+1" 的初始进位
    for (size_t i{0}; i < limb_count; ++i) {
        const uint32_t magnitude_limb{i < limbs_.size() ? limbs_[i] : 0u};
        const uint64_t sum{static_cast<uint32_t>(~magnitude_limb) + carry};
        result[i] = static_cast<uint32_t>(sum);
        carry = sum >> 32;
    }
    return result;
}

BigInt BigInt::from_twos_complement(std::vector<uint32_t> limbs) {
    if (limbs.empty()) return BigInt{};

    const bool is_negative{((limbs.back() >> 31) & 1u) != 0}; // 最高位是符号位
    if (!is_negative) return from_magnitude(std::move(limbs), false);

    // 负数：magnitude = ~limbs + 1
    uint64_t carry{1};
    for (auto &limb : limbs) {
        const uint64_t sum{static_cast<uint32_t>(~limb) + carry};
        limb = static_cast<uint32_t>(sum);
        carry = sum >> 32;
    }
    return from_magnitude(std::move(limbs), true);
}

BigInt BigInt::bitwise_big(const BigInt &rhs, uint32_t (*const op)(uint32_t, uint32_t)) const {
    const BigInt a{promoted()};
    const BigInt b{rhs.promoted()};
    // 宽度取两边 limb 数的较大者 + 1，那个 +1 就是 to_twos_complement 要的安全 limb
    const size_t n{std::max(a.limbs_.size(), b.limbs_.size()) + 1};
    const std::vector<uint32_t> a_bits{a.to_twos_complement(n)};
    const std::vector<uint32_t> b_bits{b.to_twos_complement(n)};
    std::vector<uint32_t> result(n);
    for (size_t i{0}; i < n; ++i) result[i] = op(a_bits[i], b_bits[i]);
    return shrink(from_twos_complement(std::move(result)));
}

std::pair<BigInt, BigInt> BigInt::divmod_floor_big(const BigInt &divisor) const {
    assert(!is_small_);
    assert(!divisor.is_small_);

    // q_mag/r_mag 是 |*this| 除以 |divisor| 向零截断的商与余数，满足 0 <= r_mag < |divisor|
    auto [q_mag, r_mag]{div_mod_magnitude(limbs_, divisor.limbs_)};

    if (r_mag.empty()) {
        // 整除：截断和向负无穷取整结果一致
        return {from_magnitude(std::move(q_mag), negative_ != divisor.negative_), BigInt{}};
    }

    if (negative_ == divisor.negative_) {
        // 同号：向负无穷取整的商就是截断商（结果非负）；余数符号跟除数一致
        return {
            from_magnitude(std::move(q_mag), false),
            from_magnitude(std::move(r_mag), divisor.negative_)
        };
    }

    // 异号：向负无穷取整的商比截断商更小（更负）1；余数 = |divisor| - r_mag，符号跟除数一致
    BigInt quotient{from_magnitude(std::move(q_mag), false).add(BigInt(1)).minus()};
    BigInt remainder{from_magnitude(sub_magnitude(divisor.limbs_, r_mag), divisor.negative_)};
    return {std::move(quotient), std::move(remainder)};
}

BigInt::BigInt(const long long value) : small_{value} {}

BigInt BigInt::from_decimal_string(const std::string &s) {
    const auto is_digit{[](const char c) { return c >= '0' && c <= '9'; }};

    if (s.empty()) throw std::invalid_argument("BigInt::from_decimal_string: empty string");

    size_t i{0};
    bool neg{false};
    if (s[0] == '+' || s[0] == '-') {
        neg = s[0] == '-';
        i = 1;
    }

    const size_t digits_begin{i};
    while (i < s.size() && is_digit(s[i])) ++i;
    const size_t digits_end{i};
    if (digits_begin == digits_end)
        throw std::invalid_argument("BigInt::from_decimal_string: no digits");

    // 科学计数法后缀。指数只能非负：BigInt 是整数类型，`1e-9` 不是整数；`100e-1` 数值上虽是
    // 整数 10，同样不收——合不合法只看写法，不看算出来的值（同 SL.md 2.1.4 对字面量的规定）
    int64_t exponent{0};
    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
        ++i;
        if (i < s.size() && s[i] == '-')
            throw std::invalid_argument("BigInt::from_decimal_string: negative exponent");
        if (i < s.size() && s[i] == '+') ++i;

        const size_t exp_begin{i};
        while (i < s.size() && is_digit(s[i])) ++i;
        if (exp_begin == i)
            throw std::invalid_argument("BigInt::from_decimal_string: no exponent digits");
        // 先跳过前导 0，再按剩下的位数挡掉装不进 int64_t 的（19 位就可能溢出）。挡的是
        // "指数本身表示不了"，不是"指数太大算不动"——后者不设限，见头文件
        size_t exp_digits{exp_begin};
        while (exp_digits + 1 < i && s[exp_digits] == '0') ++exp_digits;
        if (i - exp_digits > 18)
            throw std::invalid_argument("BigInt::from_decimal_string: exponent out of range");
        for (size_t k{exp_digits}; k < i; ++k) exponent = exponent * 10 + (s[k] - '0');
    }
    if (i != s.size())
        throw std::invalid_argument("BigInt::from_decimal_string: invalid character");

    std::vector<uint32_t> limbs;
    for (size_t k{digits_begin}; k < digits_end; ++k) {
        // limbs = limbs * 10 + digit
        uint64_t carry{static_cast<uint64_t>(s[k] - '0')};
        for (auto &limb : limbs) {
            const uint64_t cur{static_cast<uint64_t>(limb) * 10 + carry};
            limb = static_cast<uint32_t>(cur);
            carry = cur >> 32;
        }
        if (carry) limbs.push_back(static_cast<uint32_t>(carry));
    }
    BigInt mantissa{shrink(from_magnitude(std::move(limbs), neg))};
    // 尾数为 0 时结果恒是 0，别去算 10^exponent：那一步的代价只跟指数走，"0e1000000" 会白算
    // 三秒多，而指数不设上限（见头文件），再大一档就是分钟级
    if (exponent == 0 || mantissa.is_zero()) return mantissa;
    // 乘 10^exponent，而不是先把零拼进数字串再解析：上面那个逐位 *10 的循环是 O(位数²)，
    // 同量级下比 pow 慢一个数量级（10^65536：138ms vs 11ms）
    return mantissa.mul(BigInt(10).pow(BigInt(exponent)));
}

std::string BigInt::to_decimal_string() const {
    if (is_small_) return std::to_string(small_);
    check_invariant(); // 大路径下不该是 0（那应该走小路径），否则下面 chunks.back() 是 UB

    // 每次除以 10^9 剥一段，低位在前
    std::vector magnitude{limbs_};
    std::vector<uint32_t> chunks;

    while (!magnitude.empty()) {
        uint64_t remainder{0};
        for (size_t i{magnitude.size()}; i-- > 0;) {
            constexpr uint64_t kChunkBase{1000000000ull};
            const uint64_t cur{(remainder << 32) | magnitude[i]};
            magnitude[i] = static_cast<uint32_t>(cur / kChunkBase);
            remainder = cur % kChunkBase;
        }
        while (!magnitude.empty() && magnitude.back() == 0) magnitude.pop_back();
        chunks.push_back(static_cast<uint32_t>(remainder));
    }

    std::string result;
    if (negative_) result.push_back('-');
    result += std::to_string(chunks.back()); // 最高位 chunk 不补零
    for (size_t i{chunks.size() - 1}; i-- > 0;) {
        const std::string chunk_str{std::to_string(chunks[i])};
        result += std::string(9 - chunk_str.size(), '0'); // 其余 chunk 一律补足 9 位
        result += chunk_str;
    }
    return result;
}

double BigInt::to_double() const {
    if (is_small_) return static_cast<double>(small_);
    check_invariant(); // 同 to_decimal_string，规范化的 0 不该走到这里

    const size_t bits{
        (limbs_.size() - 1) * 32 + static_cast<size_t>(std::bit_width(limbs_.back()))
    };

    uint64_t mantissa{0};
    int exponent{0};
    if (bits <= 64) {
        // 64 位内直接精确取值，转 double 只经历这一次舍入
        for (size_t i{limbs_.size()}; i-- > 0;) mantissa = (mantissa << 32) | limbs_[i];
    } else {
        // 只取最高 64 位；被舍弃的低位只要有一个非 0，就把 sticky 位或进最低位，
        // 让 uint64_t -> double 这一次舍入等价于直接对整个大数就近取偶
        const size_t drop_bits{bits - 64};
        for (size_t i{0}; i < 64; ++i) {
            const size_t bit_index{bits - 1 - i};
            mantissa = (mantissa << 1) | ((limbs_[bit_index / 32] >> (bit_index % 32)) & 1u);
        }
        bool sticky{false};
        const size_t full_limbs{drop_bits / 32}; // 按整 limb 判断非 0，不逐 bit 扫
        for (size_t limb_i{0}; limb_i < full_limbs && !sticky; ++limb_i)
            if (limbs_[limb_i] != 0) sticky = true;
        if (!sticky) {
            if (const size_t remaining_bits{drop_bits % 32}; remaining_bits != 0) {
                const uint32_t mask{(uint32_t{1} << remaining_bits) - 1};
                if ((limbs_[full_limbs] & mask) != 0) sticky = true;
            }
        }
        if (sticky) mantissa |= 1u;
        // 这么大的指数不管怎样 ldexp 都会溢出成 infinity，提前截断避免造出天文数字
        constexpr size_t kExponentClamp{100000};
        exponent = static_cast<int>(drop_bits < kExponentClamp ? drop_bits : kExponentClamp);
    }

    const double result{std::ldexp(static_cast<double>(mantissa), exponent)};
    return negative_ ? -result : result;
}

int BigInt::sign() const {
    if (is_zero()) return 0;
    return is_negative() ? -1 : 1;
}

size_t BigInt::num_decimal_digits() const {
    if (is_small_) {
        // 小路径逐次除 10 就够快，不必绕道字符串
        uint64_t magnitude{
            small_ < 0 ? static_cast<uint64_t>(-(small_ + 1)) + 1 : static_cast<uint64_t>(small_)
        };
        size_t digits{1};
        while (magnitude >= 10) {
            magnitude /= 10;
            ++digits;
        }
        return digits;
    }
    // 大路径没有比"真的转成十进制"更省的办法
    return to_decimal_string().size() - (negative_ ? 1 : 0);
}

size_t BigInt::bit_length() const {
    if (is_small_) {
        const uint64_t magnitude{
            small_ < 0 ? static_cast<uint64_t>(-(small_ + 1)) + 1 : static_cast<uint64_t>(small_)
        };
        return static_cast<size_t>(std::bit_width(magnitude));
    }
    check_invariant(); // 大路径下 limbs_ 不该是空的，否则下面 back() 是 UB
    return (limbs_.size() - 1) * 32 + static_cast<size_t>(std::bit_width(limbs_.back()));
}

BigInt BigInt::minus() const {
    if (is_small_) {
        // -INT64_MIN 溢出 int64_t，只能停留在大路径
        if (small_ == INT64_MIN) {
            BigInt result{promoted()};
            result.negative_ = false; // magnitude（2^63）不变，取负后应当是正的
            return result;
        }
        BigInt result;
        result.is_small_ = true;
        result.small_ = -small_;
        return result;
    }
    BigInt result{*this};
    if (!result.limbs_.empty()) result.negative_ = !result.negative_;
    // magnitude 恰好为 2^63 时取负后是 INT64_MIN，能装回小路径
    return shrink(std::move(result));
}

BigInt BigInt::abs() const {
    if (is_small_) {
        // |INT64_MIN| == 2^63，装不进 int64_t
        if (small_ == INT64_MIN) return shrink(promoted().abs());
        BigInt result;
        result.is_small_ = true;
        result.small_ = small_ < 0 ? -small_ : small_;
        return result;
    }
    // 量级本就超出 int64_t，取正后不变，不需要 shrink
    BigInt result{*this};
    result.negative_ = false;
    return result;
}

BigInt BigInt::bit_not() const { return minus().sub(BigInt(1)); }

BigInt BigInt::add(const BigInt &rhs) const {
    if (is_small_ && rhs.is_small_) {
        int64_t sum;
        if (!ckd_add(&sum, small_, rhs.small_)) {
            BigInt result;
            result.is_small_ = true;
            result.small_ = sum;
            return result;
        }
    }

    const BigInt a{promoted()};
    const BigInt b{rhs.promoted()};
    if (a.negative_ == b.negative_)
        return shrink(from_magnitude(add_magnitude(a.limbs_, b.limbs_), a.negative_));

    const auto cmp{compare_magnitude(a.limbs_, b.limbs_)};
    if (cmp > 0) return shrink(from_magnitude(sub_magnitude(a.limbs_, b.limbs_), a.negative_));
    if (cmp < 0) return shrink(from_magnitude(sub_magnitude(b.limbs_, a.limbs_), b.negative_));
    return BigInt{}; // 异号且大小相等，结果为 0
}

BigInt BigInt::sub(const BigInt &rhs) const { return add(rhs.minus()); }

BigInt BigInt::mul(const BigInt &rhs) const {
    if (is_small_ && rhs.is_small_) {
        int64_t product;
        if (!ckd_mul(&product, small_, rhs.small_)) {
            BigInt result;
            result.is_small_ = true;
            result.small_ = product;
            return result;
        }
    }

    const BigInt a{promoted()};
    const BigInt b{rhs.promoted()};
    return shrink(from_magnitude(mul_magnitude(a.limbs_, b.limbs_), a.negative_ != b.negative_));
}

BigInt BigInt::floor_div(const BigInt &divisor) const {
    if (divisor.is_zero()) throw std::domain_error("BigInt: division by zero");

    // 小路径快路径：原生截断除法 + 向负无穷修正。INT64_MIN / -1 会溢出（结果应是 2^63），
    // 这一种组合退回大路径
    if (is_small_ && divisor.is_small_ && !(small_ == INT64_MIN && divisor.small_ == -1)) {
        int64_t q{small_ / divisor.small_};
        const int64_t r{small_ % divisor.small_};
        if (r != 0 && ((r < 0) != (divisor.small_ < 0))) --q;
        BigInt result;
        result.is_small_ = true;
        result.small_ = q;
        return result;
    }

    return shrink(promoted().divmod_floor_big(divisor.promoted()).first);
}

BigInt BigInt::mod(const BigInt &divisor) const {
    if (divisor.is_zero()) throw std::domain_error("BigInt: division by zero");

    if (is_small_ && divisor.is_small_ && !(small_ == INT64_MIN && divisor.small_ == -1)) {
        int64_t r{small_ % divisor.small_};
        if (r != 0 && ((r < 0) != (divisor.small_ < 0))) r += divisor.small_;
        BigInt result;
        result.is_small_ = true;
        result.small_ = r;
        return result;
    }

    return shrink(promoted().divmod_floor_big(divisor.promoted()).second);
}

BigInt BigInt::pow(const BigInt &exponent) const {
    if (exponent.is_negative()) throw std::domain_error("BigInt::pow: negative exponent");

    // 逐位快速幂；exp 归零后不再平方（最后一轮是全过程中最贵的一次）
    BigInt result{1};
    BigInt base{*this};
    BigInt exp{exponent};
    while (!exp.is_zero()) {
        if (exp.is_odd()) result = result.mul(base);
        exp = exp.shift_right(1);
        if (exp.is_zero()) break;
        base = base.mul(base);
    }
    return result;
}

BigInt BigInt::bit_and(const BigInt &rhs) const {
    // 两个都在 int64_t 范围内时，补码位运算不会让量级变大，原生结果必然也在范围内
    if (is_small_ && rhs.is_small_) return BigInt(small_ & rhs.small_);
    return bitwise_big(rhs, [](const uint32_t x, const uint32_t y) { return x & y; });
}

BigInt BigInt::bit_or(const BigInt &rhs) const {
    if (is_small_ && rhs.is_small_) return BigInt(small_ | rhs.small_);
    return bitwise_big(rhs, [](const uint32_t x, const uint32_t y) { return x | y; });
}

BigInt BigInt::bit_xor(const BigInt &rhs) const {
    if (is_small_ && rhs.is_small_) return BigInt(small_ ^ rhs.small_);
    return bitwise_big(rhs, [](const uint32_t x, const uint32_t y) { return x ^ y; });
}

BigInt BigInt::shift_left(const long long k) const {
    if (k < 0) throw std::domain_error("BigInt::shift_left: negative shift count");
    if (k == 0 || is_zero()) return *this;

    // k <= 62 时 int64_t{1} << k 不碰符号位，可以安全地拿去做溢出检测的乘数
    if (is_small_ && k <= 62) {
        int64_t product;
        if (!ckd_mul(&product, small_, int64_t{1} << k)) return BigInt(product);
    }

    return shrink(from_magnitude(
        shift_left_magnitude(promoted().limbs_, static_cast<uint64_t>(k)), is_negative()
    ));
}

BigInt BigInt::shift_right(const long long k) const {
    if (k < 0) throw std::domain_error("BigInt::shift_right: negative shift count");
    if (k == 0 || is_zero()) return *this;

    if (is_small_) {
        // 移位数超过 63 时结果恒为符号位延伸：非负得 0，负数得 -1
        if (k >= 63) return BigInt(small_ < 0 ? -1 : 0);
        // C++20 起有符号整数算术右移是标准行为，恰好等价于向负无穷取整除 2^k
        return BigInt(small_ >> k);
    }

    // 大路径直接对大小右移（向零截断），负数在被移出的位里真有非 0 时把商再多减 1。
    // shift_right_magnitude 按整 limb 判断，多大的 k 都是 O(limb 数)，不需要再单独短路
    auto [truncated, dropped_nonzero]{shift_right_magnitude(limbs_, static_cast<uint64_t>(k))};
    if (!negative_ || !dropped_nonzero)
        return shrink(from_magnitude(std::move(truncated), negative_));
    return shrink(from_magnitude(std::move(truncated), false)).add(BigInt(1)).minus();
}

bool BigInt::equals(const BigInt &rhs) const {
    if (is_small_ && rhs.is_small_) return small_ == rhs.small_;
    check_invariant();
    rhs.check_invariant();
    // 按不变量，能装进 int64_t 的值必然走小路径
    if (is_small_ != rhs.is_small_) return false;
    return negative_ == rhs.negative_ && limbs_ == rhs.limbs_;
}

std::strong_ordering BigInt::compare_ordering(const BigInt &rhs) const {
    if (is_small_ && rhs.is_small_) return small_ <=> rhs.small_;

    // 不 promoted() 拷贝：先比符号；同号且一方是大路径时按不变量它的量级必然更大
    const bool a_neg{is_negative()}, b_neg{rhs.is_negative()};
    if (a_neg != b_neg) return a_neg ? std::strong_ordering::less : std::strong_ordering::greater;

    if (is_small_ != rhs.is_small_) {
        const bool this_is_bigger_magnitude{!is_small_};
        // 同为非负：量级越大值越大；同为负数反过来
        if (a_neg)
            return this_is_bigger_magnitude ? std::strong_ordering::less
                                            : std::strong_ordering::greater;
        return this_is_bigger_magnitude ? std::strong_ordering::greater
                                        : std::strong_ordering::less;
    }

    // 都是大路径、同号；同为负数时量级越大值越小，反过来比较参数顺序即可
    return a_neg ? compare_magnitude(rhs.limbs_, limbs_) : compare_magnitude(limbs_, rhs.limbs_);
}
