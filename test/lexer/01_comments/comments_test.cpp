// 单行 #、块注释 /* */。块注释不嵌套；内部换行不产生 NEWLINE。
#include "../../../diagnostics/SyntaxError.h"
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

    // 单行注释只认 '\n' 为行终止，'\r' 不是——这跟 "\\r 是空白" 那条规则（见 06_whitespace）
    // 是同一个 '\r' 但不同的处理层：裸 '\r' 出现在 token 之间会被当空白跳过，出现在注释内部
    // 则只是被吃掉的普通文本，注释不会因为遇到 '\r' 就提前结束
    TEST_CASE("CRLF 行尾：注释正常吃到 \\n 为止，\\r 不提前结束注释") {
        CHECK(lex_dump(U"1 #c\r\n2") == "LITERAL_INT(1) NEWLINE LITERAL_INT(2)");
    }

    TEST_CASE("单行注释内部出现孤立的 \\r（不跟着 \\n）：\\r 只是文本，注释继续吃到真正的 \\n") {
        CHECK(lex_dump(U"1 #a\rb\n2") == "LITERAL_INT(1) NEWLINE LITERAL_INT(2)");
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

    TEST_CASE("注释里出现保留字/关键字拼出的文本，只是普通文本") {
        // 整行都是注释、这一行又是源码开头：行首换行被省略，不会多出一个 NEWLINE
        CHECK(lex_dump(U"# local assert yield\n1") == "LITERAL_INT(1)");
        CHECK(lex_dump(U"/* local assert */ 1") == "LITERAL_INT(1)");
    }

    TEST_CASE("空块注释 /**/ 是合法的空白") {
        CHECK(lex_dump(U"1 /**/ 2") == "LITERAL_INT(1) LITERAL_INT(2)");
    }

    TEST_CASE("/*/ 不是自己闭合的块注释：内容是单个 '/'，仍未闭合") {
        CHECK_THROWS_AS(lex(U"/*/"), SyntaxError);
    }

    TEST_CASE("未闭合块注释报错，消息和位置指向 /* 开头") {
        try {
            lex(U"1 /* never closed");
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("unterminated") != std::string::npos);
            CHECK(msg.find(":1:3:") != std::string::npos); // '/' 在第 3 列
        }
    }

    TEST_CASE("/ 后面不是 * 就是除号") {
        CHECK(lex_dump(U"1 / 2") == "LITERAL_INT(1) SIGN_SLASH LITERAL_INT(2)");
    }
}
