// SL.md 2.1.3 标识符：正则 [a-zA-Z_][a-zA-Z0-9_]*，且不能是关键字/保留字
#include "../../../builtins/exceptions/SyntaxError.h"
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("2.1.3 标识符") {

    TEST_CASE("基本标识符") {
        CHECK(lex_dump(U"x") == "IDENTIFIER(x)");
        CHECK(lex_dump(U"foo") == "IDENTIFIER(foo)");
        CHECK(lex_dump(U"fooBar123") == "IDENTIFIER(fooBar123)");
    }

    TEST_CASE("单个/多个下划线开头") {
        CHECK(lex_dump(U"_") == "IDENTIFIER(_)");
        CHECK(lex_dump(U"__") == "IDENTIFIER(__)");
        CHECK(lex_dump(U"_foo") == "IDENTIFIER(_foo)");
        CHECK(lex_dump(U"__foo__") == "IDENTIFIER(__foo__)");
    }

    TEST_CASE("首字符不能是数字，但数字可以出现在后面") {
        CHECK(lex_dump(U"a1") == "IDENTIFIER(a1)");
        CHECK(lex_dump(U"a1b2c3") == "IDENTIFIER(a1b2c3)");
        // 数字开头根本不会走到标识符这条路径，会被当成数字字面量处理（覆盖在 int_test 里）
    }

    TEST_CASE("大小写敏感：同名不同大小写是不同标识符（只看能不能正确分别识别成独立 token）") {
        CHECK(lex_dump(U"foo Foo FOO") == "IDENTIFIER(foo) IDENTIFIER(Foo) IDENTIFIER(FOO)");
    }

    TEST_CASE("标识符遇到非法字符会正确截断") {
        CHECK(lex_dump(U"foo+bar") == "IDENTIFIER(foo) SIGN_PLUS IDENTIFIER(bar)");
        CHECK(lex_dump(U"foo.bar") == "IDENTIFIER(foo) SIGN_DOT IDENTIFIER(bar)");
        CHECK(lex_dump(U"foo(bar)") == "IDENTIFIER(foo) SIGN_LPAREN IDENTIFIER(bar) SIGN_RPAREN");
    }

    TEST_CASE("非 ASCII 字母不属于标识符字符集，会报未知字符错误") {
        // 2.1.3 的正则明确限定 [a-zA-Z_][a-zA-Z0-9_]*，不含 Unicode 字母。
        // 用 char32_t 数值直接构造非 ASCII
        // 字符，不依赖源文件/编译器对字面量字符集的解释，跨平台更稳妥。
        const std::u32string e_acute{static_cast<char32_t>(0x00E9)}; // é
        CHECK_THROWS_AS(lex(e_acute), SyntaxError);
        const std::u32string chinese_word{static_cast<char32_t>(0x53D8),
                                          static_cast<char32_t>(0x91CF)}; // 变量
        CHECK_THROWS_AS(lex(chinese_word), SyntaxError);
    }
}
