// SL.md 2.2.6 函数表达式：
// ⟦@decorator ...⟧ func ⟦identifier⟧ ⟦[ALL_CAPTURE]⟧ (ALL_PARAM) ⟦: type⟧ ⟦doc⟧ { expr1; ... }
// 装饰器紧邻 func 的情况放在 2_2_8_decorator/decorator_test.cpp 测，这里只测 func 自身。
// 形参在 json 里嵌套成一个 "params" 对象，依次由 4 段组成：positional（*args 之前）/ var_args /
// kw_only（*args 之后）/ var_kwargs，见 SL.md 2.2.6。"无默认值形参必须排在有默认值形参前面"是
// 语义层校验（见 test/analyzer/syntax_checker），这里只测语法形状本身，以及"至多一个
// *args""**kwargs 必须 最后"这两条现在由 Parser 直接保证的规则。
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

nlohmann::json
param(const char *id, const nlohmann::json &type_annotation, const nlohmann::json &default_value) {
    return nlohmann::json{
        {"identifier", id}, {"type_annotation", type_annotation}, {"default_value", default_value}
    };
}

// 形参整体（AstNodeFunc::AllParams 对应的 json 形状），4 个子段都给了默认值方便只关心其中一段的用例
nlohmann::json all_params(
    const nlohmann::json &positional = nlohmann::json::array(),
    const nlohmann::json &var_args = nullptr,
    const nlohmann::json &kw_only = nlohmann::json::array(),
    const nlohmann::json &var_kwargs = nullptr
) {
    return nlohmann::json{
        {"positional", positional},
        {"var_args_name", var_args},
        {"kw_only", kw_only},
        {"var_kwargs_name", var_kwargs}
    };
}
} // namespace

TEST_SUITE("2.2.6 func——基本形状") {

    TEST_CASE("最简单的具名函数") {
        CHECK(
            parse_json(U"func f() {}") ==
            nlohmann::json{
                {"type", "Func"},
                {"decorators", nlohmann::json::array()},
                {"name", "f"},
                {"captures", nlohmann::json::array()},
                {"params", all_params()},
                {"return_type", nullptr},
                {"doc", nullptr},
                {"body", {{"type", "Program"}, {"exprs", nlohmann::json::array()}}}
            }
        );
    }

    TEST_CASE("匿名函数：省略名字") {
        CHECK(
            parse_json(U"func () {}") ==
            nlohmann::json{
                {"type", "Func"},
                {"decorators", nlohmann::json::array()},
                {"name", nullptr},
                {"captures", nlohmann::json::array()},
                {"params", all_params()},
                {"return_type", nullptr},
                {"doc", nullptr},
                {"body", {{"type", "Program"}, {"exprs", nlohmann::json::array()}}}
            }
        );
    }

    TEST_CASE("函数体含多条表达式") {
        const AstNodePtr node{parse_single(U"func f() { a; b }")};
        const auto *func_node{dynamic_cast<AstNodeFunc *>(node.get())};
        REQUIRE(func_node != nullptr);
        CHECK(func_node->body_->exprs_.size() == 2);
    }

    TEST_CASE("函数是普通表达式，可以直接赋值给变量") {
        CHECK_NOTHROW(parse_program(U"x = func() {}"));
    }
}

TEST_SUITE("2.2.6 func——形参") {

    TEST_CASE("普通形参") {
        CHECK(
            parse_json(U"func f(a, b) {}")["params"]["positional"] ==
            nlohmann::json::array({param("a", nullptr, nullptr), param("b", nullptr, nullptr)})
        );
    }

    TEST_CASE("形参尾逗号") {
        CHECK(
            parse_json(U"func f(a, b,) {}")["params"]["positional"] ==
            nlohmann::json::array({param("a", nullptr, nullptr), param("b", nullptr, nullptr)})
        );
    }

    TEST_CASE("类型注解") {
        CHECK(
            parse_json(U"func f(a: int) {}")["params"]["positional"] ==
            nlohmann::json::array({param("a", ident("int"), nullptr)})
        );
    }

    TEST_CASE("默认值") {
        CHECK(
            parse_json(U"func f(a = 1) {}")["params"]["positional"] ==
            nlohmann::json::array({param("a", nullptr, int_lit("1"))})
        );
    }

    TEST_CASE("类型注解 + 默认值同时出现") {
        CHECK(
            parse_json(U"func f(a: int = 1) {}")["params"]["positional"] ==
            nlohmann::json::array({param("a", ident("int"), int_lit("1"))})
        );
    }

    TEST_CASE("*args / **kwargs：各自是独立的 var_args/var_kwargs 子段，不再混进 positional") {
        CHECK(
            parse_json(U"func f(*args, **kwargs) {}")["params"] ==
            all_params(nlohmann::json::array(), "args", nlohmann::json::array(), "kwargs")
        );
    }

    TEST_CASE("*args 之后的普通形参进 kw_only，支持类型注解/默认值，形状跟 positional 一样") {
        const auto j = parse_json(U"func f(*args, x: int = 1, y) {}")["params"];
        CHECK(j["var_args_name"] == "args");
        CHECK(
            j["kw_only"] ==
            nlohmann::json::array(
                {param("x", ident("int"), int_lit("1")), param("y", nullptr, nullptr)}
            )
        );
    }

    TEST_CASE("只有 *args 没有 **kwargs，或者只有 **kwargs 没有 *args") {
        CHECK(
            parse_json(U"func f(*args) {}")["params"] ==
            all_params(nlohmann::json::array(), "args", nlohmann::json::array(), nullptr)
        );
        CHECK(
            parse_json(U"func f(**kwargs) {}")["params"] ==
            all_params(nlohmann::json::array(), nullptr, nlohmann::json::array(), "kwargs")
        );
    }

    TEST_CASE("形参列表为空") { CHECK(parse_json(U"func f() {}")["params"] == all_params()); }

    TEST_CASE("缺少括号/未闭合报错") {
        CHECK_THROWS_AS(parse_program(U"func f {}"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"func f(a {}"), SyntaxError);
    }

    TEST_CASE("形参列表未闭合的消息明确说'parameter list'，位置指向多出来的 '{'（不是 EOF）") {
        // "func f(a {}" -> f(1)u(2)n(3)c(4) (5)f(6)((7)a(8) (9){(10)}(11)
        try {
            parse_program(U"func f(a {}");
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("close parameter list") != std::string::npos);
            CHECK(msg.find("1:10:") != std::string::npos); // '{'
        }
    }

    TEST_CASE("至多一个 *args，第二个直接在 Parser 层报错") {
        check_parse_throws_with(U"func f(*x, *y) {}", "at most one *args parameter is allowed");
    }

    TEST_CASE("至多一个 **kwargs") {
        check_parse_throws_with(
            U"func f(**x, **y) {}", "at most one **kwargs parameter is allowed"
        );
    }

    TEST_CASE("**kwargs 必须是形参列表最后一项，后面不能再有任何形参") {
        check_parse_throws_with(U"func f(**kw, x) {}", "no parameter is allowed after **kwargs");
        check_parse_throws_with(U"func f(**kw, *y) {}", "no parameter is allowed after **kwargs");
        // 第二个 **kwargs 命中的是"至多一个 **kwargs"这条更具体的检查，不是笼统的"后面不能有形参"
        check_parse_throws_with(
            U"func f(**kw, **kw2) {}", "at most one **kwargs parameter is allowed"
        );
    }
}

