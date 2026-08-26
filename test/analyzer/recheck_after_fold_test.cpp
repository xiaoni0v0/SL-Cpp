// 不变量：折叠产物必须还能过一遍 SemanticChecker。
//
// Analyzer 的顺序是先 check 后 fold，只 check 这一次；ExprFolder 之后造出来的节点没有任何人再看
// 一眼。所以"折完的树重新 check 也能过"是这个模块的自洽性底线：check 描述的是"一棵合法 AST 长什么
// 样"，不是"Parser 刚吐出来的树长什么样"。
//
// 最典型的例子是负数：源码里 `-1` 是一元负号加上 `1`（SL.md 说负数不是字面量，那是文法层面的话），
// 折完之后变成一个 raw_ 为 "-1" 的 int 字面量节点——AST 层面它就是一个合法的字面量。
#include "../../analyzer/semantic_checker/SemanticChecker.h"
#include "test_utils.h"

#include <doctest/doctest.h>

namespace {

// 按 Analyzer 的顺序跑一遍（check -> fold），再把折完的树 check 一遍，返回折叠后的 JSON。
// 折叠走 ExprFolder::fold_expr 而不是整份 Program 的入口，免得折成纯字面量之后被 prune 掉，
// 那样重新 check 的就是一棵空树，测了等于没测。
nlohmann::json fold_and_recheck(const std::u32string &source) {
    AstNodeProgramPtr program{parse_as_file(source)};
    SemanticChecker{*program, "<test>"}.check();
    if (program->exprs_.size() != 1) {
        throw std::runtime_error(
            "fold_and_recheck: expected exactly 1 top-level expr, got " +
            std::to_string(program->exprs_.size())
        );
    }
    ExprFolder::fold_expr(program->exprs_[0]);
    SemanticChecker{*program, "<test>"}.check(); // 折完了还得能过
    return nlohmann::json(program->exprs_[0]->to_json());
}

} // namespace

TEST_SUITE("折叠产物重新 check 也能过——负 int") {

    TEST_CASE("一元负号") {
        CHECK(fold_and_recheck(U"-1") == int_lit("-1"));
        CHECK(fold_and_recheck(U"-True") == int_lit("-1"));
        CHECK(fold_and_recheck(U"-0") == int_lit("0")); // 负零折回 0，不是 "-0"
    }

    TEST_CASE("减法/取模/位运算折出负数") {
        CHECK(fold_and_recheck(U"2 - 3") == int_lit("-1"));
        CHECK(fold_and_recheck(U"-7 % -2") == int_lit("-1"));
        CHECK(fold_and_recheck(U"~0") == int_lit("-1"));
        CHECK(fold_and_recheck(U"0 - 1234567890") == int_lit("-1234567890"));
    }

    TEST_CASE("贴着 int64_t 下界") {
        CHECK(fold_and_recheck(U"-9223372036854775807 - 1") == int_lit("-9223372036854775808"));
    }

    TEST_CASE("折不动的也得能重新 check——科学计数法字面量原样留在树上") {
        // StaticEvaler 目前读不了科学计数法（node_to_int64 走 from_chars，遇到 e 就停），
        // 于是 `1e9 - 2e9` 整个不折。折没折不重要，重要的是重新 check 照样过
        CHECK_NOTHROW(fold_and_recheck(U"1e9 - 2e9"));
        CHECK_NOTHROW(fold_and_recheck(U"-1e9"));
        CHECK_NOTHROW(fold_and_recheck(U"-1.5e-3"));
    }
}

TEST_SUITE("折叠产物重新 check 也能过——负 decimal") {

    TEST_CASE("一元负号") {
        CHECK(fold_and_recheck(U"-1.5") == decimal_lit("-1.5"));
        CHECK(fold_and_recheck(U"-0.0") == decimal_lit("-0.0")); // 负零保留符号
        CHECK(fold_and_recheck(U"-0.05") == decimal_lit("-0.05"));
    }
}

TEST_SUITE("折叠产物重新 check 也能过——嵌在别的结构里") {

    TEST_CASE("负数作为子表达式") {
        CHECK_NOTHROW(fold_and_recheck(U"x = 2 - 3"));
        CHECK_NOTHROW(fold_and_recheck(U"[2 - 3, -1.5]"));
        CHECK_NOTHROW(fold_and_recheck(U"f(2 - 3, k = -1.5)"));
        CHECK_NOTHROW(fold_and_recheck(U"{'k': 2 - 3}"));
    }

    TEST_CASE("整份 Program 走 Analyzer 的正式入口，再整棵重新 check") {
        AstNodeProgramPtr program{parse_as_file(U"x = 2 - 3\ny = -1.5\nfunc f() { return -x }")};
        SemanticChecker{*program, "<test>"}.check();
        ExprFolder::fold(*program);
        CHECK_NOTHROW(SemanticChecker{*program, "<test>"}.check());
    }
}
