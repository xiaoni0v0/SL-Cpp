// 一元运算、// 和 %、比较。
#include "test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("BigDec——一元运算") {

    TEST_CASE("+x 会舍入，不是恒等操作") {
        DecContext ctx{quiet_context(3)};
        CHECK(d("1.2345").plus(ctx).to_string() == "1.23");
        CHECK(ctx.flags().has(DecCondition::Inexact));
        CHECK(ctx.flags().has(DecCondition::Rounded));
        CHECK(d("1.5").plus(ctx).to_string() == "1.5"); // 位数够就原样，标度也不动
        CHECK(d("1.50").plus(ctx).to_string() == "1.50");
    }

    TEST_CASE("-0 的符号：只有 ROUND_FLOOR 保留") {
        DecContext ctx{quiet_context()};
        CHECK(d("-0").plus(ctx).to_string() == "0");
        CHECK(d("0").minus(ctx).to_string() == "0");
        CHECK(d("-0").minus(ctx).to_string() == "0");
        CHECK(d("1").minus(ctx).to_string() == "-1");

        DecContext floor_ctx{quiet_context(28, DecRounding::Floor)};
        CHECK(d("-0").plus(floor_ctx).to_string() == "-0");
        CHECK(d("0").minus(floor_ctx).to_string() == "-0");
        CHECK(d("-0").minus(floor_ctx).to_string() == "0");
    }

    TEST_CASE("abs") {
        DecContext ctx{quiet_context(3)};
        CHECK(d("-1.5").abs(ctx).to_string() == "1.5");
        CHECK(d("1.5").abs(ctx).to_string() == "1.5");
        CHECK(d("-0").abs(ctx).to_string() == "0");
        CHECK(d("-Infinity").abs(ctx).to_string() == "Infinity");
        CHECK(d("-1.2345").abs(ctx).to_string() == "1.23"); // 一样要舍入
        CHECK(d("NaN").abs(ctx).to_string() == "NaN");
    }

    TEST_CASE("一元运算的 NaN 传播") {
        DecContext ctx{quiet_context()};
        CHECK(d("NaN").plus(ctx).to_string() == "NaN");
        CHECK(d("-NaN").minus(ctx).to_string() == "-NaN"); // 传播，不取反
        CHECK(ctx.flags().empty());
        CHECK(d("sNaN").plus(ctx).to_string() == "NaN");
        CHECK(ctx.flags().has(DecCondition::InvalidOperation));
        CHECK(d("-sNaN").plus(ctx).to_string() == "-NaN"); // sNaN 的符号跟着走
    }

    TEST_CASE("二元运算 NaN：sNaN 优先于安静 NaN，左操作数优先于右操作数") {
        DecContext ctx{quiet_context()};
        CHECK(d("-sNaN").add(d("sNaN"), ctx).to_string() == "-NaN"); // 两边都是 sNaN，跟左边
        CHECK(ctx.flags().has(DecCondition::InvalidOperation));
        DecContext c2{quiet_context()};
        CHECK(d("NaN").add(d("-sNaN"), c2).to_string() == "-NaN"); // 安静 vs sNaN，跟 sNaN
        CHECK(c2.flags().has(DecCondition::InvalidOperation));
        DecContext c3{quiet_context()};
        CHECK(d("-NaN").add(d("NaN"), c3).to_string() == "-NaN"); // 两个安静 NaN，跟左边
        CHECK(c3.flags().empty());
    }

    TEST_CASE("±Infinity 的一元 ±") {
        DecContext ctx{quiet_context()};
        CHECK(d("Infinity").plus(ctx).to_string() == "Infinity");
        CHECK(d("-Infinity").plus(ctx).to_string() == "-Infinity");
        CHECK(d("Infinity").minus(ctx).to_string() == "-Infinity");
        CHECK(d("-Infinity").minus(ctx).to_string() == "Infinity");
        CHECK(ctx.flags().empty());
    }
}
TEST_SUITE("BigDec——// 和 % 的向负无穷取整语义") {

    TEST_CASE("四种符号组合（跟 SL 的 int 一致，跟 IBM 规范的向零截断不一致）") {
        DecContext ctx{quiet_context()};
        CHECK(d("7").floor_div(d("3"), ctx).to_string() == "2");
        CHECK(d("-7").floor_div(d("3"), ctx).to_string() == "-3"); // 规范会给 -2
        CHECK(d("7").floor_div(d("-3"), ctx).to_string() == "-3");
        CHECK(d("-7").floor_div(d("-3"), ctx).to_string() == "2");

        CHECK(d("7").mod(d("3"), ctx).to_string() == "1");
        CHECK(d("-7").mod(d("3"), ctx).to_string() == "2"); // 规范会给 -1
        CHECK(d("7").mod(d("-3"), ctx).to_string() == "-2");
        CHECK(d("-7").mod(d("-3"), ctx).to_string() == "-1");
    }

    TEST_CASE("非零余数的符号恒同除数") {
        DecContext ctx{quiet_context()};
        for (const char *const a : {"7", "-7", "7.5", "-7.5", "0.1", "-0.1", "1E+10"}) {
            for (const char *const b : {"3", "-3", "0.7", "-0.7", "2.5", "-2.5"}) {
                const BigDec remainder{d(a).mod(d(b), ctx)};
                CAPTURE(a);
                CAPTURE(b);
                REQUIRE(remainder.is_finite());
                if (remainder.is_zero()) continue;
                CHECK(remainder.is_negative() == d(b).is_negative());
            }
        }
    }

    TEST_CASE("余数恰好为零时符号跟被除数走（照抄规范的一处角落）") {
        DecContext ctx{quiet_context()};
        CHECK(d("6").mod(d("3"), ctx).to_string() == "0");
        CHECK(d("-6").mod(d("3"), ctx).to_string() == "-0");
        CHECK(d("6").mod(d("-3"), ctx).to_string() == "0");
        CHECK(d("-6").mod(d("-3"), ctx).to_string() == "-0");
        // 数值上仍然是零，只有 to_string 看得出区别
        CHECK(d("-6").mod(d("3"), ctx).equals(d("0"), ctx));
    }

    TEST_CASE("商为零时符号按两个操作数异或补上（不让 // 成为唯一丢符号的运算）") {
        DecContext ctx{quiet_context()};
        CHECK(d("0").floor_div(d("3"), ctx).to_string() == "0");
        CHECK(d("0").floor_div(d("-3"), ctx).to_string() == "-0");
        CHECK(d("-0").floor_div(d("3"), ctx).to_string() == "-0");
        CHECK(d("-0").floor_div(d("-3"), ctx).to_string() == "0");
        CHECK(d("1").floor_div(d("3"), ctx).to_string() == "0");
        // 一正一负、余数非零时会被修正成 -1，不是 -0
        CHECK(d("1").floor_div(d("-3"), ctx).to_string() == "-1");
    }

    // 精确算术下成立；有舍入时右边乘法再舍一次，不保证逐位相等。值池温和，乘积顶不到 prec。
    TEST_CASE("恒等式 x % y == x - (x // y) * y（温和值池下逐位成立，非普遍恒等式）") {
        DecContext ctx{quiet_context()};
        constexpr const char *const pool[]{
            "0",   "-0",   "1",    "-1",    "7",    "-7",    "3",          "-3",
            "2.5", "-2.5", "0.1",  "-0.1",  "100",  "-100",  "1.50",       "-1.50",
            "6",   "-6",   "1E+5", "-1E+5", "1E-5", "-1E-5", "12345.6789", "0.7",
        };
        for (const char *const a : pool) {
            for (const char *const b : pool) {
                if (d(b).is_zero()) continue;
                CAPTURE(a);
                CAPTURE(b);
                const BigDec quotient{d(a).floor_div(d(b), ctx)};
                if (!quotient.is_finite()) continue; // DivisionImpossible，恒等式无从谈起
                const BigDec expected{d(a).sub(quotient.mul(d(b), ctx), ctx)};
                const BigDec actual{d(a).mod(d(b), ctx)};
                CHECK(actual.equals(expected, ctx));
            }
        }
    }

    TEST_CASE("反例：x - (x // y) * y 的乘法自己也要舍入，跟 % 直接舍出的余数逐位不同") {
        // prec 28 下的具体反例，跟 CPython 的 Decimal 核对过：x // y == 3，但 3 * y 已经是 29
        // 位、要被舍入一次，再拿去减就比 % 直接算出的精确余数多丢了一点精度。两边在数学意义上
        // 都是"正确"的（% 就该等于精确余数舍入后的样子），只是不逐位相等——钉住这个反例，防止
        // 有人真的拿"逐位恒等式"的假设去优化或校验代码
        DecContext ctx{quiet_context()};
        const BigDec x{d("1")};
        const BigDec y{d("0.30000000000000000000000000009")};
        const BigDec quotient{x.floor_div(y, ctx)};
        const BigDec remainder{x.mod(y, ctx)};
        CHECK(quotient.to_string() == "3");
        CHECK(remainder.to_string() == "0.09999999999999999999999999973");
        const BigDec via_multiply{x.sub(quotient.mul(y, ctx), ctx)};
        CHECK(via_multiply.to_string() == "0.0999999999999999999999999997"); // 少一位有效数字
        CHECK_FALSE(remainder.identical(via_multiply));
        CHECK(remainder.equals(via_multiply, ctx) == false); // 数值上也确实不相等，不只是标度不同
    }

    TEST_CASE("divmod 跟单独算 // 和 % 一致") {
        constexpr const char *const pool[]{
            "0",
            "-0",
            "1",
            "-1",
            "7",
            "-7",
            "3",
            "-3",
            "2.5",
            "0.1",
            "1E+5",
            "1E-5",
            "0.7",
            "Infinity",
            "-Infinity",
            "NaN",
            "1.50",
            "-12345.6789"
        };
        for (const char *const a : pool) {
            for (const char *const b : pool) {
                CAPTURE(a);
                CAPTURE(b);
                DecContext c1{quiet_context()};
                DecContext c2{quiet_context()};
                DecContext c3{quiet_context()};
                const auto [quotient, remainder]{d(a).divmod(d(b), c1)};
                CHECK(quotient.identical(d(a).floor_div(d(b), c2)));
                CHECK(remainder.identical(d(a).mod(d(b), c3)));
                // divmod 的信号是两边信号的并集
                DecSignalSet both{c2.flags()};
                for (const DecCondition condition :
                     {DecCondition::Clamped,
                      DecCondition::DivisionByZero,
                      DecCondition::Inexact,
                      DecCondition::InvalidOperation,
                      DecCondition::Overflow,
                      DecCondition::Rounded,
                      DecCondition::Subnormal,
                      DecCondition::Underflow}) {
                    if (c3.flags().has(condition)) both.add(condition);
                }
                CHECK(flags_to_string(c1.flags()) == flags_to_string(both));
            }
        }
    }

    TEST_CASE("跟 BigInt 的 floor_div/mod 交叉验证（整数值的 decimal）") {
        DecContext ctx{quiet_context()};
        constexpr const char *const pool[]{
            "0",
            "1",
            "-1",
            "2",
            "-2",
            "7",
            "-7",
            "13",
            "-13",
            "100",
            "-100",
            "12345",
            "-12345",
            "999999",
            "-999999",
            "3",
            "-3",
        };
        for (const char *const a : pool) {
            for (const char *const b : pool) {
                if (BigInt::from_decimal_string(b).is_zero()) continue;
                CAPTURE(a);
                CAPTURE(b);
                const BigInt int_a{BigInt::from_decimal_string(a)};
                const BigInt int_b{BigInt::from_decimal_string(b)};
                const BigDec quotient{d(a).floor_div(d(b), ctx)};
                const BigDec remainder{d(a).mod(d(b), ctx)};
                CHECK(quotient.equals(BigDec::from_bigint(int_a.floor_div(int_b)), ctx));
                CHECK(remainder.equals(BigDec::from_bigint(int_a.mod(int_b)), ctx));
            }
        }
    }

    TEST_CASE("除数是无穷：真商恰好是 0，不需要向负无穷再修正一格") {
        DecContext ctx{quiet_context()};
        CHECK(d("1").floor_div(d("Infinity"), ctx).to_string() == "0");
        CHECK(d("1").mod(d("Infinity"), ctx).to_string() == "1");
        // 一正一负：真商是精确的 0（带符号的 -0），floor(-0) 仍是 -0，不会再减 1；
        // 余数就是被除数本身，不会被"修正"成无穷
        CHECK(d("1").floor_div(d("-Infinity"), ctx).to_string() == "-0");
        CHECK(d("1").mod(d("-Infinity"), ctx).to_string() == "1");
        CHECK(d("-1").floor_div(d("Infinity"), ctx).to_string() == "-0");
        CHECK(d("-1").mod(d("Infinity"), ctx).to_string() == "-1");
        CHECK(d("0").floor_div(d("Infinity"), ctx).to_string() == "0");
        CHECK(d("0").mod(d("Infinity"), ctx).to_string() == "0");
    }

    TEST_CASE("商放不下时报 DivisionImpossible") {
        DecContext ctx{quiet_context(5)};
        CHECK(d("1E+10").floor_div(d("1"), ctx).to_string() == "NaN");
        CHECK(ctx.flags().has(DecCondition::InvalidOperation));
        ctx.flags().clear();
        CHECK(d("99999").floor_div(d("1"), ctx).to_string() == "99999"); // 恰好 5 位，可以
        CHECK(ctx.flags().empty());
        CHECK(d("100000").floor_div(d("1"), ctx).to_string() == "NaN");

        // 位数只有按修正**之后**的商算才对：这里截断商是 -99999（5 位，放得下），修正成
        // -100000 才多出一位。按截断商判就会漏掉这一例
        DecContext c2{quiet_context(5)};
        CHECK(d("-99999.5").floor_div(d("1"), c2).to_string() == "NaN");
        CHECK(c2.flags().has(DecCondition::InvalidOperation));
        DecContext c3{quiet_context(5)};
        CHECK(d("-99998.5").floor_div(d("1"), c3).to_string() == "-99999"); // 还没越界
        CHECK(c3.flags().empty());
    }

    TEST_CASE("指数差极大时不会去造几百万位的中间值") {
        // 这几个组合按"先精确算再舍入"的写法会先构造 200 万位的系数，跑不动；
        // 能在这里正常返回本身就是结论
        DecContext ctx{quiet_context()};
        CHECK(d("1E-999999").mod(d("1E+999999"), ctx).to_string() == "1E-999999");
        CHECK(
            d("1E-999999").mod(d("-1E+999999"), ctx).to_string() ==
            "-1.000000000000000000000000000E+999999"
        );
        CHECK(d("1E-999999").floor_div(d("-1E+999999"), ctx).to_string() == "-1");
        CHECK(d("1E+999999").mod(d("1E-999999"), ctx).to_string() == "NaN"); // 商放不下
    }
}
TEST_SUITE("BigDec——比较") {

    TEST_CASE("标度不影响相等") {
        DecContext ctx{quiet_context()};
        CHECK(d("1.5").equals(d("1.50"), ctx));
        CHECK(d("1.5").equals(d("1.5000"), ctx));
        CHECK(d("10").equals(d("1E+1"), ctx));
        CHECK(d("0").equals(d("0.000"), ctx));
        CHECK(d("0").equals(d("-0"), ctx));
        CHECK(d("0").equals(d("0E+100"), ctx));
        CHECK(ctx.flags().empty());
    }

    TEST_CASE("序比较") {
        DecContext ctx{quiet_context()};
        CHECK(d("1").compare_ordering(d("2"), ctx) == std::partial_ordering::less);
        CHECK(d("2").compare_ordering(d("1"), ctx) == std::partial_ordering::greater);
        CHECK(d("1.5").compare_ordering(d("1.50"), ctx) == std::partial_ordering::equivalent);
        CHECK(d("-1").compare_ordering(d("1"), ctx) == std::partial_ordering::less);
        CHECK(d("-0").compare_ordering(d("0"), ctx) == std::partial_ordering::equivalent);
        CHECK(d("-Infinity").compare_ordering(d("-1E+999999"), ctx) == std::partial_ordering::less);
        CHECK(
            d("Infinity").compare_ordering(d("Infinity"), ctx) == std::partial_ordering::equivalent
        );
        CHECK(d("1E-999999").compare_ordering(d("0"), ctx) == std::partial_ordering::greater);
        CHECK(d("-1E-999999").compare_ordering(d("0"), ctx) == std::partial_ordering::less);
        CHECK(ctx.flags().empty());
    }

    TEST_CASE("NaN：== 静默为假，序比较报信号") {
        DecContext ctx{quiet_context()};
        CHECK_FALSE(d("NaN").equals(d("NaN"), ctx));
        CHECK_FALSE(d("NaN").equals(d("1"), ctx));
        CHECK_FALSE(d("1").equals(d("NaN"), ctx));
        CHECK(ctx.flags().empty());
        CHECK_FALSE(d("sNaN").equals(d("1"), ctx));
        CHECK(ctx.flags().has(DecCondition::InvalidOperation));

        DecContext c2{quiet_context()};
        CHECK(d("NaN").compare_ordering(d("NaN"), c2) == std::partial_ordering::unordered);
        CHECK(c2.flags().has(DecCondition::InvalidOperation));
    }

    TEST_CASE("比较的自洽性：反对称、跟 equals 一致") {
        DecContext ctx{quiet_context()};
        const std::vector<BigDec> values{interesting_values()};
        for (const BigDec &a : values) {
            for (const BigDec &b : values) {
                CAPTURE(a.to_string());
                CAPTURE(b.to_string());
                const std::partial_ordering ab{a.compare_ordering(b, ctx)};
                const std::partial_ordering ba{b.compare_ordering(a, ctx)};
                if (ab == std::partial_ordering::unordered) {
                    CHECK(ba == std::partial_ordering::unordered);
                    CHECK_FALSE(a.equals(b, ctx));
                    continue;
                }
                CHECK(
                    (ab == std::partial_ordering::less) == (ba == std::partial_ordering::greater)
                );
                CHECK(
                    (ab == std::partial_ordering::equivalent) ==
                    (ba == std::partial_ordering::equivalent)
                );
                CHECK(a.equals(b, ctx) == (ab == std::partial_ordering::equivalent));
            }
        }
    }

    TEST_CASE("比较不受精度影响：prec 1 也不会把 1.0000001 和 1 看成相等") {
        DecContext ctx{quiet_context(1)};
        CHECK_FALSE(d("1.0000001").equals(d("1"), ctx));
        CHECK(d("1.0000001").compare_ordering(d("1"), ctx) == std::partial_ordering::greater);
        CHECK(ctx.flags().empty()); // 比较不舍入，也就不该报 Inexact/Rounded
    }
}
