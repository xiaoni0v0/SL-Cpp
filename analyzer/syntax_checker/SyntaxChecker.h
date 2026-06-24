#pragma once

#include "../../parser/ast_nodes/ast_nodes.h"

#include <string>


class SyntaxChecker {
    AstNodeProgram *root_;
    std::string file_path_;

    struct Context {
        int func_depth{0};
        int for_depth{0};
        bool can_star{false};
        bool can_double_star{false};
    } ctx_{};

    // 报错
    [[noreturn]] void error(const std::string &msg, int row, int col) const;
    void require_not_null(const AstNodePtr &node) const;
    void require_not_null(const std::u32string &name) const;

    template <typename T>
    void require_not_null(const std::vector<T> &name) const {
        if (name.empty()) error("unexpected null vector", 0, 0);
    }

    // 检查节点，dispatch
    void check(const AstNode *node);

    // 每种节点的
#define X(nt) void check(const nt *node);
#include "../../parser/ast_nodes/x_ast_nodes.h"
#undef X

    // 检查一个节点是否可以作为左值。要求 node 非空
    void check_lvalue(const AstNode *node) const;

public:
    /**
     * 构造 SyntaxChecker 对象
     * @param root      AST 的根节点
     * @param file_path 文件路径，用于错误信息
     */
    explicit SyntaxChecker(AstNodeProgram *root, std::string file_path);

    /**
     * 语法合法性检查
     */
    void check() &&;
};
