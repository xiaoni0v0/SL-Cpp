#include "StaticEvaler.h"

#include "../../utils/string_utils.h"

#include <algorithm>
#include <cassert>
#include <charconv>
#include <stdckdint.h>
#include <string>

namespace {

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

    // 确定为全是 True
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
    const std::optional cond_truthy{truthy(*node.cond_)};
    if (!cond_truthy || *cond_truthy) return nullptr;

    // $$ 一轮没跑的值是个空 dict，不折
    if (node.collect_.container_ == CollectMark::Container::Dict) return nullptr;

    AstNodePtr result{
        node.collect_.container_ == CollectMark::Container::List
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

    // 非负 int 才有定义
    const std::optional count_value{node_to_bigint(count_node)};
    if (!count_value || count_value->is_negative()) return nullptr;

    // 现在，挑出来数字 count_node，另一个 container_node 不知道是啥

    // 'a' * 3
    if (const auto *s{dynamic_cast<const AstNodeLiteralStr *>(&container_node)}) {
        // 空串重复多少次都还是空串，直接给结果。这条必须走在下面的上限检查前面：空串的
        // 0 * n 恒不超限，落进拼接循环就是白跑 n 次，而 n 能大到没边，等于编译期挂死
        if (s->value_.empty()) return std::make_unique<AstNodeLiteralStr>(node.pos_, U"");

        // 次数大到装不下 int64_t 的，非空串必然超长度上限，直接不折
        const std::optional count{bigint_to_int64(*count_value)};
        if (!count) return nullptr;
        const size_t n{static_cast<size_t>(*count)};

        if (mul_exceeds(s->value_.size(), n, nMaxStrLength)) return nullptr;

        std::u32string value;
        value.reserve(s->value_.size() * n);
        for (size_t i{0}; i < n; ++i) value += s->value_;
        return std::make_unique<AstNodeLiteralStr>(node.pos_, std::move(value));
    }

    // (a, b) * 3、[a, b] * 3：恒不折
    return nullptr;
}

AstNodePtr StaticEvaler::fold_arithmetic(const AstNodeOpUnary &node) {
    using enum AstNodeOpUnary::OpType;
    const AstNode &operand{*node.operand_};

    if (!(is_literal_pure(operand) && is_int_family(operand))) return nullptr;

    const std::optional v{node_to_bigint(operand)};
    if (!v) return nullptr;
    return make_int(node.pos_, node.op_ == Pos ? v->plus() : v->minus());
}

AstNodePtr StaticEvaler::fold_arithmetic(const AstNodeOpBinary &node) {
    using enum AstNodeOpBinary::OpType;
    const AstNode &l{*node.left_}, &r{*node.right_};
    if (!is_literal_pure(l) || !is_literal_pure(r)) return nullptr;

    // 任何一侧是 decimal（或 `/`，其结果恒为 decimal）都不折：结果按运行期上下文舍入，
    // 编译期不知道那时的 prec/rounding。比较不走这里，它在 fold_compare 里，照折
    if (!is_int_family(l) || !is_int_family(r) || node.op_ == Div) return nullptr;

    const std::optional lv{node_to_bigint(l)}, rv{node_to_bigint(r)};
    if (!lv || !rv) return nullptr;

    const size_t ld{lv->num_decimal_digits()}, rd{rv->num_decimal_digits()};

    switch (node.op_) {
    // +/- 至多让位数多一位，不用卡上限
    case Add:
        return make_int(node.pos_, lv->add(*rv));
    case Sub:
        return make_int(node.pos_, lv->sub(*rv));

    // 位数相加，会爆，事先卡
    case Mul:
        if (sum_exceeds(ld, rd, nMaxIntDigits)) return nullptr;
        return make_int(node.pos_, lv->mul(*rv));

    // 除零是 MathError（SL.md 3.4.2），原样留给运行期；商和余数都只会变小，不用卡上限
    case DivFloor:
        if (rv->is_zero()) return nullptr;
        return make_int(node.pos_, lv->floor_div(*rv));
    case Mod:
        if (rv->is_zero()) return nullptr;
        return make_int(node.pos_, lv->mod(*rv));

    case Pow: {
        // 指数为负时结果是 decimal（3 ** -1 == 0.333...），落进上面那条 decimal 禁令
        if (rv->is_negative()) return nullptr;
        // BigInt::pow 自己不设上限，指数大了会一路算到跑不完，必须事先估：
        // 结果位数 ≈ 底数位数 × 指数，先要求指数本身是个小整数，再卡乘积
        const std::optional exp{bigint_to_int64(*rv)};
        if (!exp) return nullptr;
        if (mul_exceeds(ld, static_cast<size_t>(*exp), nMaxIntDigits)) return nullptr;
        return make_int(node.pos_, lv->pow(*rv));
    }

    default:
        return nullptr;
    }
}

AstNodePtr StaticEvaler::fold_bitwise(const AstNodeOpUnary &node) {
    const AstNode &operand{*node.operand_};
    // 位运算只对 int 有定义：bool 没有位运算方法（SL.md 4.2.5），~True 运行期是 TypeError，
    // 所以这里用严格的 is_int 而不是 is_int_family
    if (!is_literal_pure(operand) || !is_int(operand)) return nullptr;

    const std::optional v{node_to_bigint(operand)};
    if (!v) return nullptr;
    return make_int(node.pos_, v->bit_not()); // ~x == -x-1，位数至多多一位，不用卡上限
}

AstNodePtr StaticEvaler::fold_bitwise(const AstNodeOpBinary &node) {
    using enum AstNodeOpBinary::OpType;
    const AstNode &l{*node.left_}, &r{*node.right_};
    // 同上，两侧都必须是严格的 int
    if (!is_literal_pure(l) || !is_literal_pure(r) || !is_int(l) || !is_int(r)) return nullptr;

    const std::optional lv{node_to_bigint(l)}, rv{node_to_bigint(r)};
    if (!lv || !rv) return nullptr;

    switch (node.op_) {
    // & | ^ 的结果不会比两个操作数里长的那个更长，不用卡上限
    case BitAnd:
        return make_int(node.pos_, lv->bit_and(*rv));
    case BitOr:
        return make_int(node.pos_, lv->bit_or(*rv));
    case BitXor:
        return make_int(node.pos_, lv->bit_xor(*rv));

    case LShift: {
        // 负移位量的语义还没拍板（见 .ai/context.md），不折，留给运行期
        if (rv->is_negative()) return nullptr;
        const std::optional k{bigint_to_int64(*rv)};
        if (!k) return nullptr;
        // 左移 k 位最多让十进制位数多 k * log10(2) 位，用 0.302 > log10(2) 取个上界
        if (sum_exceeds(
                lv->num_decimal_digits(), static_cast<size_t>(*k) * 302 / 1000 + 1, nMaxIntDigits
            ))
            return nullptr;
        return make_int(node.pos_, lv->shift_left(*k));
    }
    case RShift: {
        if (rv->is_negative()) return nullptr; // 同上
        // 移位量大到装不下 int64_t 时，结果必然是 0 或 -1（算术右移补符号位），直接给出来
        const std::optional k{bigint_to_int64(*rv)};
        if (!k) return make_int(node.pos_, BigInt{lv->is_negative() ? -1 : 0});
        return make_int(node.pos_, lv->shift_right(*k)); // 只会变小，不用卡上限
    }

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

    // int/decimal 都必须按**值**判零，不能看 raw_ 的字面长相：1e2 和 0e0 都是合法写法，前者非零、
    // 后者是零，光比字符串一个都判不对。取不出值就返回 nullopt（"判不了"），绝不能默认成真——
    // 这个函数的返回值会决定死分支消除留哪一支，猜错就是静默改掉程序语义
    if (is_int_family(literal)) {
        const std::optional v{node_to_bigint(literal)};
        if (!v) return std::nullopt;
        return !v->is_zero();
    }
    if (is_numeric(literal)) { // 走到这里只剩 decimal
        const std::optional v{node_to_bigdec(literal)};
        if (!v) return std::nullopt;
        return !v->is_zero(); // 负零与 0 数值相等，is_zero 已经覆盖
    }

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

bool StaticEvaler::is_int(const AstNode &node) {
    return dynamic_cast<const AstNodeLiteralInt *>(&node);
}

bool StaticEvaler::is_int_family(const AstNode &node) {
    return is_int(node) || dynamic_cast<const AstNodeLiteralBool *>(&node);
}

bool StaticEvaler::is_numeric(const AstNode &node) {
    return is_int_family(node) || dynamic_cast<const AstNodeLiteralDecimal *>(&node);
}

std::optional<BigInt> StaticEvaler::node_to_bigint(const AstNode &node) {
    assert(is_int_family(node));

    // bool 1/0，之后一律复用 int 的实现
    if (const auto *b{dynamic_cast<const AstNodeLiteralBool *>(&node)})
        return BigInt{b->value_ ? 1 : 0};

    // int。科学计数法写法（1e9）由 BigInt 自己按值展开，这里不碰 raw_ 的字符串形状
    const auto &i{dynamic_cast<const AstNodeLiteralInt &>(node)};
    try {
        return BigInt::from_decimal_string(u32_to_utf8(i.raw_));
    } catch (const std::invalid_argument &) {
        return std::nullopt; // 形状本该由节点构造函数保证，这里兜底：折不动而不是抛出去
    }
}

std::optional<BigDec> StaticEvaler::node_to_bigdec(const AstNode &node) {
    assert(is_numeric(node));

    // int/bool 精确提升成 decimal（SL.md 4.2.6：提升不舍入）
    if (is_int_family(node)) {
        const std::optional v{node_to_bigint(node)};
        if (!v) return std::nullopt;
        return BigDec::from_bigint(*v);
    }

    const auto &d{dynamic_cast<const AstNodeLiteralDecimal &>(node)};
    std::optional value{BigDec::try_from_string(u32_to_utf8(d.raw_))};
    // 超出 BigDec 表示范围的字面量（指数 lexer 不设上限）返回 nullopt；
    // inf/NaN 这里也一并挡掉——它们参与序比较会触发信号，不该在编译期替运行期做决定
    if (!value || !value->is_finite()) return std::nullopt;
    return value;
}

std::optional<int64_t> StaticEvaler::bigint_to_int64(const BigInt &value) {
    // 位数先粗筛，够小了再按十进制串精确转
    if (value.num_decimal_digits() > 18) return std::nullopt;
    const std::string text{value.to_decimal_string()};
    int64_t result{0};
    const char *begin{text.data()}, *end{begin + text.size()};
    if (const auto [ptr, ec]{std::from_chars(begin, end, result)}; ec != std::errc{} || ptr != end)
        return std::nullopt;
    return result;
}

AstNodePtr StaticEvaler::make_bool(const Position pos, const bool value) {
    return std::make_unique<AstNodeLiteralBool>(pos, value);
}

AstNodePtr StaticEvaler::make_int(const Position pos, const BigInt &value) {
    return std::make_unique<AstNodeLiteralInt>(pos, utf8_to_u32(value.to_decimal_string()));
}

std::optional<bool> StaticEvaler::literal_equal(const AstNode &a, const AstNode &b) {
    assert(is_literal_pure(a) && is_literal_pure(b));

    // bool/int/decimal。两边都是 int 家族时用 BigInt 比（精确且便宜），只要有一侧是 decimal
    // 就统一抬到 BigDec 比——int 提升成 decimal 是精确的，所以 1 == 1.0 这种跨类型比较也准
    if (is_numeric(a) && is_numeric(b)) {
        if (is_int_family(a) && is_int_family(b)) {
            const std::optional va{node_to_bigint(a)}, vb{node_to_bigint(b)};
            if (!va || !vb) return std::nullopt;
            return va->equals(*vb);
        }
        const std::optional va{node_to_bigdec(a)}, vb{node_to_bigdec(b)};
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

    // 走到这里说明两边类型不同、且没有哪一方认识对方。按 SL.md 3.8 的终局回退，`==` 此时取
    // `a is b`——而不同类型的两个字面量不可能是同一个对象，所以恒为 False。这不是近似，是精确结果。
    // 注意这条兜底只有 `==`/`!=` 有；序比较两侧都弃权是 TypeError，见 literal_compare 末尾
    return false;
}

std::partial_ordering StaticEvaler::literal_compare(const AstNode &a, const AstNode &b) {
    assert(is_literal_pure(a) && is_literal_pure(b));

    // bool/int/decimal，取值方式同 literal_equal。取不出值时返回 unordered，
    // 跟"这两个类型本来就不支持比大小"归成同一个出口——调用方对两者的处理都是不折
    if (is_numeric(a) && is_numeric(b)) {
        if (is_int_family(a) && is_int_family(b)) {
            const std::optional va{node_to_bigint(a)}, vb{node_to_bigint(b)};
            if (!va || !vb) return std::partial_ordering::unordered;
            return va->compare_ordering(*vb); // 隐式转成 partial_ordering
        }
        const std::optional va{node_to_bigdec(a)}, vb{node_to_bigdec(b)};
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

    // 类型不同、或类型本身不支持序比较（None/Ellipsis 等）。序比较没有 `==`/`!=` 那条按身份
    // 兜底的规则（SL.md 3.8），两侧都弃权就是运行期 TypeError——所以这里只能不折，交给运行期报
    return std::partial_ordering::unordered;
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
