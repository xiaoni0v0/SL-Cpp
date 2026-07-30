// StaticEvaler::prune_program：AstNodeProgram（模块顶层/函数体/类体）的剪枝规则。
//
// 关键点：Program 的值跟复合表达式不一样——不是"最后一条表达式的值"，而是完全由 return 决定：
// 触发了 return 就是那个值，从头到尾没触发就恒为 None（SL.md 3.4.1/3.4.6）。所以纯字面量
// （is_literal_pure）不管出现在 exprs_ 的哪个位置——包括最后一条——都能安全丢掉：它既不含
// return，也没有副作用，留不留都不影响 Program 的值。`func f() { 1 }` 跟 `func f() {}`
// 是同一个东西（调用都返回 None），这正是这条规则要处理的情况。
//
// 但节点本身的类型/身份不能变——LiteralFolder::root_ 是按引用持有的 AstNodeProgram&，
// AstNodeFunc::body_/AstNodeClass::body_ 也固定要求是 AstNodeProgramPtr，不是通用的
// AstNodePtr，没法像复合表达式那样整个节点换成别的类型。所以这里只原地精简 exprs_，
// 哪怕精简到空也不删除/替换这个节点。
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

    TEST_CASE("func f() { 1 } 折完跟 func f() {} 长得一模一样（这次讨论的原始例子）") {
        CHECK(fold_program_json(U"func f() { 1 }") == fold_program_json(U"func f() {}"));
    }

    TEST_CASE("类体同样会被剪枝") {
        const auto result = fold_program_json(U"class C { 1; 2; x }");
        const auto body = result["exprs"][0]["body"];
        CHECK(body["type"] == "Program");
        CHECK(body["exprs"] == nlohmann::json::array({ident("x")}));
    }
}
