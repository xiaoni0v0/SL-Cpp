#pragma once

#include "../../runtime/Object.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

/**
 * 常量表的去重池。
 *
 * 去重的判据是“结构标识”，见 ConstPool.cpp 里的 structural_identical()。
 */
class ConstPool final {
    // 常量表
    std::vector<ObjectRef> table_;

    // 结构哈希 -> 候选槽号。同一个桶里再逐个按结构标识确定
    std::unordered_map<std::size_t, std::vector<std::uint32_t>> buckets_;

    // 表里有没有一个元素恰好是 items 的元组。
    // 调用方保证：元组的元素已经是表里的规范对象。
    [[nodiscard]] std::optional<std::uint32_t>
    find_tuple(const Type *type, const std::vector<ObjectRef> &items) const;

    // 返回 value 在表里的那个规范对象。
    // 当且仅当是元组时会先把元素逐个规范化。
    [[nodiscard]] ObjectRef canonicalize(const ObjectRef &value);

  public:
    // 把 value 放进常量表，返回它的槽号；已经有结构上相同的常量就复用那个槽。
    [[nodiscard]] std::uint32_t intern(const ObjectRef &value);

    // 把整张表搬走，池子随之清空
    [[nodiscard]] std::vector<ObjectRef> take_table();
};
