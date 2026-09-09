#pragma once

#include "../../runtime/objects/Code.h"
#include "../parser/ast_nodes/ast_nodes.h"
#include "ConstPool.h"
#include "opcode.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * 一份 Code 的组装器。
 *
 * 不认识 AST，只认识"指令、标签、表"：跳转目标先写成标签，最后一次性回填；参数装不下一个字节时
 * 补 EXTENDED_ARG 前缀，而补前缀会让后面的指令右移、可能又逼出新前缀，所以反复扫描到不再变化为止
 * （前缀只增不减，必收敛）。
 *
 * 顺带静态跟踪操作数栈深，算出 max_stack。跳转处两条路径的栈深必须一致，对不上是 codegen 的 bug，
 * 用断言当场抓——这是手写字节码最容易错的地方。
 */
class CodeBuilder final {
  public:
    // 跳转目标的占位。new_label() 分配，place_label() 定位置
    using Label = std::uint32_t;

  private:
    // 一条还没定下参数的指令
    struct PseudoInstr {
        OpCode op_;
        std::uint32_t arg_{0};     // 立即数；跳转指令这里存的是标签号
        bool arg_is_label_{false}; // arg_ 该当标签号读还是当立即数读
        Position pos_;
    };

    std::vector<PseudoInstr> instrs_;

    // 标签号 -> 它指向第几条伪指令。nullopt 表示还没放置
    std::vector<std::optional<std::uint32_t>> label_targets_;
    // 标签号 -> 该处应有的操作数栈深。nullopt 表示还不知道
    std::vector<std::optional<std::uint32_t>> label_stacks_;

    ConstPool const_pool_;

    std::vector<Ref<Str>> names_;
    std::unordered_map<std::u32string, std::uint32_t> name_indices_;

    std::vector<Ref<Code>> nested_codes_;

    std::vector<bool> ref_captured_;
    std::vector<Ref<Str>> value_captured_;
    std::optional<Code::ParamShape> params_;
    std::string source_name_;

    std::uint32_t cur_stack_{0};
    std::uint32_t max_stack_{0};
    // 上一条指令之后顺序流还走不走得下去。终止指令之后为真，直到下一个标签
    bool unreachable_{false};

    // 记一笔栈深变化
    void apply_effect(int effect);
    // 断言标签处的栈深是 depth，或者第一次得知就记下来
    void note_label_stack(Label label, std::uint32_t depth);
    // 发一条伪指令，只管指令流和栈跟踪，不管参数怎么来的
    void push_instr(PseudoInstr instr);

  public:
    explicit CodeBuilder(std::string source_name);

    // ---------------- 四张表 ----------------

    // 常量进常量表，返回槽号。结构上相同的常量恒是同一个槽、因而恒是同一个对象
    [[nodiscard]] std::uint32_t intern_const(const ObjectRef &value);
    // 名字进名字表，返回下标。同名恒同一项
    [[nodiscard]] std::uint32_t intern_name(const std::u32string &name);
    // 嵌套的函数体/类体 Code 进表，返回下标。不去重
    [[nodiscard]] std::uint32_t add_nested_code(Ref<Code> code);

    // 把名字表第 index 项标成引用捕获
    void mark_ref_captured(std::uint32_t index);
    // 追加一个值捕获的名字。顺序就是 MAKE_FUNC/MAKE_CLASS 弹的那份捕获值元组的顺序
    void add_value_captured(const std::u32string &name);
    // 形参形状，只有函数体的 Code 有
    void set_params(Code::ParamShape params);

    // ---------------- 标签 ----------------

    [[nodiscard]] Label new_label();
    // 把标签定在"下一条要发的指令"处
    void place_label(Label label);

    // ---------------- 发指令 ----------------

    // 无参指令
    void emit(OpCode op, Position pos);
    // 带立即数参数的指令
    void emit(OpCode op, std::uint32_t arg, Position pos);
    void emit(OpCode op, UnaryOp arg, Position pos);
    void emit(OpCode op, BinaryOp arg, Position pos);
    // 跳转指令，目标是标签
    void emit_jump(OpCode op, Label target, Position pos);

    // ---------------- 栈 ----------------

    [[nodiscard]] std::uint32_t stack_depth() const { return cur_stack_; }
    // 顺序流还走不走得到这里
    [[nodiscard]] bool unreachable() const { return unreachable_; }

    // ---------------- 产出 ----------------

    [[nodiscard]] Ref<Code> build() &&;
};
