// return，expr 可省略。
#include "../../../builtins/exceptions/SyntaxError.h"
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("return") {

    TEST_CASE("裸 return，值为 None（value 为 null）") {
        CHECK(parse_json(U"return") == nlohmann::json{{"type", "Return"}, {"value", nullptr}});
    }

    TEST_CASE("带值") {
        CHECK(
            parse_json(U"return 5") == nlohmann::json{{"type", "Return"}, {"value", int_lit("5")}}
        );
    }

    TEST_CASE("return a, b 不是返回元组，逗号处缺分隔符") {
        CHECK_THROWS_AS(parse_as_file(U"return a, b"), SyntaxError);
    }

    TEST_CASE("带值是复杂表达式") {
        CHECK(
            parse_json(U"return 1 + 2") == nlohmann::json{
                                               {"type", "Return"},
                                               {"value",
                                                {{"type", "OpBinary"},
                                                 {"op", "+"},
                                                 {"left", int_lit("1")},
                                                 {"right", int_lit("2")}}}
                                           }
        );
    }

    TEST_CASE("裸 return 后面紧跟 ';' 也算裸 return") {
        CHECK(
            parse_program_json(U"return; 1") ==
            nlohmann::json{
                {"type", "Program"},
                {"exprs",
                 nlohmann::json::array({{{"type", "Return"}, {"value", nullptr}}, int_lit("1")})}
            }
        );
    }

    TEST_CASE(
        "裸 return 出现在括号/中括号/逗号语境时也能正确识别为裸 return（不会误吞后面的符号）"
    ) {
        CHECK(
            parse_json(U"(return,)") ==
            nlohmann::json{
                {"type", "LiteralTuple"},
                {"items", nlohmann::json::array({{{"type", "Return"}, {"value", nullptr}}})}
            }
        );
        CHECK(
            parse_json(U"[return]") ==
            nlohmann::json{
                {"type", "LiteralList"},
                {"items", nlohmann::json::array({{{"type", "Return"}, {"value", nullptr}}})}
            }
        );
        CHECK(
            parse_json(U"f(return)") ==
            nlohmann::json{
                {"type", "Call"},
                {"object", {{"type", "Identifier"}, {"identifier", "f"}}},
                {"positional_args",
                 nlohmann::json::array({{{"type", "Return"}, {"value", nullptr}}})},
                {"keyword_args", nlohmann::json::array()}
            }
        );
    }

    // 裸 return 可以直接当 if/try 某个分支的 body，此时后续子句的引导关键字就是它的右边界。
    // 这几个关键字不在终止符集合里的话，return 会拿它们去 parse_expr()，报出跟真实错因无关的
    // "unexpected token 'else'" 之类；换行写法因为撞上 NEWLINE 会碰巧躲过去，一行内写则必炸。
    TEST_CASE("裸 return 后面紧跟 else/elif 也算裸 return（if 的分支体）") {
        const auto if_else = parse_json(U"if (c) return else 1");
        CHECK(if_else["type"] == "If");
        CHECK(
            if_else["clauses"][0]["body"] == nlohmann::json{{"type", "Return"}, {"value", nullptr}}
        );
        CHECK(if_else["else_expr"] == int_lit("1"));

        const auto if_elif = parse_json(U"if (c) return elif (d) 1 else 2");
        CHECK(if_elif["clauses"].size() == 2);
        CHECK(
            if_elif["clauses"][0]["body"] == nlohmann::json{{"type", "Return"}, {"value", nullptr}}
        );
    }

    TEST_CASE("裸 return 后面紧跟 except/finally 也算裸 return（try 的主体）") {
        const auto with_except = parse_json(U"try return except (E) 1");
        CHECK(with_except["type"] == "Try");
        CHECK(with_except["try_expr"] == nlohmann::json{{"type", "Return"}, {"value", nullptr}});

        const auto with_finally = parse_json(U"try return finally 1");
        CHECK(with_finally["try_expr"] == nlohmann::json{{"type", "Return"}, {"value", nullptr}});
        CHECK(with_finally["finally_expr"] == int_lit("1"));
    }

    TEST_CASE("带值的 return 照常把这几个关键字当右边界，不会把它们吞进 value") {
        const auto result = parse_json(U"if (c) return 5 else 1");
        CHECK(
            result["clauses"][0]["body"] ==
            nlohmann::json{{"type", "Return"}, {"value", int_lit("5")}}
        );
        CHECK(result["else_expr"] == int_lit("1"));
    }
}
