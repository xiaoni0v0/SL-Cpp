#include "test_utils.h"

#include <doctest/doctest.h>

// Program 里的裸赋值一律 want_value=false；要测 want_value=true 那一路，
// 把赋值套进另一个赋值的右侧（`y = (x = 1)`）或复合表达式的末尾。

TEST_SUITE("codegen: 赋值") {

    TEST_CASE("没人要值时不垫那份复制") {
        CHECK(codegen_instrs(U"x = 1") == R"(LOAD_CONST 0
STORE_NAME 0
)");
    }

    TEST_CASE("要值时垫一份出来，让写入指令把原件弹光") {
        CHECK(codegen_instrs(U"y = (x = 1)") == R"(LOAD_CONST 0
COPY 1
STORE_NAME 0
STORE_NAME 1
)");
    }

    TEST_CASE("属性赋值：目标的对象部分先求值") {
        CHECK(codegen_instrs(U"x = 1\nx.a = x") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
LOAD_NAME 0
SET_ATTR 1
)");
        // 要值时把那份复制沉到 obj 下面，让 SET_ATTR 把两项都弹光
        CHECK(codegen_instrs(U"x = 1\ny = (x.a = x)") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
LOAD_NAME 0
COPY 1
INSERT 3
SET_ATTR 1
STORE_NAME 2
)");
    }

    TEST_CASE("下标赋值") {
        CHECK(codegen_instrs(U"x = 1\nx[x] = x") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
LOAD_NAME 0
LOAD_NAME 0
SET_INDEX 1
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
SET_INDEX_EX
)");
    }

    TEST_CASE("解构赋值：UNPACK 逆序压入，于是第一个元素在栈顶") {
        CHECK(codegen_instrs(U"x = 1\n(a, b) = x") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
UNPACK 2
STORE_NAME 1
STORE_NAME 2
)");
    }

    TEST_CASE("带 * 的解构：参数高 8 位是星号后的项数、低 8 位是星号前的") {
        // (a, *rest, b) 编译成 EXTENDED_ARG 1 + UNPACK_EX 1，反汇编把前缀折进参数里
        CHECK(codegen_instrs(U"x = 1\n(a, *rest, b) = x") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
UNPACK_EX 257
STORE_NAME 1
STORE_NAME 2
STORE_NAME 3
)");
    }

    TEST_CASE("解构的元素本身可以是属性/下标") {
        // 值已经在栈顶了，前置项要逐个垫到它下面
        CHECK(codegen_instrs(U"x = 1\n(a, x.b) = x") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
UNPACK 2
STORE_NAME 1
LOAD_NAME 0
INSERT 2
SET_ATTR 2
)");
    }

    TEST_CASE("复合赋值读旧值只查 _L，保证读写落在同一帧") {
        CHECK(codegen_instrs(U"x = 1\nx += x") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME_STRICT 0
LOAD_NAME 0
BINARY_OP Add
STORE_NAME 0
)");
    }

    TEST_CASE("复合赋值：目标只求值一次，读旧值靠复制已在栈上的那份") {
        CHECK(codegen_instrs(U"x = 1\nx.a += x") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
COPY 1
GET_ATTR 1
LOAD_NAME 0
BINARY_OP Add
SET_ATTR 1
)");
        CHECK(codegen_instrs(U"x = 1\nx[x] += x") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
LOAD_NAME 0
COPY 2
COPY 2
GET_INDEX 1
LOAD_NAME 0
BINARY_OP Add
SET_INDEX 1
)");
    }

    TEST_CASE("del 只能删名字和属性") {
        CHECK(codegen_instrs(U"x = 1\ndel x") == R"(LOAD_CONST 0
STORE_NAME 0
DELETE_NAME 0
)");
        CHECK(codegen_instrs(U"x = 1\ndel x.a") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
DELETE_ATTR 1
)");
    }

    TEST_CASE("del / global 的值恒为 None，要值时才补") {
        CHECK(codegen_instrs(U"x = 1\ny = { del x }") == R"(LOAD_CONST 0
STORE_NAME 0
DELETE_NAME 0
LOAD_CONST 1
STORE_NAME 1
)");
    }

} // TEST_SUITE
