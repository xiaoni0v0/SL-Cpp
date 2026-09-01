// 普通字符串："..." / '...'。支持转义，不支持多行。
#include "../../../builtins/exceptions/SyntaxError.h"
#include "../test_utils.h"

#include <doctest/doctest.h>
#include <string>

TEST_SUITE("普通字符串") {

    TEST_CASE("双引号与单引号，lexeme 是值不是原文") {
        CHECK(lex_dump(U"\"hello\"") == "LITERAL_STR(hello)");
        CHECK(lex_dump(U"'hello'") == "LITERAL_STR(hello)");
        CHECK(lex_dump(U"\"\"") == "LITERAL_STR()");
        CHECK(lex_dump(U"''") == "LITERAL_STR()");
        const auto tokens{lex(U"\"hello\"")};
        REQUIRE(tokens.size() == 2);
        CHECK(tokens[0].lexeme == U"hello");
    }

    TEST_CASE("另一种引号可以直接出现") {
        CHECK(lex_dump(U"\"it's\"") == "LITERAL_STR(it's)");
        CHECK(lex_dump(U"'she said \"hi\"'") == "LITERAL_STR(she said \"hi\")");
    }

    TEST_CASE("11 种转义") {
        struct Case {
            std::u32string src;
            char32_t expected;
        };
        const Case cases[]{
            {U"'\\a'", U'\a'},
            {U"'\\b'", U'\b'},
            {U"'\\f'", U'\f'},
            {U"'\\n'", U'\n'},
            {U"'\\r'", U'\r'},
            {U"'\\t'", U'\t'},
            {U"'\\v'", U'\v'},
            {U"'\\0'", U'\0'},
            {U"'\\\\'", U'\\'},
            {U"'\\''", U'\''},
            {U"'\\\"'", U'\"'},
        };
        for (const auto &[src, expected] : cases) {
            const auto tokens{lex(src)};
            REQUIRE(tokens.size() == 2);
            CHECK(tokens[0].type == TokenType::LITERAL_STR);
            REQUIRE(tokens[0].lexeme.size() == 1);
            CHECK(tokens[0].lexeme[0] == expected);
        }
    }

    TEST_CASE("转义可与普通字符混用") {
        const auto tokens{lex(U"\"a\\tb\\nc\"")};
        REQUIRE(tokens.size() == 2);
        CHECK(tokens[0].lexeme == U"a\tb\nc");
    }

    TEST_CASE("未知转义报错，列指向反斜杠") {
        CHECK_THROWS_AS(lex(U"\"\\q\""), SyntaxError);
        CHECK_THROWS_AS(lex(U"'\\x'"), SyntaxError);
        try {
            lex(U"\"\\q\"");
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            CHECK(std::string{e.what()}.find(":1:2:") != std::string::npos);
        }
    }

    TEST_CASE("不支持多行；未闭合报错") {
        const std::u32string src{U'"', U'a', U'b', U'c', U'\n', U'd', U'e', U'f', U'"'};
        CHECK_THROWS_AS(lex(src), SyntaxError);
        CHECK_THROWS_AS(lex(U"\"abc"), SyntaxError);
        CHECK_THROWS_AS(lex(U"'abc"), SyntaxError);
    }

    TEST_CASE("反斜杠后 EOF 或换行都是未闭合转义") {
        try {
            lex(U"\"abc\\");
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("unterminated") != std::string::npos);
            CHECK(msg.find("escape") != std::string::npos);
            CHECK(msg.find(":1:5:") != std::string::npos);
        }
        try {
            lex(U"\"abc\\\ndef\"");
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("unterminated") != std::string::npos);
            CHECK(msg.find("escape") != std::string::npos);
        }
    }

    TEST_CASE("与其它 token 组合") {
        CHECK(lex_dump(U"f(\"x\")") == "IDENTIFIER(f) SIGN_LPAREN LITERAL_STR(x) SIGN_RPAREN");
        CHECK(lex_dump(U"\"a\" + \"b\"") == "LITERAL_STR(a) SIGN_PLUS LITERAL_STR(b)");
    }

    TEST_CASE("字面 \\r 是字符串内容，不是行终止") {
        const auto tokens{lex(U"\"a\rb\"")};
        REQUIRE(tokens.size() == 2);
        CHECK(tokens[0].lexeme == U"a\rb");
        const auto escaped{lex(U"'a\\rb'")};
        REQUIRE(escaped.size() == 2);
        CHECK(escaped[0].lexeme.size() == 3);
        CHECK(escaped[0].lexeme[1] == U'\r');
    }
}
