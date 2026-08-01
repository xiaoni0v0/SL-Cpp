// 跨分组：SyntaxError 本身的行为——行列信息是否正确带出来、异常类型/继承关系是否符合预期。
// 各类具体的报错触发条件已经分散在各自分组的测试文件里，这里只关心“报错这件事本身做得对不对”。
#include "../../../builtins/exceptions/SLException.h"
#include "../../../builtins/exceptions/SyntaxError.h"
#include "../test_utils.h"

#include <doctest/doctest.h>
#include <stdexcept>

TEST_SUITE("SyntaxError 本身的行为") {

    TEST_CASE("SyntaxError 是 SLException 的子类，也是 std::exception 的子类（可以被泛化捕获）") {
        CHECK_THROWS_AS(lex(U"`unterminated"), SLException);
        CHECK_THROWS_AS(lex(U"`unterminated"), std::exception);
    }

    TEST_CASE("异常消息里带有文件名、行号、列号") {
        try {
            const auto tokens{Lexer{U"1\nlocal", "my_file.sl"}.tokenize()};
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("my_file.sl") != std::string::npos);
            CHECK(msg.find("2:1") != std::string::npos); // local 在第 2 行第 1 列
            CHECK(msg.find("SyntaxError") != std::string::npos);
        }
    }

    TEST_CASE("默认文件名是 <unknown>（没传 file_path 时）") {
        try {
            lex(U"local");
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("<unknown>") != std::string::npos);
        }
    }

    TEST_CASE("一旦遇到第一个错误就立即停止，不会继续扫描后面的内容") {
        // "local" 在最前面就直接报错，不会管后面写的是什么
        CHECK_THROWS_AS(lex(U"local garbage garbage garbage"), SyntaxError);
    }

    TEST_CASE("汇总：各类会触发 SyntaxError 的场景（分散测试过的，这里过一遍总览，确认互不干扰）") {
        CHECK_THROWS_AS(lex(U"1abc"), SyntaxError);            // 数字后跟字母
        CHECK_THROWS_AS(lex(U"\"unterminated"), SyntaxError);  // 未闭合普通字符串
        CHECK_THROWS_AS(lex(U"`unterminated"), SyntaxError);   // 未闭合原始字符串
        CHECK_THROWS_AS(lex(U"/* unterminated"), SyntaxError); // 未闭合块注释
        CHECK_THROWS_AS(lex(U"\"\\q\""), SyntaxError);         // 未知转义
        CHECK_THROWS_AS(lex(U"when"), SyntaxError);            // 保留字
        const std::u32string bad_char{static_cast<char32_t>(0x20AC)};
        CHECK_THROWS_AS(lex(bad_char), SyntaxError); // 无法识别的字符
    }

    TEST_CASE("正常输入不应该抛任何异常（反面对照）") {
        CHECK_NOTHROW(lex(U"func f(x: int = 1) { return x + 1 }"));
        CHECK_NOTHROW(lex(U"class C(Base) { x = `raw\nstring` }"));
    }
}
