// SL.md 2.1.1 注释
#include "../test_utils.h"
#include "../../../builtins/exceptions/SyntaxError.h"

#include <doctest/doctest.h>

TEST_SUITE("2.1.1 注释") {

TEST_CASE("单行注释：整行都被吃掉，直到行尾") {
    CHECK(lex_dump(U"1 # 这是注释 2 3\n4") == "LITERAL_INT(1) NEWLINE LITERAL_INT(4)");
}

TEST_CASE("单行注释：本身不产生任何 token") {
    CHECK(lex_dump(U"# 整行都是注释") == "");
}

TEST_CASE("单行注释：不消耗紧跟着的换行符") {
    // 换行本身仍然要正常参与 tokenize() 主循环的换行抑制逻辑，
    // 这里 1 后面紧跟注释再换行，换行前有实际 token，所以换行不会被吃掉。
    CHECK(lex_dump(U"1 #c\n2") == "LITERAL_INT(1) NEWLINE LITERAL_INT(2)");
}

TEST_CASE("单行注释：文件在注释处直接结束（没有结尾换行）") {
    CHECK(lex_dump(U"1 # comment, no trailing newline") == "LITERAL_INT(1)");
}

TEST_CASE("单行注释：注释里出现井号本身、引号、反引号等特殊字符都不影响") {
    CHECK(lex_dump(U"1 # ## \"x\" 'y' `z` \\n \n2") == "LITERAL_INT(1) NEWLINE LITERAL_INT(2)");
}

TEST_CASE("多行注释：基本用法") {
    CHECK(lex_dump(U"1 /* comment */ 2") == "LITERAL_INT(1) LITERAL_INT(2)");
}

TEST_CASE("多行注释：本身不产生任何 token") {
    CHECK(lex_dump(U"/* just a comment */") == "");
}

TEST_CASE("多行注释：可以跨行，中间的换行不产生 NEWLINE token") {
    CHECK(lex_dump(U"1 /* line1\nline2\nline3 */ 2") == "LITERAL_INT(1) LITERAL_INT(2)");
}

TEST_CASE("多行注释：跨行后行号正确递增（用于报错定位）") {
    // /* 在第 1 行开始，内部有两个换行，注释结束后的 2 应该在第 3 行
    const auto tokens{lex(U"1 /* a\nb\nc */ 2")};
    REQUIRE(tokens.size() == 3); // 1, 2, EOF
    CHECK(tokens[0].row == 1);
    CHECK(tokens[1].row == 3);
}

TEST_CASE("多行注释：不支持嵌套，内层的 */ 直接结束整个注释（spec 2.1.1 明确举的例子）") {
    // /* a /* b */ c */ 中，b 后面的 */ 结束了最外层（唯一层）注释，
    // 剩下的 ` c */` 就是普通代码：标识符 c，然后 * 和 /
    CHECK(lex_dump(U"/* a /* b */ c */") == "IDENTIFIER(c) SIGN_STAR SIGN_SLASH");
}

TEST_CASE("多行注释：未闭合抛出 SyntaxError") {
    CHECK_THROWS_AS(lex(U"1 /* never closed"), SyntaxError);
}

TEST_CASE("多行注释：紧邻 */ 前一个字符是 * 但不是紧跟在 /* 之后也能正常识别") {
    CHECK(lex_dump(U"/** comment **/ 1") == "LITERAL_INT(1)");
}

TEST_CASE("多行注释与除法运算符不混淆：/ 后面不是 * 就是普通除号") {
    CHECK(lex_dump(U"1 / 2") == "LITERAL_INT(1) SIGN_SLASH LITERAL_INT(2)");
}

}
