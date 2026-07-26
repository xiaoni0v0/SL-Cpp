#include "StaticEvaler.h"

#include "../../utils/string_utils.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

AstNodePtr StaticEvaler::fold_unary(AstNodeOpUnary &node) {
    using enum AstNodeOpUnary::OpType;

    switch (node.op_) {
    case Not:
        return fold_not(node);
    case Pos:
    case Neg:
        return fold_arithmetic(node);
    case BitInvert:
        return fold_bitwise(node);
    default:
        return nullptr;
    }
}

AstNodePtr StaticEvaler::fold_binary(AstNodeOpBinary &node) {
    using enum AstNodeOpBinary::OpType;

    switch (node.op_) {
    case Add:
        return fold_add(node);
    case Mul:
        return fold_mul(node);
    case Sub:
    case Div:
    case DivFloor:
    case Mod:
    case Pow:
        return fold_arithmetic(node);
    case BitAnd:
    case BitOr:
    case BitXor:
    case LShift:
    case RShift:
        return fold_bitwise(node);
    case And:
    case Or:
        return fold_and_or(node);
    default:
        return nullptr;
    }
}
AstNodePtr StaticEvaler::fold_compare(AstNodeCompare &node) {
    using enum AstNodeCompare::OpType;

    for (const auto &operand : node.operands_)
        if (!is_literal_pure(*operand)) return nullptr;

    for (size_t i{0}; i < node.ops_.size(); ++i) {
        const AstNode &a{*node.operands_[i]};
        const AstNode &b{*node.operands_[i + 1]};
        bool result;

        if (node.ops_[i] == Eq || node.ops_[i] == Ne) {
            const bool eq{literal_equal(a, b)};
            result = node.ops_[i] == Eq ? eq : !eq;
        } else {
            const CmpResult cmp{literal_compare(a, b)};
            if (cmp == CmpResult::Unordered)
                return nullptr; // 类型不支持比较，交给运行时报 TypeError
            switch (node.ops_[i]) {
            case Lt:
                result = cmp == CmpResult::Less;
                break;
            case Le:
                result = cmp != CmpResult::Greater;
                break;
            case Gt:
                result = cmp == CmpResult::Greater;
                break;
            case Ge:
                result = cmp != CmpResult::Less;
                break;
            default:
                return nullptr; // 不可达（Eq/Ne 已经在上面处理）
            }
        }

        // 链式比较：一旦某一环为假，整条链短路，值就是这一环的结果（恒为 False）
        if (!result) return make_bool(node.pos_, false);
    }
    return make_bool(node.pos_, true);
}
AstNodePtr StaticEvaler::fold_if(AstNodeIf &node) {
    size_t i{0};
    while (i < node.clauses_.size() && is_literal_pure(*node.clauses_[i].cond_) &&
           !truthy(*node.clauses_[i].cond_)) {
        ++i;
    }

    if (i < node.clauses_.size() && is_literal_pure(*node.clauses_[i].cond_)) {
        // 循环只有在"非字面量"或者"字面量为 True"时才会停在这个位置，能到这里说明是后者
        return std::move(node.clauses_[i].body_);
    }
    if (i == 0) return nullptr; // 第一个 clause 就没法判定，什么都没能折

    if (i == node.clauses_.size()) {
        if (node.else_expr_) return std::move(node.else_expr_);
        return std::make_unique<AstNodeLiteralNone>(node.pos_);
    }

    // 跳过了至少一个确定为 False 的 clause，但后面接的是一个还不能判定的 cond：部分折叠
    std::vector<AstNodeIf::AstNodeCondAndExpr> remaining;
    for (size_t j{i}; j < node.clauses_.size(); ++j)
        remaining.push_back(std::move(node.clauses_[j]));
    return std::make_unique<AstNodeIf>(node.pos_, std::move(remaining), std::move(node.else_expr_));
}

AstNodePtr StaticEvaler::fold_for_cond(AstNodeForCond &node) {
    if (!node.cond_ || !is_literal_pure(*node.cond_) || truthy(*node.cond_)) return nullptr;

    AstNodePtr result{
        node.collect_
            ? static_cast<AstNodePtr>(
                  std::make_unique<AstNodeLiteralList>(node.pos_, std::vector<AstNodePtr>{})
              )
            : static_cast<AstNodePtr>(std::make_unique<AstNodeLiteralInt>(node.pos_, U"0"))
    };
    if (!node.init_) return result;

    std::vector<AstNodePtr> exprs;
    exprs.push_back(std::move(node.init_));
    exprs.push_back(std::move(result));
    return std::make_unique<AstNodeCompound>(node.pos_, std::move(exprs));
}

