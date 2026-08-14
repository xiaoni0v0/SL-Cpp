// StaticEvaler/ExprFolder：数值算术折叠（+ - * / // % **）。
// 容器（str/tuple/list）的 +/*、dict 的 |、str 的 % 格式化见同目录 container_ops_test.cpp。
//
// int 运算一律用 int64_t 计算（不再用任意精度的 BigInt）：任何一步——包括操作数本身解析成
// int64_t、以及运算过程中——只要超出 int64_t 能表示的范围，就不折，原样留给以后的执行器用真正的
// 任意精度整数处理。下面专门有一组测试卡在 int64_t 的边界上，folds-exactly-at-boundary /
// doesn't-fold-just-past-it 各一个，防止回归。
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("StaticEvaler 数值算术") {

    TEST_CASE("+ - * 纯 int") {
        CHECK(fold_json(U"1 + 2") == int_lit("3"));
        CHECK(fold_json(U"2 - 3") == int_lit("-1"));
        CHECK(fold_json(U"2 * 3") == int_lit("6"));
    }

    // bool 不继承 int（SL.md 4.2.5），这里能提升是因为 bool 自己实现了 numbers.Real 要求的四则
    // 运算、大小比较，参与运算前把自己折算成 int。别把这条推广到 int 特有的运算：位运算、容器重复
    // 次数都不接受 bool（见 bitwise_test.cpp、container_ops_test.cpp）。
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

    TEST_CASE("// 和 % 都是 int 时恒产出 int，向负无穷取整（SL.md 原例）") {
        CHECK(fold_json(U"-7 // 2") == int_lit("-4"));
        CHECK(fold_json(U"-7 % 2") == int_lit("1"));
        CHECK(fold_json(U"7 // -2") == int_lit("-4"));
    }

    TEST_CASE("// 和 % 满足恒等式 x % y == x - (x // y) * y（正负操作数各种组合）") {
        CHECK(fold_json(U"(-7) % 2") == int_lit("1"));
        CHECK(fold_json(U"-7 - -7 // 2 * 2") == int_lit("1"));
        CHECK(fold_json(U"7 // 2") == int_lit("3"));
        CHECK(fold_json(U"7 % 2") == int_lit("1"));
        CHECK(fold_json(U"-7 // -2") == int_lit("3"));
        CHECK(fold_json(U"-7 % -2") == int_lit("-1"));
    }

    TEST_CASE("掺了 float 的 // 和 %，按浮点向负无穷取整") {
        CHECK(fold_json(U"7.5 // 2") == float_lit("3.0"));
        CHECK(fold_json(U"-7.5 % 2") == float_lit("0.5"));
    }

    TEST_CASE("除以 0 一律不折，交给运行时报 MathError") {
        CHECK(
            fold_json(U"1 / 0") ==
            nlohmann::json{
                {"type", "OpBinary"}, {"op", "/"}, {"left", int_lit("1")}, {"right", int_lit("0")}
            }
        );
        CHECK(
            fold_json(U"1 // 0") ==
            nlohmann::json{
                {"type", "OpBinary"}, {"op", "//"}, {"left", int_lit("1")}, {"right", int_lit("0")}
            }
        );
        CHECK(
            fold_json(U"1 % 0") ==
            nlohmann::json{
                {"type", "OpBinary"}, {"op", "%"}, {"left", int_lit("1")}, {"right", int_lit("0")}
            }
        );
        CHECK(
            fold_json(U"1.0 / 0.0") == nlohmann::json{
                                           {"type", "OpBinary"},
                                           {"op", "/"},
                                           {"left", float_lit("1.0")},
                                           {"right", float_lit("0.0")}
                                       }
        );
    }

    TEST_CASE("** 都是 int 且指数非负，结果是 int") {
        CHECK(fold_json(U"2 ** 10") == int_lit("1024"));
        CHECK(fold_json(U"0 ** 0") == int_lit("1"));
        CHECK(fold_json(U"5 ** 0") == int_lit("1"));
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
            fold_json(U"x + 1") == nlohmann::json{
                                       {"type", "OpBinary"},
                                       {"op", "+"},
                                       {"left", {{"type", "Identifier"}, {"identifier", "x"}}},
                                       {"right", int_lit("1")}
                                   }
        );
        CHECK(
            fold_json(U"1 + 2 + x") == nlohmann::json{
                                           {"type", "OpBinary"},
                                           {"op", "+"},
                                           {"left", int_lit("3")},
                                           {"right", {{"type", "Identifier"}, {"identifier", "x"}}}
                                       }
        );
    }
}

