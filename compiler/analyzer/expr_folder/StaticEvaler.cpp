#include "StaticEvaler.h"

#include "../../../utils/string_utils.h"

#include <algorithm>
#include <cassert>
#include <charconv>
#include <cstdint>
#include <stdckdint.h>
#include <string>

namespace {

// 是不是一个写着负号的 int 字面量
bool is_negative_int_literal(const AstNode &node) {
    const auto *i{dynamic_cast<const AstNodeLiteralInt *>(&node)};
    return i && !i->raw_.empty() && i->raw_.front() == U'-';
}

// a + b 是否会超过 cap
bool sum_exceeds(const size_t a, const size_t b, const size_t cap) {
    size_t sum;
    if (ckd_add(&sum, a, b)) return true;
    return sum > cap;
}

// a * b 是否会超过 cap
bool mul_exceeds(const size_t a, const size_t b, const size_t cap) {
    size_t product;
    if (ckd_mul(&product, a, b)) return true;
    return product > cap;
}

// a + b（int64_t），溢出返回 nullopt
std::optional<int64_t> checked_add(const int64_t a, const int64_t b) {
    int64_t sum;
    if (ckd_add(&sum, a, b)) return std::nullopt;
    return sum;
}

// a - b（int64_t），溢出返回 nullopt
std::optional<int64_t> checked_sub(const int64_t a, const int64_t b) {
    int64_t diff;
    if (ckd_sub(&diff, a, b)) return std::nullopt;
    return diff;
}

// a * b（int64_t），溢出返回 nullopt
std::optional<int64_t> checked_mul(const int64_t a, const int64_t b) {
    int64_t product;
    if (ckd_mul(&product, a, b)) return std::nullopt;
    return product;
}

// -a（int64_t），仅 INT64_MIN 会溢出
std::optional<int64_t> checked_neg(const int64_t a) {
    if (a == INT64_MIN) return std::nullopt;
    return -a;
}

// 向负无穷取整的整除。调用方保证 b != 0；唯一会溢出的情形是 INT64_MIN / -1
std::optional<int64_t> checked_floor_div(const int64_t a, const int64_t b) {
    assert(b != 0);

    if (a == INT64_MIN && b == -1) return std::nullopt;
    int64_t q{a / b}, r{a % b};
    if (r != 0 && (r < 0) != (b < 0)) --q; // C++ 的 / 向零截断，这里补一格调成向负无穷
    return q;
}

// 与 checked_floor_div 取整方向一致的取模。调用方保证 b != 0；b == -1 时特判直接给 0
int64_t floor_mod(const int64_t a, const int64_t b) {
    assert(b != 0);

    if (b == -1) return 0;
    int64_t r{a % b};
    if (r != 0 && (r < 0) != (b < 0)) r += b;
    return r;
}

// base ** exponent（int64_t）。调用方保证 exponent >= 0；溢出返回 nullopt
std::optional<int64_t> checked_pow(const int64_t base, const int64_t exponent) {
    assert(exponent >= 0);

    // 0 ** 0 == 1，且 base 为 0/1/-1 时指数可以大到不适合真的循环，须特判
    if (exponent == 0) return 1;
    if (base == 0) return 0;
    if (base == 1) return 1;
    if (base == -1) return exponent % 2 == 0 ? 1 : -1;

    // |base| >= 2：结果只会越来越大，一旦溢出循环就会提前退出，不会真的跑 exponent 次
    int64_t result{1};
    for (int64_t i{0}; i < exponent; ++i) {
        const std::optional next{checked_mul(result, base)};
        if (!next) return std::nullopt;
        result = *next;
    }
    return result;
}

} // namespace

