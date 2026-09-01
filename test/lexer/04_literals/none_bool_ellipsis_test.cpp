// None / True / False / _G / _L / Ellipsis 在表达式里的切分。
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("None / bool / _G / _L / Ellipsis") {

    TEST_CASE("与运算符、赋值混用") {
        CHECK(lex_dump(U"x = None") == "IDENTIFIER(x) SIGN_ASSIGN LITERAL_NONE");
        CHECK(lex_dump(U"True and False") == "LITERAL_TRUE KW_AND LITERAL_FALSE");
        CHECK(lex_dump(U"x = ...") == "IDENTIFIER(x) SIGN_ASSIGN LITERAL_ELLIPSIS");
    }

    TEST_CASE("_G / _L 后面可以接下标或属性") {
        CHECK(lex_dump(U"_L['x']") == "LITERAL_L SIGN_LBRACKET LITERAL_STR(x) SIGN_RBRACKET");
        CHECK(lex_dump(U"_G.y") == "LITERAL_G SIGN_DOT IDENTIFIER(y)");
    }

    TEST_CASE("Ellipsis") {
        CHECK(lex_dump(U"...") == "LITERAL_ELLIPSIS");
        CHECK(
            lex_dump(U"(1, ..., 2)") == "SIGN_LPAREN LITERAL_INT(1) SIGN_COMMA LITERAL_ELLIPSIS "
                                        "SIGN_COMMA LITERAL_INT(2) SIGN_RPAREN"
        );
    }
}
