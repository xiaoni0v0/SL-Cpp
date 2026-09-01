#pragma once

// BigInt 测试共用：十进制构造、边界值池、'|' 拆字段。

#include "../../../numeric/BigInt.h"

#include <doctest/doctest.h>

#include <string>
#include <vector>

inline BigInt d(const std::string &s) { return BigInt::from_decimal_string(s); }

// 用例行按 '|' 拆开
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

// 覆盖各种"容易出 bug"的数据，供后面的恒等式交叉验证批量使用：0/±1、int64_t 边界内外、
// 恰好卡在 shrink() 判定边界上的值（2^63 附近）、全 1 比特的 limb（bitwise
// 安全余量最容易翻车的地方）、多 limb 的超大数
inline std::vector<BigInt> interesting_values() {
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
