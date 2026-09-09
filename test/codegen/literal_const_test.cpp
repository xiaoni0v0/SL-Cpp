#include "test_utils.h"

#include <doctest/doctest.h>

// 注意：单独一条纯字面量语句会被折叠器当死代码剪掉，所以下面的用例一律写成
// "x = 字面量" 再取一次 x，让那个字面量活到 codegen。

TEST_SUITE("codegen: 字面量与常量表") {

    TEST_CASE("字面量一律走 LOAD_CONST") {
        const std::string tail{"LOAD_CONST 0\nSTORE_NAME 0\nLOAD_NAME 0\nRETURN_VALUE\n"};
        CHECK(codegen_instrs(U"x = 1\nx") == tail);
        CHECK(codegen_instrs(U"x = 'hi'\nx") == tail);
        CHECK(codegen_instrs(U"x = 1.5\nx") == tail);
    }

    TEST_CASE("None / True / False / Ellipsis 也不例外") {
        CHECK(constants_of(U"x = None\nx") == std::vector<std::string>{"None"});
        CHECK(constants_of(U"x = True\nx") == std::vector<std::string>{"bool True"});
        CHECK(constants_of(U"x = False\nx") == std::vector<std::string>{"bool False"});
        CHECK(constants_of(U"x = ...\nx") == std::vector<std::string>{"Ellipsis"});
    }

    TEST_CASE("_G / _L 压的是字典本身，不进常量表") {
        CHECK(
            codegen_instrs(U"x = _L\nx") ==
            R"(LOAD_GL 0
STORE_NAME 0
LOAD_NAME 0
RETURN_VALUE
)"
        );
        CHECK(
            codegen_instrs(U"x = _G\nx") ==
            R"(LOAD_GL 1
STORE_NAME 0
LOAD_NAME 0
RETURN_VALUE
)"
        );
    }

    TEST_CASE("值相同的常量共用一个槽") {
        CHECK(constants_of(U"x = [1, 1, 1]\nx") == std::vector<std::string>{"int 1"});
        CHECK(constants_of(U"x = ['a', 'a']\nx") == std::vector<std::string>{"str 'a'"});
    }

    TEST_CASE("去重按结构标识，不按 SL 的 ==") {
        // 1 == True == 1.0 都为真且 hash 相同，但必须占不同的槽，否则 True is 1 会变真
        CHECK(
            constants_of(U"x = [1, True, 1.0]\nx") ==
            std::vector<std::string>{"int 1", "bool True", "decimal 1.0"}
        );
        // 1.5 == 1.50 为真，但表示层面不同，得分开
        CHECK(
            constants_of(U"x = [1.5, 1.50]\nx") ==
            std::vector<std::string>{"decimal 1.5", "decimal 1.50"}
        );
    }

    TEST_CASE("全常量元组整个进常量表，一条 LOAD_CONST 就够") {
        CHECK(
            codegen_instrs(U"x = (1, 2)\nx") ==
            R"(LOAD_CONST 2
STORE_NAME 0
LOAD_NAME 0
RETURN_VALUE
)"
        );
        // 自底向上：元素先各自去重，父项再拿元素的对象身份当键
        CHECK(
            constants_of(U"x = (1, 2)\nx") ==
            std::vector<std::string>{"int 1", "int 2", "tuple (int 1, int 2)"}
        );
    }

    TEST_CASE("嵌套的全常量元组也是一条 LOAD_CONST") {
        CHECK(
            codegen_instrs(U"x = (1, (2, 'a'))\nx") ==
            R"(LOAD_CONST 4
STORE_NAME 0
LOAD_NAME 0
RETURN_VALUE
)"
        );
    }

    TEST_CASE("顶层的元组和它作为别人元素时是同一个槽") {
        // (1,2) 出现两次，去重后只有一份
        const std::vector<std::string> consts{constants_of(U"x = [(1, 2), ((1, 2), 3)]\nx")};
        CHECK(std::count(consts.begin(), consts.end(), "tuple (int 1, int 2)") == 1);
    }

    TEST_CASE("有一个元素不是常量，整个元组就得现建") {
        CHECK(codegen_instrs(U"x = 1\n(x, 1)") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
LOAD_CONST 0
MAKE_TUPLE 2
RETURN_VALUE
)");
    }

    TEST_CASE("list 每次求值都得是新对象，元素全是常量也不进常量表") {
        CHECK(codegen_instrs(U"x = [1, 2]\nx") == R"(LOAD_CONST 0
LOAD_CONST 1
MAKE_LIST 2
STORE_NAME 0
LOAD_NAME 0
RETURN_VALUE
)");
        CHECK(constants_of(U"x = [1, 2]\nx") == std::vector<std::string>{"int 1", "int 2"});
    }

    TEST_CASE("没人要值的纯字面量不发指令") {
        // 前一条的值没人用，而字面量本身没有副作用，那一步是 codegen 自己垫的，可以省
        CHECK(codegen_instrs(U"x = 1\nx\nx") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
POP_TOP
LOAD_NAME 0
RETURN_VALUE
)");
    }

} // TEST_SUITE
