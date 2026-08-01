// SemanticChecker：import 表达式（SL.md 2.2.5/3.4.5）。
// 关键字形态没有子表达式、也没有位置限制——名字对不对、模块找不找得到全是运行期的事
// （ImportError）；调用形态的实参检查跟普通函数调用完全同一套规则（位置组允许 *、关键字组的
// ** 项允许 **）。
#include "test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("SemanticChecker import 关键字形态") {

    TEST_CASE("单段、多段都合法") {
        CHECK_NOTHROW(check_program(U"import math"));
        CHECK_NOTHROW(check_program(U"import os.path"));
        CHECK_NOTHROW(check_program(U"import a.b.c.d"));
    }

    TEST_CASE("哪儿都能 import，不像 global 那样要求身处局部作用域") {
        // 对照组：global 在顶层是错的
        CHECK_THROWS_AS(check_program(U"global x"), SyntaxError);

        CHECK_NOTHROW(check_program(U"import a"));
        CHECK_NOTHROW(check_program(U"func f() { import a }"));
        CHECK_NOTHROW(check_program(U"class C { import a }"));
        CHECK_NOTHROW(check_program(U"while (True) { import a }"));
        CHECK_NOTHROW(check_program(U"try (import a) finally (import b)"));
        CHECK_NOTHROW(check_program(U"if (x) (import a) else (import b)"));
    }

    TEST_CASE("import 得到的值可以直接参与后续表达式") {
        CHECK_NOTHROW(check_program(U"x = import a"));
        CHECK_NOTHROW(check_program(U"f(import a)"));
        CHECK_NOTHROW(check_program(U"(import a)[0]"));
    }
}

TEST_SUITE("SemanticChecker import 调用形态") {

    TEST_CASE("实参照普通函数调用检查") {
        CHECK_NOTHROW(check_program(U"import('math')"));
        CHECK_NOTHROW(check_program(U"import('os', lazy=True, force=False)"));
        CHECK_NOTHROW(check_program(U"import(name)"));
        CHECK_NOTHROW(check_program(U"import()"));
    }

    TEST_CASE("位置组允许 *，关键字组允许 **（跟普通函数调用同一套规则）") {
        CHECK_NOTHROW(check_program(U"import(*names)"));
        CHECK_NOTHROW(check_program(U"import(**options)"));
        CHECK_NOTHROW(check_program(U"import('os', *rest, **options)"));
    }

    TEST_CASE("关键字实参的值里不能裸写 *（can_star 在关键字组被关掉了）") {
        check_throws_with(
            U"import(lazy=*a)",
            "* can only appear in tuple, list, index, or function call arguments"
        );
    }

    TEST_CASE("位置实参里不能裸写 **（can_double_star 只对 ** 那一项自己开）") {
        check_throws_with(
            U"import(**a + b)", "** can only appear in dict literal or function call arguments"
        );
    }

    TEST_CASE("实参照常递归检查，里面的非法表达式一样会被抓出来") {
        // break 不在循环里
        check_throws_with(U"import(break)", "break outside loop");
        check_throws_with(U"import(lazy=break)", "break outside loop");
        check_throws_with(U"import(*break)", "break outside loop");
    }

    TEST_CASE("实参检查完，外层的 can_star/can_double_star 上下文要恢复原样") {
        // import(...) 整体身处一个不允许裸 * 的位置：检查完实参不该把 can_star 漏在开着的状态
        check_throws_with(
            U"x = import('a') + *b",
            "* can only appear in tuple, list, index, or function call arguments"
        );
        // 反过来：import(...) 身处允许 * 的位置（函数实参位置组），* 照样合法
        CHECK_NOTHROW(check_program(U"f(import('a'), *rest)"));
    }
}
