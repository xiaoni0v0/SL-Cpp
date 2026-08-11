// SL.md for 表达式：
//   步进模式 for [收集模式记号] (init cond inc) expr；迭代模式 for [收集模式记号] (lvalue :
//   iterable) expr。 记号本身（$ / $ * / $$ / $$ **）单独在 collect_mark_test.cpp 里覆盖。
// 这里重点覆盖：
//   1. "裸单表达式当条件" for (cond) body 不是 SL.md 授权的语法，必须报错；
//   2. 中间 cond 槽禁止裸的普通赋值 =（init/inc 不受限）；
//   3. for () 彻底为空时的专门报错。
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

TEST_SUITE("for——步进模式") {

    TEST_CASE("三槽齐全") {
        CHECK(
            parse_json(U"for (i = 0; i < 10; i += 1) body") ==
            nlohmann::json{
                {"type", "ForCond"},
                {"collect", "none"},
                {"init", {{"type", "Assign"}, {"target", ident("i")}, {"value", int_lit("0")}}},
                {"cond",
                 {{"type", "Compare"},
                  {"operands", nlohmann::json::array({ident("i"), int_lit("10")})},
                  {"ops", nlohmann::json::array({"<"})}}},
                {"inc",
                 {{"type", "CompoundAssign"},
                  {"target", ident("i")},
                  {"op", "+"},
                  {"value", int_lit("1")}}},
                {"body", ident("body")}
            }
        );
    }

    TEST_CASE("三槽全空：无限循环 for (;;)") {
        CHECK(
            parse_json(U"for (;;) body") == nlohmann::json{
                                                {"type", "ForCond"},
                                                {"collect", "none"},
                                                {"init", nullptr},
                                                {"cond", nullptr},
                                                {"inc", nullptr},
                                                {"body", ident("body")}
                                            }
        );
    }

    TEST_CASE("只有 cond：for (; cond ;) body") {
        CHECK(
            parse_json(U"for (; c ;) body") == nlohmann::json{
                                                   {"type", "ForCond"},
                                                   {"collect", "none"},
                                                   {"init", nullptr},
                                                   {"cond", ident("c")},
                                                   {"inc", nullptr},
                                                   {"body", ident("body")}
                                               }
        );
    }

    TEST_CASE("只有 init：for (i = 0;;) body") {
        CHECK(
            parse_json(U"for (i = 0;;) body") ==
            nlohmann::json{
                {"type", "ForCond"},
                {"collect", "none"},
                {"init", {{"type", "Assign"}, {"target", ident("i")}, {"value", int_lit("0")}}},
                {"cond", nullptr},
                {"inc", nullptr},
                {"body", ident("body")}
            }
        );
    }

    TEST_CASE("只有 inc：for (;; i += 1) body") {
        CHECK(
            parse_json(U"for (;; i += 1) body") == nlohmann::json{
                                                       {"type", "ForCond"},
                                                       {"collect", "none"},
                                                       {"init", nullptr},
                                                       {"cond", nullptr},
                                                       {"inc",
                                                        {{"type", "CompoundAssign"},
                                                         {"target", ident("i")},
                                                         {"op", "+"},
                                                         {"value", int_lit("1")}}},
                                                       {"body", ident("body")}
                                                   }
        );
    }

    TEST_CASE("分隔符可以用换行代替分号") {
        CHECK(
            parse_json(U"for (i = 0\ni < 10\ni += 1) body") ==
            nlohmann::json{
                {"type", "ForCond"},
                {"collect", "none"},
                {"init", {{"type", "Assign"}, {"target", ident("i")}, {"value", int_lit("0")}}},
                {"cond",
                 {{"type", "Compare"},
                  {"operands", nlohmann::json::array({ident("i"), int_lit("10")})},
                  {"ops", nlohmann::json::array({"<"})}}},
                {"inc",
                 {{"type", "CompoundAssign"},
                  {"target", ident("i")},
                  {"op", "+"},
                  {"value", int_lit("1")}}},
                {"body", ident("body")}
            }
        );
    }

    TEST_CASE("收集模式 for $ (...)") {
        CHECK(
            parse_json(U"for $ (i = 0; i < 10; i += 1) body") ==
            nlohmann::json{
                {"type", "ForCond"},
                {"collect", "$"},
                {"init", {{"type", "Assign"}, {"target", ident("i")}, {"value", int_lit("0")}}},
                {"cond",
                 {{"type", "Compare"},
                  {"operands", nlohmann::json::array({ident("i"), int_lit("10")})},
                  {"ops", nlohmann::json::array({"<"})}}},
                {"inc",
                 {{"type", "CompoundAssign"},
                  {"target", ident("i")},
                  {"op", "+"},
                  {"value", int_lit("1")}}},
                {"body", ident("body")}
            }
        );
    }

    TEST_CASE("槽之间缺分隔符报错，位置指向下一槽开头（不是上一槽结尾）") {
        try {
            parse_program(U"for (i = 0 i < 10; i += 1) body");
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("separate the expressions in a for header") != std::string::npos);
            CHECK(msg.find("1:12:") != std::string::npos);
        }
    }

    TEST_CASE(
        "换行不能替代 ';' 来标记空槽：只写两个换行分隔的槽就直接收尾必须报错，"
        "不能把第三槽悄悄当成空的接受掉"
    ) {
        CHECK_THROWS_AS(parse_program(U"for (a\nb\n) body"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"for (a\n) body"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"for (; c\n) body"), SyntaxError);
    }

    TEST_CASE("空槽换行报错的消息说明白要补 ';'，位置指向那个不该出现的 ')'") {
        try {
            parse_program(U"for (a\nb\n) body");
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("must be marked with ';'") != std::string::npos);
            CHECK(msg.find("3:1:") != std::string::npos);
        }
    }

    TEST_CASE("换行 + 换行分隔的空槽必须紧跟着显式 ';' 才行：加上分号就恢复合法") {
        CHECK(
            parse_json(U"for (a\nb\n;) body") == nlohmann::json{
                                                     {"type", "ForCond"},
                                                     {"collect", "none"},
                                                     {"init", ident("a")},
                                                     {"cond", ident("b")},
                                                     {"inc", nullptr},
                                                     {"body", ident("body")}
                                                 }
        );
    }

    TEST_CASE("头部开头的换行只是格式，不影响三槽的判定") {
        CHECK(
            parse_json(U"for (\ni = 0; i < 10; i += 1) body") ==
            nlohmann::json{
                {"type", "ForCond"},
                {"collect", "none"},
                {"init", {{"type", "Assign"}, {"target", ident("i")}, {"value", int_lit("0")}}},
                {"cond",
                 {{"type", "Compare"},
                  {"operands", nlohmann::json::array({ident("i"), int_lit("10")})},
                  {"ops", nlohmann::json::array({"<"})}}},
                {"inc",
                 {{"type", "CompoundAssign"},
                  {"target", ident("i")},
                  {"op", "+"},
                  {"value", int_lit("1")}}},
                {"body", ident("body")}
            }
        );
    }

    TEST_CASE("换行分隔符之间允许多个空行，不只是恰好一个换行") {
        CHECK(
            parse_json(U"for (a\n\n\nb\n\n\nc) body") == nlohmann::json{
                                                             {"type", "ForCond"},
                                                             {"collect", "none"},
                                                             {"init", ident("a")},
                                                             {"cond", ident("b")},
                                                             {"inc", ident("c")},
                                                             {"body", ident("body")}
                                                         }
        );
    }

    TEST_CASE("只用一个显式 ';' 就想收尾（少了第二个分隔符/第三槽）必须报错") {
        CHECK_THROWS_AS(parse_program(U"for (;) body"), SyntaxError);
    }

    TEST_CASE("for () 彻底为空报错，提示改用 for (;;) 或 while (cond)") {
        CHECK_THROWS_AS(parse_program(U"for () body"), SyntaxError);
    }

    TEST_CASE("裸单表达式当条件不受语法支持（未被 SL.md 授权）：for (cond) body 必须报错") {
        CHECK_THROWS_AS(parse_program(U"for (x > 0) body"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"for $ (x > 0) body"), SyntaxError);
    }
}

