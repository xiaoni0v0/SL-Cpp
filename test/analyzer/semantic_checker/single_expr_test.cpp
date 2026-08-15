// SemanticChecker：单表达式入口（eval(code)，见 SL.md 的 eval 内置函数一节）的静态外层环境为空。
//
// 这里没有任何"给 eval 特设"的规则：外层没有 Program，`return` 就找不到归属（它的作用对象是离它
// 最近的 Program）；外层没有循环，`break`/`continue` 就无处可跳（它们只能用在 for/while 的 expr
// 部分）。两者都是已有规则在空外层环境下的自然结果。
// 反过来，code 自己写出来的函数体/类体/循环照常提供外层环境，里面的跳转合法——这组用例正是要把
// "禁的是顶层、不是全部"这条边界钉住。
#include "test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("SemanticChecker 单表达式入口") {

    TEST_CASE("普通表达式照常通过，跟在 Program 里没区别") {
        CHECK_NOTHROW(check_single_expr(U"x = 0"));
        CHECK_NOTHROW(check_single_expr(U"1 + 1"));
        CHECK_NOTHROW(check_single_expr(U"{ a = 1; a + 1 }"));
    }

    TEST_CASE("顶层 return 非法：外层一个 Program 都没有") {
        check_single_expr_throws_with(U"return", "return outside program");
        check_single_expr_throws_with(U"return 1", "return outside program");
    }

    TEST_CASE("复合表达式不是 Program，包一层花括号救不了 return") {
        check_single_expr_throws_with(U"{ return 1 }", "return outside program");
        check_single_expr_throws_with(U"{ a; return 1 }", "return outside program");
    }

    TEST_CASE("顶层 break/continue 非法：外层没有循环") {
        check_single_expr_throws_with(U"break", "break outside loop");
        check_single_expr_throws_with(U"continue", "continue outside loop");
        check_single_expr_throws_with(U"{ break }", "break outside loop");
    }

    TEST_CASE("if/try 都不提供外层环境，穿过它们仍然非法") {
        check_single_expr_throws_with(U"if (c) return 1", "return outside program");
        check_single_expr_throws_with(U"if (c) break", "break outside loop");
        check_single_expr_throws_with(U"try return 1 finally 0", "return outside program");
    }

    TEST_CASE("code 里自己写的函数体是 Program，其中的 return 合法") {
        CHECK_NOTHROW(check_single_expr(U"func f() { return 1 }"));
        CHECK_NOTHROW(check_single_expr(U"func () { return 1 }"));
        // 类体同样是 Program
        CHECK_NOTHROW(check_single_expr(U"class C { return 1 }"));
    }

    TEST_CASE("code 里自己写的循环提供外层环境，其中的 break/continue 合法") {
        CHECK_NOTHROW(check_single_expr(U"for (i : xs) { break }"));
        CHECK_NOTHROW(check_single_expr(U"while (c) { continue }"));
        CHECK_NOTHROW(check_single_expr(U"for (i = 0; i < 3; i += 1) { break }"));
    }

    TEST_CASE("函数体里的循环之外仍然不能 break——外层环境是逐层算的，不是一刀切") {
        check_single_expr_throws_with(U"func f() { break }", "break outside loop");
    }

    TEST_CASE("不要求外层构造的那些照常可用") {
        CHECK_NOTHROW(check_single_expr(U"raise E()"));
        CHECK_NOTHROW(check_single_expr(U"del x"));
        CHECK_NOTHROW(check_single_expr(U"import os"));
        CHECK_NOTHROW(check_single_expr(U"try a except (E) b"));
    }

    TEST_CASE("global 仍然要求身处局部作用域，单表达式入口不改变这条") {
        check_single_expr_throws_with(U"global x", "global outside function/class body");
        CHECK_NOTHROW(check_single_expr(U"func f() { global x }"));
    }

    TEST_CASE("走 Program 入口时，同样这些顶层写法照常合法（对照组）") {
        CHECK_NOTHROW(check_program(U"return 1"));
        CHECK_NOTHROW(check_program(U"{ return 1 }"));
    }
}
