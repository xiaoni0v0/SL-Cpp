#pragma once

#include "../../builtins/object/SlObject.h"
#include "../../parser/ast_nodes/ast_nodes.h"
#include "../frame/Frame.h"


class Evaler {

#define X(nt) SlObject *eval(const nt *node, Frame &frame) const;
#include "../../parser/ast_nodes/x_ast_nodes.h"
#undef X

public:
    /**
     * 构造 Evaler 对象
     */
    explicit Evaler() = default;

    /**
     * 求值
     * @param node 要求值的节点
     * @param frame 当前的栈帧对象
     */
    SlObject *eval(const AstNode *node, Frame &frame) const;
};
