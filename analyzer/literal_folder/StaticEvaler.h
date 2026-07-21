#pragma once

#include "../../parser/ast_nodes/ast_nodes.h"


/**
 * 编译期静态求值器
 * 给定一个可能是纯字面量组合的表达式子树，尝试把它整体求值折成一个字面量节点
 * 折不动一律返回 nullptr，从不抛异常
 *
 * 组织方式：按运算符的语义家族分组（算术/位运算/比较/逻辑/is），每组内部再按需要覆盖到的具体类型分支，
 * 大部分算术/比较运算符只是数值提升（bool -> int -> float）+ 少数几个容器类型（str/tuple/list）各自的分支，
 * 不需要真的对每一对类型单独写代码。
 */
class StaticEvaler {
    // 尝试把 node 折成一个字面量节点；node 的子节点必须已经被（外层调用方）尽可能折过一遍——
    // 这里不负责递归，只负责"看当前这一个节点能不能整体折"。返回 nullptr 表示折不动（node 不受影响）。
    [[nodiscard]] static AstNodePtr fold_unary(const AstNodeOpUnary *node);
    [[nodiscard]] static AstNodePtr fold_binary(const AstNodeOpBinary *node);
    [[nodiscard]] static AstNodePtr fold_compare(const AstNodeCompare *node);
    [[nodiscard]] static AstNodePtr fold_is(const AstNodeIs *node);

    // fold_unary 的分支：not x，唯一定义域是"任意类型"（走 truthy 取反），跟 +x/-x/~x 那种需要
    // 具体数值类型的分支不是一回事，单独拆出来
    [[nodiscard]] static AstNodePtr fold_not(const AstNodeOpUnary *node);

    // fold_binary 按运算符语义家族分派到这几个：
    // 数值二元算术：+ - * / // % **（+、* 各自还有一条 str/tuple/list 的非数值分支）
    [[nodiscard]] static AstNodePtr fold_arithmetic(const AstNodeOpBinary *node);
    // 位运算/移位：& ^ | << >>，只对 bool/int 有意义
    [[nodiscard]] static AstNodePtr fold_bitwise(const AstNodeOpBinary *node);
    // and/or：有短路语义，折叠结果可能就是"原封不动挪用左操作数（或右操作数）这个已经在树里的节点"，
    // 不是每次都要凭空构造一个新值——这跟上面几个"永远构造全新结果"的家族不是一回事，
    // 现在的 fold(const AstNode*) 只读不拿所有权，没法把一个已有子节点整个搬出来当结果用，
    // 这里先留空、恒返回 nullptr（等于"这两个运算符暂时不参与折叠"），不是忘了写，
    // 是这处需要先跟"要不要给 AstNode 加 clone()，还是把这部分改到 LiteralFolder 里用 std::move
    // 直接搬"这个问题一个明确答案，再回来实现
    [[nodiscard]] static AstNodePtr fold_and(const AstNodeOpBinary *node);
    [[nodiscard]] static AstNodePtr fold_or(const AstNodeOpBinary *node);

    // 真值判断：not/and/or 共用。每种字面量类型都有明确规则（None/False/0/0.0/空串/空容器为假，
    // 其余为真），要求 node 已经是字面量节点（不是字面量就没法判断，调用方保证）
    [[nodiscard]] static bool truthy(const AstNode *literal);

    // node 是不是一个"终态"的字面量节点（None/bool/int/float/str/tuple/list/dict/...）——
    // 用来判断某个已经处理过的子节点还要不要再尝试折，或者能不能拿去做真值判断这类操作
    [[nodiscard]] static bool is_literal(const AstNode *node);

public:
    // 把 node 整体折成一个字面量节点。要求 node 的子节点已经被递归折过（由 LiteralFolder 保证，
    // 这里不做递归），只处理"这一层节点自己能不能变成字面量"。折不动（含 node 本身已经是字面量、
    // 是容器字面量、是标识符/调用/索引……任何非算符节点）一律返回 nullptr，node 不受影响。
    [[nodiscard]] static AstNodePtr fold(const AstNode *node);
};
