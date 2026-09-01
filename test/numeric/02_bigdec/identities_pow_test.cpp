// 代数恒等式、幂与超越函数、operator 与具名方法。
#include "test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("BigDec——代数恒等式（不依赖生成的用例表）") {

    TEST_CASE("加法/乘法交换律：连标度都该一样") {
        DecContext ctx{quiet_context()};
        const std::vector<BigDec> values{interesting_values()};
        for (const BigDec &a : values) {
            for (const BigDec &b : values) {
                CAPTURE(a.to_string());
                CAPTURE(b.to_string());
                if (a.is_nan() || b.is_nan()) continue; // NaN 传播是有先后的，不满足交换律
                CHECK(a.add(b, ctx).identical(b.add(a, ctx)));
                CHECK(a.mul(b, ctx).identical(b.mul(a, ctx)));
            }
        }
    }

    TEST_CASE("a - b 就是 a + (-b)") {
        const std::vector<BigDec> values{interesting_values()};
        for (const BigDec &a : values) {
            for (const BigDec &b : values) {
                CAPTURE(a.to_string());
                CAPTURE(b.to_string());
                // NaN 是例外：sub 必须赶在取负之前判 NaN，不然传出去的 NaN 符号会被翻反
                if (a.is_nan() || b.is_nan()) continue;
                DecContext c1{quiet_context()};
                DecContext c2{quiet_context()};
                CHECK(a.sub(b, c1).identical(a.add(b.copy_negate(), c2)));
                CHECK(flags_to_string(c1.flags()) == flags_to_string(c2.flags()));
            }
        }
    }

    TEST_CASE("-(-a) 跟 +a 一样（零和 NaN 也不例外）") {
        DecContext ctx{quiet_context()};
        for (const BigDec &a : interesting_values()) {
            CAPTURE(a.to_string());
            if (a.is_signaling_nan()) continue; // sNaN 每过一次运算都会报信号，不好逐次比
            CHECK(a.minus(ctx).minus(ctx).identical(a.plus(ctx).plus(ctx)));
        }
    }

    TEST_CASE("x + 0 保住数值，标度按规范取两者较小的那个") {
        DecContext ctx{quiet_context()};
        CHECK(d("1.5").add(d("0"), ctx).to_string() == "1.5");
        CHECK(d("1.5").add(d("0.000"), ctx).to_string() == "1.500");
        CHECK(d("1.5").add(d("0E+5"), ctx).to_string() == "1.5");
        CHECK(d("0").add(d("0.00"), ctx).to_string() == "0.00");
        CHECK(d("-0").add(d("-0"), ctx).to_string() == "-0");
        CHECK(d("0").add(d("-0"), ctx).to_string() == "0");
        CHECK(d("-0").add(d("0"), ctx).to_string() == "0");
        // ROUND_FLOOR 下一正一负加出来的零是负的
        DecContext floor_ctx{quiet_context(28, DecRounding::Floor)};
        CHECK(d("0").add(d("-0"), floor_ctx).to_string() == "-0");
        CHECK(d("1.5").add(d("-1.5"), floor_ctx).to_string() == "-0.0");
        CHECK(d("1.5").add(d("-1.5"), ctx).to_string() == "0.0");
    }

    TEST_CASE("理想指数：乘法是指数相加，除法尽量往指数相减靠") {
        DecContext ctx{quiet_context()};
        CHECK(d("1.20").mul(d("1.30"), ctx).to_string() == "1.5600");
        CHECK(d("1.2").mul(d("2"), ctx).to_string() == "2.4");
        CHECK(d("100").mul(d("0.01"), ctx).to_string() == "1.00");
        CHECK(d("2.40").div(d("2"), ctx).to_string() == "1.20");
        CHECK(d("2.400").div(d("2.0"), ctx).to_string() == "1.20");
        CHECK(d("1").div(d("8"), ctx).to_string() == "0.125");
        CHECK(d("1").div(d("2"), ctx).to_string() == "0.5");
        CHECK(d("10").div(d("2"), ctx).to_string() == "5");
        CHECK(d("1").div(d("3"), ctx).to_string() == "0.3333333333333333333333333333");
    }

    TEST_CASE("0.1 + 0.2 == 0.3，decimal 的立身之本") {
        DecContext ctx;
        CHECK(d("0.1").add(d("0.2"), ctx).to_string() == "0.3");
        CHECK(d("0.1").add(d("0.2"), ctx).equals(d("0.3"), ctx));
        CHECK(ctx.flags().empty()); // 精确，连 Inexact 都不该有
        // 金额场景：末尾零不会被剥掉
        CHECK(d("1.10").add(d("2.00"), ctx).to_string() == "3.10");
        CHECK(d("19.99").mul(d("3"), ctx).to_string() == "59.97");
    }
}
TEST_SUITE("BigDec——幂运算与超越函数") {

    TEST_CASE("** 的基本值与理想指数") {
        DecContext ctx{quiet_context()};
        CHECK(d("2").pow(d("10"), ctx).to_string() == "1024");
        CHECK(d("2").pow(d("-1"), ctx).to_string() == "0.5");
        CHECK(d("10").pow(d("-3"), ctx).to_string() == "0.001");
        CHECK(d("-2").pow(d("2"), ctx).to_string() == "4");
        CHECK(d("-2").pow(d("3"), ctx).to_string() == "-8"); // 奇数次幂才带负号
        CHECK(d("5").pow(d("0"), ctx).to_string() == "1");
        CHECK(d("0").pow(d("3"), ctx).to_string() == "0");
        CHECK(d("-0").pow(d("3"), ctx).to_string() == "-0");
        CHECK(d("-0").pow(d("2"), ctx).to_string() == "0");
        CHECK(ctx.flags().empty()); // 以上全是精确的整数次幂

        // 指数是非负整数时，结果的标度往"底数标度 × 指数"靠
        CHECK(d("1.5").pow(d("2"), ctx).to_string() == "2.25");
        CHECK(d("2.00").pow(d("2"), ctx).to_string() == "4.0000");
        CHECK(d("1.00").pow(d("3"), ctx).to_string() == "1.000000");
        CHECK(d("1.00").pow(d("-3"), ctx).to_string() == "1"); // 负指数没有理想指数
        CHECK(ctx.flags().empty());
        // 指数写成带末尾零的整数（3.00 / 1.0），要按整数次幂走理想指数，不能当成非整数
        CHECK(d("2.00").pow(d("3.00"), ctx).to_string() == "8.000000");
        CHECK(d("5.0").pow(d("1.0"), ctx).to_string() == "5.0");
        CHECK(d("-1.0").pow(d("2"), ctx).to_string() == "1.00");
        CHECK(d("-1.0").pow(d("3"), ctx).to_string() == "-1.000");
        CHECK(d("-2.5").pow(d("2.0"), ctx).to_string() == "6.25");
        CHECK(d("-2.5").pow(d("3.00"), ctx).to_string() == "-15.625");
        CHECK(d("10.0").pow(d("-1"), ctx).to_string() == "0.1");
        CHECK(d("0.1").pow(d("-1"), ctx).to_string() == "1E+1"); // 负指数、xc==1，没有理想指数
        CHECK(d("0E+5").pow(d("2"), ctx).to_string() == "0");    // 零的幂指数一律归 0
        CHECK(ctx.flags().empty());
    }

    TEST_CASE("** 的特殊情形") {
        DecContext ctx{quiet_context()};
        CHECK(d("0").pow(d("-1"), ctx).to_string() == "Infinity");
        CHECK(d("-0").pow(d("-1"), ctx).to_string() == "-Infinity");
        CHECK(d("2").pow(d("Infinity"), ctx).to_string() == "Infinity");
        CHECK(d("2").pow(d("-Infinity"), ctx).to_string() == "0");
        CHECK(d("0.5").pow(d("Infinity"), ctx).to_string() == "0");
        CHECK(d("0.5").pow(d("-Infinity"), ctx).to_string() == "Infinity");
        CHECK(d("Infinity").pow(d("2"), ctx).to_string() == "Infinity");
        CHECK(d("Infinity").pow(d("-2"), ctx).to_string() == "0");
        CHECK(d("-Infinity").pow(d("3"), ctx).to_string() == "-Infinity");
        CHECK(d("-Infinity").pow(d("2"), ctx).to_string() == "Infinity");
        CHECK(d("-Infinity").pow(d("2.0"), ctx).to_string() == "Infinity"); // 2.0 必须被认成偶整数
        CHECK(d("-Infinity").pow(d("3.00"), ctx).to_string() == "-Infinity");
        CHECK(ctx.flags().empty());

        // 0 ** 0 无意义；负数的非整数次幂不是实数
        CHECK(d("0").pow(d("0"), ctx).to_string() == "NaN");
        CHECK(ctx.flags().has(DecCondition::InvalidOperation));
        DecContext c2{quiet_context()};
        CHECK(d("-2").pow(d("0.5"), c2).to_string() == "NaN");
        CHECK(c2.flags().has(DecCondition::InvalidOperation));
        // 但 (-0) ** 非整数 按 0 ** 非整数 算，不报错
        DecContext c3{quiet_context()};
        CHECK(d("-0").pow(d("0.5"), c3).to_string() == "0");
        CHECK(c3.flags().empty());
    }

    TEST_CASE("指数不是整数时，结果就算精确也要报 Inexact") {
        // 规范这么要求；fix 自己不会报，是 pow 事后补上去的
        DecContext ctx{quiet_context()};
        CHECK(d("4").pow(d("0.5"), ctx).to_string() == "2.000000000000000000000000000");
        CHECK(flags_to_string(ctx.flags()) == "Inexact,Rounded");
        DecContext c2{quiet_context()};
        CHECK(d("0.25").pow(d("-0.5"), c2).to_string() == "2.000000000000000000000000000");
        CHECK(flags_to_string(c2.flags()) == "Inexact,Rounded");
        DecContext c3{quiet_context()};
        CHECK(d("1E+10").pow(d("0.5"), c3).to_string() == "100000.0000000000000000000000");
        CHECK(flags_to_string(c3.flags()) == "Inexact,Rounded");
        // 整数指数就照常，精确就是精确
        DecContext c4{quiet_context()};
        CHECK(d("2").pow(d("10"), c4).to_string() == "1024");
        CHECK(c4.flags().empty());
    }

    TEST_CASE("指数不是整数、结果精确但落进次正规/溢出区：Underflow/Overflow 照样要补报") {
        // 跟 CPython 的 Decimal 核对过。这两条走的是 power() 里"exact 分支算出的结果落在次正规区
        // /溢出区"那段专门补 Underflow/Overflow 的路径——精确路径本身不会经过 fix() 的溢出/下溢
        // 判断，得手动把信号照规范补上
        DecContext ctx{quiet_context(5, DecRounding::HalfEven, 9, -9)};
        CHECK(d("1E-100").pow(d("0.5"), ctx).to_string() == "0E-13");
        CHECK(flags_to_string(ctx.flags()) == "Clamped,Inexact,Rounded,Subnormal,Underflow");

        DecContext c2{quiet_context(5, DecRounding::HalfEven, 100, -999999999)};
        CHECK(d("1E+400").pow(d("0.5"), c2).to_string() == "Infinity");
        CHECK(flags_to_string(c2.flags()) == "Inexact,Overflow,Rounded");
    }

    TEST_CASE("指数不是整数、结果精确、又开着陷阱：补报信号的顺序决定抛哪个、抛时 flags 到哪一步") {
        // power() 尾部那段是先在一个陷阱全关的副本上 fix，再按 Overflow → Underflow → Subnormal
        // → Inexact → Rounded → Clamped 的顺序补报到真上下文。顺序错了值不变，但"抛出来的是哪个
        // 条件""抛出时 flags 走到哪一步"就全变了。
        //
        // 交叉验证表钉不住这一块：下面 24 组里有 9 组 libmpdec 跟 _pydecimal 不一致（前者把 fix
        // 期间的 flags 全带上，后者只带补报到抛出点为止的），生成器遇到分歧整组跳过。我们跟
        // _pydecimal，同 .ai/context.md 记的那两处已知分歧一个立场
        struct Outcome {
            DecCondition trap;
            const char *result; // nullptr 表示会抛，抛出的条件就是 trap 自己
            const char *flags;  // 无论抛没抛，事后 flags 都得是这个
        };
        const auto check{[](const char *base,
                            const char *exponent,
                            const int32_t prec,
                            const int32_t emax,
                            const int32_t emin,
                            const std::vector<Outcome> &outcomes) {
            for (const Outcome &o : outcomes) {
                CAPTURE(base);
                CAPTURE(emax);
                CAPTURE(dec_condition_name(o.trap));
                DecContext ctx{quiet_context(prec, DecRounding::HalfEven, emax, emin)};
                ctx.traps().add(o.trap);
                try {
                    const BigDec got{d(base).pow(d(exponent), ctx)};
                    CHECK(o.result != nullptr);
                    if (o.result != nullptr) CHECK(got.to_string() == o.result);
                } catch (const DecTrapped &e) {
                    CHECK(o.result == nullptr);
                    CHECK(
                        std::string{dec_condition_name(e.condition())} == dec_condition_name(o.trap)
                    );
                }
                CHECK(flags_to_string(ctx.flags()) == o.flags);
            }
        }};

        // 精确结果越过 Emax：fix 报 Overflow + Inexact + Rounded
        const std::vector<Outcome> overflowed{
            {DecCondition::Clamped, "Infinity", "Inexact,Overflow,Rounded"},
            {DecCondition::DivisionByZero, "Infinity", "Inexact,Overflow,Rounded"},
            {DecCondition::Inexact, nullptr, "Inexact,Overflow"},
            {DecCondition::InvalidOperation, "Infinity", "Inexact,Overflow,Rounded"},
            {DecCondition::Overflow, nullptr, "Overflow"},
            {DecCondition::Rounded, nullptr, "Inexact,Overflow,Rounded"},
            {DecCondition::Subnormal, "Infinity", "Inexact,Overflow,Rounded"},
            {DecCondition::Underflow, "Infinity", "Inexact,Overflow,Rounded"},
        };
        // 精确结果 1E+15 越过 Emax = 4
        check("1E+30", "0.5", 5, 4, -4, overflowed);
        // 同上，但 Emin 远在天边：补报只看 fix 报了什么，跟 Emin 本身无关
        check("1E+400", "0.5", 5, 100, -999999999, overflowed);

        // 精确结果 1E-50 落进次正规区并一路下溢到零，五个信号一起报
        const std::vector<Outcome> underflowed{
            {DecCondition::Clamped, nullptr, "Clamped,Inexact,Rounded,Subnormal,Underflow"},
            {DecCondition::DivisionByZero, "0E-13", "Clamped,Inexact,Rounded,Subnormal,Underflow"},
            {DecCondition::Inexact, nullptr, "Inexact,Subnormal,Underflow"},
            {DecCondition::InvalidOperation,
             "0E-13",
             "Clamped,Inexact,Rounded,Subnormal,Underflow"},
            {DecCondition::Overflow, "0E-13", "Clamped,Inexact,Rounded,Subnormal,Underflow"},
            {DecCondition::Rounded, nullptr, "Inexact,Rounded,Subnormal,Underflow"},
            {DecCondition::Subnormal, nullptr, "Subnormal,Underflow"},
            {DecCondition::Underflow, nullptr, "Underflow"},
        };
        check("1E-100", "0.5", 5, 9, -9, underflowed);
    }

    TEST_CASE("精确结果 + 定向舍入：我们跟 _pydecimal 一致，跟 libmpdec 差 1 ulp") {
        // 规范对非整数指数的 ** 只要求"按 exp(y*ln(x)) 算"，不保证正确舍入，CPython 自己的两套
        // 实现在这里就不一致（官方扩展测试把这一类列为已知差异）。我们选真值精确就原样给出的
        // 那一支——ROUND_DOWN 下 9 ** 0.5 给 2.99 实在太不像话。交叉验证表里这些组合是跳过的，
        // 行为由这个用例钉住
        struct Case {
            const char *base;
            const char *exponent;
            DecRounding rounding;
            const char *expected;
        };
        constexpr Case cases[]{
            {"4", "-0.5", DecRounding::Up, "0.500"},
            {"4", "1.5", DecRounding::Down, "8.00"},
            {"9", "0.5", DecRounding::Down, "3.00"},
            {"9", "1.5", DecRounding::Ceiling, "27.0"},
            {"1E+10", "0.5", DecRounding::Up, "1.00E+5"},
            {"1E+10", "-0.5", DecRounding::Floor, "0.0000100"},
            {"0.25", "0.5", DecRounding::ZeroFiveUp, "0.500"},
        };
        for (const Case &c : cases) {
            CAPTURE(c.base);
            CAPTURE(c.exponent);
            DecContext ctx{quiet_context(3, c.rounding)};
            CHECK(d(c.base).pow(d(c.exponent), ctx).to_string() == c.expected);
            CHECK(flags_to_string(ctx.flags()) == "Inexact,Rounded");
        }
    }

    TEST_CASE("1 ** y：值恒是 1，标度和信号看指数长什么样") {
        DecContext ctx{quiet_context()};
        CHECK(d("1").pow(d("5"), ctx).to_string() == "1");
        CHECK(d("1.00").pow(d("3"), ctx).to_string() == "1.000000");
        CHECK(d("1.00").pow(d("-3"), ctx).to_string() == "1");
        CHECK(ctx.flags().empty());
        // 指数不是整数：值仍是 1，但要报 Inexact/Rounded，标度压到 prec 位
        DecContext c2{quiet_context(5)};
        CHECK(d("1").pow(d("0.5"), c2).to_string() == "1.0000");
        CHECK(flags_to_string(c2.flags()) == "Inexact,Rounded");
        // 底数标度太深，理想指数会被 1-prec 截住，此时只报 Rounded
        DecContext c3{quiet_context(3)};
        CHECK(d("1.0000").pow(d("5"), c3).to_string() == "1.00");
        CHECK(flags_to_string(c3.flags()) == "Rounded");
    }

    TEST_CASE(
        "power_exact 里 5 的幂的 e 修正循环：底数 5^2659、指数 -1、prec 900 时估计值多算了一次，"
        "循环会真的转一圈（注释里说过现实系数上转不起来——这组是刻意凑出来的极端反例，够窄，"
        "不适合塞进随机生成的用例表，钉一条手写用例）"
    ) {
        DecContext ctx{quiet_context(900)};
        const BigInt base_coeff{BigInt(5).pow(BigInt(2659))};
        const BigDec base{BigDec::from_bigint(base_coeff)};
        const BigDec result{base.pow(d("-1"), ctx)};
        REQUIRE(result.is_finite());
        // x = 5^2659 时 1/x == 2^2659 * 10^-2659，且这个表示已经是最简形式（系数不再被 10 整除）
        CHECK(result.coefficient() == BigInt(2).pow(BigInt(2659)));
        CHECK(result.exponent() == -2659);
        CHECK(ctx.flags().empty()); // 精确，指数又是整数，不强制报 Inexact
    }

    TEST_CASE("sqrt") {
        DecContext ctx{quiet_context()};
        CHECK(d("9").sqrt(ctx).to_string() == "3");
        CHECK(d("100").sqrt(ctx).to_string() == "10");
        CHECK(d("2.25").sqrt(ctx).to_string() == "1.5");
        CHECK(d("0.01").sqrt(ctx).to_string() == "0.1");
        CHECK(d("1.00").sqrt(ctx).to_string() == "1.0"); // 理想指数是 exp/2
        CHECK(d("0").sqrt(ctx).to_string() == "0");
        CHECK(d("-0").sqrt(ctx).to_string() == "-0"); // 负零的符号留着
        CHECK(d("Infinity").sqrt(ctx).to_string() == "Infinity");
        CHECK(ctx.flags().empty()); // 完全平方数是精确的

        CHECK(d("2").sqrt(ctx).to_string() == "1.414213562373095048801688724");
        CHECK(ctx.flags().has(DecCondition::Inexact));

        DecContext c2{quiet_context()};
        CHECK(d("-1").sqrt(c2).to_string() == "NaN");
        CHECK(c2.flags().has(DecCondition::InvalidOperation));
        DecContext c3{quiet_context()};
        CHECK(d("-Infinity").sqrt(c3).to_string() == "NaN");
        CHECK(c3.flags().has(DecCondition::InvalidOperation));
    }

    TEST_CASE("ln / log10") {
        DecContext ctx{quiet_context()};
        CHECK(d("1").ln(ctx).to_string() == "0");
        CHECK(d("1.000").ln(ctx).to_string() == "0"); // 标度不影响
        CHECK(d("0").ln(ctx).to_string() == "-Infinity");
        CHECK(d("-0").ln(ctx).to_string() == "-Infinity");
        CHECK(d("Infinity").ln(ctx).to_string() == "Infinity");
        CHECK(ctx.flags().empty());
        CHECK(d("2").ln(ctx).to_string() == "0.6931471805599453094172321215");
        CHECK(d("10").ln(ctx).to_string() == "2.302585092994045684017991455");
        CHECK(d("0.5").ln(ctx).to_string() == "-0.6931471805599453094172321215");
        CHECK(d("1E+999999").ln(ctx).to_string() == "2302582.790408952689972307437");

        DecContext c2{quiet_context()};
        CHECK(d("1000").log10(c2).to_string() == "3"); // 10 的整数次幂走精确分支
        CHECK(d("0.001").log10(c2).to_string() == "-3");
        CHECK(d("1E+999999").log10(c2).to_string() == "999999");
        CHECK(d("1E-999999").log10(c2).to_string() == "-999999");
        CHECK(d("1").log10(c2).to_string() == "0");
        CHECK(c2.flags().empty());
        CHECK(d("2").log10(c2).to_string() == "0.3010299956639811952137388947");
        CHECK(c2.flags().has(DecCondition::Inexact));

        DecContext c3{quiet_context()};
        CHECK(d("-1").ln(c3).to_string() == "NaN");
        CHECK(d("-1").log10(c3).to_string() == "NaN");
        CHECK(d("-Infinity").ln(c3).to_string() == "NaN");
        CHECK(c3.flags().has(DecCondition::InvalidOperation));
    }

    TEST_CASE("exp") {
        DecContext ctx{quiet_context()};
        CHECK(d("0").exp(ctx).to_string() == "1");
        CHECK(d("-0").exp(ctx).to_string() == "1");
        CHECK(d("Infinity").exp(ctx).to_string() == "Infinity");
        CHECK(d("-Infinity").exp(ctx).to_string() == "0");
        CHECK(ctx.flags().empty());
        CHECK(d("1").exp(ctx).to_string() == "2.718281828459045235360287471");
        CHECK(d("-1").exp(ctx).to_string() == "0.3678794411714423215955237702");
        CHECK(d("2").exp(ctx).to_string() == "7.389056098930650227230427461");

        // 参数大到一定程度必然溢出/下溢，走的是不算直接给结果的快路径
        DecContext c2{quiet_context()};
        CHECK(d("1E+30").exp(c2).to_string() == "Infinity");
        CHECK(flags_to_string(c2.flags()) == "Inexact,Overflow,Rounded");
        DecContext c3{quiet_context()};
        CHECK(d("-1E+30").exp(c3).to_string() == "0E-1000026");
        CHECK(flags_to_string(c3.flags()) == "Clamped,Inexact,Rounded,Subnormal,Underflow");
        // 参数小到跟 0 分不出来时，结果贴着 1
        DecContext c4{quiet_context()};
        CHECK(d("1E-30").exp(c4).to_string() == "1.000000000000000000000000000");
        CHECK(flags_to_string(c4.flags()) == "Inexact,Rounded");
    }

    TEST_CASE("NaN 传播与 sNaN") {
        DecContext ctx{quiet_context()};
        for (const char *const op : {"sqrt", "exp", "ln", "log10"}) {
            CAPTURE(op);
            const BigDec nan{d("-NaN")};
            const BigDec result{
                std::string{op} == "sqrt"  ? nan.sqrt(ctx)
                : std::string{op} == "exp" ? nan.exp(ctx)
                : std::string{op} == "ln"  ? nan.ln(ctx)
                                           : nan.log10(ctx)
            };
            CHECK(result.to_string() == "-NaN");
        }
        CHECK(ctx.flags().empty());
        CHECK(d("NaN").pow(d("2"), ctx).to_string() == "NaN");
        CHECK(d("2").pow(d("-NaN"), ctx).to_string() == "-NaN");
        CHECK(ctx.flags().empty());

        DecContext c2{quiet_context()};
        CHECK(d("sNaN").sqrt(c2).to_string() == "NaN");
        CHECK(c2.flags().has(DecCondition::InvalidOperation));
        DecContext c3{quiet_context()};
        CHECK(d("2").pow(d("sNaN"), c3).to_string() == "NaN");
        CHECK(c3.flags().has(DecCondition::InvalidOperation));
    }

    TEST_CASE("默认上下文（陷阱开着）下这些会抛") {
        DecContext ctx;
        CHECK_THROWS_AS((void) d("-1").sqrt(ctx), DecTrapped);
        CHECK_THROWS_AS((void) d("-1").ln(ctx), DecTrapped);
        CHECK_THROWS_AS((void) d("-1").log10(ctx), DecTrapped);
        CHECK_THROWS_AS((void) d("0").pow(d("0"), ctx), DecTrapped);
        CHECK_THROWS_AS((void) d("-2").pow(d("0.5"), ctx), DecTrapped);
        CHECK_THROWS_AS((void) d("1E+30").exp(ctx), DecTrapped); // Overflow
        CHECK_NOTHROW((void) d("2").sqrt(ctx));                  // Inexact 默认不设陷阱
        CHECK_NOTHROW((void) d("2").ln(ctx));
    }

    TEST_CASE("陷阱把 fix 打断时，上下文的舍入方式也要还原") {
        // sqrt/exp/ln/log10 内部会临时切成 HalfEven，靠析构还原；要是写成"算完再赋值回去"，
        // 这里抛出去之后上下文就永远停在 HalfEven 了
        DecContext ctx;
        ctx.set_rounding(DecRounding::Ceiling);
        CHECK_THROWS_AS((void) d("1E+30").exp(ctx), DecTrapped);
        CHECK(ctx.rounding() == DecRounding::Ceiling);
        CHECK_THROWS_AS((void) d("-1").sqrt(ctx), DecTrapped);
        CHECK(ctx.rounding() == DecRounding::Ceiling);
    }

    TEST_CASE("精确的恒等式：完全平方、10 的整数次幂、整数次幂") {
        DecContext ctx{quiet_context()};
        for (int64_t i{0}; i < 40; ++i) {
            const BigDec n{BigDec::from_bigint(BigInt(i))};
            CAPTURE(i);
            const BigDec square{n.mul(n, ctx)};
            CHECK(square.sqrt(ctx).equals(n, ctx)); // 完全平方开方精确
            CHECK(n.pow(d("2"), ctx).equals(square, ctx));
        }
        for (int64_t k{-30}; k <= 30; ++k) {
            CAPTURE(k);
            const BigDec power{d("10").pow(BigDec::from_bigint(BigInt(k)), ctx)};
            CHECK(power.log10(ctx).equals(BigDec::from_bigint(BigInt(k)), ctx));
        }
        CHECK(ctx.flags().has(DecCondition::Inexact) == false);
    }
}
TEST_SUITE("BigDec——operator 就是配一个默认上下文调同名具名方法") {

    TEST_CASE("结果跟显式传默认上下文逐个一致") {
        // 挑不会触发默认那三个陷阱的值：默认上下文下 operator 出事是抛，不是静默给 NaN
        constexpr const char *const pool[]{
            "0", "-0", "1", "-1", "2.5", "-2.5", "0.1", "1.50", "100", "-7", "1E+10", "1E-10"
        };
        for (const char *const a_s : pool) {
            const BigDec a{d(a_s)};
            CAPTURE(a_s);
            DecContext c1;
            CHECK((+a).identical(a.plus(c1)));
            DecContext c2;
            CHECK((-a).identical(a.minus(c2)));
            for (const char *const b_s : pool) {
                const BigDec b{d(b_s)};
                CAPTURE(b_s);
                DecContext c3;
                CHECK((a + b).identical(a.add(b, c3)));
                DecContext c4;
                CHECK((a - b).identical(a.sub(b, c4)));
                DecContext c5;
                CHECK((a * b).identical(a.mul(b, c5)));
                DecContext c6;
                CHECK((a == b) == a.equals(b, c6));
                DecContext c7;
                CHECK((a <=> b) == a.compare_ordering(b, c7));
                if (b.is_zero()) continue; // 除以零在默认上下文下是抛，下面单独测
                DecContext c8;
                CHECK((a / b).identical(a.div(b, c8)));
                DecContext c9;
                CHECK((a % b).identical(a.mod(b, c9)));
            }
        }
    }

    TEST_CASE("默认上下文的三个陷阱是开着的，所以 operator 出事是抛而不是静默给 NaN") {
        CHECK_THROWS_AS((void) (d("1") / d("0")), DecTrapped);
        CHECK_THROWS_AS((void) (d("0") / d("0")), DecTrapped);
        CHECK_THROWS_AS((void) (d("1") % d("0")), DecTrapped);
        CHECK_THROWS_AS((void) (d("0") * d("Infinity")), DecTrapped);
        CHECK_THROWS_AS((void) (d("Infinity") - d("Infinity")), DecTrapped);
        CHECK_THROWS_AS((void) (d("1E+999999") * d("10")), DecTrapped); // Overflow
        CHECK_THROWS_AS((void) (d("sNaN") + d("1")), DecTrapped);
        // 序比较遇 NaN 同样触发 InvalidOperation；== 遇安静 NaN 则照常返回 false
        CHECK_THROWS_AS((void) (d("NaN") < d("1")), DecTrapped);
        CHECK_FALSE(d("NaN") == d("NaN"));
        // Inexact/Rounded 默认不设陷阱，正常算
        CHECK((d("1") / d("3")).to_string() == "0.3333333333333333333333333333");
    }
}
