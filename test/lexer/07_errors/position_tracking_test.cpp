// token 的行列：从 1 起，跨行重置列；多字符 token 记起始位置。
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("词法位置") {

    TEST_CASE("单行列号") {
        const auto tokens{lex(U"1 + 22")};
        REQUIRE(tokens.size() == 4);
        CHECK(tokens[0].row == 1);
        CHECK(tokens[0].col == 1);
        CHECK(tokens[1].row == 1);
        CHECK(tokens[1].col == 3);
        CHECK(tokens[2].row == 1);
        CHECK(tokens[2].col == 5);
    }

    TEST_CASE("跨行后行号递增、列号重置") {
        const auto tokens{lex(U"1\n  22")};
        REQUIRE(tokens.size() == 4);
        CHECK(tokens[0].row == 1);
        CHECK(tokens[0].col == 1);
        CHECK(tokens[2].row == 2);
        CHECK(tokens[2].col == 3);
    }

    TEST_CASE("每行从列 1 计") {
        const auto tokens{lex(U"1\n2\n3")};
        REQUIRE(tokens.size() == 6);
        CHECK(tokens[0].row == 1);
        CHECK(tokens[0].col == 1);
        CHECK(tokens[2].row == 2);
        CHECK(tokens[2].col == 1);
        CHECK(tokens[4].row == 3);
        CHECK(tokens[4].col == 1);
    }

    TEST_CASE("多字符 token 和字符串记起始位置") {
        const auto ident{lex(U"  foobar")};
        REQUIRE(ident.size() == 2);
        CHECK(ident[0].col == 3);
        const auto str{lex(U"  \"hello\"")};
        REQUIRE(str.size() == 2);
        CHECK(str[0].col == 3);
    }

    TEST_CASE("跨行原始字符串 / 块注释之后行号正确") {
        const auto raw{lex(U"`a\nb` x")};
        REQUIRE(raw.size() == 3);
        CHECK(raw[0].row == 1);
        CHECK(raw[1].row == 2);
        const auto comment{lex(U"/* a\nb\nc */ x")};
        REQUIRE(comment.size() == 2);
        CHECK(comment[0].row == 3);
    }

    TEST_CASE("EOF 在文件最末尾") {
        const auto tokens{lex(U"1")};
        REQUIRE(tokens.size() == 2);
        CHECK(tokens[1].type == TokenType::END_OF_FILE);
        CHECK(tokens[1].col == 2);
    }
}
