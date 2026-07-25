#include "../../utils/string_utils.h"
#include "ast_nodes.h"

namespace {

json captures_to_json(const std::vector<OneCapture> &captures) {
    auto result = json::array();
    for (const auto &c : captures)
        result.push_back(
            {{"kind", c.capture_type_ == OneCapture::CaptureType::Value ? "Value" : "Reference"},
             {"identifier", u32_to_utf8(c.identifier_)},
             {"value_expr", c.value_expr_ ? c.value_expr_->to_json() : json(nullptr)}}
        );
    return result;
}

json one_param_to_json(const AstNodeFunc::OneParam &p) {
    return json{
        {"identifier", u32_to_utf8(p.identifier_)},
        {"type_annotation", p.type_annotation_ ? p.type_annotation_->to_json() : json(nullptr)},
        {"default_value", p.default_value_ ? p.default_value_->to_json() : json(nullptr)}
    };
}

json all_params_to_json(const AstNodeFunc::AllParams &params) {
    auto positional = json::array();
    for (const auto &p : params.positional_) positional.push_back(one_param_to_json(p));

    auto kw_only = json::array();
    for (const auto &p : params.kw_only_) kw_only.push_back(one_param_to_json(p));

    return json{
        {"positional", std::move(positional)},
        {"var_args",
         params.var_args_name_ ? json(u32_to_utf8(*params.var_args_name_)) : json(nullptr)},
        {"kw_only", std::move(kw_only)},
        {"var_kwargs",
         params.var_kwargs_name_ ? json(u32_to_utf8(*params.var_kwargs_name_)) : json(nullptr)}
    };
}

const char *op_str(const AstNodeOpUnary::OpType op) {
    switch (op) {
    case AstNodeOpUnary::OpType::Question:
        return "?";
    case AstNodeOpUnary::OpType::Exclaim:
        return "!";
    case AstNodeOpUnary::OpType::Pos:
        return "+";
    case AstNodeOpUnary::OpType::Neg:
        return "-";
    case AstNodeOpUnary::OpType::BitNot:
        return "~";
    case AstNodeOpUnary::OpType::Not:
        return "not";
    default:
        return "<unknown>";
    }
}

const char *op_str(const AstNodeOpBinary::OpType op) {
    switch (op) {
    case AstNodeOpBinary::OpType::Add:
        return "+";
    case AstNodeOpBinary::OpType::Sub:
        return "-";
    case AstNodeOpBinary::OpType::Mul:
        return "*";
    case AstNodeOpBinary::OpType::Div:
        return "/";
    case AstNodeOpBinary::OpType::DivFloor:
        return "//";
    case AstNodeOpBinary::OpType::Mod:
        return "%";
    case AstNodeOpBinary::OpType::Pow:
        return "**";
    case AstNodeOpBinary::OpType::BitAnd:
        return "&";
    case AstNodeOpBinary::OpType::BitOr:
        return "|";
    case AstNodeOpBinary::OpType::BitXor:
        return "^";
    case AstNodeOpBinary::OpType::LShift:
        return "<<";
    case AstNodeOpBinary::OpType::RShift:
        return ">>";
    case AstNodeOpBinary::OpType::And:
        return "and";
    case AstNodeOpBinary::OpType::Or:
        return "or";
    case AstNodeOpBinary::OpType::Range:
        return "..";
    default:
        return "<unknown>";
    }
}

const char *op_str(const AstNodeCompare::OpType op) {
    switch (op) {
    case AstNodeCompare::OpType::Lt:
        return "<";
    case AstNodeCompare::OpType::Le:
        return "<=";
    case AstNodeCompare::OpType::Gt:
        return ">";
    case AstNodeCompare::OpType::Ge:
        return ">=";
    case AstNodeCompare::OpType::Eq:
        return "==";
    case AstNodeCompare::OpType::Ne:
        return "!=";
    default:
        return "<unknown>";
    }
}

} // namespace

