// StaticEvaler/ExprFolder：str/tuple/list 的 +（拼接）/*（重复）。
// dict 的一切运算（含 |）不参与折叠——本质上依赖 VM 才能算，见 StaticEvaler.h 类注释。
//
// 两条额外的安全限制/规则（StaticEvaler.h 类注释也有记录）：
// - 拼接的结果大小超过上限（str 是 kMaxStrLength=4096，tuple/list 是 kMaxContainerItems=256）
//   不折，防止几个字符的源码在编译期就材料化出巨大的常量；* 重复只有 str 会折，同样受
//   kMaxStrLength 限制。
// - tuple/list 的 * 重复恒不折：SL.md 规定重复出来的各份中，对应位置的元素是同一个
//   引用而非独立副本，折叠只能靠深拷贝伪造出 N 份独立子树，这跟 is 恒不折是同一类顾虑
//   （对象同一性没法在编译期安全预判），不因为内容不可变就能豁免——这也是为什么这条规则
//   不再区分"tuple 内容是否深度不可变"：不管内容是什么，* 都不折。
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

    TEST_CASE("tuple 拼接（+）能折；* 重复恒不折，见下面单独的 TEST_SUITE") {
        CHECK(
            fold_json(U"(1,) + (2, 3)") ==
            nlohmann::json{
                {"type", "LiteralTuple"}, {"items", {int_lit("1"), int_lit("2"), int_lit("3")}}
            }
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

    // 容器重复只对"非负的 int"有定义（见 SL.md 的运算符表达式的值一节），bool 不继承 int
    // （见 SL.md 的 bool 内置类一节），
    // 因此 bool 当重复次数是运行时 TypeError，不折。
    TEST_CASE("bool 当重复次数不折，交给运行时报错") {
        CHECK(
            fold_json(U"'ab' * True") == nlohmann::json{
                                             {"type", "OpBinary"},
                                             {"op", "*"},
                                             {"left", str_lit("ab")},
                                             {"right", bool_lit(true)}
                                         }
        );
        CHECK(
            fold_json(U"True * 'ab'") == nlohmann::json{
                                             {"type", "OpBinary"},
                                             {"op", "*"},
                                             {"left", bool_lit(true)},
                                             {"right", str_lit("ab")}
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

    TEST_CASE("dict 自身恒不折，但每一项的 key/value 子表达式仍然各自照常递归折叠") {
        CHECK(
            fold_json(U"{1 + 1: 2 + 2}") ==
            nlohmann::json{
                {"type", "LiteralDict"},
                {"items", {nlohmann::json{{"key", int_lit("2")}, {"value", int_lit("4")}}}}
            }
        );
        // **展开项：key 是 DoubleStar 节点，value 恒为 null；跟普通 k: v 项混在一起，
        // 各自独立折叠、互不影响
        CHECK(
            fold_json(U"{**d, 1 + 1: 2 + 2}") ==
            nlohmann::json{
                {"type", "LiteralDict"},
                {"items",
                 {nlohmann::json{
                      {"key",
                       {{"type", "DoubleStar"},
                        {"operand", {{"type", "Identifier"}, {"identifier", "d"}}}}},
                      {"value", nullptr}
                  },
                  nlohmann::json{{"key", int_lit("2")}, {"value", int_lit("4")}}}}
            }
        );
    }
}

TEST_SUITE(
    "StaticEvaler 容器运算——tuple/list 的 * 恒不折（重复出来的各份共享引用，"
    "折叠只能靠深拷贝伪造，这跟 is 恒不折是同一类顾虑，不因内容/是否可变而有区别）"
) {

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

    TEST_CASE(
        "tuple * n 恒不折：纯 int/decimal/str/bool/None/Ellipsis 组成也一样（以前的版本会因为"
        "'内容深度不可变'而折，这是已经改掉的错误行为）"
    ) {
        CHECK(
            fold_json(U"(1, 2) * 3") ==
            nlohmann::json{
                {"type", "OpBinary"},
                {"op", "*"},
                {"left", {{"type", "LiteralTuple"}, {"items", {int_lit("1"), int_lit("2")}}}},
                {"right", int_lit("3")}
            }
        );
        CHECK(
            fold_json(U"(1.5, 'a', True, None, ...) * 2") ==
            nlohmann::json{
                {"type", "OpBinary"},
                {"op", "*"},
                {"left",
                 {{"type", "LiteralTuple"},
                  {"items",
                   {decimal_lit("1.5"),
                    str_lit("a"),
                    bool_lit(true),
                    none_lit(),
                    nlohmann::json{{"type", "LiteralEllipsis"}}}}}},
                {"right", int_lit("2")}
            }
        );
    }

    TEST_CASE("(...,) * 3：单元素 Ellipsis 的 tuple，同样不折") {
        CHECK(
            fold_json(U"(...,) * 3") ==
            nlohmann::json{
                {"type", "OpBinary"},
                {"op", "*"},
                {"left",
                 {{"type", "LiteralTuple"},
                  {"items", {nlohmann::json{{"type", "LiteralEllipsis"}}}}}},
                {"right", int_lit("3")}
            }
        );
    }

    TEST_CASE("嵌套 tuple-in-tuple，哪怕一路都是不可变基例，同样不折") {
        CHECK(
            fold_json(U"((1, 2), 3) * 2") ==
            nlohmann::json{
                {"type", "OpBinary"},
                {"op", "*"},
                {"left",
                 {{"type", "LiteralTuple"},
                  {"items",
                   {nlohmann::json{
                        {"type", "LiteralTuple"}, {"items", {int_lit("1"), int_lit("2")}}
                    },
                    int_lit("3")}}}},
                {"right", int_lit("2")}
            }
        );
    }

    TEST_CASE(
        "tuple 里嵌套了 list（不管多深）同样不折——现在这条已经不是重点：反正 tuple 本身也"
        "恒不折，嵌不嵌 list 都一样"
    ) {
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

    TEST_CASE("空 tuple 也不折（跟内容无关，* 对 tuple 就是恒不折）") {
        CHECK(
            fold_json(U"() * 100") ==
            nlohmann::json{
                {"type", "OpBinary"},
                {"op", "*"},
                {"left", {{"type", "LiteralTuple"}, {"items", nlohmann::json::array()}}},
                {"right", int_lit("100")}
            }
        );
    }

    TEST_CASE("重复次数很大也不折（既然恒不折，就不存在'计算量太大'这回事，n 本身不影响结果）") {
        CHECK(fold_json(U"(0,) * 100000")["type"] == "OpBinary");
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
