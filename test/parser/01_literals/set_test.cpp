// 集合的基本结构、表达式项和展开。
#include "../test_utils.h"

#include <doctest/doctest.h>

namespace {

nlohmann::json set(const nlohmann::json &items) {
    return nlohmann::json{{"type", "LiteralSet"}, {"items", items}};
}

nlohmann::json star(const char *name) {
    return nlohmann::json{{"type", "Star"}, {"operand", ident(name)}};
}

} // namespace

TEST_SUITE("集合") {

    TEST_CASE("基本、尾逗号") {
        CHECK(
            parse_json(U"${1, 'a',}") == nlohmann::json::parse(
                                             R"({"type":"LiteralSet","items":[
                  {"type":"LiteralInt","raw":"1"},{"type":"LiteralStr","value":"a"}
              ]})"
                                         )
        );
    }

    TEST_CASE("空集合没有字面量写法：${} 是空字典") {
        CHECK(parse_json(U"${}")["type"] == "LiteralDict");
    }
}

TEST_SUITE("${} 集合") {

    TEST_CASE("普通项，尾逗号可选") {
        CHECK(parse_json(U"${a}") == set(nlohmann::json::array({ident("a")})));
        CHECK(parse_json(U"${a, b,}") == set(nlohmann::json::array({ident("a"), ident("b")})));
        CHECK(parse_json(U"${\na,\nb\n}") == set(nlohmann::json::array({ident("a"), ident("b")})));
    }

    TEST_CASE("* 展开项") {
        CHECK(parse_json(U"${*s}") == set(nlohmann::json::array({star("s")})));
        CHECK(parse_json(U"${a, *s}") == set(nlohmann::json::array({ident("a"), star("s")})));
        CHECK(parse_json(U"${(*s)}") == parse_json(U"${*s}"));
    }

    TEST_CASE("项可以直接是声明") {
        CHECK(
            parse_json(U"${x: int}") ==
            set(nlohmann::json::array({declare("x", false, ident("int"), nullptr)}))
        );
        CHECK(parse_json(U"${(x: int)}") == parse_json(U"${x: int}"));
        CHECK(
            parse_json(U"${x: const = 1, y}") ==
            set(nlohmann::json::array({declare("x", true, nullptr, int_lit("1")), ident("y")}))
        );
    }

    TEST_CASE("项可以是裸赋值和复合赋值") {
        CHECK(
            parse_json(U"${x = 1}") ==
            set(nlohmann::json::array({bare_assign(ident("x"), int_lit("1"))}))
        );
        CHECK(parse_json(U"${x += 1}")["items"][0]["type"] == "CompoundAssign");
    }

    TEST_CASE("项的根不是 ** 展开时不算字典展开项") {
        // 根是加法，没有 '->'，所以是集合；里面的 ** 合不合法由语义层判
        CHECK(parse_json(U"${**d + x}")["type"] == "LiteralSet");
    }

    TEST_CASE("未闭合报错") {
        check_parse_throws_with(U"${a, b", "expected '}' to close set literal");
    }
}
