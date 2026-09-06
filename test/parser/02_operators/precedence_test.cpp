// 运算符优先级、结合性、后缀访问链。
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
        // Python 式的 a[1:2] 在 SL 里没有对应语法，': 2' 直接在这个位置报语法错误
        check_parse_throws_with(U"a[1:2]", "expected ']'");
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

TEST_SUITE("索引/调用/属性访问链") {

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

    TEST_CASE("位置实参不能出现在关键字实参之后，Parser 直接报语法错误") {
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
            parse_json(U"{**d ** e}")["items"][0]["key"] ==
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

    TEST_CASE("a[] 不允许，索引至少要有一个参数，消息说明白具体缺什么，位置指向 ']'") {
        // "a[]" -> a(1)[(2)](3)：还没消耗 ']' 前就先发现 args 为空，位置停在 ']' 自己
        try {
            parse_as_file(U"a[]");
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("at least one argument for indexing") != std::string::npos);
            CHECK(msg.find("1:3:") != std::string::npos);
        }
    }

    TEST_CASE(
        "未闭合的调用/索引抛异常，消息分别点名'function call'/'index expression'，位置指向 EOF"
    ) {
        // "f(1, 2" 共 6 个字符，EOF 在第 7 列
        try {
            parse_as_file(U"f(1, 2");
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("function call") != std::string::npos);
            CHECK(msg.find("1:7:") != std::string::npos);
        }
        // "a[0" 共 3 个字符，EOF 在第 4 列
        try {
            parse_as_file(U"a[0");
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("index expression") != std::string::npos);
            CHECK(msg.find("1:4:") != std::string::npos);
        }
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

    TEST_CASE("属性访问后面必须是标识符") {
        CHECK_THROWS_AS(parse_as_file(U"a.1"), SyntaxError);
        CHECK_THROWS_AS(parse_as_file(U"a."), SyntaxError);
        CHECK_THROWS_AS(parse_as_file(U"obj.for"), SyntaxError);
        CHECK_THROWS_AS(parse_as_file(U"obj.class"), SyntaxError);
        CHECK_THROWS_AS(parse_as_file(U"obj.if"), SyntaxError);
    }
}
