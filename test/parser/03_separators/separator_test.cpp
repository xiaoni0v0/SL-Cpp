// 表达式分隔：软终止换行可续行，硬终止分号切断；if/try 跨行合并。
#include "../../../builtins/exceptions/SyntaxError.h"
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("表达式分隔符——标准例子") {

    TEST_CASE("a = b\\n+ c：第一行完整，不合并，解析成两条表达式") {
        CHECK(
            parse_program_json(U"a = b\n+ c") ==
            nlohmann::json{
                {"type", "Program"},
                {"exprs",
                 nlohmann::json::array(
                     {{{"type", "Assign"}, {"target", ident("a")}, {"value", ident("b")}},
                      {{"type", "OpUnary"}, {"op", "+"}, {"operand", ident("c")}}}
                 )}
            }
        );
    }

    TEST_CASE("d = e +\\nf：二元运算符只有左操作数，不完整，合并成一条表达式") {
        CHECK(
            parse_program_json(U"d = e +\nf") == nlohmann::json{
                                                     {"type", "Program"},
                                                     {"exprs",
                                                      nlohmann::json::array(
                                                          {{{"type", "Assign"},
                                                            {"target", ident("d")},
                                                            {"value",
                                                             {{"type", "OpBinary"},
                                                              {"op", "+"},
                                                              {"left", ident("e")},
                                                              {"right", ident("f")}}}}}
                                                      )}
                                                 }
        );
    }

    TEST_CASE("x.\\nm()：'.' 只有左操作数，不完整，合并") {
        CHECK(
            parse_program_json(U"x.\nm()") ==
            nlohmann::json{
                {"type", "Program"},
                {"exprs",
                 nlohmann::json::array(
                     {{{"type", "Call"},
                       {"object", {{"type", "Attr"}, {"object", ident("x")}, {"attr", "m"}}},
                       {"positional_args", nlohmann::json::array()},
                       {"keyword_args", nlohmann::json::array()}}}
                 )}
            }
        );
    }

    TEST_CASE("x\\n.m()：第一行 x 已经是完整表达式，不合并；第二条以 '.' 开头解析失败，抛异常") {
        CHECK_THROWS_AS(parse_as_file(U"x\n.m()"), SyntaxError);
    }

    TEST_CASE("(x\\n.m()\\n)：括号未闭合，表达式不完整，持续合并直到收尾") {
        CHECK(
            parse_program_json(U"(x\n.m()\n)") ==
            nlohmann::json{
                {"type", "Program"},
                {"exprs",
                 nlohmann::json::array(
                     {{{"type", "Call"},
                       {"object", {{"type", "Attr"}, {"object", ident("x")}, {"attr", "m"}}},
                       {"positional_args", nlohmann::json::array()},
                       {"keyword_args", nlohmann::json::array()}}}
                 )}
            }
        );
    }

    TEST_CASE("(\\n1,\\n2,\\n)：括号内换行不影响元组的解析") {
        CHECK(
            parse_program_json(U"(\n1,\n2,\n)") ==
            nlohmann::json{
                {"type", "Program"},
                {"exprs",
                 nlohmann::json::array(
                     {{{"type", "LiteralTuple"},
                       {"items", nlohmann::json::array({int_lit("1"), int_lit("2")})}}}
                 )}
            }
        );
    }

    TEST_CASE(
        "if (x == 10) x = 100\\nelse x = 200：无 else 且下一条以 else 开头，合并成一条 if 表达式"
    ) {
        const nlohmann::json cond{
            {"type", "Compare"},
            {"operands", nlohmann::json::array({ident("x"), int_lit("10")})},
            {"ops", nlohmann::json::array({"=="})}
        };
        CHECK(
            parse_program_json(U"if (x == 10) x = 100\nelse x = 200") ==
            nlohmann::json{
                {"type", "Program"},
                {"exprs",
                 nlohmann::json::array(
                     {{{"type", "If"},
                       {"clauses",
                        nlohmann::json::array(
                            {{{"cond", cond},
                              {"body",
                               {{"type", "Assign"},
                                {"target", ident("x")},
                                {"value", int_lit("100")}}}}}
                        )},
                       {"else_expr",
                        {{"type", "Assign"}, {"target", ident("x")}, {"value", int_lit("200")}}}}}
                 )}
            }
        );
    }

    TEST_CASE(
        "if (x == 10) x = 100;\\nelse x = 200：显式分号是硬终止，不会合并，"
        "第二条以 else 开头解析失败，抛异常"
    ) {
        CHECK_THROWS_AS(parse_as_file(U"if (x == 10) x = 100;\nelse x = 200"), SyntaxError);
    }
}

