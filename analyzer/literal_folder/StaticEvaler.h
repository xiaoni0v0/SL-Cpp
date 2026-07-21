#pragma once

#include "../../numeric/BigInt.h"
#include "../../parser/ast_nodes/ast_nodes.h"


/**
 * 编译期静态求值器
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
 * 除此之外，and/or/not 对于字面量均折叠。
 *
 * 死分支消除：
 *   cond 折成的字面量真值为 False 的 clause/循环整个消失，值退化成 SL.md 3.4.5.2/3.4.5.3 规定的默认值）；
 *   if 的某个 clause 的 cond 折成的字面量真值为 True，则连同它自己在内后面的 clause/else 全部消失，只留这个 clause 的 body；
 *   for/while 的 cond 折成的字面量真值为 True 的不折。
 *
 * 入参统一用非 const 引用（而不是指针）：调用方（LiteralFolder）保证传进来的节点非空，分派内部
 * 需要裸指针做 dynamic_cast 试探时自己取地址即可；用非 const 是因为 and/or、if/for 的死分支消除都
 * 不产出全新值，而是把已有的某个子节点（body_/else_expr_/init_ 等）原封不动地"挪走"当结果用，这需要
 * 能 std::move 走它——除此之外的所有分支都只读，只是顺带能拿这个非 const 权限，不代表它们会去改 node。
 */
class StaticEvaler {
    // 尝试把 node 折成一个字面量节点；不负责递归，返回 nullptr 表示折不动
    // （node 不受影响，and/or/if/for 这几个"挪子节点"的分支例外——
    // 一旦决定折叠成功，node 自己反正会被调用方整个替换掉，不会再被读取，见 LiteralFolder::visit_and_replace）
    [[nodiscard]] static AstNodePtr fold_unary(const AstNodeOpUnary &node);
    [[nodiscard]] static AstNodePtr fold_not(const AstNodeOpUnary &node);
    [[nodiscard]] static AstNodePtr fold_pos_neg_bitnot(const AstNodeOpUnary &node);
    [[nodiscard]] static AstNodePtr fold_binary(AstNodeOpBinary &node);
    [[nodiscard]] static AstNodePtr fold_compare(const AstNodeCompare &node);

    // fold_binary 按运算符语义家族分派到这几个：
    [[nodiscard]] static AstNodePtr fold_arithmetic(const AstNodeOpBinary &node); // 纯数值算术：- / // % **
    [[nodiscard]] static AstNodePtr fold_add(const AstNodeOpBinary &node); // + ：数值相加，或 str/tuple/list 拼接
    [[nodiscard]] static AstNodePtr fold_mul(const AstNodeOpBinary &node); // * ：数值相乘，或 (str/tuple/list, 非负 int) 重复
    [[nodiscard]] static AstNodePtr fold_bitwise(const AstNodeOpBinary &node); // & ^ | << >>，只对 bool/int 有意义
    [[nodiscard]] static AstNodePtr fold_and_or(AstNodeOpBinary &node); // 不短路，两边各自已经折过，挪走确定命中的一侧

    // 死分支消除：cond 折成字面量后，能确定整个 if/for/while 的哪部分会/不会执行
    [[nodiscard]] static AstNodePtr fold_if(AstNodeIf &node);
    [[nodiscard]] static AstNodePtr fold_for_cond(AstNodeForCond &node);

    // 真值判断：not/and/or/if/for 共用。每种参与折叠的字面量类型都有明确规则（None/False/0/0.0/
    // 空串/空容器为假，其余为真，见 SL.md 3.2），要求 node 已经是字面量节点（调用方保证）
    [[nodiscard]] static bool truthy(const AstNode &literal);

    // node 是不是一个"整体已知"的字面量：None/bool/int/float/str/Ellipsis 天然是；
    // tuple/list 要求每个元素递归满足。dict、_G/_L 恒不算——dict 的任何运算都不参与折叠（见类注释），
    // _G/_L 的值是运行时的实时字典视图，不是编译期能确定的值（见 SL.md 3.4.1）
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
    // 深拷贝一份字面量子树；要求 is_literal(node)。只处理 is_literal 认可的这个子集（不是给 AstNode
    // 整体加一个通用多态 clone()——目前只有 * 的容器重复需要"同一份内容用多次"这一个场景，没必要为了
    // 这一个场景就把克隆能力铺到所有几十种节点类型上，需要更通用的克隆能力时再加不迟）
    [[nodiscard]] static AstNodePtr clone_literal(const AstNode &node);

    // 三态比较结果：Unordered 表示这两个类型之间不支持大小比较（交给运行时报 TypeError）
    enum class Cmp { Less, Equal, Greater, Unordered };

    // ==/!= 用：字面量之间的值相等（跨数字类型；str 按内容；tuple/list 逐元素；其余跨类型恒不相等）
    [[nodiscard]] static bool literal_equal(const AstNode &a, const AstNode &b);
    // </<=/>/>= 用：数字按大小、str 按字典序、tuple/list 按字典序逐元素比较；其余（含跨类型）不可比较
    [[nodiscard]] static Cmp literal_compare(const AstNode &a, const AstNode &b);

public:
    // 把 node 整体折成一个字面量节点。要求 node 的子节点已经被递归折过（由 LiteralFolder 保证，
    // 这里不做递归），只处理"这一层节点自己能不能变成字面量"。折不动（含 node 本身已经是字面量、
    // 是容器字面量、是标识符/调用/索引……任何非算符节点）一律返回 nullptr，node 不受影响。
    [[nodiscard]] static AstNodePtr fold(AstNode &node);
};
