// SL.md 2.2.5.2 for 表达式：
//   步进模式 for [$] (init cond inc) expr；迭代模式 for [$] (lvalue : iterable) expr。
// 本次会话相关改动，这里重点覆盖：
//   1. 曾经存在、但从未被 SL.md 授权的"裸单表达式当条件" for (cond) body 分支已删除，现在必须报错；
//   2. 中间 cond 槽禁止裸的普通赋值 =（init/inc 不受限）；
//   3. for () 彻底为空时的专门报错。
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

TEST_SUITE("2.2.5.2 for——步进模式") {

TEST_CASE("三槽齐全") {
    CHECK(parse_json(U"for (i = 0; i < 10; i += 1) body") == nlohmann::json{
          {"type", "ForCond"}, {"collect", false},
          {"init", {{"type", "Assign"}, {"target", ident("i")}, {"value", int_lit("0")}}},
          {
          "cond", {
          {"type", "Compare"}, {"operands", nlohmann::json::array({ident("i"), int_lit("10")})},
          {"ops", nlohmann::json::array({"<"})}
          }
          },
          {"inc", {{"type", "CompoundAssign"}, {"target", ident("i")}, {"op", "+"}, {"value", int_lit("1")}}},
          {"body", ident("body")}
          });
}

TEST_CASE("三槽全空：无限循环 for (;;)") {
    CHECK(parse_json(U"for (;;) body") == nlohmann::json{
          {"type", "ForCond"}, {"collect", false},
          {"init", nullptr}, {"cond", nullptr}, {"inc", nullptr}, {"body", ident("body")}
          });
}

TEST_CASE("只有 cond：for (; cond ;) body") {
    CHECK(parse_json(U"for (; c ;) body") == nlohmann::json{
          {"type", "ForCond"}, {"collect", false},
          {"init", nullptr}, {"cond", ident("c")}, {"inc", nullptr}, {"body", ident("body")}
          });
}

TEST_CASE("只有 init：for (i = 0;;) body") {
    CHECK(parse_json(U"for (i = 0;;) body") == nlohmann::json{
          {"type", "ForCond"}, {"collect", false},
          {"init", {{"type", "Assign"}, {"target", ident("i")}, {"value", int_lit("0")}}},
          {"cond", nullptr}, {"inc", nullptr}, {"body", ident("body")}
          });
}

TEST_CASE("只有 inc：for (;; i += 1) body") {
    CHECK(parse_json(U"for (;; i += 1) body") == nlohmann::json{
          {"type", "ForCond"}, {"collect", false}, {"init", nullptr}, {"cond", nullptr},
          {"inc", {{"type", "CompoundAssign"}, {"target", ident("i")}, {"op", "+"}, {"value", int_lit("1")}}},
          {"body", ident("body")}
          });
}

TEST_CASE("分隔符可以用换行代替分号") {
    CHECK(parse_json(U"for (i = 0\ni < 10\ni += 1) body") == nlohmann::json{
          {"type", "ForCond"}, {"collect", false},
          {"init", {{"type", "Assign"}, {"target", ident("i")}, {"value", int_lit("0")}}},
          {
          "cond", {
          {"type", "Compare"}, {"operands", nlohmann::json::array({ident("i"), int_lit("10")})},
          {"ops", nlohmann::json::array({"<"})}
          }
          },
          {"inc", {{"type", "CompoundAssign"}, {"target", ident("i")}, {"op", "+"}, {"value", int_lit("1")}}},
          {"body", ident("body")}
          });
}

TEST_CASE("收集模式 for $ (...)") {
    CHECK(parse_json(U"for $ (i = 0; i < 10; i += 1) body") == nlohmann::json{
          {"type", "ForCond"}, {"collect", true},
          {"init", {{"type", "Assign"}, {"target", ident("i")}, {"value", int_lit("0")}}},
          {
          "cond", {
          {"type", "Compare"}, {"operands", nlohmann::json::array({ident("i"), int_lit("10")})},
          {"ops", nlohmann::json::array({"<"})}
          }
          },
          {"inc", {{"type", "CompoundAssign"}, {"target", ident("i")}, {"op", "+"}, {"value", int_lit("1")}}},
          {"body", ident("body")}
          });
}

TEST_CASE("槽之间缺分隔符报错") {
    CHECK_THROWS_AS(parse_program(U"for (i = 0 i < 10; i += 1) body"), SyntaxError);
}

TEST_CASE("for () 彻底为空报错，提示改用 for (;;) 或 while (cond)") {
    CHECK_THROWS_AS(parse_program(U"for () body"), SyntaxError);
}

TEST_CASE("裸单表达式当条件已不再支持（未被 SL.md 授权的旧分支，已删除）：for (cond) body 必须报错") {
    CHECK_THROWS_AS(parse_program(U"for (x > 0) body"), SyntaxError);
    CHECK_THROWS_AS(parse_program(U"for $ (x > 0) body"), SyntaxError);
}

}

