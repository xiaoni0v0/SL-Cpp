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
    TEST_CASE("前后的空行放行") {
        CHECK_NOTHROW(parse_as_single_expr(U"\n\n  1 + 1  \n\n"));
        CHECK_NOTHROW(parse_as_single_expr(U"1 + 1\n"));
    }

    TEST_CASE("顶层的 ';' 一律拒绝，多余的结尾分号也不行") {
        check_parse_as_single_expr_throws_with(U"1 + 1;", "unexpected ';'");
        check_parse_as_single_expr_throws_with(U";1 + 1", "unexpected ';'");
        check_parse_as_single_expr_throws_with(U";;\n1 + 1\n;;", "unexpected ';'");
        check_parse_as_single_expr_throws_with(U"\n;\n", "unexpected ';'");
    }

    TEST_CASE("一条都没有：报错") {
        check_parse_as_single_expr_throws_with(U"", "got none");
        check_parse_as_single_expr_throws_with(U"\n\n", "got none");
        check_parse_as_single_expr_throws_with(U"# 只有注释", "got none");
    }

    // 换行分隔的多条走"多于一条"这条；分号分隔的在更前面就被顶层 ';' 那条拦下了——
    // 两者报的不是同一条规则，是有意的：`a; b` 和 `a;` 该因为同一个理由（顶层有 ';'）被拒，
    // 而不是按"非空项有几个"分别判。
    TEST_CASE("换行分隔的多条：报错") {
        check_parse_as_single_expr_throws_with(U"a\nb", "more than one");
        check_parse_as_single_expr_throws_with(U"1 + 1\n2 + 2", "more than one");
    }

    TEST_CASE("分号分隔的多条：按顶层 ';' 拒绝") {
        check_parse_as_single_expr_throws_with(U"a; b", "unexpected ';'");
        check_parse_as_single_expr_throws_with(U"1 + 1; 2 + 2", "unexpected ';'");
    }

    TEST_CASE("要放多条得自己写成复合表达式——那是一条表达式，能过") {
        const auto node = nlohmann::json(parse_as_single_expr(U"{ a; b }")->to_json());
        CHECK(node["type"] == "Compound");
        CHECK(node["exprs"].size() == 2);
    }

    TEST_CASE("表达式本身语法就错：照常报语法错误，不是'条数'那两个消息") {
        // 这里要的是"报错原因是表达式本身不完整"，而不是被条数检查抢先拦下
        check_parse_as_single_expr_throws_with(U"1 +", "expected");
        CHECK_THROWS_AS(parse_as_single_expr(U"("), SyntaxError);
    }
}
