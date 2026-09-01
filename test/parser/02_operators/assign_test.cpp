// 赋值 / 复合赋值。左值合法性由语义层检查。
#include "../../../builtins/exceptions/SyntaxError.h"
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("赋值与复合赋值") {

    TEST_CASE("基本赋值") {
        CHECK(
            parse_json(U"a = 1") ==
            nlohmann::json{{"type", "Assign"}, {"target", ident("a")}, {"value", int_lit("1")}}
        );
    }

    TEST_CASE("赋值右结合：a = b = c 即 a = (b = c)") {
        CHECK(
            parse_json(U"a = b = c") ==
            nlohmann::json{
                {"type", "Assign"},
                {"target", ident("a")},
                {"value", {{"type", "Assign"}, {"target", ident("b")}, {"value", ident("c")}}}
            }
        );
    }

    TEST_CASE("赋值优先级最低：a = 1 + 2 * 3") {
        CHECK(
            parse_json(U"a = 1 + 2 * 3") == nlohmann::json{
                                                {"type", "Assign"},
                                                {"target", ident("a")},
                                                {"value",
                                                 {{"type", "OpBinary"},
                                                  {"op", "+"},
                                                  {"left", int_lit("1")},
                                                  {"right",
                                                   {{"type", "OpBinary"},
                                                    {"op", "*"},
                                                    {"left", int_lit("2")},
                                                    {"right", int_lit("3")}}}}}
                                            }
        );
    }

    TEST_CASE("11 种复合赋值：op 存底层二元运算符，不含 =") {
        CHECK(
            parse_json(U"a += 1") == nlohmann::json{
                                         {"type", "CompoundAssign"},
                                         {"target", ident("a")},
                                         {"op", "+"},
                                         {"value", int_lit("1")}
                                     }
        );
        CHECK(
            parse_json(U"a -= 1") == nlohmann::json{
                                         {"type", "CompoundAssign"},
                                         {"target", ident("a")},
                                         {"op", "-"},
                                         {"value", int_lit("1")}
                                     }
        );
        CHECK(
            parse_json(U"a *= 1") == nlohmann::json{
                                         {"type", "CompoundAssign"},
                                         {"target", ident("a")},
                                         {"op", "*"},
                                         {"value", int_lit("1")}
                                     }
        );
        CHECK(
            parse_json(U"a **= 1") == nlohmann::json{
                                          {"type", "CompoundAssign"},
                                          {"target", ident("a")},
                                          {"op", "**"},
                                          {"value", int_lit("1")}
                                      }
        );
        CHECK(
            parse_json(U"a /= 1") == nlohmann::json{
                                         {"type", "CompoundAssign"},
                                         {"target", ident("a")},
                                         {"op", "/"},
                                         {"value", int_lit("1")}
                                     }
        );
        CHECK(
            parse_json(U"a //= 1") == nlohmann::json{
                                          {"type", "CompoundAssign"},
                                          {"target", ident("a")},
                                          {"op", "//"},
                                          {"value", int_lit("1")}
                                      }
        );
        CHECK(
            parse_json(U"a %= 1") == nlohmann::json{
                                         {"type", "CompoundAssign"},
                                         {"target", ident("a")},
                                         {"op", "%"},
                                         {"value", int_lit("1")}
                                     }
        );
        CHECK(
            parse_json(U"a &= 1") == nlohmann::json{
                                         {"type", "CompoundAssign"},
                                         {"target", ident("a")},
                                         {"op", "&"},
                                         {"value", int_lit("1")}
                                     }
        );
        CHECK(
            parse_json(U"a |= 1") == nlohmann::json{
                                         {"type", "CompoundAssign"},
                                         {"target", ident("a")},
                                         {"op", "|"},
                                         {"value", int_lit("1")}
                                     }
        );
        CHECK(
            parse_json(U"a ^= 1") == nlohmann::json{
                                         {"type", "CompoundAssign"},
                                         {"target", ident("a")},
                                         {"op", "^"},
                                         {"value", int_lit("1")}
                                     }
        );
        CHECK(
            parse_json(U"a <<= 1") == nlohmann::json{
                                          {"type", "CompoundAssign"},
                                          {"target", ident("a")},
                                          {"op", "<<"},
                                          {"value", int_lit("1")}
                                      }
        );
        CHECK(
            parse_json(U"a >>= 1") == nlohmann::json{
                                          {"type", "CompoundAssign"},
                                          {"target", ident("a")},
                                          {"op", ">>"},
                                          {"value", int_lit("1")}
                                      }
        );
    }

    TEST_CASE("复合赋值右结合：a += b += c") {
        CHECK(
            parse_json(U"a += b += c") == nlohmann::json{
                                              {"type", "CompoundAssign"},
                                              {"target", ident("a")},
                                              {"op", "+"},
                                              {"value",
                                               {{"type", "CompoundAssign"},
                                                {"target", ident("b")},
                                                {"op", "+"},
                                                {"value", ident("c")}}}
                                          }
        );
    }

    TEST_CASE("目标可以是属性/索引（是否左值由语义层判）") {
        CHECK(
            parse_json(U"a.b = 1") ==
            nlohmann::json{
                {"type", "Assign"},
                {"target", {{"type", "Attr"}, {"object", ident("a")}, {"attr", "b"}}},
                {"value", int_lit("1")}
            }
        );
        CHECK(
            parse_json(U"a[0] = 1") == nlohmann::json{
                                           {"type", "Assign"},
                                           {"target",
                                            {{"type", "Index"},
                                             {"object", ident("a")},
                                             {"args", nlohmann::json::array({int_lit("0")})}}},
                                           {"value", int_lit("1")}
                                       }
        );
    }

    TEST_CASE("解构赋值：语法层就是元组/列表字面量当左值") {
        CHECK(
            parse_json(U"(a, b) = (1, 2)") ==
            nlohmann::json{
                {"type", "Assign"},
                {"target",
                 {{"type", "LiteralTuple"},
                  {"items", nlohmann::json::array({ident("a"), ident("b")})}}},
                {"value",
                 {{"type", "LiteralTuple"},
                  {"items", nlohmann::json::array({int_lit("1"), int_lit("2")})}}}
            }
        );
        CHECK(
            parse_json(U"[a, *b] = [1, 2, 3]") ==
            nlohmann::json{
                {"type", "Assign"},
                {"target",
                 {{"type", "LiteralList"},
                  {"items",
                   nlohmann::json::array(
                       {ident("a"), {{"type", "Star"}, {"operand", ident("b")}}}
                   )}}},
                {"value",
                 {{"type", "LiteralList"},
                  {"items", nlohmann::json::array({int_lit("1"), int_lit("2"), int_lit("3")})}}}
            }
        );
    }

    TEST_CASE("没有无括号元组：a, b = x 在逗号处缺分隔符") {
        CHECK_THROWS_AS(parse_as_file(U"a, b = x"), SyntaxError);
    }
}
