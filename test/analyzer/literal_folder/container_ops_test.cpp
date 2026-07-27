// StaticEvaler/LiteralFolder：str/tuple/list 的 +（拼接）/*（重复），SL.md 3.4.2。
// dict 的一切运算（含 |）不参与折叠——本质上依赖 VM 才能算，见 StaticEvaler.h 类注释。
//
// 两条额外的安全限制（StaticEvaler.h 类注释也有记录）：
// - 拼接/重复的结果大小超过上限（str 是 kMaxStrLength=4096，tuple/list 是
//   kMaxContainerItems=256）不折，防止几个字符的源码在编译期就材料化出巨大的常量；
// - tuple 的 * 重复额外要求内容"深度不可变"（递归展开后不含任何 list）——因为重复出来的
//   每一份内部元素是共享引用（SL.md 3.4.2），一旦嵌套了可变的 list，"共享 vs 独立拷贝"
//   就变得可观察，折叠没法在不知道以后语义怎么实现的情况下瞎猜；list 本身永远可变，* 重复
//   恒不折。
#include "../test_utils.h"

#include <doctest/doctest.h>

namespace {
// 构造一个长度为 n 的字符串字面量源码（比如 U"'aaa...a'"）
std::u32string str_literal_source(const size_t n) { return U"'" + std::u32string(n, U'a') + U"'"; }

// 构造一个含 n 个整数元素的 tuple/list 字面量源码，比如 n=3 -> U"(0, 0, 0)" 或 U"[0, 0, 0]"
std::u32string container_literal_source(const size_t n, const char32_t open, const char32_t close) {
    std::u32string result(1, open);
    for (size_t i{0}; i < n; ++i) {
        if (i != 0) result += U", ";
        result += U"0";
    }
    if (n == 1 && open == U'(') result += U","; // 单元素 tuple 必须有尾逗号
    result += close;
    return result;
}
} // namespace

TEST_SUITE("StaticEvaler 容器运算——基本拼接/重复") {

    TEST_CASE("str 拼接与重复") {
        CHECK(fold_json(U"'ab' + 'cd'") == str_lit("abcd"));
        CHECK(fold_json(U"'ab' * 3") == str_lit("ababab"));
        CHECK(fold_json(U"3 * 'ab'") == str_lit("ababab"));
        CHECK(fold_json(U"'x' * 0") == str_lit(""));
    }

    TEST_CASE("tuple 拼接与重复（内容都是不可变的 int，深度不可变，能折）") {
        CHECK(
            fold_json(U"(1,) + (2, 3)") ==
            nlohmann::json{
                {"type", "LiteralTuple"}, {"items", {int_lit("1"), int_lit("2"), int_lit("3")}}
            }
        );
        CHECK(
            fold_json(U"(1, 2) * 2") ==
            nlohmann::json{
                {"type", "LiteralTuple"},
                {"items", {int_lit("1"), int_lit("2"), int_lit("1"), int_lit("2")}}
            }
        );
        CHECK(
            fold_json(U"(1, 2) * 0") ==
            nlohmann::json{{"type", "LiteralTuple"}, {"items", nlohmann::json::array()}}
        );
    }

    TEST_CASE("list 拼接能折（+ 只是把子节点原样接过去，不涉及复制/共享，不需要门槛）") {
        CHECK(
            fold_json(U"[1, 2] + [3]") ==
            nlohmann::json{
                {"type", "LiteralList"}, {"items", {int_lit("1"), int_lit("2"), int_lit("3")}}
            }
        );
    }

    TEST_CASE("元素里含变量的容器不算纯字面量，+/* 不折") {
        CHECK(
            fold_json(U"[x, 1] + [2]") ==
            nlohmann::json{
                {"type", "OpBinary"},
                {"op", "+"},
                {"left",
                 {{"type", "LiteralList"},
                  {"items", {{{"type", "Identifier"}, {"identifier", "x"}}, int_lit("1")}}}},
                {"right", {{"type", "LiteralList"}, {"items", {int_lit("2")}}}}
            }
        );
    }

    TEST_CASE("负数重复次数不折，交给运行时报错") {
        CHECK(
            fold_json(U"(1,) * -1") ==
            nlohmann::json{
                {"type", "OpBinary"},
                {"op", "*"},
                {"left", {{"type", "LiteralTuple"}, {"items", {int_lit("1")}}}},
                {"right", int_lit("-1")}
            }
        );
    }

    TEST_CASE("类型不匹配的 + 不折（tuple 和 list 不能互相拼接）") {
        CHECK(
            fold_json(U"(1,) + [2]") ==
            nlohmann::json{
                {"type", "OpBinary"},
                {"op", "+"},
                {"left", {{"type", "LiteralTuple"}, {"items", {int_lit("1")}}}},
                {"right", {{"type", "LiteralList"}, {"items", {int_lit("2")}}}}
            }
        );
    }

    TEST_CASE("dict 的 | 不折，交给运行时") {
        CHECK(
            fold_json(U"{1: 2} | {1: 3, 4: 5}") ==
            nlohmann::json{
                {"type", "OpBinary"},
                {"op", "|"},
                {"left",
                 {{"type", "LiteralDict"},
                  {"items", {nlohmann::json{{"key", int_lit("1")}, {"value", int_lit("2")}}}}}},
                {"right",
                 {{"type", "LiteralDict"},
                  {"items",
                   {nlohmann::json{{"key", int_lit("1")}, {"value", int_lit("3")}},
                    nlohmann::json{{"key", int_lit("4")}, {"value", int_lit("5")}}}}}}
            }
        );
    }
}

