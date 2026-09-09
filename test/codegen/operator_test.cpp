#include "test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("codegen: 运算符") {

    TEST_CASE("一元运算符") {
        CHECK(
            codegen_instrs(U"x = 1\n-x") ==
            R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
UNARY_OP Neg
RETURN_VALUE
)"
        );
        CHECK(
            codegen_instrs(U"x = 1\nnot x") ==
            R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
UNARY_OP Not
RETURN_VALUE
)"
        );
        CHECK(
            codegen_instrs(U"x = 1\nx?") ==
            R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
UNARY_OP Question
RETURN_VALUE
)"
        );
    }

    TEST_CASE("二元运算符") {
        CHECK(codegen_instrs(U"x = 1\nx + x") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
LOAD_NAME 0
BINARY_OP Add
RETURN_VALUE
)");
    }

    TEST_CASE("比较、in、.. 也走 BINARY_OP") {
        const std::string head{"LOAD_CONST 0\nSTORE_NAME 0\nLOAD_NAME 0\nLOAD_NAME 0\n"};
        CHECK(codegen_instrs(U"x = 1\nx < x") == head + "BINARY_OP Lt\nRETURN_VALUE\n");
        CHECK(codegen_instrs(U"x = 1\nx in x") == head + "BINARY_OP In\nRETURN_VALUE\n");
        CHECK(codegen_instrs(U"x = 1\nx .. x") == head + "BINARY_OP Range\nRETURN_VALUE\n");
        CHECK(codegen_instrs(U"x = 1\nx is x") == head + "BINARY_OP Is\nRETURN_VALUE\n");
    }

    TEST_CASE("and：只用消费型的 JUMP_IF_FALSE，原件留栈上当短路结果") {
        CHECK(codegen_instrs(U"x = 1\nx and x") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
COPY 1
TO_BOOL
JUMP_IF_FALSE 8
POP_TOP
LOAD_NAME 0
RETURN_VALUE
)");
    }

    TEST_CASE("or：没有 JUMP_IF_TRUE，把假才跳倒过来再加一条无条件跳") {
        CHECK(codegen_instrs(U"x = 1\nx or x") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
COPY 1
TO_BOOL
JUMP_IF_FALSE 7
JUMP 9
POP_TOP
LOAD_NAME 0
RETURN_VALUE
)");
    }

    TEST_CASE("没人要值时 and 不必复制原件") {
        CHECK(codegen_instrs(U"x = 1\nx and x\nx") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
TO_BOOL
JUMP_IF_FALSE 7
LOAD_NAME 0
POP_TOP
LOAD_NAME 0
RETURN_VALUE
)");
    }

    TEST_CASE("比较链：中间段留右操作数，失败分支共享一个出口") {
        // a < b < c：第一段比完把 b 留下当第二段的左操作数
        CHECK(codegen_instrs(U"x = 1\nx < x < x") == R"(LOAD_CONST 0
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
RETURN_VALUE
)");
    }

    TEST_CASE("两个操作数的比较没有链可言，不铺失败分支") {
        CHECK(codegen_instrs(U"x = 1\nx < x") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
LOAD_NAME 0
BINARY_OP Lt
RETURN_VALUE
)");
    }

    TEST_CASE("is 链跟比较链同一套") {
        CHECK(codegen_instrs(U"x = 1\nx is x is x") == R"(LOAD_CONST 0
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
RETURN_VALUE
)");
    }

} // TEST_SUITE
