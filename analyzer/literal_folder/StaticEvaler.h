#pragma once

#include "../../parser/ast_nodes/ast_nodes.h"

#include <compare>
#include <cstddef>
#include <cstdint>
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
 * 以上中：
 * - 纯数值运算（含位运算）一律用 int64_t 计算，任何一步（含操作数本身的解析）超出 int64_t
 *   范围都不折，交给运行时用真正的任意精度整数处理；
 * - str 的 + 拼接、* 重复，结果长度超过 kMaxStrLength 不折；
 * - tuple/list 的 + 拼接，结果元素个数超过 kMaxContainerItems 不折；
 * - tuple 的 * 重复，除了同样受 kMaxContainerItems 限制，还要求这个 tuple
 *   是"深度不可变"的（递归展开后不含任何 list）——因为重复出来的每一份内部元素是共享引用
 *   （SL.md 3.4.2），一旦其中嵌套了可变的 list，"共享 vs 独立拷贝"就变得可观察，折叠没法在
 *   不知道以后语义怎么实现的情况下瞎猜，索性不折；纯不可变内容则无所谓，折出来大家肉眼不可辨；
 * - list 的 * 重复恒不折（list 本身永远可变，不存在"深度不可变"这一说）。
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

    /**
     * node 是不是"深度不可变"：递归展开后完全不含 list。
     * None/bool/int/float/str/Ellipsis 天然是；tuple 要求每个元素递归满足；list 恒不是。
     * 调用方保证 is_literal_pure(node)。
     * 只用于判断 tuple 的 * 重复能不能安全折叠——重复出来的每一份内部元素是共享引用
     * （SL.md 3.4.2），只有内容全程不可变时"共享 vs 独立拷贝"才不可区分，折叠才是安全的。
     */
    [[nodiscard]] static bool is_deeply_immutable(const AstNode &node);

    // —————————— 数值提升相关 ——————————

    // 是不是 bool 或 int
    [[nodiscard]] static bool is_int_family(const AstNode &node);
    // is_int_family 或 float
    [[nodiscard]] static bool is_numeric(const AstNode &node);
    // 要求 is_int_family(node)；literal 的数值超出 int64_t 范围（目前 int 字面量只有十进制数字，
    // 解析时按十进制累加做溢出检测）时返回 nullopt
    [[nodiscard]] static std::optional<int64_t> to_int64(const AstNode &node);
    // 要求 is_numeric(node)；int 分支直接对十进制文本调 strtod，不需要先转成任何数值类型，
    // 任意长度的十进制整数文本都能处理
    [[nodiscard]] static double to_double(const AstNode &node);

    // —————————— int64_t 溢出检测算术 ——————————
    // 底层用 C23 <stdckdint.h> 的 ckd_add/ckd_sub/ckd_mul：这是标准明确定义的语义（C23
    // §7.20.1），不是某个编译器的专有内建函数，溢出检测这种代码历史上太容易手写出细微的
    // bug，交给标准/编译器保证比自己再判一遍更可信；溢出统一返回 nullopt

    [[nodiscard]] static std::optional<int64_t> checked_neg(int64_t a);
    [[nodiscard]] static std::optional<int64_t> checked_add(int64_t a, int64_t b);
    [[nodiscard]] static std::optional<int64_t> checked_sub(int64_t a, int64_t b);
    [[nodiscard]] static std::optional<int64_t> checked_mul(int64_t a, int64_t b);
    // 非负整数次幂，快速幂循环，每一步乘法都做溢出检测
    [[nodiscard]] static std::optional<int64_t> checked_pow(int64_t base, int64_t exponent);
    // <<：结果只会变大，要做溢出检测；shift 不在 [0, 62] 内直接不折
    [[nodiscard]] static std::optional<int64_t> checked_lshift(int64_t value, int64_t shift);
    // >>：结果只会更收敛，任意非负 shift 都有确定结果（shift 很大时饱和到 0 或 -1），
    // 不会溢出，shift 本身不用设上限；shift 为负返回 nullopt
    [[nodiscard]] static std::optional<int64_t> arithmetic_rshift(int64_t value, int64_t shift);

    // —————————— 折叠上限 ——————————

    // tuple/list：+ 拼接、tuple 的 * 重复（且深度不可变），结果元素个数上限
    static constexpr size_t nMaxContainerItems{256};
    // str：+ 拼接、* 重复，结果字符数上限
    static constexpr size_t nMaxStrLength{4096};
    // 判断 base_size 重复 n 次会不会超过 cap；用除法反推，不做乘法本身，避免 size_t 先溢出
    [[nodiscard]] static bool repeated_size_exceeds(size_t base_size, size_t n, size_t cap);

    // —————————— 构造折叠结果 ——————————

    [[nodiscard]] static AstNodePtr make_bool(Position pos, bool value);
    [[nodiscard]] static AstNodePtr make_int(Position pos, int64_t value);
    [[nodiscard]] static AstNodePtr make_float(Position pos, double value); // ±inf/NaN 返回 nullptr
    // 把 double 格式化成合法的 SL float 字面量文本（永远带小数点，不用科学计数法）
    [[nodiscard]] static std::string format_double(double value);
    // 深拷贝一份字面量子树；调用方保证 is_literal_pure(node)
    [[nodiscard]] static AstNodePtr clone_literal(const AstNode &node);

    // 三态比较结果：Unordered 表示这两个类型之间不支持大小比较（交给运行时报 TypeError）
    enum class CmpResult { Less, Equal, Greater, Unordered };

    // ==/!= 用：字面量之间的值相等（跨数字类型；str 按内容；tuple/list 逐元素；其余跨类型恒不相等）
    [[nodiscard]] static bool literal_equal(const AstNode &a, const AstNode &b);
    // </<=/>/>= 用：数字按大小、str 按字典序、tuple/list
    // 按字典序逐元素比较；其余（含跨类型）不可比较
    [[nodiscard]] static CmpResult literal_compare(const AstNode &a, const AstNode &b);
    // int 字面量（只支持十进制数字文本）按数值大小比较，不经过任何数值类型：
    // 先去掉前导零，位数不等直接分高下，位数相等再按字典序
    [[nodiscard]] static std::strong_ordering
    compare_int_literals(const AstNodeLiteralInt &a, const AstNodeLiteralInt &b);

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
