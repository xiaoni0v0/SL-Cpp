#pragma once

#include "../parser/ast_nodes/ast_nodes.h"

#include <string>

/**
 * 语义分析层的门面：按顺序跑 SemanticChecker（只报错、不改树）和 ExprFolder（只改树、不报错）。
 */
class Analyzer final {
  public:
    Analyzer() = delete;
    ~Analyzer() = delete;
    Analyzer(const Analyzer &) = delete;
    Analyzer(Analyzer &&) = delete;
    Analyzer &operator=(const Analyzer &) = delete;
    Analyzer &operator=(Analyzer &&) = delete;

    /**
     * 分析一整份 Program
     * 折叠会走到 Program 级别的死语句剪枝
     * @param root      Program 根节点，原地修改
     * @param file_path 文件路径，默认为 "<unknown>"
     */
    static void analyze_program(AstNodeProgram &root, std::string file_path = "<unknown>");

    /**
     * 分析单独一条表达式
     * 折叠只折这条表达式本身，不涉及 Program 级别的剪枝。
     * @param expr      表达式节点，原地修改（折叠可能整个换掉这个节点）。调用方保证非空
     * @param file_path 文件路径，默认为 "<unknown>"
     */
    static void analyze_single_expr(AstNodePtr &expr, std::string file_path = "<unknown>");
};
