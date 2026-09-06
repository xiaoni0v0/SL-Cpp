// 标识符：[a-zA-Z_][a-zA-Z0-9_]*，不含 Unicode 字母。
#include "../../../cppexceptions/SyntaxError.h"
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("标识符") {

    TEST_CASE("基本形状") {
        CHECK(lex_dump(U"x") == "IDENTIFIER(x)");
        CHECK(lex_dump(U"foo") == "IDENTIFIER(foo)");
        CHECK(lex_dump(U"fooBar123") == "IDENTIFIER(fooBar123)");
        CHECK(lex_dump(U"a1") == "IDENTIFIER(a1)");
        CHECK(lex_dump(U"a1b2c3") == "IDENTIFIER(a1b2c3)");
    }

    TEST_CASE("下划线开头") {
        CHECK(lex_dump(U"_") == "IDENTIFIER(_)");
        CHECK(lex_dump(U"__") == "IDENTIFIER(__)");
        CHECK(lex_dump(U"_foo") == "IDENTIFIER(_foo)");
        CHECK(lex_dump(U"__foo__") == "IDENTIFIER(__foo__)");
    }

    TEST_CASE("大小写敏感") {
        CHECK(lex_dump(U"foo Foo FOO") == "IDENTIFIER(foo) IDENTIFIER(Foo) IDENTIFIER(FOO)");
    }

    TEST_CASE("遇到非法字符就截断") {
        CHECK(lex_dump(U"foo+bar") == "IDENTIFIER(foo) SIGN_PLUS IDENTIFIER(bar)");
        CHECK(lex_dump(U"foo.bar") == "IDENTIFIER(foo) SIGN_DOT IDENTIFIER(bar)");
        CHECK(lex_dump(U"foo(bar)") == "IDENTIFIER(foo) SIGN_LPAREN IDENTIFIER(bar) SIGN_RPAREN");
    }

    TEST_CASE("非 ASCII 字母不是标识符字符") {
        const std::u32string e_acute{static_cast<char32_t>(0x00E9)};
        CHECK_THROWS_AS(lex(e_acute), SyntaxError);
        const std::u32string chinese{static_cast<char32_t>(0x53D8), static_cast<char32_t>(0x91CF)};
        CHECK_THROWS_AS(lex(chinese), SyntaxError);
        const std::u32string foo_acute{U'f', U'o', U'o', static_cast<char32_t>(0x00E9)};
        CHECK_THROWS_AS(lex(foo_acute), SyntaxError);
    }
}
