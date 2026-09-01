// 位运算（无穷位补码）、小/大路径切换、移位。
#include "test_utils.h"

#include <doctest/doctest.h>
#include <stdexcept>

TEST_SUITE("BigInt——位运算：按无穷位补码语义，与 Python 一致") {

    TEST_CASE("~5 == -6；-1 & 255 == 255；-1 >> 100 == -1") {
        CHECK((~d("5")).to_decimal_string() == "-6");
        CHECK((d("-1") & d("255")).to_decimal_string() == "255"); // -1 的所有位都是 1
        CHECK((d("-1") >> 100).to_decimal_string() == "-1");      // 无论右移多少位，结果恒为 -1
    }

    TEST_CASE("~x 恒等于 -x - 1") {
        CHECK((~d("0")).to_decimal_string() == "-1");
        CHECK((~d("-1")).to_decimal_string() == "0");
        CHECK((~d("100")).to_decimal_string() == "-101");
        CHECK((~d("-100")).to_decimal_string() == "99");
        // INT64_MIN 取负会升级到大路径：~(-2^63) == 2^63 - 1 == INT64_MAX
        CHECK((~d("-9223372036854775808")).to_decimal_string() == "9223372036854775807");
        CHECK((~d("9223372036854775807")).to_decimal_string() == "-9223372036854775808");
    }

    TEST_CASE("& 全正数：跟普通按位与一致") {
        CHECK((d("12") & d("10")).to_decimal_string() == "8"); // 1100 & 1010 == 1000
    }

    TEST_CASE("& / | / ^ 涉及负数，按补码语义验证") {
        // -1 的补码全 1，跟任何数相与得那个数本身，相或恒得 -1，相异或恒得该数按位取反
        CHECK((d("-1") & d("12345")).to_decimal_string() == "12345");
        CHECK((d("-1") | d("12345")).to_decimal_string() == "-1");
        CHECK((d("-1") ^ d("5")) == ~d("5"));

        // -1 & -1 == -1；-2 的补码 ...11110，与 -1 相或还是 -1
        CHECK((d("-1") & d("-1")).to_decimal_string() == "-1");
        CHECK((d("-2") | d("-1")).to_decimal_string() == "-1");

        // 跟 Python 手算核对过的几组：(-5) & 3 == 3；(-5) | 3 == -5；(-5) ^ 3 == -8
        CHECK((d("-5") & d("3")).to_decimal_string() == "3");
        CHECK((d("-5") | d("3")).to_decimal_string() == "-5");
        CHECK((d("-5") ^ d("3")).to_decimal_string() == "-8");
    }

    TEST_CASE("跨 limb 边界的位运算") {
        // 2^32 - 1（恰好占满第 1 个 limb）跟 2^32（恰好落在第 2 个 limb）相或，应该是两段拼起来
        CHECK((d("4294967295") | d("4294967296")).to_decimal_string() == "8589934591"); // 2^33 - 1
        CHECK((d("4294967295") & d("4294967296")).to_decimal_string() == "0"); // 高低位不重叠
    }

    TEST_CASE(
        "补码安全余量：正数的最高 limb 恰好全是 1 比特时，不能被误读成符号位（to_twos_complement "
        "的 '+1' 余量就是为这个存在的）"
    ) {
        // 4294967295 == 2^32 - 1，单 limb 全 1（0xFFFFFFFF），本身是正数；如果没有那 1 个 limb
        // 的安全余量，会被误当成 -1 的补码表示
        CHECK((d("4294967295") & d("-1")).to_decimal_string() == "4294967295");
        CHECK((d("4294967295") | d("0")).to_decimal_string() == "4294967295");
        CHECK((d("4294967295") ^ d("0")).to_decimal_string() == "4294967295");
        CHECK((d("4294967295") & d("4294967295")).to_decimal_string() == "4294967295");
        // 18446744073709551615 == 2^64 - 1，两个 limb 都全 1，同样的坑，双 limb 版本
        CHECK((d("18446744073709551615") & d("-1")).to_decimal_string() == "18446744073709551615");
        CHECK((d("18446744073709551615") | d("0")).to_decimal_string() == "18446744073709551615");
    }
}
TEST_SUITE("BigInt——小路径/大路径边界（内部按 int64_t 能不能装得下自动切换，这里专测切换点）") {

    TEST_CASE("加法：恰好在 int64_t 边界上下的进退位") {
        CHECK(
            (d("9223372036854775807") + d("1")).to_decimal_string() == "9223372036854775808"
        ); // INT64_MAX + 1
        CHECK(
            (d("-9223372036854775808") - d("1")).to_decimal_string() == "-9223372036854775809"
        ); // INT64_MIN - 1
        // 反过来：从大路径的值退回小路径也要算对
        CHECK((d("9223372036854775808") - d("1")).to_decimal_string() == "9223372036854775807");
    }

    TEST_CASE("乘法：两个能装进 int64_t 的数相乘溢出，自动转大路径") {
        CHECK((d("9223372036854775807") * d("2")).to_decimal_string() == "18446744073709551614");
    }

    TEST_CASE("一元负号 / abs：INT64_MIN 取负/取绝对值都会溢出 int64_t，退到大路径") {
        CHECK((-d("-9223372036854775808")).to_decimal_string() == "9223372036854775808");
        CHECK(d("-9223372036854775808").abs().to_decimal_string() == "9223372036854775808");
        // 反过来，大路径的值取负后如果能装回 int64_t，应该退回小路径（用后续运算能算对来间接验证）
        CHECK((-d("9223372036854775808") + d("1")).to_decimal_string() == "-9223372036854775807");
    }

    TEST_CASE(
        "floor_div / mod：INT64_MIN / -1 这个组合原生 int64_t 除法本身会溢出，必须退到大路径"
    ) {
        CHECK(
            d("-9223372036854775808").floor_div(d("-1")).to_decimal_string() ==
            "9223372036854775808"
        );
        CHECK(d("-9223372036854775808").mod(d("-1")).to_decimal_string() == "0");
    }

    TEST_CASE("小路径/大路径操作数混着算，结果不受影响") {
        const BigInt small{d("100")};
        const BigInt big{d("123456789012345678901234567890")};
        CHECK((small + big).to_decimal_string() == "123456789012345678901234567990");
        CHECK((big - small).to_decimal_string() == "123456789012345678901234567790");
        CHECK((small * big).to_decimal_string() == "12345678901234567890123456789000");
        CHECK(big.mod(small).to_decimal_string() == "90"); // ...67890 mod 100
        CHECK((small & big) == (big & small)); // & 满足交换律，顺便测两种操作数顺序都能走对应分支
        CHECK(small < big);
        CHECK(big > small);
    }

    TEST_CASE("从超出 int64_t 范围的十进制字符串解析，再做退回小路径范围的运算") {
        const BigInt huge{d("100000000000000000000")}; // 10^20，远超 int64_t
        CHECK(
            (huge - d("99999999999999999995")).to_decimal_string() == "5"
        ); // 结果退回到小路径范围
        CHECK((huge - huge).to_decimal_string() == "0");
    }

    TEST_CASE(
        "一元负号：+2^63 恰好是大路径下的合法值（正数装不下 int64_t），但取负后的 -2^63 == "
        "INT64_MIN 恰好又能装回小路径——这是曾经真实存在过的 bug：operator-() 的大路径分支只翻了"
        "符号位，没有过 shrink()，导致算出来的 -x 停留在非规范的大路径状态，跟直接构造的 "
        "INT64_MIN 用 == 比较会被误判为不等（被后面的代数恒等式套件测出来的）"
    ) {
        const BigInt positive_two_pow_63{d("9223372036854775808")}; // 2^63，只能是大路径
        const BigInt negated{-positive_two_pow_63};
        CHECK(negated.to_decimal_string() == "-9223372036854775808");
        CHECK(negated == d("-9223372036854775808"));          // 必须能跟直接构造的小路径同值相等
        CHECK(negated == BigInt(-9223372036854775807LL - 1)); // 也要跟 long long 构造的相等
        CHECK((-negated) == positive_two_pow_63);             // 再取负一次应该精确复原
        // abs() 走的是不同的代码路径（只清 negative_，不需要 shrink，见 BigInt.cpp
        // 里的注释），顺带交叉验证一下两条路径不会互相矛盾
        CHECK(negated.abs() == positive_two_pow_63);
    }
}
TEST_SUITE("BigInt——移位：<< 恒等于乘 2^k，>> 恒等于向负无穷取整除 2^k") {

    TEST_CASE("正数移位") {
        CHECK((d("1") << 10).to_decimal_string() == "1024");
        CHECK((d("1024") >> 10).to_decimal_string() == "1");
        CHECK((d("5") << 0).to_decimal_string() == "5");
        CHECK((d("5") >> 0).to_decimal_string() == "5");
    }

    TEST_CASE("负数右移恒为 -1") {
        CHECK((d("-1") >> 100).to_decimal_string() == "-1");
        CHECK((d("-1") >> 1).to_decimal_string() == "-1");
        CHECK((d("-1") >> 0).to_decimal_string() == "-1");
    }

    TEST_CASE("负数左移，等价于乘 2^k") {
        CHECK((d("-1") << 3).to_decimal_string() == "-8");
        CHECK((d("-3") << 2).to_decimal_string() == "-12");
    }

    TEST_CASE("负数右移非平凡情况，跟 floor_div(2^k) 结果一致") {
        CHECK((d("-7") >> 1).to_decimal_string() == "-4"); // floor(-7/2) == -4
        CHECK((d("-7") >> 1) == d("-7").floor_div(d("2")));
    }

    TEST_CASE("跨 limb 的大位移") {
        CHECK((d("1") << 64).to_decimal_string() == "18446744073709551616"); // 2^64
        CHECK((d("18446744073709551616") >> 64).to_decimal_string() == "1");
    }

    TEST_CASE("负的移位位数抛 std::domain_error") {
        CHECK_THROWS_AS((void) (d("1") << -1), std::domain_error);
        CHECK_THROWS_AS((void) (d("1") >> -1), std::domain_error);
    }

    TEST_CASE(
        "大路径操作数的超大位移：位移数超过数值本身的比特长度时必须 O(1) 短路，不能真的去构造 "
        "2^k 这个除数（曾经的 bug：大路径分支没有这条短路，构造 2^k 这一步本身就会撑爆内存/耗时）"
    ) {
        const BigInt x{(BigInt(1) << 128) - BigInt(1)}; // 2^128 - 1，128 位，全 1
        CHECK((x >> 1000).is_zero());
        CHECK((x >> 1000000000LL).is_zero()); // 移位数十亿级，没短路的话会直接卡死/炸内存
        CHECK((-x >> 1000).to_decimal_string() == "-1");
        CHECK((-x >> 1000000000LL).to_decimal_string() == "-1");
        // 恰好等于/前后 1 位的比特长度边界
        CHECK((x >> 127).to_decimal_string() == "1"); // 还剩最高 1 位
        CHECK((x >> 128).is_zero());
        CHECK((x >> 129).is_zero());
    }

    TEST_CASE(
        "直接对着定义验证，而不只是靠 << >> 互相抵消这条弱性质："
        "a << k == a * 2^k，a >> k == a.floor_div(2^k)"
    ) {
        const auto vals{interesting_values()};
        for (const auto &a : vals) {
            for (const long long k : {0LL, 1LL, 5LL, 31LL, 32LL, 63LL, 64LL, 100LL, 200LL}) {
                const BigInt two_pow_k{BigInt(1) << k};
                CHECK((a << k) == (a * two_pow_k));
                CHECK((a >> k) == a.floor_div(two_pow_k));
            }
        }
    }
}
