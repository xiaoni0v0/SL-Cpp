// SemanticChecker：那些"只有 Parser 出 bug 才会触发"的**结构性**防御断言（空指针、数量对不上、
// 标志位互相矛盾）。这类畸形的 AST 没法通过解析任何合法或不合法的源码构造出来（Parser 自己的
// 语法/结构性保证决定了这几个字段永远满足对应的形状），只能像这里一样手工搭一棵树直接喂给
// SemanticChecker，用来确认这些断言本身在真的遇到畸形输入时确实会正确报错，而不是静默放过或者
// 直接崩溃。
//
// int/decimal 字面量 raw_ 的形状校验同样是防御性的，但它归节点自己的构造函数管、不归
// SemanticChecker 管（常量折叠造出来的字面量节点不会再经过 SemanticChecker），测试在
// test/parser/01_literals/literal_raw_test.cpp。
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
        check_throws_internal_error_with(*program, "operands/ops count mismatch");
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
        "见 test/parser/04_basic_exprs/compound_dict_test.cpp 里 {**d: v} 直接是 SyntaxError）"
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

    TEST_CASE("AstNodeImportKw：segments_ 为空（关键字形态至少有一段）") {
        AstNodeProgramPtr program{
            wrap(std::make_unique<AstNodeImportKw>(Position{0, 0}, std::vector<std::u32string>{}))
        };
        check_throws_internal_error_with(*program, "too few elements");
    }

    TEST_CASE("AstNodeImportKw：某一段是空字符串") {
        AstNodeProgramPtr program{wrap(
            std::make_unique<AstNodeImportKw>(
                Position{0, 0}, std::vector<std::u32string>{U"os", U""}
            )
        )};
        check_throws_internal_error_with(*program, "unexpected empty name");
    }

    TEST_CASE("AstNodeImportCall：关键字实参的 keyword_ 是空字符串") {
        std::vector<OneKwArg> keyword_args;
        keyword_args.push_back({OneKwArg::Kind::Keyword, U"", int_lit()});
        AstNodeProgramPtr program{wrap(
            std::make_unique<AstNodeImportCall>(
                Position{0, 0}, std::vector<AstNodePtr>{}, std::move(keyword_args), Position{0, 0}
            )
        )};
        check_throws_internal_error_with(*program, "unexpected empty name");
    }

    TEST_CASE("AstNodeImportCall：位置实参里有空指针") {
        std::vector<AstNodePtr> positional_args;
        positional_args.push_back(nullptr);
        AstNodeProgramPtr program{wrap(
            std::make_unique<AstNodeImportCall>(
                Position{0, 0}, std::move(positional_args), std::vector<OneKwArg>{}, Position{0, 0}
            )
        )};
        check_throws_internal_error_with(*program, "unexpected null node");
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

    TEST_CASE("AstNodeForCond：没有结果容器却带着展开标志") {
        AstNodeProgramPtr program{wrap(
            std::make_unique<AstNodeForCond>(
                Position{0, 0},
                CollectMark{CollectMark::Container::None, true},
                nullptr,
                nullptr,
                nullptr,
                int_lit()
            )
        )};
        check_throws_internal_error_with(*program, "expand flag without a collect container");
    }

    TEST_CASE("AstNodeForIter：没有结果容器却带着展开标志") {
        AstNodeProgramPtr program{wrap(
            std::make_unique<AstNodeForIter>(
                Position{0, 0},
                CollectMark{CollectMark::Container::None, true},
                std::make_unique<AstNodeIdentifier>(Position{0, 0}, U"i"),
                std::make_unique<AstNodeIdentifier>(Position{0, 0}, U"xs"),
                int_lit()
            )
        )};
        check_throws_internal_error_with(*program, "expand flag without a collect container");
    }

    TEST_CASE("四种合法记号本身不该被这条断言误伤") {
        const auto build{[](const CollectMark mark) {
            return wrap(
                std::make_unique<AstNodeForCond>(
                    Position{0, 0}, mark, nullptr, nullptr, nullptr, int_lit()
                )
            );
        }};
        CHECK_NOTHROW(check_ast(*build({CollectMark::Container::None, false})));
        CHECK_NOTHROW(check_ast(*build({CollectMark::Container::List, false})));
        CHECK_NOTHROW(check_ast(*build({CollectMark::Container::List, true})));
        CHECK_NOTHROW(check_ast(*build({CollectMark::Container::Dict, false})));
        CHECK_NOTHROW(check_ast(*build({CollectMark::Container::Dict, true})));
    }
}
