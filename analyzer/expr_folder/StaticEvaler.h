#pragma once

#include "../../parser/ast_nodes/ast_nodes.h"

#include <compare>
#include <cstdint>
#include <optional>

/**
 * 编译期静态求值器
 * 不依赖高精度库
 * 折不动一律返回 nullptr，从不抛异常
 *
 * 折叠范围：
 *
 * 一元：
 *   bool/int: + - ~
 *   float: + -
 *   str/tuple/list: 无
 *
 * 二元：
 *           bool/int   float   str   tuple   list
 * bool/int     A         B      D      D      D
 *  float       B         B      E      E      E
 *   str        D         E      C      E      E
 *  tuple       D         E      E      C      E
 *  list        D         E      E      E      C
 *
 * A = { ** * / // % + - << >> & ^ | < <= > >= != == }
 * B = { ** * / // % + - < <= > >= != == }
 * C = { + < <= > >= != == }
 * D = { * != == }
 * E = { != == }
 *
 * 以上中：
 * - 纯数值运算（含位运算）一律用 int64_t 计算，任何一步超出 int64_t 范围都不折；
 * - str 的 + 拼接、* 重复，结果长度不超过 nMaxStrLength 时折叠；
 * - tuple/list 的 + 拼接，结果元素个数不超过 nMaxContainerItems 时折叠；
 * - tuple/list 的 * 重复恒不折。
 *
 * 除此之外，and/or/not 对于字面量均折叠。
 *
 * 死分支消除：
 *   1. if/for/while 的 cond 折成的字面量真值为 False 的 clause/循环整个消失，值退化成默认值）。
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
class StaticEvaler final {
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
    [[nodiscard]] static AstNodePtr fold_mul(AstNodeOpBinary &node);

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

    // 真值。调用方保证 is_literal_pure(literal)
    [[nodiscard]] static bool truthy(const AstNode &literal);

    /**
     * node 是不是一个纯字面量：
     *   None/bool/int/float/str/Ellipsis 天然是；
     *   tuple/list 要求每个元素递归满足；
     *   dict、_G/_L 恒不是。
     */
    [[nodiscard]] static bool is_literal_pure(const AstNode &node);

    // —————————— 数值提升相关 ——————————

    // 是不是 bool 或 int
    [[nodiscard]] static bool is_int_family(const AstNode &node);
    // 是不是 bool 或 int 或 float
    [[nodiscard]] static bool is_numeric(const AstNode &node);
    // node -> int64_t。调用方保证 is_int_family(node)
    [[nodiscard]] static std::optional<int64_t> node_to_int64(const AstNode &node);
    // node -> double。调用方保证 is_numeric(node)
    [[nodiscard]] static double node_to_double(const AstNode &node);

    // —————————— 折叠上限 ——————————

    // tuple/list：+ 拼接的结果元素个数上限
    static constexpr size_t nMaxContainerItems{256};
    // str：+ 拼接、* 重复，结果字符数上限
    static constexpr size_t nMaxStrLength{4096};

    // —————————— 构造折叠结果 ——————————

    [[nodiscard]] static AstNodePtr make_bool(Position pos, bool value);
    [[nodiscard]] static AstNodePtr make_int(Position pos, int64_t value);
    [[nodiscard]] static AstNodePtr make_float(Position pos, double value); // ±inf/NaN 返回 nullptr
    // 深拷贝一份字面量子树；调用方保证 is_literal_pure(node)
    [[nodiscard]] static AstNodePtr clone_literal(const AstNode &node);
    // 字面量之间的值相等。调用方保证 is_literal_pure(a) 且 is_literal_pure(b)
    [[nodiscard]] static bool literal_equal(const AstNode &a, const AstNode &b);
    // 字面量之间的值比较。调用方保证 is_literal_pure(a) 且 is_literal_pure(b)
    [[nodiscard]] static std::partial_ordering literal_compare(const AstNode &a, const AstNode &b);
    // int 字面量之间的值比较。调用方保证 a.raw_、b.raw_ 均非空
    [[nodiscard]] static std::strong_ordering
    literal_compare_int(const AstNodeLiteralInt &a, const AstNodeLiteralInt &b);

  public:
    // 纯工具类，静态、无状态，直接禁止实例化
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
