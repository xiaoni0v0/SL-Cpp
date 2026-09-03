// Program 丢掉纯字面量，不保留最后一条。
#include "../test_utils.h"

#include <doctest/doctest.h>

namespace {

nlohmann::json assign(const char *name, const nlohmann::json &value) {
    return nlohmann::json{{"type", "Assign"}, {"target", ident(name)}, {"value", value}};
}

nlohmann::json program(const nlohmann::json &exprs) {
    return nlohmann::json{{"type", "Program"}, {"exprs", exprs}};
}
} // namespace

TEST_SUITE("StaticEvaler AstNodeProgram 剪枝") {

    TEST_CASE("空 Program：没有东西可剪，恒不变") {
        CHECK(fold_program_json(U"") == program(nlohmann::json::array()));
    }

    TEST_CASE("只有一条顶层表达式、且是纯字面量：整条剪掉，Program 这层本身不消失") {
        // 5 求值不触发 return，Program 的值恒为 None，留着这条纯字面量没有任何意义
        CHECK(fold_program_json(U"5") == program(nlohmann::json::array()));
    }

    TEST_CASE("字面量前缀被丢弃，非字面量的最后一条保留") {
        CHECK(fold_program_json(U"1; 2; x") == program(nlohmann::json::array({ident("x")})));
    }

    TEST_CASE("非字面量夹在中间：字面量全丢（含最后一条），非字面量留，相对顺序不变") {
        // x 不是字面量，留着；1、2 都是字面量——包括最后一条的 2——统统丢掉，
        // 不像复合表达式那样得保留最后一条
        CHECK(fold_program_json(U"x; 1; 2") == program(nlohmann::json::array({ident("x")})));
    }

    TEST_CASE("全体都是字面量：全部剪空") {
        CHECK(fold_program_json(U"1; 2; 3") == program(nlohmann::json::array()));
    }

    TEST_CASE(
        "带真实副作用（赋值）的子表达式必须留着；后面纯字面量的尾巴照样丢，不因为"
        "它是最后一条就特殊对待"
    ) {
        CHECK(
            fold_program_json(U"x = 1; 2") ==
            program(nlohmann::json::array({assign("x", int_lit("1"))}))
        );
    }

    // 剪枝规则判的是"是不是纯字面量"（is_literal_pure），不是"子表达式折完是不是字面量"——
    // return 1 的节点类型是 AstNodeReturn，不管它的 value_ 折成什么都不该被剪掉，
    // 否则 func f() { return 1 } 会被剪成空 body，调用行为从返回 1 变成返回 None
    TEST_CASE("return 不是纯字面量，folding 后夹在字面量中间也不会被剪掉") {
        CHECK(
            fold_program_json(U"1; return 2; 3") ==
            program(
                nlohmann::json::array({nlohmann::json{{"type", "Return"}, {"value", int_lit("2")}}})
            )
        );
    }

    TEST_CASE("函数体也会被剪枝：跟模块顶层是同一套逻辑") {
        // 注意：这里必须用 `= fold_program_json(...)` 而不是 `{fold_program_json(...)}`——
        // nlohmann::json 有 initializer_list 构造函数，`auto j{已经是个 json 的值}`
        // 这种写法会被当成"用这一个元素构造数组"，见 container_ops_test.cpp 里同一个坑
        const auto result = fold_program_json(U"func f() { 1; 2; x }");
        const auto body = result["exprs"][0]["body"];
        CHECK(body["type"] == "Program"); // 剪完了还是 Program，没有被展开成别的类型
        CHECK(body["exprs"] == nlohmann::json::array({ident("x")}));
    }

    TEST_CASE(
        "函数体全体都是字面量：整个剪空，但 body 依然是 Program（不会退化成裸表达式）——"
        "func f() { 1; 2; 3 } 跟 func f() {} 是同一个东西，调用都返回 None，"
        "这正是这条规则跟复合表达式折叠不一样的地方"
    ) {
        const auto result = fold_program_json(U"func f() { 1; 2; 3 }");
        const auto body = result["exprs"][0]["body"];
        CHECK(body["type"] == "Program");
        CHECK(body["exprs"] == nlohmann::json::array());
    }

    TEST_CASE("func f() { 1 } 折完跟 func f() {} 形状相同") {
        CHECK(fold_program_json(U"func f() { 1 }") == fold_program_json(U"func f() {}"));
    }

    TEST_CASE("类体同样会被剪枝") {
        const auto result = fold_program_json(U"class C { 1; 2; x }");
        const auto body = result["exprs"][0]["body"];
        CHECK(body["type"] == "Program");
        CHECK(body["exprs"] == nlohmann::json::array({ident("x")}));
    }
}
