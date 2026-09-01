// 迭代 for 的子槽位会折；节点整体不折。
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("ExprFolder 迭代 for") {

    TEST_CASE("iterable / target 下标 / body 都会折") {
        const auto it = fold_json(U"for ('a' + 'b') x");
        CHECK(it["type"] == "ForIter");
        CHECK(it["iterable"] == str_lit("ab"));
        const auto tgt = fold_json(U"for (xs as a[1 + 1]) y");
        CHECK(tgt["target"]["type"] == "Index");
        CHECK(tgt["target"]["args"] == nlohmann::json::array({int_lit("2")}));
        const auto body = fold_json(U"for (xs) (1 + 1)");
        CHECK(body["body"] == int_lit("2"));
    }

    TEST_CASE("没有 as 时 target 为空") {
        const auto result = fold_json(U"for (1 + 1) (2 + 2)");
        CHECK(result["target"] == nullptr);
        CHECK(result["iterable"] == int_lit("2"));
        CHECK(result["body"] == int_lit("4"));
    }

    TEST_CASE("整体不折，空字面量也不消") {
        CHECK(fold_json(U"for ([]) x")["type"] == "ForIter");
        CHECK(fold_json(U"for ('') x")["type"] == "ForIter");
        CHECK(fold_json(U"for (0 as x) y")["type"] == "ForIter");
    }
}
