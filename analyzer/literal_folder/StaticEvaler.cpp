#include "StaticEvaler.h"

#include "../../utils/string_utils.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

AstNodePtr StaticEvaler::fold(AstNode &node) {
    // 只有这四种"运算符"节点才可能整体收缩成一个字面量；其余任何节点类型（含已经是字面量、
    // 容器字面量、标识符、调用……）都原样返回 nullptr，交给调用方保持原样不动
    if (auto *n{dynamic_cast<AstNodeOpUnary *>(&node)}) return fold_unary(*n);
    if (auto *n{dynamic_cast<AstNodeOpBinary *>(&node)}) return fold_binary(*n);
    if (auto *n{dynamic_cast<AstNodeCompare *>(&node)}) return fold_compare(*n);
    if (auto *n{dynamic_cast<AstNodeIs *>(&node)}) return fold_is(*n);
    return nullptr;
}

// ============================================================
// 一元运算符
// ============================================================

AstNodePtr StaticEvaler::fold_unary(const AstNodeOpUnary &node) {
    using OpType = AstNodeOpUnary::OpType;
    switch (node.op_) {
    case OpType::Not: return fold_not(node);
    case OpType::Pos:
    case OpType::Neg:
    case OpType::BitNot: return fold_pos_neg_bitnot(node);
    case OpType::Question:
    case OpType::Exclaim:
        // x?/x! 是类型运算符（生成复合类型，见 SL.md 3.4.2），不作用于值，不在字面量折叠的范围内
        return nullptr;
    default: return nullptr;
    }
}

AstNodePtr StaticEvaler::fold_not(const AstNodeOpUnary &node) {
    if (!is_literal(*node.operand_)) return nullptr;
    return make_bool(node.pos_, !truthy(*node.operand_));
}

AstNodePtr StaticEvaler::fold_pos_neg_bitnot(const AstNodeOpUnary &node) {
    using OpType = AstNodeOpUnary::OpType;
    const AstNode &operand{*node.operand_};
    if (!is_literal(operand)) return nullptr;

    if (node.op_ == OpType::BitNot) {
        if (!is_int_family(operand)) return nullptr;
        return make_int(node.pos_, ~to_bigint(operand));
    }

    if (!is_numeric(operand)) return nullptr;
    // +/- 对 bool 也生效，但结果永远提升成 int（同 Python：+True == 1，类型是 int 不是 bool）
    if (is_int_family(operand)) {
        const BigInt v{to_bigint(operand)};
        return make_int(node.pos_, node.op_ == OpType::Neg ? -v : +v);
    }
    const double v{to_double(operand)};
    return make_float(node.pos_, node.op_ == OpType::Neg ? -v : v);
}

// ============================================================
// 二元运算符
// ============================================================

AstNodePtr StaticEvaler::fold_binary(AstNodeOpBinary &node) {
    using OpType = AstNodeOpBinary::OpType;
    switch (node.op_) {
    case OpType::Add: return fold_add(node);
    case OpType::Mul: return fold_mul(node);
    case OpType::Mod: return fold_mod(node);
    case OpType::Sub:
    case OpType::Div:
    case OpType::DivFloor:
    case OpType::Pow: return fold_arithmetic(node);
    case OpType::BitOr: return fold_bitor(node);
    case OpType::BitAnd:
    case OpType::BitXor:
    case OpType::LShift:
    case OpType::RShift: return fold_bitwise(node);
    case OpType::And:
    case OpType::Or: return fold_and_or(node);
    case OpType::Range:
        // range 对象不属于 None/bool/int/float/str/tuple/list/dict 这套字面量类型，不折
        return nullptr;
    default: return nullptr;
    }
}

