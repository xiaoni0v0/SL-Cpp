#pragma once

#include "ast_node.h"
#include "../../../utils/string_utils.h"

#include <string>
#include <utility>


// 标识符
struct AstNodeIdentifier : AstNode {
    std::u32string identifier_;

    explicit AstNodeIdentifier(const Position pos,
                               std::u32string identifier)
        : AstNode{pos}, identifier_{std::move(identifier)} {
    }

    [[nodiscard]] json to_json() const override {
        return json{{"type", "Identifier"}, {"identifier", u32_to_utf8(identifier_)}};
    }
};

// del target（target 语法上是表达式，2.2.3 限定只能是标识符或属性访问，由语义层校验具体形状）
struct AstNodeDel : AstNode {
    AstNodePtr target_;

    explicit AstNodeDel(const Position pos,
                        AstNodePtr target)
        : AstNode{pos}, target_{std::move(target)} {
    }

    [[nodiscard]] json to_json() const override {
        return json{{"type", "Del"}, {"target", target_->to_json()}};
    }
};

// global identifier（语法本身就是标识符，2.2.4，不是表达式，解析时直接 expect(IDENTIFIER)）
struct AstNodeGlobal : AstNode {
    std::u32string identifier_;

    explicit AstNodeGlobal(const Position pos,
                           std::u32string identifier)
        : AstNode{pos}, identifier_{std::move(identifier)} {
    }

    [[nodiscard]] json to_json() const override {
        return json{{"type", "Global"}, {"identifier", u32_to_utf8(identifier_)}};
    }
};
