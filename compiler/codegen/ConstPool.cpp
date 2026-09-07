#include "ConstPool.h"

#include "../../cppexceptions/InternalError.h"
#include "../../runtime/Runtime.h"
#include "../../runtime/objects/Decimal.h"
#include "../../runtime/objects/Int.h"
#include "../../runtime/objects/Str.h"
#include "../../runtime/objects/Tuple.h"

#include <bit>
#include <concepts>
#include <format>
#include <functional>
#include <optional>
#include <utility>

namespace {

// 把若干个哈希值揉进 seed，返回揉好的那个。Boost 经典配方
template <std::convertible_to<std::size_t>... Values>
std::size_t hash_combine(std::size_t seed, const Values... values) {
    static constexpr std::size_t kGolden{0x9e3779b97f4a7c15ULL};

    ((seed ^= static_cast<std::size_t>(values) + kGolden + (seed << 6) + (seed >> 2)), ...);

    return seed;
}

std::size_t hash_pointer(const void *pointer) { return std::hash<const void *>{}(pointer); }

// 眼下没有现成的，按「符号 + 位长 + 转成 double 的位模式」凑一个。
// TODO: BigInt 有了自己的哈希之后，换成用 BigInt / BigDec 自己的
std::size_t hash_bigint(const BigInt &value) {
    return hash_combine(
        value.sign() + 1, value.bit_length(), std::bit_cast<std::uint64_t>(value.to_double())
    );
}

// 元组的结构哈希只取决于类型和元素的对象身份
std::size_t hash_tuple(const Type *type, const std::vector<ObjectRef> &items) {
    std::size_t hash{hash_combine(hash_pointer(type), items.size())};
    for (const ObjectRef &item : items) hash = hash_combine(hash, hash_pointer(item.target()));

    return hash;
}

// 这个对象能不能进常量表。判据是不可变，见 bytecode.md「常量表存什么」
bool can_be_constant(const Object *value) {
    const Type *type{value->type()};

    return type == Runtime::type_none_type() || type == Runtime::type_singleton_type() ||
           type == Runtime::type_bool() || type == Runtime::type_int() ||
           type == Runtime::type_decimal() || type == Runtime::type_str() ||
           type == Runtime::type_tuple();
}

} // namespace

std::optional<std::uint32_t>
ConstPool::find_tuple(const Type *type, const std::vector<ObjectRef> &items) const {
    const auto bucket{buckets_.find(hash_tuple(type, items))};
    if (bucket == buckets_.end()) return std::nullopt;

    for (const std::uint32_t slot : bucket->second) {
        if (const auto other{dynamic_cast<const Tuple *>(table_[slot].target())};
            other && other->type() == type && other->items() == items)
            return slot;
    }

    return std::nullopt;
}

ObjectRef ConstPool::canonicalize(const ObjectRef &value) {
    const auto tuple{dynamic_cast<const Tuple *>(value.target())};
    if (!tuple) return value;

    // 先让每个元素进表
    std::vector<ObjectRef> items;
    items.reserve(tuple->size());
    bool changed{false};
    for (std::size_t i{0}; i < tuple->size(); ++i) {
        Object *item{tuple->at(i)};
        const std::uint32_t slot{intern(ObjectRef{item})};
        if (table_[slot] != item) changed = true;
        items.push_back(table_[slot]);
    }

    // 没变过，元素本来就都是规范对象
    if (!changed) return value;

    // 元素定了，这个元组的结构标识也就定了。表里已经有一个就直接用那个
    if (const std::optional found{find_tuple(tuple->type(), items)}) {
        return table_[*found];
    }

    return ObjectRef{make_ref<Tuple>(tuple->type(), std::move(items))};
}

std::uint32_t ConstPool::intern(const ObjectRef &value) {
    if (!value) throw InternalError{"ConstPool: interning a null constant"};
    if (!can_be_constant(value.target())) {
        throw InternalError{
            std::format("ConstPool: '{}' cannot go into the constant table", value->type()->name())
        };
    }

    // 先规范化再取桶
    const ObjectRef entry{canonicalize(value)};
    const std::size_t hash{structural_hash(entry.target())};

    std::vector<std::uint32_t> &bucket{buckets_[hash]};
    for (const std::uint32_t slot : bucket) {
        if (structural_identical(table_[slot].target(), entry.target())) return slot;
    }

    const auto slot{static_cast<std::uint32_t>(table_.size())};
    table_.push_back(entry);
    bucket.push_back(slot);

    return slot;
}

std::vector<ObjectRef> ConstPool::take_table() {
    std::vector table{std::move(table_)};
    table_.clear();
    buckets_.clear();

    return table;
}

bool ConstPool::structural_identical(const Object *lhs, const Object *rhs) {
    if (lhs == rhs) return true;

    // 先比类型
    if (lhs->type() != rhs->type()) return false;

    // 再比值
    if (const auto left{dynamic_cast<const Int *>(lhs)}, right{dynamic_cast<const Int *>(rhs)};
        left && right) {
        return left->value().equals(right->value());
    }
    if (const auto left{dynamic_cast<const Decimal *>(lhs)},
        right{dynamic_cast<const Decimal *>(rhs)};
        left && right) {
        return left->value().identical(right->value()); // 不能用 operator==
    }
    if (const auto left{dynamic_cast<const Str *>(lhs)}, right{dynamic_cast<const Str *>(rhs)};
        left && right) {
        return left->value() == right->value();
    }
    if (const auto left{dynamic_cast<const Tuple *>(lhs)}, right{dynamic_cast<const Tuple *>(rhs)};
        left && right) {
        if (left->size() != right->size()) return false;
        for (std::size_t i{0}; i < left->size(); ++i) {
            if (left->at(i) != right->at(i)) return false;
        }
        return true;
    }

    return false;
}

std::size_t ConstPool::structural_hash(const Object *value) {
    // 类型编进 key，跟 structural_identical() 的第一条对齐
    const std::size_t seed{hash_pointer(value->type())};

    if (const auto number{dynamic_cast<const Int *>(value)}) {
        return hash_combine(seed, hash_bigint(number->value()));
    }
    if (const auto number{dynamic_cast<const Decimal *>(value)}) {
        const BigDec &decimal{number->value()};
        // kind + 符号 + 指数 + 系数，正是 BigDec::identical() 比的那四个字段
        return hash_combine(
            seed,
            static_cast<std::size_t>(decimal.kind()),
            decimal.is_negative() ? 1u : 0u,
            static_cast<std::size_t>(decimal.exponent()),
            hash_bigint(decimal.coefficient())
        );
    }
    if (const auto str{dynamic_cast<const Str *>(value)}) {
        return hash_combine(seed, std::hash<std::u32string>{}(str->value()));
    }
    if (const auto tuple{dynamic_cast<const Tuple *>(value)}) {
        return hash_tuple(tuple->type(), tuple->items());
    }

    // 单例，身份就是全部信息
    return hash_combine(seed, hash_pointer(value));
}