// - / // % **（数字分支）—— + 和 * 的数字分支各自在 fold_add/fold_mul 里处理，不在这里
AstNodePtr StaticEvaler::fold_arithmetic(const AstNodeOpBinary &node) {
    using OpType = AstNodeOpBinary::OpType;
    const AstNode &l{*node.left_};
    const AstNode &r{*node.right_};
    if (!is_literal(l) || !is_literal(r) || !is_numeric(l) || !is_numeric(r)) return nullptr;

    const bool int_int{is_int_family(l) && is_int_family(r)};

    switch (node.op_) {
    case OpType::Sub: if (int_int) return make_int(node.pos_, to_bigint(l) - to_bigint(r));
        return make_float(node.pos_, to_double(l) - to_double(r));

    case OpType::Div: {
        const double rv{to_double(r)};
        if (rv == 0.0) return nullptr; // MathError，交给运行时
        return make_float(node.pos_, to_double(l) / rv);
    }

    case OpType::DivFloor: if (int_int) {
            const BigInt rv{to_bigint(r)};
            if (rv.is_zero()) return nullptr;
            return make_int(node.pos_, to_bigint(l).floor_div(rv));
        } else {
            const double rv{to_double(r)};
            if (rv == 0.0) return nullptr;
            return make_float(node.pos_, std::floor(to_double(l) / rv));
        }

    case OpType::Mod:
        // 走到这里说明 fold_mod 已经排除了 str % ... 的情况，这里只处理数字取模
        if (int_int) {
            const BigInt rv{to_bigint(r)};
            if (rv.is_zero()) return nullptr;
            return make_int(node.pos_, to_bigint(l).mod(rv));
        } else {
            const double rv{to_double(r)};
            if (rv == 0.0) return nullptr;
            double m{std::fmod(to_double(l), rv)};
            if (m != 0.0 && (m < 0.0) != (rv < 0.0)) m += rv; // 向 y 的符号方向调整，与 // 满足同一恒等式
            return make_float(node.pos_, m);
        }

    case OpType::Pow:
        // 都是 int 且指数非负：结果仍是 int；否则（含负指数、掺了 float）一律走 float 幂
        if (int_int && !to_bigint(r).is_negative()) return make_int(node.pos_, to_bigint(l).pow(to_bigint(r)));
        // 结果不是实数（如负数开偶次方根）或溢出成 ±inf，make_float 会因为不是有限数而返回 nullptr，
        // 交给运行时报 MathError，这里不用单独判断
        return make_float(node.pos_, std::pow(to_double(l), to_double(r)));

    default: return nullptr;
    }
}

// + ：数值相加，或 str/tuple/list 各自的拼接（SL.md 3.4.2："对于均为字符串、元组、列表，返回拼接"）
AstNodePtr StaticEvaler::fold_add(const AstNodeOpBinary &node) {
    const AstNode &l{*node.left_};
    const AstNode &r{*node.right_};
    if (!is_literal(l) || !is_literal(r)) return nullptr;

    if (is_numeric(l) && is_numeric(r)) {
        if (is_int_family(l) && is_int_family(r)) return make_int(node.pos_, to_bigint(l) + to_bigint(r));
        return make_float(node.pos_, to_double(l) + to_double(r));
    }

    if (const auto *ls{dynamic_cast<const AstNodeLiteralStr *>(&l)}) {
        const auto *rs{dynamic_cast<const AstNodeLiteralStr *>(&r)};
        if (!rs) return nullptr;
        return std::make_unique<AstNodeLiteralStr>(node.pos_, ls->value_ + rs->value_);
    }
    if (const auto *lt{dynamic_cast<const AstNodeLiteralTuple *>(&l)}) {
        const auto *rt{dynamic_cast<const AstNodeLiteralTuple *>(&r)};
        if (!rt) return nullptr;
        std::vector<AstNodePtr> items;
        items.reserve(lt->items_.size() + rt->items_.size());
        for (const auto &item : lt->items_) items.push_back(clone_literal(*item));
        for (const auto &item : rt->items_) items.push_back(clone_literal(*item));
        return std::make_unique<AstNodeLiteralTuple>(node.pos_, std::move(items));
    }
    if (const auto *ll{dynamic_cast<const AstNodeLiteralList *>(&l)}) {
        const auto *rl{dynamic_cast<const AstNodeLiteralList *>(&r)};
        if (!rl) return nullptr;
        std::vector<AstNodePtr> items;
        items.reserve(ll->items_.size() + rl->items_.size());
        for (const auto &item : ll->items_) items.push_back(clone_literal(*item));
        for (const auto &item : rl->items_) items.push_back(clone_literal(*item));
        return std::make_unique<AstNodeLiteralList>(node.pos_, std::move(items));
    }
    return nullptr; // None/bool/dict/... 之间不支持 +
}

