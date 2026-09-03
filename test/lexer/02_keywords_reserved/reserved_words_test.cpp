// 保留字出现即 SyntaxError；大小写变体和超集是标识符。
#include "../../../diagnostics/SyntaxError.h"
#include "../test_utils.h"

#include <doctest/doctest.h>
#include <string>

TEST_SUITE("保留字") {

    TEST_CASE("每个保留字单独出现都报错") {
        CHECK_THROWS_AS(lex(U"assert"), SyntaxError);
        CHECK_THROWS_AS(lex(U"when"), SyntaxError);
        CHECK_THROWS_AS(lex(U"case"), SyntaxError);
        CHECK_THROWS_AS(lex(U"yield"), SyntaxError);
        CHECK_THROWS_AS(lex(U"with"), SyntaxError);
        CHECK_THROWS_AS(lex(U"async"), SyntaxError);
        CHECK_THROWS_AS(lex(U"await"), SyntaxError);
        CHECK_THROWS_AS(lex(U"define"), SyntaxError);
        CHECK_THROWS_AS(lex(U"const"), SyntaxError);
        CHECK_THROWS_AS(lex(U"static"), SyntaxError);
        CHECK_THROWS_AS(lex(U"local"), SyntaxError);
    }

    TEST_CASE("表达式中间出现同样报错") {
        CHECK_THROWS_AS(lex(U"1 + local"), SyntaxError);
        CHECK_THROWS_AS(lex(U"func f() { when }"), SyntaxError);
    }

    TEST_CASE("match 是普通标识符") { CHECK(lex_dump(U"match") == "IDENTIFIER(match)"); }

    TEST_CASE("大小写变体是标识符") {
        CHECK(lex_dump(U"When") == "IDENTIFIER(When)");
        CHECK(lex_dump(U"LOCAL") == "IDENTIFIER(LOCAL)");
        CHECK(lex_dump(U"Assert") == "IDENTIFIER(Assert)");
    }

    TEST_CASE("前缀/超集是标识符") {
        CHECK(lex_dump(U"whenever") == "IDENTIFIER(whenever)");
        CHECK(lex_dump(U"locality") == "IDENTIFIER(locality)");
        CHECK(lex_dump(U"assertion") == "IDENTIFIER(assertion)");
    }

    TEST_CASE("报错位置指向保留字开头") {
        try {
            lex(U"1\n2 local");
            FAIL("应当抛出 SyntaxError");
        } catch (const SyntaxError &e) {
            CHECK(std::string{e.what()}.find("2:3") != std::string::npos);
        }
    }
}
