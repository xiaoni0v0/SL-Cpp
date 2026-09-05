sl_add_test_target(
    sl_test_numeric_bigint
    LIBS sl_numeric
    SOURCES
        main_test.cpp
        01_bigint/test_utils.h
        01_bigint/construct_convert_test.cpp
        01_bigint/arithmetic_test.cpp
        01_bigint/bitwise_shift_test.cpp
        01_bigint/identities_test.cpp
        01_bigint/python_cross_test.cpp
        # 生成的交叉验证表（不要手改）
        big_int_cases.inc
)

sl_add_test_target(
    sl_test_numeric_bigdec
    LIBS sl_numeric
    SOURCES
        main_test.cpp
        02_bigdec/test_utils.h
        02_bigdec/construct_context_test.cpp
        02_bigdec/arithmetic_test.cpp
        02_bigdec/identities_pow_test.cpp
        02_bigdec/narrow_path_test.cpp
        02_bigdec/python_cross_test.cpp
        # 生成的交叉验证表（不要手改）
        big_dec_cases.inc
)
