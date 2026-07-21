#include "BigInt.h"

#include <stdexcept>
#include <utility>

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
    // 分两步算 magnitude，避免 small_ == INT64_MIN 时 -small_ 本身溢出 int64_t 的表示范围
    const uint64_t magnitude{
        negative
            ? static_cast<uint64_t>(-(small_ + 1)) + 1
            : static_cast<uint64_t>(small_)
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
    // 2 个 limb（64 位）已经能覆盖 int64_t 的全部表示范围，更多 limb 的值必然装不下，直接原样返回
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

std::strong_ordering BigInt::compare_magnitude(const std::vector<uint32_t> &a, const std::vector<uint32_t> &b) {
    if (a.size() != b.size()) return a.size() <=> b.size();
    for (size_t i{a.size()}; i-- > 0;) {
        if (a[i] != b[i]) return a[i] <=> b[i];
    }
    return std::strong_ordering::equal;
}

std::vector<uint32_t> BigInt::add_magnitude(const std::vector<uint32_t> &a, const std::vector<uint32_t> &b) {
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
    return result;
}

// 要求 a >= b（按 compare_magnitude），否则结果无意义（调用方保证）
std::vector<uint32_t> BigInt::sub_magnitude(const std::vector<uint32_t> &a, const std::vector<uint32_t> &b) {
    std::vector<uint32_t> result;
    result.reserve(a.size());
    int64_t borrow{0};
    for (size_t i{0}; i < a.size(); ++i) {
        int64_t diff{static_cast<int64_t>(a[i]) - borrow - (i < b.size() ? static_cast<int64_t>(b[i]) : 0)};
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

std::vector<uint32_t> BigInt::mul_magnitude(const std::vector<uint32_t> &a, const std::vector<uint32_t> &b) {
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

std::vector<uint32_t> BigInt::shift_left_magnitude(const std::vector<uint32_t> &a, const uint64_t bits) {
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

// 二进制逐位长除法：从最高位到最低位，边移边比较边减，是标准手算长除法的二进制版本。
// 不是渐进最优（Knuth Algorithm D 更快），但正确性显然、不需要处理"猜商偏大要修正"这类容易出错的细节。
std::pair<std::vector<uint32_t>, std::vector<uint32_t>> BigInt::div_mod_magnitude(
    const std::vector<uint32_t> &a, const std::vector<uint32_t> &b) {
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
            if (remainder.empty()) remainder.push_back(1);
            else remainder[0] |= 1u;
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

std::vector<uint32_t> BigInt::to_twos_complement(const size_t limb_count) const {
    std::vector<uint32_t> result(limb_count, 0);
    if (!negative_) {
        for (size_t i{0}; i < limbs_.size() && i < limb_count; ++i) result[i] = limbs_[i];
        return result; // 非负数：高位补 0
    }

    // 负数：结果 = ~magnitude + 1（在 limb_count * 32 位宽度内计算）；
    // 调用方需要保证 limb_count 足够容纳这次运算实际需要的位数，否则结果会被截断
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

std::pair<BigInt, BigInt> BigInt::divmod_floor_big(const BigInt &divisor) const {
    // 要求 *this、divisor 都已经是大路径（floor_div/mod 的小路径分支处理不了才会走到这里）
    auto [q_mag, r_mag]{div_mod_magnitude(limbs_, divisor.limbs_)};
    // q_mag = |*this| 除以 |divisor| 向零截断的商，r_mag = 对应余数，满足 0 <= r_mag < |divisor|

    if (r_mag.empty()) {
        // 整除：截断和向负无穷取整结果一致
        return {from_magnitude(std::move(q_mag), negative_ != divisor.negative_), BigInt{}};
    }

    if (negative_ == divisor.negative_) {
        // 同号：向负无穷取整的商就是截断商（结果非负）；余数符号跟除数一致
        return {from_magnitude(std::move(q_mag), false), from_magnitude(std::move(r_mag), divisor.negative_)};
    }

    // 异号：向负无穷取整的商比截断商更小（更负）1；余数 = |divisor| - r_mag，符号跟除数一致
    BigInt quotient{-(from_magnitude(std::move(q_mag), false) + BigInt(1))};
    BigInt remainder{from_magnitude(sub_magnitude(divisor.limbs_, r_mag), divisor.negative_)};
    return {std::move(quotient), std::move(remainder)};
}

BigInt::BigInt(const long long value) : is_small_{true}, small_{value} {
}

BigInt BigInt::from_decimal_string(const std::string &s) {
    if (s.empty()) throw std::invalid_argument("BigInt::from_decimal_string: empty string");

    size_t i{0};
    bool neg{false};
    if (s[0] == '+' || s[0] == '-') {
        neg = s[0] == '-';
        i = 1;
    }
    if (i >= s.size()) throw std::invalid_argument("BigInt::from_decimal_string: no digits");

    std::vector<uint32_t> limbs;
    for (; i < s.size(); ++i) {
        const char c{s[i]};
        if (c < '0' || c > '9') throw std::invalid_argument("BigInt::from_decimal_string: invalid character");

        // limbs = limbs * 10 + digit
        uint64_t carry{static_cast<uint64_t>(c - '0')};
        for (auto &limb : limbs) {
            const uint64_t cur{static_cast<uint64_t>(limb) * 10 + carry};
            limb = static_cast<uint32_t>(cur);
            carry = cur >> 32;
        }
        if (carry) limbs.push_back(static_cast<uint32_t>(carry));
    }
    return shrink(from_magnitude(std::move(limbs), neg));
}

std::string BigInt::to_decimal_string() const {
    if (is_small_) return std::to_string(small_);

    std::vector magnitude{limbs_};
    std::vector<uint32_t> chunks; // 每个 chunk 是 [0, 10^9) 内的一段十进制数字，低位在前

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

    double result{0.0};
    for (size_t i{limbs_.size()}; i-- > 0;) {
        result = result * 4294967296.0 + static_cast<double>(limbs_[i]);
    }
    return negative_ ? -result : result;
}

int BigInt::sign() const {
    if (is_zero()) return 0;
    return is_negative() ? -1 : 1;
}

BigInt BigInt::abs() const {
    if (is_small_) {
        if (small_ == INT64_MIN) return shrink(promoted().abs()); // |INT64_MIN| == 2^63，装不进 int64_t
        BigInt result;
        result.is_small_ = true;
        result.small_ = small_ < 0 ? -small_ : small_;
        return result;
    }
    BigInt result{*this};
    result.negative_ = false;
    return result;
}

BigInt BigInt::operator-() const {
    if (is_small_) {
        if (small_ == INT64_MIN) {
            // -INT64_MIN 溢出 int64_t（|INT64_MIN| == 2^63 > INT64_MAX），只能停留在大路径
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
    return result;
}

BigInt BigInt::operator~() const {
    return -(*this) - BigInt(1);
}

BigInt BigInt::operator+(const BigInt &rhs) const {
    if (is_small_ && rhs.is_small_) {
        int64_t sum;
        if (!__builtin_add_overflow(small_, rhs.small_, &sum)) {
            BigInt result;
            result.is_small_ = true;
            result.small_ = sum;
            return result;
        }
    }

    const BigInt a{promoted()};
    const BigInt b{rhs.promoted()};
    if (a.negative_ == b.negative_) return shrink(from_magnitude(add_magnitude(a.limbs_, b.limbs_), a.negative_));

    const auto cmp{compare_magnitude(a.limbs_, b.limbs_)};
    if (cmp > 0) return shrink(from_magnitude(sub_magnitude(a.limbs_, b.limbs_), a.negative_));
    if (cmp < 0) return shrink(from_magnitude(sub_magnitude(b.limbs_, a.limbs_), b.negative_));
    return BigInt{}; // 异号且大小相等，结果为 0
}

BigInt BigInt::operator-(const BigInt &rhs) const {
    return *this + (-rhs);
}

BigInt BigInt::operator*(const BigInt &rhs) const {
    if (is_small_ && rhs.is_small_) {
        int64_t product;
        if (!__builtin_mul_overflow(small_, rhs.small_, &product)) {
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

    // 小路径快路径：原生截断除法 + 向负无穷取整修正，唯一的坑是 INT64_MIN / -1 会溢出（结果本该是
    // 2^63，装不进 int64_t），这一种情况直接退回大路径，其余组合恒安全（两个 int64_t 相除/取模不会
    // 溢出）
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

    // 逐位快速幂：base/result 会随着乘法自然地按需从小路径升级到大路径，这里不用单独处理
    BigInt result{1};
    BigInt base{*this};
    BigInt exp{exponent};
    const BigInt two{2};
    while (!exp.is_zero()) {
        if (exp.is_odd()) result = result * base;
        base = base * base;
        exp = exp.floor_div(two); // exp 非负，等价于普通右移一位
    }
    return result;
}

BigInt BigInt::operator&(const BigInt &rhs) const {
    // 两个都在 int64_t 范围内时，原生按位与的结果显然也在范围内（补码位运算不会让量级变大），
    // 不需要经过 shrink
    if (is_small_ && rhs.is_small_) return BigInt(small_ & rhs.small_);

    // n 按"提升到大路径之后"的 limb 数来算：走小路径的操作数提升前 limbs_ 是空的（根本没用过），
    // 直接拿提升前的 limbs_.size() 参与 max 虽然巧合之下也不会出错（另一个操作数一定是真大数、
    // limb 数至少是 2，max 不会被那个 0 带偏），但依赖这个不太直观的不变量没必要，按提升后的算更直接
    const BigInt a{promoted()};
    const BigInt b{rhs.promoted()};
    const size_t n{(a.limbs_.size() > b.limbs_.size() ? a.limbs_.size() : b.limbs_.size()) + 1};
    const std::vector<uint32_t> a_bits{a.to_twos_complement(n)};
    const std::vector<uint32_t> b_bits{b.to_twos_complement(n)};
    std::vector<uint32_t> result(n);
    for (size_t i{0}; i < n; ++i) result[i] = a_bits[i] & b_bits[i];
    return shrink(from_twos_complement(std::move(result)));
}

BigInt BigInt::operator|(const BigInt &rhs) const {
    if (is_small_ && rhs.is_small_) return BigInt(small_ | rhs.small_);

    const BigInt a{promoted()};
    const BigInt b{rhs.promoted()};
    const size_t n{(a.limbs_.size() > b.limbs_.size() ? a.limbs_.size() : b.limbs_.size()) + 1};
    const std::vector<uint32_t> a_bits{a.to_twos_complement(n)};
    const std::vector<uint32_t> b_bits{b.to_twos_complement(n)};
    std::vector<uint32_t> result(n);
    for (size_t i{0}; i < n; ++i) result[i] = a_bits[i] | b_bits[i];
    return shrink(from_twos_complement(std::move(result)));
}

BigInt BigInt::operator^(const BigInt &rhs) const {
    if (is_small_ && rhs.is_small_) return BigInt(small_ ^ rhs.small_);

    const BigInt a{promoted()};
    const BigInt b{rhs.promoted()};
    const size_t n{(a.limbs_.size() > b.limbs_.size() ? a.limbs_.size() : b.limbs_.size()) + 1};
    const std::vector<uint32_t> a_bits{a.to_twos_complement(n)};
    const std::vector<uint32_t> b_bits{b.to_twos_complement(n)};
    std::vector<uint32_t> result(n);
    for (size_t i{0}; i < n; ++i) result[i] = a_bits[i] ^ b_bits[i];
    return shrink(from_twos_complement(std::move(result)));
}

BigInt BigInt::operator<<(const long long k) const {
    if (k < 0) throw std::domain_error("BigInt::operator<<: negative shift count");
    if (k == 0 || is_zero()) return *this;

    // k <= 62 时 int64_t{1} << k 本身不会碰到符号位，可以安全地拿去做溢出检测的乘数
    if (is_small_ && k <= 62) {
        int64_t product;
        if (!__builtin_mul_overflow(small_, int64_t{1} << k, &product)) return BigInt(product);
    }

    return shrink(from_magnitude(shift_left_magnitude(promoted().limbs_, static_cast<uint64_t>(k)), is_negative()));
}

BigInt BigInt::operator>>(const long long k) const {
    if (k < 0) throw std::domain_error("BigInt::operator>>: negative shift count");
    if (k == 0 || is_zero()) return *this;

    if (is_small_) {
        // 移位数超过 63 时更高位全是符号位延伸出来的结果：非负恒为 0，负数恒为 -1
        if (k >= 63) return BigInt(small_ < 0 ? -1 : 0);
        // C++20 起，有符号整数的算术右移是标准保证的行为，恰好等价于向负无穷取整除以 2^k
        return BigInt(small_ >> k);
    }

    // x >> k 恒等于 x // 2^k；floor_div/operator<< 自己会按需在两条路径间切换，这里直接复用即可
    return floor_div(BigInt(1) << k);
}

std::strong_ordering BigInt::operator<=>(const BigInt &rhs) const {
    if (is_small_ && rhs.is_small_) return small_ <=> rhs.small_;

    const BigInt a{promoted()};
    const BigInt b{rhs.promoted()};
    if (a.negative_ != b.negative_) return a.negative_ ? std::strong_ordering::less : std::strong_ordering::greater;
    // 同号：非负直接比大小；同为负数时，量级越大值越小，反过来比较参数顺序即可拿到正确结果
    return a.negative_ ? compare_magnitude(b.limbs_, a.limbs_) : compare_magnitude(a.limbs_, b.limbs_);
}

bool BigInt::operator==(const BigInt &rhs) const {
    if (is_small_ && rhs.is_small_) return small_ == rhs.small_;
    if (is_small_ != rhs.is_small_) return false; // 按不变量，能装进 int64_t 的值必然走小路径，不用再比较
    return negative_ == rhs.negative_ && limbs_ == rhs.limbs_;
}
