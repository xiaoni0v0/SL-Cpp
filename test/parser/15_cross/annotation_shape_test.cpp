// Parser 保留注解原式；隐式函数由 Analyzer 前置转换生成。
#include "../test_utils.h"

#include <string>

#include <doctest/doctest.h>

TEST_SUITE("注解原式的跨语境结构") {

    TEST_CASE("声明、函数签名和绑定保留同一类型表达式，const 与类型分别存放") {
        for (const auto *source :
             {U"int",
              U"'说明'",
              U"tuple[]",
              U"list[int, ...]",
              U"int | str?",
              U"choose(int)",
              U"(inner: const int = 1)",
              U"{ return int; str }",
              U"func [P]() { return P }",
              U"@deco func typed[P]() { return P }",
              U"(func(p = (b: int = 1)) {})()"}) {
            const std::u32string type_source{source};
            const auto type = parse_json(type_source);
            INFO("type AST: " << type.dump());
            for (const bool is_const : {false, true}) {
                CAPTURE(is_const);
                const std::u32string annotation_source{(is_const ? U"const " : U"") + type_source};
                const auto annotation = nlohmann::json{{"is_const", is_const}, {"type", type}};

                const auto declaration = parse_json(U"x: " + annotation_source + U" = value");
                CHECK(declaration["annotation"] == annotation);
                CHECK(declaration["value"] == ident("value"));

                const auto local =
                    parse_json(U"func outer() { x: " + annotation_source + U" = value }");
                CHECK(local["body"]["exprs"][0] == declaration);

                const auto member =
                    parse_json(U"class C { x: " + annotation_source + U" = value }");
                CHECK(member["body"]["exprs"][0] == declaration);

                const auto function = parse_json(
                    U"func f(a: " + annotation_source + U" = default_a, *args, b: " +
                    annotation_source + U" = default_b): " + type_source + U" {}"
                );
                CHECK(function["params"]["positional"][0]["annotation"] == annotation);
                CHECK(function["params"]["positional"][0]["default_value"] == ident("default_a"));
                CHECK(function["params"]["kw_only"][0]["annotation"] == annotation);
                CHECK(function["params"]["kw_only"][0]["default_value"] == ident("default_b"));
                CHECK(function["return_type"] == type);
                CHECK(function["doc"].is_null());
                CHECK(function["body"]["exprs"].empty());

                const auto loop = parse_json(U"for (xs as x: " + annotation_source + U") body");
                CHECK(loop["binding"]["annotation"] == annotation);
                CHECK(loop["binding"]["target"] == ident("x"));
                CHECK(loop["iterable"] == ident("xs"));

                const auto attempt =
                    parse_json(U"try work except (E as e: " + annotation_source + U") body");
                CHECK(attempt["except_clauses"][0]["binding"]["annotation"] == annotation);
                CHECK(attempt["except_clauses"][0]["binding"]["target"] == ident("e"));
                CHECK(attempt["except_clauses"][0]["exc_type"] == ident("E"));
            }
        }
    }

    TEST_CASE("只写 const 时不补 object 节点，也不生成函数") {
        const auto annotation = nlohmann::json{{"is_const", true}, {"type", nullptr}};
        const auto function = parse_json(U"func f(a: const, *args, b: const = 1) {}");
        CHECK(function["params"]["positional"][0]["annotation"] == annotation);
        CHECK(function["params"]["positional"][0]["default_value"].is_null());
        CHECK(function["params"]["kw_only"][0]["annotation"] == annotation);
        CHECK(function["params"]["kw_only"][0]["default_value"] == int_lit("1"));
        CHECK(
            parse_json(U"class C { x: const }")["body"]["exprs"][0] ==
            declare("x", true, nullptr, nullptr)
        );
        CHECK(
            parse_json(U"class C { x: const = 1 }")["body"]["exprs"][0] ==
            declare("x", true, nullptr, int_lit("1"))
        );
        CHECK(parse_json(U"for (xs as x: const) body")["binding"]["annotation"] == annotation);
        CHECK(
            parse_json(
                U"try work except (E as e: const) body"
            )["except_clauses"][0]["binding"]["annotation"] == annotation
        );
    }

    TEST_CASE("函数类型与调用该函数的表达式保留不同的最外层节点") {
        const auto j = parse_json(
            U"class C { "
            U"x: func(p = (a: int = 1)) {}; "
            U"y: (func(p = (b: int = 1)) {})() }"
        );
        const auto &direct = j["body"]["exprs"][0]["annotation"]["type"];
        const auto &called = j["body"]["exprs"][1]["annotation"]["type"];
        CHECK(direct["type"] == "Func");
        CHECK(
            direct["params"]["positional"][0]["default_value"] ==
            declare("a", false, ident("int"), int_lit("1"))
        );
        CHECK(called["type"] == "Call");
        CHECK(called["object"]["type"] == "Func");
        CHECK(
            called["object"]["params"]["positional"][0]["default_value"] ==
            declare("b", false, ident("int"), int_lit("1"))
        );
        CHECK(called["positional_args"].empty());
        CHECK(called["keyword_args"].empty());
        CHECK(j["body"]["exprs"].size() == 2);
    }

    TEST_CASE("类型中的赋值、声明和 return 保留原节点，不按 Python 的注解限制拒绝") {
        const auto j = parse_json(U"func f(x: { n: const int = 1; n += 1; return int; str }) {}");
        const auto &type = j["params"]["positional"][0]["annotation"]["type"];
        CHECK(type["type"] == "Compound");
        REQUIRE(type["exprs"].size() == 4);
        CHECK(type["exprs"][0] == declare("n", true, ident("int"), int_lit("1")));
        CHECK(type["exprs"][1]["type"] == "CompoundAssign");
        CHECK(type["exprs"][1]["target"] == ident("n"));
        CHECK(type["exprs"][1]["value"] == int_lit("1"));
        CHECK(type["exprs"][2] == nlohmann::json{{"type", "Return"}, {"value", ident("int")}});
        CHECK(type["exprs"][3] == ident("str"));
    }

    TEST_CASE("类体内的分支、循环、函数默认值和嵌套类声明保留在原位置") {
        const auto j = parse_json(
            U"class C { "
            U"if (flag) { x: const int }; "
            U"for (xs as item) y: str; "
            U"func f(p = (z: int = 1)) { temp: str }; "
            U"class Inner { value: int } }"
        );
        const auto &body = j["body"]["exprs"];
        REQUIRE(body.size() == 4);
        CHECK(
            body[0]["clauses"][0]["body"]["exprs"][0] == declare("x", true, ident("int"), nullptr)
        );
        CHECK(body[1]["body"] == declare("y", false, ident("str"), nullptr));
        CHECK(
            body[2]["params"]["positional"][0]["default_value"] ==
            declare("z", false, ident("int"), int_lit("1"))
        );
        CHECK(body[2]["body"]["exprs"][0] == declare("temp", false, ident("str"), nullptr));
        CHECK(body[3]["body"]["exprs"][0] == declare("value", false, ident("int"), nullptr));
    }

    TEST_CASE("类体和复合表达式中 const 后换行结束声明，外围调用括号不改变这条边界") {
        const auto expected = nlohmann::json::array(
            {declare("x", true, nullptr, nullptr), bare_assign(ident("T"), ident("value"))}
        );
        CHECK(parse_json(U"class C { x:\n const\n T = value }")["body"]["exprs"] == expected);
        CHECK(
            parse_json(U"f({ x:\n const\n T = value })")["positional_args"][0]["exprs"] == expected
        );
    }

    TEST_CASE("跨行注解的类型节点保留源码位置，不出现合成节点") {
        const AstNodePtr root{parse_single(U"func f(a:\n const\n T):\n R {}")};
        const auto *function{dynamic_cast<const AstNodeFunc *>(root.get())};
        REQUIRE(function != nullptr);
        REQUIRE(function->params_.positional_.size() == 1);
        const auto &annotation{function->params_.positional_[0].annotation_};
        CHECK(annotation.is_const_);
        const auto *param_type{dynamic_cast<const AstNodeIdentifier *>(annotation.type_.get())};
        REQUIRE(param_type != nullptr);
        CHECK(param_type->identifier_ == U"T");
        CHECK(param_type->pos_.row == 3);
        CHECK(param_type->pos_.col == 2);
        const auto *return_type{
            dynamic_cast<const AstNodeIdentifier *>(function->return_type_.get())
        };
        REQUIRE(return_type != nullptr);
        CHECK(return_type->identifier_ == U"R");
        CHECK(return_type->pos_.row == 4);
        CHECK(return_type->pos_.col == 2);

        const AstNodePtr class_root{parse_single(U"class C {\n x:\n const T = value\n}")};
        const auto *class_node{dynamic_cast<const AstNodeClass *>(class_root.get())};
        REQUIRE(class_node != nullptr);
        REQUIRE(class_node->body_->exprs_.size() == 1);
        const auto *declaration{
            dynamic_cast<const AstNodeAssign *>(class_node->body_->exprs_[0].get())
        };
        REQUIRE(declaration != nullptr);
        CHECK(declaration->annotation_.is_const_);
        const auto *member_type{
            dynamic_cast<const AstNodeIdentifier *>(declaration->annotation_.type_.get())
        };
        REQUIRE(member_type != nullptr);
        CHECK(member_type->identifier_ == U"T");
        CHECK(member_type->pos_.row == 3);
        CHECK(member_type->pos_.col == 8);
    }

    TEST_CASE("缺失或不完整的注解在各入口位置都抛 SyntaxError") {
        for (const auto *source :
             {U"", U"const const", U"int const", U"int: str", U"int += 1", U"int |", U"@deco"}) {
            const std::u32string type_source{source};
            CHECK_THROWS_AS(parse_program(U"x: " + type_source), SyntaxError);
            CHECK_THROWS_AS(parse_program(U"func f(a: " + type_source + U") {}"), SyntaxError);
            CHECK_THROWS_AS(parse_program(U"func f(): " + type_source + U" {}"), SyntaxError);
            CHECK_THROWS_AS(parse_program(U"class C { x: " + type_source + U" }"), SyntaxError);
            CHECK_THROWS_AS(
                parse_program(U"for (xs as x: " + type_source + U") body"), SyntaxError
            );
            CHECK_THROWS_AS(
                parse_program(U"try work except (E as e: " + type_source + U") body"), SyntaxError
            );
        }
        CHECK_THROWS_AS(parse_program(U"for (xs as x: int = 1) body"), SyntaxError);
        CHECK_THROWS_AS(
            parse_program(U"try work except (E as e: const = value) body"), SyntaxError
        );
    }
}
