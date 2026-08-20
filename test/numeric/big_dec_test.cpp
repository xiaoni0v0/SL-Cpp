// BigDec：十进制浮点数，SL 的 decimal 的底层实现。语义见 SL.md 的 decimal 一节
// （标度是值的一部分、构造不舍入运算才舍入、// 和 % 向负无穷取整）。
//
// 下半部分那些 kXxxCases 表是交叉验证用的，期望值由 gen_big_dec_cases.py 从 CPython 自带的
// decimal（C 实现，跟 BigDec 是两套独立代码）生成，落在 big_dec_cases.inc 里。
#include "../../numeric/BigDec.h"

#include "../../numeric/BigInt.h"
#include "../../numeric/DecContext.h"

#include <doctest/doctest.h>

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

#include "big_dec_cases.inc"

BigDec d(const std::string &s) {
    std::optional<BigDec> parsed{BigDec::try_from_string(s)};
    REQUIRE(parsed.has_value());
    return *std::move(parsed);
}

// 陷阱全关的上下文：交叉验证表只比"结果 + 触发了哪些信号"，不能被抛出来的陷阱打断
DecContext quiet_context(
    const int32_t prec = 28, const DecRounding rounding = DecRounding::HalfEven,
    const int32_t emax = 999999, const int32_t emin = -999999
) {
    DecContext ctx;
    ctx.set_prec(prec);
    ctx.set_rounding(rounding);
    ctx.set_emax(emax);
    ctx.set_emin(emin);
    ctx.traps().clear();
    return ctx;
}

// 八个信号，顺序同 DecCondition 的声明顺序，也就是生成器那边 SIGNAL_NAMES 的顺序
constexpr DecCondition kAllSignals[]{
    DecCondition::Clamped,
    DecCondition::DivisionByZero,
    DecCondition::Inexact,
    DecCondition::InvalidOperation,
    DecCondition::Overflow,
    DecCondition::Rounded,
    DecCondition::Subnormal,
    DecCondition::Underflow
};
static_assert(std::size(kAllSignals) == kDecSignalCount);

// 拼成 "Inexact,Rounded" 这样的串，跟用例表里的写法对得上
std::string flags_to_string(const DecSignalSet &flags) {
    std::string result;
    for (const DecCondition condition : kAllSignals) {
        if (!flags.has(condition)) continue;
        if (!result.empty()) result += ',';
        result += dec_condition_name(condition);
    }
    return result;
}

// 用例行按 '|' 拆开。末尾的空字段（"没有触发任何信号"）也要保留，所以不能用"遇到空就停"的写法
std::vector<std::string> split_fields(const std::string &line) {
    std::vector<std::string> fields{""};
    for (const char c : line) {
        if (c == '|')
            fields.emplace_back();
        else
            fields.back().push_back(c);
    }
    return fields;
}

DecRounding rounding_from_name(const std::string &name) {
    if (name == "Down") return DecRounding::Down;
    if (name == "Up") return DecRounding::Up;
    if (name == "HalfUp") return DecRounding::HalfUp;
    if (name == "HalfDown") return DecRounding::HalfDown;
    if (name == "HalfEven") return DecRounding::HalfEven;
    if (name == "Ceiling") return DecRounding::Ceiling;
    if (name == "Floor") return DecRounding::Floor;
    if (name == "ZeroFiveUp") return DecRounding::ZeroFiveUp;
    REQUIRE_MESSAGE(false, "未知的舍入方式名");
    return DecRounding::HalfEven;
}

DecCondition condition_from_name(const std::string &name) {
    if (name == "Clamped") return DecCondition::Clamped;
    if (name == "DivisionByZero") return DecCondition::DivisionByZero;
    if (name == "Inexact") return DecCondition::Inexact;
    if (name == "InvalidOperation") return DecCondition::InvalidOperation;
    if (name == "Overflow") return DecCondition::Overflow;
    if (name == "Rounded") return DecCondition::Rounded;
    if (name == "Subnormal") return DecCondition::Subnormal;
    if (name == "Underflow") return DecCondition::Underflow;
    REQUIRE_MESSAGE(false, "未知的信号名");
    return DecCondition::InvalidOperation;
}

