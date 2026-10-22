// 前缀一元运算与后缀问号、感叹号。
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("一元运算符与后缀问号感叹号") {

    TEST_CASE("+x -x ~x") {
        CHECK(
            parse_json(U"+1") ==
            nlohmann::json{{"type", "OpUnary"}, {"op", "+"}, {"operand", int_lit("1")}}
        );
        CHECK(
            parse_json(U"-1") ==
            nlohmann::json{{"type", "OpUnary"}, {"op", "-"}, {"operand", int_lit("1")}}
        );
        CHECK(
            parse_json(U"~1") ==
            nlohmann::json{{"type", "OpUnary"}, {"op", "~"}, {"operand", int_lit("1")}}
        );
    }

    TEST_CASE("一元运算符可以连写（不同符号之间）：+-1 == +(-1)") {
        CHECK(
            parse_json(U"+-1") ==
            nlohmann::json{
                {"type", "OpUnary"},
                {"op", "+"},
                {"operand", {{"type", "OpUnary"}, {"op", "-"}, {"operand", int_lit("1")}}}
            }
        );
    }

    TEST_CASE("x? / x!") {
        CHECK(
            parse_json(U"x?") == nlohmann::json{
                                     {"type", "OpUnary"},
                                     {"op", "?"},
                                     {"operand", {{"type", "Identifier"}, {"identifier", "x"}}}
                                 }
        );
        CHECK(
            parse_json(U"x!") == nlohmann::json{
                                     {"type", "OpUnary"},
                                     {"op", "!"},
                                     {"operand", {{"type", "Identifier"}, {"identifier", "x"}}}
                                 }
        );
    }

    TEST_CASE("? ! 可以连续叠加，左结合：x?! == (x?)!") {
        CHECK(
            parse_json(U"x?!") == nlohmann::json{
                                      {"type", "OpUnary"},
                                      {"op", "!"},
                                      {"operand",
                                       {{"type", "OpUnary"},
                                        {"op", "?"},
                                        {"operand", {{"type", "Identifier"}, {"identifier", "x"}}}}}
                                  }
        );
    }
}
