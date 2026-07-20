#pragma once

#include "../../parser/ast_nodes/ast_nodes.h"


class LiteralFolder {
    [[maybe_unused]] AstNodeProgram *root_;

public:
    explicit LiteralFolder(AstNodeProgram *root);

    void fold() const &&;
};
