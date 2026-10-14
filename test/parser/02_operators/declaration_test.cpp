// 带 ':' 的赋值（声明）：identifier : ⟦const⟧ ⟦type⟧ ⟦= expr⟧，跟裸赋值同一个节点。
#include "../../../cppexceptions/SyntaxError.h"
#include "../test_utils.h"

#include <string>

#include <doctest/doctest.h>

namespace {

nlohmann::json call0(const char *name) {
    return nlohmann::json{
        {"type", "Call"},
        {"object", ident(name)},
        {"positional_args", nlohmann::json::array()},
        {"keyword_args", nlohmann::json::array()}
    };
}

} // namespace

TEST_SUITE("声明——四种写法") {

    TEST_CASE("x: T = v") {
        CHECK(parse_json(U"x: int = 1") == declare("x", false, ident("int"), int_lit("1")));
    }

    TEST_CASE("x: T 没有值") {
        CHECK(parse_json(U"x: int") == declare("x", false, ident("int"), nullptr));
    }

    TEST_CASE("x: const T = v、x: const = v") {
        CHECK(parse_json(U"x: const int = 1") == declare("x", true, ident("int"), int_lit("1")));
        CHECK(parse_json(U"x: const = 1") == declare("x", true, nullptr, int_lit("1")));
    }

    TEST_CASE("x: const T、x: const") {
        CHECK(parse_json(U"x: const int") == declare("x", true, ident("int"), nullptr));
        CHECK(parse_json(U"x: const") == declare("x", true, nullptr, nullptr));
    }

    TEST_CASE("裸赋值和声明是同一个节点，裸赋值的注解为空") {
        CHECK(parse_json(U"x = 1") == bare_assign(ident("x"), int_lit("1")));
    }

    TEST_CASE("类型是字符串字面量时 const 和字符串都留在节点上") {
        CHECK(parse_json(U"x: const 'doc'") == declare("x", true, str_lit("doc"), nullptr));
        CHECK(
            parse_json(U"x: const 'doc' = 1") == declare("x", true, str_lit("doc"), int_lit("1"))
        );
    }

    TEST_CASE("类型是字符串字面量时照常解析") {
        CHECK(
            parse_json(U"x: 'whatever' = 1") ==
            declare("x", false, str_lit("whatever"), int_lit("1"))
        );
    }
}

TEST_SUITE("声明——优先级与结合") {

    TEST_CASE("类型按高于 '=' 的优先级解析") {
        CHECK(
            parse_json(U"x: int | str? = v") ==
            declare(
                "x",
                false,
                {{"type", "OpBinary"},
                 {"op", "|"},
                 {"left", ident("int")},
                 {"right", {{"type", "OpUnary"}, {"op", "?"}, {"operand", ident("str")}}}},
                ident("v")
            )
        );
        // 类型里的 or 也照常结合，赋值一档最低
        CHECK(
            parse_json(U"x: a or b = v")["annotation"]["type"] ==
            nlohmann::json{
                {"type", "OpBinary"}, {"op", "or"}, {"left", ident("a")}, {"right", ident("b")}
            }
        );
    }

    TEST_CASE("值按赋值的右结合解析") {
        CHECK(
            parse_json(U"x: int = y = 1") ==
            declare("x", false, ident("int"), bare_assign(ident("y"), int_lit("1")))
        );
        CHECK(
            parse_json(U"a = x: int = 1") ==
            bare_assign(ident("a"), declare("x", false, ident("int"), int_lit("1")))
        );
        CHECK(
            parse_json(U"x: int = y: str = 1") ==
            declare("x", false, ident("int"), declare("y", false, ident("str"), int_lit("1")))
        );
    }

    TEST_CASE("类型里要写赋值得加括号") {
        CHECK(
            parse_json(U"x: (T = int) = 1") ==
            declare("x", false, bare_assign(ident("T"), ident("int")), int_lit("1"))
        );
    }

    TEST_CASE("左边比赋值一档高的表达式整体当目标，不是标识符就报错") {
        check_parse_throws_with(U"not x: int", "only an identifier can be declared");
        check_parse_throws_with(U"a or b: int", "only an identifier can be declared");
    }
}

