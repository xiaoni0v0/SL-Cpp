// 字典 **/k:v，调用与索引里的 * / **。
#include "test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("SemanticChecker 字典字面量") {

    TEST_CASE("普通 k: v 项、** 展开项各种组合都合法") {
        CHECK_NOTHROW(check_program(U"{k: v}"));
        CHECK_NOTHROW(check_program(U"{**d}"));
        CHECK_NOTHROW(check_program(U"{k: v, **d}"));
        CHECK_NOTHROW(check_program(U"{**d, k: v}"));
        CHECK_NOTHROW(check_program(U"{**d1, k: v, **d2}"));
    }

    TEST_CASE("dict 的 key/val 子表达式仍然会被递归检查") {
        check_throws_with(U"{break: 1}", "break outside loop");
        check_throws_with(U"{1: break}", "break outside loop");
        check_throws_with(U"{**(break)}", "break outside loop");
    }
}

TEST_SUITE("SemanticChecker 调用参数") {

    TEST_CASE("普通位置参数、关键字参数、*/** 展开都合法") {
        CHECK_NOTHROW(check_program(U"f(1, 2)"));
        CHECK_NOTHROW(check_program(U"f(a=1, b=2)"));
        CHECK_NOTHROW(check_program(U"f(*a)"));
        CHECK_NOTHROW(check_program(U"f(**a)"));
        CHECK_NOTHROW(check_program(U"f(1, *a, **b)"));
    }

    // 位置组不能出现在关键字组之后：Parser 直接报语法错误，见 precedence_test.cpp。

    TEST_CASE("调用的 object_/各参数子表达式都会被递归检查") {
        check_throws_with(U"(break)()", "break outside loop");
        check_throws_with(U"f(break)", "break outside loop");
        check_throws_with(U"f(a=break)", "break outside loop");
        check_throws_with(U"f(*break)", "break outside loop");
        check_throws_with(U"f(**break)", "break outside loop");
    }

    TEST_CASE(
        "关键字实参的值本身不能再带 */** 前缀（跟普通表达式位置一致，*/** 只能出现在"
        "位置组/关键字组自己的展开语法上，不能是某个关键字的值）"
    ) {
        check_throws_with(
            U"f(a=*b)", "* can only appear in tuple, list, index, or function call arguments"
        );
        check_throws_with(
            U"f(a=**b)", "** can only appear in dict literal or function call arguments"
        );
    }

    TEST_CASE("index/attr 的子表达式在普通表达式位置上本来就会被递归检查（不只是当左值时）") {
        check_throws_with(U"a[break]", "break outside loop");
        check_throws_with(U"(break).b", "break outside loop");
    }

    TEST_CASE("索引参数里 * 合法（跟位置组一样，用于展开），** 不合法（索引没有关键字组）") {
        CHECK_NOTHROW(check_program(U"a[*b]"));
        CHECK_NOTHROW(check_program(U"a[1, *b]"));
        check_throws_with(
            U"a[**b]", "** can only appear in dict literal or function call arguments"
        );
    }
}

TEST_SUITE("SemanticChecker * / ** 出现位置") {

    TEST_CASE("顶层、算术、复合表达式里的 * 非法") {
        check_throws_with(
            U"*a", "* can only appear in tuple, list, index, or function call arguments"
        );
        check_throws_with(
            U"*a + 1", "* can only appear in tuple, list, index, or function call arguments"
        );
        check_throws_with(
            U"{*a}", "* can only appear in tuple, list, index, or function call arguments"
        );
        check_throws_with(
            U"{1; *a}", "* can only appear in tuple, list, index, or function call arguments"
        );
    }

    TEST_CASE("元组/列表里的 **、字典值侧的 * 非法") {
        check_throws_with(
            U"(**a,)", "** can only appear in dict literal or function call arguments"
        );
        check_throws_with(
            U"[**a]", "** can only appear in dict literal or function call arguments"
        );
        check_throws_with(
            U"{k: *v}", "* can only appear in tuple, list, index, or function call arguments"
        );
        check_throws_with(U"**a", "** can only appear in dict literal or function call arguments");
    }
}