AstNodePtr StaticEvaler::fold_not(AstNodeOpUnary &node) {
    if (!is_literal_pure(*node.operand_)) return nullptr;

    return make_bool(node.pos_, !truthy(*node.operand_));
}

AstNodePtr StaticEvaler::fold_add(AstNodeOpBinary &node) {
    AstNode &l{*node.left_}, &r{*node.right_};
    if (!is_literal_pure(l) || !is_literal_pure(r)) return nullptr;

    // 数字 + 数字：交给 fold_arithmetic
    if (is_numeric(l) && is_numeric(r)) return fold_arithmetic(node);

    // 'a' + 'b'
    if (const auto *ls{dynamic_cast<const AstNodeLiteralStr *>(&l)},
        *rs{dynamic_cast<const AstNodeLiteralStr *>(&r)};
        ls && rs) {
        return std::make_unique<AstNodeLiteralStr>(node.pos_, ls->value_ + rs->value_);
    }

    // () + ()
    if (auto *lt{dynamic_cast<AstNodeLiteralTuple *>(&l)},
        *rt{dynamic_cast<AstNodeLiteralTuple *>(&r)};
        lt && rt) {
        lt->items_.reserve(lt->items_.size() + rt->items_.size());
        for (auto &item : rt->items_) lt->items_.push_back(std::move(item));
        return std::make_unique<AstNodeLiteralTuple>(node.pos_, std::move(lt->items_));
    }

    // [] + []
    if (auto *ll{dynamic_cast<AstNodeLiteralList *>(&l)},
        *rl{dynamic_cast<AstNodeLiteralList *>(&r)};
        ll && rl) {
        ll->items_.reserve(ll->items_.size() + rl->items_.size());
        for (auto &item : rl->items_) ll->items_.push_back(std::move(item));
        return std::make_unique<AstNodeLiteralList>(node.pos_, std::move(ll->items_));
    }

    return nullptr;
}

AstNodePtr StaticEvaler::fold_mul(AstNodeOpBinary &node) {
    const AstNode &l{*node.left_}, &r{*node.right_};
    if (!is_literal_pure(l) || !is_literal_pure(r)) return nullptr;

    // 数字 * 数字：交给 fold_arithmetic
    if (is_numeric(l) && is_numeric(r)) return fold_arithmetic(node);

    // 下面尝试理解为容器的重复
    if (!is_int_family(l) && !is_int_family(r)) return nullptr;
    // 确定哪个是重复次数，哪个可能是容器
    const auto &[container, count_node] = [&]() -> std::pair<const AstNode &, const AstNode &> {
        if (is_int_family(r)) return {l, r};
        return {r, l};
    }();

    // 非负 int 才有定义；数量大到 int 都装不下的，同样交给运行时
    const std::optional count{try_to_int(to_bigint(count_node))};
    if (!count || *count < 0) return nullptr;
    const size_t n{static_cast<size_t>(*count)};

    // 现在，挑出来数字 count_node，另一个 container 不知道是啥
    // 'a' * 3
    if (const auto *s{dynamic_cast<const AstNodeLiteralStr *>(&container)}) {
        std::u32string value;
        value.reserve(s->value_.size() * n);
        for (size_t i{0}; i < n; ++i) value += s->value_;
        return std::make_unique<AstNodeLiteralStr>(node.pos_, std::move(value));
    }
    // (a, b) * 3
    if (const auto *t{dynamic_cast<const AstNodeLiteralTuple *>(&container)}) {
        std::vector<AstNodePtr> items;
        items.reserve(t->items_.size() * n);
        for (size_t i{0}; i < n; ++i) {
            for (const auto &item : t->items_) {
                items.push_back(clone_literal(*item));
            }
        }
        return std::make_unique<AstNodeLiteralTuple>(node.pos_, std::move(items));
    }
    // [a, b] * 3
    if (const auto *lst{dynamic_cast<const AstNodeLiteralList *>(&container)}) {
        std::vector<AstNodePtr> items;
        items.reserve(lst->items_.size() * n);
        for (size_t i{0}; i < n; ++i) {
            for (const auto &item : lst->items_) {
                items.push_back(clone_literal(*item));
            }
        }
        return std::make_unique<AstNodeLiteralList>(node.pos_, std::move(items));
    }
    return nullptr;
}

