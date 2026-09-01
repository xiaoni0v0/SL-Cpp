// 关键字：完整词才认，大小写敏感；前缀/超集是标识符。
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("关键字") {

    TEST_CASE("字面量关键字") {
        CHECK(lex_dump(U"None") == "LITERAL_NONE");
        CHECK(lex_dump(U"True") == "LITERAL_TRUE");
        CHECK(lex_dump(U"False") == "LITERAL_FALSE");
        CHECK(lex_dump(U"_G") == "LITERAL_G");
        CHECK(lex_dump(U"_L") == "LITERAL_L");
    }

    TEST_CASE("其余关键字") {
        CHECK(lex_dump(U"not") == "KW_NOT");
        CHECK(lex_dump(U"and") == "KW_AND");
        CHECK(lex_dump(U"or") == "KW_OR");
        CHECK(lex_dump(U"is") == "KW_IS");
        CHECK(lex_dump(U"in") == "KW_IN");
        CHECK(lex_dump(U"as") == "KW_AS");
        CHECK(lex_dump(U"del") == "KW_DEL");
        CHECK(lex_dump(U"global") == "KW_GLOBAL");
        CHECK(lex_dump(U"if") == "KW_IF");
        CHECK(lex_dump(U"elif") == "KW_ELIF");
        CHECK(lex_dump(U"else") == "KW_ELSE");
        CHECK(lex_dump(U"for") == "KW_FOR");
        CHECK(lex_dump(U"while") == "KW_WHILE");
        CHECK(lex_dump(U"break") == "KW_BREAK");
        CHECK(lex_dump(U"continue") == "KW_CONTINUE");
        CHECK(lex_dump(U"func") == "KW_FUNC");
        CHECK(lex_dump(U"return") == "KW_RETURN");
        CHECK(lex_dump(U"raise") == "KW_RAISE");
        CHECK(lex_dump(U"try") == "KW_TRY");
        CHECK(lex_dump(U"except") == "KW_EXCEPT");
        CHECK(lex_dump(U"finally") == "KW_FINALLY");
        CHECK(lex_dump(U"class") == "KW_CLASS");
        CHECK(lex_dump(U"import") == "KW_IMPORT");
        CHECK(lex_dump(U"eval") == "KW_EVAL");
    }

    TEST_CASE("大小写变体是标识符") {
        CHECK(lex_dump(U"If") == "IDENTIFIER(If)");
        CHECK(lex_dump(U"IF") == "IDENTIFIER(IF)");
        CHECK(lex_dump(U"NONE") == "IDENTIFIER(NONE)");
        CHECK(lex_dump(U"While") == "IDENTIFIER(While)");
        CHECK(lex_dump(U"Class") == "IDENTIFIER(Class)");
        CHECK(lex_dump(U"Import") == "IDENTIFIER(Import)");
        CHECK(lex_dump(U"true") == "IDENTIFIER(true)");
        CHECK(lex_dump(U"false") == "IDENTIFIER(false)");
        CHECK(lex_dump(U"none") == "IDENTIFIER(none)");
        CHECK(lex_dump(U"_g") == "IDENTIFIER(_g)");
    }

    TEST_CASE("前缀/超集是标识符") {
        CHECK(lex_dump(U"forx") == "IDENTIFIER(forx)");
        CHECK(lex_dump(U"format") == "IDENTIFIER(format)");
        CHECK(lex_dump(U"iffy") == "IDENTIFIER(iffy)");
        CHECK(lex_dump(U"classroom") == "IDENTIFIER(classroom)");
        CHECK(lex_dump(U"notify") == "IDENTIFIER(notify)");
        CHECK(lex_dump(U"whiley") == "IDENTIFIER(whiley)");
        CHECK(lex_dump(U"important") == "IDENTIFIER(important)");
        CHECK(lex_dump(U"importer") == "IDENTIFIER(importer)");
        CHECK(lex_dump(U"asx") == "IDENTIFIER(asx)");
        CHECK(lex_dump(U"evaluate") == "IDENTIFIER(evaluate)");
        CHECK(lex_dump(U"notin") == "IDENTIFIER(notin)");
        CHECK(lex_dump(U"isnot") == "IDENTIFIER(isnot)");
        CHECK(lex_dump(U"Trueor") == "IDENTIFIER(Trueor)");
        CHECK(lex_dump(U"_G2") == "IDENTIFIER(_G2)");
        CHECK(lex_dump(U"_Lx") == "IDENTIFIER(_Lx)");
        CHECK(lex_dump(U"__G") == "IDENTIFIER(__G)");
    }

    TEST_CASE("关键字与符号之间不必有空白") {
        CHECK(lex_dump(U"if(x)") == "KW_IF SIGN_LPAREN IDENTIFIER(x) SIGN_RPAREN");
        CHECK(lex_dump(U"not True") == "KW_NOT LITERAL_TRUE");
    }
}
