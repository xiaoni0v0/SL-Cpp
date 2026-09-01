// 空格、制表符、回车是空白；换行是 NEWLINE token，连续换行会合并。
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("空白") {

    TEST_CASE("空格和制表符不产生 token") {
        CHECK(lex_dump(U"1   2") == "LITERAL_INT(1) LITERAL_INT(2)");
        CHECK(lex_dump(U"1\t\t2") == "LITERAL_INT(1) LITERAL_INT(2)");
        CHECK(lex_dump(U"1 \t 2") == "LITERAL_INT(1) LITERAL_INT(2)");
        CHECK(lex_dump(U"1+2") == lex_dump(U"1 + 2"));
    }

    TEST_CASE("换行产生 NEWLINE；行首和连续换行会省略") {
        CHECK(lex_dump(U"1\n2") == "LITERAL_INT(1) NEWLINE LITERAL_INT(2)");
        CHECK(lex_dump(U"\n\n1") == "LITERAL_INT(1)");
        CHECK(lex_dump(U"1\n\n\n2") == "LITERAL_INT(1) NEWLINE LITERAL_INT(2)");
        CHECK(lex_dump(U"1;\n2") == "LITERAL_INT(1) SIGN_SEMICOLON LITERAL_INT(2)");
        CHECK(
            lex_dump(U"1\n\n2\n\n\n3") ==
            "LITERAL_INT(1) NEWLINE LITERAL_INT(2) NEWLINE LITERAL_INT(3)"
        );
        CHECK(lex_dump(U"1\n\n\n") == "LITERAL_INT(1) NEWLINE");
        CHECK(lex_dump(U"1   ") == "LITERAL_INT(1)");
    }

    TEST_CASE("空文件和纯空白只剩 EOF") {
        const auto tokens{lex(U"")};
        REQUIRE(tokens.size() == 1);
        CHECK(tokens[0].type == TokenType::END_OF_FILE);
        CHECK(lex_dump(U"   \t\t  ") == "");
        CHECK(lex_dump(U"\n\n\n") == "");
    }

    TEST_CASE("\\r 是空白；CRLF 里 \\n 仍是 NEWLINE") {
        CHECK(lex_dump(U"1\r2") == "LITERAL_INT(1) LITERAL_INT(2)");
        CHECK(lex_dump(U"1\r\r2") == "LITERAL_INT(1) LITERAL_INT(2)");
        CHECK(lex_dump(U"1 \r\t 2") == "LITERAL_INT(1) LITERAL_INT(2)");
        CHECK(lex_dump(U"1\r\n2") == "LITERAL_INT(1) NEWLINE LITERAL_INT(2)");
    }
}
