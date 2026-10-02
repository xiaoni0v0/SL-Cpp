#include "test_utils.h"

#include <string>
#include <vector>

#include <doctest/doctest.h>

// Program（整个文件 / 函数体 / 类体）的值只由 return 决定：走到末尾就是 None，
// 等价于末尾隐含一条 return None。这跟复合表达式"值是最后一条"是两套规则，别混。

TEST_SUITE("codegen: Program 的值") {

    TEST_CASE("没触发 return 时，末尾隐含一条 return None") {
        CHECK(codegen_instrs(U"x = 1") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_CONST 1
RETURN_VALUE
)");
        // 那条 None 也是常量表里的一项
        CHECK(constants_of(U"x = 1") == std::vector<std::string>{"int 1", "None"});
    }

    TEST_CASE("Program 里每一条都不产出值，最后一条也不例外") {
        // 末尾那个 x 的值没人要，读完就弹掉——不像复合表达式会把它当成整块的值
        CHECK(codegen_body(U"x = 1\nx") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
POP_TOP
)");
    }

    TEST_CASE("复合表达式相反：值就是块里最后一条") {
        // 第一条 1 没人要值，纯字面量不发指令；第二条 2 是整块的值
        CHECK(codegen_body(U"x = { 1; 2 }") == R"(LOAD_CONST 0
STORE_NAME 0
)");
        CHECK(constants_of(U"x = { 1; 2 }") == std::vector<std::string>{"int 2", "None"});
    }

    TEST_CASE("空复合表达式的值是 None") {
        CHECK(codegen_body(U"x = {}") == R"(LOAD_CONST 0
STORE_NAME 0
)");
        CHECK(constants_of(U"x = {}") == std::vector<std::string>{"None"});
    }

    TEST_CASE("return 决定 Program 的值，其后的内容走不到") {
        CHECK(codegen_instrs(U"return 1") == R"(LOAD_CONST 0
RETURN_VALUE
)");
        // return 之后不再补那条隐含的 return None
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
        // 函数体里那个 1 没人要值，压根不发指令；末尾是隐含的 return None
        CHECK(dump.find(R"(code:
     0  LOAD_CONST 0  ; None
     1  RETURN_VALUE
)") != std::string::npos);
    }

    TEST_CASE("函数体写了 return 就返回那个值") {
        const std::string dump{codegen_dump(U"func f() { return 1 }")};
        CHECK(dump.find(R"(code:
     0  LOAD_CONST 0  ; int 1
     1  RETURN_VALUE
)") != std::string::npos);
    }

    TEST_CASE("类体也是 Program，末尾照样是 RETURN_VALUE") {
        const std::string dump{codegen_dump(U"class C() { m = 1 }")};
        CHECK(dump.find(R"(code:
     0  LOAD_CONST 0  ; int 1
     1  STORE_NAME 0  ; m
     2  LOAD_CONST 1  ; None
     3  RETURN_VALUE
)") != std::string::npos);
    }

    TEST_CASE("raise 收尾时也不补那条走不到的 return None") {
        CHECK(codegen_instrs(U"x = 1\nraise x") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
RAISE
)");
    }

} // TEST_SUITE
