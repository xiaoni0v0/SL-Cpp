// str/tuple/list 的 + 拼接与 * 重复。
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

    TEST_CASE("str % 格式化不折") {
        CHECK(fold_json(U"'%s' % 'a'")["type"] == "OpBinary");
        CHECK(fold_json(U"'%s' % 'a'")["op"] == "%");
    }

    TEST_CASE("tuple 拼接（+）能折；* 重复见下面单独的 TEST_SUITE") {
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

    // 容器重复次数必须是非负 int；bool 不继承 int，当重复次数是 TypeError，不折。
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

// SL.md 4.2「x * y」规定重复出来的各份中对应位置是同一个引用。折成字面量之后，这一条由 codegen
// 的常量去重兑现：n 份克隆出来的字面量节点各自 intern，结构相同就是同一个槽、同一个对象
// （codegen/bytecode.md「常量去重」）。所以判据落在**元素能不能进常量表**上——list/dict 每次求值
// 都新建一个，含它们的容器折了就会各份各自新建，`x[0] is x[2]` 从真变假。
TEST_SUITE("StaticEvaler 容器运算——tuple/list 的 * 重复（元素能进常量表才折）") {

    TEST_CASE("tuple * n：元素全是能进常量表的字面量就折") {
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
            fold_json(U"3 * (1,)") == // 次数在左也一样
            nlohmann::json{
                {"type", "LiteralTuple"}, {"items", {int_lit("1"), int_lit("1"), int_lit("1")}}
            }
        );
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

    TEST_CASE("tuple * n：不可变基例混在一起也折") {
        const auto j = fold_json(U"(1.5, 'a', True, None, ...) * 2");
        CHECK(j["type"] == "LiteralTuple");
        REQUIRE(j.contains("items"));
        REQUIRE(j["items"].size() == 10);
        CHECK(j["items"][0] == decimal_lit("1.5"));
        CHECK(j["items"][1] == str_lit("a"));
        CHECK(j["items"][2] == bool_lit(true));
        CHECK(j["items"][3] == none_lit());
        CHECK(j["items"][4] == nlohmann::json{{"type", "LiteralEllipsis"}});
        for (size_t i{0}; i < 5; ++i) CHECK(j["items"][i] == j["items"][i + 5]);
    }

    TEST_CASE("嵌套 tuple-in-tuple 折：内层元组也进常量表，各份是同一个对象") {
        const nlohmann::json inner{
            {"type", "LiteralTuple"}, {"items", {int_lit("1"), int_lit("2")}}
        };
        CHECK(
            fold_json(U"((1, 2), 3) * 2") ==
            nlohmann::json{
                {"type", "LiteralTuple"}, {"items", {inner, int_lit("3"), inner, int_lit("3")}}
            }
        );
    }

    TEST_CASE("list * n 同样折：列表自己每次求值都新建，重复的只是元素") {
        CHECK(
            fold_json(U"[0] * 3") ==
            nlohmann::json{
                {"type", "LiteralList"}, {"items", {int_lit("0"), int_lit("0"), int_lit("0")}}
            }
        );
        CHECK(
            fold_json(U"2 * [1, 'a']") ==
            nlohmann::json{
                {"type", "LiteralList"},
                {"items", {int_lit("1"), str_lit("a"), int_lit("1"), str_lit("a")}}
            }
        );
    }

    // 这几条是这一整套判据的底线：元素只要有一个进不了常量表，折出来的各份就会各自新建，
    // `x[0] is x[1]` 从真变假。递归到任意深度都要挡住
    TEST_CASE("元素里有 list 就不折") {
        CHECK(fold_json(U"([1], 2) * 3")["type"] == "OpBinary");
        CHECK(fold_json(U"[[1]] * 2")["type"] == "OpBinary");
        CHECK(fold_json(U"[[], 1] * 2")["type"] == "OpBinary");
    }

    TEST_CASE("list 藏在深处也不折") {
        CHECK(fold_json(U"(((1, [2]), 3),) * 2")["type"] == "OpBinary");
        CHECK(fold_json(U"([([1],)],) * 2")["type"] == "OpBinary");
    }

    TEST_CASE("元素里有 dict 就不折") {
        CHECK(fold_json(U"({'a': 1},) * 2")["type"] == "OpBinary");
        CHECK(fold_json(U"[{'a': 1}] * 2")["type"] == "OpBinary");
    }

    TEST_CASE("元素不是纯字面量（标识符等）当然也不折") {
        CHECK(fold_json(U"(x,) * 2")["type"] == "OpBinary");
        CHECK(fold_json(U"[f()] * 2")["type"] == "OpBinary");
    }

    // 空容器重复走的是单独一条提前返回：上限检查算的是 元素数 * 次数，空容器恒为 0、永远不超限，
    // 于是次数再大也拦不住，只能靠"空的重复多少次还是空的"提前返回。跟空串那条是同一个形状，
    // 回归失败形态同样是**测试跑不完**而不是断言红
    TEST_CASE("空容器重复任意次都还是空的，且不按次数空转") {
        const nlohmann::json empty_tuple{
            {"type", "LiteralTuple"}, {"items", nlohmann::json::array()}
        };
        const nlohmann::json empty_list{
            {"type", "LiteralList"}, {"items", nlohmann::json::array()}
        };
        CHECK(fold_json(U"() * 0") == empty_tuple);
        CHECK(fold_json(U"() * 100") == empty_tuple);
        CHECK(fold_json(U"() * 9223372036854775807") == empty_tuple);
        CHECK(fold_json(U"9223372036854775807 * ()") == empty_tuple);
        CHECK(fold_json(U"3 * []") == empty_list);
        CHECK(fold_json(U"[] * 9223372036854775807") == empty_list);
    }

    TEST_CASE("重复 0 次折成空容器") {
        CHECK(
            fold_json(U"(1, 2) * 0") ==
            nlohmann::json{{"type", "LiteralTuple"}, {"items", nlohmann::json::array()}}
        );
        CHECK(
            fold_json(U"[1, 2] * 0") ==
            nlohmann::json{{"type", "LiteralList"}, {"items", nlohmann::json::array()}}
        );
    }

    // 次数的类型/符号判据跟 str 重复共用同一段代码，这里钉住它对容器也生效
    TEST_CASE("次数不是非负 int 就不折") {
        CHECK(fold_json(U"(1,) * -1")["type"] == "OpBinary");   // 负数运行期报错，留给运行期
        CHECK(fold_json(U"(1,) * True")["type"] == "OpBinary"); // bool 不当重复次数
        CHECK(fold_json(U"(1,) * 1.5")["type"] == "OpBinary");
        CHECK(fold_json(U"[1] * -1")["type"] == "OpBinary");
    }

    TEST_CASE("次数大到 int64_t 装不下就不折（非空容器）") {
        CHECK(fold_json(U"(0,) * 99999999999999999999")["type"] == "OpBinary");
    }
}

TEST_SUITE("StaticEvaler 容器运算——大小上限（nMaxStrLength=4096, nMaxContainerItems=256）") {

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

    // 空串重复走的是单独一条路：上限检查算的是 长度 * 次数，空串的长度是 0、乘出来恒为 0，永远
    // 不超限，于是次数再大也拦不住，只能靠"空串重复还是空串"提前返回。
    //
    // 最后两条特意取 int64_t 上界：折叠器一旦丢掉那个提前返回，就会退化成按次数逐轮拼接，这两条
    // 直接跑不完。也就是说这里的回归失败形态是**测试跑不动**而不是断言红——没有不靠计时就能断言
    // "没有空转 n 轮"的写法，取一个大到跑不完的次数是最直白的钉法。
    TEST_CASE("空串重复任意次都折成空串，且不受上限影响、不按次数空转") {
        CHECK(fold_json(U"'' * 0") == str_lit(""));
        CHECK(fold_json(U"'' * 1") == str_lit(""));
        CHECK(fold_json(U"'' * 4097") == str_lit("")); // 超过 nMaxStrLength，非空串这里就不折了
        CHECK(fold_json(U"'' * 9223372036854775807") == str_lit(""));
        CHECK(fold_json(U"9223372036854775807 * ''") == str_lit("")); // 次数在左也一样
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

    // 上限算的是结果的元素个数，也就是 元素数 * 次数，两个因子都要能踩到边界
    TEST_CASE("tuple * 恰好等于上限折，超一个元素不折") {
        const auto j256 = fold_json(U"(0,) * 256");
        CHECK(j256["type"] == "LiteralTuple");
        REQUIRE(j256.contains("items"));
        CHECK(j256["items"].size() == 256);
        CHECK(fold_json(U"(0,) * 257")["type"] == "OpBinary");

        const auto j256b = fold_json(container_literal_source(128, U'(', U')') + U" * 2");
        CHECK(j256b["type"] == "LiteralTuple");
        REQUIRE(j256b.contains("items"));
        CHECK(j256b["items"].size() == 256);
        CHECK(fold_json(container_literal_source(129, U'(', U')') + U" * 2")["type"] == "OpBinary");
    }

    TEST_CASE("list * 恰好等于上限折，超一个元素不折") {
        const auto j256 = fold_json(U"[0] * 256");
        CHECK(j256["type"] == "LiteralList");
        REQUIRE(j256.contains("items"));
        CHECK(j256["items"].size() == 256);
        CHECK(fold_json(U"[0] * 257")["type"] == "OpBinary");
    }
}
