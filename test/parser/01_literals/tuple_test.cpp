// 分组与元组的形状、分隔和换行。
#include "../../../cppexceptions/SyntaxError.h"
#include "../test_utils.h"

#include <string>

#include <doctest/doctest.h>

TEST_SUITE("元组") {

    TEST_CASE("空元组 ()") {
        CHECK(parse_json(U"()") == nlohmann::json::parse(R"({"type":"LiteralTuple","items":[]})"));
    }

    TEST_CASE("(expr) 是分组，不是单元素元组") {
        CHECK(parse_json(U"(1)") == nlohmann::json::parse(R"({"type":"LiteralInt","raw":"1"})"));
        CHECK(
            parse_json(U"(1 + 2)") ==
            nlohmann::json::parse(
                R"({"type":"OpBinary","op":"+","left":{"type":"LiteralInt","raw":"1"},
                  "right":{"type":"LiteralInt","raw":"2"}})"
            )
        );
    }

    TEST_CASE("单元素元组必须有尾逗号 (1,)") {
        CHECK(
            parse_json(U"(1,)") ==
            nlohmann::json::parse(
                R"({"type":"LiteralTuple","items":[{"type":"LiteralInt","raw":"1"}]})"
            )
        );
    }

    TEST_CASE("多元素元组，尾逗号可选") {
        const nlohmann::json expected = nlohmann::json::parse(
            R"({"type":"LiteralTuple","items":[
            {"type":"LiteralInt","raw":"1"},
            {"type":"LiteralInt","raw":"2"},
            {"type":"LiteralInt","raw":"3"}
        ]})"
        );
        CHECK(parse_json(U"(1, 2, 3)") == expected);
        CHECK(parse_json(U"(1, 2, 3,)") == expected);
    }

    TEST_CASE("嵌套元组") {
        CHECK(
            parse_json(U"((1, 2), (3,))") == nlohmann::json::parse(
                                                 R"({"type":"LiteralTuple","items":[
                  {"type":"LiteralTuple","items":[
                      {"type":"LiteralInt","raw":"1"},{"type":"LiteralInt","raw":"2"}]},
                  {"type":"LiteralTuple","items":[{"type":"LiteralInt","raw":"3"}]}
              ]})"
                                             )
        );
    }

    TEST_CASE("多行元组（括号内换行自动合并）") {
        CHECK(
            parse_json(U"(\n1,\n2,\n)") == nlohmann::json::parse(
                                               R"({"type":"LiteralTuple","items":[
                  {"type":"LiteralInt","raw":"1"},{"type":"LiteralInt","raw":"2"}]})"
                                           )
        );
    }

    TEST_CASE("换行可以出现在逗号前后的任意位置，不影响元组/分组的判别") {
        // 注意：用 = 而不是 {}——花括号初始化一个已经构造好的 json 对象会被 nlohmann 的
        // initializer_list<json> 构造函数当成"用这一个元素构造数组"，而不是拷贝这个对象本身
        const auto pair = nlohmann::json::parse(
            R"({"type":"LiteralTuple","items":[{"type":"LiteralInt","raw":"1"},{"type":"LiteralInt","raw":"2"}]})"
        );
        CHECK(parse_json(U"(1\n, 2)") == pair);  // 换行在逗号之前
        CHECK(parse_json(U"(1,\n2)") == pair);   // 换行在逗号之后
        CHECK(parse_json(U"(1\n,\n2)") == pair); // 逗号两边都有换行
        // 单个元素本身横跨多行（末尾二元运算符触发续行），之后还有逗号和更多元素
        CHECK(
            parse_json(U"(1 +\n2, 3)") == nlohmann::json::parse(
                                              R"({"type":"LiteralTuple","items":[
                  {"type":"OpBinary","op":"+","left":{"type":"LiteralInt","raw":"1"},
                                     "right":{"type":"LiteralInt","raw":"2"}},
                  {"type":"LiteralInt","raw":"3"}
              ]})"
                                          )
        );
    }

    TEST_CASE("开头的换行只是格式，不影响分组 vs 元组的判别") {
        // 开头换行 + 单元素 + 无逗号 → 仍然是分组，不是元组
        CHECK(
            parse_json(U"(\n1\n)") == nlohmann::json::parse(R"({"type":"LiteralInt","raw":"1"})")
        );
        // 开头换行 + 单元素 + 尾逗号 → 单元素元组
        CHECK(
            parse_json(U"(\n1,\n)") ==
            nlohmann::json::parse(
                R"({"type":"LiteralTuple","items":[{"type":"LiteralInt","raw":"1"}]})"
            )
        );
    }

    TEST_CASE("未闭合的元组/分组抛异常") {
        CHECK_THROWS_AS(parse_program(U"(1, 2"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"(1"), SyntaxError);
    }

    TEST_CASE(
        "报错措辞跟有没有见过逗号走：已经确认是元组才说'元组没闭合'，"
        "还看不出来是分组表达式还是元组时不能咬定是元组；位置都应该指向 EOF（缺的是收尾括号）"
    ) {
        try {
            parse_program(U"(1, 2"); // 见过逗号，确定是元组；"(1, 2" 共 5 个字符，EOF 在第 6 列
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("tuple") != std::string::npos);
            CHECK(msg.find("1:6:") != std::string::npos);
        }
        try {
            parse_program(
                U"(1"
            ); // 没见过逗号，分不清是分组表达式还是元组；"(1" 共 2 个字符，EOF 在第 3 列
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("tuple") == std::string::npos);
            CHECK(msg.find("parentheses") != std::string::npos);
            CHECK(msg.find("1:3:") != std::string::npos);
        }
    }
}
