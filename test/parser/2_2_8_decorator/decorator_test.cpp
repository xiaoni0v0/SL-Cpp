// SL.md 2.2.8 装饰器表达式：
//   紧邻 func/class 的 @decorator 属于函数/类表达式自己的语法，直接挂到 decorators_ 上；
//   通用形式 @d1 @d2 ... expr ≡ d1(d2(...(expr)))，从最贴近 expr 的装饰器开始向外包裹，
//   包成 AstNodeDecorator 链。
#include "../../../builtins/exceptions/SyntaxError.h"
#include "../test_utils.h"

#include <doctest/doctest.h>

namespace {
nlohmann::json ident(const char *name) {
    return nlohmann::json{{"type", "Identifier"}, {"identifier", name}};
}
} // namespace

TEST_SUITE("2.2.8 装饰器——紧邻 func/class") {

    TEST_CASE("单个装饰器挂到 func 的 decorators_") {
        CHECK(
            parse_json(U"@dec func f() {}") ==
            nlohmann::json{
                {"type", "Func"},
                {"decorators", nlohmann::json::array({ident("dec")})},
                {"name", "f"},
                {"captures", nlohmann::json::array()},
                {"params",
                 {{"positional", nlohmann::json::array()},
                  {"var_args", nullptr},
                  {"kw_only", nlohmann::json::array()},
                  {"var_kwargs", nullptr}}},
                {"return_type", nullptr},
                {"doc", nullptr},
                {"body", {{"type", "Program"}, {"exprs", nlohmann::json::array()}}}
            }
        );
    }

    TEST_CASE("多个装饰器按书写顺序进 decorators_") {
        CHECK(
            parse_json(U"@dec1 @dec2 func f() {}")["decorators"] ==
            nlohmann::json::array({ident("dec1"), ident("dec2")})
        );
    }

    TEST_CASE("挂到 class 的 decorators_") {
        CHECK(
            parse_json(U"@dec class C {}") ==
            nlohmann::json{
                {"type", "Class"},
                {"decorators", nlohmann::json::array({ident("dec")})},
                {"name", "C"},
                {"bases", nlohmann::json::array()},
                {"captures", nlohmann::json::array()},
                {"doc", nullptr},
                {"body", {{"type", "Program"}, {"exprs", nlohmann::json::array()}}}
            }
        );
    }

    TEST_CASE("装饰器表达式本身可以是调用") {
        CHECK(
            parse_json(U"@dec(1, 2) func f() {}")["decorators"] ==
            nlohmann::json::array({nlohmann::json{
                {"type", "Call"},
                {"object", ident("dec")},
                {"args", nlohmann::json::array(
                             {nlohmann::json::parse(R"({"type":"LiteralInt","raw":"1"})"),
                              nlohmann::json::parse(R"({"type":"LiteralInt","raw":"2"})")}
                         )},
                {"kwargs", nlohmann::json::array()}
            }})
        );
    }
}

TEST_SUITE("2.2.8 装饰器——通用形式") {

    TEST_CASE("单个装饰器包裹普通表达式") {
        CHECK(
            parse_json(U"@dec x") ==
            nlohmann::json{
                {"type", "Decorator"}, {"decorator", ident("dec")}, {"target", ident("x")}
            }
        );
    }

    TEST_CASE("多个装饰器：从最贴近 expr 的开始向外包裹，@d1 @d2 x == d1(d2(x))") {
        CHECK(
            parse_json(U"@d1 @d2 x") ==
            nlohmann::json{
                {"type", "Decorator"},
                {"decorator", ident("d1")},
                {"target",
                 {{"type", "Decorator"}, {"decorator", ident("d2")}, {"target", ident("x")}}}
            }
        );
    }

    TEST_CASE("装饰器目标是赋值表达式") {
        CHECK(
            parse_json(U"@dec x = 1") ==
            nlohmann::json{
                {"type", "Decorator"},
                {"decorator", ident("dec")},
                {"target",
                 {{"type", "Assign"},
                  {"target", ident("x")},
                  {"value", nlohmann::json::parse(R"({"type":"LiteralInt","raw":"1"})")}}}
            }
        );
    }

    TEST_CASE("装饰器后面缺表达式报错") { CHECK_THROWS_AS(parse_program(U"@dec"), SyntaxError); }
}
