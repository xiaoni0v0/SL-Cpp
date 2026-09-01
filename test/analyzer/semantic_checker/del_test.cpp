// del 目标必须是标识符或属性访问。
#include "test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("SemanticChecker del") {

    TEST_CASE("标识符、属性访问合法") {
        CHECK_NOTHROW(check_program(U"del x"));
        CHECK_NOTHROW(check_program(U"del x.y"));
        CHECK_NOTHROW(check_program(U"del x.y.z"));
    }

    TEST_CASE("元素访问、字面量、调用非法") {
        check_throws_with(U"del x[0]", "del target must be an identifier or attribute access");
        check_throws_with(U"del 1", "del target must be an identifier or attribute access");
        check_throws_with(U"del f()", "del target must be an identifier or attribute access");
    }
}