// int64_t 是 [-9223372036854775808, 9223372036854775807]；下面这些边界值都是手算出来的，
// 恰好卡在能不能折的两侧各一个，覆盖 Add/Sub/Mul/Pow/一元 - 五处溢出检测。
TEST_SUITE("StaticEvaler 数值算术——int64_t 边界") {

    TEST_CASE("+ 恰好落在 INT64_MAX 折，超一点不折") {
        CHECK(fold_json(U"9223372036854775806 + 1") == int_lit("9223372036854775807"));
        CHECK(
            fold_json(U"9223372036854775807 + 1") == nlohmann::json{
                                                         {"type", "OpBinary"},
                                                         {"op", "+"},
                                                         {"left", int_lit("9223372036854775807")},
                                                         {"right", int_lit("1")}
                                                     }
        );
    }

    TEST_CASE("- 恰好落在 INT64_MIN 折，超一点不折") {
        // -9223372036854775807 - 1 == -9223372036854775808 == INT64_MIN，这是 int64_t
        // 能表示的最小值，恰好在边界上；注意 INT64_MIN 本身没法直接从字面量文本解析出来（正数
        // 部分 9223372036854775808 已经超出 int64_t 正数范围），只能像这样通过运算恰好落到这个值
        CHECK(fold_json(U"-9223372036854775807 - 1") == int_lit("-9223372036854775808"));
        CHECK(
            fold_json(U"-9223372036854775807 - 2") == nlohmann::json{
                                                          {"type", "OpBinary"},
                                                          {"op", "-"},
                                                          {"left", int_lit("-9223372036854775807")},
                                                          {"right", int_lit("2")}
                                                      }
        );
    }

    TEST_CASE("* 恰好落在 INT64_MAX 附近，折/不折各一个（3037000499 是 floor(sqrt(INT64_MAX))）") {
        CHECK(fold_json(U"3037000499 * 3037000499") == int_lit("9223372030926249001"));
        CHECK(
            fold_json(U"3037000500 * 3037000500") == nlohmann::json{
                                                         {"type", "OpBinary"},
                                                         {"op", "*"},
                                                         {"left", int_lit("3037000500")},
                                                         {"right", int_lit("3037000500")}
                                                     }
        );
    }

    TEST_CASE("** 恰好落在 2^62 折，2^63 不折") {
        CHECK(fold_json(U"2 ** 62") == int_lit("4611686018427387904"));
        CHECK(
            fold_json(U"2 ** 63") ==
            nlohmann::json{
                {"type", "OpBinary"}, {"op", "**"}, {"left", int_lit("2")}, {"right", int_lit("63")}
            }
        );
    }

    TEST_CASE("** 指数非负但结果溢出：不折，不能退化成 float（SL.md 规定这种情况结果必须是 int）") {
        CHECK(
            fold_json(U"10 ** 100") == nlohmann::json{
                                           {"type", "OpBinary"},
                                           {"op", "**"},
                                           {"left", int_lit("10")},
                                           {"right", int_lit("100")}
                                       }
        );
    }

    TEST_CASE("一元 - 对 INT64_MAX 取反没问题（结果不是 INT64_MIN，不会溢出）") {
        CHECK(fold_json(U"-9223372036854775807") == int_lit("-9223372036854775807"));
        CHECK(fold_json(U"- -9223372036854775807") == int_lit("9223372036854775807"));
    }

    TEST_CASE("操作数本身（字面量文本）就超出 int64_t 范围，不折，交给运行时的任意精度整数处理") {
        CHECK(
            fold_json(U"99999999999999999999999999 + 1") ==
            nlohmann::json{
                {"type", "OpBinary"},
                {"op", "+"},
                {"left", int_lit("99999999999999999999999999")},
                {"right", int_lit("1")}
            }
        );
        CHECK(
            fold_json(U"2 ** 99999999999999999999999999") ==
            nlohmann::json{
                {"type", "OpBinary"},
                {"op", "**"},
                {"left", int_lit("2")},
                {"right", int_lit("99999999999999999999999999")}
            }
        );
    }

    TEST_CASE(
        "// 和 % 的除数是 -1、被除数是接近 INT64_MIN 的值，不会触发溢出（只有恰好等于\n"
        "INT64_MIN 才会，而这个值造不出字面量，见上面 - 那组测试的注释）"
    ) {
        CHECK(fold_json(U"-9223372036854775807 // -1") == int_lit("9223372036854775807"));
        CHECK(fold_json(U"-9223372036854775807 % -1") == int_lit("0"));
    }
}
