// SL.md 2.1.5 运算符——优先级表、结合性、后缀访问链（索引/调用/属性/?/!）。
// 链式比较、is 链、赋值/复合赋值放在同目录的 compare_is_assign_test.cpp。
#include "../test_utils.h"
#include "../../../builtins/classes/exceptions/SyntaxError.h"

#include <doctest/doctest.h>

namespace {
nlohmann::json int_lit(const char *raw) {
    return nlohmann::json::parse(R"({"type":"LiteralInt","raw":")" + std::string{raw} + R"("})");
}
} // namespace

TEST_SUITE("2.1.5 优先级——四则/位运算/范围") {

TEST_CASE("* 比 + 优先级高：1 + 2 * 3 == 1 + (2 * 3)") {
    CHECK(parse_json(U"1 + 2 * 3") == nlohmann::json{
          {"type", "OpBinary"}, {"op", "+"}, {"left", int_lit("1")},
          {"right", {{"type", "OpBinary"}, {"op", "*"}, {"left", int_lit("2")}, {"right", int_lit("3")}}}
          });
}

TEST_CASE("同级左结合：1 - 2 - 3 == (1 - 2) - 3，不是 1 - (2 - 3)") {
    CHECK(parse_json(U"1 - 2 - 3") == nlohmann::json{
          {"type", "OpBinary"}, {"op", "-"},
          {"left", {{"type", "OpBinary"}, {"op", "-"}, {"left", int_lit("1")}, {"right", int_lit("2")}}},
          {"right", int_lit("3")}
          });
}

TEST_CASE("* / // % 同级左结合：10 // 3 % 2 == (10 // 3) % 2") {
    CHECK(parse_json(U"10 // 3 % 2") == nlohmann::json{
          {"type", "OpBinary"}, {"op", "%"},
          {"left", {{"type", "OpBinary"}, {"op", "//"}, {"left", int_lit("10")}, {"right", int_lit("3")}}},
          {"right", int_lit("2")}
          });
}

TEST_CASE("** 右结合：2 ** 3 ** 2 == 2 ** (3 ** 2)") {
    CHECK(parse_json(U"2 ** 3 ** 2") == nlohmann::json{
          {"type", "OpBinary"}, {"op", "**"}, {"left", int_lit("2")},
          {"right", {{"type", "OpBinary"}, {"op", "**"}, {"left", int_lit("3")}, {"right", int_lit("2")}}}
          });
}

TEST_CASE("** 比一元负号优先级高：-2 ** 2 == -(2 ** 2)，不是 (-2) ** 2") {
    CHECK(parse_json(U"-2 ** 2") == nlohmann::json{
          {"type", "OpUnary"}, {"op", "-"},
          {"operand", {{"type", "OpBinary"}, {"op", "**"}, {"left", int_lit("2")}, {"right", int_lit("2")}}}
          });
}

TEST_CASE("** 右操作数可以是一元表达式：2 ** -2") {
    CHECK(parse_json(U"2 ** -2") == nlohmann::json{
          {"type", "OpBinary"}, {"op", "**"}, {"left", int_lit("2")},
          {"right", {{"type", "OpUnary"}, {"op", "-"}, {"operand", int_lit("2")}}}
          });
}

TEST_CASE("位运算优先级：& > ^ > |，1 | 2 ^ 3 & 4 == 1 | (2 ^ (3 & 4))") {
    CHECK(parse_json(U"1 | 2 ^ 3 & 4") == nlohmann::json{
          {"type", "OpBinary"}, {"op", "|"}, {"left", int_lit("1")},
          {
          "right", {
          {"type", "OpBinary"}, {"op", "^"}, {"left", int_lit("2")},
          {
          "right",
          {{"type", "OpBinary"}, {"op", "&"}, {"left", int_lit("3")}, {"right", int_lit("4")}}
          }
          }
          }
          });
}

TEST_CASE("加减比移位优先级高：1 + 2 << 3 == (1 + 2) << 3") {
    CHECK(parse_json(U"1 + 2 << 3") == nlohmann::json{
          {"type", "OpBinary"}, {"op", "<<"},
          {"left", {{"type", "OpBinary"}, {"op", "+"}, {"left", int_lit("1")}, {"right", int_lit("2")}}},
          {"right", int_lit("3")}
          });
}

TEST_CASE("范围运算 .. 比移位优先级高：1 << 2 .. 3 == 1 << (2 .. 3)") {
    CHECK(parse_json(U"1 << 2 .. 3") == nlohmann::json{
          {"type", "OpBinary"}, {"op", "<<"}, {"left", int_lit("1")},
          {"right", {{"type", "OpBinary"}, {"op", ".."}, {"left", int_lit("2")}, {"right", int_lit("3")}}}
          });
}

TEST_CASE("范围运算 .. 比加减优先级低：1 + 2 .. 3 == (1 + 2) .. 3") {
    CHECK(parse_json(U"1 + 2 .. 3") == nlohmann::json{
          {"type", "OpBinary"}, {"op", ".."},
          {"left", {{"type", "OpBinary"}, {"op", "+"}, {"left", int_lit("1")}, {"right", int_lit("2")}}},
          {"right", int_lit("3")}
          });
}

TEST_CASE(".. 左结合：1 .. 2 .. 3 == (1 .. 2) .. 3") {
    CHECK(parse_json(U"1 .. 2 .. 3") == nlohmann::json{
          {"type", "OpBinary"}, {"op", ".."},
          {"left", {{"type", "OpBinary"}, {"op", ".."}, {"left", int_lit("1")}, {"right", int_lit("2")}}},
          {"right", int_lit("3")}
          });
}

TEST_CASE("括号可以改变运算顺序：(1 + 2) * 3") {
    CHECK(parse_json(U"(1 + 2) * 3") == nlohmann::json{
          {"type", "OpBinary"}, {"op", "*"},
          {"left", {{"type", "OpBinary"}, {"op", "+"}, {"left", int_lit("1")}, {"right", int_lit("2")}}},
          {"right", int_lit("3")}
          });
}

}

