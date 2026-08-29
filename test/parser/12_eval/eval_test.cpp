// SL.md eval：eval(code) 是关键字，不是可传递的函数值（同 C 的 sizeof），括号强制。
// 之所以关键字化：eval 在调用帧里求值，做成一等值就没法静态判断"哪一帧会被现场插代码"。
#include "../../../builtins/exceptions/SyntaxError.h"
#include "../test_utils.h"

#include <doctest/doctest.h>

namespace {
nlohmann::json ident(const char *name) {
    return nlohmann::json{{"type", "Identifier"}, {"identifier", name}};
}
nlohmann::json str_lit(const char *value) {
    return nlohmann::json{{"type", "LiteralStr"}, {"value", value}};
}
} // namespace

TEST_SUITE("eval") {

    TEST_CASE("基本形态：解析成专门的 Eval 节点，不是 Call") {
        CHECK(parse_json(U"eval('x')") == nlohmann::json{{"type", "Eval"}, {"code", str_lit("x")}});
    }

    TEST_CASE("参数是普通表达式，不要求字面量——静态可见的是 eval 点的位置，不是它的内容") {
        CHECK(parse_json(U"eval(s)") == nlohmann::json{{"type", "Eval"}, {"code", ident("s")}});
        CHECK(
            parse_json(U"eval(a + b)") ==
            nlohmann::json{
                {"type", "Eval"},
                {"code",
                 {{"type", "OpBinary"}, {"op", "+"}, {"left", ident("a")}, {"right", ident("b")}}}
            }
        );
    }

    TEST_CASE("产出的值参与外层表达式，不像 return/raise 那样吃到底") {
        CHECK(
            parse_json(U"eval(s) + 1") == nlohmann::json{
                                              {"type", "OpBinary"},
                                              {"op", "+"},
                                              {"left", {{"type", "Eval"}, {"code", ident("s")}}},
                                              {"right", {{"type", "LiteralInt"}, {"raw", "1"}}}
                                          }
        );
    }

    TEST_CASE("可以被索引/调用/取属性，后缀照常接在它身上") {
        CHECK(
            parse_json(U"eval(s).x") == nlohmann::json{
                                            {"type", "Attr"},
                                            {"object", {{"type", "Eval"}, {"code", ident("s")}}},
                                            {"attr", "x"}
                                        }
        );
    }

    TEST_CASE("括号强制：eval 不是函数值，没有裸 eval 这种写法") {
        CHECK_THROWS_AS(parse_as_file(U"eval"), SyntaxError);
        CHECK_THROWS_AS(parse_as_file(U"f = eval"), SyntaxError);
        CHECK_THROWS_AS(parse_as_file(U"eval 'x'"), SyntaxError);
    }

    TEST_CASE("恰好一个参数：不接受空括号、也不接受逗号分隔的多个") {
        CHECK_THROWS_AS(parse_as_file(U"eval()"), SyntaxError);
        CHECK_THROWS_AS(parse_as_file(U"eval('a', 'b')"), SyntaxError);
    }

    TEST_CASE("未闭合括号报错") { CHECK_THROWS_AS(parse_as_file(U"eval('x'"), SyntaxError); }

    TEST_CASE("括号内换行照常当空白（普通圆括号的待遇）") {
        CHECK(
            parse_json(U"eval(\n  s\n)") == nlohmann::json{{"type", "Eval"}, {"code", ident("s")}}
        );
    }

    TEST_CASE("eval 是关键字，大小写变体和前缀/超集仍是普通标识符") {
        CHECK_NOTHROW(parse_as_file(U"Eval = 1"));
        CHECK_NOTHROW(parse_as_file(U"evaluate = 1"));
    }

    TEST_CASE("eval_isolated 仍是普通内置函数，照常按调用解析") {
        CHECK(
            parse_json(U"eval_isolated(s)") ==
            nlohmann::json{
                {"type", "Call"},
                {"object", ident("eval_isolated")},
                {"positional_args", nlohmann::json::array({ident("s")})},
                {"keyword_args", nlohmann::json::array()}
            }
        );
    }
}
