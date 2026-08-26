// SemanticChecker：int/float 字面量 raw_ 的形状校验。
//
// raw_ 存的是词法层原样保留的源码文本（`1.50e-3` 就是这十个字符），SemanticChecker 在这里复核它
// 确实符合字面量文法——包括科学计数法后缀 `[eE][+-]?digits`：尾数不带小数点即为 int，指数必须非负
// 且不超过 9999；尾数带小数点即为 decimal，指数可正可负、不设上限。
//
// 这些规则 Lexer 已经把过一道，所以违反它们只可能是 Lexer/Parser 出了 bug，报的是 InternalError
// 而不是 SyntaxError；对应的畸形 raw_ 没法通过解析源码构造出来，只能手工搭树。正例则走真实源码，
// 确认这条链路（Lexer -> Parser -> SemanticChecker）整体是通的。
#include "test_utils.h"

#include <doctest/doctest.h>

namespace {

AstNodeProgramPtr wrap(AstNodePtr expr) {
    std::vector<AstNodePtr> exprs;
    exprs.push_back(std::move(expr));
    return std::make_unique<AstNodeProgram>(Position{0, 0}, std::move(exprs));
}

AstNodeProgramPtr int_raw(const std::u32string &raw) {
    return wrap(std::make_unique<AstNodeLiteralInt>(Position{0, 0}, raw));
}

AstNodeProgramPtr float_raw(const std::u32string &raw) {
    return wrap(std::make_unique<AstNodeLiteralFloat>(Position{0, 0}, raw));
}

} // namespace

TEST_SUITE("SemanticChecker 字面量——正例走真实源码，整条链路是通的") {

    TEST_CASE("普通 int / float 字面量") {
        CHECK_NOTHROW(check_program(U"0"));
        CHECK_NOTHROW(check_program(U"123"));
        CHECK_NOTHROW(check_program(U"123456789012345678901234567890"));
        CHECK_NOTHROW(check_program(U"0.0"));
        CHECK_NOTHROW(check_program(U"123.45"));
        CHECK_NOTHROW(check_program(U"0.05"));
    }

    TEST_CASE("科学计数法：尾数不带小数点，是 int") {
        CHECK_NOTHROW(check_program(U"1e9"));
        CHECK_NOTHROW(check_program(U"1E9"));
        CHECK_NOTHROW(check_program(U"1e+9"));
        CHECK_NOTHROW(check_program(U"0e0"));
        CHECK_NOTHROW(check_program(U"123e4"));
        CHECK_NOTHROW(check_program(U"1e9999")); // 恰好是上限
    }

    TEST_CASE("科学计数法：尾数带小数点，是 decimal，指数可正可负") {
        CHECK_NOTHROW(check_program(U"1.0e9"));
        CHECK_NOTHROW(check_program(U"1.5e-3"));
        CHECK_NOTHROW(check_program(U"1.5E-3"));
        CHECK_NOTHROW(check_program(U"1.00e+9"));
        CHECK_NOTHROW(check_program(U"0.05e3"));
        CHECK_NOTHROW(check_program(U"1.0e-0"));
        CHECK_NOTHROW(check_program(U"1.0e999999")); // decimal 侧不设指数上限
    }

    TEST_CASE("科学计数法出现在各种表达式位置上") {
        CHECK_NOTHROW(check_program(U"x = 1e9"));
        CHECK_NOTHROW(check_program(U"1e9 + 2.5e-3"));
        CHECK_NOTHROW(check_program(U"[1e3, 2.0e-2]"));
        CHECK_NOTHROW(check_program(U"f(1e3, k = 2.0e-2)"));
        CHECK_NOTHROW(check_program(U"for (i = 0; i < 1e3; i += 1) {}"));
        CHECK_NOTHROW(check_single_expr(U"1e9"));
        CHECK_NOTHROW(check_single_expr(U"1.5e-3"));
    }
}

TEST_SUITE("SemanticChecker 字面量——尾数部分的畸形 raw_（手工搭树）") {

    TEST_CASE("AstNodeLiteralInt：raw_ 是空字符串") {
        check_throws_internal_error_with(*int_raw(U""), "missing digits");
    }

    TEST_CASE("AstNodeLiteralInt：raw_ 含非数字字符") {
        check_throws_internal_error_with(*int_raw(U"12a"), "non-digit character");
    }

    TEST_CASE("AstNodeLiteralInt：raw_ 有前导零（单独一个 \"0\" 除外）") {
        check_throws_internal_error_with(*int_raw(U"007"), "leading zero");
        CHECK_NOTHROW(check_ast(*int_raw(U"0"))); // 单独一个 "0" 合法
    }

    TEST_CASE("AstNodeLiteralFloat：raw_ 缺少小数点") {
        check_throws_internal_error_with(*float_raw(U"123"), "missing a '.'");
        // 有指数后缀但仍然没有小数点：剥掉后缀之后照样得有小数点，不然它就该是个 int 节点
        check_throws_internal_error_with(*float_raw(U"123e4"), "missing a '.'");
    }

    TEST_CASE("AstNodeLiteralFloat：raw_ 小数点两侧缺数字") {
        check_throws_internal_error_with(*float_raw(U"1."), "missing digits");
        check_throws_internal_error_with(*float_raw(U".5"), "missing digits");
        check_throws_internal_error_with(*float_raw(U"1.e5"), "missing digits");
        check_throws_internal_error_with(*float_raw(U".5e5"), "missing digits");
    }

    TEST_CASE("AstNodeLiteralFloat：raw_ 含非数字字符") {
        check_throws_internal_error_with(*float_raw(U"1.5a"), "non-digit character");
    }

    TEST_CASE("AstNodeLiteralFloat：整数部分有前导零，跟 int 一致；小数部分没有这条限制") {
        check_throws_internal_error_with(*float_raw(U"007.5"), "leading zero");
        CHECK_NOTHROW(check_ast(*float_raw(U"0.05"))); // 小数部分的零不受限制
        CHECK_NOTHROW(check_ast(*float_raw(U"0.05e3")));
    }

    TEST_CASE("报错信息指名是哪一段出的问题（整数部分 / 小数部分 / 指数）") {
        check_throws_internal_error_with(*int_raw(U"007"), "leading zero in the integer part");
        check_throws_internal_error_with(*float_raw(U"007.5"), "leading zero in the integer part");
        check_throws_internal_error_with(
            *float_raw(U"1.5a"), "non-digit character in the fractional part"
        );
        check_throws_internal_error_with(*int_raw(U"1e007"), "leading zero in the exponent");
    }
}

