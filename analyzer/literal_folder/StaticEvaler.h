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
 * - str 的 + 拼接、* 重复，结果长度超过 nMaxStrLength 不折；
 * - tuple/list 的 + 拼接，结果元素个数超过 nMaxContainerItems 不折；
 * - tuple 的 * 重复，除了同样受 nMaxContainerItems 限制，还要求这个 tuple 是“深度不可变”的。
 * - list 的 * 重复恒不折（list 本身永远可变）。
 *
 * 除此之外，and/or/not 对于字面量均折叠。
 *
 * 死分支消除：
 *   1. if/for/while 的 cond 折成的字面量真值为 False 的 clause/循环整个消失，值退化成默认值）。
 *   2. if 的某个 clause 的 cond 折成的字面量真值为 True，
 *      则连同它自己在内后面的 clause/else 全部消失，只留这个 clause 的 body；
 */
class StaticEvaler final {
    // —————————— 一级入口 ——————————

    // 一元
    [[nodiscard]] static AstNodePtr fold_unary(const AstNodeOpUnary &node);
    // 二元
    [[nodiscard]] static AstNodePtr fold_binary(AstNodeOpBinary &node);
    // 二元比较
    [[nodiscard]] static AstNodePtr fold_compare(const AstNodeCompare &node);
    // 死分支消除
    [[nodiscard]] static AstNodePtr fold_if(AstNodeIf &node);
    // 死循环消除
    [[nodiscard]] static AstNodePtr fold_for_cond(AstNodeForCond &node);

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

    // 真值，要求 node 已经是字面量节点
    [[nodiscard]] static bool truthy(const AstNode &literal);

    /**
     * node 是不是一个纯字面量：
     *   None/bool/int/float/str/Ellipsis 天然是；
     *   tuple/list 要求每个元素递归满足；
     *   dict、_G/_L 恒不是。
     */
    [[nodiscard]] static bool is_literal_pure(const AstNode &node);

    /**
     * node 是不是"深度不可变"：递归展开后完全不含 list。
     * None/bool/int/float/str/Ellipsis 天然是；tuple 要求每个元素递归满足；list 恒不是。
     * 调用方保证 is_literal_pure(node)。
     */
    [[nodiscard]] static bool is_deeply_immutable(const AstNode &node);

    // —————————— 数值提升相关 ——————————

    // 是不是 bool 或 int
    [[nodiscard]] static bool is_int_family(const AstNode &node);
    // 是不是 bool 或 int 或 float
    [[nodiscard]] static bool is_numeric(const AstNode &node);
    // node -> int64_t。调用方保证 is_int_family(node)
    [[nodiscard]] static std::optional<int64_t> node_to_int64(const AstNode &node);
    // node -> int64_t。调用方保证 is_numeric(node)
    [[nodiscard]] static double node_to_double(const AstNode &node);

    // —————————— 折叠上限 ——————————

    // tuple/list：+ 拼接、tuple 的 * 重复，结果元素个数上限
    static constexpr size_t nMaxContainerItems{256};
    // str：+ 拼接、* 重复，结果字符数上限
    static constexpr size_t nMaxStrLength{4096};

    // —————————— 构造折叠结果 ——————————

    [[nodiscard]] static AstNodePtr make_bool(Position pos, bool value);
    [[nodiscard]] static AstNodePtr make_int(Position pos, int64_t value);
    [[nodiscard]] static AstNodePtr make_float(Position pos, double value); // ±inf/NaN 返回 nullptr
    // 深拷贝一份字面量子树；调用方保证 is_literal_pure(node)
    [[nodiscard]] static AstNodePtr clone_literal(const AstNode &node);
    // 字面量之间的值相等
    [[nodiscard]] static bool literal_equal(const AstNode &a, const AstNode &b);
    // 字面量之间的值比较
    [[nodiscard]] static std::partial_ordering literal_compare(const AstNode &a, const AstNode &b);
    // int 字面量之间的值比较
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
};
