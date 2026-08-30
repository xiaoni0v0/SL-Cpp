// SL.md global 表达式——语法：global identifier。
// 目标整条按表达式解析（复用通用的 '.' 属性访问等规则），再要求恰好是一个标识符——
// global 的值恒为 None，任何跟在目标后面的运算符都保证跑不通，干脆在语法层直接拦。
#include "../../../builtins/exceptions/SyntaxError.h"
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("global") {

    TEST_CASE("基本形式") {
        CHECK(parse_json(U"global x") == nlohmann::json{{"type", "Global"}, {"identifier", "x"}});
    }

    TEST_CASE("global 后面必须是标识符 token，不是表达式，字面量非法") {
        CHECK_THROWS_AS(parse_as_file(U"global 5"), SyntaxError);
        CHECK_THROWS_AS(parse_as_file(U"global 'x'"), SyntaxError);
    }

    TEST_CASE("global 只认恰好一个标识符，后面多余的内容会在外层因为缺分隔符报错") {
        CHECK_THROWS_AS(parse_as_file(U"global x y"), SyntaxError);
    }

    TEST_CASE("目标不能是属性访问/索引/调用/运算符表达式等——这些接出来的值恒为 None，跑不通") {
        check_parse_throws_with(U"global x.y", "global target must be a single identifier");
        check_parse_throws_with(U"global x[0]", "global target must be a single identifier");
        check_parse_throws_with(U"global f()", "global target must be a single identifier");
        check_parse_throws_with(U"global x + 1", "global target must be a single identifier");
    }

    TEST_CASE("目标撞见关键字时报的是'期待标识符'，不会一头扎进关键字自己的产生式报无关的错") {
        check_parse_throws_with(U"global class", "expected an identifier after 'global'");
        check_parse_throws_with(U"global as", "expected an identifier after 'global'");
    }

    TEST_CASE("缺少标识符时报错") { CHECK_THROWS_AS(parse_as_file(U"global"), SyntaxError); }

    TEST_CASE("一行内可以有多条 global（用分号分隔）") {
        CHECK(
            parse_program_json(U"global x; global y") ==
            nlohmann::json{
                {"type", "Program"},
                {"exprs",
                 nlohmann::json::array(
                     {{{"type", "Global"}, {"identifier", "x"}},
                      {{"type", "Global"}, {"identifier", "y"}}}
                 )}
            }
        );
    }
}
