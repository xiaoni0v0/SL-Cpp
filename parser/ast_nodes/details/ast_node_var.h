#pragma once

#include "ast_node.h"
#include "../../../utils/string_utils.h"

#include <string>
#include <utility>


// global target
struct AstNodeIdentifier : AstNode {
    std::u32string identifier_;

    AstNodeIdentifier(const int row, const int col,
                      std::u32string identifier)
        : AstNode{row, col}, identifier_{std::move(identifier)} {
    }

    [[nodiscard]] json to_json() const override {
        return json{{"type", "Identifier"}, {"identifier", u32_to_utf8(identifier_)}};
    }
};

// del target
struct AstNodeDel : AstNode {
    AstNodePtr target_;

    AstNodeDel(const int row, const int col,
               AstNodePtr target)
        : AstNode{row, col}, target_{std::move(target)} {
    }

    [[nodiscard]] json to_json() const override {
        return json{{"type", "Del"}, {"target", target_->to_json()}};
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
        return json{{"type", "Global"}, {"target", target_->to_json()}};
    }
};
