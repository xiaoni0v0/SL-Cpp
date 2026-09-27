#include "Analyzer.h"

#include "../parser/ast_nodes/ast_nodes.h"
#include "expr_folder/ExprFolder.h"
#include "semantic_checker/SemanticChecker.h"

#include <cassert>
#include <string>
#include <utility>

void Analyzer::analyze_program(AstNodeProgram &root, std::string file_path) {
    SemanticChecker::check_program(root, std::move(file_path));
    ExprFolder::fold_program(root);
}

void Analyzer::analyze_single_expr(
    AstNodePtr &expr, std::string file_path, const bool in_local_scope
) {
    assert(expr); // 调用方保证非空

    SemanticChecker::check_single_expr(*expr, std::move(file_path), in_local_scope);
    ExprFolder::fold_single_expr(expr);
}
