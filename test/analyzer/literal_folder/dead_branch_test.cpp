// StaticEvaler：if/elif/else、for/while 的死分支消除。SL.md 3.4.5.1/3.4.5.2/3.4.5.3。
// while 内部复用 AstNodeForCond（init_/inc_ 皆为 nullptr），见
// test/parser/2_2_5_control_flow/while_test.cpp。
#include "../test_utils.h"

#include <doctest/doctest.h>

namespace {
nlohmann::json ident(const char *name) {
    return nlohmann::json{{"type", "Identifier"}, {"identifier", name}};
}
} // namespace

TEST_SUITE("StaticEvaler 死分支消除") {

    TEST_CASE("if(True) 直接替换成对应 body，后面的 elif/else 全部消失") {
        CHECK(fold_json(U"if (True) 1 else 2") == int_lit("1"));
        CHECK(fold_json(U"if (False) 1 elif (True) 2 else 3") == int_lit("2"));
    }

    TEST_CASE("if(False)（且没有更早命中的 True）整个 clause 消失，退到 else_expr 或 None") {
        CHECK(fold_json(U"if (False) 1 else 2") == int_lit("2"));
        CHECK(fold_json(U"if (False) 1") == none_lit());
        CHECK(fold_json(U"if (False) 1 elif (False) 2 else 3") == int_lit("3"));
        CHECK(fold_json(U"if (False) 1 elif (False) 2") == none_lit());
    }

    TEST_CASE("前面若干个确定 False 的 clause 可以先丢掉，剩下不能判定的部分重新拼一个更短的 if") {
        CHECK(
            fold_json(U"if (False) 1 elif (x) 2 else 3") ==
            nlohmann::json{
                {"type", "If"},
                {"clauses", nlohmann::json::array(
                                {nlohmann::json{{"cond", ident("x")}, {"body", int_lit("2")}}}
                            )},
                {"else_expr", int_lit("3")}
            }
        );
    }

    TEST_CASE("第一个 clause 就不能判定，整体不折") {
        CHECK(
            fold_json(U"if (x) 1 else 2") ==
            nlohmann::json{
                {"type", "If"},
                {"clauses", nlohmann::json::array(
                                {nlohmann::json{{"cond", ident("x")}, {"body", int_lit("1")}}}
                            )},
                {"else_expr", int_lit("2")}
            }
        );
    }

    TEST_CASE(
        "while(False) 一次都不会跑，值退化成 SL.md 3.4.5.2/3.4.5.3 的默认值：不收集是 0，收集是 []"
    ) {
        CHECK(fold_json(U"while (False) 1") == int_lit("0"));
        CHECK(
            fold_json(U"while $ (False) 1") ==
            nlohmann::json{{"type", "LiteralList"}, {"items", nlohmann::json::array()}}
        );
    }

    TEST_CASE("for 的 cond 是 False：init 无论如何都会先无条件求值一次，副作用必须保留") {
        CHECK(
            fold_json(U"for (x = 1; False; x = 2) body") ==
            nlohmann::json{
                {"type", "Compound"},
                {"exprs",
                 nlohmann::json::array(
                     {nlohmann::json{
                          {"type", "Assign"}, {"target", ident("x")}, {"value", int_lit("1")}
                      },
                      int_lit("0")}
                 )}
            }
        );
        CHECK(fold_json(U"for (; False; x = 2) body") == int_lit("0")); // 没有 init，不用管副作用
    }

    TEST_CASE("cond 折成 True 不折——只确定不会提前退出，循环本身的值仍然没法在编译期知道") {
        CHECK(
            fold_json(U"while (True) 1") == nlohmann::json{
                                                {"type", "ForCond"},
                                                {"collect", false},
                                                {"init", nullptr},
                                                {"cond", bool_lit(true)},
                                                {"inc", nullptr},
                                                {"body", int_lit("1")}
                                            }
        );
    }

    TEST_CASE("cond 不是字面量，或者压根没有 cond（视为无限循环），都不折") {
        CHECK(
            fold_json(U"while (x) 1") == nlohmann::json{
                                             {"type", "ForCond"},
                                             {"collect", false},
                                             {"init", nullptr},
                                             {"cond", ident("x")},
                                             {"inc", nullptr},
                                             {"body", int_lit("1")}
                                         }
        );
        CHECK(
            fold_json(U"for (;;) 1") == nlohmann::json{
                                            {"type", "ForCond"},
                                            {"collect", false},
                                            {"init", nullptr},
                                            {"cond", nullptr},
                                            {"inc", nullptr},
                                            {"body", int_lit("1")}
                                        }
        );
    }
}
