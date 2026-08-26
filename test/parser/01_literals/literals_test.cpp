// SL.md 字面量——parser 层面：token 如何变成对应的 AstNode
// None/bool/_G/_L/int/decimal/str/... 已在 lexer 测试里覆盖过 token 化本身，这里只关心 Parser 是否
// 把对应 token 原样正确地包进对应的 AstNode（字符串转义等已由 Lexer 处理完毕，不再重复测）。
#include "../../../builtins/exceptions/SyntaxError.h"
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

    TEST_CASE("int：Parser 原样存字符串形式，不做数值转换") {
        // 前导零本身是否合法在词法层面拦截、已在 lexer
        // 测试里覆盖过（test/lexer/04_literals/int_test.cpp）， 这里只关心合法数字文本能不能被
        // Parser 原样存进 raw_
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
        CHECK_THROWS_AS(parse_as_file(U"1."), SyntaxError);
        // ".1" 词法为 SIGN_DOT LITERAL_INT(1)，语法层一个表达式不能以裸 '.' 开头
        CHECK_THROWS_AS(parse_as_file(U".1"), SyntaxError);
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

    TEST_CASE("str：原始字符串（反引号）不处理转义，天然支持多行原样带入") {
        const AstNodePtr node{parse_single(U"`line1\nline2`")};
        const auto *lit{dynamic_cast<AstNodeLiteralStr *>(node.get())};
        REQUIRE(lit != nullptr);
        CHECK(lit->value_ == U"line1\nline2");
    }
}

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
        // 注意：这里必须用 = 而不是 nlohmann::json expected{...}——花括号初始化遇到"唯一的初始化项
        // 本身已经是个 json 对象"时，会被 nlohmann 的构造函数当成"用一个元素构造数组"，
        // 而不是拷贝这个对象本身，得到的会是包了一层数组的错误结果
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
        CHECK_THROWS_AS(parse_as_file(U"(1, 2"), SyntaxError);
        CHECK_THROWS_AS(parse_as_file(U"(1"), SyntaxError);
    }

    TEST_CASE(
        "报错措辞跟有没有见过逗号走：已经确认是元组才说'元组没闭合'，"
        "还看不出来是分组表达式还是元组时不能咬定是元组；位置都应该指向 EOF（缺的是收尾括号）"
    ) {
        try {
            parse_as_file(U"(1, 2"); // 见过逗号，确定是元组；"(1, 2" 共 5 个字符，EOF 在第 6 列
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("tuple") != std::string::npos);
            CHECK(msg.find("1:6:") != std::string::npos);
        }
        try {
            parse_as_file(
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
            parse_as_file(U"[1, 2");
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("list literal") != std::string::npos);
            CHECK(msg.find("1:6:") != std::string::npos);
        }
    }
}

TEST_SUITE("字典") {

    TEST_CASE("基本键值对") {
        CHECK(
            parse_json(U"{'a': 1}") == nlohmann::json::parse(
                                           R"({"type":"LiteralDict","items":[
                  {"key":{"type":"LiteralStr","value":"a"},"value":{"type":"LiteralInt","raw":"1"}}
              ]})"
                                       )
        );
    }

    TEST_CASE("多个键值对") {
        CHECK(
            parse_json(U"{'a': 1, 'b': 2}") == nlohmann::json::parse(
                                                   R"({"type":"LiteralDict","items":[
                  {"key":{"type":"LiteralStr","value":"a"},"value":{"type":"LiteralInt","raw":"1"}},
                  {"key":{"type":"LiteralStr","value":"b"},"value":{"type":"LiteralInt","raw":"2"}}
              ]})"
                                               )
        );
    }

    TEST_CASE("键不限于字符串字面量，可以是任意表达式") {
        CHECK(
            parse_json(U"{1: 'a', (1+1): 'b'}") == nlohmann::json::parse(
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
            parse_json(U"{'a': 1,}") == nlohmann::json::parse(
                                            R"({"type":"LiteralDict","items":[
                  {"key":{"type":"LiteralStr","value":"a"},"value":{"type":"LiteralInt","raw":"1"}}
              ]})"
                                        )
        );
    }

    TEST_CASE("嵌套字典") {
        CHECK(
            parse_json(U"{'a': {'b': 1}}") == nlohmann::json::parse(
                                                  R"({"type":"LiteralDict","items":[
                  {"key":{"type":"LiteralStr","value":"a"},"value":{"type":"LiteralDict","items":[
                      {"key":{"type":"LiteralStr","value":"b"},"value":{"type":"LiteralInt","raw":"1"}}
                  ]}}
              ]})"
                                              )
        );
    }

    TEST_CASE("未闭合的字典抛异常（走的是通用 expect('}') 报错，不是字典专属措辞），位置指向 EOF") {
        // "{'a': 1" 共 7 个字符，EOF 在第 8 列
        try {
            parse_as_file(U"{'a': 1");
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("'}'") != std::string::npos);
            CHECK(msg.find("1:8:") != std::string::npos);
        }
    }
}
