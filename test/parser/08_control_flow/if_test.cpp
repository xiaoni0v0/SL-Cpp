// if / elif / else。cond 禁止裸 `=`，复合赋值可以。
#include "../../../diagnostics/SyntaxError.h"
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("if——基本形式") {

    TEST_CASE("无 else") {
        CHECK(
            parse_json(U"if (a) b") ==
            nlohmann::json{
                {"type", "If"},
                {"clauses", nlohmann::json::array({{{"cond", ident("a")}, {"body", ident("b")}}})},
                {"else_expr", nullptr}
            }
        );
    }

    TEST_CASE("带 else") {
        CHECK(
            parse_json(U"if (a) b else c") ==
            nlohmann::json{
                {"type", "If"},
                {"clauses", nlohmann::json::array({{{"cond", ident("a")}, {"body", ident("b")}}})},
                {"else_expr", ident("c")}
            }
        );
    }

    TEST_CASE("多个 elif，无 else") {
        CHECK(
            parse_json(U"if (a) x elif (b) y elif (c) z") ==
            nlohmann::json{
                {"type", "If"},
                {"clauses",
                 nlohmann::json::array(
                     {{{"cond", ident("a")}, {"body", ident("x")}},
                      {{"cond", ident("b")}, {"body", ident("y")}},
                      {{"cond", ident("c")}, {"body", ident("z")}}}
                 )},
                {"else_expr", nullptr}
            }
        );
    }

    TEST_CASE("elif 加 else") {
        CHECK(
            parse_json(U"if (a) x elif (b) y else z") ==
            nlohmann::json{
                {"type", "If"},
                {"clauses",
                 nlohmann::json::array(
                     {{{"cond", ident("a")}, {"body", ident("x")}},
                      {{"cond", ident("b")}, {"body", ident("y")}}}
                 )},
                {"else_expr", ident("z")}
            }
        );
    }

    TEST_CASE("cond 可以是任意表达式，比如比较") {
        CHECK(
            parse_json(U"if (x == 1) y") ==
            nlohmann::json{
                {"type", "If"},
                {"clauses",
                 nlohmann::json::array(
                     {{{"cond",
                        {{"type", "Compare"},
                         {"operands", nlohmann::json::array({ident("x"), int_lit("1")})},
                         {"ops", nlohmann::json::array({"=="})}}},
                       {"body", ident("y")}}}
                 )},
                {"else_expr", nullptr}
            }
        );
    }

    TEST_CASE("缺少括号/未闭合括号都要报错") {
        CHECK_THROWS_AS(parse_as_file(U"if a) b"), SyntaxError);
        CHECK_THROWS_AS(parse_as_file(U"if (a b"), SyntaxError);
        CHECK_THROWS_AS(parse_as_file(U"if (a"), SyntaxError);
    }

    TEST_CASE("缺少 body 报错") { CHECK_THROWS_AS(parse_as_file(U"if (a)"), SyntaxError); }

    TEST_CASE("嵌套 if 的 else 绑到内层：if (a) if (b) 1 else 2") {
        CHECK(
            parse_json(U"if (a) if (b) 1 else 2") ==
            nlohmann::json{
                {"type", "If"},
                {"clauses",
                 nlohmann::json::array(
                     {{{"cond", ident("a")},
                       {"body",
                        {{"type", "If"},
                         {"clauses",
                          nlohmann::json::array({{{"cond", ident("b")}, {"body", int_lit("1")}}})},
                         {"else_expr", int_lit("2")}}}}}
                 )},
                {"else_expr", nullptr}
            }
        );
    }
}

TEST_SUITE("if——cond 槽禁止裸的普通赋值") {

    TEST_CASE("裸 = 报错，提示加括号，位置指向 '='") {
        try {
            parse_as_file(U"if (x = 1) y");
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("bare '='") != std::string::npos);
            CHECK(msg.find("add parentheses") != std::string::npos);
            CHECK(msg.find("1:7:") != std::string::npos); // '='
        }
    }

    TEST_CASE("显式再套一层括号就允许：if ((x = 1)) y") {
        CHECK(
            parse_json(U"if ((x = 1)) y") ==
            nlohmann::json{
                {"type", "If"},
                {"clauses",
                 nlohmann::json::array(
                     {{{"cond",
                        {{"type", "Assign"}, {"target", ident("x")}, {"value", int_lit("1")}}},
                       {"body", ident("y")}}}
                 )},
                {"else_expr", nullptr}
            }
        );
    }

    TEST_CASE("裸的复合赋值不受限制，可以直接写：if (x += 1) y") {
        CHECK(
            parse_json(U"if (x += 1) y") == nlohmann::json{
                                                {"type", "If"},
                                                {"clauses",
                                                 nlohmann::json::array(
                                                     {{{"cond",
                                                        {{"type", "CompoundAssign"},
                                                         {"target", ident("x")},
                                                         {"op", "+"},
                                                         {"value", int_lit("1")}}},
                                                       {"body", ident("y")}}}
                                                 )},
                                                {"else_expr", nullptr}
                                            }
        );
    }

    TEST_CASE("elif 的 cond 槽同样禁止裸赋值") {
        CHECK_THROWS_AS(parse_as_file(U"if (a) x elif (y = 1) z"), SyntaxError);
    }

    TEST_CASE("比较运算不受影响（== 不是赋值）") { CHECK_NOTHROW(parse_as_file(U"if (x == 1) y")); }
}
