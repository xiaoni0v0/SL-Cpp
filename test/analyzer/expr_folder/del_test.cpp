// del 目标内部会折（如下标）。
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("ExprFolder del") {

    TEST_CASE("target 内部折叠") {
        const auto result = fold_json(U"del b[1 + 1]");
        CHECK(result["type"] == "Del");
        CHECK(result["target"]["type"] == "Index");
        CHECK(result["target"]["args"] == nlohmann::json::array({int_lit("2")}));
    }
}
