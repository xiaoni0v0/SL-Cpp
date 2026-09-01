// import 调用形态的实参折叠。
#include "../test_utils.h"

#include <doctest/doctest.h>

namespace {
nlohmann::json import_kw(const nlohmann::json &segments) {
    return nlohmann::json{{"type", "ImportKw"}, {"segments", segments}};
}

nlohmann::json program(const nlohmann::json &exprs) {
    return nlohmann::json{{"type", "Program"}, {"exprs", exprs}};
}
} // namespace

TEST_SUITE("ExprFolder import 关键字形态") {

    TEST_CASE("没有子表达式，原样不变") {
        CHECK(fold_json(U"import math") == import_kw(nlohmann::json::array({"math"})));
        CHECK(fold_json(U"import os.path") == import_kw(nlohmann::json::array({"os", "path"})));
    }

    TEST_CASE("不是纯字面量，Program 剪枝时不会被剪掉") {
        CHECK(
            fold_program_json(U"1; import a; 2") ==
            program(nlohmann::json::array({import_kw(nlohmann::json::array({"a"}))}))
        );
    }

    TEST_CASE("作为别处的子表达式时，外层照常折叠，它自己原样保留") {
        const auto result = fold_json(U"f(1 + 1, import a)");
        CHECK(result["positional_args"][0] == int_lit("2"));
        CHECK(result["positional_args"][1] == import_kw(nlohmann::json::array({"a"})));
    }
}

TEST_SUITE("ExprFolder import 调用形态") {

    TEST_CASE("位置实参照常折叠") {
        const auto result = fold_json(U"import('ma' + 'th')");
        CHECK(result["type"] == "ImportCall");
        CHECK(result["positional_args"] == nlohmann::json::array({str_lit("math")}));
    }

    TEST_CASE("关键字实参照常折叠") {
        const auto result = fold_json(U"import('os', lazy=1 == 1, force=not True)");
        CHECK(result["keyword_args"][0]["value"] == bool_lit(true));
        CHECK(result["keyword_args"][1]["value"] == bool_lit(false));
    }

    TEST_CASE("* / ** 展开项的操作数也照常折叠") {
        const auto result = fold_json(U"import(*(1 + 1), **(2 + 2))");
        CHECK(result["positional_args"][0]["operand"] == int_lit("2"));
        CHECK(result["keyword_args"][0]["value"]["operand"] == int_lit("4"));
    }

    TEST_CASE("整体恒不折，哪怕实参全是字面量") {
        CHECK(fold_json(U"import('math')")["type"] == "ImportCall");
    }

    TEST_CASE("不是纯字面量，Program 剪枝时不会被剪掉") {
        CHECK(fold_program_json(U"1; import('math')")["exprs"].size() == 1);
    }
}
