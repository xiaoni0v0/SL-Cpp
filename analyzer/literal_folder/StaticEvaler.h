#pragma once

#include "../../numeric/BigInt.h"
#include "../../parser/ast_nodes/ast_nodes.h"

#include <optional>

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
 * 以上中，数字与容器相乘的只有在数字在 int 能装下时才折叠。
 *
 * 除此之外，and/or/not 对于字面量均折叠。
 *
 * 死分支消除：
 *   cond 折成的字面量真值为 False 的 clause/循环整个消失，值退化成 SL.md 3.4.5.2/3.4.5.3
 * 规定的默认值）； if 的某个 clause 的 cond 折成的字面量真值为 True，则连同它自己在内后面的
 * clause/else 全部消失，只留这个 clause 的 body； for/while 的 cond 折成的字面量真值为 True
 * 的不折。
 */
class StaticEvaler final {
    // —————————— 一级入口 ——————————

    // 一元
    [[nodiscard]] static AstNodePtr fold_unary(AstNodeOpUnary &node);
    // 二元
    [[nodiscard]] static AstNodePtr fold_binary(AstNodeOpBinary &node);
    // 二元比较
    [[nodiscard]] static AstNodePtr fold_compare(AstNodeCompare &node);
    // 死分支消除
    [[nodiscard]] static AstNodePtr fold_if(AstNodeIf &node);
    // 死分支消除
    [[nodiscard]] static AstNodePtr fold_for_cond(AstNodeForCond &node);

    // —————————— 二级入口 ——————————

    // not
    [[nodiscard]] static AstNodePtr fold_not(AstNodeOpUnary &node);
    // + ：数值相加，或 str/tuple/list 拼接
    [[nodiscard]] static AstNodePtr fold_add(AstNodeOpBinary &node);
    // * ：数值相乘，或 str/tuple/list 重复
    [[nodiscard]] static AstNodePtr fold_mul(AstNodeOpBinary &node);
    // 纯数值算术：一元 + -，二元 + - * / // % **；fold_add/fold_mul 数值分支也委托给二元版本
    [[nodiscard]] static AstNodePtr fold_arithmetic(AstNodeOpUnary &node);
    [[nodiscard]] static AstNodePtr fold_arithmetic(AstNodeOpBinary &node);
    // bool/int 的位运算：一元 ~，二元 & ^ | << >>
    [[nodiscard]] static AstNodePtr fold_bitwise(AstNodeOpUnary &node);
    [[nodiscard]] static AstNodePtr fold_bitwise(AstNodeOpBinary &node);
    // and or；折叠的时候不短路
    [[nodiscard]] static AstNodePtr fold_and_or(AstNodeOpBinary &node);

    // —————————— 判断 ——————————

    // 真值判断，要求 node 已经是字面量节点
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
    // is_int_family 或 float
    [[nodiscard]] static bool is_numeric(const AstNode &node);
    // 要求 is_int_family(node)
    [[nodiscard]] static BigInt to_bigint(const AstNode &node);
    // 要求 is_numeric(node)
    [[nodiscard]] static double to_double(const AstNode &node);
    // BigInt 转 int，装不下返回 nullopt
    [[nodiscard]] static std::optional<int> try_to_int(const BigInt &value);

    // —————————— 构造折叠结果 ——————————

    [[nodiscard]] static AstNodePtr make_bool(Position pos, bool value);
    [[nodiscard]] static AstNodePtr make_int(Position pos, const BigInt &value);
    [[nodiscard]] static AstNodePtr make_float(Position pos, double value); // ±inf/NaN 返回 nullptr
    // 把 double 格式化成合法的 SL float 字面量文本（永远带小数点，不用科学计数法）
    [[nodiscard]] static std::string format_double(double value);
    // 深拷贝一份字面量子树；调用方保证 is_literal(node)
    [[nodiscard]] static AstNodePtr clone_literal(const AstNode &node);

    // 三态比较结果：Unordered 表示这两个类型之间不支持大小比较（交给运行时报 TypeError）
    enum class CmpResult { Less, Equal, Greater, Unordered };

    // ==/!= 用：字面量之间的值相等（跨数字类型；str 按内容；tuple/list 逐元素；其余跨类型恒不相等）
    [[nodiscard]] static bool literal_equal(const AstNode &a, const AstNode &b);
    // </<=/>/>= 用：数字按大小、str 按字典序、tuple/list
    // 按字典序逐元素比较；其余（含跨类型）不可比较
    [[nodiscard]] static CmpResult literal_compare(const AstNode &a, const AstNode &b);

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
