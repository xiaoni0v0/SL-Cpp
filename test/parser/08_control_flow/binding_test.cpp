// 控制头部中的 as 绑定：主表达式、目标和注解各自保留，具体绑定值由外层决定。
#include "../../../cppexceptions/SyntaxError.h"
#include "../test_utils.h"

#include <string>

#include <doctest/doctest.h>

TEST_SUITE("控制头部的 as 绑定") {

    TEST_CASE("主表达式中的声明与绑定的类型注解互不混淆") {
        const auto source = declare("source", false, ident("T"), ident("items"));
        const auto expected_binding = binding(ident("target"), ident("U"));
        const auto loop = parse_json(U"for (source: T = items as target: U) body");
        CHECK(loop["iterable"] == source);
        CHECK(loop["binding"] == expected_binding);
        const auto attempt = parse_json(U"try work except (source: T = items as target: U) body");
        CHECK(attempt["except_clauses"][0]["exc_type"] == source);
        CHECK(attempt["except_clauses"][0]["binding"] == expected_binding);
    }

    TEST_CASE("复杂目标仍按左值语法解析，不带注解时不提前收紧为标识符") {
        const auto loop = parse_json(U"for (make_items() as slot()[i]) body");
        CHECK(loop["iterable"] == parse_json(U"make_items()"));
        CHECK(loop["binding"]["target"] == parse_json(U"slot()[i]"));
        CHECK(loop["binding"]["annotation"]["type"].is_null());
        const auto attempt = parse_json(U"try work except (E as (first, *rest)) body");
        CHECK(attempt["except_clauses"][0]["binding"]["target"] == parse_json(U"(first, *rest)"));
    }

    TEST_CASE("as 后只收目标与注解，不收初始化值、复合赋值或第二次绑定") {
        for (const auto *source :
             {U"items as target = value",
              U"items as target += 1",
              U"items as target: T = value",
              U"items as target: const = value",
              U"items as target as another",
              U"items as target:",
              U"items as target: const const"}) {
            const std::u32string header{source};
            CHECK_THROWS_AS(parse_program(U"for (" + header + U") body"), SyntaxError);
            CHECK_THROWS_AS(parse_program(U"try work except (" + header + U") body"), SyntaxError);
        }
    }

    TEST_CASE("as 只在控制头部引出绑定，不是普通表达式运算符") {
        CHECK_THROWS_AS(parse_program(U"items as target"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"f(items as target)"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"for ((items as target)) body"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"try work except ((E as e)) body"), SyntaxError);
        CHECK(
            parse_json(U"for (return as target) body")["iterable"] ==
            nlohmann::json{{"type", "Return"}, {"value", nullptr}}
        );
    }

    TEST_CASE("as 不会吞掉目标的源码位置") {
        const AstNodePtr root{parse_single(U"for (items as target: T) body")};
        const auto *loop{dynamic_cast<const AstNodeForIter *>(root.get())};
        REQUIRE(loop != nullptr);
        REQUIRE(loop->binding_.has_value());
        CHECK(loop->iterable_->pos_.col == 6);
        CHECK(loop->binding_->target_->pos_.col == 15);
        REQUIRE(loop->binding_->annotation_.type_ != nullptr);
        CHECK(loop->binding_->annotation_.type_->pos_.col == 23);
    }
}
