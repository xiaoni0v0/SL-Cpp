// 基本字面量如何保留在 AST 中。
#include "../../../cppexceptions/SyntaxError.h"
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("基本字面量") {

    TEST_CASE("None") {
        CHECK(parse_json(U"None") == nlohmann::json::parse(R"({"type":"LiteralNone"})"));
    }

    TEST_CASE("bool") {
        CHECK(
            parse_json(U"True") == nlohmann::json::parse(R"({"type":"LiteralBool","value":true})")
        );
        CHECK(
            parse_json(U"False") == nlohmann::json::parse(R"({"type":"LiteralBool","value":false})")
        );
    }

    TEST_CASE("_G / _L") {
        CHECK(parse_json(U"_G") == nlohmann::json::parse(R"({"type":"LiteralGL","value":"_G"})"));
        CHECK(parse_json(U"_L") == nlohmann::json::parse(R"({"type":"LiteralGL","value":"_L"})"));
    }

    TEST_CASE("...") {
        CHECK(parse_json(U"...") == nlohmann::json::parse(R"({"type":"LiteralEllipsis"})"));
    }

    TEST_CASE("int：Parser 原样存 raw_，不做数值转换") {
        CHECK(parse_json(U"123") == nlohmann::json::parse(R"({"type":"LiteralInt","raw":"123"})"));
        CHECK(parse_json(U"0") == nlohmann::json::parse(R"({"type":"LiteralInt","raw":"0"})"));
        CHECK(
            parse_json(U"123456789012345678901234567890") ==
            nlohmann::json::parse(R"({"type":"LiteralInt","raw":"123456789012345678901234567890"})")
        );
    }

    TEST_CASE("decimal：整数、小数部分都不能省略") {
        CHECK(
            parse_json(U"1.5") == nlohmann::json::parse(R"({"type":"LiteralDecimal","raw":"1.5"})")
        );
        CHECK(
            parse_json(U"123.45") ==
            nlohmann::json::parse(R"({"type":"LiteralDecimal","raw":"123.45"})")
        );
    }

    TEST_CASE(
        "`1.`/`.1` 词法上都不是合法 decimal（词法层拆成 INT+DOT / DOT+INT），"
        "语法层也各自因为别的原因解析失败"
    ) {
        // "1." 词法为 LITERAL_INT(1) SIGN_DOT，语法层把 '.' 当属性访问，
        // 但后面紧跟 EOF、没有属性名，expect(IDENTIFIER) 失败
        CHECK_THROWS_AS(parse_program(U"1."), SyntaxError);
        // ".1" 词法为 SIGN_DOT LITERAL_INT(1)，语法层一个表达式不能以裸 '.' 开头
        CHECK_THROWS_AS(parse_program(U".1"), SyntaxError);
    }

    TEST_CASE("负数不是字面量：一元负号和整数是分开的运算") {
        CHECK(
            parse_json(U"-1") ==
            nlohmann::json::parse(
                R"({"type":"OpUnary","op":"-","operand":{"type":"LiteralInt","raw":"1"}})"
            )
        );
    }

    TEST_CASE("str：三种写法产出的都是 LiteralStr，没有类型区别") {
        CHECK(
            parse_json(U"\"hello\"") ==
            nlohmann::json::parse(R"({"type":"LiteralStr","value":"hello"})")
        );
        CHECK(
            parse_json(U"'hello'") ==
            nlohmann::json::parse(R"({"type":"LiteralStr","value":"hello"})")
        );
        CHECK(
            parse_json(U"`hello`") ==
            nlohmann::json::parse(R"({"type":"LiteralStr","value":"hello"})")
        );
    }

    TEST_CASE("str：转义已由 Lexer 处理完毕，Parser 原样把处理后的内容带入 value_") {
        const AstNodePtr node{parse_single(U"\"a\\nb\"")};
        const auto *lit{dynamic_cast<AstNodeLiteralStr *>(node.get())};
        REQUIRE(lit != nullptr);
        CHECK(lit->value_ == U"a\nb");
    }

    TEST_CASE("相邻字符串不会自动拼接，同一行两个字面量缺分隔符") {
        CHECK_THROWS_AS(parse_program(U"\"a\" \"b\""), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"'a' 'b'"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"`a` `b`"), SyntaxError);
    }

    TEST_CASE("str：原始字符串（反引号）不处理转义，天然支持多行原样带入") {
        const AstNodePtr node{parse_single(U"`line1\nline2`")};
        const auto *lit{dynamic_cast<AstNodeLiteralStr *>(node.get())};
        REQUIRE(lit != nullptr);
        CHECK(lit->value_ == U"line1\nline2");
    }
}
