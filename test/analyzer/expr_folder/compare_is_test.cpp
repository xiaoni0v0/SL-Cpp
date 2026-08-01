// StaticEvaler/ExprFolder：比较运算（含链式）。is 不参与折叠（对象同一性没法
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
                      {"items", {nlohmann::json{{"key", int_lit("1")}, {"value", int_lit("2")}}}}
                  },
                  nlohmann::json{
                      {"type", "LiteralDict"},
                      {"items", {nlohmann::json{{"key", int_lit("1")}, {"value", int_lit("2")}}}}
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

    TEST_CASE(
        "链式比较里某一环类型不可比：跟含变量的情况一样部分折叠，前面已确定 True 的前缀照样丢"
    ) {
        // 1 < 2 < 'a' 等价于 (1<2) and (2<'a')，1<2 恒为 True 且无副作用可以安全丢掉；
        // 2 < 'a' 没法在编译期判定（不是不知道，是这俩类型压根不可比），折叠应该止步于此，
        // 而不是因为最后一环不可比就放弃整条链的折叠
        CHECK(
            fold_json(U"1 < 2 < 'a'") ==
            nlohmann::json{
                {"type", "Compare"}, {"operands", {int_lit("2"), str_lit("a")}}, {"ops", {"<"}}
            }
        );
        // 第一环就不可比，没有已确定的前缀可丢，整体不折（跟单环 1 < 'a' 不折是同一个道理）
        CHECK(
            fold_json(U"1 < 'a' < 2") ==
            nlohmann::json{
                {"type", "Compare"},
                {"operands", {int_lit("1"), str_lit("a"), int_lit("2")}},
                {"ops", {"<", "<"}}
            }
        );
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

    TEST_CASE(
        "链式比较短路折叠：前面已经确定为 False，后面的非字面量（含调用）不会被求值，直接折成 False"
    ) {
        // 1 < 0 < f()：短路语义等价于 (1<0) and (0<f())，第一环 1<0 就是 False，
        // 短路之后 0<f() 根本不会被求值，f() 也就不会被调用——折叠应该能利用这一点，
        // 不需要 f() 本身是字面量就能把整条链直接折成 False
        CHECK(fold_json(U"1 < 0 < f()") == bool_lit(false));
        // 同理，非字面量出现在更靠前的位置也一样：1 > 2 已经确定 False，后面的 x 不会被摸到
        CHECK(fold_json(U"1 > 2 < x") == bool_lit(false));
    }

    TEST_CASE("链式比较部分折叠：确定为 True 的字面量前缀可以安全丢弃，只保留没法判定的子链") {
        // 1 < 2 < x 等价于 (1<2) and (2<x)，1<2 恒为 True 且无副作用，可以丢掉，
        // 折叠结果应该是缩短后的子链 2 < x，而不是完全不折
        CHECK(
            fold_json(U"1 < 2 < x") ==
            nlohmann::json{
                {"type", "Compare"},
                {"operands", {int_lit("2"), {{"type", "Identifier"}, {"identifier", "x"}}}},
                {"ops", {"<"}}
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
