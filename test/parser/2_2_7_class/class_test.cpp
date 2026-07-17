// SL.md 2.2.7 类表达式：⟦@decorator ...⟧ class ⟦name⟧ ⟦(bases)⟧ ⟦doc⟧ { body }
// 装饰器紧邻 class 的情况放在 2_2_8_decorator/decorator_test.cpp 测。
#include "../test_utils.h"
#include "../../../builtins/classes/exceptions/SyntaxError.h"

#include <doctest/doctest.h>

namespace {
nlohmann::json ident(const char *name) {
    return nlohmann::json{{"type", "Identifier"}, {"identifier", name}};
}
} // namespace

TEST_SUITE("2.2.7 class") {

TEST_CASE("最简单的具名类") {
    CHECK(parse_json(U"class C {}") == nlohmann::json{
          {"type", "Class"}, {"decorators", nlohmann::json::array()}, {"name", "C"},
          {"bases", nlohmann::json::array()}, {"doc", nullptr},
          {"body", {{"type", "Program"}, {"exprs", nlohmann::json::array()}}}
          });
}

TEST_CASE("匿名类：省略名字") {
    CHECK(parse_json(U"class {}") == nlohmann::json{
          {"type", "Class"}, {"decorators", nlohmann::json::array()}, {"name", nullptr},
          {"bases", nlohmann::json::array()}, {"doc", nullptr},
          {"body", {{"type", "Program"}, {"exprs", nlohmann::json::array()}}}
          });
}

TEST_CASE("带基类列表") {
    CHECK(parse_json(U"class C(Base1, Base2) {}") == nlohmann::json{
          {"type", "Class"}, {"decorators", nlohmann::json::array()}, {"name", "C"},
          {"bases", nlohmann::json::array({ident("Base1"), ident("Base2")})}, {"doc", nullptr},
          {"body", {{"type", "Program"}, {"exprs", nlohmann::json::array()}}}
          });
}

TEST_CASE("空基类列表 ()") {
    CHECK(parse_json(U"class C() {}") == nlohmann::json{
          {"type", "Class"}, {"decorators", nlohmann::json::array()}, {"name", "C"},
          {"bases", nlohmann::json::array()}, {"doc", nullptr},
          {"body", {{"type", "Program"}, {"exprs", nlohmann::json::array()}}}
          });
}

TEST_CASE("文档字符串") {
    CHECK(parse_json(U"class C 'doc' {}") == nlohmann::json{
          {"type", "Class"}, {"decorators", nlohmann::json::array()}, {"name", "C"},
          {"bases", nlohmann::json::array()}, {"doc", {{"type", "LiteralStr"}, {"value", "doc"}}},
          {"body", {{"type", "Program"}, {"exprs", nlohmann::json::array()}}}
          });
}

TEST_CASE("类体含多条表达式") {
    const AstNodePtr node{parse_single(U"class C { x = 1; y = 2 }")};
    const auto *cls{dynamic_cast<AstNodeClass *>(node.get())};
    REQUIRE(cls != nullptr);
    CHECK(cls->body_->exprs_.size() == 2);
}

TEST_CASE("基类可以是任意表达式，比如调用") {
    CHECK(parse_json(U"class C(make_base()) {}")["bases"] == nlohmann::json::array({
        nlohmann::json{
        {"type", "Call"}, {"object", ident("make_base")},
        {"args", nlohmann::json::array()}, {"kwargs", nlohmann::json::array()}
        }
        }));
}

TEST_CASE("类是普通表达式，可以直接赋值给变量") {
    CHECK_NOTHROW(parse_program(U"x = class {}"));
}

TEST_CASE("未闭合基类列表/类体报错") {
    CHECK_THROWS_AS(parse_program(U"class C(Base {}"), SyntaxError);
    CHECK_THROWS_AS(parse_program(U"class C {"), SyntaxError);
}

}
