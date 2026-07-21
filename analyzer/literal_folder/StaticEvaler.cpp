#include "StaticEvaler.h"

AstNodePtr StaticEvaler::fold(const AstNode *node) {
    // 只有这四种"运算符"节点才可能整体收缩成一个字面量；其余任何节点类型（含已经是字面量、
    // 容器字面量、标识符、调用……）都原样返回 nullptr，交给调用方保持原样不动
    if (const auto *n{dynamic_cast<const AstNodeOpUnary *>(node)}) return fold_unary(n);
    if (const auto *n{dynamic_cast<const AstNodeOpBinary *>(node)}) return fold_binary(n);
    if (const auto *n{dynamic_cast<const AstNodeCompare *>(node)}) return fold_compare(n);
    if (const auto *n{dynamic_cast<const AstNodeIs *>(node)}) return fold_is(n);
    return nullptr;
}

AstNodePtr StaticEvaler::fold_unary(const AstNodeOpUnary *node) {
    using OpType = AstNodeOpUnary::OpType;
    switch (node->op_) {
    case OpType::Not: return fold_not(node);
    case OpType::Pos:
    case OpType::Neg:
    case OpType::BitNot:
        // TODO：+x/-x 走数值提升（bool -> int -> float），~x 只对 bool/int 有意义（借 BigInt 的
        // operator~），操作数要求已经是对应类型的字面量节点（is_literal 判过），类型不对就 nullptr
        return nullptr;
    case OpType::Question:
    case OpType::Exclaim:
        // x?/x! 是类型运算符（生成复合类型，见 SL.md 3.4.2），不作用于值，不在字面量折叠的范围内
        return nullptr;
    default: return nullptr;
    }
}

AstNodePtr StaticEvaler::fold_not(const AstNodeOpUnary *node) {
    if (!is_literal(node->operand_.get())) return nullptr;
    // TODO：构造一个 AstNodeLiteralBool{node->pos_, !truthy(node->operand_.get())} 返回
    return nullptr;
}

AstNodePtr StaticEvaler::fold_binary(const AstNodeOpBinary *node) {
    using OpType = AstNodeOpBinary::OpType;
    switch (node->op_) {
    case OpType::Add:
    case OpType::Sub:
    case OpType::Mul:
    case OpType::Div:
    case OpType::DivFloor:
    case OpType::Mod:
    case OpType::Pow: return fold_arithmetic(node);
    case OpType::BitAnd:
    case OpType::BitOr:
    case OpType::BitXor:
    case OpType::LShift:
    case OpType::RShift: return fold_bitwise(node);
    case OpType::And: return fold_and(node);
    case OpType::Or: return fold_or(node);
    case OpType::Range:
        // range 对象不属于 None/bool/int/float/str/tuple/list/dict 这套字面量类型，不折
        return nullptr;
    default: return nullptr;
    }
}

AstNodePtr StaticEvaler::fold_arithmetic(const AstNodeOpBinary * /*node*/) {
    // TODO：要求 left_/right_ 都已经是字面量节点（is_literal 判过）：
    //   - 都是数值（bool/int/float，按提升规则）：+ - * / // % ** 各自用 BigInt（int 情况）或
    //     double（掺了 float 就都转 double）算，/ 恒产出 float，// 和 % 恒产出 int（用 BigInt::floor_div/mod）；
    //   - + 的字符串/元组/列表拼接分支、* 的"容器 * 非负 int 重复"分支，单独判
    //   - 任何一步类型不对、除零之类会抛异常的情况，直接 nullptr（不是在这里抛异常，见 SL.md 1189）
    return nullptr;
}

AstNodePtr StaticEvaler::fold_bitwise(const AstNodeOpBinary * /*node*/) {
    // TODO：要求 left_/right_ 都是 bool/int 字面量，用 BigInt 的 &/|/^/<</>>；float/str/... 一律 nullptr
    return nullptr;
}

AstNodePtr StaticEvaler::fold_and(const AstNodeOpBinary * /*node*/) {
    return nullptr; // 见头文件里这个方法声明处的注释：需要先定下"怎么挪用现成子节点"这个问题
}

AstNodePtr StaticEvaler::fold_or(const AstNodeOpBinary * /*node*/) {
    return nullptr; // 同 fold_and
}

AstNodePtr StaticEvaler::fold_compare(const AstNodeCompare * /*node*/) {
    // TODO：operands_ 挨个检查是不是都已经是字面量；ops_ 逐个按"数值提升比较"或者
    // str/tuple/list 各自的字典序比较去算，只要链中有一环不满足就整条链 nullptr（== / != 例外，
    // 见下）；一旦某一环的结果已经能确定整条链是 False，理论上可以提前判定，但这属于"提前短路"
    // 的优化，先不做，跟 fold_binary 目前的朴素程度保持一致
    return nullptr;
}

AstNodePtr StaticEvaler::fold_is(const AstNodeIs * /*node*/) {
    // TODO：is 判断的是对象同一性，不是值——多数类型在编译期折叠阶段构造出来的字面量，运行时
    // 是不是"同一个对象"完全是 VM 对象模型/是否 intern 决定的，这里没法安全预判，大概率只有
    // None 这种保证单例的类型才适合折（None is None -> True），其余一律不折，比"尽量都折"更安全
    return nullptr;
}

bool StaticEvaler::truthy(const AstNode *literal) {
    if (dynamic_cast<const AstNodeLiteralNone *>(literal)) return false;
    if (const auto *b{dynamic_cast<const AstNodeLiteralBool *>(literal)}) return b->value_;
    // TODO：int 非 0、float 非 0.0、str/tuple/list/dict 非空为真，其余（目前看到的字面量类型里
    // 剩下的是 _G/_L 和 ...，这两个的真值规则得去 SL.md 确认，不能想当然）
    return true;
}

bool StaticEvaler::is_literal(const AstNode *node) {
    return dynamic_cast<const AstNodeLiteralNone *>(node)
           || dynamic_cast<const AstNodeLiteralBool *>(node)
           || dynamic_cast<const AstNodeLiteralGL *>(node)
           || dynamic_cast<const AstNodeLiteralInt *>(node)
           || dynamic_cast<const AstNodeLiteralFloat *>(node)
           || dynamic_cast<const AstNodeLiteralStr *>(node)
           || dynamic_cast<const AstNodeLiteralTuple *>(node)
           || dynamic_cast<const AstNodeLiteralList *>(node)
           || dynamic_cast<const AstNodeLiteralDict *>(node)
           || dynamic_cast<const AstNodeLiteralEllipsis *>(node);
}