TEST_SUITE("2.1.5 优先级——逻辑运算") {

TEST_CASE("and 比 or 优先级高：a or b and c == a or (b and c)") {
    CHECK(parse_json(U"a or b and c") == nlohmann::json{
          {"type", "OpBinary"}, {"op", "or"},
          {"left", {{"type", "Identifier"}, {"identifier", "a"}}},
          {
          "right", {
          {"type", "OpBinary"}, {"op", "and"},
          {"left", {{"type", "Identifier"}, {"identifier", "b"}}},
          {"right", {{"type", "Identifier"}, {"identifier", "c"}}}
          }
          }
          });
}

TEST_CASE("and/or 左结合：a and b and c == (a and b) and c") {
    CHECK(parse_json(U"a and b and c") == nlohmann::json{
          {"type", "OpBinary"}, {"op", "and"},
          {
          "left", {
          {"type", "OpBinary"}, {"op", "and"},
          {"left", {{"type", "Identifier"}, {"identifier", "a"}}},
          {"right", {{"type", "Identifier"}, {"identifier", "b"}}}
          }
          },
          {"right", {{"type", "Identifier"}, {"identifier", "c"}}}
          });
}

TEST_CASE("not 比 and 优先级高：not a and b == (not a) and b") {
    CHECK(parse_json(U"not a and b") == nlohmann::json{
          {"type", "OpBinary"}, {"op", "and"},
          {
          "left", {
          {"type", "OpUnary"}, {"op", "not"}, {"operand", {{"type", "Identifier"}, {"identifier", "a"}}}
          }
          },
          {"right", {{"type", "Identifier"}, {"identifier", "b"}}}
          });
}

TEST_CASE("not 右结合：not not a == not (not a)") {
    CHECK(parse_json(U"not not a") == nlohmann::json{
          {"type", "OpUnary"}, {"op", "not"},
          {
          "operand", {
          {"type", "OpUnary"}, {"op", "not"}, {"operand", {{"type", "Identifier"}, {"identifier", "a"}}}
          }
          }
          });
}

}

TEST_SUITE("2.1.5 一元运算符与后缀问号感叹号") {

TEST_CASE("+x -x ~x") {
    CHECK(parse_json(U"+1") == nlohmann::json{{"type", "OpUnary"}, {"op", "+"}, {"operand", int_lit("1")}});
    CHECK(parse_json(U"-1") == nlohmann::json{{"type", "OpUnary"}, {"op", "-"}, {"operand", int_lit("1")}});
    CHECK(parse_json(U"~1") == nlohmann::json{{"type", "OpUnary"}, {"op", "~"}, {"operand", int_lit("1")}});
}

TEST_CASE("一元运算符可以连写（不同符号之间）：+-1 == +(-1)") {
    CHECK(parse_json(U"+-1") == nlohmann::json{
          {"type", "OpUnary"}, {"op", "+"},
          {"operand", {{"type", "OpUnary"}, {"op", "-"}, {"operand", int_lit("1")}}}
          });
}

TEST_CASE("x? / x!") {
    CHECK(parse_json(U"x?") == nlohmann::json{
          {"type", "OpUnary"}, {"op", "?"}, {"operand", {{"type", "Identifier"}, {"identifier", "x"}}}
          });
    CHECK(parse_json(U"x!") == nlohmann::json{
          {"type", "OpUnary"}, {"op", "!"}, {"operand", {{"type", "Identifier"}, {"identifier", "x"}}}
          });
}

TEST_CASE("? ! 可以连续叠加，左结合：x?! == (x?)!") {
    CHECK(parse_json(U"x?!") == nlohmann::json{
          {"type", "OpUnary"}, {"op", "!"},
          {
          "operand", {
          {"type", "OpUnary"}, {"op", "?"}, {"operand", {{"type", "Identifier"}, {"identifier", "x"}}}
          }
          }
          });
}

}

