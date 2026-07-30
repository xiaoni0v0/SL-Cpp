// StaticEvaler/ExprFolder：not/and/or（SL.md 3.2 真值规则、3.4.2 逻辑运算符语义）。
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("StaticEvaler 逻辑运算") {

    TEST_CASE("not 恒产出 bool") {
        CHECK(fold_json(U"not True") == bool_lit(false));
        CHECK(fold_json(U"not 0") == bool_lit(true));
        CHECK(fold_json(U"not ''") == bool_lit(true));
        CHECK(fold_json(U"not [1]") == bool_lit(false));
    }

    TEST_CASE("真值规则：None/False/0/0.0/''/()/[]/dict() 为假，其余为真") {
        CHECK(fold_json(U"not None") == bool_lit(true));
        CHECK(fold_json(U"not 0.0") == bool_lit(true));
        CHECK(fold_json(U"not ()") == bool_lit(true));
        CHECK(fold_json(U"not (1,)") == bool_lit(false));
        CHECK(fold_json(U"not ...") == bool_lit(false)); // Ellipsis 不在假值列表里
    }

    TEST_CASE("and：左真则取右（原样返回，不一定是 bool），左假则取左") {
        CHECK(fold_json(U"True and 5") == int_lit("5"));
        CHECK(fold_json(U"0 and 1") == int_lit("0"));
        CHECK(fold_json(U"1 and 2 and 3") == int_lit("3"));
    }

    TEST_CASE("or：左真则取左，左假则取右") {
        CHECK(fold_json(U"False or 5") == int_lit("5"));
        CHECK(fold_json(U"1 or 2") == int_lit("1"));
        CHECK(
            fold_json(U"[] or [1]") ==
            nlohmann::json{{"type", "LiteralList"}, {"items", {int_lit("1")}}}
        );
    }

    TEST_CASE("and/or 不短路地各自折叠两边，被选中的一侧即使是复杂表达式也整体挪走") {
        // 右边 f() 不是字面量，但因为左边 True 为真，and 直接取右边（不需要右边本身可折）
        CHECK(
            fold_json(U"True and f()") ==
            nlohmann::json{
                {"type", "Call"},
                {"object", {{"type", "Identifier"}, {"identifier", "f"}}},
                {"positional_args", nlohmann::json::array()},
                {"keyword_args", nlohmann::json::array()}
            }
        );
        // 左边 False 为假，or 取右边（哪怕右边是没法预知结果的调用）
        CHECK(
            fold_json(U"False or f()") ==
            nlohmann::json{
                {"type", "Call"},
                {"object", {{"type", "Identifier"}, {"identifier", "f"}}},
                {"positional_args", nlohmann::json::array()},
                {"keyword_args", nlohmann::json::array()}
            }
        );
    }

    TEST_CASE("左操作数含变量、真值未知时不折") {
        CHECK(
            fold_json(U"x and 1") == nlohmann::json{
                                         {"type", "OpBinary"},
                                         {"op", "and"},
                                         {"left", {{"type", "Identifier"}, {"identifier", "x"}}},
                                         {"right", int_lit("1")}
                                     }
        );
    }
}
