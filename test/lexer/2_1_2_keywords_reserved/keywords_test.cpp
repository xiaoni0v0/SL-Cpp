// SL.md 2.1.2 关键字与保留字——关键字部分
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("2.1.2 关键字") {

    TEST_CASE("字面量关键字：None / True / False / _G / _L") {
        CHECK(lex_dump(U"None") == "LITERAL_NONE");
        CHECK(lex_dump(U"True") == "LITERAL_TRUE");
        CHECK(lex_dump(U"False") == "LITERAL_FALSE");
        CHECK(lex_dump(U"_G") == "LITERAL_G");
        CHECK(lex_dump(U"_L") == "LITERAL_L");
    }

    TEST_CASE("普通关键字：逐个识别（2.1.2 列出的全部，含 while）") {
        CHECK(lex_dump(U"not") == "KW_NOT");
        CHECK(lex_dump(U"and") == "KW_AND");
        CHECK(lex_dump(U"or") == "KW_OR");
        CHECK(lex_dump(U"is") == "KW_IS");
        CHECK(lex_dump(U"del") == "KW_DEL");
        CHECK(lex_dump(U"global") == "KW_GLOBAL");
        CHECK(lex_dump(U"if") == "KW_IF");
        CHECK(lex_dump(U"elif") == "KW_ELIF");
        CHECK(lex_dump(U"else") == "KW_ELSE");
        CHECK(lex_dump(U"for") == "KW_FOR");
        CHECK(lex_dump(U"while") == "KW_WHILE"); // 之前分词器完全没有的那个
        CHECK(lex_dump(U"break") == "KW_BREAK");
        CHECK(lex_dump(U"continue") == "KW_CONTINUE");
        CHECK(lex_dump(U"func") == "KW_FUNC");
        CHECK(lex_dump(U"return") == "KW_RETURN");
        CHECK(lex_dump(U"raise") == "KW_RAISE");
        CHECK(lex_dump(U"try") == "KW_TRY");
        CHECK(lex_dump(U"except") == "KW_EXCEPT");
        CHECK(lex_dump(U"finally") == "KW_FINALLY");
        CHECK(lex_dump(U"class") == "KW_CLASS");
    }

    TEST_CASE("关键字区分大小写：大小写变体一律是普通标识符") {
        CHECK(lex_dump(U"If") == "IDENTIFIER(If)");
        CHECK(lex_dump(U"IF") == "IDENTIFIER(IF)");
        CHECK(lex_dump(U"NONE") == "IDENTIFIER(NONE)");
        CHECK(lex_dump(U"While") == "IDENTIFIER(While)");
        CHECK(lex_dump(U"Class") == "IDENTIFIER(Class)");
    }

    TEST_CASE("最长匹配：关键字前缀/超集不会被误认成关键字（标识符要整个扫完才判断）") {
        CHECK(lex_dump(U"forx") == "IDENTIFIER(forx)");
        CHECK(lex_dump(U"format") == "IDENTIFIER(format)");
        CHECK(lex_dump(U"iffy") == "IDENTIFIER(iffy)");
        CHECK(lex_dump(U"classroom") == "IDENTIFIER(classroom)");
        CHECK(lex_dump(U"notify") == "IDENTIFIER(notify)");
        CHECK(lex_dump(U"whiley") == "IDENTIFIER(whiley)");
        CHECK(lex_dump(U"_G2") == "IDENTIFIER(_G2)");
        CHECK(lex_dump(U"_Lx") == "IDENTIFIER(_Lx)");
        CHECK(lex_dump(U"__G") == "IDENTIFIER(__G)");
    }

    TEST_CASE("关键字后紧跟其他 token 不需要空白分隔（关键字本身不会贪婪多吃）") {
        CHECK(lex_dump(U"if(x)") == "KW_IF SIGN_LPAREN IDENTIFIER(x) SIGN_RPAREN");
        CHECK(lex_dump(U"not True") == "KW_NOT LITERAL_TRUE");
    }
}
