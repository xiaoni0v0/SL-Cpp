// SL.md 2.2.5.6 return 表达式：return [expr]
#include "../../../builtins/exceptions/SyntaxError.h"
#include "../test_utils.h"

#include <doctest/doctest.h>

namespace {
nlohmann::json int_lit(const char *raw) {
    return nlohmann::json::parse(R"({"type":"LiteralInt","raw":")" + std::string{raw} + R"("})");
}
} // namespace

TEST_SUITE("2.2.5.6 return") {

    TEST_CASE("裸 return，值为 None（value 为 null）") {
        CHECK(parse_json(U"return") == nlohmann::json{{"type", "Return"}, {"value", nullptr}});
    }

    TEST_CASE("带值") {
        CHECK(
            parse_json(U"return 5") == nlohmann::json{{"type", "Return"}, {"value", int_lit("5")}}
        );
    }

    TEST_CASE("带值是复杂表达式") {
        CHECK(
            parse_json(U"return 1 + 2") == nlohmann::json{
                                               {"type", "Return"},
                                               {"value",
                                                {{"type", "OpBinary"},
                                                 {"op", "+"},
                                                 {"left", int_lit("1")},
                                                 {"right", int_lit("2")}}}
                                           }
        );
    }

    TEST_CASE("裸 return 后面紧跟 ';' 也算裸 return") {
        CHECK(
            parse_program_json(U"return; 1") ==
            nlohmann::json{
                {"type", "Program"},
                {"exprs",
                 nlohmann::json::array({{{"type", "Return"}, {"value", nullptr}}, int_lit("1")})}
            }
        );
    }

    TEST_CASE(
        "裸 return 出现在括号/中括号/逗号语境时也能正确识别为裸 return（不会误吞后面的符号）"
    ) {
        CHECK(
            parse_json(U"(return,)") ==
            nlohmann::json{
                {"type", "LiteralTuple"},
                {"items", nlohmann::json::array({{{"type", "Return"}, {"value", nullptr}}})}
            }
        );
        CHECK(
            parse_json(U"[return]") ==
            nlohmann::json{
                {"type", "LiteralList"},
                {"items", nlohmann::json::array({{{"type", "Return"}, {"value", nullptr}}})}
            }
        );
        CHECK(
            parse_json(U"f(return)") ==
            nlohmann::json{
                {"type", "Call"},
                {"object", {{"type", "Identifier"}, {"identifier", "f"}}},
                {"args", nlohmann::json::array({{{"type", "Return"}, {"value", nullptr}}})},
                {"kwargs", nlohmann::json::array()}
            }
        );
    }
}
