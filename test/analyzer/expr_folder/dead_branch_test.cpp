// if/for/while 死分支、死循环消除。
#include "../test_utils.h"

#include <doctest/doctest.h>

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
                {"clauses",
                 nlohmann::json::array(
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
                {"clauses",
                 nlohmann::json::array(
                     {nlohmann::json{{"cond", ident("x")}, {"body", int_lit("1")}}}
                 )},
                {"else_expr", int_lit("2")}
            }
        );
    }

    TEST_CASE("while(False) 一次都不跑：不收集折成 0，收集折成 []") {
        const auto empty_list =
            nlohmann::json{{"type", "LiteralList"}, {"items", nlohmann::json::array()}};

        CHECK(fold_json(U"while (False) 1") == int_lit("0"));
        CHECK(fold_json(U"while $ (False) 1") == empty_list);
        // 展开与否不影响结果容器，$ * 一样折成空列表
        CHECK(fold_json(U"while $ * (False) 1") == empty_list);
    }

    TEST_CASE("$$ 一次都不会跑也不折——空 dict 写不出字面量，折不出等价的节点") {
        CHECK(
            fold_json(U"while $$ (False) 1") == nlohmann::json{
                                                    {"type", "ForCond"},
                                                    {"collect", "$$"},
                                                    {"init", nullptr},
                                                    {"cond", bool_lit(false)},
                                                    {"inc", nullptr},
                                                    {"body", int_lit("1")}
                                                }
        );
        CHECK(
            fold_json(U"while $$ ** (False) 1") == nlohmann::json{
                                                       {"type", "ForCond"},
                                                       {"collect", "$$ **"},
                                                       {"init", nullptr},
                                                       {"cond", bool_lit(false)},
                                                       {"inc", nullptr},
                                                       {"body", int_lit("1")}
                                                   }
        );
        // 不折整个节点，但 init 这种子表达式该折还是照折
        CHECK(fold_json(U"for $$ (x = 1 + 1; False;) 1")["init"]["value"] == int_lit("2"));
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
                                                {"collect", "none"},
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
                                             {"collect", "none"},
                                             {"init", nullptr},
                                             {"cond", ident("x")},
                                             {"inc", nullptr},
                                             {"body", int_lit("1")}
                                         }
        );
        CHECK(
            fold_json(U"for (;;) 1") == nlohmann::json{
                                            {"type", "ForCond"},
                                            {"collect", "none"},
                                            {"init", nullptr},
                                            {"cond", nullptr},
                                            {"inc", nullptr},
                                            {"body", int_lit("1")}
                                        }
        );
    }
}
