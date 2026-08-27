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
nlohmann::json unary(const char *op, const nlohmann::json &operand) {
    return nlohmann::json{{"type", "OpUnary"}, {"op", op}, {"operand", operand}};
}

nlohmann::json binary(const char *op, const nlohmann::json &left, const nlohmann::json &right) {
    return nlohmann::json{{"type", "OpBinary"}, {"op", op}, {"left", left}, {"right", right}};
}

nlohmann::json assign(const nlohmann::json &target, const nlohmann::json &value) {
    return nlohmann::json{{"type", "Assign"}, {"target", target}, {"value", value}};
}

nlohmann::json cassign(const nlohmann::json &target, const char *op, const nlohmann::json &value) {
    return nlohmann::json{
        {"type", "CompoundAssign"}, {"target", target}, {"op", op}, {"value", value}
    };
}

nlohmann::json compare_lt(const nlohmann::json &left, const nlohmann::json &right) {
    return nlohmann::json{
        {"type", "Compare"},
        {"operands", nlohmann::json::array({left, right})},
        {"ops", nlohmann::json::array({"<"})}
    };
}

// 只取步进模式头部的三槽：这一组用例关心的是"换行切在哪儿"，不必每条都重复比对整棵树
nlohmann::json for_slots(const std::u32string &source) {
    // 必须用 = 拷贝初始化，不能用 {}——见 .ai/notes/json-test-brace-init-trap.md
    const auto node = parse_json(source);
    REQUIRE(node["type"] == "ForCond");
    return nlohmann::json{{"init", node["init"]}, {"cond", node["cond"]}, {"inc", node["inc"]}};
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
            parse_as_file(U"for (i = 0 i < 10; i += 1) body");
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
        CHECK_THROWS_AS(parse_as_file(U"for (a\nb\n) body"), SyntaxError);
        CHECK_THROWS_AS(parse_as_file(U"for (a\n) body"), SyntaxError);
        CHECK_THROWS_AS(parse_as_file(U"for (; c\n) body"), SyntaxError);
    }

    TEST_CASE("空槽换行报错的消息说明白要补 ';'，位置指向那个不该出现的 ')'") {
        try {
            parse_as_file(U"for (a\nb\n) body");
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
        CHECK_THROWS_AS(parse_as_file(U"for (;) body"), SyntaxError);
    }

    TEST_CASE("for () 彻底为空报错，提示改用 for (;;) 或 while (cond)") {
        CHECK_THROWS_AS(parse_as_file(U"for () body"), SyntaxError);
    }

    TEST_CASE("裸单表达式当条件不受语法支持（未被 SL.md 授权）：for (cond) body 必须报错") {
        CHECK_THROWS_AS(parse_as_file(U"for (x > 0) body"), SyntaxError);
        CHECK_THROWS_AS(parse_as_file(U"for $ (x > 0) body"), SyntaxError);
    }
}

