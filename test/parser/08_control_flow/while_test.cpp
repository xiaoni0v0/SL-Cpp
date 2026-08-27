// SL.md while 表达式：while [收集模式记号] (cond) expr。记号本身单独在 collect_mark_test.cpp 里覆盖
// 内部复用 AstNodeForCond（init_/inc_ 皆为 nullptr），不单独建节点类型，但对外观察到的 JSON 形状
// 就是按 ForCond 来的，这里直接按 ForCond 的字段断言。
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

TEST_SUITE("while") {

    TEST_CASE("基本形式，等价于 for (; cond ;)：init/inc 皆为 null") {
        CHECK(
            parse_json(U"while (c) body") == nlohmann::json{
                                                 {"type", "ForCond"},
                                                 {"collect", "none"},
                                                 {"init", nullptr},
                                                 {"cond", ident("c")},
                                                 {"inc", nullptr},
                                                 {"body", ident("body")}
                                             }
        );
    }

    TEST_CASE("收集模式 while $ (...)") {
        CHECK(
            parse_json(U"while $ (c) body") == nlohmann::json{
                                                   {"type", "ForCond"},
                                                   {"collect", "$"},
                                                   {"init", nullptr},
                                                   {"cond", ident("c")},
                                                   {"inc", nullptr},
                                                   {"body", ident("body")}
                                               }
        );
    }

    TEST_CASE("cond 可以是复杂表达式") {
        CHECK(
            parse_json(U"while (x < 10) body") ==
            nlohmann::json{
                {"type", "ForCond"},
                {"collect", "none"},
                {"init", nullptr},
                {"cond",
                 {{"type", "Compare"},
                  {"operands", nlohmann::json::array({ident("x"), int_lit("10")})},
                  {"ops", nlohmann::json::array({"<"})}}},
                {"inc", nullptr},
                {"body", ident("body")}
            }
        );
    }

    TEST_CASE("缺少括号/未闭合/缺 body 都报错") {
        CHECK_THROWS_AS(parse_as_file(U"while c) body"), SyntaxError);
        CHECK_THROWS_AS(parse_as_file(U"while (c"), SyntaxError);
        CHECK_THROWS_AS(parse_as_file(U"while (c)"), SyntaxError);
    }
}

TEST_SUITE("while——括号里的换行是空白，不像步进 for 的头部那样分隔") {

    // while 只有一个槽，没有槽边界要分，所以这对括号就是普通括号：换行随便折。
    // 对照 for_test.cpp 里"步进模式头部的换行按软终止分隔"那一组——同样的两行，那边会被切成两槽
    TEST_CASE("cond 跨行照旧当空白，不切断") {
        CHECK(
            parse_json(U"while (\nx\n+ 1\n) body") == nlohmann::json{
                                                          {"type", "ForCond"},
                                                          {"collect", "none"},
                                                          {"init", nullptr},
                                                          {"cond",
                                                           {{"type", "OpBinary"},
                                                            {"op", "+"},
                                                            {"left", ident("x")},
                                                            {"right", int_lit("1")}}},
                                                          {"inc", nullptr},
                                                          {"body", ident("body")}
                                                      }
        );
    }
}

TEST_SUITE("while——cond 禁止裸的普通赋值") {

    TEST_CASE("裸 = 报错") { CHECK_THROWS_AS(parse_as_file(U"while (x = 1) body"), SyntaxError); }

    TEST_CASE("裸复合赋值不受限") {
        CHECK(
            parse_json(U"while (x += 1) body") == nlohmann::json{
                                                      {"type", "ForCond"},
                                                      {"collect", "none"},
                                                      {"init", nullptr},
                                                      {"cond",
                                                       {{"type", "CompoundAssign"},
                                                        {"target", ident("x")},
                                                        {"op", "+"},
                                                        {"value", int_lit("1")}}},
                                                      {"inc", nullptr},
                                                      {"body", ident("body")}
                                                  }
        );
    }

    TEST_CASE("多套一层括号允许裸赋值") {
        CHECK(
            parse_json(U"while ((x = 1)) body") ==
            nlohmann::json{
                {"type", "ForCond"},
                {"collect", "none"},
                {"init", nullptr},
                {"cond", {{"type", "Assign"}, {"target", ident("x")}, {"value", int_lit("1")}}},
                {"inc", nullptr},
                {"body", ident("body")}
            }
        );
    }
}

TEST_SUITE("while——$ 与 while 之间不需要空白（SL.md）") {

    TEST_CASE("基本 while$ 无空格") {
        CHECK(
            parse_json(U"while$(c) body") == nlohmann::json{
                                                 {"type", "ForCond"},
                                                 {"collect", "$"},
                                                 {"init", nullptr},
                                                 {"cond", ident("c")},
                                                 {"inc", nullptr},
                                                 {"body", ident("body")}
                                             }
        );
    }

    TEST_CASE("while$ 无空格 + 复杂条件") {
        CHECK(
            parse_json(U"while$(x < 10) body") ==
            nlohmann::json{
                {"type", "ForCond"},
                {"collect", "$"},
                {"init", nullptr},
                {"cond",
                 {{"type", "Compare"},
                  {"operands", nlohmann::json::array({ident("x"), int_lit("10")})},
                  {"ops", nlohmann::json::array({"<"})}}},
                {"inc", nullptr},
                {"body", ident("body")}
            }
        );
    }
}
