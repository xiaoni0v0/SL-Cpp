// Parser 抛出的 SyntaxError：继承关系、消息里的文件名和行列。
#include "../../../cpp_exceptions/SLException.h"
#include "../../../cpp_exceptions/SyntaxError.h"
#include "../test_utils.h"

#include <doctest/doctest.h>
#include <stdexcept>

TEST_SUITE("Parser 的 SyntaxError 行为") {

    TEST_CASE("SyntaxError 是 SLException 的子类，也是 std::exception 的子类（可以被泛化捕获）") {
        CHECK_THROWS_AS(parse_as_file(U"("), SLException);
        CHECK_THROWS_AS(parse_as_file(U"("), std::exception);
    }

    TEST_CASE("异常消息里带有文件名、行号、列号") {
        try {
            parse_as_file(U"1\nfor ()", "my_file.sl");
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("my_file.sl") != std::string::npos);
            CHECK(msg.find("2:") != std::string::npos); // 报错发生在第 2 行
            CHECK(msg.find("SyntaxError") != std::string::npos);
        }
    }

    TEST_CASE("报错行列指向出问题的具体位置，不是文件开头") {
        try {
            // "x = 1\ny = " -> 第二行赋值缺右值，报错应该指向第 2 行
            parse_as_file(U"x = 1\ny = ");
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("2:") != std::string::npos);
        }
    }

    TEST_CASE(
        "expect() 报错信息用用户可读的 token 名字，不泄漏内部枚举名，位置指向实际出现的那个 token"
    ) {
        // "if a) b" -> i(1)f(2) (3)a(4)：expect(LPAREN) 时 peek() 停在 'a'
        try {
            parse_as_file(U"if a) b"); // 缺左括号：期望 '(' 实际是标识符 a
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("'('") != std::string::npos);
            CHECK(msg.find("an identifier") != std::string::npos);
            CHECK(msg.find("SIGN_LPAREN") == std::string::npos);
            CHECK(msg.find("IDENTIFIER") == std::string::npos);
            CHECK(msg.find("1:4:") != std::string::npos); // 'a' 的位置
        }
    }

    TEST_CASE("遇到 EOF 时区分'括号未闭合'和'单纯缺表达式'两种措辞，不能笼统一句带过") {
        // "(" -> 未闭合括号内缺表达式，brackets_ 非空，提示是括号没收尾
        try {
            parse_as_file(U"(");
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("unclosed bracket") != std::string::npos);
            CHECK(msg.find("1:2:") != std::string::npos); // EOF 紧跟在 '(' 之后
        }
        // "1 +" -> 二元运算符消耗完还等着右操作数，brackets_ 为空，提示是缺了表达式本身
        // （注：完全空的输入 "" 本身语法上合法——parse_exprs 的循环条件一见 EOF 就直接不进入循环体，
        // 根本不会走到 parse_non_op，产出的是空的顶层表达式列表，不是错误，断言见下面单独的用例）
        try {
            parse_as_file(U"1 +");
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("expected an expression") != std::string::npos);
            CHECK(msg.find("unclosed bracket") == std::string::npos);
            CHECK(msg.find("1:4:") != std::string::npos); // EOF 紧跟在 "1 +" 之后
        }
    }

    TEST_CASE("空输入（或只有注释/空白）语法上合法，产出空 Program，不是错误") {
        CHECK(
            parse_program_json(U"") ==
            nlohmann::json{{"type", "Program"}, {"exprs", nlohmann::json::array()}}
        );
        CHECK(parse_program_json(U"   \n\n  ") == parse_program_json(U""));
        CHECK(parse_program_json(U"# 整段都是注释\n# 还是注释") == parse_program_json(U""));
    }

    TEST_CASE(
        "遇到不能作为表达式开头的 token（如裸逗号）报错，消息里带上该 token "
        "的原始文本，位置指向它自己"
    ) {
        // "," 本身不能开始一条表达式（不是前缀运算符也不是字面量）
        try {
            parse_as_file(U",");
            FAIL("应当抛出异常");
        } catch (const SyntaxError &e) {
            const std::string msg{e.what()};
            CHECK(msg.find("unexpected token ','") != std::string::npos);
            CHECK(msg.find("1:1:") != std::string::npos);
        }
    }

    TEST_CASE("汇总：各类会触发 SyntaxError 的场景（分散测试过的，这里过一遍总览，确认互不干扰）") {
        CHECK_THROWS_AS(parse_as_file(U"1."), SyntaxError);           // 属性访问缺属性名
        CHECK_THROWS_AS(parse_as_file(U"(1, 2"), SyntaxError);        // 未闭合的元组
        CHECK_THROWS_AS(parse_as_file(U"[1, 2"), SyntaxError);        // 未闭合的列表
        CHECK_THROWS_AS(parse_as_file(U"{k: v"), SyntaxError);        // 未闭合的字典
        CHECK_THROWS_AS(parse_as_file(U"{k: v, x}"), SyntaxError);    // 字典展开项判定错误
        CHECK_THROWS_AS(parse_as_file(U"if (x = 1) y"), SyntaxError); // cond 槽裸赋值
        CHECK_THROWS_AS(
            parse_as_file(U"for (a; b) body"), SyntaxError
        );                                                          // for 头部槽数只能是 3 或 1
        CHECK_THROWS_AS(parse_as_file(U"global 5"), SyntaxError);   // global 后面不是标识符
        CHECK_THROWS_AS(parse_as_file(U"raise"), SyntaxError);      // raise 缺表达式
        CHECK_THROWS_AS(parse_as_file(U"a b"), SyntaxError);        // 缺表达式分隔符
        CHECK_THROWS_AS(parse_as_file(U"x\n.func()"), SyntaxError); // 裸 '.' 开头
    }

    TEST_CASE("正常输入不应该抛任何异常（反面对照）") {
        CHECK_NOTHROW(parse_as_file(U"func f(x: int = 1) { return x + 1 }"));
        CHECK_NOTHROW(parse_as_file(U"class C(Base) { x = `raw\nstring` }"));
        CHECK_NOTHROW(parse_as_file(U"for (i = 0; i < 10; i += 1) { if (i == 5) break }"));
        CHECK_NOTHROW(parse_as_file(U"@dec1 @dec2 func f[x, &y]() : int { return x }"));
    }
}
