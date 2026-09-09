#include "test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("codegen: 调用、函数与类") {

    TEST_CASE("三条调用指令是按出现频率分的档") {
        // 没有关键字、没有展开：零额外对象
        CHECK(codegen_instrs(U"x = 1\nx(x, x)") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
LOAD_NAME 0
LOAD_NAME 0
CALL 2
RETURN_VALUE
)");
        // 有关键字：多一个常量表里的名字元组，不新建对象
        CHECK(codegen_instrs(U"x = 1\nx(x, k = x)") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
LOAD_NAME 0
LOAD_NAME 0
LOAD_CONST 2
CALL_KW 2
RETURN_VALUE
)");
        // 有展开：这才建两个新容器
        CHECK(codegen_instrs(U"x = 1\nx(*x)") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
MAKE_LIST 0
LOAD_NAME 0
LIST_EXTEND 1
MAKE_DICT 0
CALL_EX
RETURN_VALUE
)");
    }

    TEST_CASE("** 展开也走 CALL_EX") {
        const std::string dump{codegen_instrs(U"x = 1\nx(**x)")};
        CHECK(dump.find("DICT_MERGE 1") != std::string::npos);
        CHECK(dump.find("CALL_EX") != std::string::npos);
    }

    TEST_CASE("属性与下标") {
        CHECK(
            codegen_instrs(U"x = 1\nx.a") ==
            R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
GET_ATTR 1
RETURN_VALUE
)"
        );
        CHECK(codegen_instrs(U"x = 1\nx[x, x]") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
LOAD_NAME 0
LOAD_NAME 0
GET_INDEX 2
RETURN_VALUE
)");
        // 下标里有展开项时先拼成 list 再交 _EX 版
        CHECK(codegen_instrs(U"x = 1\nx[*x]") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
MAKE_LIST 0
LOAD_NAME 0
LIST_EXTEND 1
GET_INDEX_EX
RETURN_VALUE
)");
    }

    TEST_CASE("函数体是一份嵌套 Code，不进常量表") {
        const std::string dump{codegen_dump(U"func f() { 1 }")};
        CHECK(dump.find("MAKE_FUNC 0") != std::string::npos);
        CHECK(dump.find("=== <main>.code[0] ===") != std::string::npos);
        // 常量表里不许出现 Code
        CHECK(dump.find("0: code") == std::string::npos);
    }

    TEST_CASE("MAKE_FUNC 固定弹 5 项，缺的那些用内部哨兵顶上") {
        // 匿名、无捕获、无形参、无返回注解、无 doc
        CHECK(codegen_instrs(U"func () { 1 }") == R"(MAKE_TUPLE 0
MAKE_TUPLE 0
LOAD_INTERNAL 0
LOAD_INTERNAL 0
LOAD_INTERNAL 0
MAKE_FUNC 0
RETURN_VALUE
)");
    }

    TEST_CASE("形参段按声明顺序，写了注解就压注解、写了默认值就压默认值") {
        const std::string dump{codegen_dump(U"x = 1\nfunc (p: x = x, q = x) { 1 }")};
        CHECK(dump.find("MAKE_TUPLE 3") != std::string::npos);
    }

    TEST_CASE("命名函数绑定一次") {
        CHECK(codegen_instrs(U"func f() { 1 }") == R"(MAKE_TUPLE 0
MAKE_TUPLE 0
LOAD_INTERNAL 0
LOAD_INTERNAL 0
LOAD_CONST 0
MAKE_FUNC 0
COPY 1
STORE_NAME 0
RETURN_VALUE
)");
    }

    TEST_CASE("引用捕获不产生指令，只在名字表上留标记；值捕获才压值") {
        const std::string dump{codegen_dump(U"x = 1\ny = 1\nfunc f[&x, y]() { 1 }")};
        CHECK(dump.find("(ref captured)") != std::string::npos);
        CHECK(dump.find("value captured") != std::string::npos);
        // 一个值捕获，打成一个元组
        CHECK(dump.find("MAKE_TUPLE 1") != std::string::npos);
    }

    TEST_CASE("装饰器：建立之前从前到后求值，建立之后由近到远逐个调用") {
        const std::string dump{codegen_instrs(U"d = 1\ne = 1\n@d @e func f() { 1 }")};
        // d 先压、e 后压，于是 MAKE_FUNC 之后第一条 CALL 用的是离 func 最近的 e
        CHECK(dump.find("LOAD_NAME 0\nLOAD_NAME 1\n") != std::string::npos);
        CHECK(dump.find("MAKE_FUNC 0\nCALL 1\nCALL 1\n") != std::string::npos);
    }

    TEST_CASE("通用形式的装饰器就是一次调用") {
        CHECK(codegen_instrs(U"x = 1\n@x x") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
LOAD_NAME 0
CALL 1
RETURN_VALUE
)");
    }

    TEST_CASE("MAKE_CLASS 弹 4 项，基类元组顶掉形参段") {
        CHECK(codegen_instrs(U"class C() { 1 }") == R"(MAKE_TUPLE 0
MAKE_TUPLE 0
LOAD_INTERNAL 0
LOAD_CONST 0
MAKE_CLASS 0
COPY 1
STORE_NAME 0
RETURN_VALUE
)");
    }

    TEST_CASE("类体也是一份嵌套 Code，末尾 RETURN_VALUE 交给类构建器收尾") {
        const std::string dump{codegen_dump(U"class C() { m = 1 }")};
        CHECK(dump.find("=== <main>.code[0] ===") != std::string::npos);
        CHECK(dump.find("MAKE_CLASS 0") != std::string::npos);
    }

    TEST_CASE("import 关键字形态：压入第一段模块对象，它同时也是整条表达式的值") {
        CHECK(
            codegen_instrs(U"import a.b.c") ==
            R"(IMPORT 0
COPY 1
STORE_NAME 1
RETURN_VALUE
)"
        );
        CHECK(codegen_dump(U"import a.b.c").find("0: a.b.c") != std::string::npos);
    }

    TEST_CASE("import 调用形态包成一个内部函数对象，接一次普通调用") {
        CHECK(
            codegen_instrs(U"import('a')") ==
            R"(LOAD_INTERNAL 1
LOAD_CONST 0
CALL 1
RETURN_VALUE
)"
        );
    }

    TEST_CASE("eval 不能照办，必须留成独立指令") {
        CHECK(
            codegen_instrs(U"eval('1')") ==
            R"(LOAD_CONST 0
MAKE_LIST 1
MAKE_DICT 0
EVAL
RETURN_VALUE
)"
        );
    }

} // TEST_SUITE
