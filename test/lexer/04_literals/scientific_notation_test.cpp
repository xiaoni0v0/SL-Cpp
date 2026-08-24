// 字面量——科学计数法：`[eE][+-]?digits` 后缀，int 和 decimal 共用。
//
// 类型只看尾数带不带小数点：`1e9` 是 int，`1.0e9` 是 decimal；不看指数正负、更不看算出来的值
// （`100e-1` 值恰好是整数 10，仍然不合法）。int 侧额外有两条限制：指数必须非负、且不超过 9999；
// decimal 侧两条都不适用。
//
// 词法层只保留原文、不做数值展开（`1e9` 的 token 文本就是 "1e9"），数值转换归 numeric 那边。
#include "../../../builtins/exceptions/SyntaxError.h"
#include "../test_utils.h"

#include <doctest/doctest.h>

#include <string>

namespace {

// 必然抛 SyntaxError 的输入：取出异常消息，用来断言措辞和行列。
// 消息形如 "<unknown>:1:3:\nSyntaxError: ..."
std::string lex_error(const std::u32string &source) {
    try {
        (void) lex(source);
    } catch (const SyntaxError &e) {
        return e.what();
    }
    return "<没有抛出异常>";
}

bool has(const std::string &haystack, const std::string &needle) {
    return haystack.find(needle) != std::string::npos;
}

// 期望某个源码整体被读成单个字面量 token，且 token 文本跟源码逐字一致
std::string sole_literal(const std::u32string &source, const bool is_decimal) {
    return std::string{is_decimal ? "LITERAL_FLOAT(" : "LITERAL_INT("} + u32_to_utf8(source) + ")";
}

} // namespace

TEST_SUITE("科学计数法——尾数不带小数点，整体是 int") {

    TEST_CASE("基本形状：e/E 都收，指数的正号可省") {
        CHECK(lex_dump(U"1e9") == "LITERAL_INT(1e9)");
        CHECK(lex_dump(U"1E9") == "LITERAL_INT(1E9)");
        CHECK(lex_dump(U"1e+9") == "LITERAL_INT(1e+9)");
        CHECK(lex_dump(U"1E+9") == "LITERAL_INT(1E+9)");
    }

    TEST_CASE("指数为 0、尾数为 0 都合法（单独一个 0 不算前导零）") {
        CHECK(lex_dump(U"1e0") == "LITERAL_INT(1e0)");
        CHECK(lex_dump(U"0e0") == "LITERAL_INT(0e0)");
        CHECK(lex_dump(U"0e5") == "LITERAL_INT(0e5)");
        CHECK(lex_dump(U"0e9999") == "LITERAL_INT(0e9999)");
    }

    TEST_CASE("多位尾数、超长尾数：词法层不做数值转换，原样存") {
        CHECK(lex_dump(U"123e4") == "LITERAL_INT(123e4)");
        CHECK(lex_dump(U"1000000e1") == "LITERAL_INT(1000000e1)");
        CHECK(
            lex_dump(U"123456789012345678901234567890e5") ==
            "LITERAL_INT(123456789012345678901234567890e5)"
        );
    }
}

TEST_SUITE("科学计数法——尾数带小数点，整体是 decimal") {

    TEST_CASE("基本形状，指数可正可负") {
        CHECK(lex_dump(U"1.0e9") == "LITERAL_FLOAT(1.0e9)");
        CHECK(lex_dump(U"1.0E9") == "LITERAL_FLOAT(1.0E9)");
        CHECK(lex_dump(U"1.0e+9") == "LITERAL_FLOAT(1.0e+9)");
        CHECK(lex_dump(U"1.5e-3") == "LITERAL_FLOAT(1.5e-3)");
        CHECK(lex_dump(U"1.5E-3") == "LITERAL_FLOAT(1.5E-3)");
        CHECK(lex_dump(U"0.0e0") == "LITERAL_FLOAT(0.0e0)");
    }

    TEST_CASE("标度原样保留：末尾零、前导零都不归一（标度是 decimal 值的一部分）") {
        CHECK(lex_dump(U"1.00e9") == "LITERAL_FLOAT(1.00e9)");
        CHECK(lex_dump(U"1.000e3") == "LITERAL_FLOAT(1.000e3)");
        CHECK(lex_dump(U"0.05e3") == "LITERAL_FLOAT(0.05e3)");
        CHECK(lex_dump(U"0.0001e3") == "LITERAL_FLOAT(0.0001e3)");
        CHECK(lex_dump(U"1.50e-3") == "LITERAL_FLOAT(1.50e-3)");
    }
}

