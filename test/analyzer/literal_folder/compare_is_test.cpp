// StaticEvaler/LiteralFolder：比较运算（含链式），SL.md 3.4.2、3.3。is 不参与折叠（对象同一性没法
// 在编译期安全预判，见 StaticEvaler.h 类注释），dict 的一切运算同理不折。
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("StaticEvaler 比较") {

    TEST_CASE("数字之间的大小/相等比较，跨 bool/int/float 提升") {
        CHECK(fold_json(U"1 < 2") == bool_lit(true));
        CHECK(fold_json(U"2 <= 2") == bool_lit(true));
        CHECK(fold_json(U"1 == 1.0") == bool_lit(true));
        CHECK(fold_json(U"True == 1") == bool_lit(true));
        CHECK(fold_json(U"1 != 2") == bool_lit(true));
    }

    TEST_CASE("字符串按字典序比较") {
        CHECK(fold_json(U"'a' < 'b'") == bool_lit(true));
        CHECK(fold_json(U"'ab' < 'b'") == bool_lit(true));
        CHECK(fold_json(U"'a' == 'a'") == bool_lit(true));
    }

    TEST_CASE("tuple/list 逐元素字典序比较") {
        CHECK(fold_json(U"(1, 2) < (1, 3)") == bool_lit(true));
        CHECK(fold_json(U"(1,) < (1, 2)") == bool_lit(true)); // 前缀更小
        CHECK(fold_json(U"[1, 2] == [1, 2]") == bool_lit(true));
        CHECK(fold_json(U"[1, 2] == [1, 3]") == bool_lit(false));
    }

    TEST_CASE("dict 不参与任何折叠，含 dict 操作数的比较也不折") {
        CHECK(
            fold_json(U"{1: 2} == {1: 2}") ==
            nlohmann::json{
                {"type", "Compare"},
                {"operands",
                 {nlohmann::json{
                      {"type", "LiteralDict"},
                      {"items", {nlohmann::json{{"key", int_lit("1")}, {"val", int_lit("2")}}}}
                  },
                  nlohmann::json{
                      {"type", "LiteralDict"},
                      {"items", {nlohmann::json{{"key", int_lit("1")}, {"val", int_lit("2")}}}}
                  }}},
                {"ops", {"=="}}
            }
        );
    }

    TEST_CASE("跨类型比较大小不可比，不折；但 == 恒成立（跨类型必不相等）") {
        CHECK(
            fold_json(U"1 < 'a'") ==
            nlohmann::json{
                {"type", "Compare"}, {"operands", {int_lit("1"), str_lit("a")}}, {"ops", {"<"}}
            }
        );
        CHECK(fold_json(U"1 == 'a'") == bool_lit(false));
        CHECK(fold_json(U"None == 0") == bool_lit(false));
    }

    TEST_CASE("链式比较：全部为真才是 True，中间有一环为假整条链为 False") {
        CHECK(fold_json(U"1 < 2 < 3") == bool_lit(true));
        CHECK(fold_json(U"1 < 2 > 3") == bool_lit(false));
        CHECK(fold_json(U"3 > 2 > 1") == bool_lit(true));
    }

    TEST_CASE("链上含变量不折") {
        CHECK(
            fold_json(U"1 < x < 3") ==
            nlohmann::json{
                {"type", "Compare"},
                {"operands",
                 {int_lit("1"), {{"type", "Identifier"}, {"identifier", "x"}}, int_lit("3")}},
                {"ops", {"<", "<"}}
            }
        );
    }

    TEST_CASE("is 一律不折（对象同一性没法在编译期安全预判，哪怕是 None）") {
        CHECK(
            fold_json(U"None is None") ==
            nlohmann::json{{"type", "Is"}, {"operands", {none_lit(), none_lit()}}}
        );
        CHECK(
            fold_json(U"1 is 1") ==
            nlohmann::json{{"type", "Is"}, {"operands", {int_lit("1"), int_lit("1")}}}
        );
    }
}
