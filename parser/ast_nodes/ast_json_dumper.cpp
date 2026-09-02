#include "ast_json_dumper.h"

#include "../../utils/string_utils.h"

namespace {

json pos_to_json(const Position pos) { return json{{"row", pos.row}, {"col", pos.col}}; }

json positions_to_json(const std::vector<Position> &positions) {
    auto arr = json::array();
    for (const auto pos : positions) arr.push_back(pos_to_json(pos));
    return arr;
}

// 收集模式记号原样序列化成它在源码里的写法
const char *collect_to_json(const CollectMark mark) {
    switch (mark.container_) {
    case CollectMark::Container::None:
        return "none";
    case CollectMark::Container::List:
        return mark.expand_ ? "$ *" : "$";
    case CollectMark::Container::Dict:
        return mark.expand_ ? "$$ **" : "$$";
    }
    return "none";
}

const char *op_str(const AstNodeOpUnary::OpType op) {
    // clang-format off
    switch (op) {
    case AstNodeOpUnary::OpType::Question:  return "?";
    case AstNodeOpUnary::OpType::Exclaim:   return "!";
    case AstNodeOpUnary::OpType::Pos:       return "+";
    case AstNodeOpUnary::OpType::Neg:       return "-";
    case AstNodeOpUnary::OpType::BitInvert: return "~";
    case AstNodeOpUnary::OpType::Not:       return "not";
    // clang-format on
    default:
        return "<unknown>";
    }
}

const char *op_str(const AstNodeOpBinary::OpType op) {
    // clang-format off
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
    case AstNodeOpBinary::OpType::And:      return "and";
    case AstNodeOpBinary::OpType::Or:       return "or";
    case AstNodeOpBinary::OpType::Range:    return "..";
    case AstNodeOpBinary::OpType::In:       return "in";
    // clang-format on
    default:
        return "<unknown>";
    }
}

const char *op_str(const AstNodeCompare::OpType op) {
    // clang-format off
    switch (op) {
    case AstNodeCompare::OpType::Lt: return "<";
    case AstNodeCompare::OpType::Le: return "<=";
    case AstNodeCompare::OpType::Gt: return ">";
    case AstNodeCompare::OpType::Ge: return ">=";
    case AstNodeCompare::OpType::Eq: return "==";
    case AstNodeCompare::OpType::Ne: return "!=";
    // clang-format on
    default:
        return "<unknown>";
    }
}

} // namespace

json AstJsonDumper::dump_node(const AstNode &node) {
    node.accept(*this);
    return std::move(result_);
}

json AstJsonDumper::dump_or_null(const AstNodePtr &node) {
    return node ? dump_node(*node) : json(nullptr);
}

json AstJsonDumper::dump_nodes(const std::vector<AstNodePtr> &nodes) {
    auto arr = json::array();
    for (const auto &n : nodes) arr.push_back(dump_node(*n));
    return arr;
}

json AstJsonDumper::dump_kwargs(const std::vector<OneKwArg> &kwargs) {
    auto arr = json::array();
    for (const auto &kw : kwargs)
        arr.push_back(
            json{
                {"keyword",
                 kw.kind_ == OneKwArg::Kind::Keyword ? json(u32_to_utf8(kw.keyword_))
                                                     : json(nullptr)},
                {"value", dump_node(*kw.value_)}
            }
        );
    return arr;
}

json AstJsonDumper::dump_call_args(const CallArgs &args) {
    auto positional_args = dump_nodes(args.positional_args_);
    auto keyword_args = dump_kwargs(args.keyword_args_);

    if (include_pos_)
        return json{
            {"positional_args", std::move(positional_args)},
            {"keyword_args", std::move(keyword_args)},
            {"pos_paren", pos_to_json(args.pos_paren_)}
        };
    return json{
        {"positional_args", std::move(positional_args)}, {"keyword_args", std::move(keyword_args)}
    };
}

json AstJsonDumper::dump_captures(const std::vector<OneCapture> &captures) {
    auto arr = json::array();
    for (const auto &c : captures)
        arr.push_back(
            json{
                {"capture_type",
                 c.capture_type_ == OneCapture::CaptureType::Value ? "Value" : "Reference"},
                {"identifier", u32_to_utf8(c.identifier_)},
                {"value_expr", dump_or_null(c.value_expr_)}
            }
        );
    return arr;
}

