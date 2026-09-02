// parse_as_single_expr：整份输入必须恰好一条表达式。
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("Parser 单表达式入口") {

    TEST_CASE("恰好一条：正常返回这条表达式自己的节点，外面没有 Program 那一层") {
        const auto node = nlohmann::json(AstJsonDumper::dump(*parse_as_single_expr(U"x = 0")));
        CHECK(
            node == nlohmann::json{
                        {"type", "Assign"},
                        {"target", {{"type", "Identifier"}, {"identifier", "x"}}},
                        {"value", {{"type", "LiteralInt"}, {"raw", "0"}}}
                    }
        );
    }

    // 换行是软终止，前后没内容时什么也没分隔，放行。
    // ';' 是硬终止，对只收一条的入口永远多余：开头的由 parse_non_op 报，后面的由 expect(EOF) 报。
    TEST_CASE("前后的空行放行") {
        CHECK_NOTHROW(parse_as_single_expr(U"\n\n  1 + 1  \n\n"));
        CHECK_NOTHROW(parse_as_single_expr(U"1 + 1\n"));
    }

    TEST_CASE("开头的 ';'：当成一条表达式的开头去解析，报的是“这不是表达式”") {
        check_parse_as_single_expr_throws_with(U";1 + 1", "unexpected token ';'");
        check_parse_as_single_expr_throws_with(U";;\n1 + 1\n;;", "unexpected token ';'");
        check_parse_as_single_expr_throws_with(U"\n;\n", "unexpected token ';'");
    }

    // `a;` 和 `a; b` 都是表达式后面还有 ';'，同一条规则。
    TEST_CASE("表达式之后的 ';'：卡在“后面必须就是 EOF”这一步") {
        check_parse_as_single_expr_throws_with(U"1 + 1;", "expected EOF but got ';'");
        check_parse_as_single_expr_throws_with(U"a; b", "expected EOF but got ';'");
        check_parse_as_single_expr_throws_with(U"1 + 1; 2 + 2", "expected EOF but got ';'");
    }

    TEST_CASE("一条都没有：报错") {
        check_parse_as_single_expr_throws_with(U"", "unexpected end of file");
        check_parse_as_single_expr_throws_with(U"\n\n", "unexpected end of file");
        check_parse_as_single_expr_throws_with(U"# 只有注释", "unexpected end of file");
    }

    // 第一条之后的换行被跳掉，于是卡住的是下一条的第一个 token
    TEST_CASE("换行分隔的多条：报错") {
        check_parse_as_single_expr_throws_with(U"a\nb", "expected EOF but got an identifier");
        check_parse_as_single_expr_throws_with(
            U"1 + 1\n2 + 2", "expected EOF but got an integer literal"
        );
    }

    TEST_CASE("要放多条得自己写成复合表达式——那是一条表达式，能过") {
        const auto node = nlohmann::json(AstJsonDumper::dump(*parse_as_single_expr(U"{ a; b }")));
        CHECK(node["type"] == "Compound");
        CHECK(node["exprs"].size() == 2);
    }

    TEST_CASE("表达式本身语法就错：照常报语法错误，不是条数检查抢先拦下的") {
        check_parse_as_single_expr_throws_with(U"1 +", "expected");
        CHECK_THROWS_AS(parse_as_single_expr(U"("), SyntaxError);
    }
}
