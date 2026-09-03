// Analyzer 门面（analyze_program/analyze_single_expr）本身：先 check 后 fold 的顺序、
// in_local_scope 的转发，都是只有走这两个入口才测得到的行为——单独调 SemanticChecker/ExprFolder
// 测不出"顺序反了"或"参数没转发"这类回归。
#include "../../analyzer/Analyzer.h"
#include "../../builtins/exceptions/SyntaxError.h"
#include "test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("Analyzer 门面——check 必须先于 fold") {

    // 反例见 .ai/architecture.md：check 先跑时 *args 作为 and 的右操作数在 can_star=false 语境下
    // 被正确拦下；若 fold 先跑，死分支消除会把 *args 从 and 里挪出来顶到调用参数位置
    // （can_star=true 的合法语境），这段本该永远非法的写法会被折叠悄悄变合法
    TEST_CASE("f(True and *args)：*args 不会被死分支消除救成合法调用参数") {
        AstNodeProgramPtr program{parse_as_file(U"f(True and *args)")};
        CHECK_THROWS_AS(Analyzer::analyze_program(*program, "<test>"), SyntaxError);
    }

    // 反例二：func 的 doc 位置要求"已经是字符串字面量"，check_doc 只认 AstNodeLiteralStr。
    // 若 fold 先跑，'a' + 'b' 会先被折成字面量 "ab"，check_doc 再看就会误判成合法
    TEST_CASE("func f() 'a' + 'b' {}：doc 不会被折叠成字面量之后才检查") {
        AstNodeProgramPtr program{parse_as_file(U"func f() 'a' + 'b' {}")};
        CHECK_THROWS_AS(Analyzer::analyze_program(*program, "<test>"), SyntaxError);
    }

    TEST_CASE("单表达式入口同样是先 check 后 fold") {
        AstNodePtr expr1{parse_as_single_expr(U"f(True and *args)")};
        CHECK_THROWS_AS(Analyzer::analyze_single_expr(expr1, "<test>"), SyntaxError);

        AstNodePtr expr2{parse_as_single_expr(U"func f() 'a' + 'b' {}")};
        CHECK_THROWS_AS(Analyzer::analyze_single_expr(expr2, "<test>"), SyntaxError);
    }
}

TEST_SUITE("Analyzer 门面——正常输入两步都真的跑了") {

    TEST_CASE("analyze_program：check 通过后，折叠确实落到了树上") {
        AstNodeProgramPtr program{parse_as_file(U"x = 1 + 1")};
        CHECK_NOTHROW(Analyzer::analyze_program(*program, "<test>"));
        REQUIRE(program->exprs_.size() == 1);
        const auto *assign{dynamic_cast<AstNodeAssign *>(program->exprs_[0].get())};
        REQUIRE(assign != nullptr);
        CHECK(dynamic_cast<AstNodeLiteralInt *>(assign->value_.get()) != nullptr);
    }

    TEST_CASE("analyze_single_expr：check 通过后，折叠确实落到了树上") {
        AstNodePtr expr{parse_as_single_expr(U"1 + 1")};
        CHECK_NOTHROW(Analyzer::analyze_single_expr(expr, "<test>"));
        CHECK(dynamic_cast<AstNodeLiteralInt *>(expr.get()) != nullptr);
    }
}

TEST_SUITE("Analyzer 门面——analyze_single_expr 的 in_local_scope 确实被转发给了 SemanticChecker") {

    TEST_CASE("in_local_scope 默认 false：global 在模块顶层非法") {
        AstNodePtr expr{parse_as_single_expr(U"global x")};
        CHECK_THROWS_AS(Analyzer::analyze_single_expr(expr, "<test>"), SyntaxError);
    }

    TEST_CASE("in_local_scope 传 true：global 在局部作用域里合法") {
        AstNodePtr expr{parse_as_single_expr(U"global x")};
        CHECK_NOTHROW(Analyzer::analyze_single_expr(expr, "<test>", true));
    }
}
