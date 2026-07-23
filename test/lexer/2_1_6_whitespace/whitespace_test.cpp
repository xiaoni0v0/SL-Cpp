// SL.md 2.1.6 空白字符：注释、空格、制表符都算空白；换行本身是有意义的 token（表达式分隔符的一部分）
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("2.1.6 空白字符") {

TEST_CASE("空格、制表符会被跳过，不产生 token") {
    CHECK(lex_dump(U"1   2") == "LITERAL_INT(1) LITERAL_INT(2)");
    CHECK(lex_dump(U"1\t\t2") == "LITERAL_INT(1) LITERAL_INT(2)");
    CHECK(lex_dump(U"1 \t 2") == "LITERAL_INT(1) LITERAL_INT(2)");
}

TEST_CASE("空白多少都不影响结果，有没有空白只要不产生歧义就等价") {
    CHECK(lex_dump(U"1+2") == lex_dump(U"1 + 2"));
    CHECK(lex_dump(U"1+2") == lex_dump(U"1    +    2"));
}

TEST_CASE("换行本身会产生 NEWLINE token（不算被跳过的空白）") {
    CHECK(lex_dump(U"1\n2") == "LITERAL_INT(1) NEWLINE LITERAL_INT(2)");
}

TEST_CASE("文件开头的换行被省略，不产生 NEWLINE token") {
    CHECK(lex_dump(U"\n\n1") == "LITERAL_INT(1)");
}

TEST_CASE("连续多个换行会被合并成最多一个 NEWLINE token") {
    CHECK(lex_dump(U"1\n\n\n2") == "LITERAL_INT(1) NEWLINE LITERAL_INT(2)");
}

TEST_CASE("分号后紧跟的换行会被省略（分号已经是硬分隔符了）") {
    CHECK(lex_dump(U"1;\n2") == "LITERAL_INT(1) SIGN_SEMICOLON LITERAL_INT(2)");
}

TEST_CASE("换行后再紧跟换行仍然只算一个（NEWLINE 后面的 NEWLINE 被省略）") {
    CHECK(lex_dump(U"1\n\n2\n\n\n3") == "LITERAL_INT(1) NEWLINE LITERAL_INT(2) NEWLINE LITERAL_INT(3)");
}

TEST_CASE("文件末尾的换行：第一个正常保留，多余的合并/省略，不会凭空消失") {
    // 抑制规则只看"上一个 token 是不是 NEWLINE/SEMICOLON"，跟后面还有没有内容无关：
    // 1 后面第一个 \n 正常产出 NEWLINE，第二、三个因为上一个 token 已经是 NEWLINE 才被抑制。
    CHECK(lex_dump(U"1\n\n\n") == "LITERAL_INT(1) NEWLINE");
    // 结尾只有空格没有换行，空格本来就被跳过、不留任何 token
    CHECK(lex_dump(U"1   ") == "LITERAL_INT(1)");
}

TEST_CASE("空文件只产生 EOF") {
    const auto tokens{lex(U"")};
    REQUIRE(tokens.size() == 1);
    CHECK(tokens[0].type == TokenType::END_OF_FILE);
}

TEST_CASE("只有空白的文件等价于空文件") {
    CHECK(lex_dump(U"   \t\t  ") == "");
    CHECK(lex_dump(U"\n\n\n") == "");
}

TEST_CASE("回车符 \r 也被当作空白跳过（spec 2.1.6 明确列为空白字符）") {
    CHECK(lex_dump(U"1\r2") == "LITERAL_INT(1) LITERAL_INT(2)");
    CHECK(lex_dump(U"1\r\r2") == "LITERAL_INT(1) LITERAL_INT(2)");
    CHECK(lex_dump(U"1 \r\t 2") == "LITERAL_INT(1) LITERAL_INT(2)");
}

TEST_CASE("CRLF 行尾：\r 被跳过，\n 正常产出 NEWLINE") {
    CHECK(lex_dump(U"1\r\n2") == "LITERAL_INT(1) NEWLINE LITERAL_INT(2)");
}

}
