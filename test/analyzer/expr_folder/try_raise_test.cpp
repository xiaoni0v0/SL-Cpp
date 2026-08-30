// ExprFolder：AstNodeTry/AstNodeRaise 各子槽位的子表达式折叠。
// try/raise 本身永远不参与整体折叠（不在 StaticEvaler::fold 的分发范围内，运行期才能确定异常
// 是否发生），这里只关心：try_expr_、except 的 exceptions_/target_/body_、finally_expr_、
// raise 的 value_，这些子槽位各自的表达式该折还是照样会被递归折叠。
//
// target_ 是 `except (E as 目标)` 的绑定目标，跟 for 的 as 目标一样是左值：左值只可能是
// 标识符/属性访问/索引/解构元组或列表，这几种都不在 StaticEvaler::fold 的分发范围内，所以目标
// 本身永远折不动，能折的是它内部的子表达式（如 a[1 + 1] 的下标）。
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("ExprFolder try/raise 子表达式折叠") {

    TEST_CASE("try_expr_ 会被折叠") {
        CHECK(
            fold_json(U"try (1 + 1) except (E) b") ==
            nlohmann::json{
                {"type", "Try"},
                {"try_expr", int_lit("2")},
                {"except_clauses",
                 nlohmann::json::array({nlohmann::json{
                     {"exceptions",
                      nlohmann::json::array({{{"type", "Identifier"}, {"identifier", "E"}}})},
                     {"target", nullptr},
                     {"body", {{"type", "Identifier"}, {"identifier", "b"}}}
                 }})},
                {"finally_expr", nullptr}
            }
        );
    }

    TEST_CASE("except 的 exceptions_ 各元素会被折叠") {
        const auto result = fold_json(U"try a except (1 + 1, 2 + 2) b");
        CHECK(
            result["except_clauses"][0]["exceptions"] ==
            nlohmann::json::array({int_lit("2"), int_lit("4")})
        );
    }

    TEST_CASE("except 的 body_ 会被折叠") {
        const auto result = fold_json(U"try a except (E) (1 + 1)");
        CHECK(result["except_clauses"][0]["body"] == int_lit("2"));
    }

    TEST_CASE("except 的 target_ 内部会被折叠（目标本身是左值，折不动，折的是它的下标）") {
        const auto result = fold_json(U"try a except (E as b[1 + 1]) c");
        CHECK(
            result["except_clauses"][0]["target"] ==
            nlohmann::json{
                {"type", "Index"},
                {"object", {{"type", "Identifier"}, {"identifier", "b"}}},
                {"args", nlohmann::json::array({int_lit("2")})}
            }
        );
    }

    TEST_CASE("没写 as 时 target_ 是空槽位，不影响其他槽位照常折叠") {
        const auto result = fold_json(U"try a except (E) (1 + 1)");
        CHECK(result["except_clauses"][0]["target"] == nullptr);
        CHECK(result["except_clauses"][0]["body"] == int_lit("2"));
    }

    TEST_CASE("多个 except 子句，每条自己的 exceptions_/target_/body_ 都各自独立折叠") {
        const auto result =
            fold_json(U"try a except (1 + 1 as x[2 + 2]) (3 + 3) except (4 + 4) (5 + 5)");
        CHECK(result["except_clauses"][0]["exceptions"] == nlohmann::json::array({int_lit("2")}));
        CHECK(
            result["except_clauses"][0]["target"]["args"] == nlohmann::json::array({int_lit("4")})
        );
        CHECK(result["except_clauses"][0]["body"] == int_lit("6"));
        CHECK(result["except_clauses"][1]["exceptions"] == nlohmann::json::array({int_lit("8")}));
        CHECK(result["except_clauses"][1]["target"] == nullptr);
        CHECK(result["except_clauses"][1]["body"] == int_lit("10"));
    }

    TEST_CASE("finally_expr_ 会被折叠") {
        const auto result = fold_json(U"try a finally (1 + 1)");
        CHECK(result["finally_expr"] == int_lit("2"));
    }

    TEST_CASE("finally_expr_ 本身是复合表达式时，一样会先递归折叠再套用复合表达式规则") {
        // { 1; 2 } 全是字面量，折成最后一条 2
        CHECK(fold_json(U"try a finally { 1; 2 }")["finally_expr"] == int_lit("2"));
    }

    TEST_CASE("raise 的 value_ 会被折叠") {
        CHECK(
            fold_json(U"raise (1 + 1)") ==
            nlohmann::json{{"type", "Raise"}, {"value", int_lit("2")}}
        );
    }

    TEST_CASE("含变量/调用的子表达式不折，只递归折内部能折的部分") {
        CHECK(
            fold_json(U"try a except (E) b finally (x + 1)")["finally_expr"] ==
            nlohmann::json{
                {"type", "OpBinary"},
                {"op", "+"},
                {"left", {{"type", "Identifier"}, {"identifier", "x"}}},
                {"right", int_lit("1")}
            }
        );
    }
}