// * ：数值相乘，或 (str/tuple/list, 非负 int) 的重复（两侧顺序不限）
AstNodePtr StaticEvaler::fold_mul(const AstNodeOpBinary &node) {
    const AstNode &l{*node.left_};
    const AstNode &r{*node.right_};
    if (!is_literal(l) || !is_literal(r)) return nullptr;

    if (is_numeric(l) && is_numeric(r)) {
        if (is_int_family(l) && is_int_family(r)) return make_int(node.pos_, to_bigint(l) * to_bigint(r));
        return make_float(node.pos_, to_double(l) * to_double(r));
    }

    const AstNode *container{nullptr};
    const AstNode *count_node{nullptr};
    if (is_int_family(r)) {
        container = &l;
        count_node = &r;
    } else if (is_int_family(l)) {
        container = &r;
        count_node = &l;
    } else return nullptr;

    // 非负 int 才有定义（SL.md 3.4.2），负数交给运行时报错；数量大到 long long 都装不下的，
    // 大概率本来就没法在编译期材料化出来，同样交给运行时
    long long count{};
    if (!try_to_ll(to_bigint(*count_node), count) || count < 0) return nullptr;
    const auto n{static_cast<size_t>(count)};

    if (const auto *s{dynamic_cast<const AstNodeLiteralStr *>(container)}) {
        std::u32string result;
        result.reserve(s->value_.size() * n);
        for (size_t i{0}; i < n; ++i) result += s->value_;
        return std::make_unique<AstNodeLiteralStr>(node.pos_, std::move(result));
    }
    if (const auto *t{dynamic_cast<const AstNodeLiteralTuple *>(container)}) {
        std::vector<AstNodePtr> items;
        items.reserve(t->items_.size() * n);
        for (size_t i{0}; i < n; ++i) for (const auto &item : t->items_) items.push_back(clone_literal(*item));
        return std::make_unique<AstNodeLiteralTuple>(node.pos_, std::move(items));
    }
    if (const auto *lst{dynamic_cast<const AstNodeLiteralList *>(container)}) {
        std::vector<AstNodePtr> items;
        items.reserve(lst->items_.size() * n);
        for (size_t i{0}; i < n; ++i) for (const auto &item : lst->items_) items.push_back(clone_literal(*item));
        return std::make_unique<AstNodeLiteralList>(node.pos_, std::move(items));
    }
    return nullptr;
}

AstNodePtr StaticEvaler::fold_mod(const AstNodeOpBinary &node) {
    if (dynamic_cast<const AstNodeLiteralStr *>(node.left_.get())) return fold_str_format(node);
    return fold_arithmetic(node);
}

