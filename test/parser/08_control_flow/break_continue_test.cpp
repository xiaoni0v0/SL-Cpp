// break / continue。是否在循环里由语义层检查。
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("break / continue") {

    TEST_CASE("基本形式") {
        CHECK(parse_json(U"break") == nlohmann::json{{"type", "Break"}});
        CHECK(parse_json(U"continue") == nlohmann::json{{"type", "Continue"}});
    }

    TEST_CASE("语法层不检查是否处于循环体内，随便写在哪都能解析成功") {
        CHECK_NOTHROW(parse_as_file(U"break"));
        CHECK_NOTHROW(parse_as_file(U"continue"));
    }

    TEST_CASE("出现在循环体内") {
        CHECK(
            parse_json(U"while (c) break") ==
            nlohmann::json{
                {"type", "ForCond"},
                {"collect", "none"},
                {"init", nullptr},
                {"cond", {{"type", "Identifier"}, {"identifier", "c"}}},
                {"inc", nullptr},
                {"body", {{"type", "Break"}}}
            }
        );
    }
}
