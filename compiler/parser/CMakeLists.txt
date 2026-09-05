sl_add_module(
    sl_compiler_parser
    Parser.cpp
    Parser.h
    ast_nodes/ast_json_dumper.cpp
    ast_nodes/ast_json_dumper.h
    ast_nodes/ast_nodes.h
    ast_nodes/ast_visitor.h
    ast_nodes/x_ast_nodes.inc
    ast_nodes/details/ast_node.h
    ast_nodes/details/ast_node_class.h
    ast_nodes/details/ast_node_control_flows.h
    ast_nodes/details/ast_node_decorators.h
    ast_nodes/details/ast_node_eval.h
    ast_nodes/details/ast_node_func.h
    ast_nodes/details/ast_node_import.h
    ast_nodes/details/ast_node_literals.cpp
    ast_nodes/details/ast_node_literals.h
    ast_nodes/details/ast_node_misc.h
    ast_nodes/details/ast_node_multi_exprs.h
    ast_nodes/details/ast_node_operators.h
    ast_nodes/details/ast_node_postfix.h
    ast_nodes/details/ast_node_var.h
)

target_link_libraries(
    sl_compiler_parser
    PUBLIC sl_compiler_lexer nlohmann_json::nlohmann_json
)
target_precompile_headers(sl_compiler_parser PRIVATE <nlohmann/json.hpp>)
