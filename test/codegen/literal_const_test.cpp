#include "test_utils.h"

#include <doctest/doctest.h>

// 注意两点：单独一条纯字面量语句会被折叠器当死代码剪掉；Program 里每一条都不产出值。
// 所以下面一律写成 "x = 字面量"——赋值的右侧才是 want_value=true 的位置。
// codegen_body 去掉了每份 Program 末尾那条隐含的 return None，它由 program_value_test 专门验。

TEST_SUITE("codegen: 字面量与常量表") {

    TEST_CASE("字面量一律走 LOAD_CONST") {
        const std::string tail{"LOAD_CONST 0\nSTORE_NAME 0\n"};
        CHECK(codegen_body(U"x = 1") == tail);
        CHECK(codegen_body(U"x = 'hi'") == tail);
        CHECK(codegen_body(U"x = 1.5") == tail);
    }

    TEST_CASE("None / True / False / Ellipsis 也不例外") {
        // 末尾隐含的 return None 用的就是同一个 None 槽
        CHECK(constants_of(U"x = None") == std::vector<std::string>{"None"});
        CHECK(constants_of(U"x = True") == std::vector<std::string>{"bool True", "None"});
        CHECK(constants_of(U"x = False") == std::vector<std::string>{"bool False", "None"});
        CHECK(constants_of(U"x = ...") == std::vector<std::string>{"Ellipsis", "None"});
    }

    TEST_CASE("_G / _L 压的是字典本身，不进常量表") {
        CHECK(codegen_body(U"x = _L") == R"(LOAD_GL 0
STORE_NAME 0
)");
        CHECK(codegen_body(U"x = _G") == R"(LOAD_GL 1
STORE_NAME 0
)");
    }

    TEST_CASE("值相同的常量共用一个槽") {
        CHECK(constants_of(U"x = [1, 1, 1]") == std::vector<std::string>{"int 1", "None"});
        CHECK(constants_of(U"x = ['a', 'a']") == std::vector<std::string>{"str 'a'", "None"});
    }

    TEST_CASE("去重按结构标识，不按 SL 的 ==") {
        // 1 == True == 1.0 都为真且 hash 相同，但必须占不同的槽，否则 True is 1 会变真
        CHECK(
            constants_of(U"x = [1, True, 1.0]") ==
            std::vector<std::string>{"int 1", "bool True", "decimal 1.0", "None"}
        );
        // 1.5 == 1.50 为真，但表示层面不同，得分开
        CHECK(
            constants_of(U"x = [1.5, 1.50]") ==
            std::vector<std::string>{"decimal 1.5", "decimal 1.50", "None"}
        );
    }

    TEST_CASE("全常量元组整个进常量表，一条 LOAD_CONST 就够") {
        CHECK(codegen_body(U"x = (1, 2)") == R"(LOAD_CONST 2
STORE_NAME 0
)");
        // 自底向上：元素先各自去重，父项再拿元素的对象身份当键
        CHECK(
            constants_of(U"x = (1, 2)") ==
            std::vector<std::string>{"int 1", "int 2", "tuple (int 1, int 2)", "None"}
        );
    }

    TEST_CASE("嵌套的全常量元组也是一条 LOAD_CONST") {
        CHECK(codegen_body(U"x = (1, (2, 'a'))") == R"(LOAD_CONST 4
STORE_NAME 0
)");
    }

    TEST_CASE("顶层的元组和它作为别人元素时是同一个槽") {
        // (1,2) 出现两次，去重后只有一份
        const std::vector<std::string> consts{constants_of(U"x = [(1, 2), ((1, 2), 3)]")};
        CHECK(std::count(consts.begin(), consts.end(), "tuple (int 1, int 2)") == 1);
    }

    TEST_CASE("有一个元素不是常量，整个元组就得现建") {
        CHECK(codegen_body(U"x = 1\ny = (x, 1)") == R"(LOAD_CONST 0
STORE_NAME 0
LOAD_NAME 0
LOAD_CONST 0
MAKE_TUPLE 2
STORE_NAME 1
)");
    }

    TEST_CASE("list 每次求值都得是新对象，元素全是常量也不进常量表") {
        CHECK(codegen_body(U"x = [1, 2]") == R"(LOAD_CONST 0
LOAD_CONST 1
MAKE_LIST 2
STORE_NAME 0
)");
        CHECK(constants_of(U"x = [1, 2]") == std::vector<std::string>{"int 1", "int 2", "None"});
    }

} // TEST_SUITE
