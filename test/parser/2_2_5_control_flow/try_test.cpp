// SL.md 2.2.5.7 try 表达式：try expr1 [except (Exception1, ...) expr2 ...] [finally expr3]
// "except 和 finally 不能同时省略" 是语义层校验（Parser.cpp 里也明确注释了这一点），
// 语法层单纯 try expr（不写 except/finally）也能正常解析出来。
#include "../test_utils.h"
#include "../../../builtins/classes/exceptions/SyntaxError.h"

#include <doctest/doctest.h>

namespace {
nlohmann::json ident(const char *name) {
    return nlohmann::json{{"type", "Identifier"}, {"identifier", name}};
}
} // namespace

TEST_SUITE("2.2.5.7 try") {

TEST_CASE("单个 except") {
    CHECK(parse_json(U"try a except (E) b") == nlohmann::json{
          {"type", "Try"}, {"try_expr", ident("a")},
          {
          "except_clauses",
          nlohmann::json::array({{{"exceptions", nlohmann::json::array({ident("E")})}, {"body", ident("b")}}})
          },
          {"finally_expr", nullptr}
          });
}

TEST_CASE("一个 except 子句里可以有多个异常类型") {
    CHECK(parse_json(U"try a except (E1, E2, E3) b") == nlohmann::json{
          {"type", "Try"}, {"try_expr", ident("a")},
          {
          "except_clauses", nlohmann::json::array({
              {
              {"exceptions", nlohmann::json::array({ident("E1"), ident("E2"), ident("E3")})},
              {"body", ident("b")}
              }
              })
          },
          {"finally_expr", nullptr}
          });
}

TEST_CASE("多个 except 子句 + finally") {
    CHECK(parse_json(U"try a except (E1) b except (E2) c finally d") == nlohmann::json{
          {"type", "Try"}, {"try_expr", ident("a")},
          {
          "except_clauses", nlohmann::json::array({
              {{"exceptions", nlohmann::json::array({ident("E1")})}, {"body", ident("b")}},
              {{"exceptions", nlohmann::json::array({ident("E2")})}, {"body", ident("c")}}
              })
          },
          {"finally_expr", ident("d")}
          });
}

TEST_CASE("只有 finally，没有 except") {
    CHECK(parse_json(U"try a finally b") == nlohmann::json{
          {"type", "Try"}, {"try_expr", ident("a")},
          {"except_clauses", nlohmann::json::array()}, {"finally_expr", ident("b")}
          });
}

TEST_CASE("语法层允许 except 和 finally 都不写（该约束交语义层校验）") {
    CHECK(parse_json(U"try a") == nlohmann::json{
          {"type", "Try"}, {"try_expr", ident("a")},
          {"except_clauses", nlohmann::json::array()}, {"finally_expr", nullptr}
          });
}

TEST_CASE("except 子句括号内至少要有一个异常表达式，位置指向空括号里的 ')'") {
    // "try a except () b" -> t(1)r(2)y(3) (4)a(5) (6)e(7)x(8)c(9)e(10)p(11)t(12) (13)((14))(15) (16)b(17)
    try {
        parse_program(U"try a except () b");
        FAIL("应当抛出异常");
    } catch (const SyntaxError &e) {
        const std::string msg{e.what()};
        CHECK(msg.find("except requires at least one exception type") != std::string::npos);
        CHECK(msg.find("1:15:") != std::string::npos);
    }
}

TEST_CASE("未闭合括号/缺 body 报错") {
    CHECK_THROWS_AS(parse_program(U"try a except (E"), SyntaxError);
    CHECK_THROWS_AS(parse_program(U"try a except (E)"), SyntaxError);
}

}
