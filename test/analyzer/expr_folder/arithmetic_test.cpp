// StaticEvaler/ExprFolder：数值算术折叠（+ - * / // % **）。
// 容器（str/tuple/list）的 +/*、dict 的 |、str 的 % 格式化见同目录 container_ops_test.cpp。
//
// int 运算走 numeric/ 的 BigInt，跟运行期是同一套任意精度算术，所以没有"算得出但表示不下"的
// 情形——不再有 int64_t 那道溢出边界。唯一的边界是规模上限 nMaxIntDigits（结果的十进制位数），
// 它挡的是"算得完但没必要"（2 ** 大数会一路算到跑不完），不是"算不对"。
//
// 结果为 decimal 的运算一律**不折**：decimal 按运行期上下文（prec/rounding）舍入，编译期不知道
// 那时的设置，折了就可能和实际执行不一致。这条覆盖 `/`（结果恒为 decimal）、任何一侧是 decimal
// 的四则、`**` 指数为负、以及 decimal 的一元 +/-。见 StaticEvaler.h 类注释和
// .ai/context.md "结果为 decimal 的常量折叠一律禁掉" 一节。
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("StaticEvaler 数值算术") {

    TEST_CASE("+ - * 纯 int") {
        CHECK(fold_json(U"1 + 2") == int_lit("3"));
        CHECK(fold_json(U"2 - 3") == int_lit("-1"));
        CHECK(fold_json(U"2 * 3") == int_lit("6"));
    }

    // bool 不继承 int（见 SL.md 的 bool 内置类一节），这里能提升是因为 bool 自己实现了
    // numbers.Real 要求的四则
    // 运算、大小比较，参与运算前把自己折算成 int。别把这条推广到 int 特有的运算：位运算、容器重复
    // 次数都不接受 bool（见 bitwise_test.cpp、container_ops_test.cpp）。
    TEST_CASE("bool 参与数值运算按 int 提升，结果类型是 int 不是 bool") {
        CHECK(fold_json(U"True + 1") == int_lit("2"));
        CHECK(fold_json(U"True + True") == int_lit("2"));
        CHECK(fold_json(U"+True") == int_lit("1"));
        CHECK(fold_json(U"-True") == int_lit("-1"));
    }

    // `/` 的结果恒为 decimal（SL.md 3.4.2），所以哪怕两边都是 int、哪怕除得尽，也一律不折
    TEST_CASE("/ 恒产出 decimal，因此恒不折（即使两边都是 int、即使除得尽）") {
        CHECK(
            fold_json(U"7 / 2") ==
            nlohmann::json{
                {"type", "OpBinary"}, {"op", "/"}, {"left", int_lit("7")}, {"right", int_lit("2")}
            }
        );
        CHECK(
            fold_json(U"6 / 2") ==
            nlohmann::json{
                {"type", "OpBinary"}, {"op", "/"}, {"left", int_lit("6")}, {"right", int_lit("2")}
            }
        );
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

    TEST_CASE("掺了 decimal 的 // 和 % 结果是 decimal，不折") {
        CHECK(
            fold_json(U"7.5 // 2") == nlohmann::json{
                                          {"type", "OpBinary"},
                                          {"op", "//"},
                                          {"left", decimal_lit("7.5")},
                                          {"right", int_lit("2")}
                                      }
        );
        CHECK(
            fold_json(U"-7.5 % 2") ==
            nlohmann::json{
                {"type", "OpBinary"},
                {"op", "%"},
                {"left", {{"type", "OpUnary"}, {"op", "-"}, {"operand", decimal_lit("7.5")}}},
                {"right", int_lit("2")}
            }
        );
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
                                           {"left", decimal_lit("1.0")},
                                           {"right", decimal_lit("0.0")}
                                       }
        );
    }

    TEST_CASE("** 都是 int 且指数非负，结果是 int") {
        CHECK(fold_json(U"2 ** 10") == int_lit("1024"));
        CHECK(fold_json(U"0 ** 0") == int_lit("1"));
        CHECK(fold_json(U"5 ** 0") == int_lit("1"));
    }

    TEST_CASE("** 指数为负，结果是 decimal，不折") {
        CHECK(
            // 注意右边是 int_lit("-1") 而不是 OpUnary：一元负号作用在 int 上是照折的，
            // 折完之后外层 ** 才发现指数为负、结果会是 decimal，于是停在这一步
            fold_json(U"2 ** -1") ==
            nlohmann::json{
                {"type", "OpBinary"}, {"op", "**"}, {"left", int_lit("2")}, {"right", int_lit("-1")}
            }
        );
    }

    TEST_CASE("+x/-x/~x 对字面量取值，~ 只对 bool/int 有意义") {
        CHECK(fold_json(U"-5") == int_lit("-5"));
        CHECK(fold_json(U"- -5") == int_lit("5"));
        CHECK(fold_json(U"~5") == int_lit("-6"));
        CHECK(fold_json(U"~0") == int_lit("-1"));
    }

    // decimal 的一元 +/- 也是算术运算，同样按上下文舍入（prec 小的时候 -1.234 会舍成 -1.2），
    // 不是恒等操作，所以跟 decimal 的二元算术一样不折
    TEST_CASE("+x/-x 作用在 decimal 上不折") {
        CHECK(
            fold_json(U"-1.5") ==
            nlohmann::json{{"type", "OpUnary"}, {"op", "-"}, {"operand", decimal_lit("1.5")}}
        );
        CHECK(
            fold_json(U"+1.5") ==
            nlohmann::json{{"type", "OpUnary"}, {"op", "+"}, {"operand", decimal_lit("1.5")}}
        );
    }

    TEST_CASE("~ 对 decimal 不折，交给运行时报错") {
        CHECK(
            fold_json(U"~1.5") ==
            nlohmann::json{{"type", "OpUnary"}, {"op", "~"}, {"operand", decimal_lit("1.5")}}
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

// int 折叠改走 BigInt 之后，"溢出"这个概念就没有了：以前卡在 int64_t 两侧的那批用例（+/-/*/**
// 恰好越界就不折）全部作废，因为它们现在都该正常折出精确结果。这一组换成钉住新的两件事：
//   1. 任意精度确实生效——以前折不动的大数现在折得出，且结果精确；
//   2. 规模上限 nMaxIntDigits（4096 位十进制）仍然拦得住会爆炸的 * ** <<。
TEST_SUITE("StaticEvaler 数值算术——任意精度与规模上限") {

    TEST_CASE("以前卡在 int64_t 边界上不折的，现在都精确折出来") {
        CHECK(fold_json(U"9223372036854775807 + 1") == int_lit("9223372036854775808"));
        CHECK(fold_json(U"-9223372036854775807 - 2") == int_lit("-9223372036854775809"));
        CHECK(fold_json(U"3037000500 * 3037000500") == int_lit("9223372037000250000"));
        CHECK(fold_json(U"2 ** 63") == int_lit("9223372036854775808"));
    }

    TEST_CASE("操作数本身远超 int64_t 也照折，结果精确") {
        CHECK(
            fold_json(U"99999999999999999999999999 + 1") == int_lit("100000000000000000000000000")
        );
        CHECK(
            fold_json(U"99999999999999999999999999 * 2") == int_lit("199999999999999999999999998")
        );
        // 10 ** 100 是个 101 位的整数，写全了钉住，确认不是近似值
        CHECK(
            fold_json(U"10 ** 100") ==
            int_lit(
                "1000000000000000000000000000000000000000000000000000000000000000000000000000"
                "0000000000000000000000000"
            )
        );
    }

    TEST_CASE("科学计数法写法的 int 按值参与运算，不按字面文本") {
        CHECK(fold_json(U"1e2 + 1") == int_lit("101"));
        CHECK(fold_json(U"1e9 * 1e9") == int_lit("1000000000000000000"));
        CHECK(fold_json(U"0e0 + 5") == int_lit("5"));
    }

    // 上限卡的是"算得完但没必要"，不是"算不对"。* ** << 三处会让规模爆炸，各测一组折/不折。
    //
    // ** 事先估算用的是「底数位数 × 指数」这个**上界**（只有整数运算，不引入浮点去算对数）。
    // 它偏保守：底数 10 的位数是 2，于是估出来是真实位数的约两倍，实际折/不折的分界因此落在
    // 指数 2048 而不是 4095。偏保守只意味着少折一些，不会折错，所以按实际分界钉住即可。
    TEST_CASE("** 的结果规模估算到上限就不折（估算偏保守，分界在指数 2048）") {
        CHECK(fold_json(U"10 ** 2048")["type"] == "LiteralInt");
        CHECK(
            fold_json(U"10 ** 2049") == nlohmann::json{
                                            {"type", "OpBinary"},
                                            {"op", "**"},
                                            {"left", int_lit("10")},
                                            {"right", int_lit("2049")}
                                        }
        );
    }

    TEST_CASE("指数大到装不下 int64_t 时直接不折，不会去真算") {
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

    TEST_CASE("* 的结果位数超上限不折") {
        // 两个 2049 位的数相乘，结果 4097 或 4098 位，必然超上限
        const std::u32string big{U"1" + std::u32string(2048, U'0')}; // 2049 位
        CHECK(fold_json(big + U" * " + big)["type"] == "OpBinary");
        // 对照：两个 2048 位的数相乘，结果至多 4096 位，折得出来
        const std::u32string ok{U"1" + std::u32string(2047, U'0')}; // 2048 位
        CHECK(fold_json(ok + U" * " + ok)["type"] == "LiteralInt");
    }

    TEST_CASE("一元 - 对任意大的 int 都能折") {
        CHECK(fold_json(U"-9223372036854775808") == int_lit("-9223372036854775808"));
        CHECK(fold_json(U"- -99999999999999999999999999") == int_lit("99999999999999999999999999"));
    }

    TEST_CASE("// 和 % 不再有 INT64_MIN / -1 那个溢出特例，照常折") {
        CHECK(fold_json(U"-9223372036854775808 // -1") == int_lit("9223372036854775808"));
        CHECK(fold_json(U"-9223372036854775808 % -1") == int_lit("0"));
    }
}
