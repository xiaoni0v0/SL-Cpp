// BigInt：任意精度有符号整数。语义细节（// 向负无穷取整、位运算按无穷位补码）见 SL.md 3.4.2。
#include "../../numeric/BigInt.h"

#include <doctest/doctest.h>

#include <cmath>
#include <cstdlib>
#include <stdexcept>

namespace {
BigInt d(const std::string &s) { return BigInt::from_decimal_string(s); }

// 覆盖各种"容易出 bug"的数据，供后面的恒等式交叉验证批量使用：0/±1、int64_t 边界内外、
// 恰好卡在 shrink() 判定边界上的值（2^63 附近）、全 1 比特的 limb（bitwise
// 安全余量最容易翻车的地方）、多 limb 的超大数
std::vector<BigInt> interesting_values() {
    // 2^128-1、2^192-1 靠移位+减法现算，不手抄几十位的十进制常量（这两个运算本身已经被前面
    // 的套件独立测过，拿来当值池的"生成器"是安全的）
    const BigInt two_pow_128_minus_1{(BigInt(1) << 128) - BigInt(1)};
    const BigInt two_pow_192_minus_1{(BigInt(1) << 192) - BigInt(1)};
    return {
        d("0"),
        d("1"),
        d("-1"),
        d("2"),
        d("-2"),
        d("100"),
        d("-100"),
        d("2147483647"),            // 2^31 - 1，limb 内符号位边界
        d("2147483648"),            // 2^31
        d("2147483649"),            // 2^31 + 1
        d("-2147483648"),           // -2^31
        d("9223372036854775807"),   // INT64_MAX
        d("9223372036854775806"),   // INT64_MAX - 1
        d("-9223372036854775808"),  // INT64_MIN
        d("-9223372036854775807"),  // INT64_MIN + 1
        d("9223372036854775808"),   // 2^63 == INT64_MAX + 1，只有取负后才能装回小路径
        d("9223372036854775809"),   // 2^63 + 1
        d("-9223372036854775809"),  // -(2^63 + 1)
        d("4294967295"),            // 2^32 - 1，单 limb 全 1 比特
        d("4294967296"),            // 2^32
        d("-4294967296"),           // -2^32
        d("18446744073709551615"),  // 2^64 - 1，双 limb 全 1 比特
        d("18446744073709551616"),  // 2^64
        d("18446744073709551617"),  // 2^64 + 1，中间 limb 恰好是 0
        d("-18446744073709551616"), // -2^64
        two_pow_128_minus_1,        // 2^128 - 1，四个 limb 全 1
        -two_pow_128_minus_1,
        two_pow_192_minus_1, // 2^192 - 1，六个 limb 全 1，进位/借位链更长
        -two_pow_192_minus_1,
        d("123456789012345678901234567890"),
        d("-123456789012345678901234567890"),
    };
}
} // namespace