TEST_SUITE("StaticEvaler 容器运算——list 的 * 恒不折") {

    TEST_CASE("list * n 永远不折，即使内容全是不可变的 int 也一样") {
        CHECK(
            fold_json(U"[0] * 3") ==
            nlohmann::json{
                {"type", "OpBinary"},
                {"op", "*"},
                {"left", {{"type", "LiteralList"}, {"items", {int_lit("0")}}}},
                {"right", int_lit("3")}
            }
        );
        CHECK(
            fold_json(U"3 * []") ==
            nlohmann::json{
                {"type", "OpBinary"},
                {"op", "*"},
                {"left", int_lit("3")},
                {"right", {{"type", "LiteralList"}, {"items", nlohmann::json::array()}}}
            }
        );
    }
}

TEST_SUITE("StaticEvaler 容器运算——tuple 的 * 要求内容深度不可变（SL.md 3.4.2 的共享引用语义）") {

    TEST_CASE("纯 int/float/str/bool/None/Ellipsis 组成的 tuple：深度不可变，能折") {
        CHECK(
            fold_json(U"(1, 2) * 3") == nlohmann::json{
                                            {"type", "LiteralTuple"},
                                            {"items",
                                             {int_lit("1"),
                                              int_lit("2"),
                                              int_lit("1"),
                                              int_lit("2"),
                                              int_lit("1"),
                                              int_lit("2")}}
                                        }
        );
        CHECK(
            fold_json(U"(1.5, 'a', True, None, ...) * 2") ==
            nlohmann::json{
                {"type", "LiteralTuple"},
                {"items",
                 {float_lit("1.5"),
                  str_lit("a"),
                  bool_lit(true),
                  none_lit(),
                  nlohmann::json{{"type", "LiteralEllipsis"}},
                  float_lit("1.5"),
                  str_lit("a"),
                  bool_lit(true),
                  none_lit(),
                  nlohmann::json{{"type", "LiteralEllipsis"}}}}
            }
        );
    }

    TEST_CASE("(...,) * 3：单元素 Ellipsis 的 tuple，Ellipsis 是不可变字面量，能折") {
        CHECK(
            fold_json(U"(...,) * 3") == nlohmann::json{
                                            {"type", "LiteralTuple"},
                                            {"items",
                                             {nlohmann::json{{"type", "LiteralEllipsis"}},
                                              nlohmann::json{{"type", "LiteralEllipsis"}},
                                              nlohmann::json{{"type", "LiteralEllipsis"}}}}
                                        }
        );
    }

    TEST_CASE("嵌套 tuple-in-tuple，只要一路都是 tuple/不可变基例，深度不可变，能折") {
        CHECK(
            fold_json(U"((1, 2), 3) * 2") ==
            nlohmann::json{
                {"type", "LiteralTuple"},
                {"items",
                 {nlohmann::json{{"type", "LiteralTuple"}, {"items", {int_lit("1"), int_lit("2")}}},
                  int_lit("3"),
                  nlohmann::json{{"type", "LiteralTuple"}, {"items", {int_lit("1"), int_lit("2")}}},
                  int_lit("3")}}
            }
        );
    }

    TEST_CASE("嵌套了 list（哪怕只有一层深）就不是深度不可变，* 不折") {
        CHECK(
            fold_json(U"([1], 2) * 3") ==
            nlohmann::json{
                {"type", "OpBinary"},
                {"op", "*"},
                {"left",
                 {{"type", "LiteralTuple"},
                  {"items", {{{"type", "LiteralList"}, {"items", {int_lit("1")}}}, int_lit("2")}}}},
                {"right", int_lit("3")}
            }
        );
    }

    TEST_CASE("list 藏得再深也一样：tuple 套 tuple 套 list，仍然不是深度不可变") {
        CHECK(
            fold_json(U"((1, [2]),) * 3") ==
            nlohmann::json{
                {"type", "OpBinary"},
                {"op", "*"},
                {"left",
                 {{"type", "LiteralTuple"},
                  {"items",
                   {{{"type", "LiteralTuple"},
                     {"items",
                      {int_lit("1"), {{"type", "LiteralList"}, {"items", {int_lit("2")}}}}}}}}}},
                {"right", int_lit("3")}
            }
        );
    }

    TEST_CASE("空 tuple 深度不可变恒成立，* 能折成空 tuple") {
        CHECK(
            fold_json(U"() * 100") ==
            nlohmann::json{{"type", "LiteralTuple"}, {"items", nlohmann::json::array()}}
        );
    }
}

