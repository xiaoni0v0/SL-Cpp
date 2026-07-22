// SyntaxChecker：字典字面量的 **/k:v 检查，调用参数里 */** 的展开顺序、can_star 上下文传递。
#include "test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("SyntaxChecker 字典字面量") {

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

TEST_SUITE("SyntaxChecker 调用参数") {

TEST_CASE("普通位置参数、关键字参数、*/** 展开都合法") {
    CHECK_NOTHROW(check_program(U"f(1, 2)"));
    CHECK_NOTHROW(check_program(U"f(a=1, b=2)"));
    CHECK_NOTHROW(check_program(U"f(*a)"));
    CHECK_NOTHROW(check_program(U"f(**a)"));
    CHECK_NOTHROW(check_program(U"f(1, *a, **b)"));
}

TEST_CASE("** 展开之后不能再有别的位置参数") {
    check_throws_with(U"f(**a, 1)", "argument after ** spread");
    check_throws_with(U"f(*a, **b, c)", "argument after ** spread");
}

TEST_CASE("调用的 object_/各参数子表达式都会被递归检查") {
    check_throws_with(U"(break)()", "break outside loop");
    check_throws_with(U"f(break)", "break outside loop");
    check_throws_with(U"f(a=break)", "break outside loop");
    check_throws_with(U"f(*break)", "break outside loop");
}

TEST_CASE("index/attr 的子表达式在普通表达式位置上本来就会被递归检查（不只是当左值时）") {
    check_throws_with(U"a[break]", "break outside loop");
    check_throws_with(U"(break).b", "break outside loop");
}

}
