#pragma once

#include "ast_node.h"

#include <string>
#include <utility>
#include <vector>

// ============================================================
// 访问表达式
// ============================================================

// f(arg1, arg2, kw=val, ...)
struct AstNodeCall : AstNode {
    AstNodePtr object_;
    std::vector<AstNodePtr> args_;
    std::vector<std::pair<std::u32string, AstNodePtr>> kwargs_;

    AstNodeCall(const int row, const int col,
                AstNodePtr object,
                std::vector<AstNodePtr> args,
                std::vector<std::pair<std::u32string, AstNodePtr>> kwargs)
        : AstNode{row, col}, object_{std::move(object)}, args_{std::move(args)}, kwargs_{std::move(kwargs)} {
    }
};

// x[i]  x[i, j, ...]
struct AstNodeIndex : AstNode {
    AstNodePtr object_;
    std::vector<AstNodePtr> args_;

    AstNodeIndex(const int row, const int col,
                 AstNodePtr object,
                 std::vector<AstNodePtr> args)
        : AstNode{row, col}, object_{std::move(object)}, args_{std::move(args)} {
    }
};

// x.attr
struct AstNodeAttr : AstNode {
    AstNodePtr object_;
    std::u32string attr_;

    AstNodeAttr(const int row, const int col,
                AstNodePtr object,
                std::u32string attr)
        : AstNode{row, col}, object_{std::move(object)}, attr_{std::move(attr)} {
    }
};
