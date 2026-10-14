// 赋值 / 复合赋值。左值合法性由语义层检查。
#include "../../../cppexceptions/SyntaxError.h"
#include "../test_utils.h"

#include <string>

#include <doctest/doctest.h>

TEST_SUITE("赋值与复合赋值") {

    TEST_CASE("基本赋值") {
        CHECK(
            parse_json(U"a = 1") == nlohmann::json{
                                        {"type", "Assign"},
                                        {"target", ident("a")},
                                        {"annotation", {{"is_const", false}, {"type", nullptr}}},
                                        {"value", int_lit("1")}
                                    }
        );
    }

    TEST_CASE("赋值右结合：a = b = c 即 a = (b = c)") {
        CHECK(
            parse_json(U"a = b = c") ==
            nlohmann::json{
                {"type", "Assign"},
                {"target", ident("a")},
                {"annotation", {{"is_const", false}, {"type", nullptr}}},
                {"value",
                 {{"type", "Assign"},
                  {"target", ident("b")},
                  {"annotation", {{"is_const", false}, {"type", nullptr}}},
                  {"value", ident("c")}}}
            }
        );
    }

    TEST_CASE("赋值优先级最低：a = 1 + 2 * 3") {
        CHECK(
            parse_json(U"a = 1 + 2 * 3") ==
            nlohmann::json{
                {"type", "Assign"},
                {"target", ident("a")},
                {"annotation", {{"is_const", false}, {"type", nullptr}}},
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
                {"annotation", {{"is_const", false}, {"type", nullptr}}},
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
                                           {"annotation", {{"is_const", false}, {"type", nullptr}}},
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
                {"annotation", {{"is_const", false}, {"type", nullptr}}},
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
                {"annotation", {{"is_const", false}, {"type", nullptr}}},
                {"value",
                 {{"type", "LiteralList"},
                  {"items", nlohmann::json::array({int_lit("1"), int_lit("2"), int_lit("3")})}}}
            }
        );
    }

    TEST_CASE("零个左值的解构：() = x / [] = x 语法上合法（跟 Python 一致，语义层不拦）") {
        CHECK(
            parse_json(U"() = x") ==
            nlohmann::json{
                {"type", "Assign"},
                {"target", {{"type", "LiteralTuple"}, {"items", nlohmann::json::array()}}},
                {"annotation", {{"is_const", false}, {"type", nullptr}}},
                {"value", ident("x")}
            }
        );
        CHECK(
            parse_json(U"[] = x") ==
            nlohmann::json{
                {"type", "Assign"},
                {"target", {{"type", "LiteralList"}, {"items", nlohmann::json::array()}}},
                {"annotation", {{"is_const", false}, {"type", nullptr}}},
                {"value", ident("x")}
            }
        );
    }

    TEST_CASE("没有无括号元组：a, b = x 在逗号处缺分隔符") {
        CHECK_THROWS_AS(parse_program(U"a, b = x"), SyntaxError);
    }
}

TEST_SUITE("赋值与复合赋值——调用实参里") {

    // 实参开头的 `identifier =` 是按关键字传参的实参，其余赋值写法都不能直接当实参，要再套一层括号

    TEST_CASE("identifier = expr 是按关键字传参的实参") {
        CHECK(
            parse_json(U"f(x = 1)") ==
            nlohmann::json{
                {"type", "Call"},
                {"object", ident("f")},
                {"positional_args", nlohmann::json::array()},
                {"keyword_args",
                 nlohmann::json::array({{{"keyword", "x"}, {"value", int_lit("1")}}})}
            }
        );
    }

    TEST_CASE("别的赋值写法直接当实参报错") {
        const std::string msg{"an assignment cannot be a call argument; wrap it in parentheses"};
        check_parse_throws_with(U"f(a.b = 1)", msg);
        check_parse_throws_with(U"f(a[0] = 1)", msg);
        check_parse_throws_with(U"f((a, b) = x)", msg);
        check_parse_throws_with(U"f(x += 1)", msg);
        check_parse_throws_with(U"f(a, x -= 1)", msg);
        check_parse_throws_with(U"f(x: int = 1)", msg);
        // 按关键字传参的实参的值里同样不能直接写
        check_parse_throws_with(U"f(k = x = 1)", msg);
        check_parse_throws_with(U"f(k = x += 1)", msg);
        // import(...)、eval(...) 的实参同理
        check_parse_throws_with(U"import(a.b = 1)", msg);
        check_parse_throws_with(U"eval(x += 1)", msg);
    }

    TEST_CASE("只看有没有被实参内的括号包住，嵌在别的表达式里也不行") {
        const std::string msg{"an assignment cannot be a call argument"};
        check_parse_throws_with(U"f(if (c) x = 1)", msg);
        check_parse_throws_with(U"f(@d x = 1)", msg);
    }

    TEST_CASE("加一层括号就是普通的赋值表达式，按位置传参") {
        CHECK(
            parse_json(U"f((a.b = 1))")["positional_args"][0] ==
            bare_assign(
                nlohmann::json{{"type", "Attr"}, {"object", ident("a")}, {"attr", "b"}},
                int_lit("1")
            )
        );
        CHECK(
            parse_json(U"f((x = 1))")["positional_args"][0] == bare_assign(ident("x"), int_lit("1"))
        );
        CHECK(parse_json(U"f((x += 1))")["positional_args"][0]["type"] == "CompoundAssign");
        CHECK(parse_json(U"f(k = (x = 1))")["keyword_args"][0]["value"]["type"] == "Assign");
        // 实参里的列表、函数体等新开的括号里照常可写
        CHECK(parse_json(U"f([x = 1])")["positional_args"][0]["items"][0]["type"] == "Assign");
        CHECK(
            parse_json(
                U"f(func () { x += 1 })"
            )["positional_args"][0]["body"]["exprs"][0]["type"] == "CompoundAssign"
        );
        CHECK(parse_json(U"f(if ((x = c)) y)")["positional_args"][0]["type"] == "If");
    }
}
