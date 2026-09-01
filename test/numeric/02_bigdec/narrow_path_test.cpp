// 生成表铺不到的窄路径。
#include "test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("BigDec——生成的用例表铺不到的窄路径") {

    TEST_CASE("// 的商也要过 fix：位数够但指数域不够时报 Overflow") {
        // Emax 比 prec - 1 还小的上下文才碰得到；生成的表里 prec/Emax 组合不含这一档
        DecContext ctx{quiet_context(28, DecRounding::HalfEven, 3, -999999)};
        CHECK(d("100000").floor_div(d("1"), ctx).to_string() == "Infinity");
        CHECK(flags_to_string(ctx.flags()) == "Inexact,Overflow,Rounded");
        // 余数不受影响，它本来就一直在过 fix
        DecContext c2{quiet_context(28, DecRounding::HalfEven, 3, -999999)};
        CHECK(d("100000").mod(d("1"), c2).to_string() == "0");
        CHECK(c2.flags().empty());
        // divmod 两边一致
        DecContext c3{quiet_context(28, DecRounding::HalfEven, 3, -999999)};
        const auto [quotient, remainder]{d("100000").divmod(d("1"), c3)};
        CHECK(quotient.to_string() == "Infinity");
        CHECK(remainder.to_string() == "0");
        CHECK(flags_to_string(c3.flags()) == "Inexact,Overflow,Rounded");
    }

    TEST_CASE("% 和 divmod 也要按修正之后的商判 DivisionImpossible") {
        // 截断商 -99999 是 5 位放得下，修正成 -100000 才多出一位。// 那边有对应用例，
        // 这里补上另外两个入口——它们各自独立查这一条，漏一个恒等式就不成立了
        DecContext ctx{quiet_context(5)};
        CHECK(d("-99999.5").mod(d("1"), ctx).to_string() == "NaN");
        CHECK(flags_to_string(ctx.flags()) == "InvalidOperation");
        DecContext c2{quiet_context(5)};
        const auto [quotient, remainder]{d("-99999.5").divmod(d("1"), c2)};
        CHECK(quotient.to_string() == "NaN");
        CHECK(remainder.to_string() == "NaN");
        CHECK(flags_to_string(c2.flags()) == "InvalidOperation");
    }

    TEST_CASE("小 prec + 巨大指数：dlog/dlog10 里 p <= 0 的那一支") {
        // places = prec - log10_exp_bound() + 2 要小到非正，得指数大到 10 万量级、prec 又只有 1
        DecContext ctx{quiet_context(1)};
        CHECK(d("2E+100000").log10(ctx).to_string() == "1E+5");
        CHECK(flags_to_string(ctx.flags()) == "Inexact,Rounded");
        DecContext c2{quiet_context(1)};
        CHECK(d("2E+100000").ln(c2).to_string() == "2E+5");
        CHECK(flags_to_string(c2.flags()) == "Inexact,Rounded");
    }

    TEST_CASE("指数小到 y*log(x) 都归零：dpower 里贴着 1 的那一支") {
        // 这时得给一个"不正好等于 1"的近似值，否则上层判不出能不能定夺舍入方向，会死循环
        DecContext ctx{quiet_context()};
        for (const char *const base : {"2", "0.5"}) {
            for (const char *const exponent : {"1E-999999999", "-1E-999999999"}) {
                CAPTURE(base);
                CAPTURE(exponent);
                DecContext c{quiet_context()};
                CHECK(d(base).pow(d(exponent), c).to_string() == "1.000000000000000000000000000");
                CHECK(flags_to_string(c.flags()) == "Inexact,Rounded");
            }
        }
        CHECK(ctx.flags().empty());
    }

    TEST_CASE("** 的近似值不够定夺舍入方向时要再多算三位（dpower 那圈重试循环）") {
        // 判据是"末尾恰好是 5000…0"。这几组是拿 _pydecimal 的 _dpower 扫出来的：首轮 prec+3 位
        // 算出来正好卡在两个可表示值中间，非得再转一圈不可。随机生成的表撞不到这种巧合，
        // 而少了这圈重试，下面每一条的末位都会错
        struct Case {
            const char *base;
            const char *exponent;
            int32_t prec;
            const char *expected;
        };
        constexpr Case cases[]{
            {"3", "-2.5", 3, "0.0642"},
            {"13", "1.3", 5, "28.062"},
            {"2", "0.25", 9, "1.18920712"},
            {"5", "-0.5", 10, "0.4472135955"},
            {"0.2", "0.5", 9, "0.447213595"},
        };
        for (const Case &c : cases) {
            CAPTURE(c.base);
            CAPTURE(c.exponent);
            DecContext ctx{quiet_context(c.prec)};
            CHECK(d(c.base).pow(d(c.exponent), ctx).to_string() == c.expected);
            CHECK(flags_to_string(ctx.flags()) == "Inexact,Rounded");
        }
    }

    TEST_CASE("power_exact 负指数分支的三道上限：e 超过 emax、结果系数位数超过 p") {
        // 这三条都得让 power_exact 中途放弃、退回 exp(y*log(x))。生成的表里 prec 都太大，
        // 撞不到 emax 那两道闸；位数那道闸更窄，要 p 大到 5^emax < 10^p 这个估计开始变松
        DecContext ctx{quiet_context(3)};
        CHECK(d("1024").pow(d("-3"), ctx).to_string() == "9.31E-10"); // 2 的幂：e*|y| > emax
        CHECK(flags_to_string(ctx.flags()) == "Inexact,Rounded");
        DecContext c2{quiet_context(1)};
        CHECK(d("125").pow(d("-3"), c2).to_string() == "5E-7"); // 5 的幂：同上
        CHECK(flags_to_string(c2.flags()) == "Inexact,Rounded");
        // 5^-293 精确值是 2^293×10^-293，2^293 有 89 位，比 p = prec + 1 = 88 多一位
        DecContext c3{quiet_context(87)};
        CHECK(
            d("5").pow(d("-293"), c3).to_string() ==
            "1.59143435651131725489722319406982668832145968255151269580948472605811039044010680"
            "170578E-205"
        );
        CHECK(flags_to_string(c3.flags()) == "Inexact,Rounded");
    }

    TEST_CASE("power_exact 的理想指数补零会被 p - 位数 夹住") {
        // 底数带标度、指数是正整数时结果要往理想指数靠，但补的零不能让系数超过 p 位。
        // 2.000 ** 3 的理想指数是 -9，补满要 9 个零，p - 1 = 3 把它夹到 3 个
        DecContext ctx{quiet_context(3)};
        CHECK(d("2.000").pow(d("3"), ctx).to_string() == "8.00");
        CHECK(flags_to_string(ctx.flags()) == "Rounded");
    }

    TEST_CASE("log10_digits 的缓存扩容：既要剥掉不可靠的尾零，也要在算不准时再多算三位") {
        // 这是 numeric/ 里唯一一处直接戳 dec_math 的用例：log(10) 这个常数被 ln/log10/exp/**
        // 全体依赖，而"多算几位 → 末几位全 0 说明还没定下来 → 再多算三位 → 剥掉尾零连同紧挨着
        // 的那一位"这段簿记，只有特定的 p 才走得到，从 BigDec 那一层没法定向命中。
        //
        // p 是拿 log(10) 的真实数字扫出来的：p = 176 走剥尾零那一支，p = 409 是 2500 以内唯一
        // 需要重试的。两者都远高于其余用例摸得到的量级（prec 最大 100，折算过去约 125），
        // 所以不管 doctest 用什么顺序跑，这两次调用都会真的触发重算。
        // 代价是 p = 409 那次约 1.6 秒（BigInt 是朴素算法）——嫌慢可以砍掉它，剥尾零那条很便宜
        constexpr const char *const kLog10Digits{
            // log(10) 的前 410 位有效数字
            "23025850929940456840179914546843642076011014886287729760333279009"
            "67572609677352480235997205089598298341967784042286248633409525465"
            "08280675666628736909878168948290720832555468084379989482623319852"
            "83935053089653777326288461633662222876982198867465436674744042432"
            "74365155048934314939391479619404400222105101714174800368808401264"
            "70806855677432162283552201148046637156591213734507478569476834636"
            "16792101806445070648"
        };
        const std::string expected{kLog10Digits};
        REQUIRE(expected.size() == 410);
        CHECK(dec_math::log10_digits(176).to_decimal_string() == expected.substr(0, 177));
        CHECK(dec_math::log10_digits(409).to_decimal_string() == expected);
        // 缓存只增不减：回头要小一点的 p，切出来的还得对
        CHECK(dec_math::log10_digits(0).to_decimal_string() == "2");
        CHECK(dec_math::log10_digits(46).to_decimal_string() == expected.substr(0, 47));
    }

    TEST_CASE("每个条件都有名字，包括 BigDec 自己不会产生的 InvalidContext") {
        // SL 那边要靠这个名字把 DecTrapped 映射成对应的异常类，不能有漏网的
        constexpr DecCondition all[]{
            DecCondition::Clamped,
            DecCondition::DivisionByZero,
            DecCondition::Inexact,
            DecCondition::InvalidOperation,
            DecCondition::Overflow,
            DecCondition::Rounded,
            DecCondition::Subnormal,
            DecCondition::Underflow,
            DecCondition::ConversionSyntax,
            DecCondition::DivisionImpossible,
            DecCondition::DivisionUndefined,
            DecCondition::InvalidContext
        };
        for (const DecCondition condition : all) {
            const std::string name{dec_condition_name(condition)};
            CAPTURE(name);
            CHECK_FALSE(name.empty());
            CHECK(name != "?");
        }
        CHECK(std::string{dec_condition_name(DecCondition::InvalidContext)} == "InvalidContext");
        CHECK(signal_of(DecCondition::InvalidContext) == DecCondition::InvalidOperation);
    }
}
