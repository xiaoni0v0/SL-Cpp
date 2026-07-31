#pragma once

#include "../../parser/ast_nodes/ast_nodes.h"

#include <string>

class SemanticChecker {
    const AstNodeProgram &root_;
    const std::string file_path_;

    struct Context {
        int local_scope_depth{0};   // 是否身处 func 体或 class 体的局部作用域内
        int loop_depth{0};          // for、while 共用
        int finally_loop_depth{-1}; // 身处 finally 体时的外层 loop_depth（-1 表示不在 finally 内）
        bool can_star{false};
        bool can_double_star{false};
    } ctx_;

    // 报错：SyntaxError
    [[noreturn]] void error(const std::string &msg, Position pos) const;
    // 报错：InternalError
    [[noreturn]] void error_internal(const std::string &msg, Position pos) const;

    // 节点不能是 nullptr（Parser 保证，触发即 InternalError）
    void require_not_null(const AstNodePtr &node, Position pos) const;
    // 名字不能是 ""（Parser 保证，触发即 InternalError）
    void require_not_null(const std::u32string &name, Position pos) const;

    /**
     * int/float 字面量 raw_（或 raw_ 按小数点拆出的一段）必须是非空的纯十进制数字串
     * @param no_leading_zero 是否禁止前导零（单独一个 "0" 除外）
     */
    void require_digits(const std::u32string &raw, bool no_leading_zero, Position pos) const;

    // vector 元素个数不能少于 min_size（Parser 保证，触发即 InternalError）
    template <typename T>
    void
    require_not_null(const std::vector<T> &vec, const size_t min_size, const Position pos) const {
        if (vec.size() < min_size) error_internal("too few elements", pos);
    }

    // 两个 vector 长度必须相等（Parser 保证，触发即 InternalError）
    template <typename T, typename U>
    void
    require_same_size(const std::vector<T> &a, const std::vector<U> &b, const Position pos) const {
        if (a.size() != b.size()) error_internal("mismatched array sizes", pos);
    }

    // 检查节点，dispatch
    void check(const AstNode &node);

#define X(nt) void check(const nt &node);
#include "../../parser/ast_nodes/x_ast_nodes.h"
#undef X

    // 检查节点，node 不可空
    void check_not_null(const AstNodePtr &node, Position pos);
    void check_not_null(const AstNodeProgramPtr &node, Position pos);
    // 检查节点，node 可空
    void check_nullable(const AstNodePtr &node);

    // 检查一个节点是否是左值；仍会走一遍完整的 check()
    void check_lvalue(const AstNode &node);
    // check_lvalue 的辅助：检查解构元组/列表的各元素，校验至多一个 *args
    void check_lvalue_items(const std::vector<AstNodePtr> &items, Position pos);
    // 检查一个节点是否是“纯左值”（标识符/属性访问/元素访问）；仍会走一遍完整的 check()
    void check_lvalue_pure(const AstNode &node);

    // 检查 func/class 的 doc 槽位：必须为空或者恰好是一个字符串字面量
    void check_doc(const AstNodePtr &doc) const;

  public:
    /**
     * 构造 SemanticChecker 对象
     * @param root      AST 的根节点
     * @param file_path 文件路径，默认为 "<unknown>"
     */
    explicit SemanticChecker(const AstNodeProgram &root, std::string file_path = "<unknown>");

    /**
     * 语法合法性检查
     */
    void check() &&;
};
