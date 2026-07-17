// SL.md 2.2.5.1 if 表达式：if (cond1) expr1 [elif (cond2) expr2 ...] [else expr3]
// 重点覆盖本次会话新加的规则：cond 槽禁止裸的普通赋值 =（需要显式再套一层括号），但允许裸的复合赋值。
#include "../test_utils.h"
#include "../../../builtins/classes/exceptions/SyntaxError.h"

#include <doctest/doctest.h>

namespace {
nlohmann::json ident(const char *name) {
    return nlohmann::json{{"type", "Identifier"}, {"identifier", name}};
}

nlohmann::json int_lit(const char *raw) {
    return nlohmann::json::parse(R"({"type":"LiteralInt","raw":")" + std::string{raw} + R"("})");
}
} // namespace

TEST_SUITE("2.2.5.1 if——基本形式") {

TEST_CASE("无 else") {
    CHECK(parse_json(U"if (a) b") == nlohmann::json{
          {"type", "If"},
          {"clauses", nlohmann::json::array({{{"cond", ident("a")}, {"body", ident("b")}}})},
          {"else_expr", nullptr}
          });
}

TEST_CASE("带 else") {
    CHECK(parse_json(U"if (a) b else c") == nlohmann::json{
          {"type", "If"},
          {"clauses", nlohmann::json::array({{{"cond", ident("a")}, {"body", ident("b")}}})},
          {"else_expr", ident("c")}
          });
}

TEST_CASE("多个 elif，无 else") {
    CHECK(parse_json(U"if (a) x elif (b) y elif (c) z") == nlohmann::json{
          {"type", "If"}, {
          "clauses", nlohmann::json::array({
              {{"cond", ident("a")}, {"body", ident("x")}},
              {{"cond", ident("b")}, {"body", ident("y")}},
              {{"cond", ident("c")}, {"body", ident("z")}}
              })
          },
          {"else_expr", nullptr}
          });
}

TEST_CASE("elif 加 else") {
    CHECK(parse_json(U"if (a) x elif (b) y else z") == nlohmann::json{
          {"type", "If"}, {
          "clauses", nlohmann::json::array({
              {{"cond", ident("a")}, {"body", ident("x")}},
              {{"cond", ident("b")}, {"body", ident("y")}}
              })
          },
          {"else_expr", ident("z")}
          });
}

TEST_CASE("cond 可以是任意表达式，比如比较") {
    CHECK(parse_json(U"if (x == 1) y") == nlohmann::json{
          {"type", "If"}, {
          "clauses", nlohmann::json::array({
              {
              {
              "cond",
              {
              {"type", "Compare"}, {"operands", nlohmann::json::array({ident("x"), int_lit("1")})},
              {"ops", nlohmann::json::array({"=="})}
              }
              },
              {"body", ident("y")}
              }
              })
          },
          {"else_expr", nullptr}
          });
}

TEST_CASE("缺少括号/未闭合括号都要报错") {
    CHECK_THROWS_AS(parse_program(U"if a) b"), SyntaxError);
    CHECK_THROWS_AS(parse_program(U"if (a b"), SyntaxError);
    CHECK_THROWS_AS(parse_program(U"if (a"), SyntaxError);
}

TEST_CASE("缺少 body 报错") {
    CHECK_THROWS_AS(parse_program(U"if (a)"), SyntaxError);
}

}

TEST_SUITE("2.2.5.1 if——cond 槽禁止裸的普通赋值") {

TEST_CASE("裸 = 直接报错，提示改用双层括号") {
    CHECK_THROWS_AS(parse_program(U"if (x = 1) y"), SyntaxError);
}

TEST_CASE("显式再套一层括号就允许：if ((x = 1)) y") {
    CHECK(parse_json(U"if ((x = 1)) y") == nlohmann::json{
          {"type", "If"}, {
          "clauses", nlohmann::json::array({
              {
              {"cond", {{"type", "Assign"}, {"target", ident("x")}, {"value", int_lit("1")}}},
              {"body", ident("y")}
              }
              })
          },
          {"else_expr", nullptr}
          });
}

TEST_CASE("裸的复合赋值不受限制，可以直接写：if (x += 1) y") {
    CHECK(parse_json(U"if (x += 1) y") == nlohmann::json{
          {"type", "If"}, {
          "clauses", nlohmann::json::array({
              {
              {
              "cond",
              {{"type", "CompoundAssign"}, {"target", ident("x")}, {"op", "+"}, {"value", int_lit("1")}}
              },
              {"body", ident("y")}
              }
              })
          },
          {"else_expr", nullptr}
          });
}

TEST_CASE("elif 的 cond 槽同样禁止裸赋值") {
    CHECK_THROWS_AS(parse_program(U"if (a) x elif (y = 1) z"), SyntaxError);
}

TEST_CASE("比较运算不受影响（== 不是赋值）") {
    CHECK_NOTHROW(parse_program(U"if (x == 1) y"));
}

}
