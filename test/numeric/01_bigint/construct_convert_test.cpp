// 构造、十进制往返、符号/绝对值、to_double、比较。
#include "test_utils.h"

#include <cmath>
#include <cstdlib>
#include <doctest/doctest.h>
#include <limits>
#include <stdexcept>

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
        CHECK_THROWS_AS((void) BigInt::from_decimal_string("\xef\xbc\x91"), std::invalid_argument);
    }

    TEST_CASE("科学计数法：尾数 × 10^指数") {
        CHECK(d("1e9").to_decimal_string() == "1000000000");
        CHECK(d("1E9").to_decimal_string() == "1000000000"); // e/E 都收
        CHECK(d("1e+9").to_decimal_string() == "1000000000");
        CHECK(d("-1e9").to_decimal_string() == "-1000000000"); // 符号在最前面
        CHECK(d("+1e9").to_decimal_string() == "1000000000");
        CHECK(d("123e4").to_decimal_string() == "1230000"); // 尾数不止一位
        CHECK(d("1e0").to_decimal_string() == "1");         // 指数 0 就是尾数本身
        CHECK(d("0e100").to_decimal_string() == "0");       // 零乘多少都是零，且不产生负零
        CHECK(d("-0e100").to_decimal_string() == "0");
        // 指数带前导零合法（BigInt 这层不管"不允许前导零"，那是 lexer 对源码字面量的规矩）
        CHECK(d("1e009").to_decimal_string() == "1000000000");
        // 跟等价的手写字面量、以及 pow 三方对上
        CHECK(d("1e100") == d("1" + std::string(100, '0')));
        CHECK(d("1e100") == d("10").pow(d("100")));
        CHECK(d("25e40") == d("25") * d("10").pow(d("40")));
    }

    TEST_CASE(
        "科学计数法：尾数为 0 时必须 O(1) 短路，不能真的去构造 10^指数"
        "（指数不设上限，没这条短路的话 0e1000000 白算三秒多，再大一档就是分钟级/爆内存）"
    ) {
        CHECK(d("0e1000000").is_zero());
        CHECK(d("0e999999999").to_decimal_string() == "0"); // 十亿级指数，没短路会直接卡死
        CHECK(d("-0e999999999").to_decimal_string() == "0");
        CHECK(d("00e999999999").to_decimal_string() == "0");
        // 尾数非零时照旧真去算，短路不能误伤
        CHECK(d("1e100") == d("10").pow(d("100")));
    }

    TEST_CASE("科学计数法：指数为负一律不合法（BigInt 是整数类型）") {
        CHECK_THROWS_AS((void) BigInt::from_decimal_string("1e-9"), std::invalid_argument);
        // 数值上恰好是整数 10，同样不收——合不合法只看写法，不看算出来的值
        CHECK_THROWS_AS((void) BigInt::from_decimal_string("100e-1"), std::invalid_argument);
        CHECK_THROWS_AS((void) BigInt::from_decimal_string("1e-0"), std::invalid_argument);
    }

    TEST_CASE("科学计数法：残缺/畸形的指数部分抛 std::invalid_argument") {
        CHECK_THROWS_AS((void) BigInt::from_decimal_string("1e"), std::invalid_argument);
        CHECK_THROWS_AS((void) BigInt::from_decimal_string("1e+"), std::invalid_argument);
        CHECK_THROWS_AS((void) BigInt::from_decimal_string("e9"), std::invalid_argument);
        CHECK_THROWS_AS((void) BigInt::from_decimal_string("1e9e9"), std::invalid_argument);
        CHECK_THROWS_AS((void) BigInt::from_decimal_string("1e9."), std::invalid_argument);
        CHECK_THROWS_AS((void) BigInt::from_decimal_string("1e1.5"), std::invalid_argument);
        CHECK_THROWS_AS((void) BigInt::from_decimal_string("1e 9"), std::invalid_argument);
        CHECK_THROWS_AS((void) BigInt::from_decimal_string("1ee9"), std::invalid_argument);
        CHECK_THROWS_AS((void) BigInt::from_decimal_string("1.5e9"), std::invalid_argument);
        // 指数本身装不进 int64_t（19 位）。挡的是"指数表示不了"，不是"指数太大算不动"
        CHECK_THROWS_AS(
            (void) BigInt::from_decimal_string("1e9999999999999999999"), std::invalid_argument
        );
        // 前导零不计入位数：下面这个去掉前导零只有 1 位，照收
        CHECK(d("1e0000000000000000000009").to_decimal_string() == "1000000000");
        // 尾数是零不该走"0 直接短路"那条捷径把残缺指数的错误吞掉——得先因指数残缺抛错
        CHECK_THROWS_AS((void) BigInt::from_decimal_string("0e"), std::invalid_argument);
        CHECK_THROWS_AS((void) BigInt::from_decimal_string("0e+"), std::invalid_argument);
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
        // INT64_MIN == -2^63，偶；小路径负数的 `small_ % 2` 在 C++ 里是负的，不能写成 `== 1`
        CHECK_FALSE(d("-9223372036854775808").is_odd());
        CHECK(d("-9223372036854775807").is_odd());      // INT64_MIN + 1
        CHECK_FALSE(d("9223372036854775808").is_odd()); // 2^63，大路径偶数
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

    TEST_CASE("大路径下的正数：abs()/sign()/is_negative()") {
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

    TEST_CASE("跟 std::strtod 对拍：覆盖超过 64 位、不可精确表示、需要就近舍入的大数") {
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
                 std::string("999999999999999999999999999999999999999999999999999999999999"),
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

    TEST_CASE(
        "大范围扫描 10^k（k 从 15 到 295，步长 7）跟 std::strtod 对拍，覆盖更多舍入边界量级"
    ) {
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

    TEST_CASE("刻意构造舍入平局（数值恰好卡在两个相邻 double 正中间），验证就近取偶 + sticky 位") {
        // m 是 53 位整数（顶满一个 double 尾数），X = (2m+1) * 2^(k-1) 恰好是 m*2^k 和
        // (m+1)*2^k 正中间那个整数——m、m+1 这两个尾数在这个量级上正是相邻的两个可表示 double，
        // 所以 X 是一个精确的、数学意义上的舍入平局，不依赖对 to_double 内部实现的任何假设
        const auto build_tie{[](uint64_t m, int k) {
            return (BigInt(static_cast<long long>(m)) * BigInt(2) + BigInt(1)) << (k - 1);
        }};
        for (const auto &[m, k] : {
                 std::pair<uint64_t, int>{uint64_t{1} << 52, 100},       // 偶尾数
                 std::pair<uint64_t, int>{(uint64_t{1} << 52) + 1, 100}, // 奇尾数
             }) {
            const BigInt tie{build_tie(m, k)};
            const double down{std::ldexp(static_cast<double>(m), k)};
            const double up{std::ldexp(static_cast<double>(m + 1), k)};
            const double expected_tie{m % 2 == 0 ? down : up}; // 平局就近取偶
            CHECK(tie.to_double() == expected_tie);
            CHECK((tie + BigInt(1)).to_double() == up);   // 略过平局，sticky 位必须能打破平局向上
            CHECK((tie - BigInt(1)).to_double() == down); // 略欠平局，明确落在下方，向下
        }
    }

    TEST_CASE("DBL_MAX 边界：精确命中、略微超出该舍回 DBL_MAX，明显超出才溢出成 infinity") {
        const BigInt dbl_max_int{((BigInt(1) << 53) - BigInt(1)) << 971}; // (2^53-1)*2^971==DBL_MAX
        CHECK(dbl_max_int.to_double() == std::numeric_limits<double>::max());
        // 只多 1（远小于半个 ULP == 2^970），就近取整应该舍回 DBL_MAX，不能因为超过 DBL_MAX
        // 这个整数值就直接判定成溢出
        CHECK((dbl_max_int + BigInt(1)).to_double() == std::numeric_limits<double>::max());
        // 明显超过半个 ULP，该溢出成 infinity
        CHECK(std::isinf((dbl_max_int + (BigInt(1) << 971)).to_double()));
    }

    TEST_CASE(
        "kExponentClamp 那条路径：比特长度远超 double 范围时提前截断指数，仍正确得到 ±infinity"
    ) {
        const BigInt astronomically_huge{BigInt(1) << 500000}; // 50 万位，远超截断阈值
        CHECK(std::isinf(astronomically_huge.to_double()));
        CHECK(astronomically_huge.to_double() > 0);
        CHECK(std::isinf((-astronomically_huge).to_double()));
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
