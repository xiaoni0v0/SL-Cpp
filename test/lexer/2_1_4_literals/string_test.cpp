// SL.md 2.1.4 字面量——str（"..." 和 '...'）：支持转义，不支持多行
#include "../test_utils.h"
#include "../../../builtins/classes/exceptions/SyntaxError.h"

#include <doctest/doctest.h>

TEST_SUITE("2.1.4 str（双引号/单引号）") {

TEST_CASE("基本字符串：双引号与单引号") {
    CHECK(lex_dump(U"\"hello\"") == "LITERAL_STR(hello)");
    CHECK(lex_dump(U"'hello'") == "LITERAL_STR(hello)");
}

TEST_CASE("空字符串") {
    CHECK(lex_dump(U"\"\"") == "LITERAL_STR()");
    CHECK(lex_dump(U"''") == "LITERAL_STR()");
}

TEST_CASE("词法层面存的是值（去掉引号），不是原文") {
    // "hello" 的 lexeme 应该是 hello 五个字符，不含引号
    const auto tokens{lex(U"\"hello\"")};
    REQUIRE(tokens.size() == 2); // str, EOF
    CHECK(tokens[0].lexeme == U"hello");
}

TEST_CASE("双引号字符串内可以直接出现单引号，反之亦然，不需要转义") {
    CHECK(lex_dump(U"\"it's\"") == "LITERAL_STR(it's)");
    CHECK(lex_dump(U"'she said \"hi\"'") == "LITERAL_STR(she said \"hi\")");
}

TEST_CASE("全部 11 种转义逐个验证，转成对应的实际字符") {
    struct Case {
        std::u32string src;
        char32_t expected;
    };
    const Case cases[]{
        {U"'\\a'", U'\a'}, {U"'\\b'", U'\b'}, {U"'\\f'", U'\f'},
        {U"'\\n'", U'\n'}, {U"'\\r'", U'\r'}, {U"'\\t'", U'\t'},
        {U"'\\v'", U'\v'}, {U"'\\0'", U'\0'}, {U"'\\\\'", U'\\'},
        {U"'\\''", U'\''}, {U"'\\\"'", U'"'},
    };
    for (const auto &[src, expected] : cases) {
        const auto tokens{lex(src)};
        REQUIRE(tokens.size() == 2);
        CHECK(tokens[0].type == TokenType::LITERAL_STR);
        REQUIRE(tokens[0].lexeme.size() == 1);
        CHECK(tokens[0].lexeme[0] == expected);
    }
}

TEST_CASE("转义序列可以和普通字符混在一起") {
    const auto tokens{lex(U"\"a\\tb\\nc\"")};
    REQUIRE(tokens.size() == 2);
    CHECK(tokens[0].lexeme == U"a\tb\nc");
}

TEST_CASE("未知转义序列报 SyntaxError") {
    CHECK_THROWS_AS(lex(U"\"\\q\""), SyntaxError);
    CHECK_THROWS_AS(lex(U"'\\x'"), SyntaxError);
}

TEST_CASE("不支持多行：字符串中间出现字面换行直接报未闭合错误") {
    const std::u32string src{U'"', U'a', U'b', U'c', U'\n', U'd', U'e', U'f', U'"'};
    CHECK_THROWS_AS(lex(src), SyntaxError);
}

TEST_CASE("未闭合字符串（到 EOF 都没有匹配的引号）报 SyntaxError") {
    CHECK_THROWS_AS(lex(U"\"abc"), SyntaxError);
    CHECK_THROWS_AS(lex(U"'abc"), SyntaxError);
}

TEST_CASE("反斜杠在字符串结尾、后面直接 EOF 报错，消息说明是转义序列未完整结束，且指向反斜杠自己的列") {
    try {
        lex(U"\"abc\\"); // "=col1 a=2 b=3 c=4 \=5，反斜杠在第 5 列
        FAIL("应当抛出异常");
    } catch (const SyntaxError &e) {
        const std::string msg{e.what()};
        CHECK(msg.find("unterminated") != std::string::npos);
        CHECK(msg.find("escape") != std::string::npos);
        CHECK(msg.find(":1:5:") != std::string::npos);
    }
}

TEST_CASE("未知转义序列报错的列指向反斜杠自己，不是后面那个字符") {
    try {
        lex(U"\"\\q\""); // "=col1 \=2 q=3，反斜杠在第 2 列
        FAIL("应当抛出异常");
    } catch (const SyntaxError &e) {
        const std::string msg{e.what()};
        CHECK(msg.find(":1:2:") != std::string::npos);
    }
}

TEST_CASE("反斜杠后紧跟换行也报错（不允许用反斜杠续行），同一条消息") {
    try {
        lex(U"\"abc\\\ndef\"");
        FAIL("应当抛出异常");
    } catch (const SyntaxError &e) {
        const std::string msg{e.what()};
        CHECK(msg.find("unterminated") != std::string::npos);
        CHECK(msg.find("escape") != std::string::npos);
    }
}

TEST_CASE("字符串前后可以正常和其他 token 组合") {
    CHECK(lex_dump(U"f(\"x\")") == "IDENTIFIER(f) SIGN_LPAREN LITERAL_STR(x) SIGN_RPAREN");
    CHECK(lex_dump(U"\"a\" + \"b\"") == "LITERAL_STR(a) SIGN_PLUS LITERAL_STR(b)");
}

}
