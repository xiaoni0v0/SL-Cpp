// SL.md 2.2.6 函数表达式：
// ⟦@decorator ...⟧ func ⟦identifier⟧ ⟦[ALL_CAPTURE]⟧ (ALL_PARAM) ⟦: type⟧ ⟦doc⟧ { expr1; ... }
// 装饰器紧邻 func 的情况放在 2_2_8_decorator/decorator_test.cpp 测，这里只测 func 自身。
// 形参顺序合法性（无默认值的必须排在有默认值/*args 前面等）是语义层校验，这里不测。
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

nlohmann::json param(const char *id, const char *param_type, const nlohmann::json &type_annotation,
                     const nlohmann::json &default_value) {
    return nlohmann::json{
        {"identifier", id}, {"param_type", param_type},
        {"type_annotation", type_annotation}, {"default_value", default_value}
    };
}
} // namespace

TEST_SUITE("2.2.6 func——基本形状") {

TEST_CASE("最简单的具名函数") {
    CHECK(parse_json(U"func f() {}") == nlohmann::json{
          {"type", "Func"}, {"decorators", nlohmann::json::array()}, {"name", "f"},
          {"captures", nlohmann::json::array()}, {"params", nlohmann::json::array()},
          {"return_type", nullptr}, {"doc", nullptr},
          {"body", {{"type", "Program"}, {"exprs", nlohmann::json::array()}}}
          });
}

TEST_CASE("匿名函数：省略名字") {
    CHECK(parse_json(U"func () {}") == nlohmann::json{
          {"type", "Func"}, {"decorators", nlohmann::json::array()}, {"name", nullptr},
          {"captures", nlohmann::json::array()}, {"params", nlohmann::json::array()},
          {"return_type", nullptr}, {"doc", nullptr},
          {"body", {{"type", "Program"}, {"exprs", nlohmann::json::array()}}}
          });
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
    CHECK(parse_json(U"func f(a, b) {}")["params"] == nlohmann::json::array({
        param("a", "Normal", nullptr, nullptr), param("b", "Normal", nullptr, nullptr)
        }));
}

TEST_CASE("形参尾逗号") {
    CHECK(parse_json(U"func f(a, b,) {}")["params"] == nlohmann::json::array({
        param("a", "Normal", nullptr, nullptr), param("b", "Normal", nullptr, nullptr)
        }));
}

TEST_CASE("类型注解") {
    CHECK(parse_json(U"func f(a: int) {}")["params"] == nlohmann::json::array({
        param("a", "Normal", ident("int"), nullptr)
        }));
}

TEST_CASE("默认值") {
    CHECK(parse_json(U"func f(a = 1) {}")["params"] == nlohmann::json::array({
        param("a", "Normal", nullptr, int_lit("1"))
        }));
}

TEST_CASE("类型注解 + 默认值同时出现") {
    CHECK(parse_json(U"func f(a: int = 1) {}")["params"] == nlohmann::json::array({
        param("a", "Normal", ident("int"), int_lit("1"))
        }));
}

TEST_CASE("*args / **kwargs") {
    CHECK(parse_json(U"func f(*args, **kwargs) {}")["params"] == nlohmann::json::array({
        param("args", "StarArgs", nullptr, nullptr), param("kwargs", "DoubleStarKwargs", nullptr, nullptr)
        }));
}

TEST_CASE("形参列表为空") {
    CHECK(parse_json(U"func f() {}")["params"] == nlohmann::json::array());
}

TEST_CASE("缺少括号/未闭合报错") {
    CHECK_THROWS_AS(parse_program(U"func f {}"), SyntaxError);
    CHECK_THROWS_AS(parse_program(U"func f(a {}"), SyntaxError);
}

}

TEST_SUITE("2.2.6 func——捕获列表") {

TEST_CASE("值捕获（裸标识符）") {
    CHECK(parse_json(U"func f[x]() {}")["captures"] == nlohmann::json::array({
        nlohmann::json{{"kind", "Value"}, {"identifier", "x"}, {"value_expr", nullptr}}
        }));
}

TEST_CASE("值捕获（显式表达式）") {
    CHECK(parse_json(U"func f[x = 1 + 2]() {}")["captures"] == nlohmann::json::array({
        nlohmann::json{
        {"kind", "Value"}, {"identifier", "x"},
        {"value_expr", {{"type", "OpBinary"}, {"op", "+"}, {"left", int_lit("1")}, {"right", int_lit("2")}}}
        }
        }));
}

TEST_CASE("引用捕获") {
    CHECK(parse_json(U"func f[&y]() {}")["captures"] == nlohmann::json::array({
        nlohmann::json{{"kind", "Reference"}, {"identifier", "y"}, {"value_expr", nullptr}}
        }));
}

TEST_CASE("混合捕获，空捕获列表 []") {
    CHECK(parse_json(U"func f[x, &y, z = 1]() {}")["captures"] == nlohmann::json::array({
        nlohmann::json{{"kind", "Value"}, {"identifier", "x"}, {"value_expr", nullptr}},
        nlohmann::json{{"kind", "Reference"}, {"identifier", "y"}, {"value_expr", nullptr}},
        nlohmann::json{{"kind", "Value"}, {"identifier", "z"}, {"value_expr", int_lit("1")}}
        }));
    CHECK(parse_json(U"func f[]() {}")["captures"] == nlohmann::json::array());
}

}

TEST_SUITE("2.2.6 func——返回类型/文档字符串") {

TEST_CASE("返回类型") {
    const auto j{parse_json(U"func f(): int {}")};
    CHECK(j["return_type"] == ident("int"));
}

TEST_CASE("文档字符串（紧跟在形参/返回类型之后，函数体之前，没有冒号等前缀）") {
    const auto j{parse_json(U"func f() 'doc' {}")};
    CHECK(j["doc"] == nlohmann::json{{"type", "LiteralStr"}, {"value", "doc"}});
}

TEST_CASE("返回类型和文档字符串同时出现") {
    const auto j{parse_json(U"func f(): int 'doc' {}")};
    CHECK(j["return_type"] == ident("int"));
    CHECK(j["doc"] == nlohmann::json{{"type", "LiteralStr"}, {"value", "doc"}});
}

}
