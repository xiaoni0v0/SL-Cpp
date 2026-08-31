#pragma once

#include "../../numeric/BigDec.h"
#include "../../numeric/BigInt.h"
#include "../../parser/ast_nodes/ast_nodes.h"

#include <compare>
#include <cstdint>
#include <optional>

/**
 * 编译期静态求值器
 * 折不动一律返回 nullptr，从不抛异常
 *
 * 只有当编译期算出的结果，在任何可能的运行期上下文下都与运行期结果逐位相同时，才折。
 *
 * 有两件互相独立的事会破坏这条判据：
 * 1. 上下文依赖，比如 decimal。
 * 2. 表示保真，比如用 int64_t 算 int 是保守但正确（溢出就放弃）；用 double 算 decimal 是错值。
 *
 * 折叠范围：
 *
 * 一元：
 *   int: + - ~
 *   bool: + -
 *   decimal/str/tuple/list: 无
 *
 * 二元：
 *            int  bool  decimal  str  tuple  list
 *       int   A     B      D      E     F      F
 *      bool   B     B      D      F     F      F
 *   decimal   D     D      D      F     F      F
 *       str   E     F      F      C     F      F
 *     tuple   F     F      F      F     C      F
 *      list   F     F      F      F     F      C
 *
 * A = { ** * // % + - < <= > >= != == << >> & ^ | } // 算数、比较、位运算
 * B = { ** * // % + - < <= > >= != == }             // 算数、比较
 * C = { + < <= > >= != == }                         // 容器拼接、比较
 * D = { < <= > >= != == }                           // 比较
 * E = { * != == }                                   // 容器重复、相等
 * F = { != == }                                     // 相等
 *
 * 除此之外，and/or/not 对于字面量均折叠。
 *
 * 规模上限：
 * - int 结果的十进制位数不超过 nMaxIntDigits。
 *   只有会爆炸的 `*`/`**`/`<<` 需要事先估算，`+`/`-` 至多多一位、// % & | ^ >> 只会变小，都不用卡；
 * - str 的 + 拼接、* 重复，结果长度不超过 nMaxStrLength 时折叠；
 * - tuple/list 的 + 拼接，结果元素个数不超过 nMaxContainerItems 时折叠；
 *
 * 运行期必然报错的一律不折，把错误原样留给运行期。
 *
 * 死分支消除：
 *   1. if、步进模式的 for、while 的 cond 折成的字面量真值为 False 的 clause/循环整个消失，
 *      值退化成默认值。
 *   2. if 的某个 clause 的 cond 折成的字面量真值为 True，
 *      则连同它自己在内后面的 clause/else 全部消失，只留这个 clause 的 body；
 *
 * 复合表达式 { expr1; expr2; ... }：值是最后一条表达式的值
 * 规则：
 *   1. 空复合表达式恒折成 None；
 *   2. 只有一条，直接展开成那一条本身（不管是不是字面量）；
 *   3. 否则，除最后一条外，逐条丢掉纯字面量的子表达式，非字面量的保留且相对顺序不变；
 *      最后一条永远保留（它决定整个表达式的值）；
 *      丢到只剩最后一条就直接展开，丢完还剩不止一条就拼一个更短的复合表达式，一条都没丢成就不折。
 *
 * AstNodeProgram 原地精简 exprs_，丢掉全部纯字面量子表达式
 */
class StaticEvaler {
    // —————————— 一级入口 ——————————

    // 一元
    [[nodiscard]] static AstNodePtr fold_unary(const AstNodeOpUnary &node);
    // 二元
    [[nodiscard]] static AstNodePtr fold_binary(AstNodeOpBinary &node);
    // 二元比较
    [[nodiscard]] static AstNodePtr fold_compare(AstNodeCompare &node);
    // 死分支消除
    [[nodiscard]] static AstNodePtr fold_if(AstNodeIf &node);
    // 死循环消除
    [[nodiscard]] static AstNodePtr fold_for_cond(AstNodeForCond &node);
    // 复合表达式
    [[nodiscard]] static AstNodePtr fold_compound(AstNodeCompound &node);

    // —————————— 二级入口 ——————————

    // not
    [[nodiscard]] static AstNodePtr fold_not(const AstNodeOpUnary &node);
    // + ：数值相加或 str/tuple/list 拼接
    [[nodiscard]] static AstNodePtr fold_add(AstNodeOpBinary &node);
    // * ：数值相乘或 str/tuple/list 重复
    [[nodiscard]] static AstNodePtr fold_mul(const AstNodeOpBinary &node);

    // 纯数值算术

    // 一元 + -
    [[nodiscard]] static AstNodePtr fold_arithmetic(const AstNodeOpUnary &node);
    // 二元 + - * / // % **
    [[nodiscard]] static AstNodePtr fold_arithmetic(const AstNodeOpBinary &node);

