// 反引号原始字符串：不转义，可多行，内部不能出现反引号。
#include "../../../diagnostics/SyntaxError.h"
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("原始字符串") {

    TEST_CASE("产出仍是 LITERAL_STR") {
        CHECK(lex_dump(U"`hello`") == "LITERAL_STR(hello)");
        CHECK(lex_dump(U"``") == "LITERAL_STR()");
    }

    TEST_CASE("不处理转义") {
        const auto tokens{lex(U"`a\\nb`")};
        REQUIRE(tokens.size() == 2);
        CHECK(tokens[0].lexeme == U"a\\nb");
    }

    TEST_CASE("字面换行进入 lexeme，后续行号正确") {
        const auto tokens{lex(U"`line1\nline2`")};
        REQUIRE(tokens.size() == 2);
        CHECK(tokens[0].lexeme == U"line1\nline2");
        const auto after{lex(U"`a\nb\nc` 1")};
        REQUIRE(after.size() == 3);
        CHECK(after[0].row == 1);
        CHECK(after[1].row == 3);
    }

    TEST_CASE("内部可直接出现单双引号") {
        CHECK(lex_dump(U"`it's \"quoted\"`") == "LITERAL_STR(it's \"quoted\")");
    }

    TEST_CASE("读到下一个反引号就结束") {
        CHECK_THROWS_AS(lex(U"```"), SyntaxError);
        CHECK(lex_dump(U"\"`\" + `abc`") == "LITERAL_STR(`) SIGN_PLUS LITERAL_STR(abc)");
    }

    TEST_CASE("未闭合报错") { CHECK_THROWS_AS(lex(U"`abc"), SyntaxError); }

    TEST_CASE("可与普通字符串相邻") {
        CHECK(lex_dump(U"`raw` + \"normal\"") == "LITERAL_STR(raw) SIGN_PLUS LITERAL_STR(normal)");
    }

    TEST_CASE("\\r 原样保留") {
        const auto tokens{lex(U"`a\rb`")};
        REQUIRE(tokens.size() == 2);
        CHECK(tokens[0].lexeme == U"a\rb");
    }
}
