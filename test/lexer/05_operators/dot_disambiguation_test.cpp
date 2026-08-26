// 点号消歧：`.` 在 SL 里身兼四职（小数点 / 属性运算符 / range 运算符 / Ellipsis），
// 而且要求贪婪最长匹配（连续的点尽量多吃，但最多吃 3 个，因为 Ellipsis 是最长的点号 token）。
// 这里把设计阶段过了一遍的六个极端例子原样转成测试，再加几个自己想到的边界情况。
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("点号消歧：设计阶段给出的六个例子") {

    TEST_CASE("`1.f()` 表示调用 1 的 f 方法") {
        CHECK(
            lex_dump(U"1.f()") == "LITERAL_INT(1) SIGN_DOT IDENTIFIER(f) SIGN_LPAREN SIGN_RPAREN"
        );
    }

    TEST_CASE("`1.0.f()` 表示调用 1.0 的 f 方法") {
        CHECK(
            lex_dump(U"1.0.f()") ==
            "LITERAL_DECIMAL(1.0) SIGN_DOT IDENTIFIER(f) SIGN_LPAREN SIGN_RPAREN"
        );
    }

    TEST_CASE("`....f()` 表示调用 Ellipsis 的 f 方法（4 个点 = 3 个点的 Ellipsis + 1 个点）") {
        CHECK(
            lex_dump(U"....f()") ==
            "LITERAL_ELLIPSIS SIGN_DOT IDENTIFIER(f) SIGN_LPAREN SIGN_RPAREN"
        );
    }

    TEST_CASE("`..... ...` 表示 (Ellipsis) .. (Ellipsis)（5 个点 = 3+2，空格，再 3 个点）") {
        CHECK(lex_dump(U"..... ...") == "LITERAL_ELLIPSIS SIGN_DOTDOT LITERAL_ELLIPSIS");
    }

    TEST_CASE("`1.0..1.2` 表示 range(1.0, 1.2)") {
        CHECK(lex_dump(U"1.0..1.2") == "LITERAL_DECIMAL(1.0) SIGN_DOTDOT LITERAL_DECIMAL(1.2)");
    }

    TEST_CASE("`.....1.0` 表示 (Ellipsis) .. (1.0)（5 个点 = 3+2，紧接 1.0）") {
        CHECK(lex_dump(U".....1.0") == "LITERAL_ELLIPSIS SIGN_DOTDOT LITERAL_DECIMAL(1.0)");
    }
}

TEST_SUITE("点号消歧：额外边界情况") {

    TEST_CASE("单个点：属性访问") {
        CHECK(lex_dump(U".") == "SIGN_DOT");
        CHECK(lex_dump(U"x.y") == "IDENTIFIER(x) SIGN_DOT IDENTIFIER(y)");
    }

    TEST_CASE("两个点：range 运算符") {
        CHECK(lex_dump(U"..") == "SIGN_DOTDOT");
        CHECK(lex_dump(U"1..2") == "LITERAL_INT(1) SIGN_DOTDOT LITERAL_INT(2)");
    }

    TEST_CASE("三个点：Ellipsis") { CHECK(lex_dump(U"...") == "LITERAL_ELLIPSIS"); }

    TEST_CASE("贪婪吃法逐个数量验证：N 个连续点按 3,3,3...再余数拆分，不会拆成别的组合") {
        CHECK(lex_dump(U".") == "SIGN_DOT");                                         // 1
        CHECK(lex_dump(U"..") == "SIGN_DOTDOT");                                     // 2
        CHECK(lex_dump(U"...") == "LITERAL_ELLIPSIS");                               // 3
        CHECK(lex_dump(U"....") == "LITERAL_ELLIPSIS SIGN_DOT");                     // 4 = 3+1
        CHECK(lex_dump(U".....") == "LITERAL_ELLIPSIS SIGN_DOTDOT");                 // 5 = 3+2
        CHECK(lex_dump(U"......") == "LITERAL_ELLIPSIS LITERAL_ELLIPSIS");           // 6 = 3+3
        CHECK(lex_dump(U".......") == "LITERAL_ELLIPSIS LITERAL_ELLIPSIS SIGN_DOT"); // 7 = 3+3+1
    }

    TEST_CASE("三个点后紧跟数字：先贪婪吃成 Ellipsis，再单独读数字，不会把点当成数字的一部分") {
        CHECK(lex_dump(U"...5") == "LITERAL_ELLIPSIS LITERAL_INT(5)");
        CHECK(lex_dump(U"...5.5") == "LITERAL_ELLIPSIS LITERAL_DECIMAL(5.5)");
    }

    TEST_CASE("数字紧跟三个点：数字侧只认紧邻自己的单个点，不会把三个点误吃成小数点") {
        CHECK(lex_dump(U"5...") == "LITERAL_INT(5) LITERAL_ELLIPSIS");
    }

    TEST_CASE("两个数字中间夹恰好一个点：小数点，产出一个 decimal") {
        CHECK(lex_dump(U"1.5") == "LITERAL_DECIMAL(1.5)");
    }

    TEST_CASE(
        "两个数字中间夹三个点：不是小数点（数字侧只认单个点接数字），是数字 + Ellipsis + 数字"
    ) {
        CHECK(lex_dump(U"1...2") == "LITERAL_INT(1) LITERAL_ELLIPSIS LITERAL_INT(2)");
    }

    TEST_CASE("整数、range、整数连写不需要空格") {
        CHECK(lex_dump(U"0..10") == "LITERAL_INT(0) SIGN_DOTDOT LITERAL_INT(10)");
    }

    TEST_CASE("range 运算符两侧都是属性访问链也能正确切分") {
        CHECK(
            lex_dump(U"a.b..c.d") ==
            "IDENTIFIER(a) SIGN_DOT IDENTIFIER(b) SIGN_DOTDOT IDENTIFIER(c) SIGN_DOT IDENTIFIER(d)"
        );
    }

    TEST_CASE("Ellipsis 后紧跟属性访问（没有额外的点）") {
        CHECK(lex_dump(U"....x") == "LITERAL_ELLIPSIS SIGN_DOT IDENTIFIER(x)");
        CHECK(lex_dump(U"...x") == "LITERAL_ELLIPSIS IDENTIFIER(x)");
    }
}
