#pragma once

#include "../../parser/ast_nodes/ast_nodes.h"

#include <string>


class SyntaxChecker {
    AstNodeProgram &root_;
    const std::string file_path_;

    struct Context {
        // 是否身处 func 体或 class 体的局部作用域内（SL.md 2.2.4/3.4.4："只能在局部作用域（函数体或
        // 类体）中使用"）——只用来判断 global 合不合法。return 现在处处合法（离它最近的 Program 就是
        // 它的作用对象，哪怕在文件顶层，见 3.4.1/3.4.5.6），不需要靠这个判断，所以没有 return 专用的计数
        int local_scope_depth{0};
        int loop_depth{0}; // for、while 共用（break/continue 是否合法）
        bool can_star{false};
        bool can_double_star{false};
    } ctx_{};

    // 报错
    [[noreturn]] void error(const std::string &msg, Position pos) const;
    void require_not_null(const AstNodePtr &node) const;
    void require_not_null(const std::u32string &name) const;

    template <typename T>
    void require_not_null(const std::vector<T> &name) const {
        if (name.empty()) error("unexpected null vector", Position{0, 0});
    }

    // 检查节点，dispatch；要求 node 非空
    void check(const AstNode &node);
    // 检查一个可能为空的槽位：空则跳过（对应"这个槽位本来就没有"），非空则 check
    void check_optional(const AstNodePtr &node);

    // 每种节点的
#define X(nt) void check(const nt &node);
#include "../../parser/ast_nodes/x_ast_nodes.h"
#undef X

    // 检查一个节点是否可以作为左值（含解构：元组/列表，元素里最多一个可以带 * 前缀）
    void check_lvalue(const AstNode &node) const;
    // check_lvalue 的辅助：检查解构元组/列表的各元素，校验"至多一个 *lv"（2.1.5 第 4 点）
    void check_lvalue_items(const std::vector<AstNodePtr> &items) const;
    // 检查一个节点是否可以作为"简单左值"（标识符/属性访问/元素访问，不含解构），
    // 用于复合赋值这类不支持解构的场合
    void check_simple_lvalue(const AstNode &node) const;

    // 检查 func/class 的 doc 槽位：必须为空，或者恰好是一个字符串字面量（不允许变量、拼接、重复等
    // 其他表达式，见 SL.md 3.4.6）——这是一条纯形状检查，不需要知道任何值，因此不依赖折叠结果，
    // check 可以放心在 fold 之前做
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
