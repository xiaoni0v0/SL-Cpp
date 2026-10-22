// 列表字面量的形状与错误边界。
#include "../../../cppexceptions/SyntaxError.h"
#include "../test_utils.h"

#include <string>

#include <doctest/doctest.h>

TEST_SUITE("列表") {

    TEST_CASE("空列表 []") {
        CHECK(parse_json(U"[]") == nlohmann::json::parse(R"({"type":"LiteralList","items":[]})"));
    }

    TEST_CASE("单元素列表不需要尾逗号") {
        CHECK(
            parse_json(U"[1]") ==
            nlohmann::json::parse(
                R"({"type":"LiteralList","items":[{"type":"LiteralInt","raw":"1"}]})"
            )
        );
    }

    TEST_CASE("多元素列表，尾逗号可选") {
        const nlohmann::json expected = nlohmann::json::parse(
            R"({"type":"LiteralList","items":[
            {"type":"LiteralInt","raw":"1"},
            {"type":"LiteralInt","raw":"2"},
            {"type":"LiteralInt","raw":"3"}
        ]})"
        );
        CHECK(parse_json(U"[1, 2, 3]") == expected);
        CHECK(parse_json(U"[1, 2, 3,]") == expected);
    }

    TEST_CASE("嵌套列表") {
        CHECK(
            parse_json(U"[[1, 2], [3, 4]]") == nlohmann::json::parse(
                                                   R"({"type":"LiteralList","items":[
                  {"type":"LiteralList","items":[
                      {"type":"LiteralInt","raw":"1"},{"type":"LiteralInt","raw":"2"}]},
                  {"type":"LiteralList","items":[
                      {"type":"LiteralInt","raw":"3"},{"type":"LiteralInt","raw":"4"}]}
              ]})"
                                               )
        );
    }

    TEST_CASE("未闭合的列表抛异常，消息明确说是列表，位置指向 EOF") {
        // "[1, 2" 共 5 个字符，EOF 在第 6 列
        try {
            parse_program(U"[1, 2");
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("list literal") != std::string::npos);
            CHECK(msg.find("1:6:") != std::string::npos);
        }
    }
}
