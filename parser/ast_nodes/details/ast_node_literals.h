#pragma once

#include "ast_node.h"

#include <memory>
#include <string>
#include <vector>


// None
struct AstNodeLiteralNone : AstNode {
    AstNodeLiteralNone(const int row, const int col)
        : AstNode{row, col} {
    }
};

// bool
struct AstNodeLiteralBool : AstNode {
    bool value_;

    AstNodeLiteralBool(const int row, const int col,
                       const bool value)
        : AstNode{row, col}, value_{value} {
    }
};

struct AstNodeLiteralGL : AstNode {
    enum class GLType { G, L } value_;

    AstNodeLiteralGL(const int row, const int col,
                     const GLType value)
        : AstNode{row, col}, value_{value} {
    }
};

// int
struct AstNodeLiteralInt : AstNode {
    // 保留原文，解释器按需转换（"123" -> 123）
    std::u32string raw_;

    AstNodeLiteralInt(const int row, const int col,
                      std::u32string raw)
        : AstNode{row, col}, raw_{std::move(raw)} {
    }
};

// float
struct AstNodeLiteralFloat : AstNode {
    std::u32string raw_;

    AstNodeLiteralFloat(const int row, const int col,
                        std::u32string raw)
        : AstNode{row, col}, raw_{std::move(raw)} {
    }
};

// str
struct AstNodeLiteralStr : AstNode {
    // Lexer 已处理转义，value 为最终字符串内容
    std::u32string value_;

    AstNodeLiteralStr(const int row, const int col, std::u32string value)
        : AstNode{row, col}, value_{std::move(value)} {
    }
};

// (1, 2, 3)，单元素元组须有尾逗号
struct AstNodeLiteralTuple : AstNode {
    std::vector<AstNodePtr> items_; // 可空

    AstNodeLiteralTuple(const int row, const int col, std::vector<AstNodePtr> items)
        : AstNode{row, col}, items_{std::move(items)} {
    }
};

// [1, 2, 3]
struct AstNodeLiteralList : AstNode {
    std::vector<AstNodePtr> items_; // 可空

    AstNodeLiteralList(const int row, const int col,
                       std::vector<AstNodePtr> items)
        : AstNode{row, col}, items_{std::move(items)} {
    }
};

// ['k1': 'v1', 'k2': 'v2']
struct AstNodeLiteralDict : AstNode {
    std::vector<std::pair<AstNodePtr, AstNodePtr>> items_; // 可空

    AstNodeLiteralDict(const int row, const int col,
                       std::vector<std::pair<AstNodePtr, AstNodePtr>> items)
        : AstNode{row, col}, items_{std::move(items)} {
    }
};

// ...
struct AstNodeLiteralEllipsis : AstNode {
    AstNodeLiteralEllipsis(const int row, const int col)
        : AstNode{row, col} {
    }
};
