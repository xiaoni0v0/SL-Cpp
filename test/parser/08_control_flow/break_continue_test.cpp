// SL.md break/continue 表达式：语法上就是裸关键字。
// "只能在 for/while 的 expr 部分使用" 是语义层（loop_depth 上下文）的校验，不是 parser 的事，
// 所以语法层面 break/continue 出现在任何位置都应该能正常解析出来。
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("break / continue") {

    TEST_CASE("基本形式") {
        CHECK(parse_json(U"break") == nlohmann::json{{"type", "Break"}});
        CHECK(parse_json(U"continue") == nlohmann::json{{"type", "Continue"}});
    }

    TEST_CASE("语法层不检查是否处于循环体内，随便写在哪都能解析成功") {
        CHECK_NOTHROW(parse_program(U"break"));
        CHECK_NOTHROW(parse_program(U"continue"));
    }

    TEST_CASE("出现在循环体内") {
        CHECK(
            parse_json(U"while (c) break") ==
            nlohmann::json{
                {"type", "ForCond"},
                {"collect", false},
                {"init", nullptr},
                {"cond", {{"type", "Identifier"}, {"identifier", "c"}}},
                {"inc", nullptr},
                {"body", {{"type", "Break"}}}
            }
        );
    }
}
