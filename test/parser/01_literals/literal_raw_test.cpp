// int/decimal 节点构造时校验 raw_ 形状。违反即 InternalError。
#include "../../../cpp_exceptions/InternalError.h"
#include "../test_utils.h"

#include <doctest/doctest.h>

namespace {

// 直接构造一个字面量节点：校验就发生在构造函数里，"构造得出来"本身就是通过
AstNodePtr int_raw(const std::u32string &raw) {
    return std::make_unique<AstNodeLiteralInt>(Position{1, 1}, raw);
}

AstNodePtr decimal_raw(const std::u32string &raw) {
    return std::make_unique<AstNodeLiteralDecimal>(Position{1, 1}, raw);
}

// 要求构造抛出 InternalError，且消息里含指定子串（用于区分"确实是这条规则报的错"）
template <typename Build> void raw_throws(Build &&build, const std::string &message_substring) {
    try {
        build();
        FAIL("expected InternalError containing: " << message_substring);
    } catch (const InternalError &e) {
        const std::string what{e.what()};
        CHECK_MESSAGE(
            what.find(message_substring) != std::string::npos,
            "expected message to contain \"" << message_substring << "\", got: " << what
        );
    }
}

void int_raw_throws(const std::u32string &raw, const std::string &message_substring) {
    raw_throws([&] { return int_raw(raw); }, message_substring);
}

void decimal_raw_throws(const std::u32string &raw, const std::string &message_substring) {
    raw_throws([&] { return decimal_raw(raw); }, message_substring);
}

} // namespace

TEST_SUITE("字面量 raw_ 校验——正例走真实源码，Lexer 吐出来的东西构造函数一律接受") {

    TEST_CASE("普通 int / decimal 字面量") {
        CHECK_NOTHROW(parse_as_file(U"0"));
        CHECK_NOTHROW(parse_as_file(U"123"));
        CHECK_NOTHROW(parse_as_file(U"123456789012345678901234567890"));
        CHECK_NOTHROW(parse_as_file(U"0.0"));
        CHECK_NOTHROW(parse_as_file(U"123.45"));
        CHECK_NOTHROW(parse_as_file(U"0.05"));
    }

    TEST_CASE("科学计数法：尾数不带小数点，是 int") {
        CHECK_NOTHROW(parse_as_file(U"1e9"));
        CHECK_NOTHROW(parse_as_file(U"1E9"));
        CHECK_NOTHROW(parse_as_file(U"1e+9"));
        CHECK_NOTHROW(parse_as_file(U"0e0"));
        CHECK_NOTHROW(parse_as_file(U"123e4"));
        CHECK_NOTHROW(parse_as_file(U"1e9999")); // 恰好是上限
        // 光是"能解析"钉不住"折出来的节点类型对不对"——万一 lexer/parser 把它收成了
        // LiteralDecimal，上面这些 CHECK_NOTHROW 全部照样绿
        CHECK(parse_json(U"1e9") == int_lit("1e9"));
        CHECK(parse_json(U"1e9")["type"] == "LiteralInt");
    }

    TEST_CASE("科学计数法：尾数带小数点，是 decimal，指数可正可负") {
        CHECK_NOTHROW(parse_as_file(U"1.0e9"));
        CHECK_NOTHROW(parse_as_file(U"1.5e-3"));
        CHECK_NOTHROW(parse_as_file(U"1.5E-3"));
        CHECK_NOTHROW(parse_as_file(U"1.00e+9"));
        CHECK_NOTHROW(parse_as_file(U"0.05e3"));
        CHECK_NOTHROW(parse_as_file(U"1.0e-0"));
        CHECK_NOTHROW(parse_as_file(U"1.0e999999")); // decimal 侧不设指数上限
        CHECK(parse_json(U"1.0e9")["type"] == "LiteralDecimal");
        CHECK(parse_json(U"1.0e9")["raw"] == "1.0e9");
    }

    TEST_CASE("科学计数法出现在各种表达式位置上") {
        CHECK_NOTHROW(parse_as_file(U"x = 1e9"));
        CHECK_NOTHROW(parse_as_file(U"1e9 + 2.5e-3"));
        CHECK_NOTHROW(parse_as_file(U"[1e3, 2.0e-2]"));
        CHECK_NOTHROW(parse_as_file(U"f(1e3, k = 2.0e-2)"));
        CHECK_NOTHROW(parse_as_file(U"for (i = 0; i < 1e3; i += 1) {}"));
        CHECK_NOTHROW(parse_as_single_expr(U"1e9"));
        CHECK_NOTHROW(parse_as_single_expr(U"1.5e-3"));
    }
}

