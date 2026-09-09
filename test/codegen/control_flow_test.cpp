#include "test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("codegen: 控制流") {

    TEST_CASE("if：JUMP_IF_FALSE 只认 bool，条件前恒垫一条 TO_BOOL") {
        CHECK(codegen_body(U"x = 1\ny = if (x) { x } else { x }") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
TO_BOOL
JUMP_IF_FALSE 7
LOAD_NAME 0
JUMP 8
LOAD_NAME 0
STORE_NAME 1
)");
    }

    TEST_CASE("缺 else 时补的 None 是 codegen 自己加的，没人要值就不补") {
        CHECK(codegen_body(U"x = 1\ny = if (x) { x }") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
TO_BOOL
JUMP_IF_FALSE 7
LOAD_NAME 0
JUMP 8
LOAD_CONST 1
STORE_NAME 1
)");
        CHECK(codegen_body(U"x = 1\nif (x) { x }") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
TO_BOOL
JUMP_IF_FALSE 8
LOAD_NAME 0
POP_TOP
JUMP 8
)");
    }

    TEST_CASE("elif 是又一段条件") {
        CHECK(codegen_body(U"x = 1\ny = if (x) { x } elif (x) { x } else { x }") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
TO_BOOL
JUMP_IF_FALSE 7
LOAD_NAME 0
JUMP 13
LOAD_NAME 0
TO_BOOL
JUMP_IF_FALSE 12
LOAD_NAME 0
JUMP 13
LOAD_NAME 0
STORE_NAME 1
)");
    }

    TEST_CASE("步进 for：计数模式没人要值时退化成裸循环") {
        CHECK(codegen_body(U"x = 1\nfor (; x; ) { x }") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
TO_BOOL
JUMP_IF_FALSE 8
LOAD_NAME 0
POP_TOP
JUMP 2
)");
    }

    TEST_CASE("步进 for：要值时压一个计数槽，每轮 COUNT_INC") {
        CHECK(codegen_body(U"x = 1\ny = for (; x; ) { x }") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_CONST 1
LOAD_NAME 0
TO_BOOL
JUMP_IF_FALSE 10
LOAD_NAME 0
POP_TOP
COUNT_INC 1
JUMP 3
STORE_NAME 1
)");
    }

    TEST_CASE("cond 为空视为 True，不发条件判断") {
        CHECK(codegen_body(U"for (; ; ) { break }").find("TO_BOOL") == std::string::npos);
    }

    TEST_CASE("迭代 for：结果槽在下、迭代器在上，收尾指令要跨过迭代器") {
        CHECK(codegen_body(U"x = 1\ny = for $ (x as it) { it }") == R"(LOAD_CONST 0
STORE_NAME 0
MAKE_LIST 0
LOAD_NAME 0
GET_ITER
FOR_ITER 10
STORE_NAME 1
LOAD_NAME 1
LIST_APPEND 2
JUMP 5
STORE_NAME 2
)");
    }

    TEST_CASE("没有 as 时直接把取到的元素弹掉") {
        CHECK(codegen_body(U"x = 1\ny = for $ (x) { x }") == R"(LOAD_CONST 0
STORE_NAME 0
MAKE_LIST 0
LOAD_NAME 0
GET_ITER
FOR_ITER 10
POP_TOP
LOAD_NAME 0
LIST_APPEND 2
JUMP 5
STORE_NAME 1
)");
    }

    TEST_CASE("$ * 用 LIST_EXTEND，$$ 用 DICT_PUT，$$ ** 用 DICT_MERGE") {
        CHECK(
            codegen_body(U"x = 1\ny = for $ * (x) { x }").find("LIST_EXTEND 2") != std::string::npos
        );
        CHECK(
            codegen_body(U"x = 1\ny = for $$ ** (x) { x }").find("DICT_MERGE 2") !=
            std::string::npos
        );
        // $$ 每轮的值是个 (k, v) 对：UNPACK 2 给出 v k，DICT_PUT 要 d k v
        CHECK(
            codegen_body(U"x = 1\ny = for $$ (x) { x }").find("UNPACK 2\nINSERT 2\nDICT_PUT 2") !=
            std::string::npos
        );
    }

    TEST_CASE("收集模式的结果槽省不掉：收尾指令自己可能抛异常") {
        // 没人要这个 list，但每轮的 LIST_APPEND 还得照做，最后再弹掉
        const std::string dump{codegen_body(U"x = 1\nfor $ (x) { x }")};
        CHECK(dump.find("MAKE_LIST 0") != std::string::npos);
        CHECK(dump.find("LIST_APPEND 2") != std::string::npos);
        CHECK(dump.ends_with("POP_TOP\n"));
    }

    TEST_CASE("break 统一跳出口，continue 按收集模式分") {
        // 计数模式：continue 跳到 POP_TOP 之后，仍然计入
        CHECK(
            codegen_body(U"x = 1\ny = for (; x; ) { if (x) { break } else { continue } }") ==
            R"(LOAD_CONST 0
STORE_NAME 0
LOAD_CONST 1
LOAD_NAME 0
TO_BOOL
JUMP_IF_FALSE 15
LOAD_NAME 0
TO_BOOL
JUMP_IF_FALSE 11
JUMP 15
JUMP 12
JUMP 13
POP_TOP
COUNT_INC 1
JUMP 3
STORE_NAME 1
)"
        );
    }

    TEST_CASE("迭代 for 里 break 提前退出时要把迭代器一起裁掉") {
        // 第 7 条那个 POP_TOP 就是 break 在裁迭代器。后面两条走不到，是循环体恒 break 的必然结果
        CHECK(codegen_body(U"x = 1\ny = for $ (x) { break }") == R"(LOAD_CONST 0
STORE_NAME 0
MAKE_LIST 0
LOAD_NAME 0
GET_ITER
FOR_ITER 11
POP_TOP
POP_TOP
JUMP 11
LIST_APPEND 2
JUMP 5
STORE_NAME 1
)");
    }

} // TEST_SUITE
