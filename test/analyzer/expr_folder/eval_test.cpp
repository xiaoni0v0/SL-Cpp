// eval 的实参会折；节点本身不折。
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("ExprFolder eval") {

    TEST_CASE("位置实参、关键字实参都会折") {
        const auto pos = fold_json(U"eval(1 + 1)");
        CHECK(pos["type"] == "Eval");
        CHECK(pos["positional_args"] == nlohmann::json::array({int_lit("2")}));
        const auto many = fold_json(U"eval(1 + 1, 2 + 2)");
        CHECK(many["positional_args"] == nlohmann::json::array({int_lit("2"), int_lit("4")}));
        const auto kw = fold_json(U"eval(code = 'a' + 'b')");
        CHECK(kw["keyword_args"][0]["keyword"] == "code");
        CHECK(kw["keyword_args"][0]["value"] == str_lit("ab"));
    }

    TEST_CASE("** 展开内部也会折") {
        const auto result = fold_json(U"eval(**{'code': 'a' + 'b'})");
        CHECK(result["keyword_args"][0]["keyword"] == nullptr);
        CHECK(result["keyword_args"][0]["value"]["type"] == "DoubleStar");
        // 只查到 DoubleStar 这层不够——内层字典的 value（'a' + 'b'）真的被折了才算数
        const auto &dict{result["keyword_args"][0]["value"]["operand"]};
        CHECK(dict["type"] == "LiteralDict");
        CHECK(dict["items"][0]["value"] == str_lit("ab"));
    }

    TEST_CASE("eval 节点本身不折，也不会当纯字面量丢掉") {
        CHECK(fold_json(U"eval('x')")["type"] == "Eval");
        const auto result = fold_json(U"{ eval('x'); 1 }");
        CHECK(result["type"] == "Compound");
        CHECK(result["exprs"].size() == 2);
        CHECK(result["exprs"][0]["type"] == "Eval");
    }
}