TEST_SUITE("字面量 raw_ 校验——前导负号（常量折叠的产物形状）") {

    TEST_CASE("int：一个前导负号合法") {
        CHECK_NOTHROW(int_raw(U"-1"));
        CHECK_NOTHROW(int_raw(U"-9223372036854775808"));
        CHECK_NOTHROW(int_raw(U"-0")); // 单独一个 0 带负号也算合法形状
    }

    TEST_CASE("decimal：一个前导负号合法，-0.0 也是") {
        CHECK_NOTHROW(decimal_raw(U"-1.5"));
        CHECK_NOTHROW(decimal_raw(U"-0.0"));
        CHECK_NOTHROW(decimal_raw(U"-0.05"));
    }

    TEST_CASE("负号后面照样得是合法的数字串") {
        int_raw_throws(U"-", "missing digits in the integer part");
        int_raw_throws(U"-007", "leading zero in the integer part");
        int_raw_throws(U"-1a", "non-digit character in the integer part");
        decimal_raw_throws(U"-1", "missing a '.'");
        decimal_raw_throws(U"-.5", "missing digits in the integer part");
    }

    TEST_CASE("正号不接受：没有任何一个生产者会写出来") {
        int_raw_throws(U"+1", "non-digit character in the integer part");
        decimal_raw_throws(U"+1.5", "non-digit character in the integer part");
    }

    TEST_CASE("负号只能有一个、只能在最前面") {
        int_raw_throws(U"--1", "non-digit character in the integer part");
        int_raw_throws(U"1-", "non-digit character in the integer part");
        decimal_raw_throws(U"-1.-5", "non-digit character in the fractional part");
    }

    TEST_CASE("负号跟科学计数法后缀可以同时出现") {
        CHECK_NOTHROW(int_raw(U"-1e9"));
        CHECK_NOTHROW(decimal_raw(U"-1.5e-3"));
        // 尾数的负号不影响指数那边的规则
        int_raw_throws(U"-1e-9", "negative exponent");
        int_raw_throws(U"-1e10000", "out-of-range exponent");
    }
}

TEST_SUITE("字面量 raw_ 校验——尾数部分的畸形输入") {

    TEST_CASE("AstNodeLiteralInt：raw_ 是空字符串") { int_raw_throws(U"", "missing digits"); }

    TEST_CASE("AstNodeLiteralInt：raw_ 含非数字字符") {
        int_raw_throws(U"12a", "non-digit character");
    }

    TEST_CASE("AstNodeLiteralInt：raw_ 有前导零（单独一个 0 除外）") {
        int_raw_throws(U"007", "leading zero");
        CHECK_NOTHROW(int_raw(U"0")); // 单独一个 0 合法
    }

    TEST_CASE("AstNodeLiteralDecimal：raw_ 缺少小数点") {
        decimal_raw_throws(U"123", "missing a '.'");
        // 有指数后缀但仍然没有小数点：剥掉后缀之后照样得有小数点，不然它就该是个 int 节点
        decimal_raw_throws(U"123e4", "missing a '.'");
    }

    TEST_CASE("AstNodeLiteralDecimal：raw_ 小数点两侧缺数字") {
        decimal_raw_throws(U"1.", "missing digits");
        decimal_raw_throws(U".5", "missing digits");
        decimal_raw_throws(U"1.e5", "missing digits");
        decimal_raw_throws(U".5e5", "missing digits");
    }

    TEST_CASE("AstNodeLiteralDecimal：raw_ 含非数字字符") {
        decimal_raw_throws(U"1.5a", "non-digit character");
    }

    TEST_CASE("AstNodeLiteralDecimal：整数部分有前导零，跟 int 一致；小数部分没有这条限制") {
        decimal_raw_throws(U"007.5", "leading zero");
        CHECK_NOTHROW(decimal_raw(U"0.05")); // 小数部分的零不受限制
        CHECK_NOTHROW(decimal_raw(U"0.05e3"));
    }

    TEST_CASE("报错信息指名是哪一段出的问题（整数部分 / 小数部分 / 指数）") {
        int_raw_throws(U"007", "leading zero in the integer part");
        decimal_raw_throws(U"007.5", "leading zero in the integer part");
        decimal_raw_throws(U"1.5a", "non-digit character in the fractional part");
        int_raw_throws(U"1e007", "leading zero in the exponent");
    }

    TEST_CASE("文件名那一格是占位符：节点只带行列，不拖文件名") {
        int_raw_throws(U"12a", "<file>:1:1:");
    }
}