TEST_SUITE("科学计数法——int 侧指数的两条限制，decimal 侧都不适用") {

    TEST_CASE("负指数不合法：只看写法，不看算出来的值") {
        CHECK_THROWS_AS(lex(U"1e-9"), SyntaxError);
        CHECK_THROWS_AS(lex(U"1e-1"), SyntaxError);
        CHECK_THROWS_AS(lex(U"1e-0"), SyntaxError); // 指数是 -0 也不行
        CHECK_THROWS_AS(lex(U"0e-0"), SyntaxError);
        // 值恰好是整数 10，仍然不合法——类型判定不对字面量求值
        CHECK_THROWS_AS(lex(U"100e-1"), SyntaxError);
        CHECK(has(lex_error(U"1e-9"), "a negative exponent needs a fractional part"));
    }

    TEST_CASE("同一个数值补上 `.0` 写成 decimal 就合法") {
        CHECK(lex_dump(U"1.0e-9") == "LITERAL_FLOAT(1.0e-9)");
        CHECK(lex_dump(U"100.0e-1") == "LITERAL_FLOAT(100.0e-1)");
        CHECK(lex_dump(U"1.0e-0") == "LITERAL_FLOAT(1.0e-0)");
    }

    TEST_CASE("指数上限 9999：贴着边界两侧各测一遍") {
        CHECK(lex_dump(U"1e9998") == "LITERAL_INT(1e9998)");
        CHECK(lex_dump(U"1e9999") == "LITERAL_INT(1e9999)"); // 恰好是上限
        CHECK_THROWS_AS(lex(U"1e10000"), SyntaxError);       // 最小的越界值
        CHECK_THROWS_AS(lex(U"1e99999"), SyntaxError);
        CHECK(has(lex_error(U"1e10000"), "exponent of an integer literal may not exceed 9999"));
    }

    TEST_CASE("指数位数 1..8 逐个扫：int 侧 4 位以内合法、5 位起报错，decimal 侧一路合法") {
        for (size_t digits{1}; digits <= 8; ++digits) {
            // 全 9：4 位时恰好是上限 9999，5 位时是最接近上限的越界值
            const std::u32string exponent(digits, U'9');
            CAPTURE(digits);

            const std::u32string int_source{U"1e" + exponent};
            if (digits <= 4) {
                CHECK(lex_dump(int_source) == sole_literal(int_source, false));
            } else {
                CHECK(has(lex_error(int_source), "may not exceed 9999"));
            }

            const std::u32string decimal_source{U"1.0e" + exponent};
            CHECK(lex_dump(decimal_source) == sole_literal(decimal_source, true));
        }
    }

    TEST_CASE("上限只卡 e 记法，手写的等长字面量不受限") {
        // 1 后面手写 10000 个 0，跟 1e10000 是同一个数值
        const std::u32string written_out{U"1" + std::u32string(10000, U'0')};
        const auto tokens{lex(written_out)};
        REQUIRE(tokens.size() == 2); // 字面量 + EOF
        CHECK(tokens[0].type == TokenType::LITERAL_INT);
        CHECK(tokens[0].lexeme == written_out);
        CHECK_THROWS_AS(lex(U"1e10000"), SyntaxError); // 同一个数值，e 记法就不行
    }

    TEST_CASE("decimal 侧的指数不设上限") {
        CHECK(lex_dump(U"1.0e10000") == "LITERAL_FLOAT(1.0e10000)");
        CHECK(lex_dump(U"1.0e999999") == "LITERAL_FLOAT(1.0e999999)");
        CHECK(lex_dump(U"1.0e-999999") == "LITERAL_FLOAT(1.0e-999999)");
    }
}