TEST_SUITE("BigInt——构造与十进制字符串往返") {

    TEST_CASE("0") {
        CHECK(d("0").to_decimal_string() == "0");
        CHECK(BigInt(0).to_decimal_string() == "0");
        CHECK(d("0").is_zero());
        CHECK(d("0").sign() == 0);
    }

    TEST_CASE("小的正数/负数") {
        CHECK(d("123").to_decimal_string() == "123");
        CHECK(d("-123").to_decimal_string() == "-123");
        CHECK(d("+123").to_decimal_string() == "123"); // 允许显式前导 '+'
    }

    TEST_CASE("跨 limb 边界（32/64 位）的大数往返") {
        CHECK(d("4294967295").to_decimal_string() == "4294967295"); // 2^32 - 1，恰好 1 个 limb
        CHECK(d("4294967296").to_decimal_string() == "4294967296"); // 2^32，跨到第 2 个 limb
        CHECK(d("18446744073709551615").to_decimal_string() == "18446744073709551615"); // 2^64 - 1
        CHECK(d("18446744073709551616").to_decimal_string() == "18446744073709551616"); // 2^64
    }

    TEST_CASE("30 位的超大数（跟 literals_test.cpp 里测过的字面量一致）") {
        CHECK(
            d("123456789012345678901234567890").to_decimal_string() ==
            "123456789012345678901234567890"
        );
        CHECK(
            d("-123456789012345678901234567890").to_decimal_string() ==
            "-123456789012345678901234567890"
        );
    }

    TEST_CASE("前导 0 不影响数值，但输出永远不带多余前导 0") {
        CHECK(d("007").to_decimal_string() == "7");
        CHECK(d("-007").to_decimal_string() == "-7");
        CHECK(d("000").to_decimal_string() == "0"); // "负零"归一成 "0"
    }

    TEST_CASE("非法输入抛 std::invalid_argument") {
        // BigInt 的返回值都标了 [[nodiscard]]，CHECK_THROWS_AS 内部展开成裸表达式语句，
        // 这里显式 (void) 掉返回值，不然 -Werror 会把这几行当成"忽略了 nodiscard 返回值"报错
        CHECK_THROWS_AS((void) BigInt::from_decimal_string(""), std::invalid_argument);
        CHECK_THROWS_AS((void) BigInt::from_decimal_string("-"), std::invalid_argument);
        CHECK_THROWS_AS((void) BigInt::from_decimal_string("+"), std::invalid_argument);
        CHECK_THROWS_AS((void) BigInt::from_decimal_string("12a"), std::invalid_argument);
        CHECK_THROWS_AS((void) BigInt::from_decimal_string("1.5"), std::invalid_argument);
        CHECK_THROWS_AS((void) BigInt::from_decimal_string(" 1"), std::invalid_argument);
        CHECK_THROWS_AS((void) BigInt::from_decimal_string("1 "), std::invalid_argument);
        CHECK_THROWS_AS((void) BigInt::from_decimal_string("--1"), std::invalid_argument);
        CHECK_THROWS_AS((void) BigInt::from_decimal_string("1-1"), std::invalid_argument);
    }

    TEST_CASE("更多脏输入抛 std::invalid_argument") {
        CHECK_THROWS_AS((void) BigInt::from_decimal_string("+-1"), std::invalid_argument);
        CHECK_THROWS_AS((void) BigInt::from_decimal_string("1_000"), std::invalid_argument);
        CHECK_THROWS_AS((void) BigInt::from_decimal_string("0x10"), std::invalid_argument);
        CHECK_THROWS_AS((void) BigInt::from_decimal_string("1\n"), std::invalid_argument);
        CHECK_THROWS_AS(
            (void) BigInt::from_decimal_string(std::string("1\0002", 3)), std::invalid_argument
        );
        // 全角 "1" 的 UTF-8 编码：每个字节都不落在 ASCII '0'-'9' 范围内
        CHECK_THROWS_AS(
            (void) BigInt::from_decimal_string("\xef\xbc\x91"), std::invalid_argument
        );
    }

    TEST_CASE("超长十进制字符串往返（500 位、2000 位），顺带过一遍加减法不会破坏这么长的数") {
        for (const int len : {500, 2000}) {
            std::string s(static_cast<size_t>(len), '0');
            s[0] = '9';
            for (size_t i{1}; i < s.size(); ++i) s[i] = static_cast<char>('0' + (i % 10));
            const BigInt x{d(s)};
            CHECK(x.to_decimal_string() == s);
            CHECK((x + BigInt(1) - BigInt(1)) == x);
        }
    }

    TEST_CASE("超长前导 0 + 符号的组合") {
        CHECK(d("+000").to_decimal_string() == "0");
        CHECK(d("0000000000000000000000000000001").to_decimal_string() == "1");
        CHECK(d("-0000000000000000000000000000001").to_decimal_string() == "-1");
    }

    TEST_CASE("十进制输出的 9 位一组分块逻辑：块内部/中间的 0 不能被漏掉（只有最高位块不补零）") {
        // 10^18 + 1：从右数第一个 9 位块应该是 "000000001"，如果分块补零漏了，会错输出成
        // "1000000000000000001" 少了中间的 0
        CHECK((d("1000000000000000000") + d("1")).to_decimal_string() == "1000000000000000001");
        // 10^27 + 1：中间隔着两个完整的、全是 0 的 9 位块，最容易漏补零
        CHECK(
            (d("1000000000000000000000000000") + d("1")).to_decimal_string() ==
            "1000000000000000000000000001"
        );
        // 同样的场景对负数也要成立
        CHECK(
            (d("-1000000000000000000000000000") - d("1")).to_decimal_string() ==
            "-1000000000000000000000000001"
        );
    }

    TEST_CASE("long long 构造，含边界值") {
        CHECK(BigInt(0LL).to_decimal_string() == "0");
        CHECK(BigInt(42LL).to_decimal_string() == "42");
        CHECK(BigInt(-42LL).to_decimal_string() == "-42");
        CHECK(
            BigInt(9223372036854775807LL).to_decimal_string() == "9223372036854775807"
        ); // LLONG_MAX
        CHECK(
            BigInt(-9223372036854775807LL - 1).to_decimal_string() == "-9223372036854775808"
        ); // LLONG_MIN
    }
}

