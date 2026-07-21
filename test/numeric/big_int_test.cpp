// BigInt：任意精度有符号整数。语义细节（// 向负无穷取整、位运算按无穷位补码）见 SL.md 3.4.2。
#include "../../numeric/BigInt.h"

#include <stdexcept>
#include <doctest/doctest.h>

namespace {
BigInt d(const std::string &s) { return BigInt::from_decimal_string(s); }
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
    CHECK(d("123456789012345678901234567890").to_decimal_string() == "123456789012345678901234567890");
    CHECK(d("-123456789012345678901234567890").to_decimal_string() == "-123456789012345678901234567890");
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
    CHECK_THROWS_AS((void) BigInt::from_decimal_string("12a"), std::invalid_argument);
    CHECK_THROWS_AS((void) BigInt::from_decimal_string("1.5"), std::invalid_argument);
    CHECK_THROWS_AS((void) BigInt::from_decimal_string(" 1"), std::invalid_argument);
}

TEST_CASE("long long 构造，含边界值") {
    CHECK(BigInt(0LL).to_decimal_string() == "0");
    CHECK(BigInt(42LL).to_decimal_string() == "42");
    CHECK(BigInt(-42LL).to_decimal_string() == "-42");
    CHECK(BigInt(9223372036854775807LL).to_decimal_string() == "9223372036854775807"); // LLONG_MAX
    CHECK(BigInt(-9223372036854775807LL - 1).to_decimal_string() == "-9223372036854775808"); // LLONG_MIN
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

TEST_CASE("abs") {
    CHECK(d("-5").abs().to_decimal_string() == "5");
    CHECK(d("5").abs().to_decimal_string() == "5");
    CHECK(d("0").abs().to_decimal_string() == "0");
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
    CHECK((d("18446744073709551615") + d("1")).to_decimal_string() == "18446744073709551616"); // 2^64-1 + 1
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
    CHECK((d("123456789012345678901234567890") + d("1")).to_decimal_string()
        == "123456789012345678901234567891");
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
    for (const auto &[x, y] : {
             std::pair{d("17"), d("5")}, std::pair{d("-17"), d("5")},
             std::pair{d("17"), d("-5")}, std::pair{d("-17"), d("-5")},
             std::pair{d("123456789012345678901234567890"), d("7")}
         }) {
        const BigInt q{x.floor_div(y)};
        const BigInt r{x.mod(y)};
        CHECK((q * y + r) == x);
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

TEST_CASE("大指数：用指数加法律 x^(a+b) == x^a * x^b 交叉验证，不依赖手算一个几十位的巨大常量") {
    const BigInt two{2};
    CHECK(two.pow(d("100")) == two.pow(d("50")) * two.pow(d("50")));
    // 2^64 是独立验证过的常量（跟"跨 limb 边界的大数往返"那个用例一致），拿它反过来校验 pow 本身
    CHECK(two.pow(d("64")) == d("18446744073709551616"));
    CHECK(two.pow(d("128")) == d("18446744073709551616") * d("18446744073709551616"));
}

TEST_CASE("负指数抛 std::domain_error（SL 里 int ** 负数不再是 int，是 float，不归 BigInt 管）") {
    CHECK_THROWS_AS((void) d("2").pow(d("-1")), std::domain_error);
}

}

TEST_SUITE("BigInt——位运算：按无穷位补码语义，与 Python 一致") {

TEST_CASE("SL.md 3.4.2 原文举的例子") {
    CHECK((~d("5")).to_decimal_string() == "-6");
    CHECK((d("-1") & d("255")).to_decimal_string() == "255"); // -1 的所有位都是 1
    CHECK((d("-1") >> 100).to_decimal_string() == "-1"); // 无论右移多少位，结果恒为 -1
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

}

TEST_SUITE("BigInt——小路径/大路径边界（内部按 int64_t 能不能装得下自动切换，这里专测切换点）") {

TEST_CASE("加法：恰好在 int64_t 边界上下的进退位") {
    CHECK((d("9223372036854775807") + d("1")).to_decimal_string() == "9223372036854775808"); // INT64_MAX + 1
    CHECK((d("-9223372036854775808") - d("1")).to_decimal_string() == "-9223372036854775809"); // INT64_MIN - 1
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

TEST_CASE("floor_div / mod：INT64_MIN / -1 这个组合原生 int64_t 除法本身会溢出，必须退到大路径") {
    CHECK(d("-9223372036854775808").floor_div(d("-1")).to_decimal_string() == "9223372036854775808");
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
    CHECK((huge - d("99999999999999999995")).to_decimal_string() == "5"); // 结果退回到小路径范围
    CHECK((huge - huge).to_decimal_string() == "0");
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

}
