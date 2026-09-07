#pragma once

#include "../../runtime/Object.h"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

/**
 * 常量表的去重池。codegen 一边编译一边往里塞常量，编完把整张表交给 Code::Parts。
 *
 * 值相等的常量恒占同一个槽、因而恒是同一个对象，于是 `a = (1, 2)` / `b = (1, 2)` 的 `a is b`
 * 为真；顶层的 `(1,2)` 和别处 `((1,2),3)` 里的那个 `(1,2)` 也是同一个对象。
 *
 * **去重的判据是「结构标识」，跟 SL 的 `==`/`hash` 是两回事，好些地方还正好相反**，见
 * identical()。放在 codegen 而不是 runtime，正是为了不让它跟对象模型里的相等混在一起。
 *
 * GC：池子里那些 ObjectRef 不在根集合里。codegen 全程不允许触发回收（`Heap::collect()` 的
 * 安全点前提），所以这里不需要额外注册根源；等表交给 Code、Code 被函数/模块对象持有之后，
 * 它们才重新变成标记器扫得到的东西。
 */
class ConstPool {
    std::vector<ObjectRef> table_;
    // 结构哈希 -> 候选槽号。同一个桶里再逐个按 identical() 定案。
    // 不做线性扫：机器生成的 SL 一份 Code 里出现上万个常量并非不可能
    std::unordered_map<std::size_t, std::vector<std::uint32_t>> buckets_;

    // value 在表里的那个规范对象。元组会先把元素逐个规范化，必要时重建
    [[nodiscard]] ObjectRef canonical(const ObjectRef &value);

  public:
    /**
     * 把 value 放进常量表，返回它的槽号；已经有结构上相同的常量就复用那个槽。
     *
     * 元组是自底向上处理的：先把每个元素 intern 掉、换成表里的规范对象，再拿这些对象的身份
     * 当键去 intern 元组自己。调用方不需要自己保证元素已经进过表。
     *
     * value 必须是能进常量表的类型（`None`/`bool`/`int`/`decimal`/`str`/`Ellipsis`/全常量元组），
     * 否则抛 InternalError——那意味着 codegen 自己判错了什么是常量。
     */
    [[nodiscard]] std::uint32_t intern(const ObjectRef &value);

    [[nodiscard]] const std::vector<ObjectRef> &table() const { return table_; }
    [[nodiscard]] std::uint32_t size() const { return static_cast<std::uint32_t>(table_.size()); }

    // 交出整张表。一次性操作，交完这个池就不该再用了
    [[nodiscard]] std::vector<ObjectRef> take_table() &&;

    /**
     * 两个常量该不该共用一个槽。**不是 SL 的 `==`**，三条规则都容易写反：
     *
     * - **先比类型**：`1` 和 `True`、`1` 和 `1.0` 在 SL 里 `==` 为真且 hash 相同，但必须是不同的
     *   槽，否则 `True is 1` 会变成真。CPython 的 `co_consts` 把类型编进 key 就是为了这个；
     * - **`decimal` 逐位比**（符号/系数/指数），用 `BigDec::identical()`。**绝不能用
     *   `BigDec::operator==`**——那是按默认上下文的数值相等，`1.5 == 1.50` 为真、`NaN == NaN`
     *   为假，两边都跟这里要的相反；
     * - **元组比元素的对象身份**，不递归比值。元素都已经规范化过，所以这既便宜又正好绕开前两条坑。
     *
     * 调用方保证：两边都是能进常量表的类型，且元组的元素已经是表里的规范对象。
     */
    [[nodiscard]] static bool identical(const Object *lhs, const Object *rhs);

    // identical() 配套的哈希：identical 为真的两个值，这里必须给出同一个结果
    [[nodiscard]] static std::size_t structural_hash(const Object *value);
};