TEST_SUITE("SemanticChecker 字面量——科学计数法后缀的畸形 raw_（手工搭树）") {

    TEST_CASE("指数部分缺数字") {
        check_throws_internal_error_with(*int_raw(U"1e"), "missing digits in the exponent");
        check_throws_internal_error_with(*int_raw(U"1E"), "missing digits in the exponent");
        check_throws_internal_error_with(*int_raw(U"1e+"), "missing digits in the exponent");
        check_throws_internal_error_with(*float_raw(U"1.5e"), "missing digits in the exponent");
        check_throws_internal_error_with(*float_raw(U"1.5e-"), "missing digits in the exponent");
    }

    TEST_CASE("指数部分含非数字字符（含第二个 e）") {
        check_throws_internal_error_with(*int_raw(U"1e9a"), "non-digit character in the exponent");
        check_throws_internal_error_with(*int_raw(U"1e1e9"), "non-digit character in the exponent");
        check_throws_internal_error_with(*int_raw(U"1e+-9"), "non-digit character in the exponent");
        check_throws_internal_error_with(
            *float_raw(U"1.5e-3x"), "non-digit character in the exponent"
        );
    }

    TEST_CASE("指数部分有前导零，int / decimal 两侧都不许") {
        check_throws_internal_error_with(*int_raw(U"1e01"), "leading zero in the exponent");
        check_throws_internal_error_with(*int_raw(U"1e00"), "leading zero in the exponent");
        check_throws_internal_error_with(*float_raw(U"1.0e-007"), "leading zero in the exponent");
        // 指数是单独一个 0 则合法
        CHECK_NOTHROW(check_ast(*int_raw(U"1e0")));
        CHECK_NOTHROW(check_ast(*float_raw(U"1.0e-0")));
    }

    TEST_CASE("int 侧不许负指数：只看写法，不看算出来的值") {
        check_throws_internal_error_with(*int_raw(U"1e-9"), "negative exponent");
        check_throws_internal_error_with(*int_raw(U"1e-0"), "negative exponent");
        // 值恰好是整数 10，仍然不合法
        check_throws_internal_error_with(*int_raw(U"100e-1"), "negative exponent");
        // 同样的数值写成 decimal 就合法
        CHECK_NOTHROW(check_ast(*float_raw(U"1.0e-9")));
        CHECK_NOTHROW(check_ast(*float_raw(U"100.0e-1")));
    }

    TEST_CASE("int 侧指数上限 9999：贴着边界两侧各测一遍") {
        CHECK_NOTHROW(check_ast(*int_raw(U"1e9999")));
        check_throws_internal_error_with(*int_raw(U"1e10000"), "out-of-range exponent");
        check_throws_internal_error_with(*int_raw(U"1e99999"), "out-of-range exponent");
    }

    TEST_CASE("指数位数 1..8 逐个扫：int 侧 4 位以内合法、5 位起报错，decimal 侧一路合法") {
        for (size_t digits{1}; digits <= 8; ++digits) {
            // 全 9：4 位时恰好是上限 9999，5 位时是最接近上限的越界值
            const std::u32string exponent(digits, U'9');
            CAPTURE(digits);

            if (digits <= 4) {
                CHECK_NOTHROW(check_ast(*int_raw(U"1e" + exponent)));
            } else {
                check_throws_internal_error_with(
                    *int_raw(U"1e" + exponent), "out-of-range exponent"
                );
            }
            CHECK_NOTHROW(check_ast(*float_raw(U"1.0e" + exponent)));
        }
    }

    TEST_CASE("decimal 侧不设指数上限（表示得下与否归后续的数值转换那层管，不归这里）") {
        CHECK_NOTHROW(check_ast(*float_raw(U"1.0e10000")));
        CHECK_NOTHROW(check_ast(*float_raw(U"1.0e-10000")));
        CHECK_NOTHROW(check_ast(*float_raw(U"1.0e999999999")));
    }

    TEST_CASE("前导零的检查早于指数上限的检查——上限只数位数，靠的正是这个顺序") {
        // 没有这条顺序，`1e00000000001`（值是 1）会被"位数 > 4"错判成超限
        check_throws_internal_error_with(
            *int_raw(U"1e00000000001"), "leading zero in the exponent"
        );
    }
}
