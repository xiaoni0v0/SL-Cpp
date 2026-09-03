// raise expr，表达式不可省。
#include "../../../cpp_exceptions/SyntaxError.h"
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("raise") {

    TEST_CASE("基本形式") {
        CHECK(
            parse_json(U"raise e") ==
            nlohmann::json{
                {"type", "Raise"}, {"value", {{"type", "Identifier"}, {"identifier", "e"}}}
            }
        );
    }

    TEST_CASE("value 可以是构造调用等复杂表达式") {
        CHECK(
            parse_json(U"raise Exception('msg')") ==
            nlohmann::json{
                {"type", "Raise"},
                {"value",
                 {{"type", "Call"},
                  {"object", {{"type", "Identifier"}, {"identifier", "Exception"}}},
                  {"positional_args",
                   nlohmann::json::array({{{"type", "LiteralStr"}, {"value", "msg"}}})},
                  {"keyword_args", nlohmann::json::array()}}}
            }
        );
    }

    TEST_CASE("不同于 return，raise 后面的表达式不可省略") {
        CHECK_THROWS_AS(parse_as_file(U"raise"), SyntaxError);
        CHECK_THROWS_AS(parse_as_file(U"raise;"), SyntaxError);
    }
}
