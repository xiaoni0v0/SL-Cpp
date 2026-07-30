// SemanticChecker：那些"只有 Parser 出 bug 才会触发"的防御性断言。
// 这类畸形的 AST 没法通过解析任何合法或不合法的源码构造出来（Parser 自己的语法/结构性保证决定了
// 这几个字段永远满足对应的形状），只能像这里一样手工搭一棵树直接喂给 SemanticChecker，用来确认这些
// 断言本身在真的遇到畸形输入时确实会正确报错，而不是静默放过或者直接崩溃。
#include "test_utils.h"

#include <doctest/doctest.h>

namespace {
AstNodePtr int_lit() { return std::make_unique<AstNodeLiteralInt>(Position{0, 0}, U"1"); }

AstNodeProgramPtr wrap(AstNodePtr expr) {
    std::vector<AstNodePtr> exprs;
    exprs.push_back(std::move(expr));
    return std::make_unique<AstNodeProgram>(Position{0, 0}, std::move(exprs));
}
} // namespace

TEST_SUITE("SemanticChecker 防御性断言（畸形 AST，正常解析永远构造不出来）") {

    TEST_CASE("AstNodeIf::clauses_ 为空") {
        std::vector<AstNodeIf::AstNodeCondAndExpr> clauses;
        AstNodeProgramPtr program{
            wrap(std::make_unique<AstNodeIf>(Position{0, 0}, std::move(clauses), nullptr))
        };
        check_throws_internal_error_with(*program, "too few elements");
    }

    TEST_CASE("AstNodeCompare：operands_ 数量跟 ops_ + 1 对不上") {
        std::vector<AstNodeCompare::OpType> ops{AstNodeCompare::OpType::Lt};
        std::vector<AstNodePtr> operands;
        operands.push_back(int_lit());
        operands.push_back(int_lit());
        operands.push_back(int_lit()); // 3 个 operand 却只有 1 个 op，应该是 2 个
        std::vector<Position> op_positions{Position{0, 0}};
        AstNodeProgramPtr program{wrap(
            std::make_unique<AstNodeCompare>(
                Position{0, 0}, std::move(ops), std::move(operands), std::move(op_positions)
            )
        )};
        check_throws_internal_error_with(*program, "mismatched count");
    }

    TEST_CASE("AstNodeCompare：operands_ 少于 2 个") {
        std::vector<AstNodeCompare::OpType> ops;
        std::vector<AstNodePtr> operands;
        operands.push_back(int_lit());
        std::vector<Position> op_positions;
        AstNodeProgramPtr program{wrap(
            std::make_unique<AstNodeCompare>(
                Position{0, 0}, std::move(ops), std::move(operands), std::move(op_positions)
            )
        )};
        check_throws_internal_error_with(*program, "too few elements");
    }

    TEST_CASE("AstNodeIs：operands_ 少于 2 个") {
        std::vector<AstNodePtr> operands;
        operands.push_back(int_lit());
        std::vector<Position> op_positions;
        AstNodeProgramPtr program{wrap(
            std::make_unique<AstNodeIs>(
                Position{0, 0}, std::move(operands), std::move(op_positions)
            )
        )};
        check_throws_internal_error_with(*program, "too few elements");
    }

    TEST_CASE(
        "AstNodeLiteralDict：key 是 ** 展开，但 val 不是空指针（真正的 Parser "
        "永远不会产出这种组合，"
        "见 test/parser/2_2_2_basic_exprs/compound_dict_test.cpp 里 {**d: v} 直接是 SyntaxError）"
    ) {
        std::vector<std::pair<AstNodePtr, AstNodePtr>> items;
        items.emplace_back(
            std::make_unique<AstNodeDoubleStar>(
                Position{0, 0}, std::make_unique<AstNodeIdentifier>(Position{0, 0}, U"d")
            ),
            int_lit() // 不该有值，正确的 Parser 输出这里永远是 nullptr
        );
        AstNodeProgramPtr program{
            wrap(std::make_unique<AstNodeLiteralDict>(Position{0, 0}, std::move(items)))
        };
        check_throws_internal_error_with(*program, "** dict-spread entry must not have a value");
    }

    TEST_CASE("AstNodeFunc/AstNodeClass：name_ 是空字符串（应该要么 nullopt 要么有内容）") {
        AstNodeProgramPtr func_program{wrap(
            std::make_unique<AstNodeFunc>(
                Position{0, 0},
                std::vector<AstNodePtr>{},
                std::vector<Position>{},
                std::optional<std::u32string>{U""},
                std::vector<OneCapture>{},
                AstNodeFunc::AllParams{},
                nullptr,
                nullptr,
                std::make_unique<AstNodeProgram>(Position{0, 0}, std::vector<AstNodePtr>{})
            )
        )};
        check_throws_internal_error_with(*func_program, "unexpected empty name");

        AstNodeProgramPtr class_program{wrap(
            std::make_unique<AstNodeClass>(
                Position{0, 0},
                std::vector<AstNodePtr>{},
                std::vector<Position>{},
                std::optional<std::u32string>{U""},
                std::vector<AstNodePtr>{},
                std::vector<OneCapture>{},
                nullptr,
                std::make_unique<AstNodeProgram>(Position{0, 0}, std::vector<AstNodePtr>{})
            )
        )};
        check_throws_internal_error_with(*class_program, "unexpected empty name");
    }

    TEST_CASE("AstNodeFunc/AstNodeClass：decorators_ 和 decorator_positions_ 数量对不上") {
        std::vector<AstNodePtr> decorators;
        decorators.push_back(int_lit());
        AstNodeProgramPtr func_program{wrap(
            std::make_unique<AstNodeFunc>(
                Position{0, 0},
                std::move(decorators),
                std::vector<Position>{},
                std::nullopt,
                std::vector<OneCapture>{},
                AstNodeFunc::AllParams{},
                nullptr,
                nullptr,
                std::make_unique<AstNodeProgram>(Position{0, 0}, std::vector<AstNodePtr>{})
            )
        )};
        check_throws_internal_error_with(*func_program, "mismatched array sizes");
    }

    TEST_CASE("AstNodeClass：captures_ 里某一项 identifier_ 是空字符串") {
        std::vector<OneCapture> captures;
        captures.push_back({OneCapture::CaptureType::Value, U"", nullptr});
        AstNodeProgramPtr program{wrap(
            std::make_unique<AstNodeClass>(
                Position{0, 0},
                std::vector<AstNodePtr>{},
                std::vector<Position>{},
                std::nullopt,
                std::vector<AstNodePtr>{},
                std::move(captures),
                nullptr,
                std::make_unique<AstNodeProgram>(Position{0, 0}, std::vector<AstNodePtr>{})
            )
        )};
        check_throws_internal_error_with(*program, "unexpected empty name");
    }

    TEST_CASE(
        "AstNodeFunc：var_args_name_/var_kwargs_name_ 是空字符串（应该要么 nullopt 要么有内容）"
    ) {
        AstNodeFunc::AllParams params1;
        params1.var_args_name_ = U"";
        AstNodeProgramPtr program1{wrap(
            std::make_unique<AstNodeFunc>(
                Position{0, 0},
                std::vector<AstNodePtr>{},
                std::vector<Position>{},
                std::nullopt,
                std::vector<OneCapture>{},
                std::move(params1),
                nullptr,
                nullptr,
                std::make_unique<AstNodeProgram>(Position{0, 0}, std::vector<AstNodePtr>{})
            )
        )};
        check_throws_internal_error_with(*program1, "unexpected empty name");

        AstNodeFunc::AllParams params2;
        params2.var_kwargs_name_ = U"";
        AstNodeProgramPtr program2{wrap(
            std::make_unique<AstNodeFunc>(
                Position{0, 0},
                std::vector<AstNodePtr>{},
                std::vector<Position>{},
                std::nullopt,
                std::vector<OneCapture>{},
                std::move(params2),
                nullptr,
                nullptr,
                std::make_unique<AstNodeProgram>(Position{0, 0}, std::vector<AstNodePtr>{})
            )
        )};
        check_throws_internal_error_with(*program2, "unexpected empty name");
    }

    TEST_CASE("AstNodeIndex：args_ 为空（a[] 语法上不允许，Parser 已经保证，这里是防御性断言）") {
        AstNodeProgramPtr program{wrap(
            std::make_unique<AstNodeIndex>(
                Position{0, 0},
                std::make_unique<AstNodeIdentifier>(Position{0, 0}, U"a"),
                std::vector<AstNodePtr>{},
                Position{0, 0}
            )
        )};
        check_throws_internal_error_with(*program, "too few elements");
    }

    TEST_CASE("AstNodeGlobal：identifier_ 是空字符串") {
        AstNodeProgramPtr program{wrap(std::make_unique<AstNodeGlobal>(Position{0, 0}, U""))};
        check_throws_internal_error_with(*program, "unexpected empty name");
    }

    TEST_CASE("AstNodeLiteralInt：raw_ 是空字符串") {
        AstNodeProgramPtr program{wrap(std::make_unique<AstNodeLiteralInt>(Position{0, 0}, U""))};
        check_throws_internal_error_with(*program, "missing digits");
    }

    TEST_CASE("AstNodeLiteralInt：raw_ 含非数字字符") {
        AstNodeProgramPtr program{
            wrap(std::make_unique<AstNodeLiteralInt>(Position{0, 0}, U"12a"))
        };
        check_throws_internal_error_with(*program, "non-digit character");
    }

    TEST_CASE("AstNodeLiteralInt：raw_ 有前导零（单独一个 \"0\" 除外，SL.md 2.1.4）") {
        AstNodeProgramPtr program{
            wrap(std::make_unique<AstNodeLiteralInt>(Position{0, 0}, U"007"))
        };
        check_throws_internal_error_with(*program, "leading zero");
        // 单独一个 "0" 是合法的，不应该报错
        CHECK_NOTHROW(check_ast(*wrap(std::make_unique<AstNodeLiteralInt>(Position{0, 0}, U"0"))));
    }

    TEST_CASE("AstNodeLiteralFloat：raw_ 缺少小数点") {
        AstNodeProgramPtr program{
            wrap(std::make_unique<AstNodeLiteralFloat>(Position{0, 0}, U"123"))
        };
        check_throws_internal_error_with(*program, "missing a '.'");
    }

    TEST_CASE("AstNodeLiteralFloat：raw_ 小数点两侧缺数字") {
        check_throws_internal_error_with(
            *wrap(std::make_unique<AstNodeLiteralFloat>(Position{0, 0}, U"1.")), "missing digits"
        );
        check_throws_internal_error_with(
            *wrap(std::make_unique<AstNodeLiteralFloat>(Position{0, 0}, U".5")), "missing digits"
        );
    }

    TEST_CASE("AstNodeLiteralFloat：raw_ 含非数字字符") {
        AstNodeProgramPtr program{
            wrap(std::make_unique<AstNodeLiteralFloat>(Position{0, 0}, U"1.5a"))
        };
        check_throws_internal_error_with(*program, "non-digit character");
    }

    TEST_CASE("AstNodeLiteralFloat：整数部分有前导零，跟 int 一致；小数部分没有这条限制") {
        AstNodeProgramPtr program{
            wrap(std::make_unique<AstNodeLiteralFloat>(Position{0, 0}, U"007.5"))
        };
        check_throws_internal_error_with(*program, "leading zero");
        // 小数部分的零不受限制
        CHECK_NOTHROW(
            check_ast(*wrap(std::make_unique<AstNodeLiteralFloat>(Position{0, 0}, U"0.05")))
        );
    }

    TEST_CASE(
        "AstNodeTry：某个 except 子句的 exceptions_ 为空（裸 except 语法上不允许，"
        "Parser 已经保证，这里是防御性断言）"
    ) {
        std::vector<AstNodeTry::AstNodeExceptAndExpr> except_clauses;
        except_clauses.emplace_back(std::vector<AstNodePtr>{}, int_lit());
        AstNodeProgramPtr program{wrap(
            std::make_unique<AstNodeTry>(
                Position{0, 0}, int_lit(), std::move(except_clauses), nullptr
            )
        )};
        check_throws_internal_error_with(*program, "too few elements");
    }
}
