#pragma once

#include "../parser/ast_nodes/ast_nodes.h"

#include <string>


class Analyzer {
    AstNodeProgram &root_;
    const std::string file_path_;

public:
    /**
     * 构造 Analyzer 对象
     * @param root      AST 的根节点
     * @param file_path 文件路径，默认为 "<unknown>"
     */
    explicit Analyzer(AstNodeProgram &root, std::string file_path = "<unknown>");

    /**
     * 入口
     */
    void analyze() const &&;
};
