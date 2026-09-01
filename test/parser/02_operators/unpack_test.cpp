// *expr / **expr 展开：语法形状。出现位置是否合法由语义层检查。
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("* / ** 展开") {

    TEST_CASE("*expr 在列表、元组里") {
        CHECK(
            parse_json(U"[*a, b]") ==
            nlohmann::json{
                {"type", "LiteralList"},
                {"items",
                 nlohmann::json::array({{{"type", "Star"}, {"operand", ident("a")}}, ident("b")})}
            }
        );
        CHECK(
            parse_json(U"(*a,)") ==
            nlohmann::json{
                {"type", "LiteralTuple"},
                {"items", nlohmann::json::array({{{"type", "Star"}, {"operand", ident("a")}}})}
            }
        );
    }

    TEST_CASE("**expr 在字典里，value 侧为 null") {
        CHECK(
            parse_json(U"{**a, 'b': 1}") ==
            nlohmann::json{
                {"type", "LiteralDict"},
                {"items",
                 nlohmann::json::array(
                     {{{"key", {{"type", "DoubleStar"}, {"operand", ident("a")}}},
                       {"value", nullptr}},
                      {{"key", {{"type", "LiteralStr"}, {"value", "b"}}}, {"value", int_lit("1")}}}
                 )}
            }
        );
    }

    TEST_CASE("*expr 也能出现在索引里（语法层放行，出现位置合不合法归语义层）") {
        CHECK(
            parse_json(U"a[*xs]") ==
            nlohmann::json{
                {"type", "Index"},
                {"object", ident("a")},
                {"args", nlohmann::json::array({{{"type", "Star"}, {"operand", ident("xs")}}})}
            }
        );
        CHECK(
            parse_json(U"a[i, *rest]") ==
            nlohmann::json{
                {"type", "Index"},
                {"object", ident("a")},
                {"args",
                 nlohmann::json::array(
                     {ident("i"), {{"type", "Star"}, {"operand", ident("rest")}}}
                 )}
            }
        );
    }

    TEST_CASE("操作数按单目一档解析，** 幂运算比它高") {
        CHECK(
            parse_json(U"[*a ** b]") == nlohmann::json{
                                            {"type", "LiteralList"},
                                            {"items",
                                             nlohmann::json::array(
                                                 {{{"type", "Star"},
                                                   {"operand",
                                                    {{"type", "OpBinary"},
                                                     {"op", "**"},
                                                     {"left", ident("a")},
                                                     {"right", ident("b")}}}}}
                                             )}
                                        }
        );
    }
}
