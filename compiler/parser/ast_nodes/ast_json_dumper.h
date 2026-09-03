#pragma once

#include "ast_nodes.h"

#include <nlohmann/json.hpp>
#include <vector>

using json = nlohmann::ordered_json;

/**
 * 把 AST 序列化成 JSON。字段顺序跟节点结构体成员声明顺序一致。
 */
class AstJsonDumper : public AstConstVisitor {
    // 是否把位置信息也 dump 进去
    const bool include_pos_;

    // 当前节点的产出
    json result_;

    explicit AstJsonDumper(const bool include_pos) : include_pos_{include_pos} {}

    // 入口
    [[nodiscard]] json dump_node(const AstNode &node);
    // 可空节点，空的话给 null
    [[nodiscard]] json dump_or_null(const AstNodePtr &node);

    [[nodiscard]] json dump_nodes(const std::vector<AstNodePtr> &nodes);
    [[nodiscard]] json dump_kwargs(const std::vector<OneKwArg> &kwargs);
    // 普通函数调用、import 调用形态、eval 共用的那部分
    [[nodiscard]] json dump_call_args(const CallArgs &args);
    [[nodiscard]] json dump_captures(const std::vector<OneCapture> &captures);
    [[nodiscard]] json dump_one_param(const AstNodeFunc::OneParam &param);
    [[nodiscard]] json dump_all_params(const AstNodeFunc::AllParams &params);

#define X(nt) void visit(const nt &node) override;
#include "x_ast_nodes.inc"

#undef X

  public:
    /**
     * 序列化一棵 AST 到 JSON
     * @param include_pos 是否把位置信息也 dump 进去
     */
    [[nodiscard]] static json dump(const AstNode &node, bool include_pos = false);
};
