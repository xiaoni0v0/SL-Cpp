// 先 check 再 fold 的产物必须还能再过一遍 SemanticChecker。
#include "../../compiler/analyzer/semantic_checker/SemanticChecker.h"
#include "test_utils.h"

#include <doctest/doctest.h>

namespace {

// 按 Analyzer 的顺序跑一遍（check -> fold），再把折完的树 check 一遍，返回折叠后的 JSON。
// 折叠走 ExprFolder::fold_single_expr 而不是整份 Program 的入口，免得折成纯字面量之后被 prune 掉，
// 那样重新 check 的就是一棵空树，测了等于没测。
nlohmann::json fold_and_recheck(const std::u32string &source, const bool include_pos = false) {
    AstNodeProgramPtr program{parse_as_file(source)};
    SemanticChecker{*program, "<test>"}.check();
    if (program->exprs_.size() != 1) {
        throw std::runtime_error(
            "fold_and_recheck: expected exactly 1 top-level expr, got " +
            std::to_string(program->exprs_.size())
        );
    }
    ExprFolder::fold_single_expr(program->exprs_[0]);
    SemanticChecker{*program, "<test>"}.check(); // 折完了还得能过
    return nlohmann::json(AstJsonDumper::dump(*program->exprs_[0], include_pos));
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
        // 科学计数法的 int 按值参与折叠（折叠器内部把指数展开成普通数字串再走 int64_t 那条路），
        // `1e9 - 2e9` 会折成 -1000000000。折没折不重要，重要的是重新 check 照样过
        CHECK(fold_and_recheck(U"1e9 - 2e9") == int_lit("-1000000000"));
        CHECK(fold_and_recheck(U"-1e9") == int_lit("-1000000000"));
        // decimal 一律不折，一元负号原样留着 OpUnary
        CHECK(fold_and_recheck(U"-1.5e-3")["type"] == "OpUnary");
    }
}

TEST_SUITE("折叠产物重新 check 也能过——负 decimal") {

    // decimal 算术一律不折，一元负号原样留着 OpUnary，重新 check 也要过。
    TEST_CASE("一元负号作用在 decimal 上不折，但重新 check 照样过") {
        CHECK_NOTHROW(fold_and_recheck(U"-1.5"));
        CHECK_NOTHROW(fold_and_recheck(U"-0.0"));
        CHECK_NOTHROW(fold_and_recheck(U"-0.05"));
        CHECK(fold_and_recheck(U"-1.5")["type"] == "OpUnary");
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
        ExprFolder::fold_program(*program);
        CHECK_NOTHROW(SemanticChecker{*program, "<test>"}.check());
    }
}

TEST_SUITE("折叠产物重新 check 也能过——链式比较部分折叠") {
    // 部分折叠是折叠器里唯一一条"手动重建 ops_/operands_/positions_op_ 三个 vector"的路径，
    // 一旦下标写错导致三者长度不匹配，SemanticChecker::visit(AstNodeCompare) 的
    // require_same_size 会在重新 check 时抓到——这里既确认重新 check 能过，也确认剩下那个
    // 运算符的位置没有被丢换成别的运算符的位置。

    TEST_CASE("前两环确定为 True，剩下的链重新 check 也能过") {
        CHECK_NOTHROW(fold_and_recheck(U"1 < 2 < x"));
        CHECK_NOTHROW(fold_and_recheck(U"1 < 2 < 3 < x"));
    }

    TEST_CASE("部分折叠后 positions_op 是剩下那个运算符自己的位置，不是被丢掉的那个") {
        // "1 < 2 < 3 < x" -> 1(1) (2)<(3) (4)2(5) (6)<(7) (8)3(9) (10)<(11) (12)x(13)
        // 前两环 "1 < 2"、"2 < 3" 都确定为 True，剩下 "3 < x"，对应第三个 '<'（列 11）
        // 必须用 = 拷贝初始化，不能用 {}——见 .ai/notes/json-test-brace-init-trap.md
        const auto result = fold_and_recheck(U"1 < 2 < 3 < x", true);
        CHECK(result["type"] == "Compare");
        REQUIRE(result["positions_op"].size() == 1);
        CHECK(result["positions_op"][0]["col"] == 11);
        // 剩下的链自己的起始位置也该是 "3"（列 9），不是原链的起点 "1"
        CHECK(result["pos"]["col"] == 9);
    }
}
