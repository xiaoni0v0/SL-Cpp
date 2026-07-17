// SL.md 2.2.2 基本表达式——`{}` 的判别规则：
//   func/class 之后的 {} 为函数体/类体；否则第一项以 ** 开头或紧跟 ':' 为字典字面量；都不满足为复合表达式。
// 这里只测判别规则本身和复合表达式的形状；字典各类项的具体语义（key 是否任意表达式等）已在
// 2_1_4_literals/literals_test.cpp 测过。
#include "../test_utils.h"
#include "../../../builtins/classes/exceptions/SyntaxError.h"

#include <doctest/doctest.h>

namespace {
nlohmann::json ident(const char *name) {
    return nlohmann::json{{"type", "Identifier"}, {"identifier", name}};
}
} // namespace

TEST_SUITE("2.2.2 {} 判别规则") {

TEST_CASE("空 {} → 空的复合表达式，不是空字典（空字典专门写法是 dict()）") {
    CHECK(parse_json(U"{}") == nlohmann::json{{"type", "Compound"}, {"exprs", nlohmann::json::array()}});
}

TEST_CASE("第一项以 ** 开头 → 字典字面量") {
    CHECK(parse_json(U"{**d}") == nlohmann::json{
          {"type", "LiteralDict"},
          {
          "items", nlohmann::json::array({
              {{"key", {{"type", "DoubleStar"}, {"operand", ident("d")}}}, {"val", nullptr}}
              })
          }
          });
}

TEST_CASE("第一个表达式后紧跟 ':' → 字典字面量") {
    CHECK(parse_json(U"{k: v}") == nlohmann::json{
          {"type", "LiteralDict"},
          {"items", nlohmann::json::array({{{"key", ident("k")}, {"val", ident("v")}}})}
          });
}

TEST_CASE("紧跟 ':' 之前允许换行，仍然判成字典") {
    CHECK(parse_json(U"{k\n: v}") == nlohmann::json{
          {"type", "LiteralDict"},
          {"items", nlohmann::json::array({{{"key", ident("k")}, {"val", ident("v")}}})}
          });
}

TEST_CASE("都不满足 → 复合表达式：单条表达式") {
    CHECK(parse_json(U"{x}") == nlohmann::json{{"type", "Compound"}, {"exprs", nlohmann::json::array({ident("x")})}});
}

TEST_CASE("复合表达式：分号分隔多条") {
    CHECK(parse_json(U"{a; b; c}") == nlohmann::json{
          {"type", "Compound"}, {"exprs", nlohmann::json::array({ident("a"), ident("b"), ident("c")})}
          });
}

TEST_CASE("复合表达式：换行分隔多条，不需要分号") {
    CHECK(parse_json(U"{a\nb\nc}") == nlohmann::json{
          {"type", "Compound"}, {"exprs", nlohmann::json::array({ident("a"), ident("b"), ident("c")})}
          });
}

TEST_CASE("复合表达式可以嵌套：{{}} 是外层复合表达式，唯一一条子表达式是内层的空复合表达式") {
    CHECK(parse_json(U"{{}}") == nlohmann::json{
          {"type", "Compound"},
          {
          "exprs",
          nlohmann::json::array({{{"type", "Compound"}, {"exprs", nlohmann::json::array()}}})
          }
          });
}

TEST_CASE("func/class 之后的 {} 是函数体/类体（Program），不走字典/复合表达式的判别") {
    const AstNodePtr node{parse_single(U"func f() {}")};
    const auto *func_node{dynamic_cast<AstNodeFunc *>(node.get())};
    REQUIRE(func_node != nullptr);
    CHECK(func_node->body_->exprs_.empty());

    const AstNodePtr class_node{parse_single(U"class C {}")};
    const auto *cls{dynamic_cast<AstNodeClass *>(class_node.get())};
    REQUIRE(cls != nullptr);
    CHECK(cls->body_->exprs_.empty());
}

}

