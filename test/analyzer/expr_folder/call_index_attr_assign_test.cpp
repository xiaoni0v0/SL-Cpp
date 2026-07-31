// ExprFolder：Call/Index/Attr/Assign/CompoundAssign 各子槽位的子表达式折叠。
// 这几种节点自身都不参与整体折叠（不在 StaticEvaler::fold 的分发范围内——调用/索引/属性访问
// 可能有副作用或依赖运行时状态，赋值本身就是副作用），但里面夹着的子表达式该折还是要递归折。
#include "../test_utils.h"

#include <doctest/doctest.h>

namespace {
nlohmann::json ident(const char *name) {
    return nlohmann::json{{"type", "Identifier"}, {"identifier", name}};
}
} // namespace

TEST_SUITE("ExprFolder Call 子表达式折叠") {

    TEST_CASE("object_ 会被折叠") { CHECK(fold_json(U"(1 + 1)()")["object"] == int_lit("2")); }

    TEST_CASE("positional_args_ 里每一项都会被折叠") {
        CHECK(
            fold_json(U"f(1 + 1, 2 + 2)")["positional_args"] ==
            nlohmann::json::array({int_lit("2"), int_lit("4")})
        );
    }

    TEST_CASE("*expr 展开的 operand_ 也会被折叠") {
        const auto result = fold_json(U"f(*(1 + 1,))");
        CHECK(
            result["positional_args"][0] ==
            nlohmann::json{
                {"type", "Star"}, {"operand", {{"type", "LiteralTuple"}, {"items", {int_lit("2")}}}}
            }
        );
    }

    TEST_CASE("keyword_args_ 的 value_ 会被折叠（普通关键字实参、**展开都一样）") {
        const auto result = fold_json(U"f(x = 1 + 1, **(1 + 1, 2 + 2))");
        CHECK(
            result["keyword_args"][0] == nlohmann::json{{"keyword", "x"}, {"value", int_lit("2")}}
        );
        CHECK(
            result["keyword_args"][1]["value"] ==
            nlohmann::json{
                {"type", "DoubleStar"},
                {"operand", {{"type", "LiteralTuple"}, {"items", {int_lit("2"), int_lit("4")}}}}
            }
        );
    }

    TEST_CASE("含变量的实参不折，只递归折内部能折的部分") {
        CHECK(
            fold_json(U"f(x + 1)")["positional_args"][0] ==
            nlohmann::json{
                {"type", "OpBinary"}, {"op", "+"}, {"left", ident("x")}, {"right", int_lit("1")}
            }
        );
    }
}

TEST_SUITE("ExprFolder Index/Attr 子表达式折叠") {

    TEST_CASE("Index 的 object_ 会被折叠") {
        CHECK(fold_json(U"(1 + 1)[0]")["object"] == int_lit("2"));
    }

    TEST_CASE("Index 的 args_ 里每一项都会被折叠") {
        CHECK(
            fold_json(U"a[1 + 1, 2 + 2]")["args"] ==
            nlohmann::json::array({int_lit("2"), int_lit("4")})
        );
    }

    TEST_CASE("Attr 的 object_ 会被折叠") {
        CHECK(fold_json(U"(1 + 1).x")["object"] == int_lit("2"));
    }
}

TEST_SUITE("ExprFolder Assign/CompoundAssign 子表达式折叠") {

    TEST_CASE("Assign 的 value_ 会被折叠") {
        CHECK(fold_json(U"x = 1 + 1")["value"] == int_lit("2"));
    }

    TEST_CASE("Assign 的目标是 Index/Attr 时，target_ 里的子表达式（比如索引参数）也会被折叠") {
        CHECK(
            fold_json(U"a[1 + 1] = 1")["target"]["args"] == nlohmann::json::array({int_lit("2")})
        );
    }

    TEST_CASE("解构赋值的 target_ 是元组/列表时，里面每一项也会被递归折叠") {
        CHECK(
            fold_json(U"(a[1 + 1], b) = x")["target"]["items"][0]["args"] ==
            nlohmann::json::array({int_lit("2")})
        );
    }

    TEST_CASE("CompoundAssign 的 value_ 会被折叠，op 不受影响") {
        const auto result = fold_json(U"x += 1 + 1");
        CHECK(result["op"] == "+");
        CHECK(result["value"] == int_lit("2"));
    }

    TEST_CASE("CompoundAssign 的目标是 Index/Attr 时，target_ 里的子表达式也会被折叠") {
        CHECK(
            fold_json(U"a[1 + 1] += 1")["target"]["args"] == nlohmann::json::array({int_lit("2")})
        );
    }
}
