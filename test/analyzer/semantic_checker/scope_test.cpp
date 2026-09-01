// break/continue/return/global 的作用域。
#include "test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("SemanticChecker 作用域") {

    TEST_CASE("break/continue 只能在循环体里") {
        check_throws_with(U"break", "break outside loop");
        check_throws_with(U"continue", "continue outside loop");
        CHECK_NOTHROW(check_program(U"while (True) break"));
        CHECK_NOTHROW(check_program(U"while (True) continue"));
        CHECK_NOTHROW(check_program(U"for (i = 0; i < 10; i += 1) break"));
        CHECK_NOTHROW(check_program(U"for $ (y as x) continue"));
        CHECK_NOTHROW(check_program(U"while (True) { while (True) { break } }"));
    }

    TEST_CASE("收集模式不影响 break/continue 合法性") {
        for (const std::u32string mark : {U"", U"$", U"$ *", U"$$", U"$$ **"}) {
            CHECK_NOTHROW(check_program(U"for " + mark + U" (xs as i) break"));
            CHECK_NOTHROW(check_program(U"while " + mark + U" (True) continue"));
            CHECK_NOTHROW(check_program(U"for " + mark + U" (xs as i) i"));
        }
    }

    TEST_CASE("func/class 体把 loop_depth 归零") {
        check_throws_with(U"while (True) { func f() { break } }", "break outside loop");
        check_throws_with(U"while (True) { func f() { continue } }", "continue outside loop");
        check_throws_with(U"while (True) { class C { break } }", "break outside loop");
    }

    TEST_CASE("break/continue 不能出现在循环头部的槽里，只能在循环体里") {
        // cond/init/inc/iterable 各槽都在 loop_depth++ 之前检查，出现在这些槽里跟压根不在
        // 循环里是一回事
        check_throws_with(U"while (break) 1", "break outside loop");
        check_throws_with(U"while (continue) 1", "continue outside loop");
        check_throws_with(U"for (break; True; i += 1) 1", "break outside loop");
        check_throws_with(U"for (i = 0; break; i += 1) 1", "break outside loop");
        check_throws_with(U"for (i = 0; True; break) 1", "break outside loop");
        check_throws_with(U"for (break as i) 1", "break outside loop");
    }

    TEST_CASE("return 在 Program 里都合法") {
        CHECK_NOTHROW(check_program(U"return 5"));
        CHECK_NOTHROW(check_program(U"return"));
        CHECK_NOTHROW(check_program(U"func f() { return 5 }"));
        CHECK_NOTHROW(check_program(U"class C { return None }"));
        CHECK_NOTHROW(check_program(U"while (True) { return 5 }"));
    }

    TEST_CASE("global 只能在 func/class 体") {
        check_throws_with(U"global x", "global outside function/class body");
        CHECK_NOTHROW(check_program(U"func f() { global x }"));
        CHECK_NOTHROW(check_program(U"class C { global x }"));
        CHECK_NOTHROW(check_program(U"func f() { func g() { global x } }"));
        CHECK_NOTHROW(check_program(U"class C { func m() { global x } }"));
        CHECK_NOTHROW(check_program(U"func f() { class C { global x } }"));
        CHECK_THROWS_AS(check_program(U"while (True) { global x }"), SyntaxError);
        CHECK_THROWS_AS(check_program(U"for (;;) { global x }"), SyntaxError);
    }
}