TEST_SUITE("BigInt——符号/奇偶/绝对值") {

    TEST_CASE("sign") {
        CHECK(d("5").sign() == 1);
        CHECK(d("-5").sign() == -1);
        CHECK(d("0").sign() == 0);
    }

    TEST_CASE("is_negative / is_zero") {
        CHECK(d("-1").is_negative());
        CHECK_FALSE(d("1").is_negative());
        CHECK_FALSE(d("0").is_negative()); // 0 恒非负
        CHECK(d("0").is_zero());
        CHECK_FALSE(d("1").is_zero());
    }

    TEST_CASE("is_odd") {
        CHECK(d("3").is_odd());
        CHECK(d("-3").is_odd());
        CHECK_FALSE(d("4").is_odd());
        CHECK_FALSE(d("0").is_odd());
    }

    TEST_CASE("is_odd：大路径（超出 int64_t 范围）下同样成立，只看最低位那个 limb") {
        CHECK(d("123456789012345678901234567891").is_odd());
        CHECK(d("-123456789012345678901234567891").is_odd());
        CHECK_FALSE(d("123456789012345678901234567890").is_odd());
        CHECK_FALSE(d("-123456789012345678901234567890").is_odd());
    }

    TEST_CASE("abs") {
        CHECK(d("-5").abs().to_decimal_string() == "5");
        CHECK(d("5").abs().to_decimal_string() == "5");
        CHECK(d("0").abs().to_decimal_string() == "0");
    }

    TEST_CASE("abs：大路径下（含 INT64_MIN 这种取绝对值本身会升级到大路径的情况）") {
        CHECK(
            d("-123456789012345678901234567890").abs().to_decimal_string() ==
            "123456789012345678901234567890"
        );
        CHECK(d("-9223372036854775808").abs().to_decimal_string() == "9223372036854775808");
        // 绝对值的绝对值应该是它自己（幂等）
        CHECK(d("-5").abs().abs() == d("5").abs());
    }

    TEST_CASE("大路径下的正数：abs()/sign()/is_negative() 不受影响（之前只测过大路径负数）") {
        const BigInt positive_big{d("123456789012345678901234567890")};
        CHECK(positive_big.abs() == positive_big);
        CHECK(positive_big.sign() == 1);
        CHECK_FALSE(positive_big.is_negative());
    }
}

