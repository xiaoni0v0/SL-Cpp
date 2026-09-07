#pragma once

#include "../../runtime/Object.h"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

/**
 * 常量表的去重池。
 *
 * 去重的判据是“结构标识”，见 structural_identical()。
 */
class ConstPool {
    // 常量表
    std::vector<ObjectRef> table_;

    // 结构哈希 -> 候选槽号。同一个桶里再逐个按 structural_identical() 确定
    std::unordered_map<std::size_t, std::vector<std::uint32_t>> buckets_;

    // value 在表里的那个规范对象。元组会先把元素逐个规范化，必要时重建
    [[nodiscard]] ObjectRef canonicalize(const ObjectRef &value);

  public:
    // 把 value 放进常量表，返回它的槽号；已经有结构上相同的常量就复用那个槽。
    [[nodiscard]] std::uint32_t intern(const ObjectRef &value);

    [[nodiscard]] const std::vector<ObjectRef> &table() const { return table_; }
    [[nodiscard]] std::uint32_t size() const { return static_cast<std::uint32_t>(table_.size()); }

    // 把整张表搬走，池子随之清空
    [[nodiscard]] std::vector<ObjectRef> take_table();

    // 两个常量该不该共用一个槽。
    // 调用方保证：元组的元素已经是表里的规范对象。
    [[nodiscard]] static bool structural_identical(const Object *lhs, const Object *rhs);

    // structural_identical 配套的哈希。
    [[nodiscard]] static std::size_t structural_hash(const Object *value);
};