json AstJsonDumper::dump_one_param(const AstNodeFunc::OneParam &param) {
    return json{
        {"identifier", u32_to_utf8(param.identifier_)},
        {"type_annotation", dump_or_null(param.type_annotation_)},
        {"default_value", dump_or_null(param.default_value_)}
    };
}

json AstJsonDumper::dump_all_params(const AstNodeFunc::AllParams &params) {
    auto positional = json::array();
    for (const auto &p : params.positional_) positional.push_back(dump_one_param(p));

    auto kw_only = json::array();
    for (const auto &p : params.kw_only_) kw_only.push_back(dump_one_param(p));

    return json{
        {"positional", std::move(positional)},
        {"var_args_name",
         params.var_args_name_ ? json(u32_to_utf8(*params.var_args_name_)) : json(nullptr)},
        {"kw_only", std::move(kw_only)},
        {"var_kwargs_name",
         params.var_kwargs_name_ ? json(u32_to_utf8(*params.var_kwargs_name_)) : json(nullptr)}
    };
}

void AstJsonDumper::visit(const AstNodeClass &node) {
    auto decorators = dump_nodes(node.decorators_);
    auto bases = dump_nodes(node.bases_);

    if (include_pos_) {
        result_ = json{
            {"type", "Class"},
            {"pos", pos_to_json(node.pos_)},
            {"decorators", std::move(decorators)},
            {"positions_decorator", positions_to_json(node.positions_decorator_)},
            {"name", node.name_ ? json(u32_to_utf8(*node.name_)) : json(nullptr)},
            {"bases", std::move(bases)},
            {"captures", dump_captures(node.captures_)},
            {"doc", dump_or_null(node.doc_)},
            {"body", dump_node(*node.body_)}
        };
        return;
    }
    result_ = json{
        {"type", "Class"},
        {"decorators", std::move(decorators)},
        {"name", node.name_ ? json(u32_to_utf8(*node.name_)) : json(nullptr)},
        {"bases", std::move(bases)},
        {"captures", dump_captures(node.captures_)},
        {"doc", dump_or_null(node.doc_)},
        {"body", dump_node(*node.body_)}
    };
}

void AstJsonDumper::visit(const AstNodeIf &node) {
    auto clauses = json::array();
    for (const auto &clause : node.clauses_)
        clauses.push_back(
            json{{"cond", dump_node(*clause.cond_)}, {"body", dump_node(*clause.body_)}}
        );

    if (include_pos_) {
        result_ = json{
            {"type", "If"},
            {"pos", pos_to_json(node.pos_)},
            {"clauses", std::move(clauses)},
            {"else_expr", dump_or_null(node.else_expr_)}
        };
        return;
    }
    result_ = json{
        {"type", "If"},
        {"clauses", std::move(clauses)},
        {"else_expr", dump_or_null(node.else_expr_)}
    };
}

void AstJsonDumper::visit(const AstNodeForCond &node) {
    if (include_pos_) {
        result_ = json{
            {"type", "ForCond"},
            {"pos", pos_to_json(node.pos_)},
            {"collect", collect_to_json(node.collect_)},
            {"init", dump_or_null(node.init_)},
            {"cond", dump_or_null(node.cond_)},
            {"inc", dump_or_null(node.inc_)},
            {"body", dump_node(*node.body_)}
        };
        return;
    }
    result_ = json{
        {"type", "ForCond"},
        {"collect", collect_to_json(node.collect_)},
        {"init", dump_or_null(node.init_)},
        {"cond", dump_or_null(node.cond_)},
        {"inc", dump_or_null(node.inc_)},
        {"body", dump_node(*node.body_)}
    };
}

void AstJsonDumper::visit(const AstNodeForIter &node) {
    if (include_pos_) {
        result_ = json{
            {"type", "ForIter"},
            {"pos", pos_to_json(node.pos_)},
            {"collect", collect_to_json(node.collect_)},
            {"iterable", dump_node(*node.iterable_)},
            {"target", dump_or_null(node.target_)},
            {"body", dump_node(*node.body_)}
        };
        return;
    }
    result_ = json{
        {"type", "ForIter"},
        {"collect", collect_to_json(node.collect_)},
        {"iterable", dump_node(*node.iterable_)},
        {"target", dump_or_null(node.target_)},
        {"body", dump_node(*node.body_)}
    };
}