// str % ...：Python 风格 printf 子集，语法见 SL.md 3.4.2（%s/%d/%f/%%，可选 0 前缀与宽度、
// %f 可选精度），不追求跟 Python 100% 一致（不支持 %(name)s 具名替换、不支持 %r 等）
AstNodePtr StaticEvaler::fold_str_format(const AstNodeOpBinary &node) {
    const auto &fmt_node{dynamic_cast<const AstNodeLiteralStr &>(*node.left_)};
    const AstNode &rhs{*node.right_};
    if (!is_literal(rhs)) return nullptr;

    std::vector<const AstNode *> args;
    if (const auto *t{dynamic_cast<const AstNodeLiteralTuple *>(&rhs)}) {
        for (const auto &item : t->items_) args.push_back(item.get());
    } else {
        args.push_back(&rhs); // 单个非 tuple 的值当成只有一个替换参数，同 Python
    }

    const std::u32string &fmt{fmt_node.value_};
    std::u32string result;
    size_t arg_i{0};

    for (size_t i{0}; i < fmt.size(); ++i) {
        if (fmt[i] != U'%') {
            result += fmt[i];
            continue;
        }
        if (++i >= fmt.size()) return nullptr; // 悬空的 %，格式串本身非法，交给运行时报错
        if (fmt[i] == U'%') {
            result += U'%';
            continue;
        }

        bool zero_pad{false};
        if (fmt[i] == U'0') {
            zero_pad = true;
            ++i;
        }
        size_t width{0};
        while (i < fmt.size() && fmt[i] >= U'0' && fmt[i] <= U'9') {
            width = width * 10 + (fmt[i] - U'0');
            ++i;
        }
        size_t precision{6}; // %f 默认 6 位小数，同 printf/Python
        bool has_precision{false};
        if (i < fmt.size() && fmt[i] == U'.') {
            ++i;
            precision = 0;
            has_precision = true;
            while (i < fmt.size() && fmt[i] >= U'0' && fmt[i] <= U'9') {
                precision = precision * 10 + (fmt[i] - U'0');
                ++i;
            }
        }
        if (i >= fmt.size() || arg_i >= args.size()) return nullptr;

        const AstNode &arg{*args[arg_i++]};
        std::u32string piece;
        switch (fmt[i]) {
        case U's': {
            const std::optional<std::u32string> disp{to_display_string(arg)};
            if (!disp) return nullptr;
            piece = *disp;
            if (has_precision && piece.size() > precision) piece.resize(precision);
            break;
        }
        case U'd': if (!is_int_family(arg)) return nullptr;
            piece = utf8_to_u32(to_bigint(arg).to_decimal_string());
            break;
        case U'f': {
            if (!is_numeric(arg)) return nullptr;
            char buf[512];
            const int written{std::snprintf(buf, sizeof buf, "%.*f", static_cast<int>(precision), to_double(arg))};
            if (written < 0 || static_cast<size_t>(written) >= sizeof buf) return nullptr;
            piece = utf8_to_u32(std::string(buf, static_cast<size_t>(written)));
            break;
        }
        default: return nullptr; // 不认识的转换字符，交给运行时报错
        }

        if (piece.size() < width) piece = std::u32string(width - piece.size(), zero_pad ? U'0' : U' ') + piece;
        result += piece;
    }
    if (arg_i != args.size()) return nullptr; // 参数没用完（Python 里是 TypeError），交给运行时

    return std::make_unique<AstNodeLiteralStr>(node.pos_, std::move(result));
}

// & ^ << >>（只对 bool/int 有意义）；| 单独走 fold_bitor（要先试 dict 合并）
AstNodePtr StaticEvaler::fold_bitwise(const AstNodeOpBinary &node) {
    using OpType = AstNodeOpBinary::OpType;
    const AstNode &l{*node.left_};
    const AstNode &r{*node.right_};
    if (!is_literal(l) || !is_literal(r) || !is_int_family(l) || !is_int_family(r)) return nullptr;

    const BigInt lv{to_bigint(l)};
    const BigInt rv{to_bigint(r)};
    switch (node.op_) {
    case OpType::BitAnd: return make_int(node.pos_, lv & rv);
    case OpType::BitOr: return make_int(node.pos_, lv | rv);
    case OpType::BitXor: return make_int(node.pos_, lv ^ rv);
    case OpType::LShift:
    case OpType::RShift: {
        long long shift{};
        if (!try_to_ll(rv, shift) || shift < 0) return nullptr; // 负数移位交给运行时报错
        return make_int(node.pos_, node.op_ == OpType::LShift ? lv << shift : lv >> shift);
    }
    default: return nullptr;
    }
}

AstNodePtr StaticEvaler::fold_bitor(const AstNodeOpBinary &node) {
    if (dynamic_cast<const AstNodeLiteralDict *>(node.left_.get())) return fold_dict_merge(node);
    return fold_bitwise(node);
}

