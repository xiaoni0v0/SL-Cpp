// 紧邻 func/class 的 @ 挂到节点上；否则包成 Decorator 链。
#include "../../../builtins/exceptions/SyntaxError.h"
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("装饰器——紧邻 func/class") {

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
                  {"var_args_name", nullptr},
                  {"kw_only", nlohmann::json::array()},
                  {"var_kwargs_name", nullptr}}},
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
                {"positional_args",
                 nlohmann::json::array(
                     {nlohmann::json::parse(R"({"type":"LiteralInt","raw":"1"})"),
                      nlohmann::json::parse(R"({"type":"LiteralInt","raw":"2"})")}
                 )},
                {"keyword_args", nlohmann::json::array()}
            }})
        );
    }
}

TEST_SUITE("装饰器——通用形式") {

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

    TEST_CASE("带参装饰器包裹非 func/class 的普通目标") {
        CHECK(
            parse_json(U"@dec(1, 2) x") ==
            nlohmann::json{
                {"type", "Decorator"},
                {"decorator",
                 {{"type", "Call"},
                  {"object", ident("dec")},
                  {"positional_args",
                   nlohmann::json::array(
                       {nlohmann::json::parse(R"({"type":"LiteralInt","raw":"1"})"),
                        nlohmann::json::parse(R"({"type":"LiteralInt","raw":"2"})")}
                   )},
                  {"keyword_args", nlohmann::json::array()}}},
                {"target", ident("x")}
            }
        );
    }

    TEST_CASE("装饰器比一切运算符都松：@d x + y 的目标是整个 x + y，不是只到 x") {
        CHECK(
            parse_json(U"@d x + y") ==
            nlohmann::json{
                {"type", "Decorator"},
                {"decorator", ident("d")},
                {"target",
                 {{"type", "OpBinary"}, {"op", "+"}, {"left", ident("x")}, {"right", ident("y")}}}
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

    TEST_CASE("装饰器后面缺表达式报错") { CHECK_THROWS_AS(parse_as_file(U"@dec"), SyntaxError); }

    TEST_CASE("@ 之后 Pratt 把中缀吃进装饰器：@d + x 的装饰器是 d+x，后面没有目标") {
        CHECK_THROWS_AS(parse_as_file(U"@d + x"), SyntaxError);
        CHECK_THROWS_AS(parse_as_file(U"@d * x"), SyntaxError);
    }

    TEST_CASE("@d.attr x：'.' 是后缀，装饰器停在 d.attr，x 是目标") {
        CHECK(
            parse_json(U"@d.attr x") ==
            nlohmann::json{
                {"type", "Decorator"},
                {"decorator", {{"type", "Attr"}, {"object", ident("d")}, {"attr", "attr"}}},
                {"target", ident("x")}
            }
        );
    }
}