TEST_SUITE("for——迭代模式") {

    TEST_CASE("基本迭代") {
        CHECK(
            parse_json(U"for (x : xs) body") == nlohmann::json{
                                                    {"type", "ForIter"},
                                                    {"collect", "none"},
                                                    {"target", ident("x")},
                                                    {"iterable", ident("xs")},
                                                    {"body", ident("body")}
                                                }
        );
    }

    TEST_CASE("收集模式迭代") {
        CHECK(
            parse_json(U"for $ (x : xs) body") == nlohmann::json{
                                                      {"type", "ForIter"},
                                                      {"collect", "$"},
                                                      {"target", ident("x")},
                                                      {"iterable", ident("xs")},
                                                      {"body", ident("body")}
                                                  }
        );
    }

    TEST_CASE("目标可以是解构元组/列表（语法层放行任意左值形状，交语义层校验）") {
        CHECK(
            parse_json(U"for ((a, b) : pairs) body") ==
            nlohmann::json{
                {"type", "ForIter"},
                {"collect", "none"},
                {"target",
                 {{"type", "LiteralTuple"},
                  {"items", nlohmann::json::array({ident("a"), ident("b")})}}},
                {"iterable", ident("pairs")},
                {"body", ident("body")}
            }
        );
        CHECK(
            parse_json(U"for ([a, *b] : xs) body") ==
            nlohmann::json{
                {"type", "ForIter"},
                {"collect", "none"},
                {"target",
                 {{"type", "LiteralList"},
                  {"items",
                   nlohmann::json::array(
                       {ident("a"), {{"type", "Star"}, {"operand", ident("b")}}}
                   )}}},
                {"iterable", ident("xs")},
                {"body", ident("body")}
            }
        );
    }

    TEST_CASE("元组目标不带外层括号会被当成步进模式解析，进而因为缺分隔符报错") {
        CHECK_THROWS_AS(parse_program(U"for (a, b : pairs) body"), SyntaxError);
    }

    TEST_CASE("未闭合括号/缺 body 报错") {
        CHECK_THROWS_AS(parse_program(U"for (x : xs"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"for (x : xs)"), SyntaxError);
    }
}

