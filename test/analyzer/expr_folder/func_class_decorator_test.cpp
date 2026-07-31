// ExprFolder：AstNodeFunc/AstNodeClass/AstNodeDecorator 各子槽位的子表达式折叠（SL.md
// 2.2.6/2.2.7/2.2.8）。func/class 自身、decorator 自身都不参与整体折叠（不在 StaticEvaler::fold
// 的分发范围内），这里只关心各个子槽位（形参默认值/类型注解、返回类型注解、捕获列表的
// value_expr_、decorators_/bases_、装饰器自己的 decorator_/target_）会不会被递归折叠。
// func/class 的 body_ 剪枝已经在 program_prune_test.cpp 覆盖，这里不重复。
#include "../test_utils.h"

#include <doctest/doctest.h>

namespace {
nlohmann::json ident(const char *name) {
    return nlohmann::json{{"type", "Identifier"}, {"identifier", name}};
}
} // namespace

TEST_SUITE("ExprFolder func 各槽位折叠") {

    TEST_CASE("形参的 default_value_ 会被折叠") {
        const auto result = fold_json(U"func f(x = 1 + 1) {}");
        CHECK(result["params"]["positional"][0]["default_value"] == int_lit("2"));
    }

    TEST_CASE("形参的 type_annotation_ 会被折叠") {
        const auto result = fold_json(U"func f(x: 1 + 1 = 0) {}");
        CHECK(result["params"]["positional"][0]["type_annotation"] == int_lit("2"));
    }

    TEST_CASE("*args 之后的仅关键字形参，type_annotation_/default_value_ 同样会被折叠") {
        const auto result = fold_json(U"func f(*a, x: 1 + 1 = 2 + 2) {}");
        CHECK(result["params"]["kw_only"][0]["type_annotation"] == int_lit("2"));
        CHECK(result["params"]["kw_only"][0]["default_value"] == int_lit("4"));
    }

    TEST_CASE("return_type_ 会被折叠") {
        CHECK(fold_json(U"func f(): 1 + 1 {}")["return_type"] == int_lit("2"));
    }

    TEST_CASE("捕获列表的 value_expr_ 会被折叠") {
        const auto result = fold_json(U"func f[x = 1 + 1]() {}");
        CHECK(result["captures"][0]["value_expr"] == int_lit("2"));
    }

    TEST_CASE("decorators_ 里每个装饰器表达式的子树都会被折叠") {
        const auto result = fold_json(U"@dec(1 + 1) func f() {}");
        CHECK(result["decorators"][0]["positional_args"][0] == int_lit("2"));
    }

    TEST_CASE("含变量的默认值不折，只递归折内部能折的部分") {
        const auto result = fold_json(U"func f(x = y + 1) {}");
        CHECK(
            result["params"]["positional"][0]["default_value"] ==
            nlohmann::json{
                {"type", "OpBinary"}, {"op", "+"}, {"left", ident("y")}, {"right", int_lit("1")}
            }
        );
    }
}

TEST_SUITE("ExprFolder class 各槽位折叠") {

    TEST_CASE("bases_ 里每个基类表达式都会被折叠") {
        CHECK(
            fold_json(U"class C(1 + 1, 2 + 2) {}")["bases"] == nlohmann::json::array({
                                                                   int_lit("2"),
                                                                   int_lit("4"),
                                                               })
        );
    }

    TEST_CASE("捕获列表的 value_expr_ 会被折叠") {
        const auto result = fold_json(U"class C[x = 1 + 1] {}");
        CHECK(result["captures"][0]["value_expr"] == int_lit("2"));
    }

    TEST_CASE("decorators_ 里每个装饰器表达式的子树都会被折叠") {
        const auto result = fold_json(U"@dec(1 + 1) class C {}");
        CHECK(result["decorators"][0]["positional_args"][0] == int_lit("2"));
    }
}

TEST_SUITE("ExprFolder 装饰器（通用形式）折叠") {

    TEST_CASE("decorator_ 的子树会被折叠") {
        const auto result = fold_json(U"@dec(1 + 1) x");
        CHECK(result["decorator"]["positional_args"][0] == int_lit("2"));
    }

    TEST_CASE("target_ 会被折叠") { CHECK(fold_json(U"@dec 1 + 1")["target"] == int_lit("2")); }

    TEST_CASE("target_ 是复合表达式时，一样先递归折叠再套用复合表达式规则") {
        // { 1; 2 } 全是字面量，折成最后一条 2
        CHECK(fold_json(U"@dec { 1; 2 }")["target"] == int_lit("2"));
    }
}
