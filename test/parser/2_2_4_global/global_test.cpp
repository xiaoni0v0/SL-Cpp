// SL.md 2.2.4 global 表达式——语法：global identifier。
// 注意：identifier 本身就是标识符 token，不是"解析成表达式再校验形状"，parser 直接 expect(IDENTIFIER)。
#include "../test_utils.h"
#include "../../../builtins/classes/exceptions/SyntaxError.h"

#include <doctest/doctest.h>

TEST_SUITE("2.2.4 global") {

TEST_CASE("基本形式") {
    CHECK(parse_json(U"global x") == nlohmann::json{{"type", "Global"}, {"identifier", "x"}});
}

TEST_CASE("global 后面必须是标识符 token，不是表达式，字面量非法") {
    CHECK_THROWS_AS(parse_program(U"global 5"), SyntaxError);
    CHECK_THROWS_AS(parse_program(U"global 'x'"), SyntaxError);
}

TEST_CASE("global 只消耗一个标识符，后面多余的内容如果不能接成后缀运算，会在外层因为缺分隔符报错") {
    CHECK_THROWS_AS(parse_program(U"global x y"), SyntaxError);
}

TEST_CASE("global x 整体和其他基本表达式一样参与后缀运算符链：global x.y 会被解析成 (global x).y") {
    CHECK(parse_json(U"global x.y") == nlohmann::json{
          {"type", "Attr"}, {"object", {{"type", "Global"}, {"identifier", "x"}}}, {"attr", "y"}
          });
}

TEST_CASE("缺少标识符时报错") {
    CHECK_THROWS_AS(parse_program(U"global"), SyntaxError);
}

TEST_CASE("一行内可以有多条 global（用分号分隔）") {
    CHECK(parse_program_json(U"global x; global y") == nlohmann::json{
          {"type", "Program"},
          {
          "exprs", nlohmann::json::array({
              {{"type", "Global"}, {"identifier", "x"}}, {{"type", "Global"}, {"identifier", "y"}}
              })
          }
          });
}

}