// dict1 | dict2：合并，重复的 key 后者覆盖前者的值，但位置保留前者的位置，新 key 追加在末尾
// （同 Python 3.9+ 的 dict 合并语义，SL.md 3.4.2 新增条目）
AstNodePtr StaticEvaler::fold_dict_merge(const AstNodeOpBinary &node) {
    const auto *ld{dynamic_cast<const AstNodeLiteralDict *>(node.left_.get())};
    const auto *rd{dynamic_cast<const AstNodeLiteralDict *>(node.right_.get())};
    if (!ld || !rd || !is_literal(*ld) || !is_literal(*rd)) return nullptr;

    std::vector<std::pair<AstNodePtr, AstNodePtr>> items;
    items.reserve(ld->items_.size() + rd->items_.size());
    for (const auto &[key, val] : ld->items_) {
        const auto it{std::ranges::find_if(rd->items_,
                                           [&](const auto &kv) { return literal_equal(*key, *kv.first); })};
        if (it != rd->items_.end()) items.emplace_back(clone_literal(*key),
                                                       it->second ? clone_literal(*it->second) : nullptr);
        else items.emplace_back(clone_literal(*key), val ? clone_literal(*val) : nullptr);
    }
    for (const auto &[key, val] : rd->items_) {
        const bool already{std::ranges::any_of(ld->items_,
                                               [&](const auto &kv) { return literal_equal(*key, *kv.first); })};
        if (!already) items.emplace_back(clone_literal(*key), val ? clone_literal(*val) : nullptr);
    }
    return std::make_unique<AstNodeLiteralDict>(node.pos_, std::move(items));
}

// and/or：不短路，两个操作数各自已经在 LiteralFolder 里递归折过；只要左操作数是字面量，就知道
// 该返回左边还是右边，把它整体移到父节点位置上（见 SL.md 3.4.2、类头注释）
AstNodePtr StaticEvaler::fold_and_or(AstNodeOpBinary &node) {
    using OpType = AstNodeOpBinary::OpType;
    if (!is_literal(*node.left_)) return nullptr; // 必须知道左操作数的真值才能判断该走哪边
    const bool left_truthy{truthy(*node.left_)};
    const bool take_left{node.op_ == OpType::And ? !left_truthy : left_truthy};
    return std::move(take_left ? node.left_ : node.right_);
}

// ============================================================
// 比较 / is
// ============================================================

AstNodePtr StaticEvaler::fold_compare(const AstNodeCompare &node) {
    using OpType = AstNodeCompare::OpType;
    // 链式比较的"提前短路"优化不做（一旦某一环能确定整条链是 False 就不用再算后面的），
    // 只有整条链上所有操作数都是字面量才尝试折——这样不需要短路也能算出正确结果
    for (const auto &operand : node.operands_) if (!is_literal(*operand)) return nullptr;

    for (size_t i{0}; i < node.ops_.size(); ++i) {
        const AstNode &a{*node.operands_[i]};
        const AstNode &b{*node.operands_[i + 1]};
        bool result;

        if (node.ops_[i] == OpType::Eq || node.ops_[i] == OpType::Ne) {
            const bool eq{literal_equal(a, b)};
            result = node.ops_[i] == OpType::Eq ? eq : !eq;
        } else {
            const Cmp cmp{literal_compare(a, b)};
            if (cmp == Cmp::Unordered) return nullptr; // 类型不支持比较，交给运行时报 TypeError
            switch (node.ops_[i]) {
            case OpType::Lt: result = cmp == Cmp::Less;
                break;
            case OpType::Le: result = cmp != Cmp::Greater;
                break;
            case OpType::Gt: result = cmp == Cmp::Greater;
                break;
            case OpType::Ge: result = cmp != Cmp::Less;
                break;
            default: return nullptr; // 不可达（Eq/Ne 已经在上面处理）
            }
        }

        // 链式比较：一旦某一环为假，整条链短路，值就是这一环的结果（恒为 False）
        if (!result) return make_bool(node.pos_, false);
    }
    return make_bool(node.pos_, true);
}

AstNodePtr StaticEvaler::fold_is(const AstNodeIs &node) {
    // is 判断对象同一性，多数字面量类型在编译期折叠阶段构造出来的是不是运行时同一个对象，
    // 完全取决于 VM 的对象模型/是否 intern，这里没法安全预判；只有 None 保证是单例，
    // 其余（bool 的 True/False 是否单例、str/tuple/list/dict 会不会被 intern）一律不折
    for (const auto &operand : node.operands_) if (!dynamic_cast<const AstNodeLiteralNone *>(operand.get())) return
        nullptr;
    return make_bool(node.pos_, true); // 一条链上全是 None：None is None is ... 恒为 True
}