TEST_SUITE("表达式分隔符——其他续行场景（try/except/finally 同 if/elif/else 一样的规则）") {

    TEST_CASE("try a\\nexcept (E) b：无 finally 且下一条以 except 开头，合并") {
        CHECK(
            parse_program_json(U"try a\nexcept (E) b") ==
            nlohmann::json{
                {"type", "Program"},
                {"exprs",
                 nlohmann::json::array(
                     {{{"type", "Try"},
                       {"try_expr", ident("a")},
                       {"except_clauses",
                        nlohmann::json::array(
                            {{{"exceptions", nlohmann::json::array({ident("E")})},
                              {"target", nullptr},
                              {"body", ident("b")}}}
                        )},
                       {"finally_expr", nullptr}}}
                 )}
            }
        );
    }

    TEST_CASE("try a\\nfinally b：合并成一条 try 表达式") {
        CHECK(
            parse_program_json(U"try a\nfinally b") ==
            nlohmann::json{
                {"type", "Program"},
                {"exprs",
                 nlohmann::json::array(
                     {{{"type", "Try"},
                       {"try_expr", ident("a")},
                       {"except_clauses", nlohmann::json::array()},
                       {"finally_expr", ident("b")}}}
                 )}
            }
        );
    }

    TEST_CASE("显式分号切断 try 和 except 的合并，第二条以 except 开头解析失败") {
        CHECK_THROWS_AS(parse_as_file(U"try a;\nexcept (E) b"), SyntaxError);
    }

    TEST_CASE("elif 同理可以跨行合并：if (a) x\\nelif (b) y\\nelse z") {
        const AstNodeProgramPtr program{parse_as_file(U"if (a) x\nelif (b) y\nelse z")};
        REQUIRE(program->exprs_.size() == 1);
        const auto *if_node{dynamic_cast<AstNodeIf *>(program->exprs_[0].get())};
        REQUIRE(if_node != nullptr);
        CHECK(if_node->clauses_.size() == 2);
        CHECK(if_node->else_expr_ != nullptr);
    }
}

TEST_SUITE("表达式分隔符——分号/换行的基本切分") {

    TEST_CASE("分号分隔多条表达式") {
        CHECK(
            parse_program_json(U"a; b; c") ==
            nlohmann::json{
                {"type", "Program"},
                {"exprs", nlohmann::json::array({ident("a"), ident("b"), ident("c")})}
            }
        );
    }

    TEST_CASE("换行分隔多条表达式（各自都是完整表达式，不触发合并）") {
        CHECK(
            parse_program_json(U"a\nb\nc") ==
            nlohmann::json{
                {"type", "Program"},
                {"exprs", nlohmann::json::array({ident("a"), ident("b"), ident("c")})}
            }
        );
    }

    TEST_CASE("空行、连续分号都会被忽略，不产生空表达式") {
        CHECK(
            parse_program_json(U"a\n\n\n;;;b") ==
            nlohmann::json{
                {"type", "Program"}, {"exprs", nlohmann::json::array({ident("a"), ident("b")})}
            }
        );
    }

    TEST_CASE(
        "一条表达式结尾不是换行/分号/EOF/} 时报错（如同一行写了两个不相干的表达式），"
        "消息解释清楚缺了什么，位置指向多出来的那个 token"
    ) {
        // "a b" -> a(1) (2)b(3)：报错时 peek() 停在 'b'，位置应指向 'b' 而不是 'a' 或行首
        try {
            parse_as_file(U"a b");
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("expected newline or ';' after expression") != std::string::npos);
            CHECK(msg.find("1:3:") != std::string::npos); // 'b' 的位置
        }
    }

    TEST_CASE("未闭合括号一路合并到 EOF 仍不完整，抛异常（而不是死循环）") {
        CHECK_THROWS_AS(parse_as_file(U"(1 +\n2"), SyntaxError);
    }
}
