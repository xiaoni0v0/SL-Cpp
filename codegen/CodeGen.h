#pragma once

#include "../parser/ast_nodes/ast_visitor.h"

class CodeGen : public AstConstVisitor {
#define X(nt) void visit(const nt &node) override;
#include "../parser/ast_nodes/x_ast_nodes.inc"

#undef X
};
