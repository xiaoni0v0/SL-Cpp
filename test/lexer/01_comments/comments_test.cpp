// 单行 #、块注释 /* */。块注释不嵌套；内部换行不产生 NEWLINE。
#include "../../../builtins/exceptions/SyntaxError.h"
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("注释") {

    TEST_CASE("单行注释吃到行尾，不消耗换行") {
        CHECK(lex_dump(U"1 # 这是注释 2 3\n4") == "LITERAL_INT(1) NEWLINE LITERAL_INT(4)");
        CHECK(lex_dump(U"1 #c\n2") == "LITERAL_INT(1) NEWLINE LITERAL_INT(2)");
        CHECK(lex_dump(U"# 整行都是注释") == "");
        CHECK(lex_dump(U"1 # comment, no trailing newline") == "LITERAL_INT(1)");
    }

    TEST_CASE("单行注释里的 #、引号、反引号都只是文本") {
        CHECK(lex_dump(U"1 # ## \"x\" 'y' `z` \\n \n2") == "LITERAL_INT(1) NEWLINE LITERAL_INT(2)");
    }

    TEST_CASE("块注释是空白，可跨行") {
        CHECK(lex_dump(U"1 /* comment */ 2") == "LITERAL_INT(1) LITERAL_INT(2)");
        CHECK(lex_dump(U"/* just a comment */") == "");
        CHECK(lex_dump(U"1 /* line1\nline2\nline3 */ 2") == "LITERAL_INT(1) LITERAL_INT(2)");
        CHECK(lex_dump(U"/** comment **/ 1") == "LITERAL_INT(1)");
    }

    TEST_CASE("块注释跨行后后续 token 的行号跟着走") {
        const auto tokens{lex(U"1 /* a\nb\nc */ 2")};
        REQUIRE(tokens.size() == 3);
        CHECK(tokens[0].row == 1);
        CHECK(tokens[1].row == 3);
    }

    TEST_CASE("块注释不嵌套：内层 */ 结束整段") {
        CHECK(lex_dump(U"/* a /* b */ c */") == "IDENTIFIER(c) SIGN_STAR SIGN_SLASH");
    }

    TEST_CASE("未闭合块注释报错") { CHECK_THROWS_AS(lex(U"1 /* never closed"), SyntaxError); }

    TEST_CASE("/ 后面不是 * 就是除号") {
        CHECK(lex_dump(U"1 / 2") == "LITERAL_INT(1) SIGN_SLASH LITERAL_INT(2)");
    }
}