TEST_SUITE("BigInt——to_double") {

    TEST_CASE("零和小数值：精确转换") {
        CHECK(d("0").to_double() == 0.0);
        CHECK(d("123").to_double() == 123.0);
        CHECK(d("-123").to_double() == -123.0);
    }

    TEST_CASE("跨 limb 的大路径数值：跟手算的 2^64 对上") {
        CHECK(d("18446744073709551616").to_double() == 18446744073709551616.0); // 2^64
        CHECK(d("-18446744073709551616").to_double() == -18446744073709551616.0);
    }

    TEST_CASE(
        "远超 double 表示范围（> 1.8e308）：按 IEEE 溢出语义得到 ±infinity，不抛异常、不是 NaN"
    ) {
        const BigInt huge{d("10").pow(d("400"))}; // 10^400，远超 DBL_MAX
        CHECK(std::isinf(huge.to_double()));
        CHECK(huge.to_double() > 0);
        CHECK(std::isinf((-huge).to_double()));
        CHECK((-huge).to_double() < 0);
    }

    TEST_CASE(
        "跟 std::strtod 的正确舍入结果逐条对拍：覆盖位数超过 64 位、不可精确表示、需要真正"
        "就近舍入的大数（早前逐 limb 累乘累加的实现在这类值上会错 1 ULP）"
    ) {
        const auto check_matches_strtod{[](const BigInt &x) {
            const std::string s{x.to_decimal_string()};
            CAPTURE(s);
            CHECK(x.to_double() == std::strtod(s.c_str(), nullptr));
        }};
        for (const std::string &s : {
                 std::string("10000000000000000000000000"), // 10^25
                 std::string("99999999999999999999999999"),
                 std::string("100000000000000000000000000"), // 10^26
                 std::string("123456789012345678901234567890"),
                 std::string(
                     "999999999999999999999999999999999999999999999999999999999999"
                 ),
             }) {
            check_matches_strtod(d(s));
            check_matches_strtod(-d(s));
        }
        // 2^127 附近：移位构造，不手抄一个 39 位的十进制常量
        const BigInt two_pow_127{BigInt(1) << 127};
        check_matches_strtod(two_pow_127);
        check_matches_strtod(two_pow_127 - BigInt(1));
        check_matches_strtod(two_pow_127 + BigInt(1));
        check_matches_strtod(-two_pow_127);
    }

    TEST_CASE("大范围扫描 10^k（k = 15..300）跟 std::strtod 对拍，覆盖更多可能踩中舍入边界的量级") {
        for (int k{15}; k <= 300; k += 7) {
            const std::string s{"1" + std::string(static_cast<size_t>(k), '0')};
            const BigInt x{d(s)};
            CAPTURE(s);
            CHECK(x.to_double() == std::strtod(s.c_str(), nullptr));
            // 同一量级里再测一个不是整十次幂的值，更容易踩中舍入边界
            const BigInt y{x + x.floor_div(BigInt(3))};
            const std::string y_str{y.to_decimal_string()};
            CHECK(y.to_double() == std::strtod(y_str.c_str(), nullptr));
        }
    }
}

TEST_SUITE("BigInt——比较") {

    TEST_CASE("同号比大小") {
        CHECK(d("5") < d("10"));
        CHECK(d("-10") < d("-5")); // 负数里，量级大的反而更小
        CHECK(d("5") == d("5"));
    }

    TEST_CASE("异号：非负恒大于负数") {
        CHECK(d("0") > d("-1"));
        CHECK(d("1") > d("-100000000000000000000"));
    }

    TEST_CASE("六种比较运算符都从 <=> 正确派生") {
        CHECK(d("3") <= d("3"));
        CHECK(d("3") >= d("3"));
        CHECK(d("3") != d("4"));
        CHECK_FALSE(d("3") > d("3"));
    }
}

TEST_SUITE("BigInt——加减乘（含跨 limb 进位/借位）") {

    TEST_CASE("加法：同号/异号/结果为 0") {
        CHECK((d("2") + d("3")).to_decimal_string() == "5");
        CHECK((d("-2") + d("-3")).to_decimal_string() == "-5");
        CHECK((d("5") + d("-3")).to_decimal_string() == "2");
        CHECK((d("3") + d("-5")).to_decimal_string() == "-2");
        CHECK((d("5") + d("-5")).to_decimal_string() == "0");
    }

    TEST_CASE("加法进位跨 limb 边界") {
        CHECK((d("4294967295") + d("1")).to_decimal_string() == "4294967296"); // 2^32-1 + 1 == 2^32
        CHECK(
            (d("18446744073709551615") + d("1")).to_decimal_string() == "18446744073709551616"
        ); // 2^64-1 + 1
    }

    TEST_CASE("减法：借位跨 limb 边界") {
        CHECK((d("4294967296") - d("1")).to_decimal_string() == "4294967295");
        CHECK((d("0") - d("1")).to_decimal_string() == "-1");
        CHECK((d("5") - d("5")).to_decimal_string() == "0");
    }

    TEST_CASE("一元 + / - / 与 literals_test.cpp 一致：负数不是字面量，是一元运算的结果") {
        CHECK((+d("5")).to_decimal_string() == "5");
        CHECK((-d("5")).to_decimal_string() == "-5");
        CHECK((-d("-5")).to_decimal_string() == "5");
        CHECK((-d("0")).to_decimal_string() == "0"); // 不产生"负零"
    }

    TEST_CASE("乘法：符号规则、跨多个 limb 的大数相乘") {
        CHECK((d("6") * d("7")).to_decimal_string() == "42");
        CHECK((d("-6") * d("7")).to_decimal_string() == "-42");
        CHECK((d("-6") * d("-7")).to_decimal_string() == "42");
        CHECK((d("0") * d("-7")).to_decimal_string() == "0"); // 0 * 负数不应该产生"负零"

        // (2^64) * (2^64) == 2^128，跨好几个 limb 的竖式乘法进位
        const BigInt two64{d("18446744073709551616")};
        CHECK((two64 * two64).to_decimal_string() == "340282366920938463463374607431768211456");
    }

    TEST_CASE("跟 literals_test.cpp 同款的 30 位大数做加法") {
        CHECK(
            (d("123456789012345678901234567890") + d("1")).to_decimal_string() ==
            "123456789012345678901234567891"
        );
    }
}

