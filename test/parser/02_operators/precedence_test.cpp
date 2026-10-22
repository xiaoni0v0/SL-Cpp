// 二元运算与逻辑运算的优先级、结合性。
#include "../../../cppexceptions/SyntaxError.h"
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("优先级——四则/位运算/范围") {

    TEST_CASE("* 比 + 优先级高：1 + 2 * 3 == 1 + (2 * 3)") {
        CHECK(
            parse_json(U"1 + 2 * 3") == nlohmann::json{
                                            {"type", "OpBinary"},
                                            {"op", "+"},
                                            {"left", int_lit("1")},
                                            {"right",
                                             {{"type", "OpBinary"},
                                              {"op", "*"},
                                              {"left", int_lit("2")},
                                              {"right", int_lit("3")}}}
                                        }
        );
    }

    TEST_CASE("同级左结合：1 - 2 - 3 == (1 - 2) - 3，不是 1 - (2 - 3)") {
        CHECK(
            parse_json(U"1 - 2 - 3") == nlohmann::json{
                                            {"type", "OpBinary"},
                                            {"op", "-"},
                                            {"left",
                                             {{"type", "OpBinary"},
                                              {"op", "-"},
                                              {"left", int_lit("1")},
                                              {"right", int_lit("2")}}},
                                            {"right", int_lit("3")}
                                        }
        );
    }

    TEST_CASE("* / // % 同级左结合：10 // 3 % 2 == (10 // 3) % 2") {
        CHECK(
            parse_json(U"10 // 3 % 2") == nlohmann::json{
                                              {"type", "OpBinary"},
                                              {"op", "%"},
                                              {"left",
                                               {{"type", "OpBinary"},
                                                {"op", "//"},
                                                {"left", int_lit("10")},
                                                {"right", int_lit("3")}}},
                                              {"right", int_lit("2")}
                                          }
        );
    }

    TEST_CASE("** 右结合：2 ** 3 ** 2 == 2 ** (3 ** 2)") {
        CHECK(
            parse_json(U"2 ** 3 ** 2") == nlohmann::json{
                                              {"type", "OpBinary"},
                                              {"op", "**"},
                                              {"left", int_lit("2")},
                                              {"right",
                                               {{"type", "OpBinary"},
                                                {"op", "**"},
                                                {"left", int_lit("3")},
                                                {"right", int_lit("2")}}}
                                          }
        );
    }

    TEST_CASE("** 比一元负号优先级高：-2 ** 2 == -(2 ** 2)，不是 (-2) ** 2") {
        CHECK(
            parse_json(U"-2 ** 2") == nlohmann::json{
                                          {"type", "OpUnary"},
                                          {"op", "-"},
                                          {"operand",
                                           {{"type", "OpBinary"},
                                            {"op", "**"},
                                            {"left", int_lit("2")},
                                            {"right", int_lit("2")}}}
                                      }
        );
    }

    TEST_CASE("** 右操作数可以是一元表达式：2 ** -2") {
        CHECK(
            parse_json(U"2 ** -2") ==
            nlohmann::json{
                {"type", "OpBinary"},
                {"op", "**"},
                {"left", int_lit("2")},
                {"right", {{"type", "OpUnary"}, {"op", "-"}, {"operand", int_lit("2")}}}
            }
        );
    }

    TEST_CASE("位运算优先级：& > ^ > |，1 | 2 ^ 3 & 4 == 1 | (2 ^ (3 & 4))") {
        CHECK(
            parse_json(U"1 | 2 ^ 3 & 4") == nlohmann::json{
                                                {"type", "OpBinary"},
                                                {"op", "|"},
                                                {"left", int_lit("1")},
                                                {"right",
                                                 {{"type", "OpBinary"},
                                                  {"op", "^"},
                                                  {"left", int_lit("2")},
                                                  {"right",
                                                   {{"type", "OpBinary"},
                                                    {"op", "&"},
                                                    {"left", int_lit("3")},
                                                    {"right", int_lit("4")}}}}}
                                            }
        );
    }

    TEST_CASE("加减比移位优先级高：1 + 2 << 3 == (1 + 2) << 3") {
        CHECK(
            parse_json(U"1 + 2 << 3") == nlohmann::json{
                                             {"type", "OpBinary"},
                                             {"op", "<<"},
                                             {"left",
                                              {{"type", "OpBinary"},
                                               {"op", "+"},
                                               {"left", int_lit("1")},
                                               {"right", int_lit("2")}}},
                                             {"right", int_lit("3")}
                                         }
        );
    }

    TEST_CASE("范围运算 .. 比移位优先级高：1 << 2 .. 3 == 1 << (2 .. 3)") {
        CHECK(
            parse_json(U"1 << 2 .. 3") == nlohmann::json{
                                              {"type", "OpBinary"},
                                              {"op", "<<"},
                                              {"left", int_lit("1")},
                                              {"right",
                                               {{"type", "OpBinary"},
                                                {"op", ".."},
                                                {"left", int_lit("2")},
                                                {"right", int_lit("3")}}}
                                          }
        );
    }

    TEST_CASE("范围运算 .. 比加减优先级低：1 + 2 .. 3 == (1 + 2) .. 3") {
        CHECK(
            parse_json(U"1 + 2 .. 3") == nlohmann::json{
                                             {"type", "OpBinary"},
                                             {"op", ".."},
                                             {"left",
                                              {{"type", "OpBinary"},
                                               {"op", "+"},
                                               {"left", int_lit("1")},
                                               {"right", int_lit("2")}}},
                                             {"right", int_lit("3")}
                                         }
        );
    }

    TEST_CASE(".. 左结合：1 .. 2 .. 3 == (1 .. 2) .. 3") {
        CHECK(
            parse_json(U"1 .. 2 .. 3") == nlohmann::json{
                                              {"type", "OpBinary"},
                                              {"op", ".."},
                                              {"left",
                                               {{"type", "OpBinary"},
                                                {"op", ".."},
                                                {"left", int_lit("1")},
                                                {"right", int_lit("2")}}},
                                              {"right", int_lit("3")}
                                          }
        );
    }

    TEST_CASE("..= 与 .. 同一档、左结合") {
        const auto range = [](const char *op, const nlohmann::json &l, const nlohmann::json &r) {
            return nlohmann::json{{"type", "OpBinary"}, {"op", op}, {"left", l}, {"right", r}};
        };
        CHECK(parse_json(U"1 ..= 3") == range("..=", int_lit("1"), int_lit("3")));
        CHECK(
            parse_json(U"1 + 2 ..= 3 * 4") == range(
                                                  "..=",
                                                  range("+", int_lit("1"), int_lit("2")),
                                                  range("*", int_lit("3"), int_lit("4"))
                                              )
        );
        CHECK(
            parse_json(U"1 << 2 ..= 3") ==
            range("<<", int_lit("1"), range("..=", int_lit("2"), int_lit("3")))
        );
        CHECK(
            parse_json(U"1 ..= 2 .. 3") ==
            range("..", range("..=", int_lit("1"), int_lit("2")), int_lit("3"))
        );
        CHECK(
            parse_json(U"1 .. 2 ..= 3") ==
            range("..=", range("..", int_lit("1"), int_lit("2")), int_lit("3"))
        );
        // 右操作数缺失报错，跟 .. 一样
        CHECK_THROWS_AS(parse_program(U"1 ..="), SyntaxError);
    }

    TEST_CASE("切片就是索引参数里写 ..，没有 Python 那种 a[1:2] 记法") {
        CHECK(
            parse_json(U"a[1..2]") == nlohmann::json{
                                          {"type", "Index"},
                                          {"object", ident("a")},
                                          {"args",
                                           nlohmann::json::array(
                                               {{{"type", "OpBinary"},
                                                 {"op", ".."},
                                                 {"left", int_lit("1")},
                                                 {"right", int_lit("2")}}}
                                           )}
                                      }
        );
        // Python 式的 a[1:2] 在 SL 里没有对应语法，':' 只能跟在标识符后面开始声明
        check_parse_throws_with(U"a[1:2]", "only an identifier can be declared");
    }

    TEST_CASE("括号可以改变运算顺序：(1 + 2) * 3") {
        CHECK(
            parse_json(U"(1 + 2) * 3") == nlohmann::json{
                                              {"type", "OpBinary"},
                                              {"op", "*"},
                                              {"left",
                                               {{"type", "OpBinary"},
                                                {"op", "+"},
                                                {"left", int_lit("1")},
                                                {"right", int_lit("2")}}},
                                              {"right", int_lit("3")}
                                          }
        );
    }
}