TEST_SUITE("2.1.5 索引/调用/属性访问链") {

TEST_CASE("单独的调用/索引/属性访问") {
    CHECK(parse_json(U"f()") == nlohmann::json{
          {"type", "Call"}, {"object", {{"type", "Identifier"}, {"identifier", "f"}}},
          {"args", nlohmann::json::array()}, {"kwargs", nlohmann::json::array()}
          });
    CHECK(parse_json(U"a[0]") == nlohmann::json{
          {"type", "Index"}, {"object", {{"type", "Identifier"}, {"identifier", "a"}}},
          {"args", nlohmann::json::array({int_lit("0")})}
          });
    CHECK(parse_json(U"a.b") == nlohmann::json{
          {"type", "Attr"}, {"object", {{"type", "Identifier"}, {"identifier", "a"}}}, {"attr", "b"}
          });
}

TEST_CASE("调用带位置参数和关键字参数") {
    CHECK(parse_json(U"f(1, 2, x=3)") == nlohmann::json{
          {"type", "Call"}, {"object", {{"type", "Identifier"}, {"identifier", "f"}}},
          {"args", nlohmann::json::array({int_lit("1"), int_lit("2")})},
          {"kwargs", nlohmann::json::array({nlohmann::json{{"key", "x"}, {"value", int_lit("3")}}})}
          });
}

TEST_CASE("*expr / **expr 展开可以出现在调用的位置实参里") {
    CHECK(parse_json(U"f(*args, **kwargs)") == nlohmann::json{
          {"type", "Call"}, {"object", {{"type", "Identifier"}, {"identifier", "f"}}},
          {
          "args", nlohmann::json::array({
              {{"type", "Star"}, {"operand", {{"type", "Identifier"}, {"identifier", "args"}}}},
              {{"type", "DoubleStar"}, {"operand", {{"type", "Identifier"}, {"identifier", "kwargs"}}}}
              })
          },
          {"kwargs", nlohmann::json::array()}
          });
}

TEST_CASE("*/** 展开的操作数按单目运算符一档（140）解析（SL.md 3.6 的结合力规则）") {
    // 属性访问（170）先结合进操作数：*a.b 即 *(a.b)
    CHECK(parse_json(U"[*a.b]")["items"][0] == nlohmann::json{
          {"type", "Star"},
          {
          "operand",
          {{"type", "Attr"}, {"object", {{"type", "Identifier"}, {"identifier", "a"}}}, {"attr", "b"}}
          }
          });
    // 调用（170）先结合进操作数：*f(x) 即 *(f(x))
    CHECK(parse_json(U"[*f(x)]")["items"][0]["operand"]["type"] == "Call");
    // 幂（150）也先结合进操作数：**d ** e 即 **(d ** e)
    CHECK(parse_json(U"{**d ** e}")["items"][0]["key"] == nlohmann::json{
          {"type", "DoubleStar"},
          {
          "operand",
          {
          {"type", "OpBinary"}, {"op", "**"},
          {"left", {{"type", "Identifier"}, {"identifier", "d"}}},
          {"right", {{"type", "Identifier"}, {"identifier", "e"}}}
          }
          }
          });
    // 低于 140 的（如加法 120）不结合进操作数：*a + b 即 (*a) + b（合法性归语义层按 3.6 判）
    CHECK(parse_json(U"*a + b") == nlohmann::json{
          {"type", "OpBinary"}, {"op", "+"},
          {"left", {{"type", "Star"}, {"operand", {{"type", "Identifier"}, {"identifier", "a"}}}}},
          {"right", {{"type", "Identifier"}, {"identifier", "b"}}}
          });
}

TEST_CASE("多个索引参数 a[i, j]") {
    CHECK(parse_json(U"a[i, j]") == nlohmann::json{
          {"type", "Index"}, {"object", {{"type", "Identifier"}, {"identifier", "a"}}},
          {
          "args", nlohmann::json::array({
              {{"type", "Identifier"}, {"identifier", "i"}}, {{"type", "Identifier"}, {"identifier", "j"}}
              })
          }
          });
}

TEST_CASE("左结合链式：obj.attr[0](1, 2)?") {
    CHECK(parse_json(U"obj.attr[0](1, 2)?") == nlohmann::json{
          {"type", "OpUnary"}, {"op", "?"}, {
          "operand", {
          {"type", "Call"}, {
          "object", {
          {"type", "Index"}, {
          "object", {
          {"type", "Attr"},
          {"object", {{"type", "Identifier"}, {"identifier", "obj"}}}, {"attr", "attr"}
          }
          },
          {"args", nlohmann::json::array({int_lit("0")})}
          }
          },
          {"args", nlohmann::json::array({int_lit("1"), int_lit("2")})},
          {"kwargs", nlohmann::json::array()}
          }
          }
          });
}

TEST_CASE("a[] 不允许，索引至少要有一个参数") {
    CHECK_THROWS_AS(parse_program(U"a[]"), SyntaxError);
}

TEST_CASE("未闭合的调用/索引抛异常") {
    CHECK_THROWS_AS(parse_program(U"f(1, 2"), SyntaxError);
    CHECK_THROWS_AS(parse_program(U"a[0"), SyntaxError);
}

TEST_CASE("属性访问后面必须是标识符") {
    CHECK_THROWS_AS(parse_program(U"a.1"), SyntaxError);
    CHECK_THROWS_AS(parse_program(U"a."), SyntaxError);
}

}
