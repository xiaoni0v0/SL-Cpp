// 构造与字符串往返；上下文、信号、陷阱。
#include "test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("BigDec——构造与字符串往返") {

    TEST_CASE("零：标度和符号都留着") {
        CHECK(d("0").to_string() == "0");
        CHECK(d("-0").to_string() == "-0");
        CHECK(d("0.00").to_string() == "0.00");
        CHECK(d("-0.00").to_string() == "-0.00");
        CHECK(d("0E+3").to_string() == "0E+3");
        CHECK(d("0").is_zero());
        CHECK(d("-0").is_zero());
        CHECK(d("-0").is_negative());
        CHECK_FALSE(d("0").is_negative());
    }

    TEST_CASE("末尾零保留：1.5 和 1.50 是同一个值的两种表示") {
        CHECK(d("1.5").to_string() == "1.5");
        CHECK(d("1.50").to_string() == "1.50");
        CHECK(d("1.500").to_string() == "1.500");
        CHECK(d("1.5").exponent() == -1);
        CHECK(d("1.50").exponent() == -2);
        CHECK(d("1.5").coefficient() == BigInt(15));
        CHECK(d("1.50").coefficient() == BigInt(150));
        // 表示不同，但数值相等
        CHECK_FALSE(d("1.5").identical(d("1.50")));
        DecContext ctx;
        CHECK(d("1.5").equals(d("1.50"), ctx));
    }

    TEST_CASE("前导零不影响数值，也不影响标度") {
        CHECK(d("007").to_string() == "7");
        CHECK(d("00.50").to_string() == "0.50");
        CHECK(d("000").to_string() == "0");
        CHECK(d("-000").to_string() == "-0");
    }

    TEST_CASE("科学计数法的输入") {
        CHECK(d("1E+3").to_string() == "1E+3");
        CHECK(d("1e3").to_string() == "1E+3");
        CHECK(d("1E3").to_string() == "1E+3");
        CHECK(d("1.5E+3").to_string() == "1.5E+3");
        CHECK(d("1.5E-3").to_string() == "0.0015");
        CHECK(d("15E-4").to_string() == "0.0015");
        CHECK(d("1E-006").to_string() == "0.000001"); // 指数里的前导零不算错
        CHECK(d("+1.5E+3").to_string() == "1.5E+3");
    }

    TEST_CASE("str 什么时候切换到科学计数法：指数为正、或者调整后的指数小于 -6") {
        CHECK(d("1000000").to_string() == "1000000"); // 指数 0，照常定点
        CHECK(d("1E+6").to_string() == "1E+6");       // 指数为正，切
        CHECK(d("0.000001").to_string() == "0.000001");
        CHECK(d("0.0000001").to_string() == "1E-7"); // 调整后的指数 -7，切
        CHECK(d("0.00000012").to_string() == "1.2E-7");
        CHECK(d("1.2E-7").to_string() == "1.2E-7");
        CHECK(d("0.0000012").to_string() == "0.0000012"); // 调整后的指数 -6，不切
        CHECK(d("123E+2").to_string() == "1.23E+4");
        CHECK(d("0E+7").to_string() == "0E+7");
        CHECK(d("0E-7").to_string() == "0E-7"); // 零的调整后指数就是它自己的指数
    }

    TEST_CASE("特殊值") {
        CHECK(d("Infinity").to_string() == "Infinity");
        CHECK(d("inf").to_string() == "Infinity");
        CHECK(d("-INF").to_string() == "-Infinity");
        CHECK(d("iNfInItY").to_string() == "Infinity");
        CHECK(d("NaN").to_string() == "NaN");
        CHECK(d("-nan").to_string() == "-NaN");
        CHECK(d("sNaN").to_string() == "sNaN");
        CHECK(d("-SNAN").to_string() == "-sNaN");

        CHECK(d("Infinity").is_infinite());
        CHECK(d("NaN").is_nan());
        CHECK_FALSE(d("NaN").is_signaling_nan());
        CHECK(d("sNaN").is_nan());
        CHECK(d("sNaN").is_signaling_nan());
        CHECK_FALSE(d("Infinity").is_finite());
        CHECK_FALSE(d("NaN").is_zero());
    }

    TEST_CASE("kind() 直接反映构造出来的类别") {
        CHECK(d("1.5").kind() == BigDec::Kind::Finite);
        CHECK(d("0").kind() == BigDec::Kind::Finite);
        CHECK(d("Infinity").kind() == BigDec::Kind::Infinity);
        CHECK(d("-Infinity").kind() == BigDec::Kind::Infinity);
        CHECK(d("NaN").kind() == BigDec::Kind::NaN);
        CHECK(d("sNaN").kind() == BigDec::Kind::SignalingNaN);
    }

    TEST_CASE("from_string(s, ctx)：合法串直接构造，不查上下文、不触发信号") {
        DecContext ctx;
        CHECK(BigDec::from_string("1.50", ctx).identical(d("1.50")));
        CHECK(BigDec::from_string("Infinity", ctx).identical(d("Infinity")));
        CHECK(ctx.flags().empty());
    }

    TEST_CASE("from_string(s, ctx)：不合法串在陷阱关着时返回安静 NaN、报 ConversionSyntax") {
        DecContext ctx{quiet_context()};
        const BigDec result{BigDec::from_string("abc", ctx)};
        CHECK(result.is_nan());
        CHECK_FALSE(result.is_signaling_nan());
        CHECK(ctx.flags().has(DecCondition::InvalidOperation));
    }

    TEST_CASE("非法字符串返回 nullopt") {
        constexpr const char *const bad[]{
            "",      "-",     "+",     ".",      "-.",    "e5",     "E5",      ".e5",
            "1.2.3", "1e",    "1e+",   "1e+ ",   "--1",   "1-",     "0x10",    "1 ",
            " 1",    "1_000", "abc",   "Inf5",   "NaN1",  "sNaN12", "Infinit", "Infinityy",
            "1,5",   "１",    "1.5d3", "nan(1)", "1E1.5",
        };
        for (const char *const s : bad) {
            CAPTURE(s);
            CHECK_FALSE(BigDec::try_from_string(s).has_value());
        }
    }

    TEST_CASE("Python 收但我们不收的三类写法") {
        // 首尾空白、数字里的下划线：BigInt::from_decimal_string 也不收，保持一致
        CHECK_FALSE(BigDec::try_from_string(" 1.5").has_value());
        CHECK_FALSE(BigDec::try_from_string("1.5 ").has_value());
        CHECK_FALSE(BigDec::try_from_string("1_0").has_value());
        // NaN 的诊断信息：SL 的 decimal 没这个概念，静默丢掉比拒绝更糟
        CHECK_FALSE(BigDec::try_from_string("NaN123").has_value());
        CHECK_FALSE(BigDec::try_from_string("sNaN9").has_value());
    }

    TEST_CASE("小数点在两头都合法（跟 IBM 的语法一致）") {
        CHECK(d("1.").to_string() == "1");
        CHECK(d(".5").to_string() == "0.5");
        CHECK(d("-.5").to_string() == "-0.5");
        CHECK(d("1.E+2").to_string() == "1E+2");
        CHECK(d(".5E+2").to_string() == "5E+1"); // 系数 5、指数 1，指数为正就转科学计数法
    }

    TEST_CASE("指数超出允许范围算不合法") {
        CHECK(BigDec::kMaxExponent == 1999999998); // Emax 的上限 + prec 的上限
        CHECK(BigDec::try_from_string("1E+1999999998").has_value());
        CHECK_FALSE(BigDec::try_from_string("1E+1999999999").has_value());
        CHECK_FALSE(BigDec::try_from_string("1E-1999999999").has_value());
        CHECK_FALSE(BigDec::try_from_string("1E+99999999999999999999999").has_value());
        // 小数部分也会把指数往下拽
        CHECK_FALSE(BigDec::try_from_string("0.5E-1999999998").has_value());
        // 指数里的前导零不该把它算成"太长"
        CHECK(BigDec::try_from_string("1E+0000000000000000000000009").has_value());
    }

    TEST_CASE("再极端的上下文，fix 出来的结果也得读得回去") {
        // kMaxExponent 卡在 Emax 的量级上时，次正规结果的指数（会被压到 Etiny = Emin - prec + 1）
        // 能低过它，于是运算能产出 try_from_string 读不回来的值，破坏 to_string 的往返
        DecContext ctx{quiet_context()};
        ctx.set_emin(-DecContext::kMaxExp);
        ctx.set_emax(DecContext::kMaxExp);
        ctx.set_prec(DecContext::kMaxPrec);
        CHECK(ctx.etiny() == -1999999997); // 最低的 Etiny，仍在 kMaxExponent 之内

        DecContext small{quiet_context()};
        small.set_emin(-999999999);
        const BigDec tiny{d("1E-999999999").mul(d("1E-27"), small)};
        CHECK(tiny.to_string() == "1E-1000000026");
        CHECK(small.flags().has(DecCondition::Subnormal));
        const std::optional<BigDec> parsed{BigDec::try_from_string(tiny.to_string())};
        REQUIRE(parsed.has_value());
        CHECK(parsed->identical(tiny));
    }

    TEST_CASE("to_string 往返：完整保留表示") {
        for (const BigDec &value : interesting_values()) {
            const std::string text{value.to_string()};
            CAPTURE(text);
            const std::optional<BigDec> parsed{BigDec::try_from_string(text)};
            REQUIRE(parsed.has_value());
            CHECK(parsed->identical(value));
            CHECK(parsed->to_string() == text);
        }
    }

    TEST_CASE("from_bigint 精确、指数为 0") {
        CHECK(BigDec::from_bigint(BigInt(0)).to_string() == "0");
        CHECK(BigDec::from_bigint(BigInt(-1)).to_string() == "-1");
        CHECK(
            BigDec::from_bigint(BigInt::from_decimal_string("123456789012345678901234567890"))
                .to_string() == "123456789012345678901234567890"
        );
        // BigInt 没有负零，转过来一定是正零
        CHECK_FALSE(BigDec::from_bigint(BigInt(0)).is_negative());
    }

    TEST_CASE("from_parts 挡住非法参数") {
        CHECK(BigDec::from_parts(true, BigInt(15), -1).to_string() == "-1.5");
        CHECK(BigDec::from_parts(false, BigInt(0), 3).to_string() == "0E+3");
        CHECK_THROWS_AS((void) BigDec::from_parts(false, BigInt(-1), 0), std::invalid_argument);
        CHECK_THROWS_AS(
            (void) BigDec::from_parts(false, BigInt(1), BigDec::kMaxExponent + 1),
            std::invalid_argument
        );
        CHECK_THROWS_AS(
            (void) BigDec::from_parts(false, BigInt(1), -BigDec::kMaxExponent - 1),
            std::invalid_argument
        );
    }

    TEST_CASE("digit_count / adjusted / exponent") {
        CHECK(d("0").digit_count() == 1); // 零也算 1 位
        CHECK(d("0.000").digit_count() == 1);
        CHECK(d("1.50").digit_count() == 3);
        CHECK(d("999").digit_count() == 3);
        CHECK(d("1E+100").digit_count() == 1);

        CHECK(d("1").adjusted() == 0);
        CHECK(d("9.99").adjusted() == 0);
        CHECK(d("1E+100").adjusted() == 100);
        CHECK(d("123").adjusted() == 2);
        CHECK(d("0.001").adjusted() == -3);
        CHECK(d("0").adjusted() == 0);
        CHECK(d("0E+5").adjusted() == 5);
    }

    TEST_CASE("identical 比的是表示，不是数值") {
        CHECK(d("1.5").identical(d("1.5")));
        CHECK_FALSE(d("1.5").identical(d("1.50")));
        CHECK_FALSE(d("0").identical(d("-0")));
        CHECK_FALSE(d("0").identical(d("0.0")));
        CHECK(d("NaN").identical(d("NaN"))); // 跟 SL 的 == 正好相反
        CHECK_FALSE(d("NaN").identical(d("-NaN")));
        CHECK_FALSE(d("NaN").identical(d("sNaN")));
        CHECK(d("Infinity").identical(d("Infinity")));
    }

    TEST_CASE("copy_negate / copy_abs 不舍入、不动别的") {
        CHECK(d("1.500").copy_negate().to_string() == "-1.500");
        CHECK(d("-1.500").copy_negate().to_string() == "1.500");
        CHECK(d("0").copy_negate().to_string() == "-0");
        CHECK(d("-0").copy_abs().to_string() == "0");
        CHECK(d("NaN").copy_negate().to_string() == "-NaN");
        CHECK(d("-Infinity").copy_abs().to_string() == "Infinity");
        // 位数远超 prec 也原样留着
        CHECK(
            d("1234567890123456789012345678901234").copy_negate().to_string() ==
            "-1234567890123456789012345678901234"
        );
        // copy-* 是安静操作：sNaN 只翻符号，不报 InvalidOperation
        DecContext ctx{quiet_context()};
        CHECK(d("sNaN").copy_negate().to_string() == "-sNaN");
        CHECK(d("-sNaN").copy_abs().to_string() == "sNaN");
        CHECK(ctx.flags().empty());
    }

    TEST_CASE("显式正号的特殊值") {
        CHECK(d("+Infinity").to_string() == "Infinity");
        CHECK(d("+NaN").to_string() == "NaN");
        CHECK(d("+sNaN").to_string() == "sNaN");
        CHECK(d("+Inf").to_string() == "Infinity");
    }

    TEST_CASE("is_integral：有限且没有非零小数部分") {
        CHECK(d("0").is_integral());
        CHECK(d("-0").is_integral());
        CHECK(d("0.00").is_integral()); // 数值是 0
        CHECK(d("0E-5").is_integral());
        CHECK(d("0E+5").is_integral());
        CHECK(d("1").is_integral());
        CHECK(d("-7").is_integral());
        CHECK(d("1.0").is_integral());
        CHECK(d("1.00").is_integral());
        CHECK(d("10.0").is_integral());
        CHECK(d("3.00").is_integral()); // 看起来像小数，数值是整数
        CHECK(d("2.0").is_integral());
        CHECK(d("1E+10").is_integral());
        CHECK(d("2.5E+1").is_integral()); // 25
        CHECK(d("100E-2").is_integral()); // 1.00
        CHECK_FALSE(d("1.5").is_integral());
        CHECK_FALSE(d("0.1").is_integral());
        CHECK_FALSE(d("0.5").is_integral());
        CHECK_FALSE(d("2.5E-1").is_integral()); // 0.25
        CHECK_FALSE(d("1.10").is_integral());
        CHECK_FALSE(d("Infinity").is_integral());
        CHECK_FALSE(d("-Infinity").is_integral());
        CHECK_FALSE(d("NaN").is_integral());
        CHECK_FALSE(d("sNaN").is_integral());
    }
}
TEST_SUITE("BigDec——上下文与信号机制") {

    TEST_CASE("默认上下文：prec 28、HalfEven、Emax/Emin ±999999") {
        const DecContext ctx;
        CHECK(ctx.prec() == 28);
        CHECK(ctx.rounding() == DecRounding::HalfEven);
        CHECK(ctx.emax() == 999999);
        CHECK(ctx.emin() == -999999);
        CHECK(ctx.flags().empty());
        CHECK(ctx.traps().has(DecCondition::DivisionByZero));
        CHECK(ctx.traps().has(DecCondition::Overflow));
        CHECK(ctx.traps().has(DecCondition::InvalidOperation));
        CHECK_FALSE(ctx.traps().has(DecCondition::Inexact));
        CHECK_FALSE(ctx.traps().has(DecCondition::Rounded));
        CHECK_FALSE(ctx.traps().has(DecCondition::Clamped));
        CHECK_FALSE(ctx.traps().has(DecCondition::Subnormal));
        CHECK_FALSE(ctx.traps().has(DecCondition::Underflow));
        CHECK(ctx.etiny() == -999999 - 28 + 1);
        CHECK(ctx.etop() == 999999 - 28 + 1);
    }

    TEST_CASE("InvalidOperation 的细分条件查的是 InvalidOperation 那一位") {
        CHECK(signal_of(DecCondition::ConversionSyntax) == DecCondition::InvalidOperation);
        CHECK(signal_of(DecCondition::DivisionImpossible) == DecCondition::InvalidOperation);
        CHECK(signal_of(DecCondition::DivisionUndefined) == DecCondition::InvalidOperation);
        CHECK(signal_of(DecCondition::InvalidContext) == DecCondition::InvalidOperation);
        CHECK(signal_of(DecCondition::Overflow) == DecCondition::Overflow);
        CHECK(signal_of(DecCondition::Clamped) == DecCondition::Clamped);

        DecSignalSet set;
        set.add(DecCondition::ConversionSyntax);
        CHECK(set.has(DecCondition::InvalidOperation));
        CHECK(set.has(DecCondition::DivisionImpossible)); // 同一位
        CHECK_FALSE(set.has(DecCondition::Overflow));
        set.remove(DecCondition::DivisionUndefined);
        CHECK(set.empty());
    }

    TEST_CASE("信号集合的基本操作") {
        DecSignalSet set{DecCondition::Inexact, DecCondition::Rounded};
        CHECK(set.has(DecCondition::Inexact));
        CHECK(set.has(DecCondition::Rounded));
        CHECK_FALSE(set.has(DecCondition::Clamped));
        CHECK_FALSE(set.empty());
        set.remove(DecCondition::Inexact);
        CHECK_FALSE(set.has(DecCondition::Inexact));
        set.clear();
        CHECK(set.empty());
        CHECK(set == DecSignalSet{});
        CHECK(DecSignalSet{DecCondition::Overflow} == DecSignalSet{DecCondition::Overflow});
        CHECK_FALSE(DecSignalSet{DecCondition::Overflow} == DecSignalSet{DecCondition::Inexact});
    }

    TEST_CASE("raise：flags 恒置位，陷阱开着才抛") {
        DecContext ctx{quiet_context()};
        ctx.raise(DecCondition::Inexact);
        CHECK(ctx.flags().has(DecCondition::Inexact));
        CHECK_FALSE(ctx.flags().has(DecCondition::Rounded));

        ctx.traps().add(DecCondition::Rounded);
        CHECK_THROWS_AS(ctx.raise(DecCondition::Rounded), DecTrapped);
        // 抛出去之前 flags 就已经置上了
        CHECK(ctx.flags().has(DecCondition::Rounded));
    }

    TEST_CASE("抛出来的 DecTrapped 带的是细分条件，不是折算后的信号") {
        DecContext ctx;
        try {
            ctx.raise(DecCondition::ConversionSyntax);
            FAIL("该抛没抛");
        } catch (const DecTrapped &e) {
            CHECK(e.condition() == DecCondition::ConversionSyntax);
            CHECK(std::string{e.what()} == "ConversionSyntax");
        }
        CHECK(ctx.flags().has(DecCondition::InvalidOperation)); // 记的是折算后的
    }

    TEST_CASE("flags 是粘滞的，只能手动清") {
        DecContext ctx{quiet_context()};
        (void) d("1").div(d("3"), ctx);
        CHECK(ctx.flags().has(DecCondition::Inexact));
        (void) d("1").add(d("1"), ctx); // 这次是精确的
        CHECK(ctx.flags().has(DecCondition::Inexact));
        ctx.flags().clear();
        CHECK(ctx.flags().empty());
        (void) d("1").add(d("1"), ctx);
        CHECK(ctx.flags().empty());
    }

    TEST_CASE("上下文字段越界抛 std::invalid_argument") {
        DecContext ctx;
        CHECK_THROWS_AS(ctx.set_prec(0), std::invalid_argument);
        CHECK_THROWS_AS(ctx.set_prec(-1), std::invalid_argument);
        CHECK_THROWS_AS(ctx.set_prec(DecContext::kMaxPrec + 1), std::invalid_argument);
        CHECK_THROWS_AS(ctx.set_emax(-1), std::invalid_argument);
        CHECK_THROWS_AS(ctx.set_emax(DecContext::kMaxExp + 1), std::invalid_argument);
        CHECK_THROWS_AS(ctx.set_emin(1), std::invalid_argument);
        CHECK_THROWS_AS(ctx.set_emin(-DecContext::kMaxExp - 1), std::invalid_argument);
        // 挡下来之后字段没被改坏
        CHECK(ctx.prec() == 28);
        CHECK(ctx.emax() == 999999);
        CHECK(ctx.emin() == -999999);
        ctx.set_prec(1);
        ctx.set_emax(0);
        ctx.set_emin(0);
        CHECK(ctx.prec() == 1);
        CHECK(ctx.etiny() == 0);
        CHECK(ctx.etop() == 0);
    }

    TEST_CASE("默认上下文下这些运算是抛异常的") {
        auto trapped = [](auto &&op) {
            try {
                DecContext ctx;
                op(ctx);
            } catch (const DecTrapped &e) {
                return e.condition();
            }
            FAIL("该抛没抛");
            return DecCondition::Clamped;
        };

        CHECK(trapped([](DecContext &c) {
                  (void) d("1").div(d("0"), c);
              }) == DecCondition::DivisionByZero);
        CHECK(trapped([](DecContext &c) {
                  (void) d("0").div(d("0"), c);
              }) == DecCondition::DivisionUndefined);
        CHECK(trapped([](DecContext &c) {
                  (void) BigDec::from_string("abc", c);
              }) == DecCondition::ConversionSyntax);
        CHECK(trapped([](DecContext &c) {
                  (void) d("Infinity").sub(d("Infinity"), c);
              }) == DecCondition::InvalidOperation);
        CHECK(trapped([](DecContext &c) {
                  (void) d("0").mul(d("Infinity"), c);
              }) == DecCondition::InvalidOperation);
        CHECK(trapped([](DecContext &c) {
                  (void) d("1E+999999").mul(d("10"), c);
              }) == DecCondition::Overflow);
        CHECK(trapped([](DecContext &c) {
                  (void) d("1").floor_div(d("0"), c);
              }) == DecCondition::DivisionByZero);
        CHECK(trapped([](DecContext &c) {
                  (void) d("1").mod(d("0"), c);
              }) == DecCondition::InvalidOperation);
        CHECK(trapped([](DecContext &c) {
                  (void) d("1E+30").floor_div(d("1E-30"), c);
              }) == DecCondition::DivisionImpossible);
        // sNaN 碰上任何运算都报
        CHECK(trapped([](DecContext &c) {
                  (void) d("sNaN").add(d("1"), c);
              }) == DecCondition::InvalidOperation);
        CHECK(trapped([](DecContext &c) {
                  (void) d("sNaN").plus(c);
              }) == DecCondition::InvalidOperation);
    }

    TEST_CASE("默认上下文下这些是不抛的") {
        DecContext ctx;
        CHECK_NOTHROW((void) d("1").div(d("3"), ctx));             // Inexact/Rounded 默认不设陷阱
        CHECK_NOTHROW((void) d("1E-999999").div(d("1E+10"), ctx)); // Subnormal/Underflow 同理
        CHECK_NOTHROW((void) d("NaN").add(d("1"), ctx));           // 安静 NaN 只是传播
        CHECK(d("NaN").add(d("1"), ctx).to_string() == "NaN");
        CHECK_NOTHROW((void) d("NaN").equals(d("1"), ctx)); // == 遇安静 NaN 不报信号
        CHECK_FALSE(d("NaN").equals(d("NaN"), ctx));
        CHECK(ctx.flags().has(DecCondition::Inexact));
        CHECK_FALSE(ctx.flags().has(DecCondition::InvalidOperation));
    }

    TEST_CASE("序比较遇到安静 NaN 也要报 InvalidOperation") {
        DecContext ctx;
        CHECK_THROWS_AS((void) d("NaN").compare_ordering(d("1"), ctx), DecTrapped);
        DecContext quiet{quiet_context()};
        CHECK(d("NaN").compare_ordering(d("1"), quiet) == std::partial_ordering::unordered);
        CHECK(quiet.flags().has(DecCondition::InvalidOperation));
    }

    TEST_CASE("fix() 里同时开好几个陷阱时，抛出来的必须是规范顺序里最先报的那个") {
        // fix() 内部按固定顺序 raise：溢出走 Overflow -> Inexact -> Rounded；
        // 舍入进次正规区走 Underflow -> Subnormal -> Inexact -> Rounded -> (下溢到 0 再 Clamped)。
        // 这里对着同一个触发路径，每次只放开"排在更前面的都不开"的那个信号，验证真正抛出来的
        // 条件确实是它，而不是随便哪个凑巧先被判断到的信号
        const auto first_thrown{[](DecContext ctx, auto &&op) {
            try {
                op(ctx);
            } catch (const DecTrapped &e) {
                return e.condition();
            }
            FAIL("该抛没抛");
            return DecCondition::Clamped;
        }};

        // 溢出路径：prec 3、Emax 4 下，1E+999 乘 10 必然溢出
        {
            DecContext ctx{quiet_context(3, DecRounding::HalfEven, 4, -4)};
            ctx.traps() =
                DecSignalSet{DecCondition::Overflow, DecCondition::Inexact, DecCondition::Rounded};
            CHECK(first_thrown(ctx, [](DecContext &c) {
                      (void) d("1E+999").mul(d("10"), c);
                  }) == DecCondition::Overflow);
        }

        // 舍入进次正规区、且发生了真实截断（decision != 0）：Underflow/Subnormal/Inexact/Rounded
        // 都会被 raise。prec 3、Emax 4、Emin -4（Etiny -6）下，1.23456E-5 落进这条路径
        const auto subnormal_rounded{[](DecContext &c) { (void) d("1.23456E-5").plus(c); }};
        {
            DecContext ctx{quiet_context(3, DecRounding::HalfEven, 4, -4)};
            ctx.traps() = DecSignalSet{
                DecCondition::Underflow,
                DecCondition::Subnormal,
                DecCondition::Inexact,
                DecCondition::Rounded
            };
            CHECK(first_thrown(ctx, subnormal_rounded) == DecCondition::Underflow);
        }
        {
            DecContext ctx{quiet_context(3, DecRounding::HalfEven, 4, -4)};
            ctx.traps() =
                DecSignalSet{DecCondition::Subnormal, DecCondition::Inexact, DecCondition::Rounded};
            CHECK(first_thrown(ctx, subnormal_rounded) == DecCondition::Subnormal);
        }
        {
            DecContext ctx{quiet_context(3, DecRounding::HalfEven, 4, -4)};
            ctx.traps() = DecSignalSet{DecCondition::Inexact, DecCondition::Rounded};
            CHECK(first_thrown(ctx, subnormal_rounded) == DecCondition::Inexact);
        }
        {
            DecContext ctx{quiet_context(3, DecRounding::HalfEven, 4, -4)};
            ctx.traps() = DecSignalSet{DecCondition::Rounded};
            CHECK(first_thrown(ctx, subnormal_rounded) == DecCondition::Rounded);
        }

        // 下溢到 0：同一条路径再往极端走一步（1E-100 在这个上下文里舍不出任何有效数字）。
        // 先在陷阱全关的上下文里确认真的下溢到 0、且报了 Clamped；再只放开 Clamped，验证它
        // 确实排在最后也确实会抛
        {
            DecContext quiet{quiet_context(3, DecRounding::HalfEven, 4, -4)};
            const BigDec result{d("1E-100").plus(quiet)};
            CHECK(result.is_zero());
            CHECK(quiet.flags().has(DecCondition::Clamped));
        }
        {
            DecContext ctx{quiet_context(3, DecRounding::HalfEven, 4, -4)};
            ctx.traps() = DecSignalSet{DecCondition::Clamped};
            CHECK(first_thrown(ctx, [](DecContext &c) {
                      (void) d("1E-100").plus(c);
                  }) == DecCondition::Clamped);
        }
    }
}
