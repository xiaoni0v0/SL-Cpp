// 代数恒等式、路径规范化、bit_length、operator 与具名方法。
#include "test_utils.h"

#include <doctest/doctest.h>
#include <cstdlib>

// 边界值两两、三三组合，用恒等式本身当期望值。
TEST_SUITE("BigInt——代数恒等式交叉验证（覆盖小路径/大路径边界、多 limb、正负号组合）") {

    TEST_CASE("加法：交换律、结合律、加法逆元、幺元") {
        const auto vals{interesting_values()};
        for (const auto &a : vals) {
            CHECK((a + BigInt(0)) == a);
            CHECK((a + (-a)).is_zero());
            for (const auto &b : vals) {
                CHECK((a + b) == (b + a));
                CHECK((a - b) == -(b - a));
                for (const auto &c : vals) CHECK(((a + b) + c) == (a + (b + c)));
            }
        }
    }

    TEST_CASE("乘法：交换律、幺元、零元、跟加法的分配律") {
        const auto vals{interesting_values()};
        for (const auto &a : vals) {
            CHECK((a * BigInt(1)) == a);
            CHECK((a * BigInt(0)).is_zero());
            CHECK((a * BigInt(-1)) == -a);
            for (const auto &b : vals) {
                CHECK((a * b) == (b * a));
                for (const auto &c : vals) CHECK((a * (b + c)) == (a * b + a * c));
            }
        }
    }

    TEST_CASE("floor_div / mod：恒等式 a == (a // b) * b + a % b，且非 0 余数恒跟除数同号") {
        const auto vals{interesting_values()};
        for (const auto &a : vals) {
            for (const auto &b : vals) {
                if (b.is_zero()) continue;
                const BigInt q{a.floor_div(b)};
                const BigInt r{a.mod(b)};
                CHECK((q * b + r) == a);
                if (!r.is_zero()) CHECK(r.is_negative() == b.is_negative());
            }
        }
    }

    TEST_CASE("位运算：交换律、补码恒等式（a&~a==0、a|~a==-1、a^~a==-1、~~a==a）、分配律") {
        const auto vals{interesting_values()};
        for (const auto &a : vals) {
            CHECK((a & ~a).is_zero());
            CHECK((a | ~a) == d("-1"));
            CHECK((a ^ ~a) == d("-1"));
            CHECK(~(~a) == a);
            CHECK((a & d("-1")) == a);
            CHECK((a | d("0")) == a);
            CHECK((a ^ d("0")) == a);
            CHECK((a ^ a).is_zero());
            for (const auto &b : vals) {
                CHECK((a & b) == (b & a));
                CHECK((a | b) == (b | a));
                CHECK((a ^ b) == (b ^ a));
                // a & (b | ~b) == a & (-1) == a，所以 (a&b) | (a&~b) 应该恒等于 a
                CHECK(((a & b) | (a & ~b)) == a);
                // De Morgan
                CHECK(~(a & b) == (~a | ~b));
                CHECK(~(a | b) == (~a & ~b));
                // 移位对位运算的分配律：先移位再算，跟先算再移位应该一样
                for (const long long k : {0LL, 1LL, 31LL, 32LL, 65LL}) {
                    CHECK(((a << k) & (b << k)) == ((a & b) << k));
                    CHECK(((a << k) | (b << k)) == ((a | b) << k));
                    CHECK(((a << k) ^ (b << k)) == ((a ^ b) << k));
                }
                for (const auto &c : vals) CHECK((a & (b | c)) == ((a & b) | (a & c)));
            }
        }
    }

    TEST_CASE(
        "位运算跟一个完全独立的参考实现交叉验证：靠 floor_div(2)/mod(2) 逐位剥补码位，"
        "这条路径跟 to_twos_complement 的实现完全无关，能提供恒等式测不出来的新信息"
    ) {
        // 逐位剥补码位：非负数最终收敛到 0，负数（无穷位补码下全是 1）最终收敛到 -1，
        // 之后更高位就是恒定的 0/-1，不用再继续剥
        const auto reference_bitwise{[](BigInt a, BigInt b, const char op) {
            BigInt result{0};
            const BigInt two{2};
            BigInt weight{1};
            while (!(a.is_zero() || a == BigInt(-1)) || !(b.is_zero() || b == BigInt(-1))) {
                const bool abit{a.mod(two) != BigInt(0)};
                const bool bbit{b.mod(two) != BigInt(0)};
                bool rbit;
                switch (op) {
                case '&':
                    rbit = abit && bbit;
                    break;
                case '|':
                    rbit = abit || bbit;
                    break;
                default:
                    rbit = abit != bbit;
                    break; // '^'
                }
                if (rbit) result = result + weight;
                a = a.floor_div(two);
                b = b.floor_div(two);
                weight = weight * two;
            }
            // 剩下的高位是恒定的 0/-1，直接按同样的规则算一次，非 0（即恒为 1）就再减一个位权
            // （无穷个 1 从这个位权往上延伸，等价于减去这个位权——补码"全 1 尾巴"的标准技巧）
            const bool a_hi{a.is_negative()}, b_hi{b.is_negative()};
            bool r_hi;
            switch (op) {
            case '&':
                r_hi = a_hi && b_hi;
                break;
            case '|':
                r_hi = a_hi || b_hi;
                break;
            default:
                r_hi = a_hi != b_hi;
                break;
            }
            if (r_hi) result = result - weight;
            return result;
        }};

        const auto vals{interesting_values()};
        const std::vector<BigInt> probes{d("0"), d("-1"), d("5"), d("-5"), d("255"), d("-256")};
        for (const auto &a : vals) {
            for (const auto &b : probes) {
                CAPTURE(a.to_decimal_string());
                CAPTURE(b.to_decimal_string());
                CHECK((a & b) == reference_bitwise(a, b, '&'));
                CHECK((a | b) == reference_bitwise(a, b, '|'));
                CHECK((a ^ b) == reference_bitwise(a, b, '^'));
            }
        }
    }

    TEST_CASE("移位：(a << k) >> k 精确恢复原值（左移是精确乘法，右移向下取整但除得尽）") {
        const auto vals{interesting_values()};
        for (const auto &a : vals) {
            for (const long long k : {0LL, 1LL, 5LL, 31LL, 32LL, 33LL, 63LL, 64LL, 65LL, 100LL}) {
                CHECK(((a << k) >> k) == a);
            }
        }
    }

    TEST_CASE("移位：(a << m) << n == a << (m + n)") {
        const auto vals{interesting_values()};
        for (const auto &a : vals) {
            for (const long long m : {0LL, 3LL, 32LL, 64LL}) {
                for (const long long n : {0LL, 5LL, 31LL, 33LL}) {
                    CHECK(((a << m) << n) == (a << (m + n)));
                }
            }
        }
    }

    TEST_CASE("pow：x^(m+n) == x^m * x^n，x^1 == x，x^0 == 1，覆盖正负底数、大路径底数") {
        for (const auto &base :
             {d("2"), d("-2"), d("3"), d("-7"), d("123456789012345678901234567890")}) {
            CHECK(base.pow(d("1")) == base);
            CHECK(base.pow(d("0")) == d("1"));
            for (const auto &[m, n] :
                 {std::pair{d("3"), d("4")},
                  std::pair{d("0"), d("5")},
                  std::pair{d("10"), d("10")}}) {
                CHECK(base.pow(m + n) == (base.pow(m) * base.pow(n)));
            }
        }
    }

    TEST_CASE(
        "pow：-1 的幂按奇偶交替，1 的幂恒为 1，即使指数很大也一样（验证走的是对数次幂算法）"
    ) {
        CHECK(d("-1").pow(d("1000000")) == d("1"));  // 偶数次幂
        CHECK(d("-1").pow(d("1000001")) == d("-1")); // 奇数次幂
        CHECK(d("1").pow(d("1000000")) == d("1"));
    }

    TEST_CASE("比较：三分性（< / > / == 恰好一个成立）、-a > -b <=> a < b、传递性") {
        const auto vals{interesting_values()};
        for (const auto &a : vals) {
            for (const auto &b : vals) {
                const int lt{a < b ? 1 : 0}, gt{a > b ? 1 : 0}, eq{a == b ? 1 : 0};
                CHECK(lt + gt + eq == 1);
                CHECK((a < b) == (-a > -b));
                CHECK((a <= b) == (b >= a));
                for (const auto &c : vals) {
                    if (a < b && b < c) CHECK(a < c);
                }
            }
        }
    }
}
TEST_SUITE("BigInt——小路径/大路径规范化：同一值经不同运算路径算出来必须 ==") {

    TEST_CASE("恰好卡在 int64_t 边界上的一批值，分别通过好几条不同的运算路径算出来，两两都要相等") {
        for (const std::string &target : {
                 std::string("0"),
                 std::string("1"),
                 std::string("-1"),
                 std::string("100"),
                 std::string("-100"),
                 std::string("9223372036854775807"),  // INT64_MAX
                 std::string("-9223372036854775808"), // INT64_MIN
                 std::string("9223372036854775806"),
                 std::string("-9223372036854775807"),
                 std::string("4294967295"),
                 std::string("-4294967295"),
             }) {
            const BigInt direct{d(target)};

            CHECK((direct + BigInt(1) - BigInt(1)) == direct);
            CHECK((-(-direct)) == direct);
            CHECK((direct * BigInt(1)) == direct);
            CHECK((direct & d("-1")) == direct);
            CHECK((direct | d("0")) == direct);
            CHECK(((direct << 5) >> 5) == direct);
            CHECK(direct.floor_div(BigInt(1)) == direct);
            if (direct.is_negative()) CHECK((-direct.abs()) == direct);

            // 不只是数值相等（==），字符串表示也要完全一致，防止出现"值相等但输出不一致"这种
            // 更隐蔽的规范化问题
            CHECK((direct + BigInt(1) - BigInt(1)).to_decimal_string() == target);
            CHECK((-(-direct)).to_decimal_string() == target);
        }
    }

    TEST_CASE(
        "除法商恰好落在 int64_t 范围内、但被除数/除数都远超范围：异号/同号/整除交叉组合"
        "（shrink() 那次 bug 的完整复现场景，已在 floor_div/mod 套件里更细地测过一遍，这里"
        "重点确认收缩后能正常跟直接构造的小路径同值相等）"
    ) {
        struct Case {
            std::string x, y, q;
        };
        for (const Case &c : {
                 Case{"-300000000000000000007", "100000000000000000000", "-4"},
                 Case{"300000000000000000007", "-100000000000000000000", "-4"},
                 Case{"-300000000000000000000", "100000000000000000000", "-3"},
                 Case{"-100000000000000000001", "100000000000000000000", "-2"},
                 Case{"100000000000000000001", "-100000000000000000000", "-2"},
             }) {
            const BigInt x{d(c.x)}, y{d(c.y)}, expected_q{d(c.q)};
            const BigInt q{x.floor_div(y)};
            CHECK(q == expected_q);
            CHECK_FALSE(q != expected_q); // 反过来也测一遍 !=，双保险
            CHECK(q.to_decimal_string() == expected_q.to_decimal_string());
        }
    }
}
TEST_SUITE("BigInt——bit_length 与 to_double 的窄路径") {

    TEST_CASE("bit_length：小路径、大路径、边界") {
        CHECK(d("0").bit_length() == 0);
        CHECK(d("1").bit_length() == 1);
        CHECK(d("-1").bit_length() == 1); // 只看量级，不管符号
        CHECK(d("255").bit_length() == 8);
        CHECK(d("256").bit_length() == 9);
        CHECK(d("9223372036854775807").bit_length() == 63);  // INT64_MAX
        CHECK(d("-9223372036854775808").bit_length() == 64); // |INT64_MIN| == 2^63
        // 下面这些必然走大路径（装不进 int64_t），是 limbs 那一支
        CHECK(d("9223372036854775808").bit_length() == 64);  // 2^63
        CHECK(d("18446744073709551615").bit_length() == 64); // 2^64 - 1
        CHECK(d("18446744073709551616").bit_length() == 65); // 2^64
        CHECK(d("-18446744073709551616").bit_length() == 65);
        for (int shift{60}; shift < 200; ++shift) {
            CAPTURE(shift);
            CHECK((BigInt(1) << shift).bit_length() == static_cast<size_t>(shift) + 1);
            CHECK(((BigInt(1) << shift) - BigInt(1)).bit_length() == static_cast<size_t>(shift));
        }
    }

    TEST_CASE("num_decimal_digits：具体数值直接钉住") {
        CHECK(d("0").num_decimal_digits() == 1); // 0 算 1 位
        CHECK(d("-0").num_decimal_digits() == 1);
        CHECK(d("7").num_decimal_digits() == 1);
        CHECK(d("-7").num_decimal_digits() == 1); // 负号不算位数
        CHECK(d("99").num_decimal_digits() == 2);
        CHECK(d("100").num_decimal_digits() == 3);
        CHECK(d("9223372036854775807").num_decimal_digits() == 19);  // INT64_MAX，小路径边界
        CHECK(d("-9223372036854775808").num_decimal_digits() == 19); // INT64_MIN
        // 下面这些必然走大路径
        CHECK(d("9223372036854775808").num_decimal_digits() == 19);  // 2^63
        CHECK(d("18446744073709551615").num_decimal_digits() == 20); // 2^64 - 1
        CHECK(d("18446744073709551616").num_decimal_digits() == 20); // 2^64
        CHECK(d("123456789012345678901234567890").num_decimal_digits() == 30);
        CHECK(d("-123456789012345678901234567890").num_decimal_digits() == 30);
    }

    TEST_CASE("bit_length 跟十进制位数彼此印证") {
        // b 位的数落在 [2^(b-1), 2^b)，于是十进制位数 digits 必然满足
        // (b-1)*log10(2) < digits <= b*log10(2) + 1，拿它把两个函数互相钉住
        constexpr double kLog10Of2{0.30102999566398119521};
        for (const BigInt &x : interesting_values()) {
            CAPTURE(x.to_decimal_string());
            if (x.is_zero()) continue;
            const auto bits{static_cast<double>(x.bit_length())};
            const auto digits{static_cast<double>(x.num_decimal_digits())};
            CHECK((bits - 1.0) * kLog10Of2 < digits);
            CHECK(digits <= bits * kLog10Of2 + 1.0);
        }
    }

    TEST_CASE("to_double 把整个值池灌给 strtod 对拍（含 [2^63, 2^64) 那段精确路径）") {
        // 含 [2^63, 2^64) 那段大路径但 64 位内装得下的精确路径
        for (const BigInt &x : interesting_values()) {
            const std::string s{x.to_decimal_string()};
            CAPTURE(s);
            CHECK(x.to_double() == std::strtod(s.c_str(), nullptr));
        }
    }
}
TEST_SUITE("BigInt——operator 与同名具名方法等价") {

    TEST_CASE("一元、二元、位运算、移位、比较：两种写法逐个对齐") {
        const auto vals{interesting_values()};
        for (const auto &a : vals) {
            CAPTURE(a.to_decimal_string());
            CHECK((+a) == a.plus());
            CHECK((-a) == a.minus());
            CHECK((~a) == a.bit_not());
            for (const long long k : {0LL, 1LL, 31LL, 64LL, 100LL}) {
                CAPTURE(k);
                CHECK((a << k) == a.shift_left(k));
                CHECK((a >> k) == a.shift_right(k));
            }
            for (const auto &b : vals) {
                CHECK((a + b) == a.add(b));
                CHECK((a - b) == a.sub(b));
                CHECK((a * b) == a.mul(b));
                CHECK((a & b) == a.bit_and(b));
                CHECK((a | b) == a.bit_or(b));
                CHECK((a ^ b) == a.bit_xor(b));
                CHECK((a == b) == a.equals(b));
                CHECK((a <=> b) == a.compare_ordering(b));
            }
        }
    }
}