TEST_SUITE("for——cond 槽禁止裸的普通赋值（init/inc 不受限）") {

    TEST_CASE("中间 cond 槽裸 = 报错") {
        CHECK_THROWS_AS(parse_program(U"for (i = 0; i = 10; i += 1) body"), SyntaxError);
    }

    TEST_CASE("中间 cond 槽裸复合赋值不受限") {
        CHECK(
            parse_json(U"for (; x += 1;) body") == nlohmann::json{
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

    TEST_CASE("cond 槽套一层括号就能写裸赋值") {
        CHECK(
            parse_json(U"for (; (x = 1);) body") ==
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

    TEST_CASE("init/inc 槽裸赋值不受限（本来就是为赋值而生）") {
        CHECK_NOTHROW(parse_program(U"for (i = 0; c; i = i + 1) body"));
    }
}

TEST_SUITE("for——$ 与 for 之间不需要空白（SL.md）") {

    TEST_CASE("步进模式 for$ 无空格") {
        CHECK(
            parse_json(U"for$(i = 0; i < 10; i += 1) body") ==
            nlohmann::json{
                {"type", "ForCond"},
                {"collect", "$"},
                {"init", {{"type", "Assign"}, {"target", ident("i")}, {"value", int_lit("0")}}},
                {"cond",
                 {{"type", "Compare"},
                  {"operands", nlohmann::json::array({ident("i"), int_lit("10")})},
                  {"ops", nlohmann::json::array({"<"})}}},
                {"inc",
                 {{"type", "CompoundAssign"},
                  {"target", ident("i")},
                  {"op", "+"},
                  {"value", int_lit("1")}}},
                {"body", ident("body")}
            }
        );
    }

    TEST_CASE("迭代模式 for$ 无空格") {
        CHECK(
            parse_json(U"for$(x : xs) body") == nlohmann::json{
                                                    {"type", "ForIter"},
                                                    {"collect", "$"},
                                                    {"target", ident("x")},
                                                    {"iterable", ident("xs")},
                                                    {"body", ident("body")}
                                                }
        );
    }
}

TEST_SUITE("for——迭代模式 : 前允许换行") {

    TEST_CASE(": 紧跟前导换行，仍能正确识别为迭代模式") {
        CHECK(
            parse_json(U"for (x\n: xs) body") == nlohmann::json{
                                                     {"type", "ForIter"},
                                                     {"collect", "none"},
                                                     {"target", ident("x")},
                                                     {"iterable", ident("xs")},
                                                     {"body", ident("body")}
                                                 }
        );
    }

    TEST_CASE(": 前有多个空行也同样正确") {
        CHECK(
            parse_json(U"for (x\n\n\n: xs) body") == nlohmann::json{
                                                         {"type", "ForIter"},
                                                         {"collect", "none"},
                                                         {"target", ident("x")},
                                                         {"iterable", ident("xs")},
                                                         {"body", ident("body")}
                                                     }
        );
    }
}
