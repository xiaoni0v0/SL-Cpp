sl_add_module(
    sl_analyzer
    Analyzer.cpp
    Analyzer.h
    expr_folder/ExprFolder.cpp
    expr_folder/ExprFolder.h
    expr_folder/StaticEvaler.cpp
    expr_folder/StaticEvaler.h
    semantic_checker/SemanticChecker.cpp
    semantic_checker/SemanticChecker.h
)

target_link_libraries(sl_analyzer PUBLIC sl_parser)
