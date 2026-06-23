#include "Evaler.h"

#include <cassert>

SlObject *Evaler::eval(const AstNodeCall *node, Frame &frame) const {
}

SlObject *Evaler::eval(const AstNodeIndex *node, Frame &frame) const {
}

SlObject *Evaler::eval(const AstNodeAttr *node, Frame &frame) const {
}

SlObject *Evaler::eval(const AstNodeIf *node, Frame &frame) const {
}

SlObject *Evaler::eval(const AstNodeForCond *node, Frame &frame) const {
}

SlObject *Evaler::eval(const AstNodeForIter *node, Frame &frame) const {
}

SlObject *Evaler::eval(const AstNodeBreak *node, Frame &frame) const {
}

SlObject *Evaler::eval(const AstNodeContinue *node, Frame &frame) const {
}

SlObject *Evaler::eval(const AstNodeReturn *node, Frame &frame) const {
}

SlObject *Evaler::eval(const AstNodeTry *node, Frame &frame) const {
}

SlObject *Evaler::eval(const AstNodeRaise *node, Frame &frame) const {
}

SlObject *Evaler::eval(const AstNodeDecorator *node, Frame &frame) const {
}

SlObject *Evaler::eval(const AstNodeFunc *node, Frame &frame) const {
}

SlObject *Evaler::eval(const AstNodeLiteralNone *node, Frame &frame) const {
}

SlObject *Evaler::eval(const AstNodeLiteralBool *node, Frame &frame) const {
}

SlObject *Evaler::eval(const AstNodeLiteralGL *node, Frame &frame) const {
}

SlObject *Evaler::eval(const AstNodeLiteralInt *node, Frame &frame) const {
}

SlObject *Evaler::eval(const AstNodeLiteralFloat *node, Frame &frame) const {
}

SlObject *Evaler::eval(const AstNodeLiteralStr *node, Frame &frame) const {
}

SlObject *Evaler::eval(const AstNodeLiteralTuple *node, Frame &frame) const {
}

SlObject *Evaler::eval(const AstNodeLiteralList *node, Frame &frame) const {
}

SlObject *Evaler::eval(const AstNodeLiteralDict *node, Frame &frame) const {
}

SlObject *Evaler::eval(const AstNodeLiteralEllipsis *node, Frame &frame) const {
}

SlObject *Evaler::eval(const AstNodeProgram *node, Frame &frame) const {
}

SlObject *Evaler::eval(const AstNodeCompound *node, Frame &frame) const {
}

SlObject *Evaler::eval(const AstNodeStar *node, Frame &frame) const {
}

SlObject *Evaler::eval(const AstNodeDoubleStar *node, Frame &frame) const {
}

SlObject *Evaler::eval(const AstNodeOpUnary *node, Frame &frame) const {
}

SlObject *Evaler::eval(const AstNodeOpBinary *node, Frame &frame) const {
}

SlObject *Evaler::eval(const AstNodeAssign *node, Frame &frame) const {
}

SlObject *Evaler::eval(const AstNodeCompoundAssign *node, Frame &frame) const {
}

SlObject *Evaler::eval(const AstNodeIdentifier *node, Frame &frame) const {
}

SlObject *Evaler::eval(const AstNodeDel *node, Frame &frame) const {
}

SlObject *Evaler::eval(const AstNodeGlobal *node, Frame &frame) const {
}

SlObject *Evaler::eval(const AstNode *node, Frame &frame) const {
    if (!node) {
        assert(!"null node");
    }

#define X(nt) if (const auto *n{dynamic_cast<const nt *>(node)}) return eval(n, frame);
#include "../../parser/ast_nodes/x_ast_nodes.h"
#undef X

    assert(!"Unknown node type");
}
