// StaticEvaler::prune_program：AstNodeProgram（模块顶层/函数体/类体）跟复合表达式同一条
// "丢弃非最后一条的纯字面量子表达式"规则（SL.md 3.4.1），但节点本身的类型/身份不能变——
// LiteralFolder::root_ 是按引用持有的 AstNodeProgram&，AstNodeFunc::body_/AstNodeClass::body_
// 也固定要求是 AstNodeProgramPtr，不是通用的 AstNodePtr，没法像复合表达式那样整个节点换成
// 别的类型。所以这里只原地精简 exprs_，哪怕最后只剩一条也不展开成裸表达式，见
// StaticEvaler.h 类头注释。
#include "../test_utils.h"

#include <doctest/doctest.h>

namespace {
nlohmann::json ident(const char *name) {
    return nlohmann::json{{"type", "Identifier"}, {"identifier", name}};
}

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

    TEST_CASE("只有一条顶层表达式：不剪，也不展开——哪怕就是个字面量，Program 这层不会消失") {
        CHECK(fold_program_json(U"5") == program(nlohmann::json::array({int_lit("5")})));
    }

    TEST_CASE("字面量前缀被丢弃，非字面量的最后一条保留") {
        CHECK(fold_program_json(U"1; 2; x") == program(nlohmann::json::array({ident("x")})));
    }

    TEST_CASE("非字面量夹在中间：字面量丢，非字面量留，相对顺序不变") {
        // x 在最后一条之前，不是字面量，留着；1 是字面量，丢掉；2 是最后一条，永远留着
        CHECK(
            fold_program_json(U"x; 1; 2") ==
            program(nlohmann::json::array({ident("x"), int_lit("2")}))
        );
    }

    TEST_CASE("全体都是字面量：剪到只剩最后一条，但依然是被 Program 包着的一条，不展开") {
        CHECK(fold_program_json(U"1; 2; 3") == program(nlohmann::json::array({int_lit("3")})));
    }

    TEST_CASE("带真实副作用（赋值）的子表达式，不能因为最后一条是字面量就被剪掉") {
        CHECK(
            fold_program_json(U"x = 1; 2") ==
            program(nlohmann::json::array({assign("x", int_lit("1")), int_lit("2")}))
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
        "函数体全体都是字面量：body 依然是 Program（只剩一条也不会退化成裸的 LiteralInt），"
        "这正是这条规则跟复合表达式折叠不一样的地方"
    ) {
        const auto result = fold_program_json(U"func f() { 1; 2; 3 }");
        const auto body = result["exprs"][0]["body"];
        CHECK(body["type"] == "Program");
        CHECK(body["exprs"] == nlohmann::json::array({int_lit("3")}));
    }

    TEST_CASE("类体同样会被剪枝") {
        const auto result = fold_program_json(U"class C { 1; 2; x }");
        const auto body = result["exprs"][0]["body"];
        CHECK(body["type"] == "Program");
        CHECK(body["exprs"] == nlohmann::json::array({ident("x")}));
    }
}