TEST_SUITE("2.2.6 func——捕获列表") {

    TEST_CASE("值捕获（裸标识符）") {
        CHECK(
            parse_json(U"func f[x]() {}")["captures"] ==
            nlohmann::json::array(
                {nlohmann::json{{"capture_type", "Value"}, {"identifier", "x"}, {"value_expr", nullptr}}}
            )
        );
    }

    TEST_CASE("值捕获（显式表达式）") {
        CHECK(
            parse_json(U"func f[x = 1 + 2]() {}")["captures"] ==
            nlohmann::json::array({nlohmann::json{
                {"capture_type", "Value"},
                {"identifier", "x"},
                {"value_expr",
                 {{"type", "OpBinary"},
                  {"op", "+"},
                  {"left", int_lit("1")},
                  {"right", int_lit("2")}}}
            }})
        );
    }

    TEST_CASE("引用捕获") {
        CHECK(
            parse_json(U"func f[&y]() {}")["captures"] ==
            nlohmann::json::array({nlohmann::json{
                {"capture_type", "Reference"}, {"identifier", "y"}, {"value_expr", nullptr}
            }})
        );
    }

    TEST_CASE("混合捕获，空捕获列表 []") {
        CHECK(
            parse_json(U"func f[x, &y, z = 1]() {}")["captures"] ==
            nlohmann::json::array(
                {nlohmann::json{{"capture_type", "Value"}, {"identifier", "x"}, {"value_expr", nullptr}},
                 nlohmann::json{
                     {"capture_type", "Reference"}, {"identifier", "y"}, {"value_expr", nullptr}
                 },
                 nlohmann::json{
                     {"capture_type", "Value"}, {"identifier", "z"}, {"value_expr", int_lit("1")}
                 }}
            )
        );
        CHECK(parse_json(U"func f[]() {}")["captures"] == nlohmann::json::array());
    }

    TEST_CASE("捕获列表未闭合的消息明确说'capture list'，位置指向多出来的 '{'（不是 EOF）") {
        // "func f[x {}" -> f(1)u(2)n(3)c(4) (5)f(6)[(7)x(8) (9){(10)}(11)
        try {
            parse_program(U"func f[x {}");
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("close capture list") != std::string::npos);
            CHECK(msg.find("1:10:") != std::string::npos); // '{'
        }
    }
}

TEST_SUITE("2.2.6 func——返回类型/文档字符串") {

    TEST_CASE("返回类型") {
        // 注意：用 = 而不是 auto j{...}——花括号初始化一个已经构造好的 json 对象，
        // 会被当成"用这一个元素构造数组"，而不是拷贝这个对象本身
        const auto j = parse_json(U"func f(): int {}");
        CHECK(j["return_type"] == ident("int"));
    }

    TEST_CASE("文档字符串（紧跟在形参/返回类型之后，函数体之前，没有冒号等前缀）") {
        const auto j = parse_json(U"func f() 'doc' {}");
        CHECK(j["doc"] == nlohmann::json{{"type", "LiteralStr"}, {"value", "doc"}});
    }

    TEST_CASE("返回类型和文档字符串同时出现") {
        const auto j = parse_json(U"func f(): int 'doc' {}");
        CHECK(j["return_type"] == ident("int"));
        CHECK(j["doc"] == nlohmann::json{{"type", "LiteralStr"}, {"value", "doc"}});
    }
}
