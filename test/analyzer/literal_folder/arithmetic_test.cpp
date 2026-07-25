// StaticEvaler/LiteralFolder：数值算术折叠（+ - * / // % **，SL.md 3.4.2）。
// 容器（str/tuple/list）的 +/*、dict 的 |、str 的 % 格式化见同目录 container_ops_test.cpp。
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("StaticEvaler 数值算术") {

    TEST_CASE("+ - * 纯 int") {
        CHECK(fold_json(U"1 + 2") == int_lit("3"));
        CHECK(fold_json(U"2 - 3") == int_lit("-1"));
        CHECK(fold_json(U"2 * 3") == int_lit("6"));
    }

    TEST_CASE("bool 参与数值运算按 int 提升，结果类型是 int 不是 bool") {
        CHECK(fold_json(U"True + 1") == int_lit("2"));
        CHECK(fold_json(U"True + True") == int_lit("2"));
        CHECK(fold_json(U"+True") == int_lit("1"));
        CHECK(fold_json(U"-True") == int_lit("-1"));
    }

    TEST_CASE("/ 恒产出 float，即使两边都是 int") {
        CHECK(fold_json(U"7 / 2") == float_lit("3.5"));
        CHECK(fold_json(U"6 / 2") == float_lit("3.0"));
    }

    TEST_CASE("// 和 % 都是 int 时恒产出 int，向负无穷取整（SL.md 3.4.2 原例）") {
        CHECK(fold_json(U"-7 // 2") == int_lit("-4"));
        CHECK(fold_json(U"-7 % 2") == int_lit("1"));
        CHECK(fold_json(U"7 // -2") == int_lit("-4"));
    }

    TEST_CASE("// 和 % 满足恒等式 x % y == x - (x // y) * y") {
        CHECK(fold_json(U"(-7) % 2") == int_lit("1"));
        CHECK(fold_json(U"-7 - -7 // 2 * 2") == int_lit("1"));
    }

    TEST_CASE("掺了 float 的 // 和 %，按浮点向负无穷取整") {
        CHECK(fold_json(U"7.5 // 2") == float_lit("3.0"));
        CHECK(fold_json(U"-7.5 % 2") == float_lit("0.5"));
    }

    TEST_CASE("除以 0 一律不折，交给运行时报 MathError") {
        CHECK(
            fold_json(U"1 / 0") ==
            nlohmann::json{
                {"type", "OpBinary"}, {"op", "/"}, {"left", int_lit("1")}, {"right", int_lit("0")}}
        );
        CHECK(
            fold_json(U"1 // 0") ==
            nlohmann::json{
                {"type", "OpBinary"}, {"op", "//"}, {"left", int_lit("1")}, {"right", int_lit("0")}}
        );
        CHECK(
            fold_json(U"1 % 0") ==
            nlohmann::json{
                {"type", "OpBinary"}, {"op", "%"}, {"left", int_lit("1")}, {"right", int_lit("0")}}
        );
        CHECK(
            fold_json(U"1.0 / 0.0") == nlohmann::json{{"type", "OpBinary"},
                                                      {"op", "/"},
                                                      {"left", float_lit("1.0")},
                                                      {"right", float_lit("0.0")}}
        );
    }

    TEST_CASE("** 都是 int 且指数非负，结果仍是 int（可以很大）") {
        CHECK(fold_json(U"2 ** 10") == int_lit("1024"));
        CHECK(fold_json(U"2 ** 100") == int_lit("1267650600228229401496703205376"));
    }

    TEST_CASE("** 指数为负，结果是 float") { CHECK(fold_json(U"2 ** -1") == float_lit("0.5")); }

    TEST_CASE("+x/-x/~x 对字面量取值，~ 只对 bool/int 有意义") {
        CHECK(fold_json(U"-5") == int_lit("-5"));
        CHECK(fold_json(U"- -5") == int_lit("5"));
        CHECK(fold_json(U"~5") == int_lit("-6"));
        CHECK(fold_json(U"~0") == int_lit("-1"));
        CHECK(fold_json(U"-1.5") == float_lit("-1.5"));
    }

    TEST_CASE("~ 对 float 不折，交给运行时报错") {
        CHECK(
            fold_json(U"~1.5") ==
            nlohmann::json{{"type", "OpUnary"}, {"op", "~"}, {"operand", float_lit("1.5")}}
        );
    }

    TEST_CASE("含变量/调用的子表达式不折，只递归折内部能折的部分") {
        CHECK(
            fold_json(U"x + 1") ==
            nlohmann::json{{"type", "OpBinary"},
                           {"op", "+"},
                           {"left", {{"type", "Identifier"}, {"identifier", "x"}}},
                           {"right", int_lit("1")}}
        );
        CHECK(
            fold_json(U"1 + 2 + x") ==
            nlohmann::json{{"type", "OpBinary"},
                           {"op", "+"},
                           {"left", int_lit("3")},
                           {"right", {{"type", "Identifier"}, {"identifier", "x"}}}}
        );
    }
}
