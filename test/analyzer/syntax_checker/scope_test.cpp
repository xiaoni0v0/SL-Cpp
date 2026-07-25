// SyntaxChecker：break/continue（loop_depth）、return/global（local_scope_depth）的作用域跟踪。
#include "test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("SyntaxChecker 作用域跟踪") {

    TEST_CASE("break/continue 在循环外一律报错") {
        check_throws_with(U"break", "break outside loop");
        check_throws_with(U"continue", "continue outside loop");
    }

    TEST_CASE("break/continue 在 for/while 内合法") {
        CHECK_NOTHROW(check_program(U"while (True) break"));
        CHECK_NOTHROW(check_program(U"while (True) continue"));
        CHECK_NOTHROW(check_program(U"for (i = 0; i < 10; i += 1) break"));
        CHECK_NOTHROW(check_program(U"for $ (x : y) continue"));
    }

    TEST_CASE("嵌套循环内的 break/continue 合法（内层循环就够）") {
        CHECK_NOTHROW(check_program(U"while (True) { while (True) { break } }"));
    }

    TEST_CASE("函数体把 loop_depth 归零：循环里定义的函数体内 break/continue 依然非法") {
        CHECK_THROWS_AS(check_program(U"while (True) { func f() { break } }"), SyntaxError);
        CHECK_THROWS_AS(check_program(U"while (True) { func f() { continue } }"), SyntaxError);
    }

    TEST_CASE("类体同样把 loop_depth 归零") {
        CHECK_THROWS_AS(check_program(U"while (True) { class C { break } }"), SyntaxError);
    }

    TEST_CASE("return 处处合法：顶层、函数体、类体") {
        CHECK_NOTHROW(check_program(U"return 5"));
        CHECK_NOTHROW(check_program(U"return"));
        CHECK_NOTHROW(check_program(U"func f() { return 5 }"));
        CHECK_NOTHROW(check_program(U"class C { return None }"));
        CHECK_NOTHROW(check_program(U"while (True) { return 5 }")); // 循环体里的 return 也合法
    }

    TEST_CASE("global 在真正的顶层（文件本身）非法") {
        check_throws_with(U"global x", "global outside function/class body");
    }

    TEST_CASE("global 在函数体、类体内合法") {
        CHECK_NOTHROW(check_program(U"func f() { global x }"));
        CHECK_NOTHROW(check_program(U"class C { global x }"));
    }

    TEST_CASE("global 在嵌套的函数体/类体内合法（只要身处任意一层局部作用域）") {
        CHECK_NOTHROW(check_program(U"func f() { func g() { global x } }"));
        CHECK_NOTHROW(check_program(U"class C { func m() { global x } }"));
        CHECK_NOTHROW(check_program(U"func f() { class C { global x } }"));
    }

    TEST_CASE("函数体内的顶层表达式仍是该函数自己的局部作用域，for/while body 本身不额外算一层") {
        // for/while 不引入新的 local_scope_depth，只有 func/class 才算；顶层 for 循环体里的 global
        // 非法
        CHECK_THROWS_AS(check_program(U"while (True) { global x }"), SyntaxError);
        CHECK_THROWS_AS(check_program(U"for (;;) { global x }"), SyntaxError);
    }
}

TEST_SUITE("SyntaxChecker del 目标合法性") {

    TEST_CASE("del 标识符合法") { CHECK_NOTHROW(check_program(U"del x")); }

    TEST_CASE("del 属性访问合法") {
        CHECK_NOTHROW(check_program(U"del x.y"));
        CHECK_NOTHROW(check_program(U"del x.y.z"));
    }

    TEST_CASE("del 元素访问不合法（目标只能是标识符或属性访问）") {
        check_throws_with(U"del x[0]", "del target must be an identifier or attribute access");
    }

    TEST_CASE("del 字面量不合法") {
        check_throws_with(U"del 1", "del target must be an identifier or attribute access");
    }

    TEST_CASE("del 调用表达式不合法") {
        check_throws_with(U"del f()", "del target must be an identifier or attribute access");
    }
}

TEST_SUITE("SyntaxChecker try-finally") {

    TEST_CASE("try 只有 finally 没有 except 合法") {
        CHECK_NOTHROW(check_program(U"try a finally b"));
    }

    TEST_CASE("try 有 except 也有 finally 合法") {
        CHECK_NOTHROW(check_program(U"try a except (E) b finally c"));
    }

    TEST_CASE("try 既无 except 也无 finally 报错") {
        check_throws_with(U"try a", "try must have at least one except or finally");
    }
}
