#include "Analyzer.h"

#include "expr_folder/ExprFolder.h"
#include "semantic_checker/SemanticChecker.h"

Analyzer::Analyzer(AstNodeProgram &root, std::string file_path)
    : root_{root}, file_path_{std::move(file_path)} {}

void Analyzer::analyze() const && {
    // 先检查。这个 SemanticChecker 可能报错，但绝不修改树
    SemanticChecker{root_, file_path_}.check();

    // 再折叠。这个 ExprFolder 可能修改树，但绝不报错
    ExprFolder{root_}.fold();
}
