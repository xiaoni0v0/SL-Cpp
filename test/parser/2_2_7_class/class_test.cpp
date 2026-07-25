// SL.md 2.2.7 类表达式：⟦@decorator ...⟧ class ⟦name⟧ ⟦(bases)⟧ ⟦[captures]⟧ ⟦doc⟧ { body }
// 装饰器紧邻 class 的情况放在 2_2_8_decorator/decorator_test.cpp 测。
// 捕获列表语法/语义跟 func 的完全一致（见 SL.md 2.2.6/2.2.7），细节各种组合已经在
// 2_2_6_func/func_test.cpp 里测过一遍，这里只补类特有的：位置在基类之后、未闭合报错。
#include "../../../builtins/exceptions/SyntaxError.h"
#include "../test_utils.h"

#include <doctest/doctest.h>

namespace {
nlohmann::json ident(const char *name) {
    return nlohmann::json{{"type", "Identifier"}, {"identifier", name}};
}

nlohmann::json int_lit(const char *raw) {
    return nlohmann::json::parse(R"({"type":"LiteralInt","raw":")" + std::string{raw} + R"("})");
}
} // namespace

TEST_SUITE("2.2.7 class") {

    TEST_CASE("最简单的具名类") {
        CHECK(
            parse_json(U"class C {}") ==
            nlohmann::json{
                {"type", "Class"},
                {"decorators", nlohmann::json::array()},
                {"name", "C"},
                {"bases", nlohmann::json::array()},
                {"captures", nlohmann::json::array()},
                {"doc", nullptr},
                {"body", {{"type", "Program"}, {"exprs", nlohmann::json::array()}}}
            }
        );
    }

    TEST_CASE("匿名类：省略名字") {
        CHECK(
            parse_json(U"class {}") ==
            nlohmann::json{
                {"type", "Class"},
                {"decorators", nlohmann::json::array()},
                {"name", nullptr},
                {"bases", nlohmann::json::array()},
                {"captures", nlohmann::json::array()},
                {"doc", nullptr},
                {"body", {{"type", "Program"}, {"exprs", nlohmann::json::array()}}}
            }
        );
    }

    TEST_CASE("带基类列表") {
        CHECK(
            parse_json(U"class C(Base1, Base2) {}") ==
            nlohmann::json{
                {"type", "Class"},
                {"decorators", nlohmann::json::array()},
                {"name", "C"},
                {"bases", nlohmann::json::array({ident("Base1"), ident("Base2")})},
                {"captures", nlohmann::json::array()},
                {"doc", nullptr},
                {"body", {{"type", "Program"}, {"exprs", nlohmann::json::array()}}}
            }
        );
    }

    TEST_CASE("空基类列表 ()") {
        CHECK(
            parse_json(U"class C() {}") ==
            nlohmann::json{
                {"type", "Class"},
                {"decorators", nlohmann::json::array()},
                {"name", "C"},
                {"bases", nlohmann::json::array()},
                {"captures", nlohmann::json::array()},
                {"doc", nullptr},
                {"body", {{"type", "Program"}, {"exprs", nlohmann::json::array()}}}
            }
        );
    }

    TEST_CASE("文档字符串") {
        CHECK(
            parse_json(U"class C 'doc' {}") ==
            nlohmann::json{
                {"type", "Class"},
                {"decorators", nlohmann::json::array()},
                {"name", "C"},
                {"bases", nlohmann::json::array()},
                {"captures", nlohmann::json::array()},
                {"doc", {{"type", "LiteralStr"}, {"value", "doc"}}},
                {"body", {{"type", "Program"}, {"exprs", nlohmann::json::array()}}}
            }
        );
    }

    TEST_CASE("类体含多条表达式") {
        const AstNodePtr node{parse_single(U"class C { x = 1; y = 2 }")};
        const auto *cls{dynamic_cast<AstNodeClass *>(node.get())};
        REQUIRE(cls != nullptr);
        CHECK(cls->body_->exprs_.size() == 2);
    }

    TEST_CASE("基类可以是任意表达式，比如调用") {
        CHECK(
            parse_json(U"class C(make_base()) {}")["bases"] ==
            nlohmann::json::array({nlohmann::json{
                {"type", "Call"},
                {"object", ident("make_base")},
                {"args", nlohmann::json::array()},
                {"kwargs", nlohmann::json::array()}
            }})
        );
    }

    TEST_CASE("类是普通表达式，可以直接赋值给变量") {
        CHECK_NOTHROW(parse_program(U"x = class {}"));
    }

    TEST_CASE("未闭合基类列表/类体报错") {
        CHECK_THROWS_AS(parse_program(U"class C(Base {}"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"class C {"), SyntaxError);
    }

    TEST_CASE("基类列表未闭合的消息明确说'base class list'，位置指向多出来的 '{'（不是 EOF）") {
        try {
            parse_program(U"class C(Base {}");
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("close base class list") != std::string::npos);
            CHECK(msg.find("1:14:") != std::string::npos);
        }
    }
}

