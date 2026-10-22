// 复合表达式的切分、嵌套和独立换行语境。
#include "../../../cppexceptions/SyntaxError.h"
#include "../test_utils.h"

#include <doctest/doctest.h>

namespace {

nlohmann::json compound(const nlohmann::json &exprs) {
    return nlohmann::json{{"type", "Compound"}, {"exprs", exprs}};
}

} // namespace

TEST_SUITE("{} 一律是复合表达式") {

    TEST_CASE("空 {} 是空的复合表达式，空字典写 ${}") {
        CHECK(parse_json(U"{}") == compound(nlohmann::json::array()));
    }

    TEST_CASE("单条、分号分隔、换行分隔") {
        CHECK(parse_json(U"{x}") == compound(nlohmann::json::array({ident("x")})));
        CHECK(
            parse_json(U"{a; b; c}") ==
            compound(nlohmann::json::array({ident("a"), ident("b"), ident("c")}))
        );
        CHECK(
            parse_json(U"{a\nb\nc}") ==
            compound(nlohmann::json::array({ident("a"), ident("b"), ident("c")}))
        );
    }

    TEST_CASE("前导、尾随分隔符") {
        const auto empty = compound(nlohmann::json::array());
        CHECK(parse_json(U"{;}") == empty);
        CHECK(parse_json(U"{;;}") == empty);
        CHECK(parse_json(U"{\n}") == empty);
        CHECK(parse_json(U"{\n;\n}") == empty);

        const auto single = compound(nlohmann::json::array({ident("a")}));
        CHECK(parse_json(U"{; a}") == single);
        CHECK(parse_json(U"{a;}") == single);
        CHECK(parse_json(U"{a\n}") == single);
    }

    TEST_CASE("嵌套：{{}} 的唯一一条子表达式是内层的空复合表达式") {
        CHECK(
            parse_json(U"{{}}") ==
            compound(nlohmann::json::array({compound(nlohmann::json::array())}))
        );
    }

    TEST_CASE("块首的 'x: T' 是声明，不会被当成字典") {
        CHECK(
            parse_json(U"{x: int = 1}") ==
            compound(nlohmann::json::array({declare("x", false, ident("int"), int_lit("1"))}))
        );
        CHECK(
            parse_json(U"{k: v}") ==
            compound(nlohmann::json::array({declare("k", false, ident("v"), nullptr)}))
        );
        CHECK(
            parse_json(U"{; a: b}") ==
            compound(nlohmann::json::array({declare("a", false, ident("b"), nullptr)}))
        );
    }

    TEST_CASE("{**d} 也是复合表达式，** 的位置合不合法由语义层判") {
        CHECK(
            parse_json(U"{**d}") ==
            compound(nlohmann::json::array({{{"type", "DoubleStar"}, {"operand", ident("d")}}}))
        );
    }

    TEST_CASE("func/class 之后的 {} 是函数体/类体（Program）") {
        const AstNodePtr node{parse_single(U"func f() {}")};
        const auto *func_node{dynamic_cast<AstNodeFunc *>(node.get())};
        REQUIRE(func_node != nullptr);
        CHECK(func_node->body_->exprs_.empty());

        const AstNodePtr class_node{parse_single(U"class C {}")};
        const auto *cls{dynamic_cast<AstNodeClass *>(class_node.get())};
        REQUIRE(cls != nullptr);
        CHECK(cls->body_->exprs_.empty());
    }

    TEST_CASE("{a, b} 不是合法的复合表达式") {
        CHECK_THROWS_AS(parse_program(U"{a, b}"), SyntaxError);
    }

    TEST_CASE("表达式之间没有分隔符必须报错，跟顶层 a b 同一个错误") {
        CHECK_THROWS_AS(parse_program(U"{k v}"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"{a b; c}"), SyntaxError);
    }

    TEST_CASE("未闭合的 {} 抛异常") {
        CHECK_THROWS_AS(parse_program(U"{a; b"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"{k: v"), SyntaxError);
    }
}

TEST_SUITE("{} 内部是独立的语句语境：块内换行不受外层括号续行规则影响") {

    TEST_CASE("调用实参、列表元素、分组里的复合表达式，内部换行照常分隔语句") {
        const auto ab = compound(nlohmann::json::array({ident("a"), ident("b")}));
        CHECK(parse_json(U"f({a\nb})")["positional_args"][0] == ab);
        CHECK(parse_json(U"[{a\nb}]")["items"][0] == ab);
        CHECK(parse_json(U"({a\nb})") == ab);
        CHECK(parse_json(U"f({; a\nb})")["positional_args"][0] == ab);
        CHECK(parse_json(U"${k -> {a\nb}}")["items"][0]["value"] == ab);
    }

    TEST_CASE("嵌套复合表达式：外层在调用实参里，内层还有自己的换行") {
        CHECK(
            parse_json(U"f({a\n{b\nc}})")["positional_args"][0] ==
            compound(
                nlohmann::json::array(
                    {ident("a"), compound(nlohmann::json::array({ident("b"), ident("c")}))}
                )
            )
        );
    }

    TEST_CASE("函数体/类体同样是独立语句语境（整个 func/class 写在调用实参里）") {
        CHECK(parse_json(U"f(func() {a\nb})")["positional_args"][0]["body"]["exprs"].size() == 2);
        CHECK(parse_json(U"f(class C {a\nb})")["positional_args"][0]["body"]["exprs"].size() == 2);
    }

    TEST_CASE("复合表达式里可以声明，即使它整个在实参里") {
        CHECK(
            parse_json(U"f({x: int})")["positional_args"][0] ==
            compound(nlohmann::json::array({declare("x", false, ident("int"), nullptr)}))
        );
    }

    TEST_CASE("'}' 之后回到外层语境：括号内 '}' 后面的换行仍按括号续行规则合并") {
        CHECK(
            parse_json(U"f({a; b}\n.c)")["positional_args"][0] ==
            nlohmann::json{
                {"type", "Attr"},
                {"object", compound(nlohmann::json::array({ident("a"), ident("b")}))},
                {"attr", "c"}
            }
        );
    }

    TEST_CASE("顶层（无外层括号）'}' 后面的换行则正常终止语句，'.c' 不能开启新语句") {
        CHECK_THROWS_AS(parse_program(U"{a; b}\n.c"), SyntaxError);
    }

    TEST_CASE("块内语句该报的错照报：{} 写进括号里不会放松块内的分隔符要求") {
        CHECK_THROWS_AS(parse_program(U"f({a b})"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"${k -> {a b}}"), SyntaxError);
    }
}