TEST_SUITE("BigInt——floor_div / mod：向负无穷取整，语义与 Python 一致") {

    TEST_CASE("SL.md 3.4.2 原文举的例子：-7 // 2 == -4，-7 % 2 == 1") {
        CHECK(d("-7").floor_div(d("2")).to_decimal_string() == "-4");
        CHECK(d("-7").mod(d("2")).to_decimal_string() == "1");
    }

    TEST_CASE("除数为负时，商同样向负无穷取整，余数跟除数同号：7 // -2 == -4，7 % -2 == -1") {
        CHECK(d("7").floor_div(d("-2")).to_decimal_string() == "-4");
        CHECK(d("7").mod(d("-2")).to_decimal_string() == "-1");
    }

    TEST_CASE("同号相除：结果非负，等价于普通截断除法") {
        CHECK(d("7").floor_div(d("2")).to_decimal_string() == "3");
        CHECK(d("7").mod(d("2")).to_decimal_string() == "1");
        CHECK(d("-7").floor_div(d("-2")).to_decimal_string() == "3");
        CHECK(d("-7").mod(d("-2")).to_decimal_string() == "-1"); // 余数恒跟除数同号，这里除数是负的
    }

    TEST_CASE("整除（余数为 0）时不需要向下修正，两种取整方式结果一致") {
        CHECK(d("6").floor_div(d("2")).to_decimal_string() == "3");
        CHECK(d("-6").floor_div(d("2")).to_decimal_string() == "-3");
        CHECK(d("6").mod(d("2")).to_decimal_string() == "0");
        CHECK(d("-6").mod(d("2")).to_decimal_string() == "0");
    }

    TEST_CASE("恒等式 x == (x // y) * y + x % y 在正负交叉组合下都成立") {
        for (const auto &[x, y] :
             {std::pair{d("17"), d("5")},
              std::pair{d("-17"), d("5")},
              std::pair{d("17"), d("-5")},
              std::pair{d("-17"), d("-5")},
              std::pair{d("123456789012345678901234567890"), d("7")}}) {
            const BigInt q{x.floor_div(y)};
            const BigInt r{x.mod(y)};
            CHECK((q * y + r) == x);
        }
    }

    TEST_CASE("mod 的结果严格满足 |x % y| < |y|（不只是符号跟除数一致，量级也要卡住）") {
        const auto vals{interesting_values()};
        for (const auto &x : vals) {
            for (const auto &y : vals) {
                if (y.is_zero()) continue;
                CHECK(x.mod(y).abs() < y.abs());
            }
        }
    }

    TEST_CASE(
        "反向构造：a = q*b + r（r 与 b 同号或为 0，且 |r| < |b|），floor_div/mod 必须精确复原 "
        "q、r——跟前面的恒等式测试正好反过来，能直接钉死商本身对不对，而不只是钉住乘回去的乘积"
    ) {
        const auto vals{interesting_values()};
        for (const auto &b : vals) {
            if (b.is_zero()) continue;
            for (const auto &q : vals) {
                // 三个 k 都保证落在 [0, |b|-1]：0 恒合法；|b|/2 向下取整恒 < |b|；|b|-1 是能取到的最大值
                for (const BigInt &k :
                     {BigInt(0), b.abs().floor_div(BigInt(2)), b.abs() - BigInt(1)}) {
                    const BigInt r{b.is_negative() ? -k : k};
                    const BigInt a{q * b + r};
                    CHECK(a.floor_div(b) == q);
                    CHECK(a.mod(b) == r);
                }
            }
        }
    }

    TEST_CASE("别名：同一个对象当被除数和除数（x.floor_div(x) == 1，x.mod(x) == 0）") {
        const auto vals{interesting_values()};
        for (const auto &x : vals) {
            if (x.is_zero()) continue;
            CHECK(x.floor_div(x) == BigInt(1));
            CHECK(x.mod(x).is_zero());
        }
    }

    TEST_CASE("除数为 0 抛 std::domain_error") {
        CHECK_THROWS_AS((void) d("1").floor_div(d("0")), std::domain_error);
        CHECK_THROWS_AS((void) d("1").mod(d("0")), std::domain_error);
        CHECK_THROWS_AS((void) d("0").floor_div(d("0")), std::domain_error);
    }

    TEST_CASE("被除数为 0") {
        CHECK(d("0").floor_div(d("5")).to_decimal_string() == "0");
        CHECK(d("0").mod(d("5")).to_decimal_string() == "0");
    }

    TEST_CASE("被除数为 0、但除数超出 int64_t 范围（走大路径）：0 除以巨大的数还是 0") {
        const BigInt huge{d("123456789012345678901234567890")};
        CHECK(d("0").floor_div(huge).to_decimal_string() == "0");
        CHECK(d("0").mod(huge).to_decimal_string() == "0");
        CHECK(d("0").floor_div(-huge).to_decimal_string() == "0");
    }

    TEST_CASE("被除数、除数都超出 int64_t 范围（走大路径），异号，但商本身装得进 int64_t") {
        // x = -(3 * 10^20 + 7)，y = 10^20：|x|/|y| = 3.00000000007，异号，
        // 向负无穷取整应该是 -4，不是 -3
        const BigInt x{d("-300000000000000000007")};
        const BigInt y{d("100000000000000000000")};
        CHECK(x.floor_div(y).to_decimal_string() == "-4");
        CHECK(x.mod(y).to_decimal_string() == "99999999999999999993");
        CHECK((x.floor_div(y) * y + x.mod(y)) == x);
    }

    TEST_CASE(
        "shrink() 那个 bug "
        "的完整复现场景：大路径操作数、商恰好收缩回小路径，覆盖异号/同号/整除交叉组合"
    ) {
        // 除了验证恒等式，还额外验证收缩后的商能正常跟一个直接构造的小路径同值用 ==
        // 判定相等——这正是原 bug 的真实症状：is_small_ 标志不一致导致 == 直接误判为不等，
        // 而不是数值算错
        struct Case {
            std::string x, y, q, r;
        };
        for (const Case &c : {
                 Case{
                     "-300000000000000000007", "100000000000000000000", "-4", "99999999999999999993"
                 },
                 Case{
                     "300000000000000000007",
                     "-100000000000000000000",
                     "-4",
                     "-99999999999999999993"
                 },
                 Case{"-300000000000000000000", "100000000000000000000", "-3", "0"}, // 整除，异号
                 Case{"300000000000000000000", "-100000000000000000000", "-3", "0"},
                 Case{
                     "-100000000000000000001", "100000000000000000000", "-2", "99999999999999999999"
                 },
                 Case{
                     "100000000000000000001",
                     "-100000000000000000000",
                     "-2",
                     "-99999999999999999999"
                 },
                 Case{
                     "-123456789012345678901234567890", "123456789012345678901234567891", "-1", "1"
                 },
                 Case{"100000000000000000000", "100000000000000000000", "1", "0"}, // 同号，整除
                 Case{"-100000000000000000000", "-100000000000000000000", "1", "0"},
             }) {
            const BigInt x{d(c.x)}, y{d(c.y)};
            const BigInt q{x.floor_div(y)}, r{x.mod(y)};
            CHECK(q.to_decimal_string() == c.q);
            CHECK(r.to_decimal_string() == c.r);
            CHECK((q * y + r) == x);
            CHECK(q == d(c.q)); // 收缩后必须能跟直接构造的小路径同值相等
            CHECK(r == d(c.r));
        }
    }
}

