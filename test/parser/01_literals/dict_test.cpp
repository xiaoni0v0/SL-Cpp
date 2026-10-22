// 字典的基本结构、键值表达式、展开和错误边界。
#include "../../../cppexceptions/SyntaxError.h"
#include "../test_utils.h"

#include <string>

#include <doctest/doctest.h>

namespace {

nlohmann::json compound(const nlohmann::json &exprs) {
    return nlohmann::json{{"type", "Compound"}, {"exprs", exprs}};
}

nlohmann::json dict(const nlohmann::json &items) {
    return nlohmann::json{{"type", "LiteralDict"}, {"items", items}};
}

nlohmann::json pair(const nlohmann::json &key, const nlohmann::json &value) {
    return nlohmann::json{{"key", key}, {"value", value}};
}

nlohmann::json spread(const char *name) {
    return pair(nlohmann::json{{"type", "DoubleStar"}, {"operand", ident(name)}}, nullptr);
}

} // namespace

TEST_SUITE("字典") {

    TEST_CASE("空字典") {
        CHECK(parse_json(U"${}") == nlohmann::json::parse(R"({"type":"LiteralDict","items":[]})"));
    }

    TEST_CASE("基本键值对") {
        CHECK(
            parse_json(U"${'a' -> 1}") == nlohmann::json::parse(
                                              R"({"type":"LiteralDict","items":[
                  {"key":{"type":"LiteralStr","value":"a"},"value":{"type":"LiteralInt","raw":"1"}}
              ]})"
                                          )
        );
    }

    TEST_CASE("多个键值对") {
        CHECK(
            parse_json(U"${'a' -> 1, 'b' -> 2}") == nlohmann::json::parse(
                                                        R"({"type":"LiteralDict","items":[
                  {"key":{"type":"LiteralStr","value":"a"},"value":{"type":"LiteralInt","raw":"1"}},
                  {"key":{"type":"LiteralStr","value":"b"},"value":{"type":"LiteralInt","raw":"2"}}
              ]})"
                                                    )
        );
    }

    TEST_CASE("键不限于字符串字面量，可以是任意表达式") {
        CHECK(
            parse_json(U"${1 -> 'a', (1+1) -> 'b'}") == nlohmann::json::parse(
                                                            R"({"type":"LiteralDict","items":[
                  {"key":{"type":"LiteralInt","raw":"1"},"value":{"type":"LiteralStr","value":"a"}},
                  {"key":{"type":"OpBinary","op":"+","left":{"type":"LiteralInt","raw":"1"},
                          "right":{"type":"LiteralInt","raw":"1"}},"value":{"type":"LiteralStr","value":"b"}}
              ]})"
                                                        )
        );
    }

    TEST_CASE("尾逗号可选") {
        CHECK(
            parse_json(U"${'a' -> 1,}") == nlohmann::json::parse(
                                               R"({"type":"LiteralDict","items":[
                  {"key":{"type":"LiteralStr","value":"a"},"value":{"type":"LiteralInt","raw":"1"}}
              ]})"
                                           )
        );
    }

    TEST_CASE("嵌套字典") {
        CHECK(
            parse_json(U"${'a' -> ${'b' -> 1}}") == nlohmann::json::parse(
                                                        R"({"type":"LiteralDict","items":[
                  {"key":{"type":"LiteralStr","value":"a"},"value":{"type":"LiteralDict","items":[
                      {"key":{"type":"LiteralStr","value":"b"},"value":{"type":"LiteralInt","raw":"1"}}
                  ]}}
              ]})"
                                                    )
        );
    }

    TEST_CASE("未闭合的字典抛异常，消息明确说是字典，位置指向 EOF") {
        // "${'a' -> 1" 共 10 个字符，EOF 在第 11 列
        try {
            parse_program(U"${'a' -> 1");
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("dict literal") != std::string::npos);
            CHECK(msg.find("1:11:") != std::string::npos);
        }
    }
}