TEST_SUITE("2.2.7 class——捕获列表") {

    TEST_CASE("值捕获（裸标识符/显式表达式）、引用捕获、混合捕获、空捕获列表 []") {
        CHECK(
            parse_json(U"class C[x] {}")["captures"] ==
            nlohmann::json::array(
                {nlohmann::json{{"kind", "Value"}, {"identifier", "x"}, {"value_expr", nullptr}}}
            )
        );
        CHECK(
            parse_json(U"class C[x = 1 + 2] {}")["captures"] ==
            nlohmann::json::array({nlohmann::json{
                {"kind", "Value"},
                {"identifier", "x"},
                {"value_expr",
                 {{"type", "OpBinary"},
                  {"op", "+"},
                  {"left", int_lit("1")},
                  {"right", int_lit("2")}}}
            }})
        );
        CHECK(
            parse_json(U"class C[&y] {}")["captures"] ==
            nlohmann::json::array({nlohmann::json{
                {"kind", "Reference"}, {"identifier", "y"}, {"value_expr", nullptr}
            }})
        );
        CHECK(
            parse_json(U"class C[x, &y, z = 1] {}")["captures"] ==
            nlohmann::json::array(
                {nlohmann::json{{"kind", "Value"}, {"identifier", "x"}, {"value_expr", nullptr}},
                 nlohmann::json{
                     {"kind", "Reference"}, {"identifier", "y"}, {"value_expr", nullptr}
                 },
                 nlohmann::json{
                     {"kind", "Value"}, {"identifier", "z"}, {"value_expr", int_lit("1")}
                 }}
            )
        );
        CHECK(parse_json(U"class C[] {}")["captures"] == nlohmann::json::array());
    }

    TEST_CASE("捕获列表位于基类列表之后，两者可以同时出现、互不影响") {
        const auto j = parse_json(U"class C(Base)[x] {}");
        CHECK(j["bases"] == nlohmann::json::array({ident("Base")}));
        CHECK(
            j["captures"] ==
            nlohmann::json::array(
                {nlohmann::json{{"kind", "Value"}, {"identifier", "x"}, {"value_expr", nullptr}}}
            )
        );
    }

    TEST_CASE("匿名类、无基类列表时捕获列表照样能单独出现") {
        CHECK(
            parse_json(U"class [x] {}")["captures"] ==
            nlohmann::json::array(
                {nlohmann::json{{"kind", "Value"}, {"identifier", "x"}, {"value_expr", nullptr}}}
            )
        );
    }

    TEST_CASE("捕获列表未闭合的消息明确说'capture list'，位置指向多出来的 '{'（不是 EOF）") {
        try {
            parse_program(U"class C[x {}");
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("close capture list") != std::string::npos);
            CHECK(msg.find("1:11:") != std::string::npos);
        }
    }
}

TEST_SUITE("2.2.7 class——匿名类 + 基类 + 捕获 + 文档字符串全部组合") {

    TEST_CASE("四项齐全：匿名 + 基类列表 + 捕获列表 + 文档字符串") {
        CHECK(
            parse_json(U"class (Base1, Base2)[x, &y]'doc' {}") ==
            nlohmann::json{
                {"type", "Class"},
                {"decorators", nlohmann::json::array()},
                {"name", nullptr},
                {"bases", nlohmann::json::array({ident("Base1"), ident("Base2")})},
                {"captures",
                 nlohmann::json::array(
                     {{{"kind", "Value"}, {"identifier", "x"}, {"value_expr", nullptr}},
                      {{"kind", "Reference"}, {"identifier", "y"}, {"value_expr", nullptr}}}
                 )},
                {"doc", {{"type", "LiteralStr"}, {"value", "doc"}}},
                {"body", {{"type", "Program"}, {"exprs", nlohmann::json::array()}}}
            }
        );
    }

    TEST_CASE("匿名 + 空基类列表 + 值捕获带表达式 + 文档字符串") {
        CHECK(
            parse_json(U"class ()[a = 1 + 2]'doc' {}") ==
            nlohmann::json{
                {"type", "Class"},
                {"decorators", nlohmann::json::array()},
                {"name", nullptr},
                {"bases", nlohmann::json::array()},
                {"captures",
                 nlohmann::json::array(
                     {{{"kind", "Value"},
                       {"identifier", "a"},
                       {"value_expr",
                        {{"type", "OpBinary"},
                         {"op", "+"},
                         {"left", int_lit("1")},
                         {"right", int_lit("2")}}}}}
                 )},
                {"doc", {{"type", "LiteralStr"}, {"value", "doc"}}},
                {"body", {{"type", "Program"}, {"exprs", nlohmann::json::array()}}}
            }
        );
    }
}