TEST_SUITE("优先级——逻辑运算") {

    TEST_CASE("and 比 or 优先级高：a or b and c == a or (b and c)") {
        CHECK(
            parse_json(U"a or b and c") ==
            nlohmann::json{
                {"type", "OpBinary"},
                {"op", "or"},
                {"left", {{"type", "Identifier"}, {"identifier", "a"}}},
                {"right",
                 {{"type", "OpBinary"},
                  {"op", "and"},
                  {"left", {{"type", "Identifier"}, {"identifier", "b"}}},
                  {"right", {{"type", "Identifier"}, {"identifier", "c"}}}}}
            }
        );
    }

    TEST_CASE("and/or 左结合：a and b and c == (a and b) and c") {
        CHECK(
            parse_json(U"a and b and c") ==
            nlohmann::json{
                {"type", "OpBinary"},
                {"op", "and"},
                {"left",
                 {{"type", "OpBinary"},
                  {"op", "and"},
                  {"left", {{"type", "Identifier"}, {"identifier", "a"}}},
                  {"right", {{"type", "Identifier"}, {"identifier", "b"}}}}},
                {"right", {{"type", "Identifier"}, {"identifier", "c"}}}
            }
        );
    }

    TEST_CASE("not 比 and 优先级高：not a and b == (not a) and b") {
        CHECK(
            parse_json(U"not a and b") ==
            nlohmann::json{
                {"type", "OpBinary"},
                {"op", "and"},
                {"left",
                 {{"type", "OpUnary"},
                  {"op", "not"},
                  {"operand", {{"type", "Identifier"}, {"identifier", "a"}}}}},
                {"right", {{"type", "Identifier"}, {"identifier", "b"}}}
            }
        );
    }

    TEST_CASE("not 右结合：not not a == not (not a)") {
        CHECK(
            parse_json(U"not not a") ==
            nlohmann::json{
                {"type", "OpUnary"},
                {"op", "not"},
                {"operand",
                 {{"type", "OpUnary"},
                  {"op", "not"},
                  {"operand", {{"type", "Identifier"}, {"identifier", "a"}}}}}
            }
        );
    }
}
