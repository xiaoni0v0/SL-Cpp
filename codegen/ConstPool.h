#pragma once

#include "../numeric/BigDec.h"
#include "../numeric/BigInt.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

// 单例常量的负载，只靠类型区分，没有值
struct ConstNone {};
struct ConstEllipsis {};

// 常量的种类。取值必须跟 ConstEntry::Value 的候选顺序一一对应，见文件末尾的 static_assert
enum class ConstKind : uint8_t { None, Bool, Int, Decimal, Str, Ellipsis, Tuple };

/**
 * 常量表里的一项：向 VM 描述 LOAD_CONST 该弄出个什么对象，本身不是运行期的 SL 对象。
 * 物化（描述 → 对象）由 VM 加载 Code 时统一做，见 bytecode.md。
 *
 * 元组存的是各元素在同一张表里的槽号，不是嵌套的描述树；配套不变量是子项槽号恒小于父项槽号，
 * 于是物化只要从 0 往后扫一遍，轮到元组时元素对象必然已经建好。
 */
struct ConstEntry {
    using Value = std::variant<
        ConstNone,            // None
        bool,                 // True / False
        BigInt,               // int
        BigDec,               // decimal
        std::u32string,       // str
        ConstEllipsis,        // ...
        std::vector<uint32_t> // 元组，各元素的槽号
        >;

    Value value_;

    [[nodiscard]] ConstKind kind() const { return static_cast<ConstKind>(value_.index()); }

    // 取负载。调用方保证 kind() 对得上
    template <typename T> [[nodiscard]] const T &as() const { return std::get<T>(value_); }

    /**
     * 结构标识相等 —— 去重用的判据，跟 SL 的 == 不是一回事：
     *   - 种类不同一律不等（1 与 True、1 与 1.0 都 == 为真，但必须是不同的槽，
     *     否则 `True is 1` 会变成真）；
     *   - decimal 逐位比（1.5 与 1.50、0 与 -0 都 == 为真而 str 分得出来，不能合并）；
     *   - 元组比子项槽号序列，不递归比值。子项已经规范化过，既便宜又正好绕开上面两条。
     */
    [[nodiscard]] bool identical(const ConstEntry &rhs) const;

    // identical 意义下的哈希：相等必同值，不等允许撞
    [[nodiscard]] size_t structural_hash() const;
};

/**
 * 一份 Code 的常量表，边加边按结构去重（hash consing）：
 * 值相等的常量恒落在同一个槽，因而物化后恒是同一个对象——`a = (1, 2)` 与 `b = (1, 2)` 的 `a is b`
 * 为真。
 */
class ConstPool {
    std::vector<ConstEntry> entries_;
    // structural_hash -> 候选槽号。撞了再逐个 identical 定案
    std::unordered_map<size_t, std::vector<uint32_t>> buckets_;

    [[nodiscard]] uint32_t intern(ConstEntry entry);

  public:
    [[nodiscard]] uint32_t add_none();
    [[nodiscard]] uint32_t add_bool(bool value);
    [[nodiscard]] uint32_t add_int(BigInt value);
    [[nodiscard]] uint32_t add_decimal(BigDec value);
    [[nodiscard]] uint32_t add_str(std::u32string value);
    [[nodiscard]] uint32_t add_ellipsis();
    // items 是各元素的槽号，调用方保证它们都已经在本表里（于是子项槽号必然小于本项）
    [[nodiscard]] uint32_t add_tuple(std::vector<uint32_t> items);

    [[nodiscard]] const std::vector<ConstEntry> &entries() const { return entries_; }
    [[nodiscard]] size_t size() const { return entries_.size(); }

    // 交出建好的表，之后本对象不该再用
    [[nodiscard]] std::vector<ConstEntry> take();
};

// ConstKind 与 variant 候选顺序必须一致，kind() 直接拿 index() 当种类
template <ConstKind kind, typename T>
inline constexpr bool kind_matches_alternative =
    std::is_same_v<std::variant_alternative_t<static_cast<size_t>(kind), ConstEntry::Value>, T>;

static_assert(kind_matches_alternative<ConstKind::None, ConstNone>);
static_assert(kind_matches_alternative<ConstKind::Bool, bool>);
static_assert(kind_matches_alternative<ConstKind::Int, BigInt>);
static_assert(kind_matches_alternative<ConstKind::Decimal, BigDec>);
static_assert(kind_matches_alternative<ConstKind::Str, std::u32string>);
static_assert(kind_matches_alternative<ConstKind::Ellipsis, ConstEllipsis>);
static_assert(kind_matches_alternative<ConstKind::Tuple, std::vector<uint32_t>>);
static_assert(std::variant_size_v<ConstEntry::Value> == static_cast<size_t>(ConstKind::Tuple) + 1);
