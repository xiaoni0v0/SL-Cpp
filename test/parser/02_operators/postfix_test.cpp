// 属性访问、调用、索引及其后缀组合。
#include "../../../cppexceptions/SyntaxError.h"
#include "../test_utils.h"

#include <string>

#include <doctest/doctest.h>

TEST_SUITE("后缀访问的基本形式与组合") {

    TEST_CASE("单独的调用/索引/属性访问") {
        CHECK(
            parse_json(U"f()") == nlohmann::json{
                                      {"type", "Call"},
                                      {"object", {{"type", "Identifier"}, {"identifier", "f"}}},
                                      {"positional_args", nlohmann::json::array()},
                                      {"keyword_args", nlohmann::json::array()}
                                  }
        );
        CHECK(
            parse_json(U"a[0]") == nlohmann::json{
                                       {"type", "Index"},
                                       {"object", {{"type", "Identifier"}, {"identifier", "a"}}},
                                       {"args", nlohmann::json::array({int_lit("0")})}
                                   }
        );
        CHECK(
            parse_json(U"a.b") == nlohmann::json{
                                      {"type", "Attr"},
                                      {"object", {{"type", "Identifier"}, {"identifier", "a"}}},
                                      {"attr", "b"}
                                  }
        );
    }

    TEST_CASE("属性访问后面必须是标识符") {
        CHECK_THROWS_AS(parse_program(U"a.1"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"a."), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"obj.for"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"obj.class"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"obj.if"), SyntaxError);
    }

    TEST_CASE("左结合链式：obj.attr[0](1, 2)?") {
        CHECK(
            parse_json(U"obj.attr[0](1, 2)?") ==
            nlohmann::json{
                {"type", "OpUnary"},
                {"op", "?"},
                {"operand",
                 {{"type", "Call"},
                  {"object",
                   {{"type", "Index"},
                    {"object",
                     {{"type", "Attr"},
                      {"object", {{"type", "Identifier"}, {"identifier", "obj"}}},
                      {"attr", "attr"}}},
                    {"args", nlohmann::json::array({int_lit("0")})}}},
                  {"positional_args", nlohmann::json::array({int_lit("1"), int_lit("2")})},
                  {"keyword_args", nlohmann::json::array()}}}
            }
        );
    }

    TEST_CASE(
        "未闭合的调用/索引抛异常，消息分别点名'function call'/'index expression'，位置指向 EOF"
    ) {
        // "f(1, 2" 共 6 个字符，EOF 在第 7 列
        try {
            parse_program(U"f(1, 2");
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("function call") != std::string::npos);
            CHECK(msg.find("1:7:") != std::string::npos);
        }
        // "a[0" 共 3 个字符，EOF 在第 4 列
        try {
            parse_program(U"a[0");
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("index expression") != std::string::npos);
            CHECK(msg.find("1:4:") != std::string::npos);
        }
    }
}

TEST_SUITE("调用实参") {

    TEST_CASE("调用带位置参数和关键字参数") {
        CHECK(
            parse_json(U"f(1, 2, x=3)") ==
            nlohmann::json{
                {"type", "Call"},
                {"object", {{"type", "Identifier"}, {"identifier", "f"}}},
                {"positional_args", nlohmann::json::array({int_lit("1"), int_lit("2")})},
                {"keyword_args",
                 nlohmann::json::array({nlohmann::json{{"keyword", "x"}, {"value", int_lit("3")}}})}
            }
        );
    }

    TEST_CASE("*expr 展开出现在位置组（args），**expr 展开出现在关键字组（kwargs）") {
        CHECK(
            parse_json(U"f(*args, **kwargs)") ==
            nlohmann::json{
                {"type", "Call"},
                {"object", {{"type", "Identifier"}, {"identifier", "f"}}},
                {"positional_args",
                 nlohmann::json::array(
                     {{{"type", "Star"},
                       {"operand", {{"type", "Identifier"}, {"identifier", "args"}}}}}
                 )},
                {"keyword_args",
                 nlohmann::json::array(
                     {{{"keyword", nullptr},
                       {"value",
                        {{"type", "DoubleStar"},
                         {"operand", {{"type", "Identifier"}, {"identifier", "kwargs"}}}}}}}
                 )}
            }
        );
    }

    TEST_CASE("按位置传参的实参不能出现在按关键字传参的实参之后，Parser 直接报语法错误") {
        check_parse_throws_with(
            U"f(a=1, 1)", "positional argument cannot appear after keyword argument"
        );
        check_parse_throws_with(
            U"f(**a, 1)", "positional argument cannot appear after keyword argument"
        );
        check_parse_throws_with(
            U"f(a=1, *b)", "positional argument cannot appear after keyword argument"
        );
    }

    TEST_CASE("调用参数尾逗号") {
        CHECK(
            parse_json(U"f(1, 2,)") ==
            nlohmann::json{
                {"type", "Call"},
                {"object", {{"type", "Identifier"}, {"identifier", "f"}}},
                {"positional_args", nlohmann::json::array({int_lit("1"), int_lit("2")})},
                {"keyword_args", nlohmann::json::array()}
            }
        );
    }
}

TEST_SUITE("索引实参") {

    TEST_CASE("零项索引保留空实参列表，可读取和写入") {
        const auto index = nlohmann::json{
            {"type", "Index"}, {"object", ident("a")}, {"args", nlohmann::json::array()}
        };
        CHECK(parse_json(U"a[]") == index);
        CHECK(parse_json(U"a[\n]") == index);
        CHECK(parse_json(U"a[] = 1") == bare_assign(index, int_lit("1")));
        CHECK(parse_json(U"a[] += 1")["target"] == index);
        CHECK(parse_json(U"tuple[]")["args"] == nlohmann::json::array());
        CHECK(parse_json(U"a[][1]")["object"] == index);
        CHECK(parse_json(U"a[]()")["object"] == index);
        CHECK(parse_json(U"a[()]")["args"].size() == 1);
        CHECK_THROWS_AS(parse_program(U"a[,]"), SyntaxError);
    }

    TEST_CASE("多个索引参数 a[i, j]") {
        CHECK(
            parse_json(U"a[i, j]") == nlohmann::json{
                                          {"type", "Index"},
                                          {"object", {{"type", "Identifier"}, {"identifier", "a"}}},
                                          {"args",
                                           nlohmann::json::array(
                                               {{{"type", "Identifier"}, {"identifier", "i"}},
                                                {{"type", "Identifier"}, {"identifier", "j"}}}
                                           )}
                                      }
        );
    }

    TEST_CASE("索引参数尾逗号") {
        CHECK(
            parse_json(U"a[1, 2,]") ==
            nlohmann::json{
                {"type", "Index"},
                {"object", {{"type", "Identifier"}, {"identifier", "a"}}},
                {"args", nlohmann::json::array({int_lit("1"), int_lit("2")})}
            }
        );
    }
}
