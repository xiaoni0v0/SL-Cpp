// SL.md 2.1.5——链式比较（== != < <= > >=）、is 链（自成一组，不与比较组混链）、
// 赋值与复合赋值（右结合）、*expr/**expr 展开。
#include "../test_utils.h"

#include <doctest/doctest.h>

namespace {
nlohmann::json ident(const char *name) {
    return nlohmann::json{{"type", "Identifier"}, {"identifier", name}};
}

nlohmann::json int_lit(const char *raw) {
    return nlohmann::json::parse(R"({"type":"LiteralInt","raw":")" + std::string{raw} + R"("})");
}
} // namespace

TEST_SUITE("2.1.5 链式比较") {

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

    TEST_CASE("比较运算优先级比加减低：1 + 1 < 2 + 2 == (1+1) < (2+2)") {
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

TEST_SUITE("2.1.5 is 链") {

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

    TEST_CASE("is 不与比较组混链，按各自优先级正常求值：a < b is c == (a < b) is c") {
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

    TEST_CASE("反过来也一样：a is b < c == a is (b < c)（比较优先级比 is 高，不管书写顺序）") {
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

TEST_SUITE("2.1.5 赋值与复合赋值") {

    TEST_CASE("基本赋值") {
        CHECK(
            parse_json(U"a = 1") ==
            nlohmann::json{{"type", "Assign"}, {"target", ident("a")}, {"value", int_lit("1")}}
        );
    }

    TEST_CASE("赋值右结合：a = b = c == a = (b = c)") {
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

    TEST_CASE(
        "全部 11 种复合赋值运算符：CompoundAssign 的 op 字段是底层二元运算符符号，不是带 = "
        "的原样写法"
    ) {
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

    TEST_CASE("复合赋值右结合、右侧可以是完整表达式：a += b += c") {
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

    TEST_CASE("赋值目标可以是属性/索引访问（是否为合法左值由语义层校验，语法层只管形状）") {
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

    TEST_CASE(
        "解构赋值：(a, b) = (1, 2) / [a, *b] = [1, 2, 3]（语法层只是普通的元组/列表字面量）"
    ) {
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
}

TEST_SUITE("2.1.5 */** 展开") {

    TEST_CASE("*expr 在列表/元组字面量里") {
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

    TEST_CASE("**expr 在字典字面量里（展开项，value 侧为 null）") {
        CHECK(
            parse_json(U"{**a, 'b': 1}") ==
            nlohmann::json{
                {"type", "LiteralDict"},
                {"items",
                 nlohmann::json::array(
                     {{{"key", {{"type", "DoubleStar"}, {"operand", ident("a")}}},
                       {"val", nullptr}},
                      {{"key", {{"type", "LiteralStr"}, {"value", "b"}}}, {"val", int_lit("1")}}}
                 )}
            }
        );
    }

    TEST_CASE("*/** 的操作数优先级跟一元运算符一致（140），** 幂运算比它高") {
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
