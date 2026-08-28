#include "Analyzer.h"

#include "expr_folder/ExprFolder.h"
#include "semantic_checker/SemanticChecker.h"

#include <cassert>
#include <utility>

void Analyzer::analyze_program(AstNodeProgram &root, std::string file_path) {
    SemanticChecker{root, std::move(file_path)}.check();
    ExprFolder::fold(root);
}

void Analyzer::analyze_single_expr(AstNodePtr &expr, std::string file_path) {
    assert(expr); // 调用方保证非空

    SemanticChecker{*expr, std::move(file_path)}.check();
    ExprFolder::fold_expr(expr);
}
