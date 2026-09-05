sl_add_test_target(
    sl_test_analyzer
    LIBS sl_compiler_analyzer
    SOURCES
        # 测试工具（复用 Parser 测试的 parse_as_file/parse_json 等）
        ../parser/test_utils.h
        test_utils.h
        semantic_checker/test_utils.h
        analyzer_facade_test.cpp
        recheck_after_fold_test.cpp
        # semantic_checker
        semantic_checker/scope_test.cpp
        semantic_checker/lvalue_test.cpp
        semantic_checker/del_test.cpp
        semantic_checker/try_finally_test.cpp
        semantic_checker/dict_call_test.cpp
        semantic_checker/func_class_test.cpp
        semantic_checker/import_test.cpp
        semantic_checker/eval_test.cpp
        semantic_checker/single_expr_test.cpp
        semantic_checker/defensive_test.cpp
        # expr_folder
        expr_folder/arithmetic_test.cpp
        expr_folder/bitwise_test.cpp
        expr_folder/logical_test.cpp
        expr_folder/compare_is_test.cpp
        expr_folder/container_ops_test.cpp
        expr_folder/numeric_fidelity_test.cpp
        expr_folder/dead_branch_test.cpp
        expr_folder/compound_test.cpp
        expr_folder/program_prune_test.cpp
        expr_folder/call_index_attr_assign_test.cpp
        expr_folder/try_raise_test.cpp
        expr_folder/func_class_decorator_test.cpp
        expr_folder/import_test.cpp
        expr_folder/eval_test.cpp
        expr_folder/for_iter_test.cpp
        expr_folder/del_test.cpp
)
target_precompile_headers(sl_test_analyzer PRIVATE <nlohmann/json.hpp>)