TEST_SUITE("BigInt——pow") {

    TEST_CASE("基本形式") {
        CHECK(d("2").pow(d("10")).to_decimal_string() == "1024");
        CHECK(d("5").pow(d("0")).to_decimal_string() == "1"); // 任何数的 0 次幂是 1
        CHECK(d("0").pow(d("0")).to_decimal_string() == "1"); // 约定 0**0 == 1
        CHECK(d("0").pow(d("5")).to_decimal_string() == "0");
    }

    TEST_CASE("负数底数") {
        CHECK(d("-2").pow(d("3")).to_decimal_string() == "-8");
        CHECK(d("-2").pow(d("4")).to_decimal_string() == "16");
    }

    TEST_CASE(
        "大指数：用指数加法律 x^(a+b) == x^a * x^b 交叉验证，不依赖手算一个几十位的巨大常量"
    ) {
        const BigInt two{2};
        CHECK(two.pow(d("100")) == two.pow(d("50")) * two.pow(d("50")));
        // 2^64 是独立验证过的常量（跟"跨 limb 边界的大数往返"那个用例一致），拿它反过来校验 pow
        // 本身
        CHECK(two.pow(d("64")) == d("18446744073709551616"));
        CHECK(two.pow(d("128")) == d("18446744073709551616") * d("18446744073709551616"));
    }

    TEST_CASE(
        "负指数抛 std::domain_error（SL 里 int ** 负数不再是 int，是 float，不归 BigInt 管）"
    ) {
        CHECK_THROWS_AS((void) d("2").pow(d("-1")), std::domain_error);
    }

    TEST_CASE("指数本身是大路径值（走 is_odd()/floor_div 的大路径分支，之前这条路径零覆盖）") {
        CHECK(d("1").pow(d("100000000000000000000")) == d("1")); // 1 的任何次幂恒为 1
        CHECK(d("0").pow(d("100000000000000000000")).is_zero());
        CHECK(d("-1").pow(d("100000000000000000000")) == d("1"));  // 个位是 0，偶数
        CHECK(d("-1").pow(d("100000000000000000001")) == d("-1")); // 个位是 1，奇数
    }

    TEST_CASE("大路径负指数同样抛 std::domain_error") {
        CHECK_THROWS_AS((void) d("2").pow(d("-100000000000000000000")), std::domain_error);
    }
}

