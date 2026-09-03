// 捕获/形参重名、默认值顺序、doc、装饰器子树。
#include "test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("SemanticChecker 捕获/形参重名") {

    TEST_CASE("func 捕获列表内部重名报错") {
        check_throws_with(U"func f[x, x]() {}", "duplicate name in capture/parameter list");
    }

    TEST_CASE("func 形参列表内部重名报错") {
        check_throws_with(U"func f(x, x) {}", "duplicate name in capture/parameter list");
    }

    TEST_CASE("func 捕获名和形参名跨列表重名同样报错") {
        check_throws_with(U"func f[x](x) {}", "duplicate name in capture/parameter list");
    }

    TEST_CASE("class 捕获列表内部重名报错") {
        check_throws_with(U"class C[x, x] {}", "duplicate name in capture list");
    }

    TEST_CASE("class 捕获列表不重名则正常通过，且 value_expr_ 会被递归检查") {
        CHECK_NOTHROW(check_program(U"class C[x, y] {}"));
        // value_expr_ 里的非法内容（比如循环外的 break）应该被递归检查出来，而不是被 class 的
        // captures_ 检查跳过
        check_throws_with(U"class C[x = { break }] {}", "break outside loop");
    }

    TEST_CASE("func 捕获列表的 value_expr_ 同样会被递归检查") {
        CHECK_NOTHROW(check_program(U"func f[x, y]() {}"));
        check_throws_with(U"func f[x = { break }]() {}", "break outside loop");
    }
}

TEST_SUITE("SemanticChecker func/class 头部子树都会被递归检查") {
    // 这几个位置在结构上都是"通用表达式"（doc 走通用表达式的理由同样适用），容易在新增/改动
    // visit(AstNodeFunc)/visit(AstNodeClass) 时漏掉某一个子树的递归检查

    TEST_CASE("形参的类型注解、默认值都会被递归检查") {
        check_throws_with(U"func f(x: break) {}", "break outside loop");
        check_throws_with(U"func f(x = break) {}", "break outside loop");
    }

    TEST_CASE("返回类型会被递归检查") {
        check_throws_with(U"func f(): break {}", "break outside loop");
    }

    TEST_CASE("class 的基类列表会被递归检查") {
        check_throws_with(U"class C(break) {}", "break outside loop");
    }
}

TEST_SUITE("SemanticChecker 形参顺序") {
    // 至多一个 *args、**kwargs 必须最后：Parser 已保证。这里只测语义层的默认值顺序。

    TEST_CASE("*args 之后的普通形参是仅关键字形参，有没有默认值、彼此顺序都不受限制") {
        CHECK_NOTHROW(check_program(U"func f(*x, y) {}"));
        CHECK_NOTHROW(check_program(U"func f(*x, y = 1) {}"));
        CHECK_NOTHROW(check_program(U"func f(*x, y, z = 1) {}"));
        CHECK_NOTHROW(
            check_program(U"func f(*x, z = 1, y) {}")
        ); // 有默认值的排在无默认值的前面也行
    }

    TEST_CASE("*args 之前的位置形参部分，无默认值的形参仍然必须排在有默认值的形参之前") {
        check_throws_with(
            U"func f(x, y = 1, z) {}", "non-default parameter after default parameter"
        );
        check_throws_with(U"func f(x = 1, y) {}", "non-default parameter after default parameter");
    }

    TEST_CASE("正常的完整顺序：无默认值、有默认值/*args（可交错）、**kwargs") {
        CHECK_NOTHROW(check_program(U"func f(a, b = 1, *c, d, e = 2, **f) {}"));
    }

    TEST_CASE("重名检查覆盖 *args/**kwargs 自己的名字，不只是 params 里的普通形参") {
        check_throws_with(U"func f(x, *x) {}", "duplicate name in capture/parameter list");
        check_throws_with(U"func f(x, **x) {}", "duplicate name in capture/parameter list");
        check_throws_with(U"func f(*x, x) {}", "duplicate name in capture/parameter list");
        check_throws_with(U"func f(*x, **x) {}", "duplicate name in capture/parameter list");
    }
}

TEST_SUITE("SemanticChecker doc 槽位") {

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

TEST_SUITE("SemanticChecker 装饰器") {

    TEST_CASE("紧邻 func/class 的装饰器合法，含多个、含调用形式") {
        CHECK_NOTHROW(check_program(U"@dec func f() {}"));
        CHECK_NOTHROW(check_program(U"@dec1 @dec2 func f() {}"));
        CHECK_NOTHROW(check_program(U"@dec class C {}"));
        CHECK_NOTHROW(check_program(U"@dec(1, 2) func f() {}"));
    }

    TEST_CASE("紧邻 func/class 的装饰器表达式子树仍会被递归检查") {
        check_throws_with(U"@break func f() {}", "break outside loop");
        check_throws_with(U"@dec(break) func f() {}", "break outside loop");
    }

    TEST_CASE("通用形式（包裹的不是紧邻的 func/class）合法，含多层嵌套") {
        CHECK_NOTHROW(check_program(U"@dec x"));
        CHECK_NOTHROW(check_program(U"@d1 @d2 x"));
        CHECK_NOTHROW(check_program(U"@dec x = 1"));
    }

    TEST_CASE("通用形式的 decorator_/target_ 子树都会被递归检查") {
        check_throws_with(U"@break x", "break outside loop");
        check_throws_with(U"@dec break", "break outside loop");
    }
}
