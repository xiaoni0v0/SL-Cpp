// StaticEvaler/LiteralFolder：位运算折叠（& ^ | << >>，只对 bool/int 有意义，SL.md 3.4.2）。
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

    TEST_CASE(
        "按无穷位补码语义（SL.md 3.4.2 原例，本项目 int 字面量目前只支持十进制，255 即 0xff）"
    ) {
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

    TEST_CASE("bool 参与位运算按 int 提升") {
        CHECK(fold_json(U"True & 1") == int_lit("1"));
        CHECK(fold_json(U"True | False") == int_lit("1"));
    }

    TEST_CASE("float 不参与位运算，不折") {
        CHECK(
            fold_json(U"1.0 & 1") == nlohmann::json{
                                         {"type", "OpBinary"},
                                         {"op", "&"},
                                         {"left", float_lit("1.0")},
                                         {"right", int_lit("1")}
                                     }
        );
    }
}

TEST_SUITE("StaticEvaler 位运算——int64_t 边界") {

    TEST_CASE("<< 移位量恰好落在 [0, 62] 折，63 一律不折（哪怕 value 是 0）") {
        CHECK(fold_json(U"1 << 62") == int_lit("4611686018427387904"));
        CHECK(
            fold_json(U"1 << 63") ==
            nlohmann::json{
                {"type", "OpBinary"}, {"op", "<<"}, {"left", int_lit("1")}, {"right", int_lit("63")}
            }
        );
        // 0 << 63 数学上显然还是 0，但这里为了简单统一，shift 一超过 62 就不折，不单独优化这个特例
        CHECK(
            fold_json(U"0 << 63") ==
            nlohmann::json{
                {"type", "OpBinary"}, {"op", "<<"}, {"left", int_lit("0")}, {"right", int_lit("63")}
            }
        );
    }

    TEST_CASE("<< 移位量合法但结果溢出 int64_t，不折") {
        CHECK(
            fold_json(U"2 << 62") ==
            nlohmann::json{
                {"type", "OpBinary"}, {"op", "<<"}, {"left", int_lit("2")}, {"right", int_lit("62")}
            }
        );
        CHECK(fold_json(U"1 << 61") == int_lit("2305843009213693952"));
    }

    TEST_CASE("<< 移位量本身超出 int64_t 范围，不折") {
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

    TEST_CASE("操作数本身超出 int64_t 范围的 & ^ |，不折（哪怕数学上算得出来）") {
        CHECK(
            fold_json(U"99999999999999999999999999 & 1") ==
            nlohmann::json{
                {"type", "OpBinary"},
                {"op", "&"},
                {"left", int_lit("99999999999999999999999999")},
                {"right", int_lit("1")}
            }
        );
    }
}
