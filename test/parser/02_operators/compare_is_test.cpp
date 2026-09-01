// 链式比较、is 链。赋值见 assign_test.cpp。
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("链式比较") {

    TEST_CASE("两项比较") {
        CHECK(
            parse_json(U"1 < 2") ==
            nlohmann::json{
                {"type", "Compare"},
                {"operands", nlohmann::json::array({int_lit("1"), int_lit("2")})},
                {"ops", nlohmann::json::array({"<"})}
            }
        );
    }

    TEST_CASE("三项链式比较：1 < 2 <= 3") {
        CHECK(
            parse_json(U"1 < 2 <= 3") ==
            nlohmann::json{
                {"type", "Compare"},
                {"operands", nlohmann::json::array({int_lit("1"), int_lit("2"), int_lit("3")})},
                {"ops", nlohmann::json::array({"<", "<="})}
            }
        );
    }

    TEST_CASE("六种比较符都能出现在同一条链里") {
        CHECK(
            parse_json(U"a < b <= c > d >= e == f != g") ==
            nlohmann::json{
                {"type", "Compare"},
                {"operands",
                 nlohmann::json::array(
                     {ident("a"),
                      ident("b"),
                      ident("c"),
                      ident("d"),
                      ident("e"),
                      ident("f"),
                      ident("g")}
                 )},
                {"ops", nlohmann::json::array({"<", "<=", ">", ">=", "==", "!="})}
            }
        );
    }

    TEST_CASE("比较运算优先级比加减低：1 + 1 < 2 + 2 即 (1+1) < (2+2)") {
        CHECK(
            parse_json(U"1 + 1 < 2 + 2") == nlohmann::json{
                                                {"type", "Compare"},
                                                {"operands",
                                                 nlohmann::json::array(
                                                     {{{"type", "OpBinary"},
                                                       {"op", "+"},
                                                       {"left", int_lit("1")},
                                                       {"right", int_lit("1")}},
                                                      {{"type", "OpBinary"},
                                                       {"op", "+"},
                                                       {"left", int_lit("2")},
                                                       {"right", int_lit("2")}}}
                                                 )},
                                                {"ops", nlohmann::json::array({"<"})}
                                            }
        );
    }
}

TEST_SUITE("is 链") {

    TEST_CASE("两项 is") {
        CHECK(
            parse_json(U"a is b") ==
            nlohmann::json{
                {"type", "Is"}, {"operands", nlohmann::json::array({ident("a"), ident("b")})}
            }
        );
    }

    TEST_CASE("三项链式 is：a is b is c") {
        CHECK(
            parse_json(U"a is b is c") ==
            nlohmann::json{
                {"type", "Is"},
                {"operands", nlohmann::json::array({ident("a"), ident("b"), ident("c")})}
            }
        );
    }

    TEST_CASE("is 不与比较组混链：a < b is c 即 (a < b) is c") {
        CHECK(
            parse_json(U"a < b is c") ==
            nlohmann::json{
                {"type", "Is"},
                {"operands",
                 nlohmann::json::array(
                     {{{"type", "Compare"},
                       {"operands", nlohmann::json::array({ident("a"), ident("b")})},
                       {"ops", nlohmann::json::array({"<"})}},
                      ident("c")}
                 )}
            }
        );
    }

    TEST_CASE("反过来：a is b < c 即 a is (b < c)（比较比 is 紧）") {
        CHECK(
            parse_json(U"a is b < c") ==
            nlohmann::json{
                {"type", "Is"},
                {"operands",
                 nlohmann::json::array(
                     {ident("a"),
                      {{"type", "Compare"},
                       {"operands", nlohmann::json::array({ident("b"), ident("c")})},
                       {"ops", nlohmann::json::array({"<"})}}}
                 )}
            }
        );
    }
}
