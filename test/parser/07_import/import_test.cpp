// SL.md import 表达式——两种语法形态，各自一个节点类型：
//   1. 关键字形态 import a / import a.b.c ...：各段都是标识符 token（不是表达式），解析成
//      AstNodeImportKw；
//   2. 调用形态 import(...)：实参解析规则跟普通函数调用完全一致（共用 finish_call_args），但没有
//      被调对象槽位——import 是运算符本身，不是能按名字取到的函数对象，解析成 AstNodeImportCall。
//      实参本身一概不校验、留给运行时。
#include "../../../builtins/exceptions/SyntaxError.h"
#include "../test_utils.h"

#include <doctest/doctest.h>

namespace {
nlohmann::json import_kw(const nlohmann::json &segments) {
    return nlohmann::json{{"type", "ImportKw"}, {"segments", segments}};
}

nlohmann::json
import_call(const nlohmann::json &positional_args, const nlohmann::json &keyword_args) {
    return nlohmann::json{
        {"type", "ImportCall"}, {"positional_args", positional_args}, {"keyword_args", keyword_args}
    };
}

nlohmann::json str_literal(const char *value) {
    return nlohmann::json{{"type", "LiteralStr"}, {"value", value}};
}

nlohmann::json ident(const char *name) {
    return nlohmann::json{{"type", "Identifier"}, {"identifier", name}};
}
} // namespace

TEST_SUITE("import 关键字形态") {

    TEST_CASE("单段") {
        CHECK(parse_json(U"import math") == import_kw(nlohmann::json::array({"math"})));
    }

    TEST_CASE("多段：点号一路吃干净，整体是一个 ImportKw，不是 Attr 属性访问链") {
        CHECK(parse_json(U"import os.path") == import_kw(nlohmann::json::array({"os", "path"})));
        CHECK(
            parse_json(U"import a.b.c.d") == import_kw(nlohmann::json::array({"a", "b", "c", "d"}))
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
        CHECK_THROWS_AS(parse_program(U"import import"), SyntaxError);
    }

    TEST_CASE("点号后面必须还有一段") {
        CHECK_THROWS_AS(parse_program(U"import a."), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"import a.b."), SyntaxError);
    }

    TEST_CASE("'..' 是独立的 Range 运算符 token，不是两个 '.'：段名到此为止") {
        CHECK(
            parse_json(U"import a..b") == nlohmann::json{
                                              {"type", "OpBinary"},
                                              {"op", ".."},
                                              {"left", import_kw(nlohmann::json::array({"a"}))},
                                              {"right", ident("b")}
                                          }
        );
    }

    TEST_CASE("缺少名字时报错") {
        CHECK_THROWS_AS(parse_program(U"import"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"import ."), SyntaxError);
    }

    TEST_CASE("import 后面允许换行（跟 global/del 一致）") {
        CHECK(parse_json(U"import\nmath") == import_kw(nlohmann::json::array({"math"})));
        CHECK(parse_json(U"import\n\n\nmath") == import_kw(nlohmann::json::array({"math"})));
    }

    TEST_CASE("'.' 后无条件允许换行（跟属性访问 x.y 一致）") {
        CHECK(parse_json(U"import a.\nb") == import_kw(nlohmann::json::array({"a", "b"})));
        CHECK(
            parse_json(U"import a.\n\nb.\nc") == import_kw(nlohmann::json::array({"a", "b", "c"}))
        );
    }

    TEST_CASE("'.' 前只在括号内允许换行（顶层换行就是表达式结束，同 x\\n.y）") {
        CHECK(parse_json(U"(import a\n.b)") == import_kw(nlohmann::json::array({"a", "b"})));
        // 顶层：import a 到此为止，下一行的 .b 单独成句，是语法错误
        CHECK_THROWS_AS(parse_program(U"import a\n.b"), SyntaxError);
    }

    TEST_CASE("import a 整体和其他基本表达式一样参与后缀运算符链（点号除外，被段名吃掉了）") {
        CHECK(
            parse_json(U"import a[0]") ==
            nlohmann::json{
                {"type", "Index"},
                {"object", import_kw(nlohmann::json::array({"a"}))},
                {"args", nlohmann::json::array({{{"type", "LiteralInt"}, {"raw", "0"}}})}
            }
        );
        // 段名吃干净之后才轮到调用：import a.b(x) 是 (import a.b)(x)
        const auto called = parse_json(U"import a.b(x)");
        CHECK(called["type"] == "Call");
        CHECK(called["object"] == import_kw(nlohmann::json::array({"a", "b"})));
    }

    TEST_CASE("import 是表达式，可以出现在任何需要值的位置") {
        CHECK(
            parse_json(U"x = import a") == nlohmann::json{
                                               {"type", "Assign"},
                                               {"target", ident("x")},
                                               {"value", import_kw(nlohmann::json::array({"a"}))}
                                           }
        );
        // 当实参传给别的函数
        const auto arg = parse_json(U"f(import a)");
        CHECK(arg["positional_args"][0] == import_kw(nlohmann::json::array({"a"})));
    }

    TEST_CASE("一行内可以有多条 import（用分号分隔）") {
        CHECK(
            parse_program_json(U"import a; import b.c") ==
            nlohmann::json{
                {"type", "Program"},
                {"exprs",
                 nlohmann::json::array(
                     {import_kw(nlohmann::json::array({"a"})),
                      import_kw(nlohmann::json::array({"b", "c"}))}
                 )}
            }
        );
    }
}