// ============================================================
// 真值 / 是否字面量
// ============================================================

bool StaticEvaler::truthy(const AstNode &literal) {
    if (dynamic_cast<const AstNodeLiteralNone *>(&literal)) return false;
    if (const auto *b{dynamic_cast<const AstNodeLiteralBool *>(&literal)}) return b->value_;
    if (const auto *i{dynamic_cast<const AstNodeLiteralInt *>(&literal)}) return !to_bigint(*i).is_zero();
    if (const auto *f{dynamic_cast<const AstNodeLiteralFloat *>(&literal)}) return to_double(*f) != 0.0;
    if (const auto *s{dynamic_cast<const AstNodeLiteralStr *>(&literal)}) return !s->value_.empty();
    if (const auto *t{dynamic_cast<const AstNodeLiteralTuple *>(&literal)}) return !t->items_.empty();
    if (const auto *l{dynamic_cast<const AstNodeLiteralList *>(&literal)}) return !l->items_.empty();
    if (const auto *d{dynamic_cast<const AstNodeLiteralDict *>(&literal)}) return !d->items_.empty();
    return true; // Ellipsis：SL.md 3.2 的假值列表里没有它，"其他均为 True"
}

bool StaticEvaler::is_literal(const AstNode &node) {
    if (dynamic_cast<const AstNodeLiteralNone *>(&node)) return true;
    if (dynamic_cast<const AstNodeLiteralBool *>(&node)) return true;
    if (dynamic_cast<const AstNodeLiteralInt *>(&node)) return true;
    if (dynamic_cast<const AstNodeLiteralFloat *>(&node)) return true;
    if (dynamic_cast<const AstNodeLiteralStr *>(&node)) return true;
    if (dynamic_cast<const AstNodeLiteralEllipsis *>(&node)) return true;
    if (const auto *t{dynamic_cast<const AstNodeLiteralTuple *>(&node)}) return std::ranges::all_of(
        t->items_, [](const AstNodePtr &item) { return is_literal(*item); });
    if (const auto *l{dynamic_cast<const AstNodeLiteralList *>(&node)}) return std::ranges::all_of(
        l->items_, [](const AstNodePtr &item) { return is_literal(*item); });
    if (const auto *d{dynamic_cast<const AstNodeLiteralDict *>(&node)})
        return std::ranges::all_of(d->items_, [](const auto &kv) {
            return is_literal(*kv.first) && (!kv.second || is_literal(*kv.second));
        });
    return false; // _G/_L、标识符、调用……都不是
}

// ============================================================
// 数值提升
// ============================================================

bool StaticEvaler::is_int_family(const AstNode &node) {
    return dynamic_cast<const AstNodeLiteralBool *>(&node) || dynamic_cast<const AstNodeLiteralInt *>(&node);
}

bool StaticEvaler::is_numeric(const AstNode &node) {
    return is_int_family(node) || dynamic_cast<const AstNodeLiteralFloat *>(&node);
}

BigInt StaticEvaler::to_bigint(const AstNode &node) {
    if (const auto *b{dynamic_cast<const AstNodeLiteralBool *>(&node)}) return BigInt{b->value_ ? 1LL : 0LL};
    const auto &i{dynamic_cast<const AstNodeLiteralInt &>(node)}; // 调用方保证 is_int_family(node)
    return BigInt::from_decimal_string(u32_to_utf8(i.raw_));
}

double StaticEvaler::to_double(const AstNode &node) {
    if (is_int_family(node)) return to_bigint(node).to_double();
    const auto &f{dynamic_cast<const AstNodeLiteralFloat &>(node)}; // 调用方保证 is_numeric(node)
    return std::strtod(u32_to_utf8(f.raw_).c_str(), nullptr);
}

bool StaticEvaler::try_to_ll(const BigInt &value, long long &out) {
    try {
        out = std::stoll(value.to_decimal_string());
        return true;
    } catch (const std::exception &) {
        return false; // 装不下 long long（数值太大/太小），不是我们能处理的规模，交给运行时
    }
}

