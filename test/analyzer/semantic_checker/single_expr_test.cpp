// eval 入口的静态外层环境为空。
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
        CHECK_NOTHROW(check_single_expr(U"for (xs as i) { break }"));
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

    TEST_CASE("global 是否合法取决于调用帧，不是单表达式入口本身说了算") {
        // 默认 in_local_scope=false，模拟顶层/模块帧调用 eval：跟文件顶层 global 一样非法
        check_single_expr_throws_with(U"global x", "global outside function/class body");
        // in_local_scope=true，模拟从函数/类体内部调用 eval（SL.md 3.4.10：eval 的 code
        // 在调用帧求值，global 判的是那一帧，不是 code 的 AST 里有没有包一层 func/class）
        CHECK_NOTHROW(check_single_expr(U"global x", true));
        // code 里自己写的函数体建立了新的局部作用域，跟外层调用帧是不是局部作用域无关，
        // 两种 in_local_scope 传参下都合法
        CHECK_NOTHROW(check_single_expr(U"func f() { global x }"));
        CHECK_NOTHROW(check_single_expr(U"func f() { global x }", true));
    }

    TEST_CASE("走 Program 入口时，同样这些顶层写法照常合法（对照组）") {
        CHECK_NOTHROW(check_program(U"return 1"));
        CHECK_NOTHROW(check_program(U"{ return 1 }"));
    }
}
