// try 必须带 except 或 finally；finally 里不能 return/break/continue 跳出这段求值。
#include "test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("SemanticChecker try / finally") {

    TEST_CASE("except 和 finally 至少要有一个") {
        CHECK_NOTHROW(check_program(U"try a finally b"));
        CHECK_NOTHROW(check_program(U"try a except (E) b"));
        CHECK_NOTHROW(check_program(U"try a except (E) b finally c"));
        check_throws_with(U"try a", "try must have at least one except or finally");
    }

    TEST_CASE("finally 顶层禁止 return / break / continue") {
        check_throws_with(U"try a finally return 1", "return inside finally is not allowed");
        check_throws_with(
            U"try a except (E) b finally return 1", "return inside finally is not allowed"
        );
        check_throws_with(
            U"while (True) { try a finally break }", "break inside finally is not allowed"
        );
        check_throws_with(
            U"while (True) { try a finally continue }", "continue inside finally is not allowed"
        );
        check_throws_with(
            U"while (True) { while (True) { try a finally break } }",
            "break inside finally is not allowed"
        );
        check_throws_with(
            U"for (xs as x) { try a finally break }", "break inside finally is not allowed"
        );
    }

    TEST_CASE("finally 内部新开的循环 / 函数 / 类不受影响") {
        CHECK_NOTHROW(check_program(U"try a finally { for (;;) { break } }"));
        CHECK_NOTHROW(check_program(U"try a finally { while (True) { continue } }"));
        CHECK_NOTHROW(check_program(U"try a finally { func f() { return 1 } }"));
        CHECK_NOTHROW(check_program(U"try a finally { class C { return None } }"));
        CHECK_NOTHROW(check_program(U"try a finally { for (xs as x) { break } }"));
    }

    TEST_CASE("复合表达式不引入新作用域，里面的 return 仍拦截") {
        check_throws_with(
            U"try a finally { { return 1 } }", "return inside finally is not allowed"
        );
    }

    TEST_CASE("同一个 try 的 except 不受自己的 finally 拦截") {
        CHECK_NOTHROW(check_program(U"try a except (E) { return 1 } finally c"));
        CHECK_NOTHROW(check_program(U"while (True) { try a except (E) { break } finally c }"));
    }

    TEST_CASE("外层 finally 里的内层 try/except 仍算在 finally 范围内") {
        check_throws_with(
            U"try a finally { try b except (E) { return 1 } }",
            "return inside finally is not allowed"
        );
        check_throws_with(
            U"try a finally { try { return 1 } except (E) b }",
            "return inside finally is not allowed"
        );
        check_throws_with(
            U"try a finally { try b except (E) c finally { return 1 } }",
            "return inside finally is not allowed"
        );
    }

    TEST_CASE("finally 里 raise 合法") {
        CHECK_NOTHROW(check_program(U"try a finally { raise E() }"));
    }
}