void AstJsonDumper::visit(const AstNodeBreak &node) {
    if (include_pos_) {
        result_ = json{{"type", "Break"}, {"pos", pos_to_json(node.pos_)}};
        return;
    }
    result_ = json{{"type", "Break"}};
}

void AstJsonDumper::visit(const AstNodeContinue &node) {
    if (include_pos_) {
        result_ = json{{"type", "Continue"}, {"pos", pos_to_json(node.pos_)}};
        return;
    }
    result_ = json{{"type", "Continue"}};
}

void AstJsonDumper::visit(const AstNodeReturn &node) {
    if (include_pos_) {
        result_ = json{
            {"type", "Return"},
            {"pos", pos_to_json(node.pos_)},
            {"value", dump_or_null(node.value_)}
        };
        return;
    }
    result_ = json{{"type", "Return"}, {"value", dump_or_null(node.value_)}};
}

void AstJsonDumper::visit(const AstNodeTry &node) {
    auto except_clauses = json::array();
    for (const auto &clause : node.except_clauses_) {
        auto exceptions = dump_nodes(clause.exceptions_);
        except_clauses.push_back(
            json{
                {"exceptions", std::move(exceptions)},
                {"target", dump_or_null(clause.target_)},
                {"body", dump_node(*clause.body_)}
            }
        );
    }

    if (include_pos_) {
        result_ = json{
            {"type", "Try"},
            {"pos", pos_to_json(node.pos_)},
            {"try_expr", dump_node(*node.try_expr_)},
            {"except_clauses", std::move(except_clauses)},
            {"finally_expr", dump_or_null(node.finally_expr_)}
        };
        return;
    }
    result_ = json{
        {"type", "Try"},
        {"try_expr", dump_node(*node.try_expr_)},
        {"except_clauses", std::move(except_clauses)},
        {"finally_expr", dump_or_null(node.finally_expr_)}
    };
}

void AstJsonDumper::visit(const AstNodeRaise &node) {
    if (include_pos_) {
        result_ = json{
            {"type", "Raise"}, {"pos", pos_to_json(node.pos_)}, {"value", dump_node(*node.value_)}
        };
        return;
    }
    result_ = json{{"type", "Raise"}, {"value", dump_node(*node.value_)}};
}

void AstJsonDumper::visit(const AstNodeDecorator &node) {
    if (include_pos_) {
        result_ = json{
            {"type", "Decorator"},
            {"pos", pos_to_json(node.pos_)},
            {"decorator", dump_node(*node.decorator_)},
            {"target", dump_node(*node.target_)}
        };
        return;
    }
    result_ = json{
        {"type", "Decorator"},
        {"decorator", dump_node(*node.decorator_)},
        {"target", dump_node(*node.target_)}
    };
}

void AstJsonDumper::visit(const AstNodeEval &node) {
    auto args_json = dump_call_args(node.args_);
    if (include_pos_) {
        result_ = json{
            {"type", "Eval"},
            {"pos", pos_to_json(node.pos_)},
            {"positional_args", args_json["positional_args"]},
            {"keyword_args", args_json["keyword_args"]},
            {"pos_paren", args_json["pos_paren"]}
        };
        return;
    }
    result_ = json{
        {"type", "Eval"},
        {"positional_args", args_json["positional_args"]},
        {"keyword_args", args_json["keyword_args"]}
    };
}

void AstJsonDumper::visit(const AstNodeFunc &node) {
    auto decorators = dump_nodes(node.decorators_);

    if (include_pos_) {
        result_ = json{
            {"type", "Func"},
            {"pos", pos_to_json(node.pos_)},
            {"decorators", std::move(decorators)},
            {"positions_decorator", positions_to_json(node.positions_decorator_)},
            {"name", node.name_ ? json(u32_to_utf8(*node.name_)) : json(nullptr)},
            {"captures", dump_captures(node.captures_)},
            {"params", dump_all_params(node.params_)},
            {"return_type", dump_or_null(node.return_type_)},
            {"doc", dump_or_null(node.doc_)},
            {"body", dump_node(*node.body_)}
        };
        return;
    }
    result_ = json{
        {"type", "Func"},
        {"decorators", std::move(decorators)},
        {"name", node.name_ ? json(u32_to_utf8(*node.name_)) : json(nullptr)},
        {"captures", dump_captures(node.captures_)},
        {"params", dump_all_params(node.params_)},
        {"return_type", dump_or_null(node.return_type_)},
        {"doc", dump_or_null(node.doc_)},
        {"body", dump_node(*node.body_)}
    };
}

