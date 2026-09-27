#pragma once

#include "../../parser/ast_nodes/ast_nodes.h"

#include <cstddef>

/**
 * 编译期静态求值器
 * 折不动一律返回 nullptr，从不抛异常
 *
 * 只有当编译期算出的结果，在任何可能的运行期上下文下都与运行期结果逐位相同时，才折。
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
 *       int   A     B      G      E     E      E
 *      bool   B     B      G      F     F      F
 *   decimal   G     G      G      F     F      F
 *       str   E     F      F      C     F      F
 *     tuple   E     F      F      F     C      F
 *      list   E     F      F      F     F      C
 *
 * A = { ** * // % + - == != < <= > >= << >> & ^ | } // 算数、比较、位运算
 * B = { ** * // % + - == != < <= > >= }             // 算数、比较
 * C = { + == != < <= > >= }                         // 容器拼接、比较
 * E = { * == != }                                   // 容器重复、相等
 * F = { == != }                                     // 相等
 * G = { }                                           // 都不折
 *
 * 除此之外，and/or/not 对于字面量均折叠。
 *
 * 规模上限：
 * - int 如果能用 int64_t 装下则折，科学计数法写法如果够小则字面展开；
 * - str 的 + 拼接、* 重复，结果长度不超过 kMaxStrLength 时折叠；
 * - tuple/list 的 + 拼接、* 重复，结果元素个数不超过 kMaxContainerItems 时折叠；
 *
 * 运行期必然报错的一律不折，把错误原样留给运行期。
 *
 * 死代码消除：
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
    [[nodiscard]] static AstNodePtr fold_mul(const AstNodeOpBinary &node);

    // 纯数值算术

    // 一元 + -
    [[nodiscard]] static AstNodePtr fold_arithmetic(const AstNodeOpUnary &node);
    // 二元 + - * // % **
    [[nodiscard]] static AstNodePtr fold_arithmetic(const AstNodeOpBinary &node);

    // bool/int 的位运算

    // 一元 ~
    [[nodiscard]] static AstNodePtr fold_bitwise(const AstNodeOpUnary &node);
    // 二元 & ^ | << >>
    [[nodiscard]] static AstNodePtr fold_bitwise(const AstNodeOpBinary &node);

    // and/or。按短路语义只看左操作数
    [[nodiscard]] static AstNodePtr fold_and_or(AstNodeOpBinary &node);

    // —————————— 折叠上限 ——————————

    // tuple/list：+ 拼接、* 重复的结果元素个数上限
    static constexpr std::size_t kMaxContainerItems{256};
    // str：+ 拼接、* 重复，结果字符数上限
    static constexpr std::size_t kMaxStrLength{4096};

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
