#include "test_utils.h"

#include <string>
#include <vector>

#include <doctest/doctest.h>

// Program（整个文件 / 函数体 / 类体）的值只由 return 决定：走到末尾就是 None，
// 等价于末尾隐含一条 return None——由虚拟机在 pc 走出字节码末尾时兑现，字节码里没有对应指令。
// 这跟复合表达式"值是最后一条"是两套规则，别混。

TEST_SUITE("codegen: Program 的值") {

    TEST_CASE("没触发 return 时，字节码里没有收尾指令，走出末尾就是 return None") {
        CHECK(codegen_instrs(U"x = 1") == R"(LOAD_CONST 0
STORE_NAME 0
)");
        // None 不会为此进常量表
        CHECK(constants_of(U"x = 1") == std::vector<std::string>{"int 1"});
    }

    TEST_CASE("空 Program 没有任何指令") { CHECK(codegen_instrs(U"") == ""); }

    TEST_CASE("跳转目标可以恰好是字节码末尾") {
        // if 是最后一条语句，两条跳转都落在指令总数（10）处
        CHECK(codegen_instrs(U"x = 1\nif (x == 1) { y = 2 }") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
LOAD_CONST 0
BINARY_OP Eq
TO_BOOL
JUMP_IF_FALSE 10
LOAD_CONST 1
STORE_NAME 1
JUMP 10
)");
    }

    TEST_CASE("Program 里每一条都不产出值，最后一条也不例外") {
        // 末尾那个 x 的值没人要，读完就弹掉——不像复合表达式会把它当成整块的值
        CHECK(codegen_instrs(U"x = 1\nx") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
POP_TOP
)");
    }

    TEST_CASE("复合表达式相反：值就是块里最后一条") {
        // 第一条 1 没人要值，纯字面量不发指令；第二条 2 是整块的值
        CHECK(codegen_instrs(U"x = { 1; 2 }") == R"(LOAD_CONST 0
STORE_NAME 0
)");
        CHECK(constants_of(U"x = { 1; 2 }") == std::vector<std::string>{"int 2"});
    }

    TEST_CASE("空复合表达式的值是 None") {
        CHECK(codegen_instrs(U"x = {}") == R"(LOAD_CONST 0
STORE_NAME 0
)");
        CHECK(constants_of(U"x = {}") == std::vector<std::string>{"None"});
    }

    TEST_CASE("return 决定 Program 的值，其后的内容走不到") {
        CHECK(codegen_instrs(U"return 1") == R"(LOAD_CONST 0
RETURN_VALUE
)");
        CHECK(constants_of(U"return 1") == std::vector<std::string>{"int 1"});
    }

    TEST_CASE("裸 return 等价于 return None") {
        CHECK(codegen_instrs(U"return") == R"(LOAD_CONST 0
RETURN_VALUE
)");
        CHECK(constants_of(U"return") == std::vector<std::string>{"None"});
    }

    TEST_CASE("函数体同一套规则：不写 return 就返回 None") {
        const std::string dump{codegen_dump(U"func f() { 1 }")};
        // 函数体里那个 1 没人要值，压根不发指令，整份字节码是空的
        CHECK(dump.ends_with("code:\n"));
    }

    TEST_CASE("函数体写了 return 就返回那个值") {
        const std::string dump{codegen_dump(U"func f() { return 1 }")};
        CHECK(dump.find(R"(code:
     0  LOAD_CONST 0  ; int 1
     1  RETURN_VALUE
)") != std::string::npos);
    }

    TEST_CASE("类体也是 Program，同一套规则") {
        const std::string dump{codegen_dump(U"class C() { m = 1 }")};
        CHECK(dump.find(R"(code:
     0  LOAD_CONST 0  ; int 1
     1  STORE_NAME 0  ; m
)") != std::string::npos);
    }

    TEST_CASE("raise 收尾时同样没有额外指令") {
        CHECK(codegen_instrs(U"x = 1\nraise x") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
RAISE
)");
    }

} // TEST_SUITE