    // bool/int 的位运算

    // 一元 ~
    [[nodiscard]] static AstNodePtr fold_bitwise(const AstNodeOpUnary &node);
    // 二元 & ^ | << >>
    [[nodiscard]] static AstNodePtr fold_bitwise(const AstNodeOpBinary &node);

    // and or。折叠的时候不短路
    [[nodiscard]] static AstNodePtr fold_and_or(AstNodeOpBinary &node);

    // —————————— 判断 ——————————

    // 真值。调用方保证 is_literal_pure(literal)。
    // nullopt = "判不了"（字面量形状不合法、或 decimal 超出 BigDec 表示范围）。调用方必须把它
    // 当"不折"处理，**绝不能默认成真或假**——这个返回值会决定死分支消除留下哪一支
    [[nodiscard]] static std::optional<bool> truthy(const AstNode &literal);

    /**
     * node 是不是一个纯字面量：
     *   None/bool/int/decimal/str/Ellipsis 天然是；
     *   tuple/list 要求每个元素递归满足；
     *   dict、_G/_L 恒不是。
     */
    [[nodiscard]] static bool is_literal_pure(const AstNode &node);

    // —————————— 数值分类与取值 ——————————
    //
    // 三个谓词是分层的：is_int ⊂ is_int_family ⊂ is_numeric，各自对应上面表里的一档：
    //   is_int        —— 严格 int，只有位运算该用它（bool 没有位运算方法）
    //   is_int_family —— int 或 bool，四则运算/比较用它（bool 先折算成 int，SL.md 4.2.5）
    //   is_numeric    —— 再加上 decimal，只有比较和真值该用它（decimal 算术一律不折）

    // 是不是 int。只有位运算该用它
    [[nodiscard]] static bool is_int(const AstNode &node);
    // 是不是 bool 或 int。四则运算、比较会把 bool 折算成 int 再算，用这个
    [[nodiscard]] static bool is_int_family(const AstNode &node);
    // 是不是 bool 或 int 或 decimal
    [[nodiscard]] static bool is_numeric(const AstNode &node);

    // node -> BigInt。调用方保证 is_int_family(node)；字面量形状不合法时返回 nullopt（不抛）
    // bool 按 SL.md 4.2.5 折算成 1/0；int 的科学计数法写法（1e9）由 BigInt 自己按值展开
    [[nodiscard]] static std::optional<BigInt> node_to_bigint(const AstNode &node);
    // node -> BigDec。调用方保证 is_numeric(node)；不合法或非有限（inf/NaN）时返回 nullopt
    // int/bool 精确提升成 decimal（SL.md 4.2.6：提升不舍入）
    [[nodiscard]] static std::optional<BigDec> node_to_bigdec(const AstNode &node);
    // BigInt -> int64_t，装不下返回 nullopt。移位量、指数这类"必须是小整数"的场合用
    [[nodiscard]] static std::optional<int64_t> bigint_to_int64(const BigInt &value);

    // —————————— 折叠上限 ——————————

    // tuple/list：+ 拼接的结果元素个数上限
    static constexpr size_t nMaxContainerItems{256};
    // str：+ 拼接、* 重复，结果字符数上限
    static constexpr size_t nMaxStrLength{4096};
    // int：折叠结果的十进制位数上限。只有 * ** << 需要事先估算并卡它，见类注释
    static constexpr size_t nMaxIntDigits{4096};

    // —————————— 构造折叠结果 ——————————

    [[nodiscard]] static AstNodePtr make_bool(Position pos, bool value);
    [[nodiscard]] static AstNodePtr make_int(Position pos, const BigInt &value);
    // 字面量之间的值相等。调用方保证 is_literal_pure(a) 且 is_literal_pure(b)
    [[nodiscard]] static std::optional<bool> literal_equal(const AstNode &a, const AstNode &b);
    // 字面量之间的值比较。调用方保证 is_literal_pure(a) 且 is_literal_pure(b)
    [[nodiscard]] static std::partial_ordering literal_compare(const AstNode &a, const AstNode &b);

  public:
    StaticEvaler() = delete;
    ~StaticEvaler() = delete;
    StaticEvaler(const StaticEvaler &) = delete;
    StaticEvaler(StaticEvaler &&) = delete;
    StaticEvaler &operator=(const StaticEvaler &) = delete;
    StaticEvaler &operator=(StaticEvaler &&) = delete;

    // 尝试把 node 折成一个字面量节点；不递归，返回 nullptr 表示折不动
    [[nodiscard]] static AstNodePtr fold(AstNode &node);

    // 原地精简 AstNodeProgram 的 exprs_
    static void prune_program(AstNodeProgram &node);
};
