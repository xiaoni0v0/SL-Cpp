// SL.md 2.2.5 import 表达式——两种语法形态：
//   1. 关键字形态 import a / import a.b.c ...：各段都是标识符 token（不是表达式），解析成
//      AstNodeImport；
//   2. 调用形态 import(...)：跟普通函数调用完全一致，解析成 object_ 为标识符 "import" 的
//      AstNodeCall，实参一概不校验、留给运行时。
#include "../../../builtins/exceptions/SyntaxError.h"
#include "../test_utils.h"

#include <doctest/doctest.h>

namespace {
// 关键字形态的期望值：{"type": "Import", "segments": [...]}
nlohmann::json import_node(const nlohmann::json &segments) {
    return nlohmann::json{{"type", "Import"}, {"segments", segments}};
}

// 调用形态的被调对象恒是标识符 "import"
nlohmann::json import_callee() {
    return nlohmann::json{{"type", "Identifier"}, {"identifier", "import"}};
}
} // namespace

TEST_SUITE("2.2.5 import 关键字形态") {

    TEST_CASE("单段") {
        CHECK(parse_json(U"import math") == import_node(nlohmann::json::array({"math"})));
    }

    TEST_CASE("多段：点号一路吃干净，整体是一个 Import，不是 Attr 属性访问链") {
        CHECK(parse_json(U"import os.path") == import_node(nlohmann::json::array({"os", "path"})));
        CHECK(
            parse_json(U"import a.b.c.d") ==
            import_node(nlohmann::json::array({"a", "b", "c", "d"}))
        );
    }

    TEST_CASE("各段都是标识符 token，不是表达式，字面量非法") {
        CHECK_THROWS_AS(parse_program(U"import 5"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"import 'math'"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"import a.5"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"import a.'b'"), SyntaxError);
    }

    TEST_CASE("段名不能是关键字/保留字") {
        CHECK_THROWS_AS(parse_program(U"import class"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"import os.class"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"import as"), SyntaxError);
    }

    TEST_CASE("点号后面必须还有一段") { CHECK_THROWS_AS(parse_program(U"import a."), SyntaxError); }

    TEST_CASE("缺少名字时报错") { CHECK_THROWS_AS(parse_program(U"import"), SyntaxError); }

    TEST_CASE("import 后面允许换行（跟 global/del 一致）") {
        CHECK(parse_json(U"import\nmath") == import_node(nlohmann::json::array({"math"})));
    }

    TEST_CASE("'.' 前后的换行规则跟属性访问 x.y 一致：'.' 后无条件允许换行") {
        CHECK(parse_json(U"import a.\nb") == import_node(nlohmann::json::array({"a", "b"})));
        CHECK(
            parse_json(U"import a.\n\nb.\nc") == import_node(nlohmann::json::array({"a", "b", "c"}))
        );
    }

    TEST_CASE("'.' 前只在括号内允许换行（顶层换行就是表达式结束，同 x\\n.y）") {
        CHECK(parse_json(U"(import a\n.b)") == import_node(nlohmann::json::array({"a", "b"})));
        // 顶层：import a 到此为止，下一行的 .b 单独成句，是语法错误
        CHECK_THROWS_AS(parse_program(U"import a\n.b"), SyntaxError);
    }

    TEST_CASE("import a 整体和其他基本表达式一样参与后缀运算符链（点号除外，被段名吃掉了）") {
        CHECK(
            parse_json(U"import a[0]") ==
            nlohmann::json{
                {"type", "Index"},
                {"object", import_node(nlohmann::json::array({"a"}))},
                {"args", nlohmann::json::array({{{"type", "LiteralInt"}, {"raw", "0"}}})}
            }
        );
    }

    TEST_CASE("import 是表达式，可以出现在任何需要值的位置") {
        CHECK(
            parse_json(U"x = import a") ==
            nlohmann::json{
                {"type", "Assign"},
                {"target", {{"type", "Identifier"}, {"identifier", "x"}}},
                {"value", import_node(nlohmann::json::array({"a"}))}
            }
        );
    }

    TEST_CASE("一行内可以有多条 import（用分号分隔）") {
        CHECK(
            parse_program_json(U"import a; import b.c") ==
            nlohmann::json{
                {"type", "Program"},
                {"exprs",
                 nlohmann::json::array(
                     {import_node(nlohmann::json::array({"a"})),
                      import_node(nlohmann::json::array({"b", "c"}))}
                 )}
            }
        );
    }
}

TEST_SUITE("2.2.5 import 调用形态") {

    TEST_CASE("被调对象是标识符 import，实参照普通函数调用解析") {
        CHECK(
            parse_json(U"import('math')") ==
            nlohmann::json{
                {"type", "Call"},
                {"object", import_callee()},
                {"positional_args",
                 nlohmann::json::array({{{"type", "LiteralStr"}, {"value", "math"}}})},
                {"keyword_args", nlohmann::json::array()}
            }
        );
    }

    TEST_CASE("关键字实参") {
        const auto result = parse_json(U"import('os', lazy=True, force=False)");
        CHECK(result["object"] == import_callee());
        CHECK(result["keyword_args"].size() == 2);
        CHECK(result["keyword_args"][0]["keyword"] == "lazy");
        CHECK(result["keyword_args"][1]["keyword"] == "force");
    }

    TEST_CASE("实参一概不校验：任意表达式、* 展开、** 展开都照单全收，留给运行时") {
        CHECK_NOTHROW(parse_program(U"import(f() + g())"));
        CHECK_NOTHROW(parse_program(U"import(*names)"));
        CHECK_NOTHROW(parse_program(U"import(**options)"));
        CHECK_NOTHROW(parse_program(U"import()")); // 参数个数不对也是运行期的事
    }

    TEST_CASE("调用结果照常参与后缀运算符链") {
        const auto result = parse_json(U"import('os.path').join");
        CHECK(result["type"] == "Attr");
        CHECK(result["attr"] == "join");
        CHECK(result["object"]["type"] == "Call");
        CHECK(result["object"]["object"] == import_callee());
    }

    TEST_CASE("'(' 的换行规则跟普通函数调用一样：只有括号内才允许跨行") {
        // 括号内：换行后的 '(' 照样接成调用
        CHECK(parse_json(U"(import\n('math'))")["type"] == "Call");
        // 顶层：换行后不再接调用，退回关键字形态，于是 '(' 处报"期望标识符"
        CHECK_THROWS_AS(parse_program(U"import\n('math')"), SyntaxError);
    }
}
