// 收集模式记号：`$` / `$ *` / `$$` / `$$ **`。
#include "../../../builtins/exceptions/SyntaxError.h"
#include "../test_utils.h"

#include <doctest/doctest.h>

namespace {

// for [mark] (xs as i) body 的期望形状，只有 collect 一项不同
nlohmann::json for_iter(const char *mark) {
    return nlohmann::json{
        {"type", "ForIter"},
        {"collect", mark},
        {"target", ident("i")},
        {"iterable", ident("xs")},
        {"body", ident("body")}
    };
}
} // namespace

TEST_SUITE("收集模式记号——四种合法写法") {

    TEST_CASE("迭代模式上的四种记号") {
        CHECK(parse_json(U"for (xs as i) body") == for_iter("none"));
        CHECK(parse_json(U"for $ (xs as i) body") == for_iter("$"));
        CHECK(parse_json(U"for $ * (xs as i) body") == for_iter("$ *"));
        CHECK(parse_json(U"for $$ (xs as i) body") == for_iter("$$"));
        CHECK(parse_json(U"for $$ ** (xs as i) body") == for_iter("$$ **"));
    }

    TEST_CASE("步进模式上的四种记号") {
        const auto collect_of{[](const std::u32string &source) {
            return parse_json(source)["collect"];
        }};
        CHECK(collect_of(U"for (;;) body") == "none");
        CHECK(collect_of(U"for $ (;;) body") == "$");
        CHECK(collect_of(U"for $ * (;;) body") == "$ *");
        CHECK(collect_of(U"for $$ (;;) body") == "$$");
        CHECK(collect_of(U"for $$ ** (;;) body") == "$$ **");
    }

    TEST_CASE("while 上的四种记号——出的仍是 ForCond") {
        const auto collect_of{[](const std::u32string &source) {
            // 必须用 = 拷贝初始化，不能用 {}——见 .ai/notes/json-test-brace-init-trap.md
            const auto node = parse_json(source);
            CHECK(node["type"] == "ForCond");
            return node["collect"];
        }};
        CHECK(collect_of(U"while (c) body") == "none");
        CHECK(collect_of(U"while $ (c) body") == "$");
        CHECK(collect_of(U"while $ * (c) body") == "$ *");
        CHECK(collect_of(U"while $$ (c) body") == "$$");
        CHECK(collect_of(U"while $$ ** (c) body") == "$$ **");
    }

    TEST_CASE("两位之间、记号与括号之间都可以有空白或换行") {
        CHECK(parse_json(U"for$*(xs as i) body") == for_iter("$ *"));
        CHECK(parse_json(U"for\n$\n*\n(xs as i) body") == for_iter("$ *"));
        CHECK(parse_json(U"for $$\n**\n(xs as i) body") == for_iter("$$ **"));
    }
}

TEST_SUITE("收集模式记号——非法组合") {

    TEST_CASE("两位不配套") {
        check_parse_throws_with(U"for $ ** (xs as i) body", "pairs with '*', not '**'");
        check_parse_throws_with(U"while $ ** (c) body", "pairs with '*', not '**'");
        check_parse_throws_with(U"for $$ * (xs as i) body", "pairs with '**', not '*'");
        check_parse_throws_with(U"while $$ * (c) body", "pairs with '**', not '*'");
    }

    TEST_CASE("只有第二位，没有第一位") {
        check_parse_throws_with(U"for * (xs as i) body", "must follow a collect mark");
        check_parse_throws_with(U"for ** (xs as i) body", "must follow a collect mark");
        check_parse_throws_with(U"while * (c) body", "must follow a collect mark");
        check_parse_throws_with(U"while ** (c) body", "must follow a collect mark");
    }

    TEST_CASE("$$ 必须连写，$ $ 不是 $$") {
        // 第二个 $ 既不是 * 也不是 '('，卡在等待 '(' 这一步
        CHECK_THROWS_AS(parse_as_file(U"for $ $ (xs as i) body"), SyntaxError);
        CHECK_THROWS_AS(parse_as_file(U"for $ $ ** (xs as i) body"), SyntaxError);
    }

    TEST_CASE("记号不能重复，也不能出现在括号之后") {
        CHECK_THROWS_AS(parse_as_file(U"for $ $$ (xs as i) body"), SyntaxError);
        CHECK_THROWS_AS(parse_as_file(U"for $ * * (xs as i) body"), SyntaxError);
        CHECK_THROWS_AS(parse_as_file(U"for (xs as i) $ body"), SyntaxError);
    }
}
