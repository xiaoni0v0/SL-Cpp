// 普通字符串："..." / '...'。支持转义，不支持多行。
#include "../../../diagnostics/SyntaxError.h"
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

    TEST_CASE("转义自己的闭合引号：双引号串里 \\\" 也认，不是只有单引号串里的 \\' 被测过") {
        // 上面 11 种转义那组两个例子都是拿单引号串测的（'\''、'\"'），双引号串转义自己的
        // 闭合引号从没单独测过——两种引号共用同一张转义表，这里补上双引号那一侧
        CHECK(lex_dump(U"\"\\\"\"") == "LITERAL_STR(\")");
        const auto tokens{lex(U"\"\\\"\"")};
        REQUIRE(tokens.size() == 2);
        CHECK(tokens[0].lexeme == U"\"");
    }

    TEST_CASE("转义可与普通字符混用") {
        const auto tokens{lex(U"\"a\\tb\\nc\"")};
        REQUIRE(tokens.size() == 2);
        CHECK(tokens[0].lexeme == U"a\tb\nc");
    }

    TEST_CASE("\\0 只是单个 NUL 字符，不是 C 系的八进制转义前缀") {
        // "\01" 应该是 NUL + 字符 '1'（两个码点），不是当成 \001 之类的八进制值去吃后续数字
        const auto tokens{lex(U"\"\\01\"")};
        REQUIRE(tokens.size() == 2);
        REQUIRE(tokens[0].lexeme.size() == 2);
        CHECK(tokens[0].lexeme[0] == U'\0');
        CHECK(tokens[0].lexeme[1] == U'1');
        // "\1" 没有这个转义，未知转义照样报错，不会被误当成八进制/十进制转义收掉
        CHECK_THROWS_AS(lex(U"\"\\1\""), SyntaxError);
    }

    TEST_CASE("相邻字符串字面量不会自动拼接（SL 没有这条规则）") {
        CHECK(lex_dump(U"\"a\" \"b\"") == "LITERAL_STR(a) LITERAL_STR(b)");
        const auto tokens{lex(U"\"a\"\"b\"")};
        REQUIRE(tokens.size() == 3);
        CHECK(tokens[0].type == TokenType::LITERAL_STR);
        CHECK(tokens[0].lexeme == U"a");
        CHECK(tokens[1].type == TokenType::LITERAL_STR);
        CHECK(tokens[1].lexeme == U"b");
    }

    TEST_CASE("保留字/关键字拼出的文本出现在字符串内容里，只是普通文本，不报保留字错误") {
        CHECK(lex_dump(U"\"local assert yield\"") == "LITERAL_STR(local assert yield)");
        const auto tokens{lex(U"\"local\"")};
        REQUIRE(tokens.size() == 2);
        CHECK(tokens[0].type == TokenType::LITERAL_STR);
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

    // CRLF 源码里，行尾反斜杠后紧跟的是 '\r' 不是 '\n'：不命中"换行"这条判断，于是转义继续往下走，
    // 把 '\r' 本身当成待查表的转义字符——查不到，报"unknown escape"而不是上面那条"unterminated
    // escape"。这跟 LF 源码里同样的写法走的是两条不同分支，报错文案不一样；这里只钉住当前确实会
    // 抛出的错误类型，不去评判这条诊断信息合不合理
    TEST_CASE("CRLF 源码里反斜杠后紧跟 \\r\\n：跟 LF 走的是不同分支，仍然抛错但文案不同") {
        try {
            lex(U"\"abc\\\r\ndef\"");
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("unknown escape") != std::string::npos);
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