// ============================================================
// 构造折叠结果
// ============================================================

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
            if (s.find('.') == std::string::npos) s += ".0"; // SL float 字面量语法要求小数点不可省略
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
    if (dynamic_cast<const AstNodeLiteralNone *>(&node)) return std::make_unique<AstNodeLiteralNone>(node.pos_);
    if (const auto *b{dynamic_cast<const AstNodeLiteralBool *>(&node)}) return std::make_unique<AstNodeLiteralBool>(
        node.pos_, b->value_);
    if (const auto *i{dynamic_cast<const AstNodeLiteralInt *>(&node)}) return std::make_unique<AstNodeLiteralInt>(
        node.pos_, i->raw_);
    if (const auto *f{dynamic_cast<const AstNodeLiteralFloat *>(&node)}) return std::make_unique<AstNodeLiteralFloat>(
        node.pos_, f->raw_);
    if (const auto *s{dynamic_cast<const AstNodeLiteralStr *>(&node)}) return std::make_unique<AstNodeLiteralStr>(
        node.pos_, s->value_);
    if (dynamic_cast<const AstNodeLiteralEllipsis *>(&node)) return std::make_unique<AstNodeLiteralEllipsis>(node.pos_);
    if (const auto *t{dynamic_cast<const AstNodeLiteralTuple *>(&node)}) {
        std::vector<AstNodePtr> items;
        items.reserve(t->items_.size());
        for (const auto &item : t->items_) items.push_back(clone_literal(*item));
        return std::make_unique<AstNodeLiteralTuple>(node.pos_, std::move(items));
    }
    if (const auto *l{dynamic_cast<const AstNodeLiteralList *>(&node)}) {
        std::vector<AstNodePtr> items;
        items.reserve(l->items_.size());
        for (const auto &item : l->items_) items.push_back(clone_literal(*item));
        return std::make_unique<AstNodeLiteralList>(node.pos_, std::move(items));
    }
    // 调用方保证 is_literal(node)，排除以上分支后只剩 dict
    const auto &d{dynamic_cast<const AstNodeLiteralDict &>(node)};
    std::vector<std::pair<AstNodePtr, AstNodePtr>> items;
    items.reserve(d.items_.size());
    for (const auto &[key, val] : d.items_) items.
        emplace_back(clone_literal(*key), val ? clone_literal(*val) : nullptr);
    return std::make_unique<AstNodeLiteralDict>(node.pos_, std::move(items));
}

bool StaticEvaler::literal_equal(const AstNode &a, const AstNode &b) {
    if (is_numeric(a) && is_numeric(b)) {
        if (is_int_family(a) && is_int_family(b)) return to_bigint(a) == to_bigint(b);
        return to_double(a) == to_double(b);
    }
    if (dynamic_cast<const AstNodeLiteralNone *>(&a)) return dynamic_cast<const AstNodeLiteralNone *>(&b) != nullptr;
    if (dynamic_cast<const AstNodeLiteralEllipsis *>(&a)) return
        dynamic_cast<const AstNodeLiteralEllipsis *>(&b) != nullptr;
    if (const auto *sa{dynamic_cast<const AstNodeLiteralStr *>(&a)}) {
        const auto *sb{dynamic_cast<const AstNodeLiteralStr *>(&b)};
        return sb && sa->value_ == sb->value_;
    }
    if (const auto *ta{dynamic_cast<const AstNodeLiteralTuple *>(&a)}) {
        const auto *tb{dynamic_cast<const AstNodeLiteralTuple *>(&b)};
        if (!tb || ta->items_.size() != tb->items_.size()) return false;
        for (size_t i{0}; i < ta->items_.size(); ++i) if (!literal_equal(*ta->items_[i], *tb->items_[i])) return false;
        return true;
    }
    if (const auto *la{dynamic_cast<const AstNodeLiteralList *>(&a)}) {
        const auto *lb{dynamic_cast<const AstNodeLiteralList *>(&b)};
        if (!lb || la->items_.size() != lb->items_.size()) return false;
        for (size_t i{0}; i < la->items_.size(); ++i) if (!literal_equal(*la->items_[i], *lb->items_[i])) return false;
        return true;
    }
    if (const auto *da{dynamic_cast<const AstNodeLiteralDict *>(&a)}) {
        const auto *db{dynamic_cast<const AstNodeLiteralDict *>(&b)};
        if (!db || da->items_.size() != db->items_.size()) return false;
        // dict 相等不计顺序：每个 key 都能在另一侧找到值相等的项
        for (const auto &[ka, va] : da->items_) {
            const auto it{std::ranges::find_if(db->items_,
                                               [&](const auto &kv) { return literal_equal(*ka, *kv.first); })};
            if (it == db->items_.end()) return false;
            if (static_cast<bool>(va) != static_cast<bool>(it->second)) return false;
            if (va && !literal_equal(*va, *it->second)) return false;
        }
        return true;
    }
    return false; // 剩下的（bool 已经被数字分支吃掉）不同类型之间一律不相等
}

