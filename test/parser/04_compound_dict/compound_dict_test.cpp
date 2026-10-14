// `{}` 一律是复合表达式；`${}` 是字典或集合字面量，第一项定种类，一项都没有是空字典。
#include "../../../cppexceptions/SyntaxError.h"
#include "../test_utils.h"

#include <string>

#include <doctest/doctest.h>

namespace {

nlohmann::json compound(const nlohmann::json &exprs) {
    return nlohmann::json{{"type", "Compound"}, {"exprs", exprs}};
}

nlohmann::json dict(const nlohmann::json &items) {
    return nlohmann::json{{"type", "LiteralDict"}, {"items", items}};
}

nlohmann::json set(const nlohmann::json &items) {
    return nlohmann::json{{"type", "LiteralSet"}, {"items", items}};
}

nlohmann::json pair(const nlohmann::json &key, const nlohmann::json &value) {
    return nlohmann::json{{"key", key}, {"value", value}};
}

// 字典里的 **expr 展开项
nlohmann::json spread(const char *name) {
    return pair(nlohmann::json{{"type", "DoubleStar"}, {"operand", ident(name)}}, nullptr);
}

nlohmann::json star(const char *name) {
    return nlohmann::json{{"type", "Star"}, {"operand", ident(name)}};
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

    TEST_CASE("未闭合的 {} 抛异常") {
        CHECK_THROWS_AS(parse_program(U"{a; b"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"{k: v"), SyntaxError);
    }
}

TEST_SUITE("${} 字典") {

    TEST_CASE("${} 是空字典") { CHECK(parse_json(U"${}") == dict(nlohmann::json::array())); }

    TEST_CASE("$ 与 { 之间可以有空白和换行") {
        CHECK(parse_json(U"$ {}") == dict(nlohmann::json::array()));
        CHECK(parse_json(U"$\n{}") == dict(nlohmann::json::array()));
        CHECK(
            parse_json(U"$ /* c */ {k -> v}") ==
            dict(nlohmann::json::array({pair(ident("k"), ident("v"))}))
        );
    }

    TEST_CASE("键值对，尾逗号可选") {
        const auto expected = dict(
            nlohmann::json::array({pair(ident("k"), ident("v")), pair(ident("k2"), ident("v2"))})
        );
        CHECK(parse_json(U"${k -> v, k2 -> v2}") == expected);
        CHECK(parse_json(U"${k->v,k2->v2,}") == expected);
        CHECK(parse_json(U"${\nk -> v,\nk2 -> v2,\n}") == expected);
        CHECK(parse_json(U"${k\n->\nv, k2 -> v2}") == expected);
    }

    TEST_CASE("键和值都是完整的表达式，'->' 不是运算符") {
        CHECK(
            parse_json(U"${a + 1 -> b or c}") == dict(
                                                     nlohmann::json::array({pair(
                                                         {{"type", "OpBinary"},
                                                          {"op", "+"},
                                                          {"left", ident("a")},
                                                          {"right", int_lit("1")}},
                                                         {{"type", "OpBinary"},
                                                          {"op", "or"},
                                                          {"left", ident("b")},
                                                          {"right", ident("c")}}
                                                     )})
                                                 )
        );
        // 负数值紧跟 '->'：'->' 是一个 token，'-' 留给值
        CHECK(
            parse_json(U"${k->-1}")["items"][0]["value"] ==
            nlohmann::json{{"type", "OpUnary"}, {"op", "-"}, {"operand", int_lit("1")}}
        );
    }

    TEST_CASE("** 展开项可以出现在任意位置、可以有多个") {
        CHECK(parse_json(U"${**d}") == dict(nlohmann::json::array({spread("d")})));
        CHECK(
            parse_json(U"${**d1, k -> v, **d2,}") ==
            dict(nlohmann::json::array({spread("d1"), pair(ident("k"), ident("v")), spread("d2")}))
        );
        CHECK(parse_json(U"${**\nd}") == dict(nlohmann::json::array({spread("d")})));
    }

    TEST_CASE("展开项外面套分组括号是透明的") {
        CHECK(parse_json(U"${(**d)}") == parse_json(U"${**d}"));
        CHECK(parse_json(U"${k -> v, ((**d2))}") == parse_json(U"${k -> v, **d2}"));
    }

    TEST_CASE("展开项不能带值") {
        check_parse_throws_with(U"${**d -> v}", "a '**' item cannot be followed by '->'");
        check_parse_throws_with(U"${(**d) -> v}", "a '**' item cannot be followed by '->'");
    }

    TEST_CASE("残缺的键值对报错") {
        CHECK_THROWS_AS(parse_program(U"${k ->}"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"${-> v}"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"${k -> v,,}"), SyntaxError);
    }

    TEST_CASE("项之间只能用逗号分隔") {
        CHECK_THROWS_AS(parse_program(U"${k -> v; k2 -> v2}"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"${k -> v\nk2 -> v2}"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"${**d; x}"), SyntaxError);
    }

    TEST_CASE("括号里的换行不算分隔：值跨行接着解析，跟列表一样") {
        CHECK(parse_json(U"${k -> v\n+ w}") == parse_json(U"${k -> v + w}"));
        CHECK(parse_json(U"f(${k -> v\n+ w})") == parse_json(U"f(${k -> v + w})"));
    }

    TEST_CASE("$ 后面不是 { 报错") {
        check_parse_throws_with(U"$ x", "expected '{' after '$'");
        check_parse_throws_with(U"$(1)", "expected '{' after '$'");
        check_parse_throws_with(U"$[1]", "expected '{' after '$'");
        check_parse_throws_with(U"x = $", "expected '{' after '$'");
    }

    TEST_CASE("未闭合报错，位置指向 EOF") {
        // "${'a' -> 1" 共 10 个字符，EOF 在第 11 列
        try {
            parse_program(U"${'a' -> 1");
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("dict literal") != std::string::npos);
            CHECK(msg.find("1:11:") != std::string::npos);
        }
    }

