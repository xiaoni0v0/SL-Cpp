#pragma once

#include "../../parser/ast_nodes/ast_nodes.h"

#include <string>


class SyntaxChecker {
    AstNodeProgram &root_;
    const std::string file_path_;

    struct Context {
        int local_scope_depth{0}; // 是否身处 func 体或 class 体的局部作用域内
        int loop_depth{0}; // for、while 共用
        bool can_star{false};
        bool can_double_star{false};
    } ctx_;

    // 报错
    [[noreturn]] void error(const std::string &msg, Position pos) const;
    // 节点不能是 nullptr
    void require_not_null(const AstNodePtr &node, Position pos) const;
    // 名字不能是 ""
    void require_not_null(const std::u32string &name, Position pos) const;
    // vector 元素个数不能少于 min_size
    template <typename T>
    void require_not_null(const std::vector<T> &vec, const size_t min_size, const Position pos) const {
        if (vec.size() < min_size) error("Bad AstNode: too few elements", pos);
    }

    // 两个 vector 长度必须相等
    template <typename T, typename U>
    void require_same_size(const std::vector<T> &a, const std::vector<U> &b, const Position pos) const {
        if (a.size() != b.size()) error("Bad AstNode: mismatched array sizes", pos);
    }

    // 检查节点，dispatch；要求 node 非空
    void check(const AstNode &node);

#define X(nt) void check(const nt &node);
#include "../../parser/ast_nodes/x_ast_nodes.h"
#undef X

    // 检查节点，dispatch：node 可空
    void check_optional(const AstNodePtr &node);

    // 检查一个节点是否是左值
    void check_lvalue(const AstNode &node) const;
    // check_lvalue 的辅助：检查解构元组/列表的各元素，校验至多一个 *args
    void check_lvalue_items(const std::vector<AstNodePtr> &items) const;
    // 检查一个节点是否是“纯左值”（标识符/属性访问/元素访问）
    void check_lvalue_pure(const AstNode &node) const;

    // 检查 func/class 的 doc 槽位：必须为空或者恰好是一个字符串字面量
    void check_doc(const AstNodePtr &doc) const;

public:
    /**
     * 构造 SyntaxChecker 对象
     * @param root      AST 的根节点
     * @param file_path 文件路径，默认为 "<unknown>"
     */
    explicit SyntaxChecker(AstNodeProgram &root, std::string file_path = "<unknown>");

    /**
     * 语法合法性检查
     */
    void check() &&;
};
