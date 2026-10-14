// try / except / finally。except 和 finally 不能同时省略（语义层）。
#include "../../../cppexceptions/SyntaxError.h"
#include "../test_utils.h"

#include <string>

#include <doctest/doctest.h>

namespace {

nlohmann::json except_clause(
    const nlohmann::json &target, const nlohmann::json &type_annotation,
    const nlohmann::json &exc_type, const nlohmann::json &body, const bool is_const = false
) {
    return nlohmann::json{
        {"binding", binding(target, type_annotation, is_const)},
        {"exc_type", exc_type},
        {"body", body}
    };
}

} // namespace

TEST_SUITE("try") {

    TEST_CASE("单个 except") {
        CHECK(
            parse_json(U"try a except (E) b") ==
            nlohmann::json{
                {"type", "Try"},
                {"try_expr", ident("a")},
                {"except_clauses",
                 nlohmann::json::array({except_clause(nullptr, nullptr, ident("E"), ident("b"))})},
                {"finally_expr", nullptr}
            }
        );
    }

    TEST_CASE("多个异常类用 | 拼成一个表达式") {
        CHECK(
            parse_json(U"try a except (E1 | E2 | E3) b")["except_clauses"][0]["exc_type"] ==
            nlohmann::json{
                {"type", "OpBinary"},
                {"op", "|"},
                {"left",
                 {{"type", "OpBinary"},
                  {"op", "|"},
                  {"left", ident("E1")},
                  {"right", ident("E2")}}},
                {"right", ident("E3")}
            }
        );
    }

    TEST_CASE("逗号分隔的异常类列表不再支持，报错时提示用 |") {
        check_parse_throws_with(U"try a except (E1, E2) b", "use '|'");
        check_parse_throws_with(U"try a except (e from E1, E2) b", "use '|'");
    }

    TEST_CASE("多个 except 子句 + finally") {
        CHECK(
            parse_json(U"try a except (E1) b except (E2) c finally d") ==
            nlohmann::json{
                {"type", "Try"},
                {"try_expr", ident("a")},
                {"except_clauses",
                 nlohmann::json::array(
                     {except_clause(nullptr, nullptr, ident("E1"), ident("b")),
                      except_clause(nullptr, nullptr, ident("E2"), ident("c"))}
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

    TEST_CASE("嵌套 try 的 finally 绑到内层：try try a finally b finally c") {
        CHECK(
            parse_json(U"try try a finally b finally c") ==
            nlohmann::json{
                {"type", "Try"},
                {"try_expr",
                 {{"type", "Try"},
                  {"try_expr", ident("a")},
                  {"except_clauses", nlohmann::json::array()},
                  {"finally_expr", ident("b")}}},
                {"except_clauses", nlohmann::json::array()},
                {"finally_expr", ident("c")}
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

    TEST_CASE("except 子句括号内必须有异常类，位置指向空括号里的 ')'") {
        // "try a except () b" -> t(1)r(2)y(3) (4)a(5) (6)e(7)x(8)c(9)e(10)p(11)t(12) (13)((14))(15)
        // (16)b(17)
        try {
            parse_program(U"try a except () b");
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("except requires an exception type") != std::string::npos);
            CHECK(msg.find("1:15:") != std::string::npos);
        }
    }

    TEST_CASE("未闭合括号/缺 body 报错") {
        CHECK_THROWS_AS(parse_program(U"try a except (E"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"try a except (E)"), SyntaxError);
        check_parse_throws_with(U"try a except (E b", "expected ')' to close the except clause");
    }
}

TEST_SUITE("try——except 的 from 绑定") {

    // 绑定写在括号里：')' 仍然是"头部到此为止"的可靠信号，body 从哪开始永远确定。

    TEST_CASE("target from exc_type") {
        CHECK(
            parse_json(U"try a except (e from E) b")["except_clauses"][0] ==
            except_clause(ident("e"), nullptr, ident("E"), ident("b"))
        );
    }

    TEST_CASE("target : type from exc_type，类型和用来匹配的异常类互相独立") {
        CHECK(
            parse_json(U"try a except (e: Exception from E1 | E2) b")["except_clauses"][0] ==
            except_clause(
                ident("e"),
                ident("Exception"),
                {{"type", "OpBinary"}, {"op", "|"}, {"left", ident("E1")}, {"right", ident("E2")}},
                ident("b")
            )
        );
    }

    TEST_CASE("from 可选：不写就是不关心异常对象，target 为空") {
        const auto result = parse_json(U"try a except (E) b");
        CHECK(result["except_clauses"][0]["binding"] == nullptr);
    }

    TEST_CASE("每个 except 子句各自独立决定写不写 from") {
        const auto result = parse_json(U"try a except (e from E1) b except (E2) c");
        CHECK(result["except_clauses"][0]["binding"]["target"] == ident("e"));
        CHECK(result["except_clauses"][1]["binding"] == nullptr);
    }

    TEST_CASE("不带类型时目标是任意左值形状（左值校验交语义层）") {
        CHECK(
            parse_json(
                U"try a except (x.y from E) b"
            )["except_clauses"][0]["binding"]["target"]["type"] == "Attr"
        );
        CHECK(
            parse_json(
                U"try a except (x[0] from E) b"
            )["except_clauses"][0]["binding"]["target"]["type"] == "Index"
        );
        CHECK(
            parse_json(
                U"try a except ((p, q) from E) b"
            )["except_clauses"][0]["binding"]["target"]["type"] == "LiteralTuple"
        );
    }

    TEST_CASE("带类型时目标只能是标识符") {
        check_parse_throws_with(
            U"try a except (x.y: T from E) b", "only an identifier can be declared"
        );
    }

    TEST_CASE("注解里可以写 const，类型可省") {
        CHECK(
            parse_json(U"try a except (e: const E from E) b")["except_clauses"][0] ==
            except_clause(ident("e"), ident("E"), ident("E"), ident("b"), true)
        );
        CHECK(
            parse_json(U"try a except (e: const from E) b")["except_clauses"][0] ==
            except_clause(ident("e"), nullptr, ident("E"), ident("b"), true)
        );
    }

    TEST_CASE("没写 from 时括号里整个是异常类表达式，写成声明也照常解析（交给运行期）") {
        CHECK(
            parse_json(U"try a except (e: E) b")["except_clauses"][0] ==
            except_clause(nullptr, nullptr, declare("e", false, ident("E"), nullptr), ident("b"))
        );
        CHECK(
            parse_json(U"try a except (e: E = F) b")["except_clauses"][0]["exc_type"] ==
            declare("e", false, ident("E"), ident("F"))
        );
        CHECK(parse_json(U"try a except ((e: E)) b") == parse_json(U"try a except (e: E) b"));
    }

    TEST_CASE("from 和 ':' 之后可以换行（括号内换行照常当空白）") {
        CHECK(
            parse_json(U"try a except (e\nfrom\nE) b")["except_clauses"][0] ==
            except_clause(ident("e"), nullptr, ident("E"), ident("b"))
        );
        CHECK(
            parse_json(U"try a except (e:\nT\nfrom E) b")["except_clauses"][0] ==
            except_clause(ident("e"), ident("T"), ident("E"), ident("b"))
        );
    }

    TEST_CASE("from 之后缺异常类、from 写在括号外、as 都报错") {
        CHECK_THROWS_AS(parse_program(U"try a except (e from) b"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"try a except (E) from e b"), SyntaxError);
        CHECK_THROWS_AS(parse_program(U"try a except (E as e) b"), SyntaxError);
    }

    TEST_CASE("body 以 '[' 开头也不会跟目标粘在一起：')' 已经把头部封死") {
        const auto result = parse_json(U"try a except (e from E) [1, 2]");
        CHECK(result["except_clauses"][0]["binding"]["target"] == ident("e"));
        CHECK(result["except_clauses"][0]["body"]["type"] == "LiteralList");
    }
}
