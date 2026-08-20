#pragma once

#include <cstdint>
#include <optional>
#include <utility>

#include "BigInt.h"

// decimal 超越函数（ln / log10 / exp / **）用的整数层定点算法，逐个对应 Python `_pydecimal`
// 里的同名辅助函数。这一层只跟整数打交道：算出"带 p 位精度、误差不超过 1 ulp 的近似值"，
// "要不要再多算几位才能定下舍入方向"由上层（BigDec）循环判断。
//
// 算法用定点整数模拟实数：z 用 round(z * M) 表示，M 通常取 10^p 或 2^k。误差界都是
// Python 那边论证过的，改这里任何常数（尤其 kTaylorL）都会拆掉上层"算到能定夺为止"的循环。
namespace dec_math {

// 泰勒展开前的参数缩减深度，Python 里 _ilog/_iexp 的默认参数 L
inline constexpr int64_t kTaylorL{8};

// 最接近 a/b 的整数，正好一半时取偶。调用方保证 b > 0（a 可以为负）
[[nodiscard]] BigInt div_nearest(const BigInt &a, const BigInt &b);

// 最接近 x / 2^shift 的整数，正好一半时取偶。调用方保证 shift >= 0（x 可以为负）
[[nodiscard]] BigInt rshift_nearest(const BigInt &x, int64_t shift);

// 最接近 sqrt(n) 的整数，a 是初始近似（任意正整数，越接近收敛越快）。调用方保证 n > 0、a > 0
[[nodiscard]] BigInt sqrt_nearest(const BigInt &n, BigInt a);

// M * log(x/M) 的整数近似。调用方保证 x > 0、M > 0；0.1 <= x/M <= 10 时误差不超过 22
[[nodiscard]] BigInt ilog(const BigInt &x, const BigInt &m);

// floor(10^p * log(10))。调用方保证 p >= 0。
// 内部有份只增不减的数字缓存，因此不是线程安全的（前端目前全程单线程）
[[nodiscard]] BigInt log10_digits(int64_t p);

// 10^p * log(c*10^e) 的整数近似，绝对误差不超过 1。调用方保证 c > 0 且 c*10^e != 1
[[nodiscard]] BigInt dlog(BigInt c, int64_t e, int64_t p);
// 同上，换成以 10 为底
[[nodiscard]] BigInt dlog10(BigInt c, int64_t e, int64_t p);

// M * exp(x/M) 的整数近似。调用方保证 M > 0 且 |x/M| 不大（0 <= x/M <= 2.4 时误差 < 60）
[[nodiscard]] BigInt iexp(const BigInt &x, const BigInt &m);

// 把 exp(c*10^e) 近似成 d*10^f，返回 (d, f)：10^(p-1) <= d <= 10^p，d 的误差不超过 1
[[nodiscard]] std::pair<BigInt, int64_t> dexp(const BigInt &c, int64_t e, int64_t p);

// 把 (xc*10^xe) ** (yc*10^ye) 近似成 c*10^e，返回 (c, e)：10^(p-1) <= c <= 10^p，
// c 的误差不超过 1。调用方保证底数为正且不等于 1、指数不为 0
[[nodiscard]] std::pair<BigInt, int64_t>
dpower(const BigInt &xc, int64_t xe, const BigInt &yc, int64_t ye, int64_t p);

// 100*log10(c) 的一个下界。调用方保证 c > 0
[[nodiscard]] int64_t log10_lb(const BigInt &c);

// n * 10^e，结果不是整数（e 为负且 n 末尾的 0 不够抵消）则返回 nullopt。
// 特意不先造 10^|e|（e 是个很大的负数时那一步会白白炸开）
[[nodiscard]] std::optional<BigInt> decimal_lshift_exact(const BigInt &n, int64_t e);

// 10^k。调用方保证 k >= 0
[[nodiscard]] BigInt pow10(int64_t k);

// x 落在 int64_t 里就取出来，否则 nullopt
[[nodiscard]] std::optional<int64_t> to_int64(const BigInt &x);

} // namespace dec_math
