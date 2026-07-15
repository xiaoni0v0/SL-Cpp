// SL.md 2.1.4 字面量——反引号原始字符串：不处理任何转义，原样天然支持多行
#include "../test_utils.h"
#include "../../../builtins/classes/exceptions/SyntaxError.h"

#include <doctest/doctest.h>

TEST_SUITE("2.1.4 反引号原始字符串") {

TEST_CASE("基本用法，产出的类型跟普通字符串一样是 LITERAL_STR") {
    CHECK(lex_dump(U"`hello`") == "LITERAL_STR(hello)");
}

TEST_CASE("空原始字符串") {
    CHECK(lex_dump(U"``") == "LITERAL_STR()");
}

TEST_CASE("不处理任何转义：反斜杠 n 是字面两个字符，不是换行") {
    const auto tokens{lex(U"`a\\nb`")};
    REQUIRE(tokens.size() == 2);
    CHECK(tokens[0].lexeme == U"a\\nb"); // 注意是反斜杠+n 两个字符，不是真正的换行符
}

TEST_CASE("天然支持多行：字面换行直接进入 lexeme") {
    const auto tokens{lex(U"`line1\nline2`")};
    REQUIRE(tokens.size() == 2);
    CHECK(tokens[0].lexeme == U"line1\nline2");
}

TEST_CASE("多行原始字符串跨行后行号正确递增") {
    const auto tokens{lex(U"`a\nb\nc` 1")};
    REQUIRE(tokens.size() == 3); // str, int, EOF
    CHECK(tokens[0].row == 1);
    CHECK(tokens[1].row == 3); // 字符串内跨了两个换行，1 应该在第 3 行
}

TEST_CASE("内部可以自由出现单引号、双引号，不需要转义") {
    CHECK(lex_dump(U"`it's \"quoted\"`") == "LITERAL_STR(it's \"quoted\")");
}

TEST_CASE("内部不能出现反引号本身（读到第一个反引号就终止，这是天然的，不需要额外校验）") {
    // ``` 会被读成：空字符串 `` ，再剩一个反引号单独开始一个新的、未闭合的字符串
    CHECK_THROWS_AS(lex(U"```"), SyntaxError);
}

TEST_CASE("要表达含反引号的内容，需要用普通字符串拼接（这里只验证词法能正确切出普通字符串+反引号字符串两段）") {
    CHECK(lex_dump(U"\"`\" + `abc`") == "LITERAL_STR(`) SIGN_PLUS LITERAL_STR(abc)");
}

TEST_CASE("未闭合的原始字符串（到 EOF 都没有反引号）报 SyntaxError") {
    CHECK_THROWS_AS(lex(U"`abc"), SyntaxError);
}

TEST_CASE("原始字符串可以和普通字符串混用在同一段源码里") {
    CHECK(lex_dump(U"`raw` + \"normal\"") == "LITERAL_STR(raw) SIGN_PLUS LITERAL_STR(normal)");
}

}