AstNodePtr StaticEvaler::fold_arithmetic(AstNodeOpUnary &node) {
    using enum AstNodeOpUnary::OpType;
    const AstNode &operand{*node.operand_};

    if (!is_literal_pure(operand) || !is_numeric(operand)) return nullptr;

    // int
    if (is_int_family(operand)) {
        const BigInt v{to_bigint(operand)};
        return make_int(node.pos_, node.op_ == Neg ? -v : +v);
    }
    // float
    const double v{to_double(operand)};
    return make_float(node.pos_, node.op_ == Neg ? -v : v);
}

AstNodePtr StaticEvaler::fold_arithmetic(AstNodeOpBinary &node) {
    using enum AstNodeOpBinary::OpType;
    const AstNode &l{*node.left_}, &r{*node.right_};
    if (!is_literal_pure(l) || !is_literal_pure(r) || !is_numeric(l) || !is_numeric(r))
        return nullptr;

    const bool is_both_int{is_int_family(l) && is_int_family(r)};

    switch (node.op_) {
    case Add:
        if (is_both_int) return make_int(node.pos_, to_bigint(l) + to_bigint(r));
        return make_float(node.pos_, to_double(l) + to_double(r));

    case Sub:
        if (is_both_int) return make_int(node.pos_, to_bigint(l) - to_bigint(r));
        return make_float(node.pos_, to_double(l) - to_double(r));

    case Mul:
        if (is_both_int) return make_int(node.pos_, to_bigint(l) * to_bigint(r));
        return make_float(node.pos_, to_double(l) * to_double(r));

    case Div: {
        const double rv{to_double(r)};
        if (rv == 0.0) return nullptr; // MathError，交给运行时
        return make_float(node.pos_, to_double(l) / rv);
    }

    case DivFloor: {
        if (is_both_int) {
            const BigInt rv{to_bigint(r)};
            if (rv.is_zero()) return nullptr;
            return make_int(node.pos_, to_bigint(l).floor_div(rv));
        }
        const double rv{to_double(r)};
        if (rv == 0.0) return nullptr;
        return make_float(node.pos_, std::floor(to_double(l) / rv));
    }

    case Mod: {
        if (is_both_int) {
            const BigInt rv{to_bigint(r)};
            if (rv.is_zero()) return nullptr;
            return make_int(node.pos_, to_bigint(l).mod(rv));
        }
        const double rv{to_double(r)};
        if (rv == 0.0) return nullptr;
        double m{std::fmod(to_double(l), rv)};
        if (m != 0.0 && (m < 0.0) != (rv < 0.0))
            m += rv; // 向 y 的符号方向调整，与 // 满足同一恒等式
        return make_float(node.pos_, m);
    }

    case Pow:
        // 都是 int 且指数非负：结果仍是 int
        if (is_both_int && !to_bigint(r).is_negative())
            return make_int(node.pos_, to_bigint(l).pow(to_bigint(r)));
        // 否则一律走 float 幂
        return make_float(node.pos_, std::pow(to_double(l), to_double(r)));

    default:
        return nullptr;
    }
}

AstNodePtr StaticEvaler::fold_bitwise(AstNodeOpUnary &node) {
    const AstNode &operand{*node.operand_};
    if (!is_literal_pure(operand) || !is_int_family(operand)) return nullptr;

    return make_int(node.pos_, ~to_bigint(operand));
}

AstNodePtr StaticEvaler::fold_bitwise(AstNodeOpBinary &node) {
    using enum AstNodeOpBinary::OpType;
    const AstNode &l{*node.left_}, &r{*node.right_};
    if (!is_literal_pure(l) || !is_literal_pure(r) || !is_int_family(l) || !is_int_family(r))
        return nullptr;

    const BigInt lv{to_bigint(l)}, rv{to_bigint(r)};
    switch (node.op_) {
    case BitAnd:
        return make_int(node.pos_, lv & rv);
    case BitOr:
        return make_int(node.pos_, lv | rv);
    case BitXor:
        return make_int(node.pos_, lv ^ rv);
    case LShift:
    case RShift: {
        const std::optional<long long> shift{try_to_int(rv)};
        if (!shift || *shift < 0) return nullptr; // 负数移位交给运行时报错
        return make_int(node.pos_, node.op_ == LShift ? lv << *shift : lv >> *shift);
    }
    default:
        return nullptr;
    }
}

