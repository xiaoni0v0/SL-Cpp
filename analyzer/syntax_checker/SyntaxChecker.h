#pragma once

#include "../../parser/ast_nodes/ast_nodes.h"

#include <string>
#include <vector>


class SyntaxChecker {
    AstNodeProgram *root_;
    std::string file_path_;

    struct Context {
        int func_depth{0};
        int for_depth{0};
        bool star_ok{false};
        bool double_star_ok{false};
    } ctx_{};

    [[noreturn]] void error(const std::string &msg, int row, int col) const;

    void check(const AstNode *node);

#define X(nt) void check(const nt *node);
#include "../../parser/ast_nodes/x_ast_nodes.h"
#undef X

    void check_lvalue(const AstNode *node, bool allow_star = false) const;
    void check_simple_lvalue(const AstNode *node) const;
    void check_unpack_items(const std::vector<AstNodePtr> &items) const;

public:
    explicit SyntaxChecker(AstNodeProgram *root, std::string file_path);

    void check() &&;
};