TEST_SUITE("科学计数法——前导零：三个部位各自的规矩") {

    TEST_CASE("整数部分不许前导零，报错指名 integer part") {
        CHECK_THROWS_AS(lex(U"01e5"), SyntaxError);
        CHECK_THROWS_AS(lex(U"00e5"), SyntaxError);
        CHECK_THROWS_AS(lex(U"007.5e2"), SyntaxError);
        CHECK(has(lex_error(U"01e5"), "leading zeros in the integer part are not permitted"));
        // 单独一个 0 合法
        CHECK(lex_dump(U"0e5") == "LITERAL_INT(0e5)");
        CHECK(lex_dump(U"0.5e2") == "LITERAL_FLOAT(0.5e2)");
    }

    TEST_CASE("小数部分不受限") {
        CHECK(lex_dump(U"1.05e3") == "LITERAL_FLOAT(1.05e3)");
        CHECK(lex_dump(U"1.000e3") == "LITERAL_FLOAT(1.000e3)");
        CHECK(lex_dump(U"0.00001e3") == "LITERAL_FLOAT(0.00001e3)");
    }

    TEST_CASE("指数部分不许前导零，报错指名 exponent；单独一个 0 合法") {
        CHECK_THROWS_AS(lex(U"1e01"), SyntaxError);
        CHECK_THROWS_AS(lex(U"1e00"), SyntaxError);
        CHECK_THROWS_AS(lex(U"1e+007"), SyntaxError);
        CHECK_THROWS_AS(lex(U"1.0e-007"), SyntaxError);
        CHECK(has(lex_error(U"1e01"), "leading zeros in the exponent are not permitted"));
        CHECK(lex_dump(U"1e0") == "LITERAL_INT(1e0)");
        CHECK(lex_dump(U"1.0e-0") == "LITERAL_FLOAT(1.0e-0)");
    }

    TEST_CASE("前导零的检查早于指数上限的检查——上限只数位数，靠的正是这个顺序") {
        // 没有这条顺序，`1e00000000001`（值是 1）会被"位数 > 4"错判成超限
        CHECK(has(lex_error(U"1e00000000001"), "leading zeros in the exponent"));
        CHECK(has(lex_error(U"1e000000000000000000009"), "leading zeros in the exponent"));
    }
}