// 覆盖各种"容易出 bug"的值，供后面的恒等式交叉验证批量使用：三类特殊值、正负零、同一个数值的
// 不同标度、指数落在 Emin/Emax 附近的、系数恰好卡在 prec 位上的、跨多个 limb 的巨大系数
std::vector<std::string> interesting_strings() {
    return {
        "0",
        "-0",
        "0.00",
        "-0E+3",
        "1",
        "-1",
        "1.0",
        "1.00",
        "1.5",
        "1.50",
        "-1.50",
        "0.1",
        "0.2",
        "0.3",
        "2.5",
        "-2.5",
        "3.5",
        "7",
        "-7",
        "3",
        "-3",
        "10",
        "1E+1", // 跟 "10" 数值相同、标度不同
        "100",
        "1E+2",
        "0.000001",
        "1E-6",
        "1E-7", // str 在这里从定点切换到科学计数法
        "12345.6789",
        "-12345.6789",
        "9999999999999999999999999999",  // 28 个 9，恰好是默认 prec
        "10000000000000000000000000000", // 29 位，一定要舍入
        "1234567890123456789012345678901234",
        "9.999999999999999999999999999E+15",
        "1E+999999",  // Emax
        "1E-999999",  // Emin
        "1E-1000026", // Etiny（prec 28 时）
        "-1E+999999",
        "Infinity",
        "-Infinity",
        "NaN",
        "-NaN",
        "sNaN",
    };
}

std::vector<BigDec> interesting_values() {
    std::vector<BigDec> result;
    for (const std::string &s : interesting_strings()) result.push_back(d(s));
    return result;
}

} // namespace

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
    }
}

