#include "StaticEvaler.h"

#include "../../utils/string_utils.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <stdckdint.h>
#include <string>
#include <string_view>

namespace {

// a + b 是否会超过 cap
bool sum_exceeds(const size_t a, const size_t b, const size_t cap) {
    size_t sum;
    if (ckd_add(&sum, a, b)) return true;
    return sum > cap;
}

// a * b 是否会超过 cap
bool mul_size_exceeds(const size_t a, const size_t b, const size_t cap) {
    size_t product;
    if (ckd_mul(&product, a, b)) return true;
    return product > cap;
}

// 以下几个 checked_*，溢出统一返回 nullopt

// -a
std::optional<int64_t> checked_neg(const int64_t a) {
    int64_t result;
    // 事实上，只有 a == INT64_MIN 时会溢出
    if (ckd_sub(&result, int64_t{0}, a)) return std::nullopt;
    return result;
}

// a + b
std::optional<int64_t> checked_add(const int64_t a, const int64_t b) {
    int64_t result;
    if (ckd_add(&result, a, b)) return std::nullopt;
    return result;
}

// a - b
std::optional<int64_t> checked_sub(const int64_t a, const int64_t b) {
    int64_t result;
    if (ckd_sub(&result, a, b)) return std::nullopt;
    return result;
}

// a * b
std::optional<int64_t> checked_mul(const int64_t a, const int64_t b) {
    int64_t result;
    if (ckd_mul(&result, a, b)) return std::nullopt;
    return result;
}

// 非负整数次幂，快速幂，每一步乘法都做溢出检测。调用方保证 exponent >= 0
std::optional<int64_t> checked_pow(int64_t base, int64_t exponent) {
    int64_t result{1};
    while (exponent > 0) {
        if (exponent % 2 != 0) {
            const std::optional next{checked_mul(result, base)};
            if (!next) return std::nullopt;
            result = *next;
        }
        exponent /= 2;
        if (exponent > 0) {
            const std::optional next_base{checked_mul(base, base)};
            if (!next_base) return std::nullopt;
            base = *next_base;
        }
    }
    return result;
}

// a << b
std::optional<int64_t> checked_lshift(const int64_t a, const int64_t b) {
    if (b < 0 || b >= 63) return std::nullopt;
    return checked_mul(a, int64_t{1} << b); // value << shift 等价于 value * 2^shift
}

// a >> b
std::optional<int64_t> checked_rshift(const int64_t a, const int64_t b) {
    if (b < 0) return std::nullopt;
    if (b >= 63) return a < 0 ? int64_t{-1} : int64_t{0};
    return a >> b; // C++20 起对负数的右移是良定义的
}

// double -> string。调用方保证 value 有限
std::string double_to_string(const double value) {
    constexpr size_t nBufSize{std::numeric_limits<double>::max_exponent10 + 32}; // 这个数肯定够的
    std::array<char, nBufSize> buf{};
    const auto [ptr, ec]{
        std::to_chars(buf.data(), buf.data() + buf.size(), value, std::chars_format::fixed)
    };
    std::string s{buf.data(), ptr};
    if (s.find('.') == std::string::npos) s += ".0"; // 没有小数点就添上
    return s;
}

// string -> int64_t（可以带一个前导 '-'，但源码里的字面量本身不会）
std::optional<int64_t> string_to_int64(const std::u32string &raw) {
    const std::string text{u32_to_utf8(raw)};
    int64_t value{0};
    const char *begin{text.data()}, *end{begin + text.size()};
    if (const auto [ptr, ec]{std::from_chars(begin, end, value)}; ec != std::errc{} || ptr != end)
        return std::nullopt;
    return value;
}

// bool 提升成 int。调用方保证 is_int_family(node)
AstNodeLiteralInt promote_as_int(const AstNode &node) {
    if (const auto *i{dynamic_cast<const AstNodeLiteralInt *>(&node)}) return *i;
    const auto &b{dynamic_cast<const AstNodeLiteralBool &>(node)};
    return AstNodeLiteralInt{node.pos_, b.value_ ? U"1" : U"0"};
}

} // namespace

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
            const std::partial_ordering cmp{literal_compare(a, b)};
            if (cmp == std::partial_ordering::unordered)
                return nullptr; // 类型不支持比较，交给运行时报 TypeError
            switch (node.ops_[i]) {
            case Lt:
                result = cmp < 0;
                break;
            case Le:
                result = cmp <= 0;
                break;
            case Gt:
                result = cmp > 0;
                break;
            case Ge:
                result = cmp >= 0;
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

    // 'a' + 'b'：结果长度超过 kMaxStrLength 不折
    if (const auto *ls{dynamic_cast<const AstNodeLiteralStr *>(&l)},
        *rs{dynamic_cast<const AstNodeLiteralStr *>(&r)};
        ls && rs) {
        if (sum_exceeds(ls->value_.size(), rs->value_.size(), nMaxStrLength)) return nullptr;
        return std::make_unique<AstNodeLiteralStr>(node.pos_, ls->value_ + rs->value_);
    }

    // () + ()：结果元素个数超过 kMaxContainerItems 不折
    if (auto *lt{dynamic_cast<AstNodeLiteralTuple *>(&l)},
        *rt{dynamic_cast<AstNodeLiteralTuple *>(&r)};
        lt && rt) {
        if (sum_exceeds(lt->items_.size(), rt->items_.size(), nMaxContainerItems)) return nullptr;
        lt->items_.reserve(lt->items_.size() + rt->items_.size());
        for (auto &item : rt->items_) lt->items_.push_back(std::move(item));
        return std::make_unique<AstNodeLiteralTuple>(node.pos_, std::move(lt->items_));
    }

    // [] + []，同上
    if (auto *ll{dynamic_cast<AstNodeLiteralList *>(&l)},
        *rl{dynamic_cast<AstNodeLiteralList *>(&r)};
        ll && rl) {
        if (sum_exceeds(ll->items_.size(), rl->items_.size(), nMaxContainerItems)) return nullptr;
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

    // 非负 int 才有定义；数量转 int64_t 都装不下的，同样交给运行时
    const std::optional<int64_t> count{node_to_int64(count_node)};
    if (!count || *count < 0) return nullptr;
    const size_t n{static_cast<size_t>(*count)};

    // 现在，挑出来数字 count_node，另一个 container 不知道是啥
    // 'a' * 3：str 不可变，重复几份互相独立还是共享无法区分，只受长度上限约束
    if (const auto *s{dynamic_cast<const AstNodeLiteralStr *>(&container)}) {
        if (mul_size_exceeds(s->value_.size(), n, nMaxStrLength)) return nullptr;
        std::u32string value;
        value.reserve(s->value_.size() * n);
        for (size_t i{0}; i < n; ++i) value += s->value_;
        return std::make_unique<AstNodeLiteralStr>(node.pos_, std::move(value));
    }
    // (a, b) * 3：重复出来的每一份内部元素是共享引用（SL.md 3.4.2），只有内容深度不可变
    // （不含任何 list）时"共享 vs 独立拷贝"才不可区分，折叠才安全；否则不折，交给运行时
    if (const auto *t{dynamic_cast<const AstNodeLiteralTuple *>(&container)}) {
        if (!is_deeply_immutable(*t)) return nullptr;
        if (mul_size_exceeds(t->items_.size(), n, nMaxContainerItems)) return nullptr;
        std::vector<AstNodePtr> items;
        items.reserve(t->items_.size() * n);
        for (size_t i{0}; i < n; ++i) {
            for (const auto &item : t->items_) {
                items.push_back(clone_literal(*item));
            }
        }
        return std::make_unique<AstNodeLiteralTuple>(node.pos_, std::move(items));
    }
    // [a, b] * 3：list 恒可变，不存在"深度不可变"这一说，恒不折，交给运行时实现共享引用语义
    return nullptr;
}

AstNodePtr StaticEvaler::fold_arithmetic(AstNodeOpUnary &node) {
    using enum AstNodeOpUnary::OpType;
    const AstNode &operand{*node.operand_};

    if (!is_literal_pure(operand) || !is_numeric(operand)) return nullptr;

    // int：转 int64_t 失败（超范围）或者取反溢出（-INT64_MIN）都不折
    if (is_int_family(operand)) {
        const std::optional v{node_to_int64(operand)};
        if (!v) return nullptr;
        if (node.op_ == Pos) return make_int(node.pos_, *v);
        const std::optional negated{checked_neg(*v)};
        if (!negated) return nullptr;
        return make_int(node.pos_, *negated);
    }
    // float
    const double v{node_to_double(operand)};
    return make_float(node.pos_, node.op_ == Neg ? -v : v);
}

AstNodePtr StaticEvaler::fold_arithmetic(AstNodeOpBinary &node) {
    using enum AstNodeOpBinary::OpType;
    const AstNode &l{*node.left_}, &r{*node.right_};
    if (!is_literal_pure(l) || !is_literal_pure(r) || !is_numeric(l) || !is_numeric(r))
        return nullptr;

    const bool is_both_int{is_int_family(l) && is_int_family(r)};

    // int 分支公用：把两个操作数都解析成 int64_t，任意一个超范围就不折
    const auto int_operands{[&]() -> std::optional<std::pair<int64_t, int64_t>> {
        const std::optional<int64_t> lv{node_to_int64(l)};
        const std::optional<int64_t> rv{node_to_int64(r)};
        if (!lv || !rv) return std::nullopt;
        return std::make_pair(*lv, *rv);
    }};

    switch (node.op_) {
    case Add:
        if (is_both_int) {
            const auto operands{int_operands()};
            if (!operands) return nullptr;
            const auto result{checked_add(operands->first, operands->second)};
            if (!result) return nullptr;
            return make_int(node.pos_, *result);
        }
        return make_float(node.pos_, node_to_double(l) + node_to_double(r));

    case Sub:
        if (is_both_int) {
            const auto operands{int_operands()};
            if (!operands) return nullptr;
            const auto result{checked_sub(operands->first, operands->second)};
            if (!result) return nullptr;
            return make_int(node.pos_, *result);
        }
        return make_float(node.pos_, node_to_double(l) - node_to_double(r));

    case Mul:
        if (is_both_int) {
            const auto operands{int_operands()};
            if (!operands) return nullptr;
            const auto result{checked_mul(operands->first, operands->second)};
            if (!result) return nullptr;
            return make_int(node.pos_, *result);
        }
        return make_float(node.pos_, node_to_double(l) * node_to_double(r));

    case Div: {
        const double rv{node_to_double(r)};
        if (rv == 0.0) return nullptr; // MathError，交给运行时
        return make_float(node.pos_, node_to_double(l) / rv);
    }

    case DivFloor: {
        if (is_both_int) {
            const auto operands{int_operands()};
            if (!operands) return nullptr;
            const auto [lv, rv]{*operands};
            if (rv == 0) return nullptr;
            // INT64_MIN / -1 商本身就溢出（二补码下 -INT64_MIN 无法表示），不折
            if (lv == std::numeric_limits<int64_t>::min() && rv == -1) return nullptr;
            // C++ 内置 / 是向零截断，这里手动调整成向负无穷取整（SL.md 3.4.2）
            int64_t q{lv / rv};
            if (lv % rv != 0 && (lv % rv < 0) != (rv < 0)) --q;
            return make_int(node.pos_, q);
        }
        const double rv{node_to_double(r)};
        if (rv == 0.0) return nullptr;
        return make_float(node.pos_, std::floor(node_to_double(l) / rv));
    }

    case Mod: {
        if (is_both_int) {
            const auto operands{int_operands()};
            if (!operands) return nullptr;
            const auto [lv, rv]{*operands};
            if (rv == 0) return nullptr;
            if (lv == std::numeric_limits<int64_t>::min() && rv == -1) return nullptr; // 同上
            int64_t m{lv % rv};
            if (m != 0 && (m < 0) != (rv < 0))
                m += rv; // 向 rv 的符号方向调整，与 // 满足同一恒等式
            return make_int(node.pos_, m);
        }
        const double rv{node_to_double(r)};
        if (rv == 0.0) return nullptr;
        double m{std::fmod(node_to_double(l), rv)};
        if (m != 0.0 && (m < 0.0) != (rv < 0.0))
            m += rv; // 向 y 的符号方向调整，与 // 满足同一恒等式
        return make_float(node.pos_, m);
    }

    case Pow:
        // 都是 int 且指数非负：结果必须仍是 int（SL.md 3.4.2），要么折成 int，要么（指数/底数超出
        // int64_t 范围、结果溢出）干脆不折，绝不能退化成下面的 float 分支——
        // 哪怕退化算出来的 float 结果凑巧看着没问题（比如 1 ** 一个超大的数，float 幂算出来正好是
        // 1.0），类型也是错的，SL.md 明确要求这种情况结果必须是 int
        if (is_both_int) {
            const auto operands{int_operands()};
            if (!operands) return nullptr; // 操作数超出 int64_t 范围，不折
            if (operands->second >= 0) {
                const auto result{checked_pow(operands->first, operands->second)};
                if (result) return make_int(node.pos_, *result);
                return nullptr; // 结果溢出，同样不折
            }
            // 指数为负：这是唯一允许从"都是 int"退化成 float 结果的情况，走下面
        }
        return make_float(node.pos_, std::pow(node_to_double(l), node_to_double(r)));

    default:
        return nullptr;
    }
}

AstNodePtr StaticEvaler::fold_bitwise(AstNodeOpUnary &node) {
    const AstNode &operand{*node.operand_};
    if (!is_literal_pure(operand) || !is_int_family(operand)) return nullptr;

    const std::optional<int64_t> v{node_to_int64(operand)};
    if (!v) return nullptr;
    return make_int(node.pos_, ~*v); // 按位取反不会溢出，无需额外检查
}

AstNodePtr StaticEvaler::fold_bitwise(AstNodeOpBinary &node) {
    using enum AstNodeOpBinary::OpType;
    const AstNode &l{*node.left_}, &r{*node.right_};
    if (!is_literal_pure(l) || !is_literal_pure(r) || !is_int_family(l) || !is_int_family(r))
        return nullptr;

    const std::optional<int64_t> lv{node_to_int64(l)};
    const std::optional<int64_t> rv{node_to_int64(r)};
    if (!lv || !rv) return nullptr;

    switch (node.op_) {
    case BitAnd:
        return make_int(node.pos_, *lv & *rv); // & ^ | 两个定宽整数直接算，不会溢出
    case BitOr:
        return make_int(node.pos_, *lv | *rv);
    case BitXor:
        return make_int(node.pos_, *lv ^ *rv);
    case LShift: {
        const std::optional<int64_t> result{checked_lshift(*lv, *rv)};
        if (!result) return nullptr;
        return make_int(node.pos_, *result);
    }
    case RShift: {
        const std::optional<int64_t> result{checked_rshift(*lv, *rv)};
        if (!result) return nullptr;
        return make_int(node.pos_, *result);
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
    if (const auto *i{dynamic_cast<const AstNodeLiteralInt *>(&literal)}) {
        const std::optional v{node_to_int64(*i)};
        return !v || *v != 0; // 太大了装不下则必然非零；或者能装下而且是非零
    }
    if (const auto *f{dynamic_cast<const AstNodeLiteralFloat *>(&literal)})
        return node_to_double(*f) != 0.0;
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

bool StaticEvaler::is_deeply_immutable(const AstNode &node) {
    if (dynamic_cast<const AstNodeLiteralList *>(&node)) return false; // list 恒可变
    if (const auto *t{dynamic_cast<const AstNodeLiteralTuple *>(&node)})
        return std::ranges::all_of(t->items_, [](const AstNodePtr &item) {
            return is_deeply_immutable(*item);
        });
    return true; // 调用方保证 is_literal_pure(node)，排除 list/tuple 后剩下的都是天然不可变的基例
}

bool StaticEvaler::is_int_family(const AstNode &node) {
    return dynamic_cast<const AstNodeLiteralBool *>(&node) ||
           dynamic_cast<const AstNodeLiteralInt *>(&node);
}

bool StaticEvaler::is_numeric(const AstNode &node) {
    return is_int_family(node) || dynamic_cast<const AstNodeLiteralFloat *>(&node);
}

std::optional<int64_t> StaticEvaler::node_to_int64(const AstNode &node) {
    if (const auto *b{dynamic_cast<const AstNodeLiteralBool *>(&node)})
        return b->value_ ? int64_t{1} : int64_t{0};
    const auto &i{dynamic_cast<const AstNodeLiteralInt &>(node)};
    return string_to_int64(i.raw_);
}

double StaticEvaler::node_to_double(const AstNode &node) {
    if (const auto *b{dynamic_cast<const AstNodeLiteralBool *>(&node)})
        return b->value_ ? 1.0 : 0.0;
    if (const auto *i{dynamic_cast<const AstNodeLiteralInt *>(&node)})
        return std::strtod(u32_to_utf8(i->raw_).c_str(), nullptr);  // 任意长度的十进制文本都能处理
    const auto &f{dynamic_cast<const AstNodeLiteralFloat &>(node)}; // 调用方保证 is_numeric(node)
    return std::strtod(u32_to_utf8(f.raw_).c_str(), nullptr);
}

AstNodePtr StaticEvaler::make_bool(const Position pos, const bool value) {
    return std::make_unique<AstNodeLiteralBool>(pos, value);
}

AstNodePtr StaticEvaler::make_int(const Position pos, const int64_t value) {
    return std::make_unique<AstNodeLiteralInt>(pos, utf8_to_u32(std::to_string(value)));
}

AstNodePtr StaticEvaler::make_float(const Position pos, const double value) {
    if (!std::isfinite(value)) return nullptr; // ±inf/NaN 写不出合法的 float 字面量，交给运行时处理
    return std::make_unique<AstNodeLiteralFloat>(pos, utf8_to_u32(double_to_string(value)));
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
        if (is_int_family(a) && is_int_family(b))
            return literal_compare_int(promote_as_int(a), promote_as_int(b)) ==
                   std::strong_ordering::equal;
        return node_to_double(a) == node_to_double(b);
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

std::partial_ordering StaticEvaler::literal_compare(const AstNode &a, const AstNode &b) {
    if (is_numeric(a) && is_numeric(b)) {
        if (is_int_family(a) && is_int_family(b))
            return literal_compare_int(
                promote_as_int(a), promote_as_int(b)
            ); // 隐式转成 partial_ordering
        return node_to_double(a) <=> node_to_double(b);
    }
    if (const auto *sa{dynamic_cast<const AstNodeLiteralStr *>(&a)}) {
        const auto *sb{dynamic_cast<const AstNodeLiteralStr *>(&b)};
        if (!sb) return std::partial_ordering::unordered;
        return sa->value_ <=> sb->value_;
    }

    // tuple/tuple、list/list 逐元素比较，第一个不相等的元素决定结果；一方是另一方的前缀则前缀更小
    const auto lexicographic{
        [](const std::vector<AstNodePtr> &xa,
           const std::vector<AstNodePtr> &xb) -> std::partial_ordering {
            const size_t n{std::min(xa.size(), xb.size())};
            for (size_t i{0}; i < n; ++i) {
                const std::partial_ordering c{literal_compare(*xa[i], *xb[i])};
                if (c != 0) return c;
            }
            return xa.size() <=> xb.size();
        }
    };
    if (const auto *ta{dynamic_cast<const AstNodeLiteralTuple *>(&a)}) {
        const auto *tb{dynamic_cast<const AstNodeLiteralTuple *>(&b)};
        return tb ? lexicographic(ta->items_, tb->items_) : std::partial_ordering::unordered;
    }
    if (const auto *la{dynamic_cast<const AstNodeLiteralList *>(&a)}) {
        const auto *lb{dynamic_cast<const AstNodeLiteralList *>(&b)};
        return lb ? lexicographic(la->items_, lb->items_) : std::partial_ordering::unordered;
    }
    return std::partial_ordering::unordered; // None/dict/Ellipsis 均不支持大小比较
}

std::strong_ordering
StaticEvaler::literal_compare_int(const AstNodeLiteralInt &a, const AstNodeLiteralInt &b) {
    const bool a_neg{!a.raw_.empty() && a.raw_[0] == U'-'};
    const bool b_neg{!b.raw_.empty() && b.raw_[0] == U'-'};
    if (a_neg != b_neg) return a_neg ? std::strong_ordering::less : std::strong_ordering::greater;

    // 符号相同，去掉符号和前导零后比较：位数不等，位数多的更大；位数相等再按字典序
    const auto magnitude{[](const std::u32string &raw) {
        size_t idx{!raw.empty() && (raw[0] == U'-' || raw[0] == U'+') ? size_t{1} : size_t{0}};
        while (idx + 1 < raw.size() && raw[idx] == U'0') ++idx;
        return std::u32string_view{raw}.substr(idx);
    }};
    const std::u32string_view ma{magnitude(a.raw_)};
    const std::u32string_view mb{magnitude(b.raw_)};

    std::strong_ordering magnitude_cmp{std::strong_ordering::equal};
    if (ma.size() != mb.size())
        magnitude_cmp = ma.size() <=> mb.size();
    else if (const int c{ma.compare(mb)}; c != 0)
        magnitude_cmp = c <=> 0;

    if (!a_neg) return magnitude_cmp; // 都非负，绝对值大小就是数值大小
    // 都是负数：绝对值越大，数值越小，方向取反
    if (magnitude_cmp == std::strong_ordering::less) return std::strong_ordering::greater;
    if (magnitude_cmp == std::strong_ordering::greater) return std::strong_ordering::less;
    return std::strong_ordering::equal;
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