TEST_SUITE("2.2.5.2 for——迭代模式") {

TEST_CASE("基本迭代") {
    CHECK(parse_json(U"for (x : xs) body") == nlohmann::json{
          {"type", "ForIter"}, {"collect", false}, {"target", ident("x")}, {"iterable", ident("xs")},
          {"body", ident("body")}
          });
}

TEST_CASE("收集模式迭代") {
    CHECK(parse_json(U"for $ (x : xs) body") == nlohmann::json{
          {"type", "ForIter"}, {"collect", true}, {"target", ident("x")}, {"iterable", ident("xs")},
          {"body", ident("body")}
          });
}

TEST_CASE("目标可以是解构元组/列表（语法层放行任意左值形状，交语义层校验）") {
    CHECK(parse_json(U"for ((a, b) : pairs) body") == nlohmann::json{
          {"type", "ForIter"}, {"collect", false},
          {
          "target",
          {{"type", "LiteralTuple"}, {"items", nlohmann::json::array({ident("a"), ident("b")})}}
          },
          {"iterable", ident("pairs")}, {"body", ident("body")}
          });
    CHECK(parse_json(U"for ([a, *b] : xs) body") == nlohmann::json{
          {"type", "ForIter"}, {"collect", false},
          {
          "target", {
          {"type", "LiteralList"}, {
          "items", nlohmann::json::array({
              ident("a"), {{"type", "Star"}, {"operand", ident("b")}}
              })
          }
          }
          },
          {"iterable", ident("xs")}, {"body", ident("body")}
          });
}

TEST_CASE("元组目标不带外层括号会被当成步进模式解析，进而因为缺分隔符报错") {
    CHECK_THROWS_AS(parse_program(U"for (a, b : pairs) body"), SyntaxError);
}

TEST_CASE("未闭合括号/缺 body 报错") {
    CHECK_THROWS_AS(parse_program(U"for (x : xs"), SyntaxError);
    CHECK_THROWS_AS(parse_program(U"for (x : xs)"), SyntaxError);
}

}

TEST_SUITE("2.2.5.2 for——cond 槽禁止裸的普通赋值（init/inc 不受限）") {

TEST_CASE("中间 cond 槽裸 = 报错") {
    CHECK_THROWS_AS(parse_program(U"for (i = 0; i = 10; i += 1) body"), SyntaxError);
}

TEST_CASE("中间 cond 槽裸复合赋值不受限") {
    CHECK(parse_json(U"for (; x += 1;) body") == nlohmann::json{
          {"type", "ForCond"}, {"collect", false}, {"init", nullptr},
          {"cond", {{"type", "CompoundAssign"}, {"target", ident("x")}, {"op", "+"}, {"value", int_lit("1")}}},
          {"inc", nullptr}, {"body", ident("body")}
          });
}

TEST_CASE("cond 槽套一层括号就能写裸赋值") {
    CHECK(parse_json(U"for (; (x = 1);) body") == nlohmann::json{
          {"type", "ForCond"}, {"collect", false}, {"init", nullptr},
          {"cond", {{"type", "Assign"}, {"target", ident("x")}, {"value", int_lit("1")}}},
          {"inc", nullptr}, {"body", ident("body")}
          });
}

TEST_CASE("init/inc 槽裸赋值不受限（本来就是为赋值而生）") {
    CHECK_NOTHROW(parse_program(U"for (i = 0; c; i = i + 1) body"));
}

}