void AstJsonDumper::visit(const AstNodeImportKw &node) {
    auto segments = json::array();
    for (const auto &segment : node.segments_) segments.push_back(u32_to_utf8(segment));

    if (include_pos_) {
        result_ = json{
            {"type", "ImportKw"}, {"pos", pos_to_json(node.pos_)}, {"segments", std::move(segments)}
        };
        return;
    }
    result_ = json{{"type", "ImportKw"}, {"segments", std::move(segments)}};
}

void AstJsonDumper::visit(const AstNodeImportCall &node) {
    auto args_json = dump_call_args(node.args_);
    if (include_pos_) {
        result_ = json{
            {"type", "ImportCall"},
            {"pos", pos_to_json(node.pos_)},
            {"positional_args", args_json["positional_args"]},
            {"keyword_args", args_json["keyword_args"]},
            {"pos_paren", args_json["pos_paren"]}
        };
        return;
    }
    result_ = json{
        {"type", "ImportCall"},
        {"positional_args", args_json["positional_args"]},
        {"keyword_args", args_json["keyword_args"]}
    };
}

void AstJsonDumper::visit(const AstNodeLiteralNone &node) {
    if (include_pos_) {
        result_ = json{{"type", "LiteralNone"}, {"pos", pos_to_json(node.pos_)}};
        return;
    }
    result_ = json{{"type", "LiteralNone"}};
}

void AstJsonDumper::visit(const AstNodeLiteralBool &node) {
    if (include_pos_) {
        result_ =
            json{{"type", "LiteralBool"}, {"pos", pos_to_json(node.pos_)}, {"value", node.value_}};
        return;
    }
    result_ = json{{"type", "LiteralBool"}, {"value", node.value_}};
}

void AstJsonDumper::visit(const AstNodeLiteralGL &node) {
    const auto *const value = node.value_ == AstNodeLiteralGL::GLType::G ? "_G" : "_L";

    if (include_pos_) {
        result_ = json{{"type", "LiteralGL"}, {"pos", pos_to_json(node.pos_)}, {"value", value}};
        return;
    }
    result_ = json{{"type", "LiteralGL"}, {"value", value}};
}

void AstJsonDumper::visit(const AstNodeLiteralInt &node) {
    if (include_pos_) {
        result_ = json{
            {"type", "LiteralInt"}, {"pos", pos_to_json(node.pos_)}, {"raw", u32_to_utf8(node.raw_)}
        };
        return;
    }
    result_ = json{{"type", "LiteralInt"}, {"raw", u32_to_utf8(node.raw_)}};
}

void AstJsonDumper::visit(const AstNodeLiteralDecimal &node) {
    if (include_pos_) {
        result_ = json{
            {"type", "LiteralDecimal"},
            {"pos", pos_to_json(node.pos_)},
            {"raw", u32_to_utf8(node.raw_)}
        };
        return;
    }
    result_ = json{{"type", "LiteralDecimal"}, {"raw", u32_to_utf8(node.raw_)}};
}

void AstJsonDumper::visit(const AstNodeLiteralStr &node) {
    if (include_pos_) {
        result_ = json{
            {"type", "LiteralStr"},
            {"pos", pos_to_json(node.pos_)},
            {"value", u32_to_utf8(node.value_)}
        };
        return;
    }
    result_ = json{{"type", "LiteralStr"}, {"value", u32_to_utf8(node.value_)}};
}

void AstJsonDumper::visit(const AstNodeLiteralTuple &node) {
    auto items = dump_nodes(node.items_);

    if (include_pos_) {
        result_ = json{
            {"type", "LiteralTuple"}, {"pos", pos_to_json(node.pos_)}, {"items", std::move(items)}
        };
        return;
    }
    result_ = json{{"type", "LiteralTuple"}, {"items", std::move(items)}};
}

void AstJsonDumper::visit(const AstNodeLiteralList &node) {
    auto items = dump_nodes(node.items_);

    if (include_pos_) {
        result_ = json{
            {"type", "LiteralList"}, {"pos", pos_to_json(node.pos_)}, {"items", std::move(items)}
        };
        return;
    }
    result_ = json{{"type", "LiteralList"}, {"items", std::move(items)}};
}

