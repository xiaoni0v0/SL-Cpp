#pragma once

// BigDec 测试共用：构造、陷阱全关的上下文、信号名、边界值池。

#include "../../../numeric/BigDec.h"
#include "../../../numeric/BigInt.h"
#include "../../../numeric/DecContext.h"
#include "../../../numeric/dec_math.h"

#include <doctest/doctest.h>

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

inline BigDec d(const std::string &s) {
    std::optional<BigDec> parsed{BigDec::try_from_string(s)};
    REQUIRE(parsed.has_value());
    return *std::move(parsed);
}

// 陷阱全关的上下文：交叉验证表只比"结果 + 触发了哪些信号"，不能被抛出来的陷阱打断
inline DecContext quiet_context(
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
inline std::string flags_to_string(const DecSignalSet &flags) {
    std::string result;
    for (const DecCondition condition : kAllSignals) {
        if (!flags.has(condition)) continue;
        if (!result.empty()) result += ',';
        result += dec_condition_name(condition);
    }
    return result;
}

// 用例行按 '|' 拆开。末尾的空字段（"没有触发任何信号"）也要保留，所以不能用"遇到空就停"的写法
inline std::vector<std::string> split_fields(const std::string &line) {
    std::vector<std::string> fields{""};
    for (const char c : line) {
        if (c == '|')
            fields.emplace_back();
        else
            fields.back().push_back(c);
    }
    return fields;
}

inline DecRounding rounding_from_name(const std::string &name) {
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

inline DecCondition condition_from_name(const std::string &name) {
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
inline std::vector<std::string> interesting_strings() {
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

inline std::vector<BigDec> interesting_values() {
    std::vector<BigDec> result;
    for (const std::string &s : interesting_strings()) result.push_back(d(s));
    return result;
}
