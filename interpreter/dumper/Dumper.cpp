#include "Dumper.h"

#include "../../utils/string_utils.h"

#include <nlohmann/json.hpp>

using json = nlohmann::ordered_json;

static json to_json(const AstNode *node);

static json make_node(const char *type) {
    return json{{"type", type}};
}

static json node_array(const std::vector<AstNodePtr> &vec) {
    json arr = json::array();
    for (const auto &item : vec) arr.push_back(to_json(item.get()));
    return arr;
}

// clang-format off
static const char *op_str_unary(const AstNodeOpUnary::OpType op) {
    switch (op) {
    case AstNodeOpUnary::OpType::Question: return "?";
    case AstNodeOpUnary::OpType::Exclaim:  return "!";
    case AstNodeOpUnary::OpType::Inc:      return "++";
    case AstNodeOpUnary::OpType::Dec:      return "--";
    case AstNodeOpUnary::OpType::Pos:      return "+";
    case AstNodeOpUnary::OpType::Neg:      return "-";
    case AstNodeOpUnary::OpType::BitNot:   return "~";
    case AstNodeOpUnary::OpType::Not:      return "not";
    default:                               return "<unknown>";
    }
}

static const char *op_str_binary(const AstNodeOpBinary::OpType op) {
    switch (op) {
    case AstNodeOpBinary::OpType::Add:      return "+";
    case AstNodeOpBinary::OpType::Sub:      return "-";
    case AstNodeOpBinary::OpType::Mul:      return "*";
    case AstNodeOpBinary::OpType::Div:      return "/";
    case AstNodeOpBinary::OpType::DivFloor: return "//";
    case AstNodeOpBinary::OpType::Mod:      return "%";
    case AstNodeOpBinary::OpType::Pow:      return "**";
    case AstNodeOpBinary::OpType::BitAnd:   return "&";
    case AstNodeOpBinary::OpType::BitOr:    return "|";
    case AstNodeOpBinary::OpType::BitXor:   return "^";
    case AstNodeOpBinary::OpType::LShift:   return "<<";
    case AstNodeOpBinary::OpType::RShift:   return ">>";
    case AstNodeOpBinary::OpType::Eq:       return "==";
    case AstNodeOpBinary::OpType::Ne:       return "!=";
    case AstNodeOpBinary::OpType::Lt:       return "<";
    case AstNodeOpBinary::OpType::Le:       return "<=";
    case AstNodeOpBinary::OpType::Gt:       return ">";
    case AstNodeOpBinary::OpType::Ge:       return ">=";
    case AstNodeOpBinary::OpType::Is:       return "is";
    case AstNodeOpBinary::OpType::And:      return "and";
    case AstNodeOpBinary::OpType::Or:       return "or";
    case AstNodeOpBinary::OpType::Range:    return "..";
    default:                                return "<unknown>";
    }
}

static const char *param_type_str(const AstNodeFunc::Param::ParamType pt) {
    switch (pt) {
    case AstNodeFunc::Param::ParamType::Normal:           return "Normal";
    case AstNodeFunc::Param::ParamType::StarArgs:         return "StarArgs";
    case AstNodeFunc::Param::ParamType::DoubleStarKwargs: return "DoubleStarKwargs";
    default:                                              return "<unknown>";
    }
}
// clang-format on

static json to_json(const AstNodeCall *node) {
    json j = make_node("Call");
    j["object"] = to_json(node->object_.get());
    j["args"] = node_array(node->args_);
    json kwargs = json::array();
    for (const auto &[key, val] : node->kwargs_) {
        kwargs.push_back({
            {"key", u32_to_utf8(key)},
            {"value", to_json(val.get())}
        });
    }
    j["kwargs"] = std::move(kwargs);
    return j;
}

static json to_json(const AstNodeIndex *node) {
    json j = make_node("Index");
    j["object"] = to_json(node->object_.get());
    j["args"] = node_array(node->args_);
    return j;
}

static json to_json(const AstNodeAttr *node) {
    json j = make_node("Attr");
    j["object"] = to_json(node->object_.get());
    j["attr"] = u32_to_utf8(node->attr_);
    return j;
}

static json to_json(const AstNodeIf *node) {
    json j = make_node("If");
    json clauses = json::array();
    for (const auto &clause : node->clauses_)
        clauses.push_back({
            {"cond", to_json(clause.cond_.get())},
            {"body", to_json(clause.body_.get())}
        });
    j["clauses"] = std::move(clauses);
    j["else_expr"] = to_json(node->else_expr_.get());
    return j;
}

static json to_json(const AstNodeForCond *node) {
    json j = make_node("ForCond");
    j["collect"] = node->collect_;
    j["init"] = to_json(node->init_.get());
    j["cond"] = to_json(node->cond_.get());
    j["inc"] = to_json(node->inc_.get());
    j["body"] = to_json(node->body_.get());
    return j;
}

static json to_json(const AstNodeForIter *node) {
    json j = make_node("ForIter");
    j["collect"] = node->collect_;
    j["target"] = to_json(node->target_.get());
    j["iterable"] = to_json(node->iterable_.get());
    j["body"] = to_json(node->body_.get());
    return j;
}

static json to_json(const AstNodeBreak * /* node */) {
    return make_node("Break");
}

static json to_json(const AstNodeContinue * /* node */) {
    return make_node("Continue");
}

static json to_json(const AstNodeReturn *node) {
    json j = make_node("Return");
    j["value"] = to_json(node->value_.get());
    return j;
}

