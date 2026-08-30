// SL.md eval：eval 是关键字伪装的函数（同 C 的 sizeof），不是可传递的函数值，括号强制。
// 之所以关键字化：eval 在调用帧里求值，做成一等值就没法静态判断"哪一帧会被现场插代码"。
// 既然是"伪装成函数"，实参形状就要尽可能像真正的调用：直接复用 finish_call，
// eval('x')、eval(code='x')、eval(**{'code':'x'}) 语法上都合法，恰好绑出一个叫 code 的形参
// 是运行期的事（3.5/3.4.10），跟普通函数调用的参数个数/类型不在语法层校验是同一套道理。
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
nlohmann::json
eval_node(const nlohmann::json &positional_args, const nlohmann::json &keyword_args) {
    return nlohmann::json{
        {"type", "Eval"}, {"positional_args", positional_args}, {"keyword_args", keyword_args}
    };
}
} // namespace

TEST_SUITE("eval") {

    TEST_CASE("基本形态：解析成专门的 Eval 节点，不是 Call，没有被调对象槽位") {
        CHECK(
            parse_json(U"eval('x')") ==
            eval_node(nlohmann::json::array({str_lit("x")}), nlohmann::json::array())
        );
    }

    TEST_CASE("参数是普通表达式，不要求字面量——静态可见的是 eval 点的位置，不是它的内容") {
        CHECK(
            parse_json(U"eval(s)") ==
            eval_node(nlohmann::json::array({ident("s")}), nlohmann::json::array())
        );
        const auto result = parse_json(U"eval(a + b)");
        CHECK(result["type"] == "Eval");
        CHECK(result["positional_args"][0]["type"] == "OpBinary");
    }

    TEST_CASE("关键字实参：eval(code='x') 语法上合法，恰好绑出 code 是运行期的事") {
        const auto result = parse_json(U"eval(code='x')");
        CHECK(result["type"] == "Eval");
        CHECK(result["positional_args"] == nlohmann::json::array());
        CHECK(result["keyword_args"].size() == 1);
        CHECK(result["keyword_args"][0]["keyword"] == "code");
        CHECK(result["keyword_args"][0]["value"] == str_lit("x"));
    }

    TEST_CASE("* / ** 展开进对应的组（keyword 为 null），跟普通函数调用共用同一套分组规则") {
        const auto result = parse_json(U"eval(**opts)");
        CHECK(result["positional_args"] == nlohmann::json::array());
        CHECK(result["keyword_args"].size() == 1);
        CHECK(result["keyword_args"][0]["keyword"] == nullptr);
        CHECK(result["keyword_args"][0]["value"]["type"] == "DoubleStar");
    }

    TEST_CASE("产出的值参与外层表达式，不像 return/raise 那样吃到底") {
        const auto result = parse_json(U"eval(s) + 1");
        CHECK(result["type"] == "OpBinary");
        CHECK(result["left"]["type"] == "Eval");
        CHECK(result["right"] == nlohmann::json{{"type", "LiteralInt"}, {"raw", "1"}});
    }

    TEST_CASE("可以被索引/调用/取属性，后缀照常接在它身上") {
        const auto result = parse_json(U"eval(s).x");
        CHECK(result["type"] == "Attr");
        CHECK(result["attr"] == "x");
        CHECK(result["object"]["type"] == "Eval");
    }

    TEST_CASE("括号强制：eval 不是函数值，没有裸 eval 这种写法") {
        CHECK_THROWS_AS(parse_as_file(U"eval"), SyntaxError);
        CHECK_THROWS_AS(parse_as_file(U"f = eval"), SyntaxError);
        CHECK_THROWS_AS(parse_as_file(U"eval 'x'"), SyntaxError);
    }

    TEST_CASE("空实参表、多个位置实参都合法（参数个数对不对是运行期的事，同 import 调用形态）") {
        CHECK(parse_json(U"eval()") == eval_node(nlohmann::json::array(), nlohmann::json::array()));
        CHECK(
            parse_json(U"eval('a', 'b')") ==
            eval_node(nlohmann::json::array({str_lit("a"), str_lit("b")}), nlohmann::json::array())
        );
    }

    TEST_CASE("实参分组规则跟普通函数调用共用同一套：位置实参不能出现在关键字实参之后") {
        check_parse_throws_with(
            U"eval(code='x', 'y')", "positional argument cannot appear after keyword argument"
        );
    }

    TEST_CASE("未闭合括号报的是函数调用那一套错") {
        check_parse_throws_with(U"eval('x'", "expected ')' to close function call");
    }

    TEST_CASE("括号内换行照常当空白（普通函数调用的待遇）") {
        CHECK(
            parse_json(U"eval(\n  s\n)") ==
            eval_node(nlohmann::json::array({ident("s")}), nlohmann::json::array())
        );
    }

    TEST_CASE("eval 后面允许换行再接调用（跟 import 一视同仁）") {
        CHECK(parse_json(U"eval\n('x')") == parse_json(U"eval('x')"));
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