AstNodePtr StaticEvaler::fold_and_or(AstNodeOpBinary &node) {
    using enum AstNodeOpBinary::OpType;
    if (!is_literal_pure(*node.left_)) return nullptr;

    const bool left_truthy{truthy(*node.left_)};
    const bool take_left{node.op_ == And ? !left_truthy : left_truthy};
    return std::move(take_left ? node.left_ : node.right_);
}

bool StaticEvaler::truthy(const AstNode &literal) {
    if (dynamic_cast<const AstNodeLiteralNone *>(&literal)) return false;
    if (const auto *b{dynamic_cast<const AstNodeLiteralBool *>(&literal)}) return b->value_;
    if (const auto *i{dynamic_cast<const AstNodeLiteralInt *>(&literal)})
        return !to_bigint(*i).is_zero();
    if (const auto *f{dynamic_cast<const AstNodeLiteralFloat *>(&literal)})
        return to_double(*f) != 0.0;
    if (const auto *s{dynamic_cast<const AstNodeLiteralStr *>(&literal)}) return !s->value_.empty();
    if (const auto *t{dynamic_cast<const AstNodeLiteralTuple *>(&literal)})
        return !t->items_.empty();
    if (const auto *l{dynamic_cast<const AstNodeLiteralList *>(&literal)})
        return !l->items_.empty();
    return true; // 其他均为 True
}

bool StaticEvaler::is_literal_pure(const AstNode &node) {
    // 天然满足的
    if (dynamic_cast<const AstNodeLiteralNone *>(&node)) return true;
    if (dynamic_cast<const AstNodeLiteralBool *>(&node)) return true;
    if (dynamic_cast<const AstNodeLiteralInt *>(&node)) return true;
    if (dynamic_cast<const AstNodeLiteralFloat *>(&node)) return true;
    if (dynamic_cast<const AstNodeLiteralStr *>(&node)) return true;
    if (dynamic_cast<const AstNodeLiteralEllipsis *>(&node)) return true;

    // 容器类的递归判断
    if (const auto *t{dynamic_cast<const AstNodeLiteralTuple *>(&node)})
        return std::ranges::all_of(t->items_, [](const AstNodePtr &item) {
            return is_literal_pure(*item);
        });
    if (const auto *l{dynamic_cast<const AstNodeLiteralList *>(&node)})
        return std::ranges::all_of(l->items_, [](const AstNodePtr &item) {
            return is_literal_pure(*item);
        });
    return false; // dict、_G/_L、标识符等都不是
}

bool StaticEvaler::is_int_family(const AstNode &node) {
    return dynamic_cast<const AstNodeLiteralBool *>(&node) ||
           dynamic_cast<const AstNodeLiteralInt *>(&node);
}

bool StaticEvaler::is_numeric(const AstNode &node) {
    return is_int_family(node) || dynamic_cast<const AstNodeLiteralFloat *>(&node);
}

BigInt StaticEvaler::to_bigint(const AstNode &node) {
    if (const auto *b{dynamic_cast<const AstNodeLiteralBool *>(&node)})
        return BigInt{b->value_ ? 1LL : 0LL};
    const auto &i{dynamic_cast<const AstNodeLiteralInt &>(node)}; // 调用方保证 is_int_family(node)
    return BigInt::from_decimal_string(u32_to_utf8(i.raw_));
}

double StaticEvaler::to_double(const AstNode &node) {
    if (is_int_family(node)) return to_bigint(node).to_double();
    const auto &f{dynamic_cast<const AstNodeLiteralFloat &>(node)}; // 调用方保证 is_numeric(node)
    return std::strtod(u32_to_utf8(f.raw_).c_str(), nullptr);
}

std::optional<int> StaticEvaler::try_to_int(const BigInt &value) {
    try {
        return std::stoi(value.to_decimal_string());
    } catch (const std::exception &) {
        return std::nullopt; // 装不下 int
    }
}

