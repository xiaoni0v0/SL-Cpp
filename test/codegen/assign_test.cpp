#include "test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("codegen: 赋值") {

    TEST_CASE("没人要值时不垫那份复制") {
        CHECK(
            codegen_instrs(U"x = 1\nx") ==
            R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
RETURN_VALUE
)"
        );
    }

    TEST_CASE("要值时垫一份出来，让写入指令把原件弹光") {
        CHECK(
            codegen_instrs(U"x = 1") ==
            R"(LOAD_CONST 0
COPY 1
STORE_NAME 0
RETURN_VALUE
)"
        );
    }

    TEST_CASE("属性赋值：目标的对象部分先求值") {
        CHECK(codegen_instrs(U"x = 1\nx.a = x") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
LOAD_NAME 0
COPY 1
INSERT 3
SET_ATTR 1
RETURN_VALUE
)");
    }

    TEST_CASE("下标赋值") {
        CHECK(codegen_instrs(U"x = 1\nx[x] = x") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
LOAD_NAME 0
LOAD_NAME 0
COPY 1
INSERT 4
SET_INDEX 1
RETURN_VALUE
)");
    }

    TEST_CASE("下标里有展开项时个数不再是常数，先拼成 list") {
        CHECK(codegen_instrs(U"x = 1\nx[*x] = x") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
MAKE_LIST 0
LOAD_NAME 0
LIST_EXTEND 1
LOAD_NAME 0
COPY 1
INSERT 4
SET_INDEX_EX
RETURN_VALUE
)");
    }

    TEST_CASE("解构赋值：UNPACK 逆序压入，于是第一个元素在栈顶") {
        CHECK(codegen_instrs(U"x = 1\n(a, b) = x\nx") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
UNPACK 2
STORE_NAME 1
STORE_NAME 2
LOAD_NAME 0
RETURN_VALUE
)");
    }

    TEST_CASE("带 * 的解构：参数高 8 位是星号后的项数、低 8 位是星号前的") {
        // (a, *rest, b) 编译成 EXTENDED_ARG 1 + UNPACK_EX 1，反汇编把前缀折进参数里
        CHECK(codegen_instrs(U"x = 1\n(a, *rest, b) = x\nx") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
UNPACK_EX 257
STORE_NAME 1
STORE_NAME 2
STORE_NAME 3
LOAD_NAME 0
RETURN_VALUE
)");
    }

    TEST_CASE("解构的元素本身可以是属性/下标") {
        // 值已经在栈顶了，前置项要逐个垫到它下面
        CHECK(codegen_instrs(U"x = 1\n(a, x.b) = x\nx") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
UNPACK 2
STORE_NAME 1
LOAD_NAME 0
INSERT 2
SET_ATTR 2
LOAD_NAME 0
RETURN_VALUE
)");
    }

    TEST_CASE("复合赋值读旧值只查 _L，保证读写落在同一帧") {
        CHECK(codegen_instrs(U"x = 1\nx += x\nx") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME_STRICT 0
LOAD_NAME 0
BINARY_OP Add
STORE_NAME 0
LOAD_NAME 0
RETURN_VALUE
)");
    }

    TEST_CASE("复合赋值：目标只求值一次，读旧值靠复制已在栈上的那份") {
        CHECK(codegen_instrs(U"x = 1\nx.a += x\nx") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
COPY 1
GET_ATTR 1
LOAD_NAME 0
BINARY_OP Add
SET_ATTR 1
LOAD_NAME 0
RETURN_VALUE
)");
        CHECK(codegen_instrs(U"x = 1\nx[x] += x\nx") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
LOAD_NAME 0
COPY 2
COPY 2
GET_INDEX 1
LOAD_NAME 0
BINARY_OP Add
SET_INDEX 1
LOAD_NAME 0
RETURN_VALUE
)");
    }

    TEST_CASE("del 只能删名字和属性") {
        CHECK(
            codegen_instrs(U"x = 1\ndel x\nx") ==
            R"(LOAD_CONST 0
STORE_NAME 0
DELETE_NAME 0
LOAD_NAME 0
RETURN_VALUE
)"
        );
        CHECK(codegen_instrs(U"x = 1\ndel x.a\nx") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
DELETE_ATTR 1
LOAD_NAME 0
RETURN_VALUE
)");
    }

    TEST_CASE("del / global 的值恒为 None，要值时才补") {
        CHECK(
            codegen_instrs(U"x = 1\ndel x") ==
            R"(LOAD_CONST 0
STORE_NAME 0
DELETE_NAME 0
LOAD_CONST 1
RETURN_VALUE
)"
        );
    }

} // TEST_SUITE