TEST_SUITE("StaticEvaler 容器运算——大小上限（kMaxStrLength=4096, kMaxContainerItems=256）") {

    TEST_CASE("str + 恰好等于上限折，超一个字符不折") {
        CHECK(
            fold_json(str_literal_source(2048) + U" + " + str_literal_source(2048)) ==
            str_lit(std::string(4096, 'a'))
        );
        CHECK(
            fold_json(str_literal_source(2048) + U" + " + str_literal_source(2049))["type"] ==
            "OpBinary"
        );
    }

    TEST_CASE("str * 恰好等于上限折，超一个字符不折") {
        CHECK(fold_json(U"'a' * 4096") == str_lit(std::string(4096, 'a')));
        CHECK(fold_json(U"'a' * 4097")["type"] == "OpBinary");
    }

    // 注意：这里存中间结果必须用 `= fold_json(...)` 而不是 `{fold_json(...)}`——nlohmann::json
    // 有 initializer_list 构造函数，`auto j{已经是个 json 的值}` 这种写法会被当成"用这一个元素
    // 构造数组"，把本该是的 object 包成一个只有一个元素的 array（跟前面遇到过的
    // `nlohmann::json{{"items", {}}}` 空数组坑是同一类问题）
    TEST_CASE("tuple + 恰好等于上限折，超一个元素不折") {
        const auto j256 = fold_json(
            container_literal_source(128, U'(', U')') + U" + " +
            container_literal_source(128, U'(', U')')
        );
        CHECK(j256["type"] == "LiteralTuple");
        REQUIRE(j256.contains("items"));
        CHECK(j256["items"].size() == 256);

        const auto j257 = fold_json(
            container_literal_source(128, U'(', U')') + U" + " +
            container_literal_source(129, U'(', U')')
        );
        CHECK(j257["type"] == "OpBinary"); // 257 个元素，超限不折
    }

    TEST_CASE("tuple * 恰好等于上限折，超一个元素不折（内容是 int，深度不可变，门槛能过）") {
        const auto j256 = fold_json(container_literal_source(1, U'(', U')') + U" * 256");
        CHECK(j256["type"] == "LiteralTuple");
        REQUIRE(j256.contains("items"));
        CHECK(j256["items"].size() == 256);

        const auto j257 = fold_json(container_literal_source(1, U'(', U')') + U" * 257");
        CHECK(j257["type"] == "OpBinary");
    }

    TEST_CASE("list + 恰好等于上限折，超一个元素不折") {
        const auto j256 = fold_json(
            container_literal_source(128, U'[', U']') + U" + " +
            container_literal_source(128, U'[', U']')
        );
        CHECK(j256["type"] == "LiteralList");
        REQUIRE(j256.contains("items"));
        CHECK(j256["items"].size() == 256);

        const auto j257 = fold_json(
            container_literal_source(128, U'[', U']') + U" + " +
            container_literal_source(129, U'[', U']')
        );
        CHECK(j257["type"] == "OpBinary");
    }
}