json AstNodeClass::to_json() const {
    auto decorators = json::array();
    for (const auto &d : decorators_) decorators.push_back(d->to_json());
    auto bases = json::array();
    for (const auto &base : bases_) bases.push_back(base->to_json());

    return json{
        {"type", "Class"},
        {"decorators", std::move(decorators)},
        {"name", name_ ? json(u32_to_utf8(*name_)) : json(nullptr)},
        {"bases", std::move(bases)},
        {"captures", captures_to_json(captures_)},
        {"doc", doc_ ? doc_->to_json() : json(nullptr)},
        {"body", body_->to_json()}
    };
}

json AstNodeIf::to_json() const {
    auto clauses = json::array();
    for (const auto &clause : clauses_)
        clauses.push_back({{"cond", clause.cond_->to_json()}, {"body", clause.body_->to_json()}});
    return json{
        {"type", "If"},
        {"clauses", std::move(clauses)},
        {"else_expr", else_expr_ ? else_expr_->to_json() : json(nullptr)}
    };
}

json AstNodeForCond::to_json() const {
    return json{
        {"type", "ForCond"},
        {"collect", collect_},
        {"init", init_ ? init_->to_json() : json(nullptr)},
        {"cond", cond_ ? cond_->to_json() : json(nullptr)},
        {"inc", inc_ ? inc_->to_json() : json(nullptr)},
        {"body", body_->to_json()}
    };
}

json AstNodeForIter::to_json() const {
    return json{
        {"type", "ForIter"},
        {"collect", collect_},
        {"target", target_->to_json()},
        {"iterable", iterable_->to_json()},
        {"body", body_->to_json()}
    };
}

json AstNodeBreak::to_json() const { return json{{"type", "Break"}}; }

json AstNodeContinue::to_json() const { return json{{"type", "Continue"}}; }

json AstNodeReturn::to_json() const {
    return json{{"type", "Return"}, {"value", value_ ? value_->to_json() : json(nullptr)}};
}

json AstNodeTry::to_json() const {
    auto except_clauses = json::array();
    for (const auto &clause : except_clauses_) {
        auto exceptions = json::array();
        for (const auto &exc : clause.exceptions_) exceptions.push_back(exc->to_json());
        except_clauses.push_back(
            {{"exceptions", std::move(exceptions)}, {"body", clause.body_->to_json()}}
        );
    }
    return json{
        {"type", "Try"},
        {"try_expr", try_expr_->to_json()},
        {"except_clauses", std::move(except_clauses)},
        {"finally_expr", finally_expr_ ? finally_expr_->to_json() : json(nullptr)}
    };
}

json AstNodeRaise::to_json() const { return json{{"type", "Raise"}, {"value", value_->to_json()}}; }

json AstNodeDecorator::to_json() const {
    return json{
        {"type", "Decorator"}, {"decorator", decorator_->to_json()}, {"target", target_->to_json()}
    };
}

json AstNodeFunc::to_json() const {
    auto decorators = json::array();
    for (const auto &d : decorators_) decorators.push_back(d->to_json());

    return json{
        {"type", "Func"},
        {"decorators", std::move(decorators)},
        {"name", name_ ? json(u32_to_utf8(*name_)) : json(nullptr)},
        {"captures", captures_to_json(captures_)},
        {"params", all_params_to_json(params_)},
        {"return_type", return_type_ ? return_type_->to_json() : json(nullptr)},
        {"doc", doc_ ? doc_->to_json() : json(nullptr)},
        {"body", body_->to_json()}
    };
}

json AstNodeLiteralNone::to_json() const { return json{{"type", "LiteralNone"}}; }

json AstNodeLiteralBool::to_json() const {
    return json{{"type", "LiteralBool"}, {"value", value_}};
}

json AstNodeLiteralGL::to_json() const {
    return json{{"type", "LiteralGL"}, {"value", value_ == GLType::G ? "_G" : "_L"}};
}

json AstNodeLiteralInt::to_json() const {
    return json{{"type", "LiteralInt"}, {"raw", u32_to_utf8(raw_)}};
}

json AstNodeLiteralFloat::to_json() const {
    return json{{"type", "LiteralFloat"}, {"raw", u32_to_utf8(raw_)}};
}

json AstNodeLiteralStr::to_json() const {
    return json{{"type", "LiteralStr"}, {"value", u32_to_utf8(value_)}};
}

json AstNodeLiteralTuple::to_json() const {
    auto items = json::array();
    for (const auto &item : items_) items.push_back(item->to_json());
    return json{{"type", "LiteralTuple"}, {"items", std::move(items)}};
}