AstNodePtr StaticEvaler::fold_unary(const AstNodeOpUnary &node) {
    using enum AstNodeOpUnary::OpType;

    switch (node.op_) {
    case Pos:
    case Neg:
        return fold_arithmetic(node);
    case BitInvert:
        return fold_bitwise(node);
    case Not:
        return fold_not(node);
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

    // 链式比较短路（a < b < c 等价于 a<b and b<c and ...）
    size_t i{0};
    while (i < node.ops_.size()) {
        const AstNode &a{*node.operands_[i]}, &b{*node.operands_[i + 1]};
        if (!is_literal_pure(a) || !is_literal_pure(b)) break; // 无法确定

        bool result;
        if (node.ops_[i] == Eq || node.ops_[i] == Ne) {
            const std::optional eq{literal_equal(a, b)};
            if (!eq) break; // 值判不了，这一环之后的都停在这
            result = node.ops_[i] == Eq ? *eq : !*eq;
        } else {
            const std::partial_ordering cmp{literal_compare(a, b)};
            // 类型不支持比较，没法折
            if (cmp == std::partial_ordering::unordered) break;
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

        // 链式比较：一旦某一环为假就短路
        if (!result) return make_bool(node.pos_, false);

        ++i;
    }

    // 全链都确定为 True
    if (i == node.ops_.size()) return make_bool(node.pos_, true);
    // 第一环就没法判定，整体没能折
    if (i == 0) return nullptr;

    // 前 i 环都是字面量且确定为 True 但接下来的一环没法判定：部分折叠
    std::vector<AstNodeCompare::OpType> ops;
    std::vector<AstNodePtr> operands;
    std::vector<Position> positions_op;
    for (size_t j{i}; j < node.operands_.size(); ++j)
        operands.push_back(std::move(node.operands_[j]));
    for (size_t j{i}; j < node.ops_.size(); ++j) {
        ops.push_back(node.ops_[j]);
        positions_op.push_back(node.positions_op_[j]);
    }
    const Position new_pos{operands.front()->pos_}; // 新链自己的起始位置 = 剩下的第一个操作数
    return std::make_unique<AstNodeCompare>(
        new_pos, std::move(ops), std::move(operands), std::move(positions_op)
    );
}

AstNodePtr StaticEvaler::fold_if(AstNodeIf &node) {
    // 判不了真值的 clause 跟"不是字面量"一样，到它为止停下——绝不能猜
    const auto cond_is_false{[](const AstNodePtr &cond) {
        if (!is_literal_pure(*cond)) return false;
        const std::optional t{truthy(*cond)};
        return t && !*t;
    }};
    const auto cond_is_true{[](const AstNodePtr &cond) {
        if (!is_literal_pure(*cond)) return false;
        const std::optional t{truthy(*cond)};
        return t && *t;
    }};

    size_t i{0}; // 索引 i 之前的所有 clauses 都是字面量且是 False
    while (i < node.clauses_.size() && cond_is_false(node.clauses_[i].cond_)) ++i;

    // 确定为 False* 跟着个 True
    if (i < node.clauses_.size() && cond_is_true(node.clauses_[i].cond_)) {
        return std::move(node.clauses_[i].body_);
    }

    // 第一个 clause 就没法判定
    if (i == 0) return nullptr;

    // 确定为全是 False
    if (i == node.clauses_.size()) {
        if (node.else_expr_) return std::move(node.else_expr_);
        return std::make_unique<AstNodeLiteralNone>(node.pos_);
    }

    // 确定为 False+ 跟着个 不能确定的：部分折叠
    std::vector<AstNodeIf::AstNodeCondAndExpr> remaining;
    for (size_t j{i}; j < node.clauses_.size(); ++j)
        remaining.push_back(std::move(node.clauses_[j]));
    return std::make_unique<AstNodeIf>(node.pos_, std::move(remaining), std::move(node.else_expr_));
}

AstNodePtr StaticEvaler::fold_for_cond(AstNodeForCond &node) {
    // 空->True、不是字面量、真值判不了、真值为 True，这四种都不折
    if (!node.cond_ || !is_literal_pure(*node.cond_)) return nullptr;
    if (const std::optional cond_truthy{truthy(*node.cond_)}; !cond_truthy || *cond_truthy)
        return nullptr;

    // 一轮都没跑时各收集模式的退化值
    AstNodePtr result;
    switch (node.collect_.container_) {
    case CollectMark::Container::None:
        result = std::make_unique<AstNodeLiteralInt>(node.pos_, U"0");
        break;
    case CollectMark::Container::List:
        result = std::make_unique<AstNodeLiteralList>(node.pos_, std::vector<AstNodePtr>{});
        break;
    case CollectMark::Container::Dict:
        result = std::make_unique<AstNodeLiteralDict>(
            node.pos_, std::vector<std::pair<AstNodePtr, AstNodePtr>>{}
        );
        break;
    }
    if (!node.init_) return result;

    std::vector<AstNodePtr> exprs;
    exprs.push_back(std::move(node.init_));
    exprs.push_back(std::move(result));
    return std::make_unique<AstNodeCompound>(node.pos_, std::move(exprs));
}

AstNodePtr StaticEvaler::fold_compound(AstNodeCompound &node) {
    // {} -> None
    if (node.exprs_.empty()) return std::make_unique<AstNodeLiteralNone>(node.pos_);

    // { expr } -> expr，无论是否字面量
    if (node.exprs_.size() == 1) return std::move(node.exprs_.front());

    // 除最后一条外，逐条判断能不能丢。最后一条永远保留。
    bool has_dropped_anything{false}; // 有没有剪去东西
    for (size_t i{0}; i + 1 < node.exprs_.size(); ++i) {
        if (is_literal_pure(*node.exprs_[i])) has_dropped_anything = true;
    }
    if (!has_dropped_anything) return nullptr;

    std::vector<AstNodePtr> kept;
    for (size_t i{0}; i + 1 < node.exprs_.size(); ++i) {
        if (!is_literal_pure(*node.exprs_[i])) kept.push_back(std::move(node.exprs_[i]));
    }
    kept.push_back(std::move(node.exprs_.back()));

    // 丢到只剩最后一条，直接展开
    if (kept.size() == 1) return std::move(kept.front());
    // 部分折：拼一个更短的
    return std::make_unique<AstNodeCompound>(node.pos_, std::move(kept));
}

AstNodePtr StaticEvaler::fold_not(const AstNodeOpUnary &node) {
    if (!is_literal_pure(*node.operand_)) return nullptr;

    const std::optional t{truthy(*node.operand_)};
    if (!t) return nullptr;
    return make_bool(node.pos_, !*t);
}

AstNodePtr StaticEvaler::fold_add(AstNodeOpBinary &node) {
    AstNode &l{*node.left_}, &r{*node.right_};
    if (!is_literal_pure(l) || !is_literal_pure(r)) return nullptr;

    // 数字 + 数字：交给 fold_arithmetic
    if (is_numeric(l) && is_numeric(r)) return fold_arithmetic(node);

    // 'a' + 'b'：结果长度超过 nMaxStrLength 不折
    if (const auto *ls{dynamic_cast<const AstNodeLiteralStr *>(&l)},
        *rs{dynamic_cast<const AstNodeLiteralStr *>(&r)};
        ls && rs) {
        if (sum_exceeds(ls->value_.size(), rs->value_.size(), nMaxStrLength)) return nullptr;
        return std::make_unique<AstNodeLiteralStr>(node.pos_, ls->value_ + rs->value_);
    }

    // () + ()：结果元素个数超过 nMaxContainerItems 不折
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

AstNodePtr StaticEvaler::fold_mul(const AstNodeOpBinary &node) {
    const AstNode &l{*node.left_}, &r{*node.right_};
    if (!is_literal_pure(l) || !is_literal_pure(r)) return nullptr;

    // 数字 * 数字：交给 fold_arithmetic
    if (is_numeric(l) && is_numeric(r)) return fold_arithmetic(node);

    // 下面尝试理解为容器的重复。重复次数只接受 int，bool 不算
    if (!is_int(l) && !is_int(r)) return nullptr;
    // 确定哪个是重复次数，哪个可能是容器
    const auto &[container_node, count_node] =
        [&]() -> std::pair<const AstNode &, const AstNode &> {
        if (is_int(r)) return {l, r};
        return {r, l};
    }();

    // 尝试塞进 int64_t
    const std::optional count_optional{node_to_int64(count_node)};
    if (!count_optional) return nullptr;
    const size_t count{static_cast<size_t>(*count_optional)};

    // 'a' * 3
    if (const auto *ls{dynamic_cast<const AstNodeLiteralStr *>(&container_node)}) {
        // 空串重复多少次都还是空串，直接给结果
        if (ls->value_.empty()) return std::make_unique<AstNodeLiteralStr>(node.pos_, U"");

        if (mul_exceeds(ls->value_.size(), count, nMaxStrLength)) return nullptr;

        std::u32string value;
        value.reserve(ls->value_.size() * count);
        for (size_t i{0}; i < count; ++i) value += ls->value_;
        return std::make_unique<AstNodeLiteralStr>(node.pos_, std::move(value));
    }

    // (a, b) * 3
    if (const auto *lt{dynamic_cast<const AstNodeLiteralTuple *>(&container_node)}) {
        if (lt->items_.empty())
            return std::make_unique<AstNodeLiteralTuple>(node.pos_, std::vector<AstNodePtr>{});

        if (!std::ranges::all_of(lt->items_, [](const AstNodePtr &item) {
                return is_literal_const(*item);
            }))
            return nullptr;

        if (mul_exceeds(lt->items_.size(), count, nMaxContainerItems)) return nullptr;

        std::vector<AstNodePtr> repeated;
        repeated.reserve(lt->items_.size() * count);
        for (size_t i{0}; i < count; ++i) {
            for (const AstNodePtr &item : lt->items_)
                repeated.push_back(clone_const_literal(*item));
        }

        return std::make_unique<AstNodeLiteralTuple>(node.pos_, std::move(repeated));
    }

    // [a, b] * 3。列表本身每次求值都新建，重复的是元素，判据跟元组的一样
    if (const auto *ll{dynamic_cast<const AstNodeLiteralList *>(&container_node)}) {
        if (ll->items_.empty())
            return std::make_unique<AstNodeLiteralList>(node.pos_, std::vector<AstNodePtr>{});

        if (!std::ranges::all_of(ll->items_, [](const AstNodePtr &item) {
                return is_literal_const(*item);
            }))
            return nullptr;

        if (mul_exceeds(ll->items_.size(), count, nMaxContainerItems)) return nullptr;

        std::vector<AstNodePtr> repeated;
        repeated.reserve(ll->items_.size() * count);
        for (size_t i{0}; i < count; ++i) {
            for (const AstNodePtr &item : ll->items_)
                repeated.push_back(clone_const_literal(*item));
        }

        return std::make_unique<AstNodeLiteralList>(node.pos_, std::move(repeated));
    }

    return nullptr;
}

AstNodePtr StaticEvaler::fold_arithmetic(const AstNodeOpUnary &node) {
    using enum AstNodeOpUnary::OpType;
    const AstNode &operand{*node.operand_};

    // 只接 int/bool
    if (!is_literal_pure(operand) || !is_int_family(operand)) return nullptr;

    const std::optional v{node_to_int64(operand)};
    if (!v) return nullptr;
    if (node.op_ == Pos) return make_int(node.pos_, *v);
    const std::optional neg{checked_neg(*v)};
    if (!neg) return nullptr; // -INT64_MIN 装不下
    return make_int(node.pos_, *neg);
}

AstNodePtr StaticEvaler::fold_arithmetic(const AstNodeOpBinary &node) {
    using enum AstNodeOpBinary::OpType;
    const AstNode &l{*node.left_}, &r{*node.right_};
    if (!is_literal_pure(l) || !is_literal_pure(r)) return nullptr;

    // 任何一侧是 decimal（或 `/`）都不折
    if (!is_int_family(l) || !is_int_family(r) || node.op_ == Div) return nullptr;

    const std::optional lv{node_to_int64(l)}, rv{node_to_int64(r)};
    if (!lv || !rv) return nullptr;

    switch (node.op_) {
    case Add: {
        const std::optional v{checked_add(*lv, *rv)};
        if (!v) return nullptr;
        return make_int(node.pos_, *v);
    }
    case Sub: {
        const std::optional v{checked_sub(*lv, *rv)};
        if (!v) return nullptr;
        return make_int(node.pos_, *v);
    }
    case Mul: {
        const std::optional v{checked_mul(*lv, *rv)};
        if (!v) return nullptr;
        return make_int(node.pos_, *v);
    }

    // 除零是 MathError，原样留给运行期
    case DivFloor: {
        if (*rv == 0) return nullptr;
        const std::optional v{checked_floor_div(*lv, *rv)};
        if (!v) return nullptr;
        return make_int(node.pos_, *v);
    }
    case Mod: {
        if (*rv == 0) return nullptr;
        return make_int(node.pos_, floor_mod(*lv, *rv));
    }

    case Pow: {
        // 指数为负时结果是 decimal（3 ** -1 == 0.333...），落进上面那条 decimal 禁令
        if (*rv < 0) return nullptr;
        const std::optional v{checked_pow(*lv, *rv)};
        if (!v) return nullptr;
        return make_int(node.pos_, *v);
    }

    default:
        return nullptr;
    }
}

AstNodePtr StaticEvaler::fold_bitwise(const AstNodeOpUnary &node) {
    const AstNode &operand{*node.operand_};

    // 只接 int
    if (!is_literal_pure(operand) || !is_int(operand)) return nullptr;

    const std::optional v{node_to_int64(operand)};
    if (!v) return nullptr;
    return make_int(node.pos_, ~*v); // ~x == -x-1，int64_t 位运算不会溢出
}

AstNodePtr StaticEvaler::fold_bitwise(const AstNodeOpBinary &node) {
    using enum AstNodeOpBinary::OpType;
    const AstNode &l{*node.left_}, &r{*node.right_};

    // 只接 int
    if (!is_literal_pure(l) || !is_literal_pure(r) || !is_int(l) || !is_int(r)) return nullptr;

    // 右操作数装不下 int64_t 时各运算符的处置并不一致，所以这里只取值不退出
    const std::optional lv{node_to_int64(l)}, rv{node_to_int64(r)};
    if (!lv) return nullptr; // 左操作数装不下 int64_t，一律不折

    switch (node.op_) {
    // & | ^ 两侧都已经是 int64_t，结果不可能超出 int64_t，不会溢出
    case BitAnd:
    case BitOr:
    case BitXor:
        if (!rv) return nullptr;
        if (node.op_ == BitAnd) return make_int(node.pos_, *lv & *rv);
        if (node.op_ == BitOr) return make_int(node.pos_, *lv | *rv);
        return make_int(node.pos_, *lv ^ *rv);

    case LShift: {
        // 负移位量是 ValueError，原样留给运行期
        if (!rv || *rv < 0) return nullptr;
        if (*lv == 0) return make_int(node.pos_, 0); // 0 左移多少位都是 0，避免下面循环一直跑
        // a << k == 反复乘 2，一旦溢出立刻退出——不管 k 有多大，非零值最多翻 63 次倍就必然溢出
        int64_t result{*lv};
        for (int64_t i{0}; i < *rv; ++i) {
            const std::optional next{checked_mul(result, int64_t{2})};
            if (!next) return nullptr;
            result = *next;
        }
        return make_int(node.pos_, result);
    }
    case RShift:
        // 负移位量同上不折——这一步只需要知道符号，不需要移位量真的能塞进 int64_t
        if (is_negative_int_literal(r)) return nullptr;
        // 移位量装不下 int64_t（且已确认非负）时，结果只会是 0 或 -1（算术右移补符号位），
        if (!rv) return make_int(node.pos_, *lv < 0 ? -1 : 0);
        // 移位量达到/超过 int64_t 位宽时同理，直接给出来，避免对 >> 传入一个 C++ 认定为 UB 的位移量
        if (*rv >= 63) return make_int(node.pos_, *lv < 0 ? -1 : 0);
        return make_int(node.pos_, *lv >> *rv); // 有符号右移是算术移位，等价于 floor(x / 2^k)

    default:
        return nullptr;
    }
}

AstNodePtr StaticEvaler::fold_and_or(AstNodeOpBinary &node) {
    using enum AstNodeOpBinary::OpType;
    if (!is_literal_pure(*node.left_)) return nullptr;

    const std::optional left_truthy{truthy(*node.left_)};
    if (!left_truthy) return nullptr;
    const bool take_left{node.op_ == And ? !*left_truthy : *left_truthy};
    return std::move(take_left ? node.left_ : node.right_);
}

std::optional<bool> StaticEvaler::truthy(const AstNode &literal) {
    assert(is_literal_pure(literal));

    if (dynamic_cast<const AstNodeLiteralNone *>(&literal)) return false;
    if (const auto *b{dynamic_cast<const AstNodeLiteralBool *>(&literal)}) return b->value_;

    // int 必须按值判零，不能看 raw_ 的字面长相
    if (is_int_family(literal)) {
        const std::optional v{node_to_int64(literal)};
        if (!v) return std::nullopt;
        return *v != 0;
    }
    // decimal 不折（走到这里说明 is_numeric(literal) 且不是 int_family，只剩 decimal）
    if (is_numeric(literal)) return std::nullopt;

    if (const auto *s{dynamic_cast<const AstNodeLiteralStr *>(&literal)}) return !s->value_.empty();
    if (const auto *t{dynamic_cast<const AstNodeLiteralTuple *>(&literal)})
        return !t->items_.empty();
    if (const auto *l{dynamic_cast<const AstNodeLiteralList *>(&literal)})
        return !l->items_.empty();

    return true; // 其他均为 True
}

bool StaticEvaler::is_literal_pure(const AstNode &node) {
    // 天然满足的
    if (dynamic_cast<const AstNodeLiteralNone *>(&node) ||
        dynamic_cast<const AstNodeLiteralBool *>(&node) ||
        dynamic_cast<const AstNodeLiteralInt *>(&node) ||
        dynamic_cast<const AstNodeLiteralDecimal *>(&node) ||
        dynamic_cast<const AstNodeLiteralStr *>(&node) ||
        dynamic_cast<const AstNodeLiteralEllipsis *>(&node))
        return true;

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

bool StaticEvaler::is_literal_const(const AstNode &node) {
    // 天然满足的
    if (dynamic_cast<const AstNodeLiteralNone *>(&node) ||
        dynamic_cast<const AstNodeLiteralBool *>(&node) ||
        dynamic_cast<const AstNodeLiteralInt *>(&node) ||
        dynamic_cast<const AstNodeLiteralDecimal *>(&node) ||
        dynamic_cast<const AstNodeLiteralStr *>(&node) ||
        dynamic_cast<const AstNodeLiteralEllipsis *>(&node))
        return true;

    // 容器类（只有 tuple）的递归判断
    if (const auto *t{dynamic_cast<const AstNodeLiteralTuple *>(&node)})
        return std::ranges::all_of(t->items_, [](const AstNodePtr &item) {
            return is_literal_const(*item);
        });

    return false;
}

bool StaticEvaler::is_int(const AstNode &node) {
    return dynamic_cast<const AstNodeLiteralInt *>(&node);
}

bool StaticEvaler::is_int_family(const AstNode &node) {
    return is_int(node) || dynamic_cast<const AstNodeLiteralBool *>(&node);
}

bool StaticEvaler::is_numeric(const AstNode &node) {
    return is_int_family(node) || dynamic_cast<const AstNodeLiteralDecimal *>(&node);
}

std::optional<int64_t> StaticEvaler::node_to_int64(const AstNode &node) {
    assert(is_int_family(node));

    // bool 1/0
    if (const auto *b{dynamic_cast<const AstNodeLiteralBool *>(&node)}) return b->value_ ? 1 : 0;

    const auto &i{dynamic_cast<const AstNodeLiteralInt &>(node)};
    const std::u32string_view raw{i.raw_};
    const bool is_negative{!raw.empty() && raw[0] == U'-'};
    // raw_ 的形状已经在构造时校验过，这里不会真的触发内部报错
    const auto [mantissa, exponent]{
        strip_literal_exponent(strip_literal_sign(raw), false, 4, i.pos_)
    };

    std::u32string digits; // 展开后的数字字符串（无符号）
    if (exponent.empty()) {
        digits = std::u32string{mantissa};
    } else {
        // 特判1：尾数去掉符号后全是 0 -> 0
        if (std::ranges::all_of(mantissa, [](const char32_t c) { return c == U'0'; })) return 0;

        // 特判2：指数超过上限 -> nullopt
        size_t exponent_value{0};
        for (const char32_t c : exponent) exponent_value = exponent_value * 10 + (c - U'0');
        if (exponent_value > nMaxIntScientificExponent) return std::nullopt;

        // 1e5 -> "100000"：尾数后面补上指数份的 0
        digits = std::u32string{mantissa} + std::u32string(exponent_value, U'0');
    }

    std::string text{u32_to_utf8(digits)}; // 展开后的数字字符串
    if (is_negative) text.insert(text.begin(), '-');

    int64_t result{0};
    const char *begin{text.data()}, *end{begin + text.size()};
    if (const auto [ptr, ec]{std::from_chars(begin, end, result)}; ec != std::errc{} || ptr != end)
        return std::nullopt;
    return result;
}

AstNodePtr StaticEvaler::make_bool(const Position pos, const bool value) {
    return std::make_unique<AstNodeLiteralBool>(pos, value);
}

AstNodePtr StaticEvaler::make_int(const Position pos, const int64_t value) {
    return std::make_unique<AstNodeLiteralInt>(pos, utf8_to_u32(std::to_string(value)));
}

std::optional<bool> StaticEvaler::literal_equal(const AstNode &a, const AstNode &b) {
    assert(is_literal_pure(a) && is_literal_pure(b));

    // bool/int/decimal。只要有一侧是 decimal 就不折
    if (is_numeric(a) && is_numeric(b)) {
        if (!is_int_family(a) || !is_int_family(b)) return std::nullopt;
        const std::optional va{node_to_int64(a)}, vb{node_to_int64(b)};
        if (!va || !vb) return std::nullopt;
        return *va == *vb;
    }

    // None
    if (dynamic_cast<const AstNodeLiteralNone *>(&a) &&
        dynamic_cast<const AstNodeLiteralNone *>(&b))
        return true;

    // ...
    if (dynamic_cast<const AstNodeLiteralEllipsis *>(&a) &&
        dynamic_cast<const AstNodeLiteralEllipsis *>(&b))
        return true;

    // str
    if (const auto *sa{dynamic_cast<const AstNodeLiteralStr *>(&a)},
        *sb{dynamic_cast<const AstNodeLiteralStr *>(&b)};
        sa && sb) {
        return sa->value_ == sb->value_;
    }

    // tuple
    if (const auto *ta{dynamic_cast<const AstNodeLiteralTuple *>(&a)},
        *tb{dynamic_cast<const AstNodeLiteralTuple *>(&b)};
        ta && tb) {
        if (ta->items_.size() != tb->items_.size()) return false;
        for (size_t i{0}; i < ta->items_.size(); ++i) {
            const std::optional eq{literal_equal(*ta->items_[i], *tb->items_[i])};
            if (!eq) return std::nullopt; // 有一个元素判不了，整体就判不了
            if (!*eq) return false;
        }
        return true;
    }

    // list
    if (const auto *la{dynamic_cast<const AstNodeLiteralList *>(&a)},
        *lb{dynamic_cast<const AstNodeLiteralList *>(&b)};
        la && lb) {
        if (la->items_.size() != lb->items_.size()) return false;
        for (size_t i{0}; i < la->items_.size(); ++i) {
            const std::optional eq{literal_equal(*la->items_[i], *lb->items_[i])};
            if (!eq) return std::nullopt; // 同上
            if (!*eq) return false;
        }
        return true;
    }

    // 走到这里说明两边类型不同、且没有哪一方认识对方。
    // 按运算符重载一节 `==`/`!=` 的终局回退，`==` 此时取 `a is b` 即 False。
    return false;
}

std::partial_ordering StaticEvaler::literal_compare(const AstNode &a, const AstNode &b) {
    assert(is_literal_pure(a) && is_literal_pure(b));

    // bool/int/decimal，取值方式同 literal_equal，只要有一侧是 decimal 就不折。
    if (is_numeric(a) && is_numeric(b)) {
        if (!is_int_family(a) || !is_int_family(b)) return std::partial_ordering::unordered;
        const std::optional va{node_to_int64(a)}, vb{node_to_int64(b)};
        if (!va || !vb) return std::partial_ordering::unordered;
        return *va <=> *vb;
    }

    // str
    if (const auto *sa{dynamic_cast<const AstNodeLiteralStr *>(&a)},
        *sb{dynamic_cast<const AstNodeLiteralStr *>(&b)};
        sa && sb) {
        return sa->value_ <=> sb->value_;
    }

    // 工具函数：tuple/tuple、list/list 逐元素比较，字典序
    const auto lexicographic{
        [](const std::vector<AstNodePtr> &xa,
           const std::vector<AstNodePtr> &xb) -> std::partial_ordering {
            const size_t n{std::min(xa.size(), xb.size())};
            for (size_t i{0}; i < n; ++i) {
                if (const std::partial_ordering c{literal_compare(*xa[i], *xb[i])}; c != 0)
                    return c;
            }
            return xa.size() <=> xb.size();
        }
    };

    // tuple
    if (const auto *ta{dynamic_cast<const AstNodeLiteralTuple *>(&a)},
        *tb{dynamic_cast<const AstNodeLiteralTuple *>(&b)};
        ta && tb) {
        return lexicographic(ta->items_, tb->items_);
    }

    // list
    if (const auto *la{dynamic_cast<const AstNodeLiteralList *>(&a)},
        *lb{dynamic_cast<const AstNodeLiteralList *>(&b)};
        la && lb) {
        return lexicographic(la->items_, lb->items_);
    }

    // 类型不同、或类型本身不支持序比较（None/Ellipsis 等）。交给运行期报错
    return std::partial_ordering::unordered;
}

AstNodePtr StaticEvaler::clone_const_literal(const AstNode &node) {
    assert(is_literal_const(node));
    const Position pos{node.pos_};

    if (dynamic_cast<const AstNodeLiteralNone *>(&node))
        return std::make_unique<AstNodeLiteralNone>(pos);
    if (dynamic_cast<const AstNodeLiteralEllipsis *>(&node))
        return std::make_unique<AstNodeLiteralEllipsis>(pos);
    if (const auto *b{dynamic_cast<const AstNodeLiteralBool *>(&node)})
        return std::make_unique<AstNodeLiteralBool>(pos, b->value_);
    if (const auto *i{dynamic_cast<const AstNodeLiteralInt *>(&node)})
        return std::make_unique<AstNodeLiteralInt>(pos, i->raw_);
    if (const auto *d{dynamic_cast<const AstNodeLiteralDecimal *>(&node)})
        return std::make_unique<AstNodeLiteralDecimal>(pos, d->raw_);
    if (const auto *s{dynamic_cast<const AstNodeLiteralStr *>(&node)})
        return std::make_unique<AstNodeLiteralStr>(pos, s->value_);
    if (const auto *t{dynamic_cast<const AstNodeLiteralTuple *>(&node)}) {
        std::vector<AstNodePtr> items;
        items.reserve(t->items_.size());
        for (const AstNodePtr &item : t->items_) items.push_back(clone_const_literal(*item));
        return std::make_unique<AstNodeLiteralTuple>(pos, std::move(items));
    }

    return nullptr;
}

AstNodePtr StaticEvaler::fold(AstNode &node) {
    // 只有这几种节点才可能整体收缩成一个字面量
    if (const auto *n{dynamic_cast<AstNodeOpUnary *>(&node)}) return fold_unary(*n);
    if (auto *n{dynamic_cast<AstNodeOpBinary *>(&node)}) return fold_binary(*n);
    if (auto *n{dynamic_cast<AstNodeCompare *>(&node)}) return fold_compare(*n);
    if (auto *n{dynamic_cast<AstNodeIf *>(&node)}) return fold_if(*n);
    if (auto *n{dynamic_cast<AstNodeForCond *>(&node)}) return fold_for_cond(*n);
    if (auto *n{dynamic_cast<AstNodeCompound *>(&node)}) return fold_compound(*n);
    return nullptr;
}

void StaticEvaler::prune_program(AstNodeProgram &node) {
    std::vector<AstNodePtr> kept;
    for (auto &e : node.exprs_) {
        if (!is_literal_pure(*e)) kept.push_back(std::move(e));
    }
    node.exprs_ = std::move(kept);
}