TEST_SUITE("字面量 raw_ 校验——科学计数法后缀的畸形输入") {

    TEST_CASE("指数部分缺数字") {
        int_raw_throws(U"1e", "missing digits in the exponent");
        int_raw_throws(U"1E", "missing digits in the exponent");
        int_raw_throws(U"1e+", "missing digits in the exponent");
        decimal_raw_throws(U"1.5e", "missing digits in the exponent");
        decimal_raw_throws(U"1.5e-", "missing digits in the exponent");
    }

    TEST_CASE("指数部分含非数字字符（含第二个 e）") {
        int_raw_throws(U"1e9a", "non-digit character in the exponent");
        int_raw_throws(U"1e1e9", "non-digit character in the exponent");
        int_raw_throws(U"1e+-9", "non-digit character in the exponent");
        decimal_raw_throws(U"1.5e-3x", "non-digit character in the exponent");
    }

    TEST_CASE("指数部分有前导零，int / decimal 两侧都不许") {
        int_raw_throws(U"1e01", "leading zero in the exponent");
        int_raw_throws(U"1e00", "leading zero in the exponent");
        decimal_raw_throws(U"1.0e-007", "leading zero in the exponent");
        // 指数是单独一个 0 则合法
        CHECK_NOTHROW(int_raw(U"1e0"));
        CHECK_NOTHROW(decimal_raw(U"1.0e-0"));
    }

    TEST_CASE("int 侧不许负指数：只看写法，不看算出来的值") {
        int_raw_throws(U"1e-9", "negative exponent");
        int_raw_throws(U"1e-0", "negative exponent");
        // 值恰好是整数 10，仍然不合法
        int_raw_throws(U"100e-1", "negative exponent");
        // 同样的数值写成 decimal 就合法
        CHECK_NOTHROW(decimal_raw(U"1.0e-9"));
        CHECK_NOTHROW(decimal_raw(U"100.0e-1"));
    }

    TEST_CASE("int 侧指数上限 9999：贴着边界两侧各测一遍") {
        CHECK_NOTHROW(int_raw(U"1e9999"));
        int_raw_throws(U"1e10000", "out-of-range exponent");
        int_raw_throws(U"1e99999", "out-of-range exponent");
    }

    TEST_CASE("指数位数 1..8 逐个扫：int 侧 4 位以内合法、5 位起报错，decimal 侧一路合法") {
        for (size_t digits{1}; digits <= 8; ++digits) {
            // 全 9：4 位时恰好是上限 9999，5 位时是最接近上限的越界值
            const std::u32string exponent(digits, U'9');
            CAPTURE(digits);

            if (digits <= 4) {
                CHECK_NOTHROW(int_raw(U"1e" + exponent));
            } else {
                int_raw_throws(U"1e" + exponent, "out-of-range exponent");
            }
            CHECK_NOTHROW(decimal_raw(U"1.0e" + exponent));
        }
    }

    TEST_CASE("decimal 侧不设指数上限（表示得下与否归后续的数值转换那层管，不归这里）") {
        CHECK_NOTHROW(decimal_raw(U"1.0e10000"));
        CHECK_NOTHROW(decimal_raw(U"1.0e-10000"));
        CHECK_NOTHROW(decimal_raw(U"1.0e999999999"));
    }

    TEST_CASE("前导零的检查早于指数上限的检查——上限只数位数，靠的正是这个顺序") {
        // 没有这条顺序，`1e00000000001`（值是 1）会被"位数 > 4"错判成超限
        int_raw_throws(U"1e00000000001", "leading zero in the exponent");
    }
}