TEST_SUITE("科学计数法——畸形写法") {

    TEST_CASE("指数部分缺数字") {
        for (const char32_t *const raw :
             {U"1e", U"1E", U"1e+", U"1e-", U"1E-", U"1.5e", U"1.5e+", U"1.5e-"}) {
            const std::u32string source{raw};
            CAPTURE(u32_to_utf8(source));
            CHECK_THROWS_AS(lex(source), SyntaxError);
            CHECK(has(lex_error(source), "missing exponent digits in numeric literal"));
        }
    }

    TEST_CASE("指数位置出现非数字：一律归到「缺指数数字」") {
        CHECK(has(lex_error(U"1e.5"), "missing exponent digits"));
        CHECK(has(lex_error(U"1e*2"), "missing exponent digits"));
        CHECK(has(lex_error(U"1eabc"), "missing exponent digits"));
        CHECK(has(lex_error(U"1ee9"), "missing exponent digits"));
        CHECK(has(lex_error(U"1e 9"), "missing exponent digits"));  // 空格隔开也不行
        CHECK(has(lex_error(U"1e+ 9"), "missing exponent digits")); // 符号后隔空格同理
        CHECK(has(lex_error(U"1e)"), "missing exponent digits"));
    }

    TEST_CASE("指数的符号只允许一个") {
        CHECK_THROWS_AS(lex(U"1e+-9"), SyntaxError);
        CHECK_THROWS_AS(lex(U"1e-+9"), SyntaxError);
        CHECK_THROWS_AS(lex(U"1e--9"), SyntaxError);
        CHECK_THROWS_AS(lex(U"1e++9"), SyntaxError);
        CHECK_THROWS_AS(lex(U"1.0e+-9"), SyntaxError);
    }

    TEST_CASE("指数之后紧跟字母/下划线，跟不带 e 的情形一样非法") {
        CHECK_THROWS_AS(lex(U"1e9abc"), SyntaxError);
        CHECK_THROWS_AS(lex(U"1e9_"), SyntaxError);
        CHECK_THROWS_AS(lex(U"1.5e3x"), SyntaxError);
        CHECK(has(lex_error(U"1e9abc"), "unexpected character 'a'"));
        CHECK(has(lex_error(U"1e9_"), "unexpected character '_'"));
        // 第二个 e：第一个 e 已经把指数吃完了，它落进「数字后紧跟字母」那条
        CHECK(has(lex_error(U"1e1e9"), "unexpected character 'e'"));
    }

    TEST_CASE("e 被贪婪吃掉不会改变任何可接受的程序：数字后直接跟字母本来就非法") {
        // `1else` 在加科学计数法之前就是错的，之后依然是错的，只是换了条错误信息；
        // 要写「1 后面跟 else」必须有分隔，那时 e 根本轮不到数字侧看见
        CHECK_THROWS_AS(lex(U"1else"), SyntaxError);
        CHECK(lex_dump(U"1 else") == "LITERAL_INT(1) KW_ELSE");
        CHECK(lex_dump(U"1\nelse") == "LITERAL_INT(1) NEWLINE KW_ELSE");
    }
}

TEST_SUITE("科学计数法——与点号 / range / Ellipsis 的交界") {

    TEST_CASE("`1.e9`：点后面不是数字，点不归数字管，e9 成了标识符") {
        CHECK(lex_dump(U"1.e9") == "LITERAL_INT(1) SIGN_DOT IDENTIFIER(e9)");
        CHECK(lex_dump(U"1.E9") == "LITERAL_INT(1) SIGN_DOT IDENTIFIER(E9)");
    }

    TEST_CASE("指数之后的点不归数字管（跟 float 后面的点同一条规矩）") {
        CHECK(lex_dump(U"1e9.5") == "LITERAL_INT(1e9) SIGN_DOT LITERAL_INT(5)");
        CHECK(
            lex_dump(U"1e9.f()") ==
            "LITERAL_INT(1e9) SIGN_DOT IDENTIFIER(f) SIGN_LPAREN SIGN_RPAREN"
        );
        CHECK(
            lex_dump(U"1.5e3.f()") ==
            "LITERAL_FLOAT(1.5e3) SIGN_DOT IDENTIFIER(f) SIGN_LPAREN SIGN_RPAREN"
        );
    }

    TEST_CASE("range：两端都能是科学计数法，不需要空格") {
        CHECK(lex_dump(U"1e9..2e9") == "LITERAL_INT(1e9) SIGN_DOTDOT LITERAL_INT(2e9)");
        CHECK(lex_dump(U"0e0..1e1") == "LITERAL_INT(0e0) SIGN_DOTDOT LITERAL_INT(1e1)");
        CHECK(lex_dump(U"1.5e3..2.5e3") == "LITERAL_FLOAT(1.5e3) SIGN_DOTDOT LITERAL_FLOAT(2.5e3)");
        CHECK(
            lex_dump(U"1.0e-3..1.0e3") == "LITERAL_FLOAT(1.0e-3) SIGN_DOTDOT LITERAL_FLOAT(1.0e3)"
        );
    }

    TEST_CASE("Ellipsis 与科学计数法相邻") {
        CHECK(lex_dump(U"1e9...2e9") == "LITERAL_INT(1e9) LITERAL_ELLIPSIS LITERAL_INT(2e9)");
        CHECK(lex_dump(U"5e2...") == "LITERAL_INT(5e2) LITERAL_ELLIPSIS");
        CHECK(lex_dump(U"...5e2") == "LITERAL_ELLIPSIS LITERAL_INT(5e2)");
    }
}

