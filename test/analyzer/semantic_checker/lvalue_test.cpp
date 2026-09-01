// 赋值、for as、except as 的左值形状。
#include "test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("SemanticChecker 左值检查") {

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

    TEST_CASE("零个左值的解构（() = x / [] = x）语法上合法，跟 Python 一致") {
        // 要求右边解构出恰好 0 个元素，是运行期的事（元素个数不匹配同一类错误），
        // 不在这一层检查
        CHECK_NOTHROW(check_program(U"() = x"));
        CHECK_NOTHROW(check_program(U"[] = x"));
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

    TEST_CASE("* 后面仍是左值，可以是嵌套的 tuple/list 解构") {
        CHECK_NOTHROW(check_program(U"(a, *(b, c)) = x"));
        CHECK_NOTHROW(check_program(U"(a, *[b, c]) = x"));
        // * 后面是纯左值（标识符/索引/属性）也都合法
        CHECK_NOTHROW(check_program(U"(a, *b) = x"));
        CHECK_NOTHROW(check_program(U"(a, *b[0]) = x"));
        CHECK_NOTHROW(check_program(U"(a, *b.c) = x"));
        // * 后面不是左值（比如字面量）仍然非法，走的是 check_lvalue 通用的报错
        check_throws_with(U"(a, *(1, 2)) = x", "lvalue expected before assignment");
    }

    TEST_CASE("“每一层至多一个 *”按层独立算，嵌套解构内部可以各自再带一个 *") {
        CHECK_NOTHROW(check_program(U"[a, *[b, *c]] = x"));
        check_throws_with(
            U"[*[*a, *b]] = x", "at most one starred lvalue allowed in destructuring"
        );
    }

    TEST_CASE("复合赋值只允许简单左值（标识符/索引/属性），不允许解构") {
        CHECK_NOTHROW(check_program(U"a += 1"));
        CHECK_NOTHROW(check_program(U"a[0] += 1"));
        CHECK_NOTHROW(check_program(U"a.b += 1"));
        check_throws_with(
            U"(a, b) += x", "identifier, attribute access, or index expression expected before op="
        );
    }

    TEST_CASE("左值内部的子表达式仍要完整 check()") {
        check_throws_with(U"a[break] = 1", "break outside loop");
        check_throws_with(U"a[break] += 1", "break outside loop");
        check_throws_with(U"(break).b = 1", "break outside loop");
    }

    TEST_CASE("赋值/复合赋值的右侧 value_ 正常参与检查") {
        check_throws_with(U"a = break", "break outside loop");
        check_throws_with(U"a += break", "break outside loop");
    }
}

TEST_SUITE("SemanticChecker for 迭代目标左值检查") {

    TEST_CASE("标识符/索引/属性访问都是合法的迭代目标") {
        CHECK_NOTHROW(check_program(U"for (xs as x) body"));
        CHECK_NOTHROW(check_program(U"for (xs as a[0]) body"));
        CHECK_NOTHROW(check_program(U"for (xs as a.b) body"));
    }

    TEST_CASE("元组/列表解构是合法的迭代目标") {
        CHECK_NOTHROW(check_program(U"for (xs as (a, b)) body"));
        CHECK_NOTHROW(check_program(U"for (xs as [a, *b]) body"));
    }

    TEST_CASE("字面量不是合法的迭代目标") {
        check_throws_with(U"for (xs as 1) body", "lvalue expected before assignment");
    }

    TEST_CASE("调用表达式不是合法的迭代目标") {
        check_throws_with(U"for (xs as f()) body", "lvalue expected before assignment");
    }

    TEST_CASE("不写 as 就没有目标可查，任何可迭代表达式都放行") {
        CHECK_NOTHROW(check_program(U"for (xs) body"));
        CHECK_NOTHROW(check_program(U"for (f()) body"));
        CHECK_NOTHROW(check_program(U"for (1) body")); // 能不能真迭代是运行期的事
    }

    TEST_CASE("iterable 自身的子表达式照常参与检查") {
        check_throws_with(U"for (break) body", "break outside loop");
        check_throws_with(U"for (break as x) body", "break outside loop");
    }
}

TEST_SUITE("SemanticChecker except 绑定目标左值检查") {

    TEST_CASE("跟 for 的迭代目标同一套规则：标识符/索引/属性/解构都合法") {
        CHECK_NOTHROW(check_program(U"try a except (E as e) b"));
        CHECK_NOTHROW(check_program(U"try a except (E as x[0]) b"));
        CHECK_NOTHROW(check_program(U"try a except (E as x.y) b"));
        CHECK_NOTHROW(check_program(U"try a except (E as (p, q)) b"));
    }

    TEST_CASE("非左值的绑定目标报错") {
        check_throws_with(U"try a except (E as 1) b", "lvalue expected before assignment");
        check_throws_with(U"try a except (E as f()) b", "lvalue expected before assignment");
    }

    TEST_CASE("不写 as 就没有目标可查") { CHECK_NOTHROW(check_program(U"try a except (E) b")); }

    TEST_CASE("绑定目标内部的子表达式仍然要完整 check()") {
        check_throws_with(U"try a except (E as x[break]) b", "break outside loop");
    }
}
