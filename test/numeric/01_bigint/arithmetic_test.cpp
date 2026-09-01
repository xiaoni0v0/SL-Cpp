// 加减乘、向负无穷整除取模、幂。
#include "test_utils.h"

#include <doctest/doctest.h>
#include <stdexcept>

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

    TEST_CASE("-7 // 2 == -4，-7 % 2 == 1") {
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
                // 三个 k 都保证落在 [0, |b|-1]：0 恒合法；|b|/2 向下取整恒 < |b|；|b|-1
                // 是能取到的最大值
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
        "负指数抛 std::domain_error（SL 里 int ** 负数不再是 int，是 decimal，不归 BigInt 管）"
    ) {
        CHECK_THROWS_AS((void) d("2").pow(d("-1")), std::domain_error);
    }

    TEST_CASE("指数本身是大路径值（走 is_odd()/floor_div 的大路径分支）") {
        CHECK(d("1").pow(d("100000000000000000000")) == d("1")); // 1 的任何次幂恒为 1
        CHECK(d("0").pow(d("100000000000000000000")).is_zero());
        CHECK(d("-1").pow(d("100000000000000000000")) == d("1"));  // 个位是 0，偶数
        CHECK(d("-1").pow(d("100000000000000000001")) == d("-1")); // 个位是 1，奇数
    }

    TEST_CASE("大路径负指数同样抛 std::domain_error") {
        CHECK_THROWS_AS((void) d("2").pow(d("-100000000000000000000")), std::domain_error);
    }
}
