// StaticEvaler/ExprFolder：位运算折叠（~ & ^ | << >>，只对 int 有意义，bool 也不行）。
// dict 的 | 合并见 container_ops_test.cpp（跟 int 的 | 是同一个运算符，按左操作数类型分派）。
//
// int 运算一律用 int64_t 计算（不再用任意精度的 BigInt）。& ^ | 两个定宽整数直接算，恒不溢出；
// >> 只会让值更收敛，任意非负的移位量都有确定结果（移位量很大时饱和到 0 或 -1），不设上限；
// << 会让值变大，移位量必须落在 [0, 62]，还要另外检查结果没有溢出 int64_t。
#include "../test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE("StaticEvaler 位运算") {

    TEST_CASE("& ^ | 基本情况") {
        CHECK(fold_json(U"6 & 3") == int_lit("2"));
        CHECK(fold_json(U"6 ^ 3") == int_lit("5"));
        CHECK(fold_json(U"6 | 3") == int_lit("7"));
    }

    TEST_CASE("按无穷位补码语义（SL.md 原例，本项目 int 字面量目前只支持十进制，255 即 0xff）") {
        CHECK(fold_json(U"-1 & 255") == int_lit("255"));
        CHECK(fold_json(U"~5") == int_lit("-6"));
    }

    TEST_CASE(">> 移位量任意大都有确定结果（饱和到 0 或 -1），不受任何上限约束") {
        CHECK(fold_json(U"-1 >> 100") == int_lit("-1"));
        CHECK(fold_json(U"5 >> 100") == int_lit("0"));
        CHECK(fold_json(U"-1 >> 99999999999999") == int_lit("-1"));
        CHECK(fold_json(U"-1 >> 63") == int_lit("-1"));
        CHECK(fold_json(U"5 >> 63") == int_lit("0"));
    }

    TEST_CASE("<< >> 基本情况") {
        CHECK(fold_json(U"1 << 4") == int_lit("16"));
        CHECK(fold_json(U"16 >> 4") == int_lit("1"));
        CHECK(fold_json(U"-16 >> 4") == int_lit("-1"));
    }

    TEST_CASE("负数移位不折，交给运行时报错（<< >> 都一样）") {
        CHECK(
            fold_json(U"1 << -1") ==
            nlohmann::json{
                {"type", "OpBinary"}, {"op", "<<"}, {"left", int_lit("1")}, {"right", int_lit("-1")}
            }
        );
        CHECK(
            fold_json(U"1 >> -1") ==
            nlohmann::json{
                {"type", "OpBinary"}, {"op", ">>"}, {"left", int_lit("1")}, {"right", int_lit("-1")}
            }
        );
    }

    // bool 不继承 int（见 SL.md 的 bool 内置类一节），位运算是 int 特有的方法，bool 没有；
    // 这跟四则运算/比较
    // 会把 bool 折算成 int 再算是两回事（见 arithmetic_test.cpp）。折叠器一律不折，留给运行时抛
    // TypeError。
    TEST_CASE("bool 不参与位运算，不折") {
        CHECK(
            fold_json(U"True & 1") ==
            nlohmann::json{
                {"type", "OpBinary"}, {"op", "&"}, {"left", bool_lit(true)}, {"right", int_lit("1")}
            }
        );
        CHECK(
            fold_json(U"True | False") == nlohmann::json{
                                              {"type", "OpBinary"},
                                              {"op", "|"},
                                              {"left", bool_lit(true)},
                                              {"right", bool_lit(false)}
                                          }
        );
        CHECK(
            fold_json(U"1 << True") == nlohmann::json{
                                           {"type", "OpBinary"},
                                           {"op", "<<"},
                                           {"left", int_lit("1")},
                                           {"right", bool_lit(true)}
                                       }
        );
        CHECK(
            fold_json(U"~True") ==
            nlohmann::json{{"type", "OpUnary"}, {"op", "~"}, {"operand", bool_lit(true)}}
        );
    }

    TEST_CASE("decimal 不参与位运算，不折") {
        CHECK(
            fold_json(U"1.0 & 1") == nlohmann::json{
                                         {"type", "OpBinary"},
                                         {"op", "&"},
                                         {"left", decimal_lit("1.0")},
                                         {"right", int_lit("1")}
                                     }
        );
    }
}

// 位运算一律用 int64_t 计算，不追求任意精度。这一组钉住新的边界：
//   1. 操作数本身装不下 int64_t 时，& | ^ << 一律不折，~ 同样不折；
//   2. `<<` 会让值变大，移位量、结果都必须落在 int64_t 范围内才折；
//   3. 移位量本身大到装不下 int64_t 时，`<<` 不折，`>>` 反而能直接给出答案（结果只会是 0 或 -1）。
TEST_SUITE("StaticEvaler 位运算——int64_t 边界") {

    TEST_CASE("<< 结果超出 int64_t 就不折，恰好没超的照折") {
        // 1 << 63 == 9223372036854775808，比 INT64_MAX 大 1
        CHECK(fold_json(U"1 << 63")["type"] == "OpBinary");
        CHECK(fold_json(U"2 << 62")["type"] == "OpBinary");
        // 对照：1 << 62 恰好没超界
        CHECK(fold_json(U"1 << 62") == int_lit("4611686018427387904"));
        // 0 左移多少位都是 0，不受结果范围限制
        CHECK(fold_json(U"0 << 63") == int_lit("0"));
    }

    TEST_CASE("操作数本身装不下 int64_t 时，& | ^ ~ 一律不折") {
        CHECK(fold_json(U"99999999999999999999999999 & 1")["type"] == "OpBinary");
        CHECK(fold_json(U"99999999999999999999999999 | 0")["type"] == "OpBinary");
        CHECK(fold_json(U"99999999999999999999999999 ^ 0")["type"] == "OpBinary");
        CHECK(fold_json(U"~99999999999999999999999999")["type"] == "OpUnary");
    }

    TEST_CASE("<< 移位量本身超出 int64_t 范围，不折（左操作数非零时结果必然也超界）") {
        CHECK(
            fold_json(U"1 << 99999999999999999999999999") ==
            nlohmann::json{
                {"type", "OpBinary"},
                {"op", "<<"},
                {"left", int_lit("1")},
                {"right", int_lit("99999999999999999999999999")}
            }
        );
    }

    // >> 只会让数变小，移位量再大也算得出来：非负数右移到底是 0，负数是 -1（算术右移补符号位）。
    // 左操作数本身必须先装得下 int64_t（不然连"是不是非负"都无从谈起）
    TEST_CASE(">> 移位量大到装不下 int64_t 时直接给出结果，不需要真移") {
        CHECK(fold_json(U"1 >> 99999999999999999999999999") == int_lit("0"));
        CHECK(fold_json(U"-1 >> 99999999999999999999999999") == int_lit("-1"));
        CHECK(fold_json(U"5 >> 99999999999999999999999999") == int_lit("0"));
    }

    TEST_CASE(">> 大移位量但装得下时照常算") {
        CHECK(fold_json(U"1 >> 1000") == int_lit("0"));
        CHECK(fold_json(U"-1 >> 1000") == int_lit("-1"));
    }
}
