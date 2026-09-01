// try / except / finally。except 和 finally 不能同时省略（语义层）。
#include "../../../builtins/exceptions/SyntaxError.h"
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("try") {

    TEST_CASE("单个 except") {
        CHECK(
            parse_json(U"try a except (E) b") ==
            nlohmann::json{
                {"type", "Try"},
                {"try_expr", ident("a")},
                {"except_clauses",
                 nlohmann::json::array(
                     {{{"exceptions", nlohmann::json::array({ident("E")})},
                       {"target", nullptr},
                       {"body", ident("b")}}}
                 )},
                {"finally_expr", nullptr}
            }
        );
    }

    TEST_CASE("一个 except 子句里可以有多个异常类型") {
        CHECK(
            parse_json(U"try a except (E1, E2, E3) b") ==
            nlohmann::json{
                {"type", "Try"},
                {"try_expr", ident("a")},
                {"except_clauses",
                 nlohmann::json::array(
                     {{{"exceptions",
                        nlohmann::json::array({ident("E1"), ident("E2"), ident("E3")})},
                       {"target", nullptr},
                       {"body", ident("b")}}}
                 )},
                {"finally_expr", nullptr}
            }
        );
    }

    TEST_CASE("多个 except 子句 + finally") {
        CHECK(
            parse_json(U"try a except (E1) b except (E2) c finally d") ==
            nlohmann::json{
                {"type", "Try"},
                {"try_expr", ident("a")},
                {"except_clauses",
                 nlohmann::json::array(
                     {{{"exceptions", nlohmann::json::array({ident("E1")})},
                       {"target", nullptr},
                       {"body", ident("b")}},
                      {{"exceptions", nlohmann::json::array({ident("E2")})},
                       {"target", nullptr},
                       {"body", ident("c")}}}
                 )},
                {"finally_expr", ident("d")}
            }
        );
    }

    TEST_CASE("只有 finally，没有 except") {
        CHECK(
            parse_json(U"try a finally b") == nlohmann::json{
                                                  {"type", "Try"},
                                                  {"try_expr", ident("a")},
                                                  {"except_clauses", nlohmann::json::array()},
                                                  {"finally_expr", ident("b")}
                                              }
        );
    }

    TEST_CASE("语法层允许 except 和 finally 都不写（该约束交语义层校验）") {
        CHECK(
            parse_json(U"try a") == nlohmann::json{
                                        {"type", "Try"},
                                        {"try_expr", ident("a")},
                                        {"except_clauses", nlohmann::json::array()},
                                        {"finally_expr", nullptr}
                                    }
        );
    }

    TEST_CASE("except 子句括号内至少要有一个异常表达式，位置指向空括号里的 ')'") {
        // "try a except () b" -> t(1)r(2)y(3) (4)a(5) (6)e(7)x(8)c(9)e(10)p(11)t(12) (13)((14))(15)
        // (16)b(17)
        try {
            parse_as_file(U"try a except () b");
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("except requires at least one exception type") != std::string::npos);
            CHECK(msg.find("1:15:") != std::string::npos);
        }
    }

    TEST_CASE("未闭合括号/缺 body 报错") {
        CHECK_THROWS_AS(parse_as_file(U"try a except (E"), SyntaxError);
        CHECK_THROWS_AS(parse_as_file(U"try a except (E)"), SyntaxError);
    }
}

TEST_SUITE("try——except 的 as 绑定") {

    // as 在括号里，不在括号外：')' 仍然是"头部到此为止"的可靠信号，body 从哪开始永远确定。
    // 放到括号外的话，`except (E) as a [1, 2]` 里的 `a [1, 2]` 会被贪心吃成索引左值，
    // 跟"目标是 a、body 是列表 [1, 2]"这个意图撞车。

    TEST_CASE("单个异常类型 + as") {
        CHECK(
            parse_json(U"try a except (E as e) b") ==
            nlohmann::json{
                {"type", "Try"},
                {"try_expr", ident("a")},
                {"except_clauses",
                 nlohmann::json::array(
                     {{{"exceptions", nlohmann::json::array({ident("E")})},
                       {"target", ident("e")},
                       {"body", ident("b")}}}
                 )},
                {"finally_expr", nullptr}
            }
        );
    }

    TEST_CASE("多个异常类型时 as 绑定的是整个子句，不是最后那个类型") {
        CHECK(
            parse_json(U"try a except (E1, E2 as e) b") ==
            nlohmann::json{
                {"type", "Try"},
                {"try_expr", ident("a")},
                {"except_clauses",
                 nlohmann::json::array(
                     {{{"exceptions", nlohmann::json::array({ident("E1"), ident("E2")})},
                       {"target", ident("e")},
                       {"body", ident("b")}}}
                 )},
                {"finally_expr", nullptr}
            }
        );
    }

    TEST_CASE("as 可选：不写就是不关心异常对象，target 为空") {
        const auto result = parse_json(U"try a except (E) b");
        CHECK(result["except_clauses"][0]["target"] == nullptr);
    }

    TEST_CASE("每个 except 子句各自独立决定写不写 as") {
        const auto result = parse_json(U"try a except (E1 as e) b except (E2) c");
        CHECK(result["except_clauses"][0]["target"] == ident("e"));
        CHECK(result["except_clauses"][1]["target"] == nullptr);
    }

    TEST_CASE("目标是表达式，语法层放行任意形状（左值校验交语义层），解构也能写") {
        CHECK(
            parse_json(U"try a except (E as x.y) b")["except_clauses"][0]["target"]["type"] ==
            "Attr"
        );
        CHECK(
            parse_json(U"try a except (E as x[0]) b")["except_clauses"][0]["target"]["type"] ==
            "Index"
        );
        CHECK(
            parse_json(U"try a except (E as (p, q)) b")["except_clauses"][0]["target"]["type"] ==
            "LiteralTuple"
        );
    }

    TEST_CASE("as 之后可以换行（括号内换行照常当空白）") {
        CHECK(parse_json(U"try a except (E as\ne) b")["except_clauses"][0]["target"] == ident("e"));
    }

    TEST_CASE("as 之后缺目标、as 写在括号外都报错") {
        CHECK_THROWS_AS(parse_as_file(U"try a except (E as) b"), SyntaxError);
        CHECK_THROWS_AS(parse_as_file(U"try a except (E) as e b"), SyntaxError);
    }

    TEST_CASE("body 以 '[' 开头也不会跟目标粘在一起——as 在括号内，')' 已经把头部封死") {
        const auto result = parse_json(U"try a except (E as e) [1, 2]");
        CHECK(result["except_clauses"][0]["target"] == ident("e"));
        CHECK(result["except_clauses"][0]["body"]["type"] == "LiteralList");
    }
}
