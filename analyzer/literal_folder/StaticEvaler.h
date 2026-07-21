#pragma once

#include "../../parser/ast_nodes/ast_nodes.h"
#include "../../numeric/BigInt.h"

#include <optional>


/**
 * 编译期静态求值器
 * 折不动一律返回 nullptr，从不抛异常
 *
 * 组织方式：
 * 按运算符的语义家族分组（算术/位运算/比较/逻辑/is），每组内部再按需要覆盖到的具体类型分支，
 * 大部分算术/比较运算符只是数值提升（bool -> int -> float）+ 少数几个容器类型（str/tuple/list/dict）各自的分支，
 *
 * 入参统一用非 const 引用（而不是指针）：调用方（LiteralFolder）保证传进来的节点非空，分派内部
 * 需要裸指针做 dynamic_cast 试探时自己取地址即可；用非 const 是因为 and/or 折叠不产出新值，而是把
 * 左操作数或右操作数原封不动地"挪走"当结果用（见 fold_and_or），这需要能 std::move 走某个子节点——
 * 除此之外的所有分支都只读，只是顺带能拿这个非 const 权限，不代表它们会去改 node。
 */
class StaticEvaler {
    // 尝试把 node 折成一个字面量节点；这里不负责递归。
    // 返回 nullptr 表示折不动。
    [[nodiscard]] static AstNodePtr fold_unary(const AstNodeOpUnary &node); // 一元
    [[nodiscard]] static AstNodePtr fold_not(const AstNodeOpUnary &node); // 一元 not
    [[nodiscard]] static AstNodePtr fold_pos_neg_bitnot(const AstNodeOpUnary &node); // 一元 + - ~
    [[nodiscard]] static AstNodePtr fold_binary(AstNodeOpBinary &node); // 二元
    [[nodiscard]] static AstNodePtr fold_compare(const AstNodeCompare &node); // 二元比较
    [[nodiscard]] static AstNodePtr fold_is(const AstNodeIs &node); // 二元 is

    // fold_binary 按运算符语义家族分派到这几个：
    [[nodiscard]] static AstNodePtr fold_arithmetic(const AstNodeOpBinary &node); // 纯数值二元算术：- / // % **
    [[nodiscard]] static AstNodePtr fold_add(const AstNodeOpBinary &node); // +：数值相加，或 str/tuple/list 各自的拼接
    [[nodiscard]] static AstNodePtr fold_mul(const AstNodeOpBinary &node); // *：数值相乘，或 (str/tuple/list, 非负 int) 的重复
    [[nodiscard]] static AstNodePtr fold_mod(const AstNodeOpBinary &node); // %：先试 str 的格式化，不是 str 再退回数值取模
    [[nodiscard]] static AstNodePtr fold_str_format(const AstNodeOpBinary &node); // %：str 的格式化
    [[nodiscard]] static AstNodePtr fold_bitwise(const AstNodeOpBinary &node); // & ^ << >>，仅 bool/int。 | 单独处理
    [[nodiscard]] static AstNodePtr fold_bitor(const AstNodeOpBinary &node); // 
    [[nodiscard]] static AstNodePtr fold_dict_merge(const AstNodeOpBinary &node);
    [[nodiscard]] static AstNodePtr fold_and_or(AstNodeOpBinary &node);

    // 真值判断：not/and/or 共用。每种字面量类型都有明确规则（None/False/0/0.0/空串/空容器为假，
    // 其余为真，见 SL.md 3.2），要求 node 已经是字面量节点（不是字面量就没法判断，调用方保证）
    [[nodiscard]] static bool truthy(const AstNode &literal);

    // node 是不是一个"整体已知"的字面量：None/bool/int/float/str/Ellipsis 天然是；
    // tuple/list 要求每个元素递归满足；dict 要求每个 key、非空 val 递归满足。
    // _G/_L 不算——它们的值是运行时的实时字典视图，不是编译期能确定的值（见 SL.md 3.4.1）
    [[nodiscard]] static bool is_literal(const AstNode &node);

    // ---- 数值提升相关 ----
    [[nodiscard]] static bool is_int_family(const AstNode &node); // bool 或 int
    [[nodiscard]] static bool is_numeric(const AstNode &node); // is_int_family 或 float
    [[nodiscard]] static BigInt to_bigint(const AstNode &node); // 要求 is_int_family(node)
    [[nodiscard]] static double to_double(const AstNode &node); // 要求 is_numeric(node)
    // BigInt 转 long long，装不下（也包括本来就不合法的场景）返回 false，绝不抛异常
    [[nodiscard]] static bool try_to_ll(const BigInt &value, long long &out);

    // ---- 构造折叠结果 ----
    // 结果不是有限数（±inf/NaN）时返回 nullptr——当前 float 字面量语法写不出这两种值，交给运行时处理
    [[nodiscard]] static AstNodePtr make_bool(Position pos, bool value);
    [[nodiscard]] static AstNodePtr make_int(Position pos, const BigInt &value);
    [[nodiscard]] static AstNodePtr make_float(Position pos, double value);
    // 把 double 格式化成合法的 SL float 字面量文本（永远带小数点，不用科学计数法，见 SL.md 2.1.4），
    // 取能精确 round-trip 回原值的最短小数位数
    [[nodiscard]] static std::string format_double(double value);
    // 深拷贝一份字面量子树；要求 is_literal(node)。用于 + 拼接、* 重复、dict 合并这类需要
    // "同一份内容用多次"的场合——这些场合各自都已经确认过操作数是 is_literal，可以放心复制
    [[nodiscard]] static AstNodePtr clone_literal(const AstNode &node);

    // 三态比较结果：Unordered 表示这两个类型之间不支持大小比较（交给运行时报 TypeError）
    enum class Cmp { Less, Equal, Greater, Unordered };

    // ==/!= 用：字面量之间的值相等（跨数字类型；str 按内容；tuple/list 逐元素；dict 不计顺序按 key 配对）
    [[nodiscard]] static bool literal_equal(const AstNode &a, const AstNode &b);
    // </<=/>/>= 用：数字按大小、str 按字典序、tuple/list 按字典序逐元素比较；其余（含跨类型、dict）不可比较
    [[nodiscard]] static Cmp literal_compare(const AstNode &a, const AstNode &b);

    // %s 的取值：None/bool/int/float/str 有明确文本；tuple/list/dict 这类容器的 str() 涉及递归拼接
    // repr，形状比这里其他分支复杂得多，且 %s 传容器本来就是少见用法，暂不支持（返回 nullopt）
    [[nodiscard]] static std::optional<std::u32string> to_display_string(const AstNode &node);

public:
    // 把 node 整体折成一个字面量节点。要求 node 的子节点已经被递归折过（由 LiteralFolder 保证，
    // 这里不做递归），只处理"这一层节点自己能不能变成字面量"。折不动（含 node 本身已经是字面量、
    // 是容器字面量、是标识符/调用/索引……任何非算符节点）一律返回 nullptr，node 不受影响。
    [[nodiscard]] static AstNodePtr fold(AstNode &node);
};
