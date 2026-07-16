#pragma once

#include "ast_node.h"
#include "../../../utils/string_utils.h"

#include <string>
#include <utility>


// 标识符
struct AstNodeIdentifier : AstNode {
    std::u32string identifier_;

    AstNodeIdentifier(const int row, const int col,
                      std::u32string identifier)
        : AstNode{row, col}, identifier_{std::move(identifier)} {
    }

    [[nodiscard]] json to_json() const override {
        json j{{"type", "Identifier"}};
        j["identifier"] = u32_to_utf8(identifier_);
        return j;
    }
};

// del target（target 必须是标识符，由语义层校验）
struct AstNodeDel : AstNode {
    AstNodePtr target_;

    AstNodeDel(const int row, const int col,
               AstNodePtr target)
        : AstNode{row, col}, target_{std::move(target)} {
    }

    [[nodiscard]] json to_json() const override {
        json j{{"type", "Del"}};
        j["target"] = target_->to_json();
        return j;
    }
};

// global target（target 必须是标识符，由语义层校验）
struct AstNodeGlobal : AstNode {
    AstNodePtr target_;

    AstNodeGlobal(const int row, const int col,
                  AstNodePtr target)
        : AstNode{row, col}, target_{std::move(target)} {
    }

    [[nodiscard]] json to_json() const override {
        json j{{"type", "Global"}};
        j["target"] = target_->to_json();
        return j;
    }
};