TEST_SUITE("for——步进模式头部的换行按软终止分隔") {

    // SL.md 表达式分隔符一节的规则：换行处左侧若已能构成完整表达式就断开，否则并入下一行继续
    // 解析。步进 for 的头部沿用同一套，不像别处的括号那样把换行当成空白——否则 `x = 1` 换行 `+1`
    // 会被悄悄粘成 `x = 2`，跟同样这两行写在块里的结果正好相反。

    TEST_CASE("左侧已完整：在换行处断开，下一行归下一槽（下一行以中缀号开头也一样）") {
        CHECK(
            for_slots(U"for (x = 1\n+1\n;) body") == nlohmann::json{
                                                         {"init", assign(ident("x"), int_lit("1"))},
                                                         {"cond", unary("+", int_lit("1"))},
                                                         {"inc", nullptr}
                                                     }
        );
        CHECK(
            for_slots(U"for (x = 1\n-1\n;) body") == nlohmann::json{
                                                         {"init", assign(ident("x"), int_lit("1"))},
                                                         {"cond", unary("-", int_lit("1"))},
                                                         {"inc", nullptr}
                                                     }
        );
    }

    TEST_CASE("下一行以 '(' / '[' 开头：是下一槽，不是对上一槽的调用/取下标") {
        CHECK(
            for_slots(U"for (a\n(b)\n;) body") ==
            nlohmann::json{{"init", ident("a")}, {"cond", ident("b")}, {"inc", nullptr}}
        );
        CHECK(
            for_slots(U"for (a\n[b]\n;) body") ==
            nlohmann::json{
                {"init", ident("a")},
                {"cond", {{"type", "LiteralList"}, {"items", nlohmann::json::array({ident("b")})}}},
                {"inc", nullptr}
            }
        );
    }

    TEST_CASE("下一行以 '.' 开头：起不了头，跟块里写 `x` 换行 `.f()` 一样报错") {
        CHECK_THROWS_AS(parse_as_file(U"for (a\n.b\n;) body"), SyntaxError);
    }

    TEST_CASE("左侧不完整：并入下一行，换行不生效") {
        // 中缀运算符结尾
        CHECK(
            for_slots(U"for (i = 0\ni <\nn\ni += 1) body") ==
            nlohmann::json{
                {"init", assign(ident("i"), int_lit("0"))},
                {"cond", compare_lt(ident("i"), ident("n"))},
                {"inc", cassign(ident("i"), "+", int_lit("1"))}
            }
        );
        // and 结尾，跨行的长条件照样写得出来
        CHECK(
            for_slots(U"for (i = 0\ni < n and\nn < m\ni += 1) body") ==
            nlohmann::json{
                {"init", assign(ident("i"), int_lit("0"))},
                {"cond",
                 {{"type", "OpBinary"},
                  {"op", "and"},
                  {"left", compare_lt(ident("i"), ident("n"))},
                  {"right", compare_lt(ident("n"), ident("m"))}}},
                {"inc", cassign(ident("i"), "+", int_lit("1"))}
            }
        );
    }

    TEST_CASE("槽内嵌套的括号里换行照旧是空白，只有头部自己那一层特殊") {
        CHECK(
            for_slots(U"for (i = f(a,\nb)\nc\nd) body") ==
            nlohmann::json{
                {"init",
                 assign(
                     ident("i"),
                     nlohmann::json{
                         {"type", "Call"},
                         {"object", ident("f")},
                         {"positional_args", nlohmann::json::array({ident("a"), ident("b")})},
                         {"keyword_args", nlohmann::json::array()}
                     }
                 )},
                {"cond", ident("c")},
                {"inc", ident("d")}
            }
        );
    }

    TEST_CASE("槽内的 '{' 块把括号状态整个隔开：块里再开括号，换行又变回空白") {
        // 没隔开的话，块内那对 '(' 的深度会跟头部那层撞上，`(1` 换行 `+ 2)` 会被切断
        CHECK(
            for_slots(U"for (i = {\n(1\n+ 2)\n}\n;\n;) body") ==
            nlohmann::json{
                {"init",
                 assign(
                     ident("i"),
                     nlohmann::json{
                         {"type", "Compound"},
                         {"exprs", nlohmann::json::array({binary("+", int_lit("1"), int_lit("2"))})}
                     }
                 )},
                {"cond", nullptr},
                {"inc", nullptr}
            }
        );
    }

    TEST_CASE("嵌套 for：内层头部结束后还原成外层那一层，外层的换行仍然分隔") {
        const auto node = parse_json(U"for (i = for (a;;) b\nc\nd) body");
        CHECK(node["init"]["value"]["type"] == "ForCond");
        CHECK(node["cond"] == ident("c"));
        CHECK(node["inc"] == ident("d"));
    }

    TEST_CASE("换行切出第四槽：报错，不再把多出来的那行悄悄拌进上一槽") {
        // 改动之前 `i += 1` 换行 `-1` 会被粘成 `i += (1 - 1)`，循环一步都不推进，还不报错
        CHECK_THROWS_AS(parse_as_file(U"for (i = 0\nc\ni += 1\n-1\n) body"), SyntaxError);
        CHECK_THROWS_AS(parse_as_file(U"for (a\nb\nc\nd) body"), SyntaxError);
        CHECK_THROWS_AS(parse_as_file(U"for (a;b;c;d) body"), SyntaxError);
    }

    TEST_CASE("';' 前后的换行都归这个分隔符，允许把 ';' 单独写一行") {
        CHECK(
            for_slots(U"for (a\n;\nb\n;\nc) body") ==
            nlohmann::json{{"init", ident("a")}, {"cond", ident("b")}, {"inc", ident("c")}}
        );
        CHECK(
            for_slots(U"for (\na\n\n;\n\nb\n\n;\n\nc\n) body") ==
            nlohmann::json{{"init", ident("a")}, {"cond", ident("b")}, {"inc", ident("c")}}
        );
    }

    TEST_CASE("三槽各占一行的常规多行写法") {
        CHECK(
            for_slots(U"for (\ni = 0\ni < n\ni += 1\n) body") ==
            nlohmann::json{
                {"init", assign(ident("i"), int_lit("0"))},
                {"cond", compare_lt(ident("i"), ident("n"))},
                {"inc", cassign(ident("i"), "+", int_lit("1"))}
            }
        );
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
        CHECK_THROWS_AS(parse_as_file(U"for (a, b : pairs) body"), SyntaxError);
    }

    TEST_CASE("未闭合括号/缺 body 报错") {
        CHECK_THROWS_AS(parse_as_file(U"for (x : xs"), SyntaxError);
        CHECK_THROWS_AS(parse_as_file(U"for (x : xs)"), SyntaxError);
    }
}

TEST_SUITE("for——迭代模式里 ':' 之后的换行不分隔") {

    // ':' 之后只剩 iterable 一个槽，没有槽边界要分了，换行退回普通括号里的待遇（同 while 的条件）

    TEST_CASE("iterable 跨行照旧当空白，不切断") {
        // 必须用 = 拷贝初始化，不能用 {}——见 .ai/notes/json-test-brace-init-trap.md
        const auto node = parse_json(U"for (x : a\n+ b) body");
        CHECK(node["type"] == "ForIter");
        CHECK(node["target"] == ident("x"));
        CHECK(node["iterable"] == binary("+", ident("a"), ident("b")));
    }

    TEST_CASE("target 那一槽仍然按换行分隔：它得靠换行之后的 ':' 才认得出是迭代模式") {
        // 于是 `a` 换行 `.b` 会被切成步进模式的 init 和 cond，后者 '.' 起不了头，报错——
        // 跟同样两行写在块里的结果一致
        CHECK_THROWS_AS(parse_as_file(U"for (a\n.b : xs) body"), SyntaxError);
    }
}

TEST_SUITE("for——cond 槽禁止裸的普通赋值（init/inc 不受限）") {

    TEST_CASE("中间 cond 槽裸 = 报错") {
        CHECK_THROWS_AS(parse_as_file(U"for (i = 0; i = 10; i += 1) body"), SyntaxError);
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
        CHECK_NOTHROW(parse_as_file(U"for (i = 0; c; i = i + 1) body"));
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
