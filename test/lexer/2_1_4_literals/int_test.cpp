// SL.md 2.1.4 字面量——int：`123`，暂不支持二进制/八进制/十六进制
#include "../test_utils.h"
#include "../../../builtins/classes/exceptions/SyntaxError.h"

#include <doctest/doctest.h>

TEST_SUITE("2.1.4 int") {

TEST_CASE("基本整数") {
    CHECK(lex_dump(U"0") == "LITERAL_INT(0)");
    CHECK(lex_dump(U"9") == "LITERAL_INT(9)");
    CHECK(lex_dump(U"123") == "LITERAL_INT(123)");
}

TEST_CASE("词法层面存的是原文字符串，不做数值转换（大整数也原样存，不溢出）") {
    CHECK(lex_dump(U"123456789012345678901234567890") == "LITERAL_INT(123456789012345678901234567890)");
}

TEST_CASE("前导零：词法层面不特殊处理，原样当普通数字字符串") {
    CHECK(lex_dump(U"007") == "LITERAL_INT(007)");
    CHECK(lex_dump(U"00") == "LITERAL_INT(00)");
}

TEST_CASE("数字后紧跟字母是非法的") {
    CHECK_THROWS_AS(lex(U"123abc"), SyntaxError);
    CHECK_THROWS_AS(lex(U"1x"), SyntaxError);
}

TEST_CASE("数字后紧跟下划线是非法的（下划线分隔以后才支持，现在还没加）") {
    CHECK_THROWS_AS(lex(U"123_"), SyntaxError);
    CHECK_THROWS_AS(lex(U"1_000_000"), SyntaxError);
}

TEST_CASE("负数不是字面量：一元负号和整数是分开的两个 token") {
    CHECK(lex_dump(U"-1") == "SIGN_MINUS LITERAL_INT(1)");
    CHECK(lex_dump(U"- 1") == "SIGN_MINUS LITERAL_INT(1)");
}

TEST_CASE("整数在各种符号之间正确截断") {
    CHECK(lex_dump(U"1+2") == "LITERAL_INT(1) SIGN_PLUS LITERAL_INT(2)");
    CHECK(lex_dump(U"1,2") == "LITERAL_INT(1) SIGN_COMMA LITERAL_INT(2)");
    CHECK(lex_dump(U"(1)") == "SIGN_LPAREN LITERAL_INT(1) SIGN_RPAREN");
    CHECK(lex_dump(U"1;2") == "LITERAL_INT(1) SIGN_SEMICOLON LITERAL_INT(2)");
}

}
