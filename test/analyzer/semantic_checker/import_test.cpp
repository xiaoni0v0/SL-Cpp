// SemanticChecker：import 表达式（SL.md 2.2.5/3.4.5）。
// 关键字形态没有子表达式、也没有位置限制——名字对不对、模块找不找得到全是运行期的事
// （ImportError）；调用形态本来就是一次普通函数调用，走的是 AstNodeCall 那套检查。
#include "test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("SemanticChecker import") {

    TEST_CASE("关键字形态：单段、多段都合法") {
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
    }

    TEST_CASE("调用形态：实参照普通函数调用检查") {
        CHECK_NOTHROW(check_program(U"import('math')"));
        CHECK_NOTHROW(check_program(U"import('os', lazy=True, force=False)"));
        CHECK_NOTHROW(check_program(U"import(name)"));
        CHECK_NOTHROW(check_program(U"import(*names)"));
        CHECK_NOTHROW(check_program(U"import(**options)"));
    }

    TEST_CASE("调用形态的实参也照常递归检查，里面的非法表达式一样会被抓出来") {
        // break 不在循环里
        check_throws_with(U"import(break)", "break outside loop");
        check_throws_with(U"import(lazy=break)", "break outside loop");
    }
}