    TEST_CASE("键和值里都可以直接写赋值和声明") {
        CHECK(
            parse_json(U"${k -> x = 1}") ==
            dict(nlohmann::json::array({pair(ident("k"), bare_assign(ident("x"), int_lit("1")))}))
        );
        CHECK(
            parse_json(U"${k -> x: int}") ==
            dict(
                nlohmann::json::array(
                    {pair(ident("k"), declare("x", false, ident("int"), nullptr))}
                )
            )
        );
        CHECK(
            parse_json(U"${x: int -> 1}") ==
            dict(
                nlohmann::json::array(
                    {pair(declare("x", false, ident("int"), nullptr), int_lit("1"))}
                )
            )
        );
    }

    TEST_CASE("值可以是复合表达式，复合表达式里也可以有字典") {
        CHECK(
            parse_json(U"${k -> {a; b}}") ==
            dict(
                nlohmann::json::array(
                    {pair(ident("k"), compound(nlohmann::json::array({ident("a"), ident("b")})))}
                )
            )
        );
        CHECK(
            parse_json(U"{ ${k -> v}; x }") ==
            compound(
                nlohmann::json::array(
                    {dict(nlohmann::json::array({pair(ident("k"), ident("v"))})), ident("x")}
                )
            )
        );
    }

    TEST_CASE("嵌套字典") {
        CHECK(
            parse_json(U"${k -> ${a -> b}}") ==
            dict(
                nlohmann::json::array(
                    {pair(ident("k"), dict(nlohmann::json::array({pair(ident("a"), ident("b"))})))}
                )
            )
        );
    }

    TEST_CASE("'->' 只在字典项里有意义，别处出现报错") {
        CHECK_THROWS_AS(parse_program(U"a -> b"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"[a -> b]"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"f(a -> b)"), SyntaxError);
        // 冒号不再是字典分隔符
        CHECK(parse_json(U"${k: v}")["type"] == "LiteralSet");
    }
}

TEST_SUITE("${} 集合") {

    TEST_CASE("普通项，尾逗号可选") {
        CHECK(parse_json(U"${a}") == set(nlohmann::json::array({ident("a")})));
        CHECK(parse_json(U"${a, b,}") == set(nlohmann::json::array({ident("a"), ident("b")})));
        CHECK(parse_json(U"${\na,\nb\n}") == set(nlohmann::json::array({ident("a"), ident("b")})));
    }

    TEST_CASE("* 展开项") {
        CHECK(parse_json(U"${*s}") == set(nlohmann::json::array({star("s")})));
        CHECK(parse_json(U"${a, *s}") == set(nlohmann::json::array({ident("a"), star("s")})));
        CHECK(parse_json(U"${(*s)}") == parse_json(U"${*s}"));
    }

    TEST_CASE("项可以直接是声明") {
        CHECK(
            parse_json(U"${x: int}") ==
            set(nlohmann::json::array({declare("x", false, ident("int"), nullptr)}))
        );
        CHECK(parse_json(U"${(x: int)}") == parse_json(U"${x: int}"));
        CHECK(
            parse_json(U"${x: const = 1, y}") ==
            set(nlohmann::json::array({declare("x", true, nullptr, int_lit("1")), ident("y")}))
        );
    }

    TEST_CASE("项可以是裸赋值和复合赋值") {
        CHECK(
            parse_json(U"${x = 1}") ==
            set(nlohmann::json::array({bare_assign(ident("x"), int_lit("1"))}))
        );
        CHECK(parse_json(U"${x += 1}")["items"][0]["type"] == "CompoundAssign");
    }

    TEST_CASE("项的根不是 ** 展开时不算字典展开项") {
        // 根是加法，没有 '->'，所以是集合；里面的 ** 合不合法由语义层判
        CHECK(parse_json(U"${**d + x}")["type"] == "LiteralSet");
    }

    TEST_CASE("未闭合报错") {
        check_parse_throws_with(U"${a, b", "expected '}' to close set literal");
    }
}

TEST_SUITE("字典项和集合项不能混用") {

    TEST_CASE("集合里出现键值对或 ** 展开") {
        check_parse_throws_with(U"${a, k -> v}", "set literal");
        check_parse_throws_with(U"${*s, **d}", "set literal");
    }

    TEST_CASE("字典里出现普通项或 * 展开") {
        check_parse_throws_with(U"${k -> v, a}", "dict literal");
        check_parse_throws_with(U"${**d, *s}", "dict literal");
        check_parse_throws_with(U"${k -> v, **d + x}", "dict literal");
        check_parse_throws_with(U"${k -> v, x, y -> z}", "dict literal");
        check_parse_throws_with(U"${k -> v, x: int}", "dict literal");
    }

    TEST_CASE("报错位置指向出问题的那一项") {
        // "${k -> v, a}" 里 a 在第 11 列
        check_parse_throws_with(U"${k -> v, a}", "1:11:");
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
