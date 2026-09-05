sl_add_test_target(
    sl_test_lexer
    LIBS sl_compiler_lexer
    SOURCES
        test_utils.h
        main_test.cpp
        # 注释
        01_comments/comments_test.cpp
        # 关键字与保留字
        02_keywords_reserved/keywords_test.cpp
        02_keywords_reserved/reserved_words_test.cpp
        # 标识符
        03_identifiers/identifiers_test.cpp
        # 字面量
        04_literals/none_bool_ellipsis_test.cpp
        04_literals/int_test.cpp
        04_literals/decimal_test.cpp
        04_literals/scientific_notation_test.cpp
        04_literals/string_test.cpp
        04_literals/raw_string_test.cpp
        # 运算符与符号
        05_operators/symbols_test.cpp
        05_operators/dot_disambiguation_test.cpp
        # 空白
        06_whitespace/whitespace_test.cpp
        # 报错与位置
        07_errors/error_handling_test.cpp
        07_errors/position_tracking_test.cpp
)
