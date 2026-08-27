// 跨分组：Parser::parse_as_single_expr——把整个输入解析成恰好一条表达式的入口（eval(code) 用）。
// 只测"恰好一条"这件事本身；表达式内部怎么解析跟 parse_as_file 走的是同一套代码，各分组已经覆盖。
// 顶层 return/break/continue 的合法性不归这一步管（那是 SemanticChecker 的事），
// 见 test/analyzer/semantic_checker/single_expr_test.cpp。
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("Parser 单表达式入口") {

    TEST_CASE("恰好一条：正常返回这条表达式自己的节点，外面没有 Program 那一层") {
        const auto node = nlohmann::json(parse_as_single_expr(U"x = 0")->to_json());
        CHECK(
            node == nlohmann::json{
                        {"type", "Assign"},
                        {"target", {{"type", "Identifier"}, {"identifier", "x"}}},
                        {"value", {{"type", "LiteralInt"}, {"raw", "0"}}}
                    }
        );
    }

    // 换行是软终止（见 SL.md 的表达式分隔符一节），前后没有内容时它什么也没分隔，放行；
    // ';' 是硬终止，写出来即断言一条表达式的边界，对只收一条的入口永远多余，一律拒绝。
    // 拒绝不靠专门的检查：';' 既不是表达式的开头、也不是 EOF，前后两步自然就把它挡下了——
    // 于是开头的 ';' 由 parse_non_op 报，表达式之后的 ';' 由结尾那个 expect(EOF) 报。
    TEST_CASE("前后的空行放行") {
        CHECK_NOTHROW(parse_as_single_expr(U"\n\n  1 + 1  \n\n"));
        CHECK_NOTHROW(parse_as_single_expr(U"1 + 1\n"));
    }

    TEST_CASE("开头的 ';'：当成一条表达式的开头去解析，报的是“这不是表达式”") {
        check_parse_as_single_expr_throws_with(U";1 + 1", "unexpected token ';'");
        check_parse_as_single_expr_throws_with(U";;\n1 + 1\n;;", "unexpected token ';'");
        check_parse_as_single_expr_throws_with(U"\n;\n", "unexpected token ';'");
    }

    // `a;` 和 `a; b` 因为同一个理由（表达式之后还有东西，而且那东西是 ';'）被拒，
    // 不按"非空项有几个"分别判——分号分隔的多条本来就该跟多余的结尾分号同一条规则。
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
        const auto node = nlohmann::json(parse_as_single_expr(U"{ a; b }")->to_json());
        CHECK(node["type"] == "Compound");
        CHECK(node["exprs"].size() == 2);
    }

    TEST_CASE("表达式本身语法就错：照常报语法错误，不是条数检查抢先拦下的") {
        check_parse_as_single_expr_throws_with(U"1 +", "expected");
        CHECK_THROWS_AS(parse_as_single_expr(U"("), SyntaxError);
    }
}