void AstJsonDumper::visit(const AstNodeLiteralDict &node) {
    auto items = json::array();
    for (const auto &[key, val] : node.items_)
        items.push_back(json{{"key", dump_node(*key)}, {"value", dump_or_null(val)}});

    if (include_pos_) {
        result_ = json{
            {"type", "LiteralDict"}, {"pos", pos_to_json(node.pos_)}, {"items", std::move(items)}
        };
        return;
    }
    result_ = json{{"type", "LiteralDict"}, {"items", std::move(items)}};
}

void AstJsonDumper::visit(const AstNodeLiteralEllipsis &node) {
    if (include_pos_) {
        result_ = json{{"type", "LiteralEllipsis"}, {"pos", pos_to_json(node.pos_)}};
        return;
    }
    result_ = json{{"type", "LiteralEllipsis"}};
}

void AstJsonDumper::visit(const AstNodeProgram &node) {
    auto exprs = dump_nodes(node.exprs_);

    if (include_pos_) {
        result_ =
            json{{"type", "Program"}, {"pos", pos_to_json(node.pos_)}, {"exprs", std::move(exprs)}};
        return;
    }
    result_ = json{{"type", "Program"}, {"exprs", std::move(exprs)}};
}

void AstJsonDumper::visit(const AstNodeCompound &node) {
    auto exprs = dump_nodes(node.exprs_);

    if (include_pos_) {
        result_ = json{
            {"type", "Compound"}, {"pos", pos_to_json(node.pos_)}, {"exprs", std::move(exprs)}
        };
        return;
    }
    result_ = json{{"type", "Compound"}, {"exprs", std::move(exprs)}};
}

void AstJsonDumper::visit(const AstNodeStar &node) {
    if (include_pos_) {
        result_ = json{
            {"type", "Star"},
            {"pos", pos_to_json(node.pos_)},
            {"operand", dump_node(*node.operand_)}
        };
        return;
    }
    result_ = json{{"type", "Star"}, {"operand", dump_node(*node.operand_)}};
}

void AstJsonDumper::visit(const AstNodeDoubleStar &node) {
    if (include_pos_) {
        result_ = json{
            {"type", "DoubleStar"},
            {"pos", pos_to_json(node.pos_)},
            {"operand", dump_node(*node.operand_)}
        };
        return;
    }
    result_ = json{{"type", "DoubleStar"}, {"operand", dump_node(*node.operand_)}};
}

void AstJsonDumper::visit(const AstNodeOpUnary &node) {
    if (include_pos_) {
        result_ = json{
            {"type", "OpUnary"},
            {"pos", pos_to_json(node.pos_)},
            {"op", op_str(node.op_)},
            {"operand", dump_node(*node.operand_)},
            {"pos_op", pos_to_json(node.pos_op_)}
        };
        return;
    }
    result_ =
        json{{"type", "OpUnary"}, {"op", op_str(node.op_)}, {"operand", dump_node(*node.operand_)}};
}

void AstJsonDumper::visit(const AstNodeOpBinary &node) {
    if (include_pos_) {
        result_ = json{
            {"type", "OpBinary"},
            {"pos", pos_to_json(node.pos_)},
            {"op", op_str(node.op_)},
            {"left", dump_node(*node.left_)},
            {"right", dump_node(*node.right_)},
            {"pos_op", pos_to_json(node.pos_op_)}
        };
        return;
    }
    result_ = json{
        {"type", "OpBinary"},
        {"op", op_str(node.op_)},
        {"left", dump_node(*node.left_)},
        {"right", dump_node(*node.right_)}
    };
}

void AstJsonDumper::visit(const AstNodeCompare &node) {
    auto ops = json::array();
    for (const auto &op : node.ops_) ops.push_back(op_str(op));
    auto operands = dump_nodes(node.operands_);

    if (include_pos_) {
        result_ = json{
            {"type", "Compare"},
            {"pos", pos_to_json(node.pos_)},
            {"ops", std::move(ops)},
            {"operands", std::move(operands)},
            {"positions_op", positions_to_json(node.positions_op_)}
        };
        return;
    }
    result_ = json{{"type", "Compare"}, {"ops", std::move(ops)}, {"operands", std::move(operands)}};
}

void AstJsonDumper::visit(const AstNodeIs &node) {
    auto operands = dump_nodes(node.operands_);

    if (include_pos_) {
        result_ = json{
            {"type", "Is"},
            {"pos", pos_to_json(node.pos_)},
            {"operands", std::move(operands)},
            {"positions_op", positions_to_json(node.positions_op_)}
        };
        return;
    }
    result_ = json{{"type", "Is"}, {"operands", std::move(operands)}};
}