TEST_SUITE("声明——换行") {

    TEST_CASE("':' 之后不完整，并入下一行") {
        CHECK(parse_json(U"x:\nint") == declare("x", false, ident("int"), nullptr));
        CHECK(parse_json(U"x:\nconst\n") == declare("x", true, nullptr, nullptr));
    }

    TEST_CASE("x: const 本身完整，下一行是新的表达式") {
        CHECK(
            parse_program_json(U"x: const\nint")["exprs"] ==
            nlohmann::json::array({declare("x", true, nullptr, nullptr), ident("int")})
        );
    }

    TEST_CASE("x: T 本身完整，下一行的 '= v' 不能接上来") {
        CHECK_THROWS_AS(parse_program(U"x: int\n= 1"), SyntaxError);
    }

    TEST_CASE("括号里的换行是空白") {
        CHECK(parse_json(U"(x: const\nint)") == declare("x", true, ident("int"), nullptr));
        CHECK(parse_json(U"(x: int\n= 1)") == declare("x", false, ident("int"), int_lit("1")));
    }

    TEST_CASE("'=' 之后不完整，并入下一行") {
        CHECK(parse_json(U"x: int =\n1") == declare("x", false, ident("int"), int_lit("1")));
    }
}

TEST_SUITE("声明——语法错误") {

    TEST_CASE("目标只能是标识符") {
        const std::string msg{"only an identifier can be declared"};
        check_parse_throws_with(U"a.b: int = 1", msg);
        check_parse_throws_with(U"a[0]: int", msg);
        check_parse_throws_with(U"(a, b): int", msg);
        check_parse_throws_with(U"[a]: int", msg);
        check_parse_throws_with(U"f(): int", msg);
        check_parse_throws_with(U"1: int", msg);
    }

    TEST_CASE("报错位置指向目标开头") {
        // "y = a.b: int" 里 a 在第 5 列
        check_parse_throws_with(U"y = a.b: int", "1:5:");
    }

    TEST_CASE("冗余括号包住的标识符仍是标识符") {
        CHECK(parse_json(U"(x): int = 1") == parse_json(U"x: int = 1"));
    }

    TEST_CASE("':' 后面 const 和类型一个都没有") {
        const std::string msg{"expected 'const' or a type after ':'"};
        check_parse_throws_with(U"x: = 1", msg);
        check_parse_throws_with(U"x:", msg);
        check_parse_throws_with(U"(x:)", msg);
    }

    TEST_CASE("没有带类型的复合赋值") {
        const std::string msg{"a compound assignment cannot be declared"};
        check_parse_throws_with(U"x: int += 1", msg);
        check_parse_throws_with(U"x: const -= 1", msg);
    }

    TEST_CASE("const 只能紧跟在 ':' 后面") {
        CHECK_THROWS_AS(parse_program(U"const x = 1"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"x = const"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"x: int const = 1"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"x: const const = 1"), SyntaxError);
    }

    TEST_CASE("连着两个 ':' 报错") { CHECK_THROWS_AS(parse_program(U"x: int: str"), SyntaxError); }
}

TEST_SUITE("声明——类型位置上的函数表达式") {

    TEST_CASE("匿名函数表达式可以当类型，冗余括号不影响") {
        CHECK(parse_json(U"x: func () { return int } = 1")["annotation"]["type"]["type"] == "Func");
        CHECK(parse_json(U"x: (func () { return int })")["annotation"]["type"]["type"] == "Func");
        CHECK(
            parse_json(U"x: const func [P]() { return P }")["annotation"]["type"]["type"] == "Func"
        );
    }

    TEST_CASE("带名字的函数表达式不能当类型") {
        const std::string msg{"a function expression used as a type must be anonymous"};
        check_parse_throws_with(U"x: func f() { return int } = 1", msg);
        check_parse_throws_with(U"x: (func f() { return int })", msg);
        check_parse_throws_with(U"x: @deco func f() { return int }", msg);
    }

    TEST_CASE("最外层不是函数表达式时不受限制") {
        CHECK(parse_json(U"x: func f() {}() = 1")["annotation"]["type"]["type"] == "Call");
        CHECK(parse_json(U"x: func f() {} | int")["annotation"]["type"]["type"] == "OpBinary");
    }
}

