#include "test_utils.h"

#include <doctest/doctest.h>

// 被测表达式一律放在赋值右侧——那才是 want_value=true 的位置，Program 里每一条都不产出值。

TEST_SUITE("codegen: 运算符") {

    TEST_CASE("一元运算符") {
        const std::string head{"LOAD_CONST 0\nSTORE_NAME 0\nLOAD_NAME 0\n"};
        CHECK(codegen_body(U"x = 1\ny = -x") == head + "UNARY_OP Neg\nSTORE_NAME 1\n");
        CHECK(codegen_body(U"x = 1\ny = not x") == head + "UNARY_OP Not\nSTORE_NAME 1\n");
        CHECK(codegen_body(U"x = 1\ny = x?") == head + "UNARY_OP Question\nSTORE_NAME 1\n");
    }

    TEST_CASE("二元运算符") {
        CHECK(codegen_body(U"x = 1\ny = x + x") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
LOAD_NAME 0
BINARY_OP Add
STORE_NAME 1
)");
    }

    TEST_CASE("比较、in、.. 也走 BINARY_OP") {
        const std::string head{"LOAD_CONST 0\nSTORE_NAME 0\nLOAD_NAME 0\nLOAD_NAME 0\n"};
        const std::string tail{"\nSTORE_NAME 1\n"};
        CHECK(codegen_body(U"x = 1\ny = x < x") == head + "BINARY_OP Lt" + tail);
        CHECK(codegen_body(U"x = 1\ny = x in x") == head + "BINARY_OP In" + tail);
        CHECK(codegen_body(U"x = 1\ny = x .. x") == head + "BINARY_OP Range" + tail);
        CHECK(codegen_body(U"x = 1\ny = x is x") == head + "BINARY_OP Is" + tail);
    }

    TEST_CASE("and：只用消费型的 JUMP_IF_FALSE，原件留栈上当短路结果") {
        CHECK(codegen_body(U"x = 1\ny = x and x") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
COPY 1
TO_BOOL
JUMP_IF_FALSE 8
POP_TOP
LOAD_NAME 0
STORE_NAME 1
)");
    }

    TEST_CASE("or：没有 JUMP_IF_TRUE，把假才跳倒过来再加一条无条件跳") {
        CHECK(codegen_body(U"x = 1\ny = x or x") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
COPY 1
TO_BOOL
JUMP_IF_FALSE 7
JUMP 9
POP_TOP
LOAD_NAME 0
STORE_NAME 1
)");
    }

    TEST_CASE("没人要值时 and 不必复制原件") {
        // 裸表达式一条，值没人要
        CHECK(codegen_body(U"x = 1\nx and x") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
TO_BOOL
JUMP_IF_FALSE 7
LOAD_NAME 0
POP_TOP
)");
    }

    TEST_CASE("比较链：中间段留右操作数，失败分支共享一个出口") {
        // a < b < c：第一段比完把 b 留下当第二段的左操作数
        CHECK(codegen_body(U"x = 1\ny = x < x < x") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
LOAD_NAME 0
COPY 1
INSERT 3
BINARY_OP Lt
COPY 1
TO_BOOL
JUMP_IF_FALSE 14
POP_TOP
LOAD_NAME 0
BINARY_OP Lt
JUMP 16
INSERT 2
POP_TOP
STORE_NAME 1
)");
    }

    TEST_CASE("两个操作数的比较没有链可言，不铺失败分支") {
        CHECK(codegen_body(U"x = 1\ny = x < x") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
LOAD_NAME 0
BINARY_OP Lt
STORE_NAME 1
)");
    }

    TEST_CASE("is 链跟比较链同一套") {
        CHECK(codegen_body(U"x = 1\ny = x is x is x") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
LOAD_NAME 0
COPY 1
INSERT 3
BINARY_OP Is
COPY 1
TO_BOOL
JUMP_IF_FALSE 14
POP_TOP
LOAD_NAME 0
BINARY_OP Is
JUMP 16
INSERT 2
POP_TOP
STORE_NAME 1
)");
    }

} // TEST_SUITE
