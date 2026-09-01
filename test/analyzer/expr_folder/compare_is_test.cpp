// 比较折叠；is 整体不折。
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("StaticEvaler 比较") {

    TEST_CASE("数字之间的大小/相等比较，跨 bool/int 提升") {
        CHECK(fold_json(U"1 < 2") == bool_lit(true));
        CHECK(fold_json(U"2 <= 2") == bool_lit(true));
        CHECK(fold_json(U"True == 1") == bool_lit(true));
        CHECK(fold_json(U"1 != 2") == bool_lit(true));
    }

    // decimal 一概不折（哪怕是跨类型比较），见 numeric_fidelity_test.cpp
    TEST_CASE("掺了 decimal 的比较不折") { CHECK(fold_json(U"1 == 1.0")["type"] == "Compare"); }

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

// == / != 两侧都弃权后按对象身份兜底；序比较没有这条，跨类型不折。
TEST_SUITE("StaticEvaler 比较——跨类型的 ==/!= 兜底与序比较的不对称") {

    TEST_CASE("跨类型 == 折成 False、!= 折成 True（按身份兜底）") {
        CHECK(fold_json(U"() == []") == bool_lit(false));
        CHECK(fold_json(U"() != []") == bool_lit(true));
        CHECK(fold_json(U"(1,) == [1]") == bool_lit(false));
        CHECK(fold_json(U"1 == 'x'") == bool_lit(false));
        CHECK(fold_json(U"1 != 'x'") == bool_lit(true));
        CHECK(fold_json(U"None == 1") == bool_lit(false));
        CHECK(fold_json(U"'a' == ()") == bool_lit(false));
        CHECK(fold_json(U"True == 'x'") == bool_lit(false));
        CHECK(fold_json(U"1.0 == 'x'") == bool_lit(false));
    }

    // 这几条是兜底规则的另一半：没有身份兜底，两侧都弃权就是运行期 TypeError，编译期只能不折
    TEST_CASE("跨类型的序比较一律不折，把 TypeError 留给运行期") {
        CHECK(fold_json(U"() < []")["type"] == "Compare");
        CHECK(fold_json(U"1 < 'x'")["type"] == "Compare");
        CHECK(fold_json(U"None < 1")["type"] == "Compare");
        CHECK(fold_json(U"'a' < ()")["type"] == "Compare");
        CHECK(fold_json(U"1.0 <= 'x'")["type"] == "Compare");
        CHECK(fold_json(U"(1,) >= [1]")["type"] == "Compare");
    }

    // 同一个类型自己认识对方时轮不到兜底，走的是该类型的按值比较
    TEST_CASE("同类型走按值比较，不经过兜底") {
        CHECK(fold_json(U"() == ()") == bool_lit(true));
        CHECK(fold_json(U"(1, 2) == (1, 2)") == bool_lit(true));
        CHECK(fold_json(U"(1,) == (1, 2)") == bool_lit(false));
        CHECK(fold_json(U"'a' == 'a'") == bool_lit(true));
    }

    // None/Ellipsis 是单例，同一个对象 → 兜底给出 True；它们不支持序比较
    TEST_CASE("None 与 Ellipsis 是单例，== 走身份为真，序比较仍不折") {
        CHECK(fold_json(U"None == None") == bool_lit(true));
        CHECK(fold_json(U"None != None") == bool_lit(false));
        CHECK(fold_json(U"... == ...") == bool_lit(true));
        CHECK(fold_json(U"None == ...") == bool_lit(false));
        CHECK(fold_json(U"None < None")["type"] == "Compare");
        CHECK(fold_json(U"... < ...")["type"] == "Compare");
    }

    TEST_CASE("bool/int 按值比；掺 decimal 不折") {
        CHECK(fold_json(U"True == 1") == bool_lit(true));
        CHECK(fold_json(U"False < 1.5")["type"] == "Compare"); // 认识但 decimal 不折，不是 False
        CHECK(fold_json(U"True == 1.0")["type"] == "Compare");
        CHECK(fold_json(U"1 < 1.5")["type"] == "Compare");
    }

    // 容器的元素级比较要递归套用同一套规则，兜底也要递归生效
    TEST_CASE("嵌套容器：元素级比较递归套用同一套规则") {
        CHECK(fold_json(U"((),) == ([],)") == bool_lit(false)); // 元素跨类型 → 元素不等
        CHECK(fold_json(U"(1,) == ('a',)") == bool_lit(false));
        CHECK(fold_json(U"[1] == [1.0]")["type"] == "Compare"); // 元素含 decimal，判不了，整体不折
        CHECK(fold_json(U"(True,) == (1,)") == bool_lit(true)); // 元素是 bool/int，按值相等
    }

    TEST_CASE("嵌套容器的序比较：某个元素不可比 → 整体不可比 → 不折") {
        CHECK(fold_json(U"(1,) < ('a',)")["type"] == "Compare");
        CHECK(fold_json(U"(1, 1) < (1, 'a')")["type"] == "Compare"); // 第二个元素才不可比
        CHECK(fold_json(U"((),) < ([],)")["type"] == "Compare");
        // 对照：短的那个是长的的前缀时，根本走不到后面那个不可比的元素，照折
        CHECK(fold_json(U"(1,) < (1, 'a')") == bool_lit(true));
        CHECK(fold_json(U"(1, 2) < (1, 3)") == bool_lit(true));
    }

    // 链式比较里，跨类型的那一环照样按上面两套规则各自处理
    TEST_CASE("链式比较里混入跨类型的一环") {
        CHECK(fold_json(U"1 == 1 == 'a'") == bool_lit(false)); // 后一环跨类型 → False → 整链 False
        CHECK(fold_json(U"3 < 2 < 'a'") == bool_lit(false));   // 第一环就假，短路，不看后面
        CHECK(fold_json(U"1 < 2 < 'a'")["type"] == "Compare"); // 第一环真、第二环不可比 → 部分折
    }
}

TEST_SUITE("StaticEvaler in") {

    TEST_CASE("in 整体不折") {
        CHECK(fold_json(U"1 in (1,)")["type"] == "OpBinary");
        CHECK(fold_json(U"1 in (1,)")["op"] == "in");
        CHECK(fold_json(U"'a' in 'ab'")["type"] == "OpBinary");
        CHECK(fold_json(U"(1 + 1) in xs")["left"] == int_lit("2"));
    }
}

TEST_SUITE("ExprFolder is") {

    TEST_CASE("操作数会折，整体不折") {
        const auto result = fold_json(U"(1 + 1) is (2 + 2)");
        CHECK(result["type"] == "Is");
        CHECK(result["operands"] == nlohmann::json::array({int_lit("2"), int_lit("4")}));
        CHECK(fold_json(U"1 is 1")["type"] == "Is");
        CHECK(fold_json(U"None is None")["type"] == "Is");
    }
}