TEST_SUITE("BigInt——位运算：按无穷位补码语义，与 Python 一致") {

    TEST_CASE("SL.md 3.4.2 原文举的例子") {
        CHECK((~d("5")).to_decimal_string() == "-6");
        CHECK((d("-1") & d("255")).to_decimal_string() == "255"); // -1 的所有位都是 1
        CHECK((d("-1") >> 100).to_decimal_string() == "-1");      // 无论右移多少位，结果恒为 -1
    }

    TEST_CASE("~x 恒等于 -x - 1") {
        CHECK((~d("0")).to_decimal_string() == "-1");
        CHECK((~d("-1")).to_decimal_string() == "0");
        CHECK((~d("100")).to_decimal_string() == "-101");
        CHECK((~d("-100")).to_decimal_string() == "99");
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

    TEST_CASE("负数右移恒为 -1（SL.md 3.4.2 原文例子）") {
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

// 以下两个 TEST_SUITE 是针对 shrink() 那次 bug 的教训专门加的高强度测试：不再靠手挑几个具体案例，
// 而是拿一批"边界值"两两、三三组合，批量验证数学上必然成立的恒等式。这类测试的好处是覆盖面是
// 组合爆炸级的（几百上千种组合），且不需要我手算大数的期望值——期望值就是恒等式本身，只要 BigInt
// 内部实现哪怕有一处不满足某条数学定律，几百种组合里大概率会踩中至少一种。
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

TEST_SUITE(
    "BigInt——小路径/大路径规范化不变量：同一个值无论经过哪条运算路径算出来，都必须能用 == "
    "判定相等（这正是 shrink() 那次 bug 的教训——bug 发作时数值本身没错，只是 is_small_ "
    "标志跟别的同值对象不一致，导致 == 被误判为不等，只有专门针对这一点测才测得出来）"
) {

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
