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
            parse_json(U"${**a, 'b' -> 1}") ==
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

    TEST_CASE("*/** 展开的操作数按单目一档解析") {
        // 属性访问（170）先结合进操作数：*a.b 即 *(a.b)
        CHECK(
            parse_json(U"[*a.b]")["items"][0] ==
            nlohmann::json{
                {"type", "Star"},
                {"operand",
                 {{"type", "Attr"},
                  {"object", {{"type", "Identifier"}, {"identifier", "a"}}},
                  {"attr", "b"}}}
            }
        );
        // 调用（170）先结合进操作数：*f(x) 即 *(f(x))
        CHECK(parse_json(U"[*f(x)]")["items"][0]["operand"]["type"] == "Call");
        // 幂（150）也先结合进操作数：**d ** e 即 **(d ** e)
        CHECK(
            parse_json(U"${**d ** e}")["items"][0]["key"] ==
            nlohmann::json{
                {"type", "DoubleStar"},
                {"operand",
                 {{"type", "OpBinary"},
                  {"op", "**"},
                  {"left", {{"type", "Identifier"}, {"identifier", "d"}}},
                  {"right", {{"type", "Identifier"}, {"identifier", "e"}}}}}
            }
        );
        // 低于 140 的（如加法 120）不结合进操作数：*a + b 即 (*a) + b（合法性归语义层判）
        CHECK(
            parse_json(U"*a + b") ==
            nlohmann::json{
                {"type", "OpBinary"},
                {"op", "+"},
                {"left",
                 {{"type", "Star"}, {"operand", {{"type", "Identifier"}, {"identifier", "a"}}}}},
                {"right", {{"type", "Identifier"}, {"identifier", "b"}}}
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
