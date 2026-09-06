#pragma once

#include <string>

// 源码位置：文件 + 行列。**行列从 1 开始**，跟 lexer 的 Position 一致。
//
// 这里不复用 `compiler/parser/.../ast_node.h` 里的 `Position`：那个只有行列、且属于
// compiler 层，而 cpp_exceptions 在依赖链上位于 utils/lexer 之下，不能反过来依赖 compiler
struct SourceLocation {
    std::string file_path;
    int row{0};
    int col{0};
};