static json to_json(const AstNodeTry *node) {
    json j = make_node("Try");
    j["try_expr"] = to_json(node->try_expr_.get());
    json except_clauses = json::array();
    for (const auto &clause : node->except_clauses_) {
        json exceptions = json::array();
        for (const auto &exc : clause.exceptions_) exceptions.push_back(to_json(exc.get()));
        except_clauses.push_back({
            {"exceptions", std::move(exceptions)},
            {"body", to_json(clause.body_.get())}
        });
    }
    j["except_clauses"] = std::move(except_clauses);
    j["finally_expr"] = to_json(node->finally_expr_.get());
    return j;
}

static json to_json(const AstNodeRaise *node) {
    json j = make_node("Raise");
    j["value"] = to_json(node->value_.get());
    return j;
}

static json to_json(const AstNodeDecorator *node) {
    json j = make_node("Decorator");
    j["decorator"] = to_json(node->decorator_.get());
    j["target"] = to_json(node->target_.get());
    return j;
}

static json to_json(const AstNodeFunc *node) {
    json j = make_node("Func");
    j["name"] = node->name_ ? json(u32_to_utf8(*node->name_)) : json(nullptr);
    json params = json::array();
    for (const auto &p : node->params_)
        params.push_back({
            {"identifier", u32_to_utf8(p.identifier)},
            {"param_type", param_type_str(p.param_type)},
            {"type_annotation", to_json(p.type_annotation.get())},
            {"default_value", to_json(p.default_value.get())}
        });
    j["params"] = std::move(params);
    j["body"] = to_json(node->body_.get());
    return j;
}

static json to_json(const AstNodeLiteralNone * /* node */) {
    return make_node("LiteralNone");
}

static json to_json(const AstNodeLiteralBool *node) {
    json j = make_node("LiteralBool");
    j["value"] = node->value_;
    return j;
}

static json to_json(const AstNodeLiteralGL *node) {
    json j = make_node("LiteralGL");
    j["value"] = (node->value_ == AstNodeLiteralGL::GLType::G) ? "_G" : "_L";
    return j;
}

static json to_json(const AstNodeLiteralInt *node) {
    json j = make_node("LiteralInt");
    j["raw"] = u32_to_utf8(node->raw_);
    return j;
}

static json to_json(const AstNodeLiteralFloat *node) {
    json j = make_node("LiteralFloat");
    j["raw"] = u32_to_utf8(node->raw_);
    return j;
}

static json to_json(const AstNodeLiteralStr *node) {
    json j = make_node("LiteralStr");
    j["value"] = u32_to_utf8(node->value_);
    return j;
}

static json to_json(const AstNodeLiteralTuple *node) {
    json j = make_node("LiteralTuple");
    j["items"] = node_array(node->items_);
    return j;
}

static json to_json(const AstNodeLiteralList *node) {
    json j = make_node("LiteralList");
    j["items"] = node_array(node->items_);
    return j;
}

static json to_json(const AstNodeLiteralDict *node) {
    json j = make_node("LiteralDict");
    json items = json::array();
    for (const auto &[key, val] : node->items_)
        items.push_back({{"key", to_json(key.get())},
                         {"val", to_json(val.get())}});
    j["items"] = std::move(items);
    return j;
}

static json to_json(const AstNodeLiteralEllipsis * /* node */) {
    return make_node("LiteralEllipsis");
}

static json to_json(const AstNodeProgram *node) {
    json j = make_node("Program");
    j["exprs"] = node_array(node->exprs_);
    return j;
}

static json to_json(const AstNodeCompound *node) {
    json j = make_node("Compound");
    j["exprs"] = node_array(node->exprs_);
    return j;
}

static json to_json(const AstNodeStar *node) {
    json j = make_node("Star");
    j["operand"] = to_json(node->operand_.get());
    return j;
}

static json to_json(const AstNodeDoubleStar *node) {
    json j = make_node("DoubleStar");
    j["operand"] = to_json(node->operand_.get());
    return j;
}

static json to_json(const AstNodeOpUnary *node) {
    json j = make_node("OpUnary");
    j["op"] = op_str_unary(node->op_);
    j["operand"] = to_json(node->operand_.get());
    return j;
}

static json to_json(const AstNodeOpBinary *node) {
    json j = make_node("OpBinary");
    j["op"] = op_str_binary(node->op_);
    j["left"] = to_json(node->left_.get());
    j["right"] = to_json(node->right_.get());
    return j;
}

static json to_json(const AstNodeAssign *node) {
    json j = make_node("Assign");
    j["target"] = to_json(node->target_.get());
    j["value"] = to_json(node->value_.get());
    return j;
}

static json to_json(const AstNodeCompoundAssign *node) {
    json j = make_node("CompoundAssign");
    j["target"] = to_json(node->target_.get());
    j["op"] = op_str_binary(node->op_);
    j["value"] = to_json(node->value_.get());
    return j;
}

static json to_json(const AstNodeIdentifier *node) {
    json j = make_node("Identifier");
    j["identifier"] = u32_to_utf8(node->identifier_);
    return j;
}

static json to_json(const AstNodeDel *node) {
    json j = make_node("Del");
    j["target"] = to_json(node->target_.get());
    return j;
}

static json to_json(const AstNodeGlobal *node) {
    json j = make_node("Global");
    j["target"] = to_json(node->target_.get());
    return j;
}

static json to_json(const AstNode *node) {
    if (!node) return nullptr;

#define X(nt) if (const auto *n{dynamic_cast<const nt *>(node)}) return to_json(n);
#include "../../parser/ast_nodes/x_ast_nodes.h"
#undef X

    return {{"type", "<unknown>"}};
}

Dumper::Dumper(std::ostream &os) : os_{os} {
}

void Dumper::dump(const AstNode *node, const int indent) const {
    os_ << to_json(node).dump(indent) << "\n";
}
