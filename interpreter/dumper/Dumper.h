#pragma once

#include "../../parser/ast_nodes/ast_nodes.h"

#include <iostream>

class Dumper {
    std::ostream &os_;

public:
    explicit Dumper(std::ostream &os = std::cout);

    /**
     * 将 AST 以 JSON 格式输出
     * @param node   要输出的节点
     * @param indent 缩进空格数（-1 表示压缩输出）
     */
    void dump(const AstNode *node, int indent = 2) const;
};
