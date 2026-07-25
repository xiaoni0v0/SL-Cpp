// SyntaxChecker：赋值/复合赋值目标的左值检查，含解构、以及左值根节点内部子表达式仍需完整 check()。
#include "test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("SyntaxChecker 左值检查") {

    TEST_CASE("标识符/索引/属性访问都是合法的赋值目标") {
        CHECK_NOTHROW(check_program(U"a = 1"));
        CHECK_NOTHROW(check_program(U"a[0] = 1"));
        CHECK_NOTHROW(check_program(U"a.b = 1"));
    }

    TEST_CASE("元组/列表解构是合法的赋值目标") {
        CHECK_NOTHROW(check_program(U"(a, b) = x"));
        CHECK_NOTHROW(check_program(U"[a, b] = x"));
        CHECK_NOTHROW(check_program(U"(a, *b) = x"));
        CHECK_NOTHROW(check_program(U"[*a, b] = x"));
    }

    TEST_CASE("解构里嵌套解构也合法") { CHECK_NOTHROW(check_program(U"(a, (b, c)) = x")); }

    TEST_CASE("字面量/调用等不是合法的赋值目标") {
        check_throws_with(U"1 = x", "lvalue expected before assignment");
        check_throws_with(U"f() = x", "lvalue expected before assignment");
        check_throws_with(U"(1, 2) = x", "lvalue expected before assignment"); // 元组里含非左值元素
    }

    TEST_CASE("解构里至多一个 * 前缀") {
        check_throws_with(U"(*a, *b) = x", "at most one starred lvalue allowed in destructuring");
        check_throws_with(U"[*a, *b] = x", "at most one starred lvalue allowed in destructuring");
    }

    TEST_CASE("复合赋值只允许简单左值（标识符/索引/属性），不允许解构") {
        CHECK_NOTHROW(check_program(U"a += 1"));
        CHECK_NOTHROW(check_program(U"a[0] += 1"));
        CHECK_NOTHROW(check_program(U"a.b += 1"));
        check_throws_with(
            U"(a, b) += x", "identifier, attribute access, or index expression expected before op="
        );
    }

    TEST_CASE(
        "左值根节点内部的子表达式仍然要完整 check()——之前 check_lvalue 对 Index/Attr 直接"
        "返回、完全跳过了 object_/args_，导致里面的问题被忽略"
    ) {
        // a[break] = 1：break 在这里没有被任何循环包着，之前会被 check_lvalue
        // 完全跳过检查，现在要报错
        check_throws_with(U"a[break] = 1", "break outside loop");
        check_throws_with(U"a[break] += 1", "break outside loop");
        // 复合赋值同理，target_ 是 Attr 时 object_ 的子表达式也要被检查
        check_throws_with(U"(break).b = 1", "break outside loop");
    }

    TEST_CASE("赋值/复合赋值的右侧 value_ 正常参与检查") {
        check_throws_with(U"a = break", "break outside loop");
        check_throws_with(U"a += break", "break outside loop");
    }
}
