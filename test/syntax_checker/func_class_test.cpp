// SyntaxChecker：func/class 的捕获/形参重名检查、形参顺序规则、doc 槽位校验。
#include "test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("SyntaxChecker 捕获/形参重名") {

TEST_CASE("func 捕获列表内部重名报错") {
    check_throws_with(U"func f[x, x]() {}", "duplicate name in capture/parameter list");
}

TEST_CASE("func 形参列表内部重名报错") {
    check_throws_with(U"func f(x, x) {}", "duplicate name in capture/parameter list");
}

TEST_CASE("func 捕获名和形参名跨列表重名同样报错") {
    check_throws_with(U"func f[x](x) {}", "duplicate name in capture/parameter list");
}

TEST_CASE("class 捕获列表内部重名报错（此前遗漏：class 的 captures_ 完全没被检查过）") {
    check_throws_with(U"class C[x, x] {}", "duplicate name in capture list");
}

TEST_CASE("class 捕获列表不重名则正常通过，且 value_expr_ 会被递归检查") {
    CHECK_NOTHROW(check_program(U"class C[x, y] {}"));
    // value_expr_ 里的非法内容（比如循环外的 break）应该被递归检查出来，而不是被 class 的
    // captures_ 检查跳过
    check_throws_with(U"class C[x = { break }] {}", "break outside loop");
}

}

TEST_SUITE("SyntaxChecker 形参顺序（SL.md 2.2.6）") {

TEST_CASE("*args 之后的普通形参是仅关键字形参，有没有默认值、彼此顺序都不受限制") {
    CHECK_NOTHROW(check_program(U"func f(*x, y) {}"));
    CHECK_NOTHROW(check_program(U"func f(*x, y = 1) {}"));
    CHECK_NOTHROW(check_program(U"func f(*x, y, z = 1) {}"));
    CHECK_NOTHROW(check_program(U"func f(*x, z = 1, y) {}")); // 有默认值的排在无默认值的前面也行
}

TEST_CASE("*args 之前的位置形参部分，无默认值的形参仍然必须排在有默认值的形参之前") {
    check_throws_with(U"func f(x, y = 1, z) {}", "non-default parameter after default parameter");
    check_throws_with(U"func f(x = 1, y) {}", "non-default parameter after default parameter");
}

TEST_CASE("**kwargs 之后不能再有任何形参") {
    check_throws_with(U"func f(**kw, x) {}", "parameter after **kwargs");
    check_throws_with(U"func f(**kw, *y) {}", "parameter after **kwargs");
    check_throws_with(U"func f(**kw, **kw2) {}", "parameter after **kwargs");
}

TEST_CASE("*args 至多一个") {
    check_throws_with(U"func f(*x, *y) {}", "duplicate *args");
}

TEST_CASE("正常的完整顺序：无默认值、有默认值/*args（可交错）、**kwargs") {
    CHECK_NOTHROW(check_program(U"func f(a, b = 1, *c, d, e = 2, **f) {}"));
}

}

TEST_SUITE("SyntaxChecker doc 槽位") {

TEST_CASE("doc 为普通字符串/原始字符串字面量都合法") {
    CHECK_NOTHROW(check_program(U"func f() 'plain doc' {}"));
    CHECK_NOTHROW(check_program(U"func f() `raw doc` {}"));
    CHECK_NOTHROW(check_program(U"class C 'plain doc' {}"));
}

TEST_CASE("doc 缺省合法") {
    CHECK_NOTHROW(check_program(U"func f() {}"));
    CHECK_NOTHROW(check_program(U"class C {}"));
}

TEST_CASE("doc 不允许拼接/重复等表达式，哪怕编译期可折叠") {
    check_throws_with(U"func f() 'a' + 'b' {}", "doc must be a string literal");
    check_throws_with(U"func f() 'a' * 3 {}", "doc must be a string literal");
    check_throws_with(U"class C 'a' + 'b' {}", "doc must be a string literal");
}

TEST_CASE("doc 不允许是变量或其他非字符串字面量表达式") {
    check_throws_with(U"func f() x {}", "doc must be a string literal");
    check_throws_with(U"func f() 5 {}", "doc must be a string literal");
    check_throws_with(U"class C x {}", "doc must be a string literal");
}

}
