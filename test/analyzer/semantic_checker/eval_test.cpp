// eval 调用形态的语义约束：跟普通函数调用、import 调用形态共用同一份 check_call_args，
// 这里补一套跟 import_test.cpp 对称的用例，防止合并之后这条路径成为回归真空。
#include "test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("SemanticChecker eval 调用形态") {

    TEST_CASE("实参照普通函数调用检查，个数/是否叫 code 留给运行期 DispatchError") {
        CHECK_NOTHROW(check_program(U"eval('x')"));
        CHECK_NOTHROW(check_program(U"eval(code='x')"));
        CHECK_NOTHROW(check_program(U"eval(**opts)"));
        CHECK_NOTHROW(check_program(U"eval()"));
        CHECK_NOTHROW(check_program(U"eval('a', 'b')"));
    }

    TEST_CASE("位置组允许 *，关键字组允许 **（跟普通函数调用同一套规则）") {
        CHECK_NOTHROW(check_program(U"eval(*codes)"));
        CHECK_NOTHROW(check_program(U"eval(**opts)"));
        CHECK_NOTHROW(check_program(U"eval('x', *rest, **opts)"));
    }

    TEST_CASE("关键字实参的值里不能裸写 *（can_star 在关键字组被关掉了）") {
        check_throws_with(
            U"eval(code=*a)", "* can only appear in tuple, list, index, or function call arguments"
        );
    }

    TEST_CASE("位置实参里不能裸写 **（can_double_star 只对 ** 那一项自己开）") {
        check_throws_with(
            U"eval(**a + b)", "** can only appear in dict literal or function call arguments"
        );
    }

    TEST_CASE("实参照常递归检查，里面的非法表达式一样会被抓出来") {
        // break 不在循环里
        check_throws_with(U"eval(break)", "break outside loop");
        check_throws_with(U"eval(code=break)", "break outside loop");
        check_throws_with(U"eval(*break)", "break outside loop");
    }

    TEST_CASE("实参检查完，外层的 can_star/can_double_star 上下文要恢复原样") {
        // eval(...) 整体身处一个不允许裸 * 的位置：检查完实参不该把 can_star 漏在开着的状态
        check_throws_with(
            U"x = eval('a') + *b",
            "* can only appear in tuple, list, index, or function call arguments"
        );
        // 反过来：eval(...) 身处允许 * 的位置（函数实参位置组），* 照样合法
        CHECK_NOTHROW(check_program(U"f(eval('a'), *rest)"));
    }
}
