#include "ConstPool.h"

#include <bit>
#include <cassert>
#include <utility>

namespace {

size_t hash_mix(const size_t seed, const size_t value) {
    return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
}

// 便宜且够散：小整数各不相同（to_double 精确），大整数靠位数区分
size_t hash_bigint(const BigInt &value) {
    size_t h{static_cast<size_t>(value.sign() + 1)};
    h = hash_mix(h, value.bit_length());
    return hash_mix(h, std::bit_cast<uint64_t>(value.to_double()));
}

// 逐位而非按值：1.5 与 1.50 的 exp 不同，必须落进不同的桶
size_t hash_bigdec(const BigDec &value) {
    size_t h{static_cast<size_t>(value.kind())};
    h = hash_mix(h, static_cast<size_t>(value.is_negative()));
    h = hash_mix(h, static_cast<size_t>(value.exponent()));
    return hash_mix(h, hash_bigint(value.coefficient()));
}

} // namespace

bool ConstEntry::identical(const ConstEntry &rhs) const {
    if (value_.index() != rhs.value_.index()) return false;

    switch (kind()) {
    case ConstKind::None:
    case ConstKind::Ellipsis:
        return true;
    case ConstKind::Bool:
        return as<bool>() == rhs.as<bool>();
    case ConstKind::Int:
        return as<BigInt>() == rhs.as<BigInt>();
    case ConstKind::Decimal:
        // 不能用 BigDec::operator==，那是按默认上下文的数值相等
        return as<BigDec>().identical(rhs.as<BigDec>());
    case ConstKind::Str:
        return as<std::u32string>() == rhs.as<std::u32string>();
    case ConstKind::Tuple:
        return as<std::vector<uint32_t>>() == rhs.as<std::vector<uint32_t>>();
    }
    return false;
}

size_t ConstEntry::structural_hash() const {
    const size_t seed{static_cast<size_t>(kind())};

    switch (kind()) {
    case ConstKind::None:
    case ConstKind::Ellipsis:
        return seed;
    case ConstKind::Bool:
        return hash_mix(seed, static_cast<size_t>(as<bool>()));
    case ConstKind::Int:
        return hash_mix(seed, hash_bigint(as<BigInt>()));
    case ConstKind::Decimal:
        return hash_mix(seed, hash_bigdec(as<BigDec>()));
    case ConstKind::Str:
        return hash_mix(seed, std::hash<std::u32string>{}(as<std::u32string>()));
    case ConstKind::Tuple: {
        size_t h{seed};
        for (const uint32_t item : as<std::vector<uint32_t>>()) h = hash_mix(h, item);
        return h;
    }
    }
    return seed;
}

uint32_t ConstPool::intern(ConstEntry entry) {
    const size_t hash{entry.structural_hash()};
    auto &bucket = buckets_[hash];
    for (const uint32_t slot : bucket)
        if (entries_[slot].identical(entry)) return slot;

    const auto slot{static_cast<uint32_t>(entries_.size())};
    entries_.push_back(std::move(entry));
    bucket.push_back(slot);
    return slot;
}

uint32_t ConstPool::add_none() { return intern(ConstEntry{ConstNone{}}); }

uint32_t ConstPool::add_bool(const bool value) { return intern(ConstEntry{value}); }

uint32_t ConstPool::add_int(BigInt value) { return intern(ConstEntry{std::move(value)}); }

uint32_t ConstPool::add_decimal(BigDec value) { return intern(ConstEntry{std::move(value)}); }

uint32_t ConstPool::add_str(std::u32string value) { return intern(ConstEntry{std::move(value)}); }

uint32_t ConstPool::add_ellipsis() { return intern(ConstEntry{ConstEllipsis{}}); }

uint32_t ConstPool::add_tuple(std::vector<uint32_t> items) {
    // 子项必须已经在表里，这样才有「子项槽号 < 父项槽号」
    for ([[maybe_unused]] const uint32_t item : items) assert(item < entries_.size());

    return intern(ConstEntry{std::move(items)});
}

std::vector<ConstEntry> ConstPool::take() {
    buckets_.clear();
    return std::move(entries_);
}