StaticEvaler::Cmp StaticEvaler::literal_compare(const AstNode &a, const AstNode &b) {
    if (is_numeric(a) && is_numeric(b)) {
        if (is_int_family(a) && is_int_family(b)) {
            const std::strong_ordering cmp{to_bigint(a) <=> to_bigint(b)};
            return cmp < 0 ? Cmp::Less : cmp > 0 ? Cmp::Greater : Cmp::Equal;
        }
        const double da{to_double(a)};
        const double db{to_double(b)};
        return da < db ? Cmp::Less : da > db ? Cmp::Greater : Cmp::Equal;
    }
    if (const auto *sa{dynamic_cast<const AstNodeLiteralStr *>(&a)}) {
        const auto *sb{dynamic_cast<const AstNodeLiteralStr *>(&b)};
        if (!sb) return Cmp::Unordered;
        return sa->value_ < sb->value_ ? Cmp::Less : sb->value_ < sa->value_ ? Cmp::Greater : Cmp::Equal;
    }

    // tuple/tuple、list/list 逐元素比较，第一个不相等的元素决定结果；一方是另一方的前缀则前缀更小
    const auto lexicographic{[](const std::vector<AstNodePtr> &xa, const std::vector<AstNodePtr> &xb) -> Cmp {
        const size_t n{std::min(xa.size(), xb.size())};
        for (size_t i{0}; i < n; ++i) {
            const Cmp c{literal_compare(*xa[i], *xb[i])};
            if (c != Cmp::Equal) return c;
        }
        return xa.size() < xb.size() ? Cmp::Less : xa.size() > xb.size() ? Cmp::Greater : Cmp::Equal;
    }};
    if (const auto *ta{dynamic_cast<const AstNodeLiteralTuple *>(&a)}) {
        const auto *tb{dynamic_cast<const AstNodeLiteralTuple *>(&b)};
        return tb ? lexicographic(ta->items_, tb->items_) : Cmp::Unordered;
    }
    if (const auto *la{dynamic_cast<const AstNodeLiteralList *>(&a)}) {
        const auto *lb{dynamic_cast<const AstNodeLiteralList *>(&b)};
        return lb ? lexicographic(la->items_, lb->items_) : Cmp::Unordered;
    }
    return Cmp::Unordered; // None/dict/Ellipsis 均不支持大小比较
}

std::optional<std::u32string> StaticEvaler::to_display_string(const AstNode &node) {
    if (dynamic_cast<const AstNodeLiteralNone *>(&node)) return U"None";
    if (const auto *b{dynamic_cast<const AstNodeLiteralBool *>(&node)}) return b->value_ ? U"True" : U"False";
    if (const auto *i{dynamic_cast<const AstNodeLiteralInt *>(&node)}) return utf8_to_u32(
        to_bigint(*i).to_decimal_string());
    if (const auto *f{dynamic_cast<const AstNodeLiteralFloat *>(&node)}) return utf8_to_u32(
        format_double(to_double(*f)));
    if (const auto *s{dynamic_cast<const AstNodeLiteralStr *>(&node)}) return s->value_;
    return std::nullopt; // tuple/list/dict/Ellipsis/_G/_L：str() 涉及递归 repr，暂不支持
}