json AstNodeLiteralList::to_json() const {
    auto items = json::array();
    for (const auto &item : items_) items.push_back(item->to_json());
    return json{{"type", "LiteralList"}, {"items", std::move(items)}};
}

json AstNodeLiteralDict::to_json() const {
    auto items = json::array();
    for (const auto &[key, val] : items_)
        items.push_back({{"key", key->to_json()}, {"val", val ? val->to_json() : json(nullptr)}});
    return json{{"type", "LiteralDict"}, {"items", std::move(items)}};
}

json AstNodeLiteralEllipsis::to_json() const { return json{{"type", "LiteralEllipsis"}}; }

json AstNodeProgram::to_json() const {
    auto exprs = json::array();
    for (const auto &e : exprs_) exprs.push_back(e->to_json());
    return json{{"type", "Program"}, {"exprs", std::move(exprs)}};
}

json AstNodeCompound::to_json() const {
    auto exprs = json::array();
    for (const auto &e : exprs_) exprs.push_back(e->to_json());
    return json{{"type", "Compound"}, {"exprs", std::move(exprs)}};
}

json AstNodeStar::to_json() const {
    return json{{"type", "Star"}, {"operand", operand_->to_json()}};
}

json AstNodeDoubleStar::to_json() const {
    return json{{"type", "DoubleStar"}, {"operand", operand_->to_json()}};
}

json AstNodeOpUnary::to_json() const {
    return json{{"type", "OpUnary"}, {"op", op_str(op_)}, {"operand", operand_->to_json()}};
}

json AstNodeOpBinary::to_json() const {
    return json{
        {"type", "OpBinary"},
        {"op", op_str(op_)},
        {"left", left_->to_json()},
        {"right", right_->to_json()}
    };
}

json AstNodeCompare::to_json() const {
    auto operands = json::array();
    for (const auto &operand : operands_) operands.push_back(operand->to_json());
    auto ops = json::array();
    for (const auto &op : ops_) ops.push_back(op_str(op));
    return json{{"type", "Compare"}, {"operands", std::move(operands)}, {"ops", std::move(ops)}};
}

json AstNodeIs::to_json() const {
    auto operands = json::array();
    for (const auto &operand : operands_) operands.push_back(operand->to_json());
    return json{{"type", "Is"}, {"operands", std::move(operands)}};
}

json AstNodeAssign::to_json() const {
    return json{{"type", "Assign"}, {"target", target_->to_json()}, {"value", value_->to_json()}};
}

json AstNodeCompoundAssign::to_json() const {
    return json{
        {"type", "CompoundAssign"},
        {"target", target_->to_json()},
        {"op", op_str(op_)},
        {"value", value_->to_json()}
    };
}

json AstNodeCall::to_json() const {
    auto args = json::array();
    for (const auto &arg : positional_args_) args.push_back(arg->to_json());
    auto kwargs = json::array();
    for (const auto &kw : keyword_args_)
        kwargs.push_back(
            {{"key",
              kw.kind_ == OneKwArg::Kind::Keyword ? json(u32_to_utf8(kw.keyword_)) : json(nullptr)},
             {"value", kw.value_->to_json()}}
        );
    return json{
        {"type", "Call"},
        {"object", object_->to_json()},
        {"args", std::move(args)},
        {"kwargs", std::move(kwargs)}
    };
}

json AstNodeIndex::to_json() const {
    auto args = json::array();
    for (const auto &arg : args_) args.push_back(arg->to_json());
    return json{{"type", "Index"}, {"object", object_->to_json()}, {"args", std::move(args)}};
}

json AstNodeAttr::to_json() const {
    return json{{"type", "Attr"}, {"object", object_->to_json()}, {"attr", u32_to_utf8(attr_)}};
}

json AstNodeIdentifier::to_json() const {
    return json{{"type", "Identifier"}, {"identifier", u32_to_utf8(identifier_)}};
}

json AstNodeDel::to_json() const { return json{{"type", "Del"}, {"target", target_->to_json()}}; }

json AstNodeGlobal::to_json() const {
    return json{{"type", "Global"}, {"identifier", u32_to_utf8(identifier_)}};
}
