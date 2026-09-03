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

    // 合不合法只看目标节点自己的类型（根节点是 Attr 还是 Index），不看这个节点的 object_ 是什么
    // 形状——同样掺了下标访问，object_ 在里面（根节点仍是 Attr）合法，根节点本身是 Index 就非法
    TEST_CASE("目标是不是下标访问，只看根节点类型，不看嵌在里面的 object_") {
        CHECK_NOTHROW(check_program(U"del x[0].y")); // 根节点是 Attr，合法
        check_throws_with(
            U"del x.y[0]", "del target must be an identifier or attribute access"
        ); // 根节点是 Index，非法
    }
}
