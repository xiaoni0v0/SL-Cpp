// SL.md 2.2.3 del 表达式——语法：del target。
// 语法层只解析一个表达式当 target，"target 必须是标识符或属性访问"
// 是语义层（SyntaxChecker）的校验， 不是 parser
// 的事，所以这里故意也测了几个语义上非法、但语法上能正常解析出来的 target。
#include "../../../builtins/exceptions/SyntaxError.h"
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("2.2.3 del") {

    TEST_CASE("del 标识符") {
        CHECK(
            parse_json(U"del x") ==
            nlohmann::json{
                {"type", "Del"}, {"target", {{"type", "Identifier"}, {"identifier", "x"}}}
            }
        );
    }

    TEST_CASE("del 属性访问") {
        CHECK(
            parse_json(U"del x.attr") ==
            nlohmann::json{
                {"type", "Del"},
                {"target",
                 {{"type", "Attr"},
                  {"object", {{"type", "Identifier"}, {"identifier", "x"}}},
                  {"attr", "attr"}}}
            }
        );
    }

    TEST_CASE(
        "语法层不限制 target 的形状：del 元素访问/字面量在语法层都能解析（合法性交给语义层）"
    ) {
        CHECK(
            parse_json(U"del x[0]") ==
            nlohmann::json{
                {"type", "Del"},
                {"target",
                 {{"type", "Index"},
                  {"object", {{"type", "Identifier"}, {"identifier", "x"}}},
                  {"args",
                   nlohmann::json::array(
                       {nlohmann::json::parse(R"({"type":"LiteralInt","raw":"0"})")}
                   )}}}
            }
        );
        CHECK(
            parse_json(U"del 5") ==
            nlohmann::json{
                {"type", "Del"},
                {"target", nlohmann::json::parse(R"({"type":"LiteralInt","raw":"5"})")}
            }
        );
    }

    TEST_CASE("del 后面必须有一个表达式，缺失时报错") {
        CHECK_THROWS_AS(parse_program(U"del"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"del\n"), SyntaxError);
    }
}