void AstJsonDumper::visit(const AstNodeAssign &node) {
    if (include_pos_) {
        result_ = json{
            {"type", "Assign"},
            {"pos", pos_to_json(node.pos_)},
            {"target", dump_node(*node.target_)},
            {"value", dump_node(*node.value_)}
        };
        return;
    }
    result_ = json{
        {"type", "Assign"}, {"target", dump_node(*node.target_)}, {"value", dump_node(*node.value_)}
    };
}

void AstJsonDumper::visit(const AstNodeCompoundAssign &node) {
    if (include_pos_) {
        result_ = json{
            {"type", "CompoundAssign"},
            {"pos", pos_to_json(node.pos_)},
            {"target", dump_node(*node.target_)},
            {"op", op_str(node.op_)},
            {"value", dump_node(*node.value_)},
            {"pos_op", pos_to_json(node.pos_op_)}
        };
        return;
    }
    result_ = json{
        {"type", "CompoundAssign"},
        {"target", dump_node(*node.target_)},
        {"op", op_str(node.op_)},
        {"value", dump_node(*node.value_)}
    };
}

void AstJsonDumper::visit(const AstNodeCall &node) {
    auto args_json = dump_call_args(node.args_);
    if (include_pos_) {
        result_ = json{
            {"type", "Call"},
            {"pos", pos_to_json(node.pos_)},
            {"object", dump_node(*node.object_)},
            {"positional_args", args_json["positional_args"]},
            {"keyword_args", args_json["keyword_args"]},
            {"pos_paren", args_json["pos_paren"]}
        };
        return;
    }
    result_ = json{
        {"type", "Call"},
        {"object", dump_node(*node.object_)},
        {"positional_args", args_json["positional_args"]},
        {"keyword_args", args_json["keyword_args"]}
    };
}

void AstJsonDumper::visit(const AstNodeIndex &node) {
    auto args = dump_nodes(node.args_);

    if (include_pos_) {
        result_ = json{
            {"type", "Index"},
            {"pos", pos_to_json(node.pos_)},
            {"object", dump_node(*node.object_)},
            {"args", std::move(args)},
            {"pos_bracket", pos_to_json(node.pos_bracket_)}
        };
        return;
    }
    result_ =
        json{{"type", "Index"}, {"object", dump_node(*node.object_)}, {"args", std::move(args)}};
}

void AstJsonDumper::visit(const AstNodeAttr &node) {
    if (include_pos_) {
        result_ = json{
            {"type", "Attr"},
            {"pos", pos_to_json(node.pos_)},
            {"object", dump_node(*node.object_)},
            {"attr", u32_to_utf8(node.attr_)},
            {"pos_dot", pos_to_json(node.pos_dot_)}
        };
        return;
    }
    result_ = json{
        {"type", "Attr"}, {"object", dump_node(*node.object_)}, {"attr", u32_to_utf8(node.attr_)}
    };
}

void AstJsonDumper::visit(const AstNodeIdentifier &node) {
    if (include_pos_) {
        result_ = json{
            {"type", "Identifier"},
            {"pos", pos_to_json(node.pos_)},
            {"identifier", u32_to_utf8(node.identifier_)}
        };
        return;
    }
    result_ = json{{"type", "Identifier"}, {"identifier", u32_to_utf8(node.identifier_)}};
}

void AstJsonDumper::visit(const AstNodeDel &node) {
    if (include_pos_) {
        result_ = json{
            {"type", "Del"}, {"pos", pos_to_json(node.pos_)}, {"target", dump_node(*node.target_)}
        };
        return;
    }
    result_ = json{{"type", "Del"}, {"target", dump_node(*node.target_)}};
}

void AstJsonDumper::visit(const AstNodeGlobal &node) {
    if (include_pos_) {
        result_ = json{
            {"type", "Global"},
            {"pos", pos_to_json(node.pos_)},
            {"identifier", u32_to_utf8(node.identifier_)}
        };
        return;
    }
    result_ = json{{"type", "Global"}, {"identifier", u32_to_utf8(node.identifier_)}};
}

json AstJsonDumper::dump(const AstNode &node, const bool include_pos) {
    AstJsonDumper dumper{include_pos};
    return dumper.dump_node(node);
}
