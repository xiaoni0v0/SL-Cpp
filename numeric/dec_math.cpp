#include "dec_math.h"

#include <algorithm>
#include <cassert>
#include <string>

namespace dec_math {

namespace {

// ceil(a / b)。调用方保证 a >= 0、b > 0
int64_t ceil_div(const int64_t a, const int64_t b) { return (a + b - 1) / b; }

// 泰勒展开的项数：(2^L)^T > M 就够了
int64_t taylor_terms(const BigInt &m) {
    return ceil_div(10 * static_cast<int64_t>(m.num_decimal_digits()), 3 * kTaylorL);
}

} // namespace

BigInt pow10(const int64_t k) {
    assert(k >= 0);
    return BigInt(10).pow(BigInt(k));
}

std::optional<int64_t> to_int64(const BigInt &x) {
    // 位长卡在 62 而不是 63：63 位的值里只有 INT64_MIN 装得下，为这一个特例放宽不值当
    if (x.bit_length() > 62) return std::nullopt;
    // 能过上面那关的值必然在小路径上，to_decimal_string 就是一次 std::to_string
    return std::stoll(x.to_decimal_string());
}

BigInt div_nearest(const BigInt &a, const BigInt &b) {
    assert(!b.is_negative() && !b.is_zero());
    const BigInt q{a.floor_div(b)};
    const BigInt r{a - q * b}; // 向负无穷取整，因此恒有 0 <= r < b
    // 2r 超过 b 就该进位；正好等于 b（恰好一半）时看 q 的奇偶，奇数才进，也就是取偶
    return r * BigInt(2) + BigInt(q.is_odd() ? 1 : 0) > b ? q + BigInt(1) : q;
}

BigInt rshift_nearest(const BigInt &x, const int64_t shift) {
    assert(shift >= 0);
    const BigInt b{BigInt(1) << shift};
    const BigInt q{x >> shift};            // 向负无穷取整
    const BigInt low{x & (b - BigInt(1))}; // x mod 2^shift，按无穷位补码取，恒非负
    return low * BigInt(2) + BigInt(q.is_odd() ? 1 : 0) > b ? q + BigInt(1) : q;
}

BigInt sqrt_nearest(const BigInt &n, BigInt a) {
    assert(!n.is_negative() && !n.is_zero());
    assert(!a.is_negative() && !a.is_zero());
    BigInt b{0};
    while (a != b) {
        b = a;
        // 牛顿迭代 a = (a + ceil(n/a)) / 2。ceil 借向负无穷的除法写成 -((-n) // a)
        a = (a - (-n).floor_div(a)) >> 1;
    }
    return a;
}

BigInt ilog(const BigInt &x, const BigInt &m) {
    assert(!x.is_negative() && !x.is_zero());
    assert(!m.is_negative() && !m.is_zero());

    // 反复用 log1p(y) = 2*log1p(y/(1+sqrt(1+y))) 把 y 压到 2^-L 以内再上泰勒级数。
    // 全程定点：实数 z 用 z*M 的整数近似表示，y 存的是 2^R*y*M 的近似（R 是已缩减次数）
    BigInt y{x - m};
    int64_t r{0};
    while (true) {
        const BigInt magnitude{y.abs()};
        const bool too_big{
            r <= kTaylorL ? (magnitude << (kTaylorL - r)) >= m : (magnitude >> (r - kTaylorL)) >= m
        };
        if (!too_big) break;
        y = div_nearest((m * y) << 1, m + sqrt_nearest(m * (m + rshift_nearest(y, r)), m));
        ++r;
    }

    // 泰勒级数 log1p(y) ~ y - y^2/2 + y^3/3 - ...，按秦九韶的形式从最高次往回算
    const int64_t t{taylor_terms(m)};
    const BigInt y_shift{rshift_nearest(y, r)};
    BigInt w{div_nearest(m, BigInt(t))};
    for (int64_t k{t - 1}; k > 0; --k) w = div_nearest(m, BigInt(k)) - div_nearest(y_shift * w, m);
    return div_nearest(w * y, m);
}

BigInt log10_digits(const int64_t p) {
    assert(p >= 0);
    // 先存一段够用的 log(10)，不够了再往后算（这些位是截断的、恒正确，缓存只增不减）
    static std::string digits{"23025850929940456840179914546843642076011014886"};

    if (static_cast<size_t>(p) >= digits.size()) {
        // 一次多算 3 位，直到多出来的那几位不全是 0（全是 0 说明还没定下来）。
        // ilog 出来的位数只比 m 略少，不可能短到连 tail 位都不够——用 assert 把假设钉住
        int64_t extra{3};
        std::string computed;
        while (true) {
            const BigInt m{pow10(p + extra + 2)};
            computed = div_nearest(ilog(m * BigInt(10), m), BigInt(100)).to_decimal_string();
            const size_t tail{static_cast<size_t>(extra)};
            assert(computed.size() > tail);
            if (computed.size() > tail &&
                computed.substr(computed.size() - tail) != std::string(tail, '0'))
                break;
            extra += 3;
        }
        // 末尾的 0 连同紧挨着的那一位一起丢掉，剩下的都是可靠的
        size_t end{computed.size()};
        while (end > 0 && computed[end - 1] == '0') --end;
        assert(end > 1);
        digits = computed.substr(0, end - 1);
    }
    // 上面的扩容保证 digits 至少覆盖到 p 位
    assert(digits.size() > static_cast<size_t>(p));
    return BigInt::from_decimal_string(digits.substr(0, static_cast<size_t>(p) + 1));
}

BigInt dlog(BigInt c, const int64_t e, int64_t p) {
    assert(!c.is_negative() && !c.is_zero());
    p += 2; // 多算两位，最后除以 100 补偿回去

    // 把 c*10^e 写成 d*10^f：f >= 0 时 1 <= d <= 10，f <= 0 时 0.1 <= d <= 1。
    // 于是 10^p*log(c*10^e) = 10^p*log(d) + f*10^p*log(10)
    const int64_t l{static_cast<int64_t>(c.num_decimal_digits())};
    const int64_t f{e + l - (e + l >= 1 ? 1 : 0)};

    BigInt log_d{0};
    if (p > 0) {
        const int64_t k{e + p - f};
        if (k >= 0)
            c = c * pow10(k);
        else
            c = div_nearest(c, pow10(-k)); // c 里带进不超过 0.5 的误差
        log_d = ilog(c, pow10(p));         // ilog 最多把它放大十倍，总误差 < 5 + 22 = 27
    }
    // p <= 0 时整项按 0 算，误差 < 2.31

    BigInt f_log_ten{0};
    if (f != 0) {
        const int64_t extra{static_cast<int64_t>(BigInt(f).num_decimal_digits()) - 1};
        if (p + extra >= 0) // 误差 < |f|/10^extra + 0.5 < 11
            f_log_ten = div_nearest(BigInt(f) * log10_digits(p + extra), pow10(extra));
    }
    // 和的误差 < 11 + 27 = 38，除以 100 之后 < 0.38 + 0.5 < 1
    return div_nearest(f_log_ten + log_d, BigInt(100));
}

BigInt dlog10(BigInt c, const int64_t e, int64_t p) {
    assert(!c.is_negative() && !c.is_zero());
    p += 2;

    const int64_t l{static_cast<int64_t>(c.num_decimal_digits())};
    const int64_t f{e + l - (e + l >= 1 ? 1 : 0)};

    BigInt log_d{0};
    BigInt log_tenpower;
    if (p > 0) {
        const BigInt m{pow10(p)};
        const int64_t k{e + p - f};
        if (k >= 0)
            c = c * pow10(k);
        else
            c = div_nearest(c, pow10(-k));
        log_d = ilog(c, m);                              // 误差 < 27
        log_d = div_nearest(log_d * m, log10_digits(p)); // 换底，log(10) 自身误差 < 1
        log_tenpower = BigInt(f) * m;                    // 精确
    } else {
        log_tenpower = div_nearest(BigInt(f), pow10(-p)); // 误差 < 0.5
    }
    return div_nearest(log_tenpower + log_d, BigInt(100));
}

BigInt iexp(const BigInt &x, const BigInt &m) {
    assert(!m.is_negative() && !m.is_zero());

    // 先把 z = x/M 除以 2^R 压到 2^-L 以内，用泰勒级数算 expm1，
    // 再用 expm1(2z) = expm1(z)*(expm1(z)+2) 逐步倍回去
    const int64_t r{static_cast<int64_t>((x << kTaylorL).floor_div(m).bit_length())};
    const int64_t t{taylor_terms(m)};

    BigInt y{div_nearest(x, BigInt(t))};
    BigInt m_shift{m << r};
    for (int64_t i{t - 1}; i > 0; --i) y = div_nearest(x * (m_shift + y), m_shift * BigInt(i));
    for (int64_t k{r - 1}; k >= 0; --k) {
        m_shift = m << (k + 2);
        y = div_nearest(y * (y + m_shift), m_shift);
    }
    return m + y;
}

std::pair<BigInt, int64_t> dexp(const BigInt &c, const int64_t e, int64_t p) {
    p += 2; // 拿 M = 10^(p+2) 去调 iexp，也就是多算三位

    // log(10) 要跟着多算 c*10^e 的调整后指数那么多位。位数跟 Python 一样把负号也算进去
    // （它写 len(str(c))）——c 为负时只会多算一位、更保守，照抄以免跟参考实现分叉
    const int64_t c_len{static_cast<int64_t>(c.num_decimal_digits()) + (c.is_negative() ? 1 : 0)};
    const int64_t extra{std::max<int64_t>(0, e + c_len - 1)};
    const int64_t q{p + extra};

    // 算 c*10^e / log(10)，向下取整；商是 10 的幂次，余数留给 iexp
    const int64_t shift{e + q};
    const BigInt c_shift{shift >= 0 ? c * pow10(shift) : c.floor_div(pow10(-shift))};
    const BigInt log_ten{log10_digits(q)};
    const BigInt quot{c_shift.floor_div(log_ten)};
    const BigInt rem{div_nearest(c_shift - quot * log_ten, pow10(extra))};

    // 商的量级由上层的溢出/下溢粗筛保证落在 int64_t 里，装不下说明上层漏了一道闸
    return {div_nearest(iexp(rem, pow10(p)), BigInt(1000)), to_int64(quot).value() - p + 3};
}

std::pair<BigInt, int64_t>
dpower(const BigInt &xc, const int64_t xe, const BigInt &yc, const int64_t ye, const int64_t p) {
    // b 满足 10^(b-1) <= |y| <= 10^b
    const int64_t b{static_cast<int64_t>(yc.num_decimal_digits()) + ye};

    // log(x) = lxc * 10^(-p-b-1)，小数点后算到 p+b+1 位
    const BigInt lxc{dlog(xc, xe, p + b + 1)};

    // y*log(x) = yc*lxc*10^(-p-b-1+ye) = pc * 10^(-p-1)
    const int64_t shift{ye - b};
    // b 的定义是 digits(yc) + ye，所以 shift 恒等于 -digits(yc) <= -1；shift >= 0 这一支
    // 永远走不到，留着只是跟 Python 的写法（不假设这条恒等式）保持一致
    assert(shift < 0);
    const BigInt pc{shift >= 0 ? lxc * yc * pow10(shift) : div_nearest(lxc * yc, pow10(-shift))};

    if (pc.is_zero()) {
        // 结果贴着 1。这里特意给一个不正好等于 1 的近似值——上层要靠"末几位不是 5000…"判断
        // 能不能定下舍入方向，正好是 1 会让它永远判不出来
        const bool greater_than_one{
            (static_cast<int64_t>(xc.num_decimal_digits()) + xe >= 1) == !yc.is_negative()
        };
        if (greater_than_one) return {pow10(p - 1) + BigInt(1), 1 - p};
        return {pow10(p) - BigInt(1), -p};
    }

    auto [coeff, exp]{dexp(pc, -(p + 1), p + 1)};
    return {div_nearest(coeff, BigInt(10)), exp + 1};
}

int64_t log10_lb(const BigInt &c) {
    assert(!c.is_negative() && !c.is_zero());
    // 位数给出上界，再按最高位数字往回扣一点
    static constexpr int64_t kCorrection[]{0, 100, 70, 53, 40, 31, 23, 16, 10, 5};
    const std::string digits{c.to_decimal_string()};
    return 100 * static_cast<int64_t>(digits.size()) - kCorrection[digits[0] - '0'];
}

std::optional<BigInt> decimal_lshift_exact(const BigInt &n, const int64_t e) {
    if (n.is_zero()) return BigInt(0);
    if (e >= 0) return n * pow10(e);

    // n 末尾有几个 0，就最多能被 10 整除几次
    const std::string digits{n.abs().to_decimal_string()};
    size_t trailing{0};
    while (trailing < digits.size() && digits[digits.size() - 1 - trailing] == '0') ++trailing;
    if (static_cast<int64_t>(trailing) < -e) return std::nullopt;
    return n.floor_div(pow10(-e)); // 整除得尽，向负无穷取整跟精确除法一致
}

} // namespace dec_math
