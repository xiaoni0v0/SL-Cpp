// SL.md 2.1.4 字面量——None / bool / _G / _L / Ellipsis
// （None/True/False/_G/_L 机制上走的是关键字表，2.1.2 keywords_test 已经覆盖过基本识别，
//  这里只补 2.1.4 语境下、和其他字面量搭配使用时的场景）
#include "../test_utils.h"
#include <doctest/doctest.h>

TEST_SUITE("2.1.4 None/bool/_G/_L/Ellipsis") {

TEST_CASE("None/True/False 在表达式里混用") {
    CHECK(lex_dump(U"x = None") == "IDENTIFIER(x) SIGN_ASSIGN LITERAL_NONE");
    CHECK(lex_dump(U"True and False") == "LITERAL_TRUE KW_AND LITERAL_FALSE");
}

TEST_CASE("_G / _L 作为字面量可以直接下标、取属性") {
    CHECK(lex_dump(U"_L['x']") == "LITERAL_L SIGN_LBRACKET LITERAL_STR(x) SIGN_RBRACKET");
    CHECK(lex_dump(U"_G.y") == "LITERAL_G SIGN_DOT IDENTIFIER(y)");
}

TEST_CASE("Ellipsis 字面量") {
    CHECK(lex_dump(U"...") == "LITERAL_ELLIPSIS");
    CHECK(lex_dump(U"x = ...") == "IDENTIFIER(x) SIGN_ASSIGN LITERAL_ELLIPSIS");
}

TEST_CASE("Ellipsis 作为元组/列表元素") {
    CHECK(lex_dump(U"(1, ..., 2)") ==
        "SIGN_LPAREN LITERAL_INT(1) SIGN_COMMA LITERAL_ELLIPSIS SIGN_COMMA LITERAL_INT(2) SIGN_RPAREN");
}

}