TEST_SUITE("${} 字典") {

    TEST_CASE("${} 是空字典") { CHECK(parse_json(U"${}") == dict(nlohmann::json::array())); }

    TEST_CASE("$ 与 { 之间可以有空白和换行") {
        CHECK(parse_json(U"$ {}") == dict(nlohmann::json::array()));
        CHECK(parse_json(U"$\n{}") == dict(nlohmann::json::array()));
        CHECK(
            parse_json(U"$ /* c */ {k -> v}") ==
            dict(nlohmann::json::array({pair(ident("k"), ident("v"))}))
        );
    }

    TEST_CASE("键值对，尾逗号可选") {
        const auto expected = dict(
            nlohmann::json::array({pair(ident("k"), ident("v")), pair(ident("k2"), ident("v2"))})
        );
        CHECK(parse_json(U"${k -> v, k2 -> v2}") == expected);
        CHECK(parse_json(U"${k->v,k2->v2,}") == expected);
        CHECK(parse_json(U"${\nk -> v,\nk2 -> v2,\n}") == expected);
        CHECK(parse_json(U"${k\n->\nv, k2 -> v2}") == expected);
    }

    TEST_CASE("键和值都是完整的表达式，'->' 不是运算符") {
        CHECK(
            parse_json(U"${a + 1 -> b or c}") == dict(
                                                     nlohmann::json::array({pair(
                                                         {{"type", "OpBinary"},
                                                          {"op", "+"},
                                                          {"left", ident("a")},
                                                          {"right", int_lit("1")}},
                                                         {{"type", "OpBinary"},
                                                          {"op", "or"},
                                                          {"left", ident("b")},
                                                          {"right", ident("c")}}
                                                     )})
                                                 )
        );
        // 负数值紧跟 '->'：'->' 是一个 token，'-' 留给值
        CHECK(
            parse_json(U"${k->-1}")["items"][0]["value"] ==
            nlohmann::json{{"type", "OpUnary"}, {"op", "-"}, {"operand", int_lit("1")}}
        );
    }

    TEST_CASE("键和值里都可以直接写赋值和声明") {
        CHECK(
            parse_json(U"${k -> x = 1}") ==
            dict(nlohmann::json::array({pair(ident("k"), bare_assign(ident("x"), int_lit("1")))}))
        );
        CHECK(
            parse_json(U"${k -> x: int}") ==
            dict(
                nlohmann::json::array(
                    {pair(ident("k"), declare("x", false, ident("int"), nullptr))}
                )
            )
        );
        CHECK(
            parse_json(U"${x: int -> 1}") ==
            dict(
                nlohmann::json::array(
                    {pair(declare("x", false, ident("int"), nullptr), int_lit("1"))}
                )
            )
        );
    }

    TEST_CASE("值可以是复合表达式，复合表达式里也可以有字典") {
        CHECK(
            parse_json(U"${k -> {a; b}}") ==
            dict(
                nlohmann::json::array(
                    {pair(ident("k"), compound(nlohmann::json::array({ident("a"), ident("b")})))}
                )
            )
        );
        CHECK(
            parse_json(U"{ ${k -> v}; x }") ==
            compound(
                nlohmann::json::array(
                    {dict(nlohmann::json::array({pair(ident("k"), ident("v"))})), ident("x")}
                )
            )
        );
    }

    TEST_CASE("嵌套字典") {
        CHECK(
            parse_json(U"${k -> ${a -> b}}") ==
            dict(
                nlohmann::json::array(
                    {pair(ident("k"), dict(nlohmann::json::array({pair(ident("a"), ident("b"))})))}
                )
            )
        );
    }

    TEST_CASE("** 展开项可以出现在任意位置、可以有多个") {
        CHECK(parse_json(U"${**d}") == dict(nlohmann::json::array({spread("d")})));
        CHECK(
            parse_json(U"${**d1, k -> v, **d2,}") ==
            dict(nlohmann::json::array({spread("d1"), pair(ident("k"), ident("v")), spread("d2")}))
        );
        CHECK(parse_json(U"${**\nd}") == dict(nlohmann::json::array({spread("d")})));
    }

    TEST_CASE("展开项外面套分组括号是透明的") {
        CHECK(parse_json(U"${(**d)}") == parse_json(U"${**d}"));
        CHECK(parse_json(U"${k -> v, ((**d2))}") == parse_json(U"${k -> v, **d2}"));
    }

    TEST_CASE("括号里的换行不算分隔：值跨行接着解析，跟列表一样") {
        CHECK(parse_json(U"${k -> v\n+ w}") == parse_json(U"${k -> v + w}"));
        CHECK(parse_json(U"f(${k -> v\n+ w})") == parse_json(U"f(${k -> v + w})"));
    }

    TEST_CASE("展开项不能带值") {
        check_parse_throws_with(U"${**d -> v}", "a '**' item cannot be followed by '->'");
        check_parse_throws_with(U"${(**d) -> v}", "a '**' item cannot be followed by '->'");
    }

    TEST_CASE("残缺的键值对报错") {
        CHECK_THROWS_AS(parse_program(U"${k ->}"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"${-> v}"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"${k -> v,,}"), SyntaxError);
    }

    TEST_CASE("项之间只能用逗号分隔") {
        CHECK_THROWS_AS(parse_program(U"${k -> v; k2 -> v2}"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"${k -> v\nk2 -> v2}"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"${**d; x}"), SyntaxError);
    }

    TEST_CASE("$ 后面不是 { 报错") {
        check_parse_throws_with(U"$ x", "expected '{' after '$'");
        check_parse_throws_with(U"$(1)", "expected '{' after '$'");
        check_parse_throws_with(U"$[1]", "expected '{' after '$'");
        check_parse_throws_with(U"x = $", "expected '{' after '$'");
    }

    TEST_CASE("未闭合报错，位置指向 EOF") {
        // "${'a' -> 1" 共 10 个字符，EOF 在第 11 列
        try {
            parse_program(U"${'a' -> 1");
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("dict literal") != std::string::npos);
            CHECK(msg.find("1:11:") != std::string::npos);
        }
    }

    TEST_CASE("'->' 只在字典项里有意义，别处出现报错") {
        CHECK_THROWS_AS(parse_program(U"a -> b"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"[a -> b]"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"f(a -> b)"), SyntaxError);
        // 冒号不再是字典分隔符
        CHECK(parse_json(U"${k: v}")["type"] == "LiteralSet");
    }
}
