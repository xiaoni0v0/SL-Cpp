#include "Analyzer.h"

#include "literal_folder/LiteralFolder.h"
#include "syntax_checker/SyntaxChecker.h"

Analyzer::Analyzer(AstNodeProgram *const root, std::string file_path)
    : root_{root}, file_path_{std::move(file_path)} {
}

void Analyzer::analyze() const && {
    SyntaxChecker{root_, file_path_}.check();
    LiteralFolder{root_}.fold();
}
