// StaticEvaler/LiteralFolder：str/tuple/list 的 +（拼接）/*（重复），SL.md 3.4.2。
// dict 的一切运算（含 |）不参与折叠——本质上依赖 VM 才能算，见 StaticEvaler.h 类注释。
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("StaticEvaler 容器运算") {

    TEST_CASE("str 拼接与重复") {
        CHECK(fold_json(U"'ab' + 'cd'") == str_lit("abcd"));
        CHECK(fold_json(U"'ab' * 3") == str_lit("ababab"));
        CHECK(fold_json(U"3 * 'ab'") == str_lit("ababab"));
        CHECK(fold_json(U"'x' * 0") == str_lit(""));
    }

    TEST_CASE("tuple 拼接与重复") {
        CHECK(
            fold_json(U"(1,) + (2, 3)") ==
            nlohmann::json{{"type", "LiteralTuple"},
                           {"items", {int_lit("1"), int_lit("2"), int_lit("3")}}}
        );
        CHECK(
            fold_json(U"(1, 2) * 2") ==
            nlohmann::json{{"type", "LiteralTuple"},
                           {"items", {int_lit("1"), int_lit("2"), int_lit("1"), int_lit("2")}}}
        );
    }

    TEST_CASE("list 拼接与重复，且重复出来的每一份是独立拷贝（不是同一个节点）") {
        CHECK(
            fold_json(U"[1, 2] + [3]") ==
            nlohmann::json{{"type", "LiteralList"},
                           {"items", {int_lit("1"), int_lit("2"), int_lit("3")}}}
        );
        CHECK(
            fold_json(U"[0] * 3") ==
            nlohmann::json{{"type", "LiteralList"},
                           {"items", {int_lit("0"), int_lit("0"), int_lit("0")}}}
        );
    }

    TEST_CASE("元素里含变量的容器不算纯字面量，+/* 不折") {
        CHECK(
            fold_json(U"[x, 1] + [2]") ==
            nlohmann::json{
                {"type", "OpBinary"},
                {"op", "+"},
                {"left",
                 {{"type", "LiteralList"},
                  {"items", {{{"type", "Identifier"}, {"identifier", "x"}}, int_lit("1")}}}},
                {"right", {{"type", "LiteralList"}, {"items", {int_lit("2")}}}}}
        );
    }

    TEST_CASE("负数重复次数不折，交给运行时报错") {
        CHECK(
            fold_json(U"[1] * -1") ==
            nlohmann::json{{"type", "OpBinary"},
                           {"op", "*"},
                           {"left", {{"type", "LiteralList"}, {"items", {int_lit("1")}}}},
                           {"right", int_lit("-1")}}
        );
    }

    TEST_CASE("类型不匹配的 + 不折（tuple 和 list 不能互相拼接）") {
        CHECK(
            fold_json(U"(1,) + [2]") ==
            nlohmann::json{{"type", "OpBinary"},
                           {"op", "+"},
                           {"left", {{"type", "LiteralTuple"}, {"items", {int_lit("1")}}}},
                           {"right", {{"type", "LiteralList"}, {"items", {int_lit("2")}}}}}
        );
    }

    TEST_CASE("dict 的 | 不折，交给运行时") {
        CHECK(
            fold_json(U"{1: 2} | {1: 3, 4: 5}") ==
            nlohmann::json{
                {"type", "OpBinary"},
                {"op", "|"},
                {"left",
                 {{"type", "LiteralDict"},
                  {"items", {nlohmann::json{{"key", int_lit("1")}, {"val", int_lit("2")}}}}}},
                {"right",
                 {{"type", "LiteralDict"},
                  {"items",
                   {nlohmann::json{{"key", int_lit("1")}, {"val", int_lit("3")}},
                    nlohmann::json{{"key", int_lit("4")}, {"val", int_lit("5")}}}}}}}
        );
    }
}