TEST_SUITE("2.2.2 字典展开项：按 ** 前缀直接判定（不是靠有没有冒号反推）") {

TEST_CASE("展开项可以出现在第一项之外的位置") {
    CHECK(parse_json(U"{k: v, **d2}") == nlohmann::json{
          {"type", "LiteralDict"},
          {
          "items", nlohmann::json::array({
              {{"key", ident("k")}, {"val", ident("v")}},
              {{"key", {{"type", "DoubleStar"}, {"operand", ident("d2")}}}, {"val", nullptr}}
              })
          }
          });
}

TEST_CASE("展开项可以有多个，穿插在普通键值对之间") {
    CHECK(parse_json(U"{**d1, k: v, **d2}") == nlohmann::json{
          {"type", "LiteralDict"},
          {
          "items", nlohmann::json::array({
              {{"key", {{"type", "DoubleStar"}, {"operand", ident("d1")}}}, {"val", nullptr}},
              {{"key", ident("k")}, {"val", ident("v")}},
              {{"key", {{"type", "DoubleStar"}, {"operand", ident("d2")}}}, {"val", nullptr}}
              })
          }
          });
}

TEST_CASE("展开项后面也支持尾逗号") {
    CHECK(parse_json(U"{k: v, **d2,}") == nlohmann::json{
          {"type", "LiteralDict"},
          {
          "items", nlohmann::json::array({
              {{"key", ident("k")}, {"val", ident("v")}},
              {{"key", {{"type", "DoubleStar"}, {"operand", ident("d2")}}}, {"val", nullptr}}
              })
          }
          });
}

TEST_CASE("回归测试：第二项及以后既不是 ** 开头也没有冒号，必须报错（曾经的 bug：被静默当成合法展开项）") {
    CHECK_THROWS_AS(parse_program(U"{k: v, x}"), SyntaxError);
    CHECK_THROWS_AS(parse_program(U"{k: v, x, y: z}"), SyntaxError);
}

}

TEST_SUITE("2.2.2 复合表达式内部也必须有合法分隔符") {

TEST_CASE("回归测试：判别用的 first 和后续表达式之间没有分隔符必须报错，"
    "跟顶层 a b 同一个错误（曾经的 bug：first 绕过了 parse_exprs 内部的终止符检查，被静默接受）") {
    CHECK_THROWS_AS(parse_program(U"{k v}"), SyntaxError);
    CHECK_THROWS_AS(parse_program(U"{a b; c}"), SyntaxError);
}

TEST_CASE("first 和后续表达式之间只要有合法分隔符（换行/分号）就没问题") {
    CHECK(parse_json(U"{a\nb}") == nlohmann::json{
          {"type", "Compound"}, {"exprs", nlohmann::json::array({ident("a"), ident("b")})}
          });
    CHECK(parse_json(U"{a; b}") == nlohmann::json{
          {"type", "Compound"}, {"exprs", nlohmann::json::array({ident("a"), ident("b")})}
          });
}

}

TEST_SUITE("2.2.2 字典与复合表达式的其他边缘情况") {

TEST_CASE("未闭合的 {} 抛异常") {
    CHECK_THROWS_AS(parse_program(U"{a; b"), SyntaxError);
    CHECK_THROWS_AS(parse_program(U"{k: v"), SyntaxError);
}

TEST_CASE("字典值可以是复合表达式，复合表达式里也可以嵌字典") {
    CHECK(parse_json(U"{k: {a; b}}") == nlohmann::json{
          {"type", "LiteralDict"},
          {
          "items", nlohmann::json::array({
              {
              {"key", ident("k")}, {
              "val",
              {{"type", "Compound"}, {"exprs", nlohmann::json::array({ident("a"), ident("b")})}}
              }
              }
              })
          }
          });
    CHECK(parse_json(U"{ {k: v}; x }") == nlohmann::json{
          {"type", "Compound"}, {
          "exprs", nlohmann::json::array({
              {
              {"type", "LiteralDict"},
              {"items", nlohmann::json::array({{{"key", ident("k")}, {"val", ident("v")}}})}
              },
              ident("x")
              })
          }
          });
}

}
