// SL.md 字面量——decimal：`123.45`；整数、小数部分都不能省略，`1.`、`.1` 不是合法的 decimal
// 字面量。
//
// 点号和数字更复杂的交互（`1.0..1.2` 这类）放在 05_operators/dot_disambiguation_test.cpp，
// 科学计数法后缀（`1.5e-3`）放在 scientific_notation_test.cpp，这里只测 decimal 字面量本身的
// 基本形状。
#include "../../../builtins/exceptions/SyntaxError.h"
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("decimal") {

    TEST_CASE("基本浮点数") {
        CHECK(lex_dump(U"123.45") == "LITERAL_DECIMAL(123.45)");
        CHECK(lex_dump(U"0.0") == "LITERAL_DECIMAL(0.0)");
        CHECK(lex_dump(U"1.0") == "LITERAL_DECIMAL(1.0)");
    }

    TEST_CASE("`1.`：小数部分省略，不是合法 decimal，词法为 INT 后跟 DOT") {
        CHECK(lex_dump(U"1.") == "LITERAL_INT(1) SIGN_DOT");
    }

    TEST_CASE("`.1`：整数部分省略，不是合法 decimal 的开头，词法为 DOT 后跟 INT") {
        CHECK(lex_dump(U".1") == "SIGN_DOT LITERAL_INT(1)");
    }

    TEST_CASE("`1.` 紧跟标识符：典型场景是对 int 字面量取属性/调方法") {
        CHECK(
            lex_dump(U"1.f()") == "LITERAL_INT(1) SIGN_DOT IDENTIFIER(f) SIGN_LPAREN SIGN_RPAREN"
        );
    }

    TEST_CASE("`1.0.` 同理：decimal 建完之后剩下的点独立处理") {
        CHECK(
            lex_dump(U"1.0.f()") ==
            "LITERAL_DECIMAL(1.0) SIGN_DOT IDENTIFIER(f) SIGN_LPAREN SIGN_RPAREN"
        );
    }

    TEST_CASE("整数部分前导零不合法，跟 int 情形一致；小数部分没有这个限制") {
        CHECK_THROWS_AS(lex(U"007.5"), SyntaxError);
        CHECK_THROWS_AS(lex(U"00.5"), SyntaxError);
        CHECK(lex_dump(U"0.05") == "LITERAL_DECIMAL(0.05)"); // 小数部分的零不受限制
    }

    TEST_CASE("浮点数后紧跟字母非法，跟 int 情形一致") {
        CHECK_THROWS_AS(lex(U"1.5f"), SyntaxError);
        CHECK_THROWS_AS(lex(U"1.5abc"), SyntaxError);
    }

    TEST_CASE("浮点数后紧跟下划线非法") { CHECK_THROWS_AS(lex(U"1.5_"), SyntaxError); }

    TEST_CASE("带科学计数法后缀仍然是 decimal（细则见 scientific_notation_test.cpp）") {
        CHECK(lex_dump(U"1.5e-3") == "LITERAL_DECIMAL(1.5e-3)");
        CHECK(lex_dump(U"1.0e10") == "LITERAL_DECIMAL(1.0e10)");
    }

    TEST_CASE(
        "小数点只认一次：多个点会被拆成 decimal 加上后续的点号 token（不是一次性吞成畸形数字）"
    ) {
        // "1.2.3" -> 数字侧读出 1.2（第一个点后是数字 2，吞），
        // 第二个点后面虽然也是数字 3，但数字侧已经在读完 "1.2" 时退出了，不会回头再吞一次；
        // 剩下的单个点交给符号侧，独立处理成 SIGN_DOT
        CHECK(lex_dump(U"1.2.3") == "LITERAL_DECIMAL(1.2) SIGN_DOT LITERAL_INT(3)");
    }
}