TEST_SUITE("import 调用形态") {

    TEST_CASE("没有被调对象槽位，只有实参") {
        CHECK(
            parse_json(U"import('math')") ==
            import_call(nlohmann::json::array({str_literal("math")}), nlohmann::json::array())
        );
    }

    TEST_CASE("空实参表也合法（参数个数对不对是运行期的事）") {
        CHECK(
            parse_json(U"import()") == import_call(nlohmann::json::array(), nlohmann::json::array())
        );
    }

    TEST_CASE("关键字实参：按书写顺序进关键字组") {
        const auto result = parse_json(U"import('os', lazy=True, force=False)");
        CHECK(result["type"] == "ImportCall");
        CHECK(result["positional_args"] == nlohmann::json::array({str_literal("os")}));
        CHECK(result["keyword_args"].size() == 2);
        CHECK(result["keyword_args"][0]["keyword"] == "lazy");
        CHECK(result["keyword_args"][1]["keyword"] == "force");
    }

    TEST_CASE("* 展开进位置组，** 展开进关键字组（keyword 为 null）") {
        const auto result = parse_json(U"import(*names, **options)");
        CHECK(result["positional_args"].size() == 1);
        CHECK(result["positional_args"][0]["type"] == "Star");
        CHECK(result["keyword_args"].size() == 1);
        CHECK(result["keyword_args"][0]["keyword"] == nullptr);
        CHECK(result["keyword_args"][0]["value"]["type"] == "DoubleStar");
    }

    TEST_CASE("实参可以是任意表达式，一概不校验，留给运行时") {
        CHECK_NOTHROW(parse_program(U"import(f() + g())"));
        CHECK_NOTHROW(parse_program(U"import(if (b) a else c)"));
        CHECK_NOTHROW(parse_program(U"import(1, 2, 3, whatever=None)"));
    }

    TEST_CASE("实参分组规则跟普通函数调用共用同一套：位置实参不能出现在关键字实参之后") {
        check_parse_throws_with(
            U"import(lazy=True, 'math')", "positional argument cannot appear after keyword argument"
        );
    }

    TEST_CASE("括号不闭合时报的是函数调用那一套错") {
        check_parse_throws_with(U"import('math'", "expected ')' to close function call");
    }

    TEST_CASE("实参表内部可以随意换行（括号内，paren_depth_ 正常生效）") {
        CHECK(
            parse_json(U"import(\n  'os',\n  lazy=True,\n)")["positional_args"] ==
            nlohmann::json::array({str_literal("os")})
        );
    }

    TEST_CASE("import 后面换行照样能接调用形态（跟关键字形态一视同仁）") {
        CHECK(parse_json(U"import\n('math')") == parse_json(U"import('math')"));
        CHECK(parse_json(U"(import\n('math'))") == parse_json(U"import('math')"));
    }

    TEST_CASE("调用结果照常参与后缀运算符链") {
        const auto result = parse_json(U"import('os.path').join");
        CHECK(result["type"] == "Attr");
        CHECK(result["attr"] == "join");
        CHECK(result["object"]["type"] == "ImportCall");
    }

    TEST_CASE("嵌套：实参里还能再写 import") {
        const auto result = parse_json(U"import(import a)");
        CHECK(result["type"] == "ImportCall");
        CHECK(result["positional_args"][0] == import_kw(nlohmann::json::array({"a"})));
    }
}
