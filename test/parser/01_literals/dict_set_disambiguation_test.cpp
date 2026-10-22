// 字典项与集合项的混用错误。
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("字典项和集合项不能混用") {

    TEST_CASE("集合里出现键值对或 ** 展开") {
        check_parse_throws_with(U"${a, k -> v}", "set literal");
        check_parse_throws_with(U"${*s, **d}", "set literal");
    }

    TEST_CASE("字典里出现普通项或 * 展开") {
        check_parse_throws_with(U"${k -> v, a}", "dict literal");
        check_parse_throws_with(U"${**d, *s}", "dict literal");
        check_parse_throws_with(U"${k -> v, **d + x}", "dict literal");
        check_parse_throws_with(U"${k -> v, x, y -> z}", "dict literal");
        check_parse_throws_with(U"${k -> v, x: int}", "dict literal");
    }

    TEST_CASE("报错位置指向出问题的那一项") {
        // "${k -> v, a}" 里 a 在第 11 列
        check_parse_throws_with(U"${k -> v, a}", "1:11:");
    }
}
