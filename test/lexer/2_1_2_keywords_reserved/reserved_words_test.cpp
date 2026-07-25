// SL.md 2.1.2 关键字与保留字——保留字部分：出现即无条件 SyntaxError
#include "../test_utils.h"
#include "../../../builtins/exceptions/SyntaxError.h"

#include <doctest/doctest.h>

TEST_SUITE("2.1.2 保留字") {

TEST_CASE("每个保留字单独出现都抛 SyntaxError（含 when/local/assert 这几个之前分词器漏掉/没跟上改名的）") {
    CHECK_THROWS_AS(lex(U"define"), SyntaxError);
    CHECK_THROWS_AS(lex(U"as"), SyntaxError);
    CHECK_THROWS_AS(lex(U"yield"), SyntaxError);
    CHECK_THROWS_AS(lex(U"async"), SyntaxError);
    CHECK_THROWS_AS(lex(U"await"), SyntaxError);
    CHECK_THROWS_AS(lex(U"in"), SyntaxError);
    CHECK_THROWS_AS(lex(U"const"), SyntaxError);
    CHECK_THROWS_AS(lex(U"static"), SyntaxError);
    CHECK_THROWS_AS(lex(U"with"), SyntaxError);
    CHECK_THROWS_AS(lex(U"when"), SyntaxError);
    CHECK_THROWS_AS(lex(U"case"), SyntaxError);
    CHECK_THROWS_AS(lex(U"local"), SyntaxError);
    CHECK_THROWS_AS(lex(U"assert"), SyntaxError);
}

TEST_CASE("match 不再是保留字（match/case 改名成 when/case 之后），现在是普通标识符") {
    CHECK(lex_dump(U"match") == "IDENTIFIER(match)");
}

TEST_CASE("保留字出现在表达式中间同样报错，不是只在开头才检查") {
    CHECK_THROWS_AS(lex(U"1 + local"), SyntaxError);
    CHECK_THROWS_AS(lex(U"func f() { when }"), SyntaxError);
}

TEST_CASE("保留字大小写变体不受影响，是普通标识符") {
    CHECK(lex_dump(U"When") == "IDENTIFIER(When)");
    CHECK(lex_dump(U"LOCAL") == "IDENTIFIER(LOCAL)");
    CHECK(lex_dump(U"Assert") == "IDENTIFIER(Assert)");
}

TEST_CASE("保留字前缀/超集不受影响，是普通标识符") {
    CHECK(lex_dump(U"whenever") == "IDENTIFIER(whenever)");
    CHECK(lex_dump(U"locality") == "IDENTIFIER(locality)");
    CHECK(lex_dump(U"assertion") == "IDENTIFIER(assertion)");
}

TEST_CASE("报错信息带有正确的行列，指向保留字出现的位置") {
    try {
        lex(U"1\n2 local");
        FAIL("应当抛出 SyntaxError");
    } catch (const SyntaxError &e) {
        const std::string msg{e.what()};
        CHECK(msg.find("2:3") != std::string::npos); // 第 2 行第 3 列是 local 开头
    }
}

}
