// SL.md 2.1.5 运算符——符号本身的识别、贪婪最长匹配。点号单独放在 dot_disambiguation_test.cpp。
#include "../test_utils.h"
#include "../../../builtins/exceptions/SyntaxError.h"

#include <doctest/doctest.h>

TEST_SUITE("2.1.5 符号：严格单字符") {

TEST_CASE("括号类") {
    CHECK(lex_dump(U"(") == "SIGN_LPAREN");
    CHECK(lex_dump(U")") == "SIGN_RPAREN");
    CHECK(lex_dump(U"[") == "SIGN_LBRACKET");
    CHECK(lex_dump(U"]") == "SIGN_RBRACKET");
    CHECK(lex_dump(U"{") == "SIGN_LBRACE");
    CHECK(lex_dump(U"}") == "SIGN_RBRACE");
}

TEST_CASE("分隔符类") {
    CHECK(lex_dump(U",") == "SIGN_COMMA");
    CHECK(lex_dump(U";") == "SIGN_SEMICOLON");
    CHECK(lex_dump(U":") == "SIGN_COLON");
    CHECK(lex_dump(U"@") == "SIGN_AT");
    CHECK(lex_dump(U"$") == "SIGN_DOLLAR");
}

TEST_CASE("永远只有单字符形式的几个") {
    CHECK(lex_dump(U"~") == "SIGN_TILDE");
    CHECK(lex_dump(U"?") == "SIGN_QUESTION");
}

}

TEST_SUITE("2.1.5 符号：贪婪最长匹配（多字符优先于短的）") {

TEST_CASE("+ 系列：+ / +=") {
    CHECK(lex_dump(U"+") == "SIGN_PLUS");
    CHECK(lex_dump(U"+=") == "SIGN_PLUS_ASSIGN");
    CHECK(lex_dump(U"++") == "SIGN_PLUS SIGN_PLUS");
    CHECK(lex_dump(U"+++") == "SIGN_PLUS SIGN_PLUS SIGN_PLUS");
}

TEST_CASE("- 系列：- / -=") {
    CHECK(lex_dump(U"-") == "SIGN_MINUS");
    CHECK(lex_dump(U"-=") == "SIGN_MINUS_ASSIGN");
    CHECK(lex_dump(U"--") == "SIGN_MINUS SIGN_MINUS");
    CHECK(lex_dump(U"---") == "SIGN_MINUS SIGN_MINUS SIGN_MINUS");
}

TEST_CASE("* 系列：* / ** / *= / **=，四层长度都要对") {
    CHECK(lex_dump(U"*") == "SIGN_STAR");
    CHECK(lex_dump(U"**") == "SIGN_DOUBLESTAR");
    CHECK(lex_dump(U"*=") == "SIGN_STAR_ASSIGN");
    CHECK(lex_dump(U"**=") == "SIGN_DOUBLESTAR_ASSIGN");
    CHECK(lex_dump(U"***") == "SIGN_DOUBLESTAR SIGN_STAR");
}

TEST_CASE("/ 系列：/ / // / /= / //=") {
    CHECK(lex_dump(U"/") == "SIGN_SLASH");
    CHECK(lex_dump(U"//") == "SIGN_DOUBLESLASH");
    CHECK(lex_dump(U"/=") == "SIGN_SLASH_ASSIGN");
    CHECK(lex_dump(U"//=") == "SIGN_DOUBLESLASH_ASSIGN");
}

TEST_CASE("% 系列：% / %=") {
    CHECK(lex_dump(U"%") == "SIGN_PERCENT");
    CHECK(lex_dump(U"%=") == "SIGN_PERCENT_ASSIGN");
}

TEST_CASE("& 系列：& / &=") {
    CHECK(lex_dump(U"&") == "SIGN_AMPERSAND");
    CHECK(lex_dump(U"&=") == "SIGN_AMPERSAND_ASSIGN");
}

TEST_CASE("| 系列：| / |=") {
    CHECK(lex_dump(U"|") == "SIGN_PIPE");
    CHECK(lex_dump(U"|=") == "SIGN_PIPE_ASSIGN");
}

TEST_CASE("^ 系列：^ / ^=") {
    CHECK(lex_dump(U"^") == "SIGN_CARET");
    CHECK(lex_dump(U"^=") == "SIGN_CARET_ASSIGN");
}

TEST_CASE("< 系列：< / << / <= / <<=，四层长度都要对") {
    CHECK(lex_dump(U"<") == "SIGN_LT");
    CHECK(lex_dump(U"<<") == "SIGN_LSHIFT");
    CHECK(lex_dump(U"<=") == "SIGN_LE");
    CHECK(lex_dump(U"<<=") == "SIGN_LSHIFT_ASSIGN");
}

TEST_CASE("> 系列：> / >> / >= / >>=") {
    CHECK(lex_dump(U">") == "SIGN_GT");
    CHECK(lex_dump(U">>") == "SIGN_RSHIFT");
    CHECK(lex_dump(U">=") == "SIGN_GE");
    CHECK(lex_dump(U">>=") == "SIGN_RSHIFT_ASSIGN");
}

TEST_CASE("= 系列：= / ==") {
    CHECK(lex_dump(U"=") == "SIGN_ASSIGN");
    CHECK(lex_dump(U"==") == "SIGN_EQ");
    CHECK(lex_dump(U"===") == "SIGN_EQ SIGN_ASSIGN");
}

TEST_CASE("! 系列：! / !=") {
    CHECK(lex_dump(U"!") == "SIGN_EXCLAIM");
    CHECK(lex_dump(U"!=") == "SIGN_NE");
}

TEST_CASE("spec 2.1.6 举的反例：`a! == b` 不能写成 `a!==b`，两者词法结果不同") {
    CHECK(lex_dump(U"a! == b") == "IDENTIFIER(a) SIGN_EXCLAIM SIGN_EQ IDENTIFIER(b)");
    // 贪婪匹配下 !== 被切成 != 和 =，跟上面那句语义完全不同，这正是 spec 里特别提醒的坑
    CHECK(lex_dump(U"a!==b") == "IDENTIFIER(a) SIGN_NE SIGN_ASSIGN IDENTIFIER(b)");
}

TEST_CASE("spec 2.1.6 举的正例：for $ (i : ls) { i ** 2 } 可以完全不加空白地压缩") {
    CHECK(lex_dump(U"for $ (i : ls) { i ** 2 }") == lex_dump(U"for$(i:ls){i**2}"));
}

}

TEST_SUITE("2.1.5 未知字符") {

TEST_CASE("完全不认识的字符报 SyntaxError") {
    // 用 char32_t 数值直接构造（0x20AC 是欧元符号），不依赖源文件/编译器对非 ASCII 字面量的解释，跨平台更稳妥
    const std::u32string euro_sign{static_cast<char32_t>(0x20AC)};
    CHECK_THROWS_AS(lex(euro_sign), SyntaxError);
    CHECK_THROWS_AS(lex(U"\\"), SyntaxError); // 裸反斜杠，不在任何符号表里
}

TEST_CASE("报错的行列指向那个字符本身，不是行首") {
    const std::u32string src{U'a', U' ', static_cast<char32_t>(0x20AC)}; // "a €"，€ 在第 3 列
    try {
        lex(src);
        FAIL("应当抛出异常");
    } catch (const SyntaxError &e) {
        const std::string msg{e.what()};
        CHECK(msg.find(":1:3:") != std::string::npos);
    }
}

}