AstNodePtr StaticEvaler::make_bool(const Position pos, const bool value) {
    return std::make_unique<AstNodeLiteralBool>(pos, value);
}

AstNodePtr StaticEvaler::make_int(const Position pos, const BigInt &value) {
    return std::make_unique<AstNodeLiteralInt>(pos, utf8_to_u32(value.to_decimal_string()));
}

AstNodePtr StaticEvaler::make_float(const Position pos, const double value) {
    if (!std::isfinite(value)) return nullptr; // ±inf/NaN 写不出合法的 float 字面量，交给运行时处理
    return std::make_unique<AstNodeLiteralFloat>(pos, utf8_to_u32(format_double(value)));
}

std::string StaticEvaler::format_double(const double value) {
    for (int prec{0}; prec <= 17; ++prec) {
        const int needed{std::snprintf(nullptr, 0, "%.*f", prec, value)};
        std::string s(static_cast<size_t>(needed), '\0');
        std::snprintf(s.data(), s.size() + 1, "%.*f", prec, value);
        if (std::strtod(s.c_str(), nullptr) == value) {
            if (s.find('.') == std::string::npos)
                s += ".0"; // SL float 字面量语法要求小数点不可省略
            return s;
        }
    }
    // IEEE754 double 十进制有效位数不超过 17 位，理论上走不到这里；留一个兜底避免万一
    const int needed{std::snprintf(nullptr, 0, "%.17f", value)};
    std::string s(static_cast<size_t>(needed), '\0');
    std::snprintf(s.data(), s.size() + 1, "%.17f", value);
    return s;
}

AstNodePtr StaticEvaler::clone_literal(const AstNode &node) {
    if (dynamic_cast<const AstNodeLiteralNone *>(&node))
        return std::make_unique<AstNodeLiteralNone>(node.pos_);
    if (const auto *b{dynamic_cast<const AstNodeLiteralBool *>(&node)})
        return std::make_unique<AstNodeLiteralBool>(node.pos_, b->value_);
    if (const auto *i{dynamic_cast<const AstNodeLiteralInt *>(&node)})
        return std::make_unique<AstNodeLiteralInt>(node.pos_, i->raw_);
    if (const auto *f{dynamic_cast<const AstNodeLiteralFloat *>(&node)})
        return std::make_unique<AstNodeLiteralFloat>(node.pos_, f->raw_);
    if (const auto *s{dynamic_cast<const AstNodeLiteralStr *>(&node)})
        return std::make_unique<AstNodeLiteralStr>(node.pos_, s->value_);
    if (dynamic_cast<const AstNodeLiteralEllipsis *>(&node))
        return std::make_unique<AstNodeLiteralEllipsis>(node.pos_);
    // 调用方保证 is_literal(node)，排除以上分支后只剩 tuple/list（dict 不在 is_literal 认可范围内）
    if (const auto *t{dynamic_cast<const AstNodeLiteralTuple *>(&node)}) {
        std::vector<AstNodePtr> items;
        items.reserve(t->items_.size());
        for (const auto &item : t->items_) items.push_back(clone_literal(*item));
        return std::make_unique<AstNodeLiteralTuple>(node.pos_, std::move(items));
    }
    const auto &l{dynamic_cast<const AstNodeLiteralList &>(node)};
    std::vector<AstNodePtr> items;
    items.reserve(l.items_.size());
    for (const auto &item : l.items_) items.push_back(clone_literal(*item));
    return std::make_unique<AstNodeLiteralList>(node.pos_, std::move(items));
}