TEST_SUITE("声明——能出现在哪") {

    TEST_CASE("调用实参里不能直接写，加括号可以") {
        const std::string msg{"an assignment cannot be a call argument"};
        check_parse_throws_with(U"f(x: int)", msg);
        check_parse_throws_with(U"f(a, x: int = 1)", msg);
        check_parse_throws_with(U"f(k = x: int)", msg);
        check_parse_throws_with(U"import(x: int)", msg);
        check_parse_throws_with(U"eval(x: int)", msg);
        CHECK(
            parse_json(U"f((x: int))")["positional_args"][0] ==
            declare("x", false, ident("int"), nullptr)
        );
    }

    TEST_CASE("按关键字传参的实参不受影响") {
        CHECK(parse_json(U"f(k = 1)")["keyword_args"][0]["keyword"] == "k");
    }

    TEST_CASE("实参里的函数体、列表、元组不受限制") {
        CHECK(
            parse_json(U"f(func () { x: int })")["positional_args"][0]["body"]["exprs"][0] ==
            declare("x", false, ident("int"), nullptr)
        );
        CHECK(
            parse_json(U"f([x: int])")["positional_args"][0]["items"][0] ==
            declare("x", false, ident("int"), nullptr)
        );
    }

    TEST_CASE("列表、元组、集合、字典的项和索引参数里可以直接写") {
        CHECK(parse_json(U"[x: int]")["items"][0] == declare("x", false, ident("int"), nullptr));
        CHECK(parse_json(U"(x: int, y)")["items"][0] == declare("x", false, ident("int"), nullptr));
        CHECK(parse_json(U"${x: int}")["items"][0] == declare("x", false, ident("int"), nullptr));
        CHECK(
            parse_json(U"${k -> x: int}")["items"][0]["value"] ==
            declare("x", false, ident("int"), nullptr)
        );
        CHECK(parse_json(U"a[i: int]")["args"][0] == declare("i", false, ident("int"), nullptr));
        // a[i:j] 就是以声明 i: j 为下标
        CHECK(parse_json(U"a[i:j]")["args"][0] == declare("i", false, ident("j"), nullptr));
    }

    TEST_CASE("if / while 的条件只拒绝裸赋值") {
        CHECK(
            parse_json(U"if (ok: bool = f()) a")["clauses"][0]["cond"] ==
            declare("ok", false, ident("bool"), call0("f"))
        );
        CHECK(parse_json(U"while (x: const) a")["cond"] == declare("x", true, nullptr, nullptr));
        CHECK_THROWS_AS(parse_program(U"if (x = 1) a"), SyntaxError);
    }

    TEST_CASE("装饰器作用在整个声明上") {
        CHECK(
            parse_json(U"@d x: int = 1") ==
            nlohmann::json{
                {"type", "Decorator"},
                {"decorator", ident("d")},
                {"target", declare("x", false, ident("int"), int_lit("1"))}
            }
        );
    }

    TEST_CASE("if 的分支、return 的值都可以是声明") {
        CHECK(
            parse_json(U"if (c) x: int else y")["clauses"][0]["body"] ==
            declare("x", false, ident("int"), nullptr)
        );
        CHECK(
            parse_json(U"return x: int = 1")["value"] ==
            declare("x", false, ident("int"), int_lit("1"))
        );
    }
}

TEST_SUITE("声明——位置") {

    TEST_CASE("节点位置是目标的位置") {
        const AstNodePtr node{parse_single(U"  x: const int = 1")};
        const auto *assign{dynamic_cast<AstNodeAssign *>(node.get())};
        REQUIRE(assign != nullptr);
        CHECK(assign->pos_.col == 3);
        CHECK(assign->annotation_.is_const_);
        REQUIRE(assign->annotation_.type_ != nullptr);
        CHECK(assign->annotation_.type_->pos_.col == 12); // x(3):(4) (5)const(6..10) (11)int(12)
    }
}