TEST_SUITE("BigDec——上下文与信号机制") {

    TEST_CASE("默认上下文就是 SL.md 写的那套") {
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

    // 这条恒等式只在精确算术下必然成立，右边的乘法要按上下文舍入，跟 % 直接舍入出来的余数不保证
    // 逐位相等（例：1 % 0.30000000000000000000000000009 在 prec 28 下就不相等，CPython 的
    // Decimal 同样如此）——SL.md 3.4.2 只拿它定 % 的方向和符号，不承诺逐位相等。下面这个值池刻意
    // 温和（系数位数不多，乘积不会顶到 prec），能过是这批具体输入凑巧没撞上双重舍入，不代表恒成立
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

    TEST_CASE("除数是无穷：结果符合 floor 的定义，也保住了恒等式") {
        DecContext ctx{quiet_context()};
        CHECK(d("1").floor_div(d("Infinity"), ctx).to_string() == "0");
        CHECK(d("1").mod(d("Infinity"), ctx).to_string() == "1");
        // 一正一负：真商是个无穷小的负数，向负无穷取整就是 -1，余数随之变成 -Infinity
        CHECK(d("1").floor_div(d("-Infinity"), ctx).to_string() == "-1");
        CHECK(d("1").mod(d("-Infinity"), ctx).to_string() == "-Infinity");
        CHECK(d("-1").floor_div(d("Infinity"), ctx).to_string() == "-1");
        CHECK(d("-1").mod(d("Infinity"), ctx).to_string() == "Infinity");
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

TEST_SUITE("BigDec——跟 CPython decimal 的交叉验证") {

    TEST_CASE("加法") {
        for (const char *const raw : kAddCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 4);
            DecContext ctx{quiet_context()};
            CHECK(d(f[0]).add(d(f[1]), ctx).to_string() == f[2]);
            CHECK(flags_to_string(ctx.flags()) == f[3]);
        }
    }

    TEST_CASE("减法") {
        for (const char *const raw : kSubCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 4);
            DecContext ctx{quiet_context()};
            CHECK(d(f[0]).sub(d(f[1]), ctx).to_string() == f[2]);
            CHECK(flags_to_string(ctx.flags()) == f[3]);
        }
    }

    TEST_CASE("乘法") {
        for (const char *const raw : kMulCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 4);
            DecContext ctx{quiet_context()};
            CHECK(d(f[0]).mul(d(f[1]), ctx).to_string() == f[2]);
            CHECK(flags_to_string(ctx.flags()) == f[3]);
        }
    }

    TEST_CASE("除法") {
        for (const char *const raw : kDivCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 4);
            DecContext ctx{quiet_context()};
            CHECK(d(f[0]).div(d(f[1]), ctx).to_string() == f[2]);
            CHECK(flags_to_string(ctx.flags()) == f[3]);
        }
    }

    TEST_CASE("//（期望值是按向负无穷重新推的，不是 Python 的 //）") {
        for (const char *const raw : kFloorDivCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 4);
            DecContext ctx{quiet_context()};
            CHECK(d(f[0]).floor_div(d(f[1]), ctx).to_string() == f[2]);
            CHECK(flags_to_string(ctx.flags()) == f[3]);
        }
    }

    TEST_CASE("%（同上）") {
        for (const char *const raw : kModCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 4);
            DecContext ctx{quiet_context()};
            CHECK(d(f[0]).mod(d(f[1]), ctx).to_string() == f[2]);
            CHECK(flags_to_string(ctx.flags()) == f[3]);
        }
    }

    TEST_CASE("divmod（期望值取 // 和 % 各自的结果，flags 取并集）") {
        for (const char *const raw : kDivmodCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 5);
            DecContext ctx{quiet_context()};
            const auto [quotient, remainder]{d(f[0]).divmod(d(f[1]), ctx)};
            CHECK(quotient.to_string() == f[2]);
            CHECK(remainder.to_string() == f[3]);
            CHECK(flags_to_string(ctx.flags()) == f[4]);
        }
    }

    TEST_CASE("八种舍入方式 × prec 1/2/3/7") {
        for (const char *const raw : kRoundCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 5);
            DecContext ctx{quiet_context(std::stoi(f[1]), rounding_from_name(f[2]))};
            CHECK(d(f[0]).plus(ctx).to_string() == f[3]);
            CHECK(flags_to_string(ctx.flags()) == f[4]);
        }
    }

    TEST_CASE("指数边界：Overflow / Underflow / Subnormal / Clamped") {
        for (const char *const raw : kEdgeCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 9);
            DecContext ctx{quiet_context(
                std::stoi(f[3]), rounding_from_name(f[4]), std::stoi(f[5]), std::stoi(f[6])
            )};
            const BigDec a{d(f[0])};
            const BigDec b{d(f[1])};
            const std::string &op{f[2]};
            const BigDec result{
                op == "mul"        ? a.mul(b, ctx)
                : op == "div"      ? a.div(b, ctx)
                : op == "add"      ? a.add(b, ctx)
                : op == "sub"      ? a.sub(b, ctx)
                : op == "floordiv" ? a.floor_div(b, ctx)
                                   : a.mod(b, ctx)
            };
            REQUIRE_MESSAGE(
                (op == "mul" || op == "div" || op == "add" || op == "sub" || op == "floordiv" ||
                 op == "mod"),
                "用例表里出现了没实现的 op"
            );
            CHECK(result.to_string() == f[7]);
            CHECK(flags_to_string(ctx.flags()) == f[8]);
        }
    }

    TEST_CASE("随机操作数 × 随机 op/prec/rounding") {
        for (const char *const raw : kMixedCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 7);
            DecContext ctx{quiet_context(std::stoi(f[3]), rounding_from_name(f[4]))};
            const BigDec a{d(f[0])};
            const BigDec b{d(f[1])};
            const std::string &op{f[2]};
            const BigDec result{
                op == "add"        ? a.add(b, ctx)
                : op == "sub"      ? a.sub(b, ctx)
                : op == "mul"      ? a.mul(b, ctx)
                : op == "div"      ? a.div(b, ctx)
                : op == "floordiv" ? a.floor_div(b, ctx)
                                   : a.mod(b, ctx)
            };
            REQUIRE_MESSAGE(
                (op == "add" || op == "sub" || op == "mul" || op == "div" || op == "floordiv" ||
                 op == "mod"),
                "用例表里出现了没实现的 op"
            );
            CHECK(result.to_string() == f[5]);
            CHECK(flags_to_string(ctx.flags()) == f[6]);
        }
    }

    TEST_CASE(
        "陷阱开启：抛不抛、抛哪个条件、抛之前 flags 走到哪一步（上面所有表都只有陷阱全关"
        "的路径，这张表专补陷阱开着时的行为，值池里塞了带非零指数的零和极端指数）"
    ) {
        // 行格式：a|b|op|prec|rounding|emax|emin|trap|结果|flags——真抛了的话结果为空、
        // flags 是 "THROW:条件名;抛出时已记下的flags"
        const auto run_trapped{
            [](const BigDec &a, const BigDec &b, const std::string &op, DecContext &ctx) {
                try {
                    const BigDec r{
                        op == "add"        ? a.add(b, ctx)
                        : op == "sub"      ? a.sub(b, ctx)
                        : op == "mul"      ? a.mul(b, ctx)
                        : op == "div"      ? a.div(b, ctx)
                        : op == "floordiv" ? a.floor_div(b, ctx)
                        : op == "mod"      ? a.mod(b, ctx)
                        : op == "pow"      ? a.pow(b, ctx)
                        : op == "sqrt"     ? a.sqrt(ctx)
                        : op == "exp"      ? a.exp(ctx)
                        : op == "ln"       ? a.ln(ctx)
                                           : a.log10(ctx)
                    };
                    return std::pair{r.to_string(), flags_to_string(ctx.flags())};
                } catch (const DecTrapped &e) {
                    return std::pair{
                        std::string{},
                        "THROW:" + std::string{dec_condition_name(e.condition())} + ";" +
                            flags_to_string(ctx.flags())
                    };
                }
            }
        };

        const auto check_trap_table{[&run_trapped](const auto &table, const bool unary) {
            for (const char *const raw : table) {
                const std::string line{raw};
                CAPTURE(line);
                const std::vector<std::string> f{split_fields(line)};
                REQUIRE(f.size() == 10);
                const DecRounding rounding{rounding_from_name(f[4])};
                DecContext ctx{
                    quiet_context(std::stoi(f[3]), rounding, std::stoi(f[5]), std::stoi(f[6]))
                };
                ctx.traps().add(condition_from_name(f[7]));
                const BigDec a{d(f[0])};
                const BigDec b{unary && f[1].empty() ? a : d(f[1])};
                const auto [res, fl]{run_trapped(a, b, f[2], ctx)};
                CHECK(res == f[8]);
                CHECK(fl == f[9]);
                // 抛出来的 DecTrapped 在 BigDec 一侧就该是细分条件（0/0 是 DivisionUndefined，
                // 不是折算后的 InvalidOperation）——生成器那边按这个约定出的题
            }
        }};

        check_trap_table(kTrappedArith, false);
        check_trap_table(kTrappedFloorDivMod, false);
        check_trap_table(kTrappedTrans, true);
    }

    TEST_CASE("陷阱开启：== 与序比较抛不抛、抛什么") {
        // 行格式：a|b|trap|eq;eqf|rel;orf，';' 前是结果、后是 flags（抛了就是
        // "THROW:条件名;flags"）
        for (const char *const raw : kTrappedCmp) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 5);
            const BigDec a{d(f[0])};
            const BigDec b{d(f[1])};
            DecContext ctx{quiet_context()};
            ctx.traps().add(condition_from_name(f[2]));
            try {
                const bool eq{a.equals(b, ctx)};
                const std::string got{
                    (eq ? "1" : "0") + std::string{";"} + flags_to_string(ctx.flags())
                };
                CHECK(got == f[3]);
            } catch (const DecTrapped &e) {
                CHECK(
                    "THROW:" + std::string{dec_condition_name(e.condition())} + ";" +
                        flags_to_string(ctx.flags()) ==
                    f[3]
                );
            }
            try {
                const std::partial_ordering ordering{a.compare_ordering(b, ctx)};
                const std::string rel{
                    ordering == std::partial_ordering::unordered ? "un"
                    : ordering == std::partial_ordering::less    ? "lt"
                    : ordering == std::partial_ordering::greater ? "gt"
                                                                 : "eq"
                };
                CHECK(rel + ";" + flags_to_string(ctx.flags()) == f[4]);
            } catch (const DecTrapped &e) {
                CHECK(
                    "THROW:" + std::string{dec_condition_name(e.condition())} + ";" +
                        flags_to_string(ctx.flags()) ==
                    f[4]
                );
            }
        }
    }

    TEST_CASE("幂运算 **") {
        for (const char *const raw : kPowCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 8);
            DecContext ctx{quiet_context(
                std::stoi(f[2]), rounding_from_name(f[3]), std::stoi(f[4]), std::stoi(f[5])
            )};
            CHECK(d(f[0]).pow(d(f[1]), ctx).to_string() == f[6]);
            CHECK(flags_to_string(ctx.flags()) == f[7]);
        }
    }

    TEST_CASE("超越函数 sqrt/exp/ln/log10") {
        for (const char *const raw : kTranscendentalCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 8);
            const DecRounding rounding{rounding_from_name(f[3])};
            DecContext ctx{
                quiet_context(std::stoi(f[2]), rounding, std::stoi(f[4]), std::stoi(f[5]))
            };
            const BigDec a{d(f[0])};
            const std::string &op{f[1]};
            const BigDec result{
                op == "sqrt"  ? a.sqrt(ctx)
                : op == "exp" ? a.exp(ctx)
                : op == "ln"  ? a.ln(ctx)
                              : a.log10(ctx)
            };
            CHECK(result.to_string() == f[6]);
            CHECK(flags_to_string(ctx.flags()) == f[7]);
            // 这四个内部会临时把舍入方式换成 HalfEven，算完必须还回去
            CHECK(ctx.rounding() == rounding);
        }
    }

    TEST_CASE("比较") {
        for (const char *const raw : kCompareCases) {
            const std::string line{raw};
            CAPTURE(line);
            const std::vector<std::string> f{split_fields(line)};
            REQUIRE(f.size() == 6);
            const BigDec a{d(f[0])};
            const BigDec b{d(f[1])};

            DecContext eq_ctx{quiet_context()};
            CHECK(a.equals(b, eq_ctx) == (f[3] == "1"));
            CHECK(flags_to_string(eq_ctx.flags()) == f[4]);

            DecContext ord_ctx{quiet_context()};
            const std::partial_ordering ordering{a.compare_ordering(b, ord_ctx)};
            const std::string relation{
                ordering == std::partial_ordering::unordered ? "un"
                : ordering == std::partial_ordering::less    ? "lt"
                : ordering == std::partial_ordering::greater ? "gt"
                                                             : "eq"
            };
            CHECK(relation == f[2]);
            CHECK(flags_to_string(ord_ctx.flags()) == f[5]);
        }
    }
}
