// ExprFolder：Eval / ForIter / Del / Is 四个节点各子槽位的递归折叠。
//
// 这四个节点的 visit 之前一条测试都没有。它们本身都不参与整体折叠（不在 StaticEvaler::fold 的
// 分发范围内），能测的就是"子槽位有没有被递归折到"——而这恰好是 .ai/architecture.md 点名警告过
// 的那类漏：给已有节点加一个新的子节点槽位，编译器一个字都不会提醒，X-macro 只保证每个**类型**
// 都有 visit，管不到某个 visit 里面漏读了哪个**字段**。`as` 那次就是这么漏掉 except 的 target_
// 的，四套测试全绿也没发现。
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("ExprFolder Eval 子表达式折叠") {

    // eval 是关键字伪装的函数，实参形状跟普通调用一致，折叠也该一视同仁
    TEST_CASE("位置实参会被折叠") {
        const auto result = fold_json(U"eval(1 + 1)");
        CHECK(result["type"] == "Eval");
        CHECK(result["positional_args"] == nlohmann::json::array({int_lit("2")}));
    }

    TEST_CASE("多个位置实参各自独立折叠") {
        const auto result = fold_json(U"eval(1 + 1, 2 + 2)");
        CHECK(result["positional_args"] == nlohmann::json::array({int_lit("2"), int_lit("4")}));
    }

    TEST_CASE("关键字实参的 value 会被折叠") {
        const auto result = fold_json(U"eval(code = 'a' + 'b')");
        CHECK(result["keyword_args"][0]["keyword"] == "code");
        CHECK(result["keyword_args"][0]["value"] == str_lit("ab"));
    }

    TEST_CASE("** 展开的实参内部也会被折叠") {
        const auto result = fold_json(U"eval(**{'code': 'a' + 'b'})");
        CHECK(result["keyword_args"][0]["keyword"] == nullptr);
        CHECK(result["keyword_args"][0]["value"]["type"] == "DoubleStar");
    }

    // eval 有副作用（现场往调用帧里插代码），既不是纯字面量、也不能被当死代码剪掉
    TEST_CASE("eval 节点本身永远不折，也不会被当成纯字面量剪掉") {
        CHECK(fold_json(U"eval('x')")["type"] == "Eval");
        // 复合表达式里除最后一条外只丢纯字面量，eval 必须留着
        const auto result = fold_json(U"{ eval('x'); 1 }");
        CHECK(result["type"] == "Compound");
        CHECK(result["exprs"].size() == 2);
        CHECK(result["exprs"][0]["type"] == "Eval");
    }
}

TEST_SUITE("ExprFolder ForIter 子表达式折叠") {

    // 迭代模式的 for 有三个子槽位，其中 target_ 可空（不写 as 时）
    TEST_CASE("iterable_ 会被折叠") {
        const auto result = fold_json(U"for ('a' + 'b') x");
        CHECK(result["type"] == "ForIter");
        CHECK(result["iterable"] == str_lit("ab"));
    }

    TEST_CASE("target_ 内部会被折叠（目标本身是左值，折不动，折的是它的下标）") {
        const auto result = fold_json(U"for (xs as a[1 + 1]) y");
        CHECK(result["target"]["type"] == "Index");
        CHECK(result["target"]["args"] == nlohmann::json::array({int_lit("2")}));
    }

    TEST_CASE("body_ 会被折叠") {
        const auto result = fold_json(U"for (xs) (1 + 1)");
        CHECK(result["body"] == int_lit("2"));
    }

    TEST_CASE("没写 as 时 target_ 是空槽位，其他两个槽位照常折") {
        const auto result = fold_json(U"for (1 + 1) (2 + 2)");
        CHECK(result["target"] == nullptr);
        CHECK(result["iterable"] == int_lit("2"));
        CHECK(result["body"] == int_lit("4"));
    }

    // 迭代模式没有 cond 槽位，不参与死循环消除：可迭代对象是不是空、能不能迭代，
    // 都得等运行期拿到真对象才知道（`for (0 as x)` 运行期是 TypeError，折掉就吞了这个错）
    TEST_CASE("ForIter 整体永远不折，哪怕可迭代对象是空字面量") {
        CHECK(fold_json(U"for ([]) x")["type"] == "ForIter");
        CHECK(fold_json(U"for ('') x")["type"] == "ForIter");
        CHECK(fold_json(U"for (0 as x) y")["type"] == "ForIter");
    }
}

TEST_SUITE("ExprFolder Del / Is 子表达式折叠") {

    TEST_CASE("del 的 target_ 内部会被折叠") {
        const auto result = fold_json(U"del b[1 + 1]");
        CHECK(result["type"] == "Del");
        CHECK(result["target"]["type"] == "Index");
        CHECK(result["target"]["args"] == nlohmann::json::array({int_lit("2")}));
    }

    TEST_CASE("is 链的各操作数会被折叠") {
        const auto result = fold_json(U"(1 + 1) is (2 + 2)");
        CHECK(result["type"] == "Is");
        CHECK(result["operands"] == nlohmann::json::array({int_lit("2"), int_lit("4")}));
    }

    // is 比的是对象同一性，不是值——编译期不知道运行期会不会共享对象，所以整体不折
    TEST_CASE("is 整体永远不折，哪怕两边是相等的字面量") {
        CHECK(fold_json(U"1 is 1")["type"] == "Is");
        CHECK(fold_json(U"None is None")["type"] == "Is");
    }
}
