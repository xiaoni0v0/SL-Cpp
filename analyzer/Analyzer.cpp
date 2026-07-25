#include "Analyzer.h"

#include "literal_folder/LiteralFolder.h"
#include "syntax_checker/SyntaxChecker.h"

Analyzer::Analyzer(AstNodeProgram &root, std::string file_path)
    : root_{root}, file_path_{std::move(file_path)} {}

void Analyzer::analyze() const && {
    // 先检查。这个 SyntaxChecker 可能报错，但绝不修改树
    SyntaxChecker{root_, file_path_}.check();

    // 再折叠。这个 LiteralFolder 可能修改树，但绝不报错
    LiteralFolder{root_}.fold();
}
