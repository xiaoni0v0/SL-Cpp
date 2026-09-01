// int 字面量。科学计数法见 scientific_notation_test.cpp。
#include "../../../builtins/exceptions/SyntaxError.h"
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("int") {

    TEST_CASE("基本形状，原文原样存") {
        CHECK(lex_dump(U"0") == "LITERAL_INT(0)");
        CHECK(lex_dump(U"9") == "LITERAL_INT(9)");
        CHECK(lex_dump(U"123") == "LITERAL_INT(123)");
        CHECK(
            lex_dump(U"123456789012345678901234567890") ==
            "LITERAL_INT(123456789012345678901234567890)"
        );
    }

    TEST_CASE("前导零非法，单独一个 0 除外") {
        CHECK_THROWS_AS(lex(U"007"), SyntaxError);
        CHECK_THROWS_AS(lex(U"00"), SyntaxError);
        CHECK_THROWS_AS(lex(U"0123"), SyntaxError);
        CHECK(lex_dump(U"0") == "LITERAL_INT(0)");
    }

    TEST_CASE("数字后紧跟字母或下划线非法") {
        CHECK_THROWS_AS(lex(U"123abc"), SyntaxError);
        CHECK_THROWS_AS(lex(U"1x"), SyntaxError);
        CHECK_THROWS_AS(lex(U"123_"), SyntaxError);
        CHECK_THROWS_AS(lex(U"1_000_000"), SyntaxError);
    }

    TEST_CASE("负号是独立 token，不是字面量的一部分") {
        CHECK(lex_dump(U"-1") == "SIGN_MINUS LITERAL_INT(1)");
        CHECK(lex_dump(U"- 1") == "SIGN_MINUS LITERAL_INT(1)");
        CHECK(lex_dump(U"+1") == "SIGN_PLUS LITERAL_INT(1)");
    }

    TEST_CASE("与符号相邻时正确截断") {
        CHECK(lex_dump(U"1+2") == "LITERAL_INT(1) SIGN_PLUS LITERAL_INT(2)");
        CHECK(lex_dump(U"1,2") == "LITERAL_INT(1) SIGN_COMMA LITERAL_INT(2)");
        CHECK(lex_dump(U"(1)") == "SIGN_LPAREN LITERAL_INT(1) SIGN_RPAREN");
        CHECK(lex_dump(U"1;2") == "LITERAL_INT(1) SIGN_SEMICOLON LITERAL_INT(2)");
    }
}
