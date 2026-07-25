// SL.md 2.1.4 字面量——float：`123.45`，暂不支持科学计数法；
// 整数、小数部分都不能省略，`1.`、`.1` 不是合法的 float 字面量。
//
// 点号和数字更复杂的交互（`1.0..1.2` 这类）放在 2_1_5_operators/dot_disambiguation_test.cpp，
// 这里只测 float 字面量本身的基本形状。
#include "../test_utils.h"
#include "../../../builtins/exceptions/SyntaxError.h"

#include <doctest/doctest.h>

TEST_SUITE("2.1.4 float") {

TEST_CASE("基本浮点数") {
    CHECK(lex_dump(U"123.45") == "LITERAL_FLOAT(123.45)");
    CHECK(lex_dump(U"0.0") == "LITERAL_FLOAT(0.0)");
    CHECK(lex_dump(U"1.0") == "LITERAL_FLOAT(1.0)");
}

TEST_CASE("`1.`：小数部分省略，不是合法 float，词法为 INT 后跟 DOT") {
    CHECK(lex_dump(U"1.") == "LITERAL_INT(1) SIGN_DOT");
}

TEST_CASE("`.1`：整数部分省略，不是合法 float 的开头，词法为 DOT 后跟 INT") {
    CHECK(lex_dump(U".1") == "SIGN_DOT LITERAL_INT(1)");
}

TEST_CASE("`1.` 紧跟标识符：典型场景是对 int 字面量取属性/调方法") {
    CHECK(lex_dump(U"1.f()") == "LITERAL_INT(1) SIGN_DOT IDENTIFIER(f) SIGN_LPAREN SIGN_RPAREN");
}

TEST_CASE("`1.0.` 同理：float 建完之后剩下的点独立处理") {
    CHECK(lex_dump(U"1.0.f()") == "LITERAL_FLOAT(1.0) SIGN_DOT IDENTIFIER(f) SIGN_LPAREN SIGN_RPAREN");
}

TEST_CASE("浮点数后紧跟字母非法，跟 int 情形一致") {
    CHECK_THROWS_AS(lex(U"1.5f"), SyntaxError);
    CHECK_THROWS_AS(lex(U"1.5abc"), SyntaxError);
}

TEST_CASE("浮点数后紧跟下划线非法") {
    CHECK_THROWS_AS(lex(U"1.5_"), SyntaxError);
}

TEST_CASE("科学计数法暂不支持：e 会被当成后面紧跟的标识符开头，从而触发数字后紧跟字母的错误") {
    CHECK_THROWS_AS(lex(U"1e10"), SyntaxError);
    CHECK_THROWS_AS(lex(U"1.5e-3"), SyntaxError);
}

TEST_CASE("小数点只认一次：多个点会被拆成 float 加上后续的点号 token（不是一次性吞成畸形数字）") {
    // "1.2.3" -> 数字侧读出 1.2（第一个点后是数字 2，吞），
    // 第二个点后面虽然也是数字 3，但数字侧已经在读完 "1.2" 时退出了，不会回头再吞一次；
    // 剩下的单个点交给符号侧，独立处理成 SIGN_DOT
    CHECK(lex_dump(U"1.2.3") == "LITERAL_FLOAT(1.2) SIGN_DOT LITERAL_INT(3)");
}

}
