// StaticEvaler/LiteralFolder：位运算折叠（& ^ | << >>，只对 bool/int 有意义，SL.md 3.4.2）。
// dict 的 | 合并见 container_ops_test.cpp（跟 int 的 | 是同一个运算符，按左操作数类型分派）。
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
        CHECK(fold_json(U"-1 >> 100") == int_lit("-1"));
    }

    TEST_CASE("<< >> 基本情况") {
        CHECK(fold_json(U"1 << 4") == int_lit("16"));
        CHECK(fold_json(U"16 >> 4") == int_lit("1"));
        CHECK(fold_json(U"-16 >> 4") == int_lit("-1"));
    }

    TEST_CASE("负数移位不折，交给运行时报错") {
        CHECK(
            fold_json(U"1 << -1") == nlohmann::json{{"type", "OpBinary"},
                                                    {"op", "<<"},
                                                    {"left", int_lit("1")},
                                                    {"right", int_lit("-1")}}
        );
    }

    TEST_CASE("bool 参与位运算按 int 提升") {
        CHECK(fold_json(U"True & 1") == int_lit("1"));
        CHECK(fold_json(U"True | False") == int_lit("1"));
    }

    TEST_CASE("float 不参与位运算，不折") {
        CHECK(
            fold_json(U"1.0 & 1") == nlohmann::json{{"type", "OpBinary"},
                                                    {"op", "&"},
                                                    {"left", float_lit("1.0")},
                                                    {"right", int_lit("1")}}
        );
    }
}
