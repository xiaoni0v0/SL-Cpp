// StaticEvaler：复合表达式 { expr1; expr2; ... } 的折叠。值为最后一条表达式的值，
// 空复合表达式为 None；{} 不引入作用域，所以子表达式之间可能靠副作用互相影响。
//
// 折叠规则（详见 StaticEvaler.h 类头注释）：
//   1. 空复合表达式恒折成 None；
//   2. 只有一条，直接展开成那一条本身，不管是不是字面量；
//   3. 否则，除最后一条外，逐条丢掉纯字面量的子表达式（无副作用、值也用不上），非字面量的
//      保留且相对顺序不变；最后一条永远保留（它决定整个表达式的值，不管是不是字面量）；
//      丢到只剩最后一条就直接展开成那一条，丢完还剩不止一条就拼一个更短的复合表达式，
//      一条都没丢成就不折。
#include "../test_utils.h"

#include <doctest/doctest.h>

namespace {
nlohmann::json ident(const char *name) {
    return nlohmann::json{{"type", "Identifier"}, {"identifier", name}};
}

nlohmann::json call0(const char *name) {
    return nlohmann::json{
        {"type", "Call"},
        {"object", ident(name)},
        {"positional_args", nlohmann::json::array()},
        {"keyword_args", nlohmann::json::array()}
    };
}

nlohmann::json assign(const char *name, const nlohmann::json &value) {
    return nlohmann::json{{"type", "Assign"}, {"target", ident(name)}, {"value", value}};
}
} // namespace

TEST_SUITE("StaticEvaler 复合表达式折叠") {

    TEST_CASE("空复合表达式恒折成 None") { CHECK(fold_json(U"{}") == none_lit()); }

    TEST_CASE("只有一条：直接展开成那一条，不管是不是字面量") {
        CHECK(fold_json(U"{5}") == int_lit("5"));
        CHECK(fold_json(U"{f()}") == call0("f")); // 非字面量也一样展开——{} 只有一条时纯属多余包装
        CHECK(fold_json(U"{x}") == ident("x"));
    }

    TEST_CASE("全体子表达式都是字面量：折成最后一条") {
        CHECK(fold_json(U"{1; 2; 3}") == int_lit("3"));
        CHECK(fold_json(U"{1; 'a'; True; None; 3.14}") == float_lit("3.14")); // 类型可以互不相同
    }

    TEST_CASE("最后一条折出来是容器字面量，同样能折") {
        CHECK(
            fold_json(U"{1; (1, 2)}") ==
            nlohmann::json{{"type", "LiteralTuple"}, {"items", {int_lit("1"), int_lit("2")}}}
        );
    }

    TEST_CASE("子表达式先各自折成字面量，再触发整体折叠（自底向上）") {
        // 1+2 和 3*4 先各自折成 3、12，之后整个 Compound 才发现全是字面量，折成 12
        CHECK(fold_json(U"{1 + 2; 3 * 4}") == int_lit("12"));
    }

    TEST_CASE("嵌套复合表达式：内层先折，外层再跟着折") {
        // {1;2} 先折成 2，外层变成 {2; 3}，再折成 3
        CHECK(fold_json(U"{{1; 2}; 3}") == int_lit("3"));
    }

    TEST_CASE("字面量前缀 + 非字面量收尾：丢掉前缀，直接展开成最后那条（用户提的原始例子）") {
        CHECK(fold_json(U"{1; 2; f()}") == call0("f"));
        CHECK(fold_json(U"{1; 2; 3; 4; f()}") == call0("f")); // 前缀多长都一样
        CHECK(fold_json(U"{1; x}") == ident("x"));
    }

    TEST_CASE("非字面量夹在中间：只丢字面量，非字面量原样保留、相对顺序不变") {
        // x 和 1 都在"最后一条"之前；x 不是字面量留着，1 是字面量丢掉，剩 {x; 2}
        CHECK(
            fold_json(U"{x; 1; 2}") ==
            nlohmann::json{
                {"type", "Compound"}, {"exprs", nlohmann::json::array({ident("x"), int_lit("2")})}
            }
        );
        // 两个非字面量调用中间夹一个字面量：字面量丢了，两个调用都留着且顺序不变
        CHECK(
            fold_json(U"{f(); 1; g()}") ==
            nlohmann::json{
                {"type", "Compound"}, {"exprs", nlohmann::json::array({call0("f"), call0("g")})}
            }
        );
    }

    TEST_CASE("最后一条本身是字面量时必须保留，不能因为它是字面量就被当成'可丢的'") {
        // f() 不是字面量，留着；最后一条 1 是字面量，但它是最后一条、决定整个表达式的值，必须留着
        // ——如果这里被误丢，就变成只剩 f()，值就错了（应该是 1，不是 f() 的返回值）
        CHECK(
            fold_json(U"{f(); 1}") ==
            nlohmann::json{
                {"type", "Compound"}, {"exprs", nlohmann::json::array({call0("f"), int_lit("1")})}
            }
        );
    }

    TEST_CASE("前面的子表达式带真实副作用（赋值），不能因为最后一条是字面量就瞎折") {
        CHECK(
            fold_json(U"{x = 1; 2}") ==
            nlohmann::json{
                {"type", "Compound"},
                {"exprs", nlohmann::json::array({assign("x", int_lit("1")), int_lit("2")})}
            }
        );
    }

    TEST_CASE("一条都没能丢：全部子表达式（除最后一条外）都不是字面量，原样不折") {
        CHECK(
            fold_json(U"{x; y; 1}") ==
            nlohmann::json{
                {"type", "Compound"},
                {"exprs", nlohmann::json::array({ident("x"), ident("y"), int_lit("1")})}
            }
        );
    }

    TEST_CASE(
        "for 折叠产生的复合表达式也要能被继续折（visit_and_replace 折到不动为止，"
        "不是只折一次）：for (1; 0; ) {} 的 init 是纯字面量 1，跟死循环退化值 0 拼成 "
        "{1; 0} 后应该继续折成 0，而不是停在 {1; 0} 这个中间态"
    ) {
        CHECK(fold_json(U"for (1; 0;) {}") == int_lit("0"));
        // init 带真实副作用（赋值）的对照组：不能被继续折掉，跟 dead_branch_test.cpp
        // 里"for 的 cond 是 False：init 无论如何都会先无条件求值一次，副作用必须保留"是同一条用例
        CHECK(
            fold_json(U"for (x = 1; False; x = 2) body") ==
            nlohmann::json{
                {"type", "Compound"},
                {"exprs", nlohmann::json::array({assign("x", int_lit("1")), int_lit("0")})}
            }
        );
    }
}