bool StaticEvaler::literal_equal(const AstNode &a, const AstNode &b) {
    if (is_numeric(a) && is_numeric(b)) {
        if (is_int_family(a) && is_int_family(b)) return to_bigint(a) == to_bigint(b);
        return to_double(a) == to_double(b);
    }
    if (dynamic_cast<const AstNodeLiteralNone *>(&a))
        return dynamic_cast<const AstNodeLiteralNone *>(&b) != nullptr;
    if (dynamic_cast<const AstNodeLiteralEllipsis *>(&a))
        return dynamic_cast<const AstNodeLiteralEllipsis *>(&b) != nullptr;
    if (const auto *sa{dynamic_cast<const AstNodeLiteralStr *>(&a)}) {
        const auto *sb{dynamic_cast<const AstNodeLiteralStr *>(&b)};
        return sb && sa->value_ == sb->value_;
    }
    if (const auto *ta{dynamic_cast<const AstNodeLiteralTuple *>(&a)}) {
        const auto *tb{dynamic_cast<const AstNodeLiteralTuple *>(&b)};
        if (!tb || ta->items_.size() != tb->items_.size()) return false;
        for (size_t i{0}; i < ta->items_.size(); ++i)
            if (!literal_equal(*ta->items_[i], *tb->items_[i])) return false;
        return true;
    }
    if (const auto *la{dynamic_cast<const AstNodeLiteralList *>(&a)}) {
        const auto *lb{dynamic_cast<const AstNodeLiteralList *>(&b)};
        if (!lb || la->items_.size() != lb->items_.size()) return false;
        for (size_t i{0}; i < la->items_.size(); ++i)
            if (!literal_equal(*la->items_[i], *lb->items_[i])) return false;
        return true;
    }
    return false; // 剩下的（bool 已经被数字分支吃掉）不同类型之间一律不相等
}

StaticEvaler::CmpResult StaticEvaler::literal_compare(const AstNode &a, const AstNode &b) {
    if (is_numeric(a) && is_numeric(b)) {
        if (is_int_family(a) && is_int_family(b)) {
            const std::strong_ordering cmp{to_bigint(a) <=> to_bigint(b)};
            return cmp < 0 ? CmpResult::Less : cmp > 0 ? CmpResult::Greater : CmpResult::Equal;
        }
        const double da{to_double(a)};
        const double db{to_double(b)};
        return da < db ? CmpResult::Less : da > db ? CmpResult::Greater : CmpResult::Equal;
    }
    if (const auto *sa{dynamic_cast<const AstNodeLiteralStr *>(&a)}) {
        const auto *sb{dynamic_cast<const AstNodeLiteralStr *>(&b)};
        if (!sb) return CmpResult::Unordered;
        return sa->value_ < sb->value_   ? CmpResult::Less
               : sb->value_ < sa->value_ ? CmpResult::Greater
                                         : CmpResult::Equal;
    }

    // tuple/tuple、list/list 逐元素比较，第一个不相等的元素决定结果；一方是另一方的前缀则前缀更小
    const auto lexicographic{
        [](const std::vector<AstNodePtr> &xa, const std::vector<AstNodePtr> &xb) -> CmpResult {
            const size_t n{std::min(xa.size(), xb.size())};
            for (size_t i{0}; i < n; ++i) {
                const CmpResult c{literal_compare(*xa[i], *xb[i])};
                if (c != CmpResult::Equal) return c;
            }
            return xa.size() < xb.size()   ? CmpResult::Less
                   : xa.size() > xb.size() ? CmpResult::Greater
                                           : CmpResult::Equal;
        }
    };
    if (const auto *ta{dynamic_cast<const AstNodeLiteralTuple *>(&a)}) {
        const auto *tb{dynamic_cast<const AstNodeLiteralTuple *>(&b)};
        return tb ? lexicographic(ta->items_, tb->items_) : CmpResult::Unordered;
    }
    if (const auto *la{dynamic_cast<const AstNodeLiteralList *>(&a)}) {
        const auto *lb{dynamic_cast<const AstNodeLiteralList *>(&b)};
        return lb ? lexicographic(la->items_, lb->items_) : CmpResult::Unordered;
    }
    return CmpResult::Unordered; // None/dict/Ellipsis 均不支持大小比较
}

AstNodePtr StaticEvaler::fold(AstNode &node) {
    // 只有这几种节点才可能整体收缩成一个字面量
    if (auto *n{dynamic_cast<AstNodeOpUnary *>(&node)}) return fold_unary(*n);
    if (auto *n{dynamic_cast<AstNodeOpBinary *>(&node)}) return fold_binary(*n);
    if (auto *n{dynamic_cast<AstNodeCompare *>(&node)}) return fold_compare(*n);
    if (auto *n{dynamic_cast<AstNodeIf *>(&node)}) return fold_if(*n);
    if (auto *n{dynamic_cast<AstNodeForCond *>(&node)}) return fold_for_cond(*n);
    return nullptr;
}