TEST_SUITE("科学计数法——token 文本与位置") {

    TEST_CASE("token 文本跟源码逐字一致：不归一大小写、不补/删正号、不展开成数值") {
        for (const char32_t *const raw :
             {U"1e9",
              U"1E9",
              U"1e+9",
              U"1E+9",
              U"1e0",
              U"1e9999",
              U"1.0e9",
              U"1.5E-3",
              U"0.05e3",
              U"1.00e-0"}) {
            const std::u32string source{raw};
            CAPTURE(u32_to_utf8(source));
            const auto tokens{lex(source)};
            REQUIRE(tokens.size() == 2); // 字面量 + EOF
            CHECK(tokens[0].lexeme == source);
        }
    }

    TEST_CASE("token 的行列是字面量开头的位置，不是结尾") {
        const auto tokens{lex(U"  1.5e-3")};
        REQUIRE(tokens.size() == 2);
        CHECK(tokens[0].row == 1);
        CHECK(tokens[0].col == 3);
    }

    TEST_CASE("跨行的科学计数法位置正确") {
        const auto tokens{lex(U"1e3\n  2.0e-4")};
        REQUIRE(tokens.size() == 4); // INT NEWLINE FLOAT EOF
        CHECK(tokens[0].row == 1);
        CHECK(tokens[0].col == 1);
        CHECK(tokens[2].row == 2);
        CHECK(tokens[2].col == 3);
    }

    TEST_CASE("「整条字面量的形状不对」这类报错指向字面量开头") {
        CHECK(has(lex_error(U"  1e-9"), ":1:3:"));
        CHECK(has(lex_error(U"  1e10000"), ":1:3:"));
        CHECK(has(lex_error(U"  1e01"), ":1:3:"));
        CHECK(has(lex_error(U"  01e5"), ":1:3:"));
    }

    TEST_CASE("「某一处具体出了问题」这类报错指向那一处") {
        CHECK(has(lex_error(U"1e"), ":1:3:"));  // 该有指数数字的位置
        CHECK(has(lex_error(U"1e+"), ":1:4:")); // 符号之后
        CHECK(has(lex_error(U"1.5e"), ":1:5:"));
        CHECK(has(lex_error(U"1e9abc"), ":1:4:")); // 冒犯的那个字符
    }

    TEST_CASE("第二行上的报错行号正确") {
        CHECK(has(lex_error(U"1\n1e-9"), ":2:1:"));
        CHECK(has(lex_error(U"x = 1\ny = 1e10000"), ":2:5:"));
    }
}

