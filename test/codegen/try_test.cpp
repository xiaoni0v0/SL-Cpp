#include "test_utils.h"

#include <string>

#include <doctest/doctest.h>

TEST_SUITE("codegen: 异常与 finally") {

    TEST_CASE("raise") {
        CHECK(codegen_instrs(U"x = 1\nraise x") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
RAISE
)");
    }

    TEST_CASE("处理器入口栈上只有异常对象，CHECK_EXC_MATCH 自己会把它留下") {
        CHECK(codegen_instrs(U"x = 1\ny = try { x } except (x) { x }") == R"(LOAD_CONST 0
STORE_NAME 0
SETUP_EXCEPT 6
LOAD_NAME 0
POP_BLOCK
JUMP 13
LOAD_NAME 0
CHECK_EXC_MATCH
JUMP_IF_FALSE 12
POP_TOP
LOAD_NAME 0
JUMP 13
RERAISE
STORE_NAME 1
)");
    }

    TEST_CASE("as 把栈顶那个异常对象赋出去") {
        const std::string dump{codegen_instrs(U"x = 1\ntry { x } except (x as e) { e }")};
        CHECK(dump.find("CHECK_EXC_MATCH") != std::string::npos);
        CHECK(dump.find("STORE_NAME 1") != std::string::npos);
    }

    TEST_CASE("一条子句列多个异常类型时任一匹配就算中") {
        CHECK(codegen_instrs(U"x = 1\ny = try { x } except (x, x) { x }") == R"(LOAD_CONST 0
STORE_NAME 0
SETUP_EXCEPT 6
LOAD_NAME 0
POP_BLOCK
JUMP 17
LOAD_NAME 0
CHECK_EXC_MATCH
JUMP_IF_FALSE 10
JUMP 13
LOAD_NAME 0
CHECK_EXC_MATCH
JUMP_IF_FALSE 16
POP_TOP
LOAD_NAME 0
JUMP 17
RERAISE
STORE_NAME 1
)");
    }

    TEST_CASE("都不匹配就继续往外传播") {
        CHECK(
            codegen_instrs(U"x = 1\ntry { x } except (x) { x }").find("RERAISE") !=
            std::string::npos
        );
    }

    TEST_CASE("finally 只编译一份，正常路径和异常路径共用") {
        // 进 finally 体时栈顶恒是"这条路径要带出去的那个值"：正常路径带的是 try 的值，
        // 异常路径带的是异常对象，两边深度对得上，所以一份代码够用
        CHECK(codegen_instrs(U"x = 1\ny = try { x } finally { x }") == R"(LOAD_CONST 0
STORE_NAME 0
SETUP_FINALLY 7
LOAD_NAME 0
POP_BLOCK
CALL_FINALLY 7
JUMP 10
LOAD_NAME 0
POP_TOP
END_FINALLY
STORE_NAME 1
)");
    }

    TEST_CASE("没人要 try 的值时压个 None 占位，好让两条路径的栈深对上") {
        CHECK(codegen_instrs(U"x = 1\ntry { x } finally { x }") == R"(LOAD_CONST 0
STORE_NAME 0
SETUP_FINALLY 10
LOAD_NAME 0
POP_TOP
POP_BLOCK
LOAD_CONST 1
CALL_FINALLY 10
POP_TOP
JUMP 13
LOAD_NAME 0
POP_TOP
END_FINALLY
)");
    }

    TEST_CASE("except 和 finally 一起时 finally 包在外面") {
        const std::string dump{codegen_instrs(U"x = 1\ntry { x } except (x) { x } finally { x }")};
        CHECK(dump.find("SETUP_FINALLY") < dump.find("SETUP_EXCEPT"));
        CHECK(dump.find("RERAISE") < dump.find("END_FINALLY"));
    }

    TEST_CASE("return 穿过 finally 时返回值就是那条路径带出去的值") {
        const std::string dump{codegen_dump(U"func f() { try { return 1 } finally { 2 } }")};
        // 返回值本来就在栈顶，不用为它另开位置
        CHECK(dump.find("CALL_FINALLY") != std::string::npos);
        CHECK(dump.find("RETURN_VALUE") != std::string::npos);
    }

    TEST_CASE("跳出 try 之前要逐层 POP_BLOCK，块栈是运行期的") {
        // 第 6 条那个 POP_BLOCK 就是 break 在弹 SETUP_EXCEPT 压的块。
        // 漏了它，跳到循环外之后再抛异常还会撞进这个已经失效的处理器
        CHECK(
            codegen_instrs(U"x = 1\nfor (; x; ) { try { break } except (x) { x } }") ==
            R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
TO_BOOL
JUMP_IF_FALSE 19
SETUP_EXCEPT 10
POP_BLOCK
JUMP 19
POP_BLOCK
JUMP 17
LOAD_NAME 0
CHECK_EXC_MATCH
JUMP_IF_FALSE 16
POP_TOP
LOAD_NAME 0
JUMP 17
RERAISE
POP_TOP
JUMP 2
)"
        );
    }

    TEST_CASE("return 也要先弹掉自己这层块再进 finally 体") {
        // 不弹的话，finally 体里抛的异常会被它自己这层接住，转圈
        const std::string dump{codegen_dump(U"func f() { try { return 1 } finally { 2 } }")};
        CHECK(dump.find(R"(     1  LOAD_CONST 0  ; int 1
     2  POP_BLOCK
     3  CALL_FINALLY 10
     4  RETURN_VALUE)") != std::string::npos);
    }

    TEST_CASE("break 穿过 finally 时补一个 None 占位，回来再弹掉") {
        const std::string dump{
            codegen_instrs(U"x = 1\nfor (; x; ) { try { break } finally { x } }")
        };
        CHECK(dump.find("CALL_FINALLY") != std::string::npos);
        CHECK(dump.find("END_FINALLY") != std::string::npos);
    }

} // TEST_SUITE
