// 跨章节：token 的行/列追踪是否正确（报错定位靠这个，2.1 各节都间接依赖它）
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("位置追踪") {

    TEST_CASE("单行内列号正确递增，从 1 开始") {
        const auto tokens{lex(U"1 + 22")};
        REQUIRE(tokens.size() == 4); // 1, +, 22, EOF
        CHECK(tokens[0].row == 1);
        CHECK(tokens[0].col == 1); // "1"
        CHECK(tokens[1].row == 1);
        CHECK(tokens[1].col == 3); // "+"
        CHECK(tokens[2].row == 1);
        CHECK(tokens[2].col == 5); // "22"
    }

    TEST_CASE("跨行后行号递增、列号重置为 1") {
        const auto tokens{lex(U"1\n  22")};
        REQUIRE(tokens.size() == 4); // 1, NEWLINE, 22, EOF
        CHECK(tokens[0].row == 1);
        CHECK(tokens[0].col == 1);
        CHECK(tokens[2].row == 2);
        CHECK(tokens[2].col == 3); // "22" 前面有两个空格
    }

    TEST_CASE("多行输入里每一行都从列 1 开始计") {
        const auto tokens{lex(U"1\n2\n3")};
        REQUIRE(tokens.size() == 6); // 1 NEWLINE 2 NEWLINE 3 EOF
        CHECK(tokens[0].row == 1);
        CHECK(tokens[0].col == 1);
        CHECK(tokens[2].row == 2);
        CHECK(tokens[2].col == 1);
        CHECK(tokens[4].row == 3);
        CHECK(tokens[4].col == 1);
    }

    TEST_CASE("多字符 token 的位置记录的是起始位置，不是结束位置") {
        const auto tokens{lex(U"  foobar")};
        REQUIRE(tokens.size() == 2); // foobar, EOF
        CHECK(tokens[0].col == 3);   // f 在第 3 列，不是 bar 结束的那一列
    }

    TEST_CASE("字符串字面量的位置是开头引号的位置") {
        const auto tokens{lex(U"  \"hello\"")};
        REQUIRE(tokens.size() == 2);
        CHECK(tokens[0].col == 3); // 开头引号在第 3 列
    }

    TEST_CASE("跨行字符串（反引号）内部换行不影响之后 token 的行号计算错") {
        const auto tokens{lex(U"`a\nb` x")};
        REQUIRE(tokens.size() == 3); // str, x, EOF
        CHECK(tokens[0].row == 1);
        CHECK(tokens[1].row == 2); // x 在第二行
    }

    TEST_CASE("跨行块注释不影响之后 token 的行号计算") {
        const auto tokens{lex(U"/* a\nb\nc */ x")};
        REQUIRE(tokens.size() == 2); // x, EOF
        CHECK(tokens[0].row == 3);
    }

    TEST_CASE("EOF token 的位置是文件最末尾") {
        const auto tokens{lex(U"1")};
        REQUIRE(tokens.size() == 2);
        CHECK(tokens[1].type == TokenType::END_OF_FILE);
        CHECK(tokens[1].col == 2); // "1" 后面紧接的位置
    }
}
