#include "test_utils.h"

#include <string>

#include <doctest/doctest.h>

TEST_SUITE("codegen: 容器") {

    TEST_CASE("没有展开项时个数是编译期常数，一条指令建好") {
        const std::string head{"LOAD_CONST 0\nSTORE_NAME 0\nLOAD_NAME 0\nLOAD_NAME 0\n"};
        const std::string tail{"\nSTORE_NAME 1\n"};
        CHECK(codegen_body(U"x = 1\ny = [x, x]") == head + "MAKE_LIST 2" + tail);
        CHECK(codegen_body(U"x = 1\ny = (x, x)") == head + "MAKE_TUPLE 2" + tail);
        CHECK(codegen_body(U"x = 1\ny = {x: x}") == head + "MAKE_DICT 1" + tail);
    }

    TEST_CASE("空 list") {
        CHECK(codegen_body(U"x = []") == R"(MAKE_LIST 0
STORE_NAME 0
)");
    }

    TEST_CASE("有展开项：第一个展开项之前的一次建好，其后逐项并入") {
        CHECK(codegen_body(U"x = 1\ny = [x, *x, x]") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
MAKE_LIST 1
LOAD_NAME 0
LIST_EXTEND 1
LOAD_NAME 0
LIST_APPEND 1
STORE_NAME 1
)");
    }

    TEST_CASE("第一个元素就是展开项时先建个空的") {
        CHECK(codegen_body(U"x = 1\ny = [*x]") == R"(LOAD_CONST 0
STORE_NAME 0
MAKE_LIST 0
LOAD_NAME 0
LIST_EXTEND 1
STORE_NAME 1
)");
    }

    TEST_CASE("带展开的元组先拼成 list 再换成 tuple") {
        CHECK(codegen_body(U"x = 1\ny = (x, *x)") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
MAKE_LIST 1
LOAD_NAME 0
LIST_EXTEND 1
LIST_TO_TUPLE
STORE_NAME 1
)");
    }

    TEST_CASE("dict 的 ** 展开走 DICT_MERGE，普通项走 DICT_PUT") {
        CHECK(codegen_body(U"x = 1\ny = {x: x, **x, x: x}") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
LOAD_NAME 0
MAKE_DICT 1
LOAD_NAME 0
DICT_MERGE 1
LOAD_NAME 0
LOAD_NAME 0
DICT_PUT 1
STORE_NAME 1
)");
    }

    TEST_CASE("求值顺序与源码顺序一致") {
        // a 在 *b 之前求值，b 在 c 之前
        const std::string dump{codegen_dump(U"a = 1\nb = 1\nc = 1\nd = [a, *b, c]")};
        CHECK(dump.find("LOAD_NAME 0") < dump.find("LOAD_NAME 1"));
        CHECK(dump.find("LOAD_NAME 1") < dump.find("LOAD_NAME 2"));
    }

} // TEST_SUITE
