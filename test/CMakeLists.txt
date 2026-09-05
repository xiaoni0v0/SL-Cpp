# 建一个测试目标：链上被测模块，套测试专用的编译参数，注册进 ctest。
function(sl_add_test_target target)
    cmake_parse_arguments(ARG "" "" "LIBS;SOURCES" ${ARGN})

    add_executable(${target} ${ARG_SOURCES})
    sl_apply_test_target_options(${target})
    target_link_libraries(${target} PRIVATE ${ARG_LIBS} doctest::doctest)
    target_precompile_headers(${target} PRIVATE <doctest/doctest.h>)

    foreach(src IN LISTS ARG_SOURCES)
        if(src MATCHES "main_test\\.cpp$")
            # 每个测试目标的 main 都是 doctest 的入口，不走预编译头。
            set_source_files_properties(
                ${src}
                PROPERTIES SKIP_PRECOMPILE_HEADERS ON
            )
        endif()
    endforeach()

    add_test(NAME ${target} COMMAND ${target})
endfunction()

sl_add_test_target(
    SL_Cpp_Numeric_BigInt_Tests
    LIBS sl_numeric
    SOURCES
        numeric/main_test.cpp
        numeric/01_bigint/test_utils.h
        numeric/01_bigint/construct_convert_test.cpp
        numeric/01_bigint/arithmetic_test.cpp
        numeric/01_bigint/bitwise_shift_test.cpp
        numeric/01_bigint/identities_test.cpp
        numeric/01_bigint/python_cross_test.cpp
        # 生成的交叉验证表（不要手改）
        numeric/big_int_cases.inc
)

sl_add_test_target(
    SL_Cpp_Numeric_BigDec_Tests
    LIBS sl_numeric
    SOURCES
        numeric/main_test.cpp
        numeric/02_bigdec/test_utils.h
        numeric/02_bigdec/construct_context_test.cpp
        numeric/02_bigdec/arithmetic_test.cpp
        numeric/02_bigdec/identities_pow_test.cpp
        numeric/02_bigdec/narrow_path_test.cpp
        numeric/02_bigdec/python_cross_test.cpp
        # 生成的交叉验证表（不要手改）
        numeric/big_dec_cases.inc
)

sl_add_test_target(
    SL_Cpp_Lexer_Tests
    LIBS sl_lexer
    SOURCES
        lexer/test_utils.h
        lexer/main_test.cpp
        # 注释
        lexer/01_comments/comments_test.cpp
        # 关键字与保留字
        lexer/02_keywords_reserved/keywords_test.cpp
        lexer/02_keywords_reserved/reserved_words_test.cpp
        # 标识符
        lexer/03_identifiers/identifiers_test.cpp
        # 字面量
        lexer/04_literals/none_bool_ellipsis_test.cpp
        lexer/04_literals/int_test.cpp
        lexer/04_literals/decimal_test.cpp
        lexer/04_literals/scientific_notation_test.cpp
        lexer/04_literals/string_test.cpp
        lexer/04_literals/raw_string_test.cpp
        # 运算符与符号
        lexer/05_operators/symbols_test.cpp
        lexer/05_operators/dot_disambiguation_test.cpp
        # 空白
        lexer/06_whitespace/whitespace_test.cpp
        # 报错与位置
        lexer/07_errors/error_handling_test.cpp
        lexer/07_errors/position_tracking_test.cpp
)

sl_add_test_target(
    SL_Cpp_Parser_Tests
    LIBS sl_parser
    SOURCES
        parser/test_utils.h
        parser/main_test.cpp
        parser/01_literals/literals_test.cpp
        parser/01_literals/literal_raw_test.cpp
        parser/02_operators/precedence_test.cpp
        parser/02_operators/compare_is_test.cpp
        parser/02_operators/in_test.cpp
        parser/02_operators/assign_test.cpp
        parser/02_operators/unpack_test.cpp
        parser/03_separators/separator_test.cpp
        parser/04_compound_dict/compound_dict_test.cpp
        parser/05_del/del_test.cpp
        parser/06_global/global_test.cpp
        parser/07_import/import_test.cpp
        parser/08_control_flow/if_test.cpp
        parser/08_control_flow/for_test.cpp
        parser/08_control_flow/while_test.cpp
        parser/08_control_flow/collect_mark_test.cpp
        parser/08_control_flow/break_continue_test.cpp
        parser/08_control_flow/return_test.cpp
        parser/08_control_flow/try_test.cpp
        parser/08_control_flow/raise_test.cpp
        parser/09_func/func_test.cpp
        parser/10_class/class_test.cpp
        parser/11_decorator/decorator_test.cpp
        parser/12_eval/eval_test.cpp
        parser/13_cross/combination_test.cpp
        parser/13_cross/error_handling_test.cpp
        parser/13_cross/position_tracking_test.cpp
        parser/13_cross/single_expr_entry_test.cpp
)
target_precompile_headers(SL_Cpp_Parser_Tests PRIVATE <nlohmann/json.hpp>)

sl_add_test_target(
    SL_Cpp_Analyzer_Tests
    LIBS sl_analyzer
    SOURCES
        # 测试工具（复用 Parser 测试的 parse_as_file/parse_json 等）
        parser/test_utils.h
        analyzer/test_utils.h
        analyzer/semantic_checker/test_utils.h
        analyzer/main_test.cpp
        analyzer/analyzer_facade_test.cpp
        analyzer/recheck_after_fold_test.cpp
        # semantic_checker
        analyzer/semantic_checker/scope_test.cpp
        analyzer/semantic_checker/lvalue_test.cpp
        analyzer/semantic_checker/del_test.cpp
        analyzer/semantic_checker/try_finally_test.cpp
        analyzer/semantic_checker/dict_call_test.cpp
        analyzer/semantic_checker/func_class_test.cpp
        analyzer/semantic_checker/import_test.cpp
        analyzer/semantic_checker/eval_test.cpp
        analyzer/semantic_checker/single_expr_test.cpp
        analyzer/semantic_checker/defensive_test.cpp
        # expr_folder
        analyzer/expr_folder/arithmetic_test.cpp
        analyzer/expr_folder/bitwise_test.cpp
        analyzer/expr_folder/logical_test.cpp
        analyzer/expr_folder/compare_is_test.cpp
        analyzer/expr_folder/container_ops_test.cpp
        analyzer/expr_folder/numeric_fidelity_test.cpp
        analyzer/expr_folder/dead_branch_test.cpp
        analyzer/expr_folder/compound_test.cpp
        analyzer/expr_folder/program_prune_test.cpp
        analyzer/expr_folder/call_index_attr_assign_test.cpp
        analyzer/expr_folder/try_raise_test.cpp
        analyzer/expr_folder/func_class_decorator_test.cpp
        analyzer/expr_folder/import_test.cpp
        analyzer/expr_folder/eval_test.cpp
        analyzer/expr_folder/for_iter_test.cpp
        analyzer/expr_folder/del_test.cpp
)
target_precompile_headers(SL_Cpp_Analyzer_Tests PRIVATE <nlohmann/json.hpp>)
