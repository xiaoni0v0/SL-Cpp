// decimal 字面量：整数和小数部分都不能省。点号与 range 的交错见 05_operators。
#include "../../../cppexceptions/SyntaxError.h"
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("decimal") {

    TEST_CASE("基本形状") {
        CHECK(lex_dump(U"123.45") == "LITERAL_DECIMAL(123.45)");
        CHECK(lex_dump(U"0.0") == "LITERAL_DECIMAL(0.0)");
        CHECK(lex_dump(U"1.0") == "LITERAL_DECIMAL(1.0)");
        CHECK(lex_dump(U"0.05") == "LITERAL_DECIMAL(0.05)");
    }

    TEST_CASE("`1.` / `.1` 不是 decimal") {
        CHECK(lex_dump(U"1.") == "LITERAL_INT(1) SIGN_DOT");
        CHECK(lex_dump(U".1") == "SIGN_DOT LITERAL_INT(1)");
        CHECK(
            lex_dump(U"1.f()") == "LITERAL_INT(1) SIGN_DOT IDENTIFIER(f) SIGN_LPAREN SIGN_RPAREN"
        );
        CHECK(
            lex_dump(U"1.0.f()") ==
            "LITERAL_DECIMAL(1.0) SIGN_DOT IDENTIFIER(f) SIGN_LPAREN SIGN_RPAREN"
        );
    }

    TEST_CASE("整数部分不许前导零；小数部分可以") {
        CHECK_THROWS_AS(lex(U"007.5"), SyntaxError);
        CHECK_THROWS_AS(lex(U"00.5"), SyntaxError);
        CHECK(lex_dump(U"0.05") == "LITERAL_DECIMAL(0.05)");
    }

    TEST_CASE("后面紧跟字母或下划线非法") {
        CHECK_THROWS_AS(lex(U"1.5f"), SyntaxError);
        CHECK_THROWS_AS(lex(U"1.5abc"), SyntaxError);
        CHECK_THROWS_AS(lex(U"1.5_"), SyntaxError);
    }

    TEST_CASE("多个点：只认第一次紧跟数字的点") {
        CHECK(lex_dump(U"1.2.3") == "LITERAL_DECIMAL(1.2) SIGN_DOT LITERAL_INT(3)");
    }
}