TEST_SUITE("科学计数法——组合扫描与代码片段") {

    TEST_CASE("尾数 × e/E × 指数符号 全组合：合法性和类型都按规则推，不照抄实现") {
        struct Mantissa {
            const char32_t *text;
            bool is_decimal;
        };
        constexpr Mantissa mantissas[]{
            {U"1", false},
            {U"0", false},
            {U"123", false},
            {U"1.0", true},
            {U"0.5", true},
            {U"12.34", true},
        };
        for (const auto &[mantissa, is_decimal] : mantissas) {
            for (const char32_t *const exponent_marker : {U"e", U"E"}) {
                for (const char32_t *const sign : {U"", U"+", U"-"}) {
                    const std::u32string source{
                        std::u32string{mantissa} + exponent_marker + sign + U"7"
                    };
                    CAPTURE(u32_to_utf8(source));
                    // 规则：尾数不带小数点（即结果为 int）时不收负指数，其余一律合法
                    if (!is_decimal && std::u32string{sign} == U"-") {
                        CHECK_THROWS_AS(lex(source), SyntaxError);
                    } else {
                        CHECK(lex_dump(source) == sole_literal(source, is_decimal));
                    }
                }
            }
        }
    }

    TEST_CASE("赋值、算术、调用、容器里都能正确切分") {
        CHECK(lex_dump(U"x = 1e9") == "IDENTIFIER(x) SIGN_ASSIGN LITERAL_INT(1e9)");
        CHECK(lex_dump(U"1e9+1") == "LITERAL_INT(1e9) SIGN_PLUS LITERAL_INT(1)");
        CHECK(lex_dump(U"2.5e-3*4") == "LITERAL_FLOAT(2.5e-3) SIGN_STAR LITERAL_INT(4)");
        CHECK(lex_dump(U"f(1e3)") == "IDENTIFIER(f) SIGN_LPAREN LITERAL_INT(1e3) SIGN_RPAREN");
        CHECK(
            lex_dump(U"[1e3, 2.0e-2]") == "SIGN_LBRACKET LITERAL_INT(1e3) SIGN_COMMA "
                                          "LITERAL_FLOAT(2.0e-2) SIGN_RBRACKET"
        );
        CHECK(lex_dump(U"1e3;2e3") == "LITERAL_INT(1e3) SIGN_SEMICOLON LITERAL_INT(2e3)");
    }

    TEST_CASE("负号是独立的一元运算符，不属于字面量") {
        CHECK(lex_dump(U"-1e9") == "SIGN_MINUS LITERAL_INT(1e9)");
        CHECK(lex_dump(U"-1.0e-9") == "SIGN_MINUS LITERAL_FLOAT(1.0e-9)");
        CHECK(lex_dump(U"- 1e9") == "SIGN_MINUS LITERAL_INT(1e9)");
    }

    TEST_CASE("相邻两个科学计数法字面量之间不会互相污染") {
        CHECK(lex_dump(U"1e3 1e4") == "LITERAL_INT(1e3) LITERAL_INT(1e4)");
        CHECK(lex_dump(U"1e3,2e3") == "LITERAL_INT(1e3) SIGN_COMMA LITERAL_INT(2e3)");
        CHECK(lex_dump(U"1.0e3 2.0e-3") == "LITERAL_FLOAT(1.0e3) LITERAL_FLOAT(2.0e-3)");
    }

    TEST_CASE("紧贴除号：`/` 和 `//` 都不会跟指数抢字符") {
        CHECK(lex_dump(U"1e3/2e3") == "LITERAL_INT(1e3) SIGN_SLASH LITERAL_INT(2e3)");
        CHECK(lex_dump(U"1e3//2e3") == "LITERAL_INT(1e3) SIGN_DOUBLESLASH LITERAL_INT(2e3)");
        CHECK(lex_dump(U"1.0e3//2") == "LITERAL_FLOAT(1.0e3) SIGN_DOUBLESLASH LITERAL_INT(2)");
    }

    TEST_CASE("注释和字符串里的科学计数法只是普通文本，不参与词法判定") {
        CHECK(lex_dump(U"# 1e-9\n1e9") == "LITERAL_INT(1e9)");
        CHECK(lex_dump(U"1e9 # 1e10000") == "LITERAL_INT(1e9)");
        CHECK(lex_dump(U"\"1e-9\"") == "LITERAL_STR(1e-9)");
        CHECK(lex_dump(U"`1e10000`") == "LITERAL_STR(1e10000)");
        CHECK_NOTHROW(lex(U"/* 1e10000 01e5 1e */ 1e9")); // 块注释里的畸形写法不报错
    }
}
