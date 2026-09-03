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

    TEST_CASE("嵌套在外层循环里时，内层循环头部槽里的 break/continue 算外层循环体里的，合法") {
        // 内层 while 的 cond 槽在 loop_depth++ 之前检查，此时 loop_depth 是外层循环已经
        // 累加过的值——这个 break 落在外层循环的循环体（它的 expr 部分）里，跟直接写在
        // 外层循环体里的 break 是一回事，不是"内层循环头部"这条限制要拦的对象
        CHECK_NOTHROW(check_program(U"while (True) { while (break) 1 }"));
        CHECK_NOTHROW(check_program(U"while (True) { for (i = 0; break; i += 1) 1 }"));
        CHECK_NOTHROW(check_program(U"for (xs as i) { while (continue) 1 }"));
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

    TEST_CASE("if/for/while/{} 不引入作用域：局部作用域里，同样的位置反过来是合法的") {
        // 跟上一条正好对称：模块顶层的 while/for/if/{} 里 global 非法，是因为当前帧不是局部作用域，
        // 不是因为 while/for/if/{} 本身开了一层新作用域挡住了 global——同样的写法挪到 func/class
        // 体内部（局部作用域），应该照样合法
        CHECK_NOTHROW(check_program(U"func f() { while (True) { global x } }"));
        CHECK_NOTHROW(check_program(U"func f() { for (;;) { global x } }"));
        CHECK_NOTHROW(check_program(U"func f() { if (True) { global x } }"));
        CHECK_NOTHROW(check_program(U"func f() { { global x } }"));
        CHECK_NOTHROW(check_program(U"class C { for (;;) { global x } }"));
    }
}
