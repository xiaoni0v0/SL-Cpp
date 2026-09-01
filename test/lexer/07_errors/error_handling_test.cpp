// SyntaxError 的类型、消息格式、遇错即停。具体触发条件分散在各主题文件。
#include "../../../builtins/exceptions/SLException.h"
#include "../../../builtins/exceptions/SyntaxError.h"
#include "../test_utils.h"

#include <doctest/doctest.h>
#include <stdexcept>
#include <string>

TEST_SUITE("词法 SyntaxError") {

    TEST_CASE("继承 SLException / std::exception") {
        CHECK_THROWS_AS(lex(U"`unterminated"), SLException);
        CHECK_THROWS_AS(lex(U"`unterminated"), std::exception);
    }

    TEST_CASE("消息含文件名、行、列") {
        try {
            (void) Lexer{U"1\nlocal", "my_file.sl"}.tokenize();
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("my_file.sl") != std::string::npos);
            CHECK(msg.find("2:1") != std::string::npos);
            CHECK(msg.find("SyntaxError") != std::string::npos);
        }
    }

    TEST_CASE("默认文件名是 <unknown>") {
        try {
            lex(U"local");
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            CHECK(std::string{e.what()}.find("<unknown>") != std::string::npos);
        }
    }

    TEST_CASE("遇第一个错误立即停") {
        CHECK_THROWS_AS(lex(U"local garbage garbage garbage"), SyntaxError);
    }

    TEST_CASE("各类触发条件互不干扰") {
        CHECK_THROWS_AS(lex(U"1abc"), SyntaxError);
        CHECK_THROWS_AS(lex(U"0x10"), SyntaxError);
        CHECK_THROWS_AS(lex(U"\"unterminated"), SyntaxError);
        CHECK_THROWS_AS(lex(U"`unterminated"), SyntaxError);
        CHECK_THROWS_AS(lex(U"/* unterminated"), SyntaxError);
        CHECK_THROWS_AS(lex(U"\"\\q\""), SyntaxError);
        CHECK_THROWS_AS(lex(U"when"), SyntaxError);
        CHECK_THROWS_AS(lex(U"1e"), SyntaxError);
        CHECK_THROWS_AS(lex(U"1e-9"), SyntaxError);
        CHECK_THROWS_AS(lex(U"1e10000"), SyntaxError);
        CHECK_THROWS_AS(lex(U"1e01"), SyntaxError);
        const std::u32string bad_char{static_cast<char32_t>(0x20AC)};
        CHECK_THROWS_AS(lex(bad_char), SyntaxError);
    }

    TEST_CASE("合法输入不抛") {
        CHECK_NOTHROW(lex(U"func f(x: int = 1) { return x + 1 }"));
        CHECK_NOTHROW(lex(U"class C(Base) { x = `raw\nstring` }"));
    }
}
