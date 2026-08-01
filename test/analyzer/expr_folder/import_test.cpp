// ExprFolder：import 表达式（SL.md 2.2.5/3.4.5）。
// 关键字形态没有任何子表达式，整体也不是字面量，所以既折不动也剪不掉（导入是有副作用的：它会
// 执行模块源文件、往当前作用域绑定名字）；调用形态就是普通的 AstNodeCall，实参照常递归折叠。
#include "../test_utils.h"

#include <doctest/doctest.h>

namespace {
nlohmann::json import_node(const nlohmann::json &segments) {
    return nlohmann::json{{"type", "Import"}, {"segments", segments}};
}
} // namespace

TEST_SUITE("ExprFolder import") {

    TEST_CASE("关键字形态没有子表达式，原样不变") {
        CHECK(fold_json(U"import math") == import_node(nlohmann::json::array({"math"})));
        CHECK(fold_json(U"import os.path") == import_node(nlohmann::json::array({"os", "path"})));
    }

    TEST_CASE("关键字形态不是纯字面量，Program 剪枝时不会被剪掉") {
        CHECK(
            fold_program_json(U"1; import a; 2") ==
            nlohmann::json{
                {"type", "Program"},
                {"exprs", nlohmann::json::array({import_node(nlohmann::json::array({"a"}))})}
            }
        );
    }

    TEST_CASE("调用形态的位置实参照常折叠") {
        const auto result = fold_json(U"import('ma' + 'th')");
        CHECK(result["type"] == "Call");
        CHECK(result["positional_args"] == nlohmann::json::array({str_lit("math")}));
    }

    TEST_CASE("调用形态的关键字实参照常折叠") {
        const auto result = fold_json(U"import('os', lazy=1 == 1, force=not True)");
        CHECK(result["keyword_args"][0]["value"] == bool_lit(true));
        CHECK(result["keyword_args"][1]["value"] == bool_lit(false));
    }

    TEST_CASE("调用形态整体不折：import 有副作用，编译期不可能求值") {
        CHECK(fold_json(U"import('math')")["type"] == "Call");
    }
}
