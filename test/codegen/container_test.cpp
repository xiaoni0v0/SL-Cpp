#include "test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("codegen: 容器") {

    TEST_CASE("没有展开项时个数是编译期常数，一条指令建好") {
        CHECK(codegen_instrs(U"x = 1\n[x, x]") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
LOAD_NAME 0
MAKE_LIST 2
RETURN_VALUE
)");
        CHECK(codegen_instrs(U"x = 1\n(x, x)") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
LOAD_NAME 0
MAKE_TUPLE 2
RETURN_VALUE
)");
        CHECK(codegen_instrs(U"x = 1\n{x: x}") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
LOAD_NAME 0
MAKE_DICT 1
RETURN_VALUE
)");
    }

    TEST_CASE("空容器") {
        CHECK(
            codegen_instrs(U"x = []\nx") ==
            R"(MAKE_LIST 0
STORE_NAME 0
LOAD_NAME 0
RETURN_VALUE
)"
        );
        // 空 dict 没有字面量写法：{} 是空的复合表达式，值为 None
        CHECK(
            codegen_instrs(U"x = {}\nx") ==
            R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
RETURN_VALUE
)"
        );
        CHECK(constants_of(U"x = {}\nx") == std::vector<std::string>{"None"});
    }

    TEST_CASE("有展开项：第一个展开项之前的一次建好，其后逐项并入") {
        CHECK(codegen_instrs(U"x = 1\n[x, *x, x]") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
MAKE_LIST 1
LOAD_NAME 0
LIST_EXTEND 1
LOAD_NAME 0
LIST_APPEND 1
RETURN_VALUE
)");
    }

    TEST_CASE("第一个元素就是展开项时先建个空的") {
        CHECK(codegen_instrs(U"x = 1\n[*x]") == R"(LOAD_CONST 0
STORE_NAME 0
MAKE_LIST 0
LOAD_NAME 0
LIST_EXTEND 1
RETURN_VALUE
)");
    }

    TEST_CASE("带展开的元组先拼成 list 再换成 tuple") {
        CHECK(codegen_instrs(U"x = 1\n(x, *x)") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
MAKE_LIST 1
LOAD_NAME 0
LIST_EXTEND 1
LIST_TO_TUPLE
RETURN_VALUE
)");
    }

    TEST_CASE("dict 的 ** 展开走 DICT_MERGE，普通项走 DICT_PUT") {
        CHECK(codegen_instrs(U"x = 1\n{x: x, **x, x: x}") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
LOAD_NAME 0
MAKE_DICT 1
LOAD_NAME 0
DICT_MERGE 1
LOAD_NAME 0
LOAD_NAME 0
DICT_PUT 1
RETURN_VALUE
)");
    }

    TEST_CASE("求值顺序与源码顺序一致") {
        // a 在 *b 之前求值，b 在 c 之前
        const std::string dump{codegen_dump(U"a = 1\nb = 1\nc = 1\n[a, *b, c]")};
        CHECK(dump.find("LOAD_NAME 0") < dump.find("LOAD_NAME 1"));
        CHECK(dump.find("LOAD_NAME 1") < dump.find("LOAD_NAME 2"));
    }

} // TEST_SUITE
