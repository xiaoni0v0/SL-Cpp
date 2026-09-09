#include "CodeBuilder.h"

#include <algorithm>
#include <cassert>
#include <utility>

namespace {

// 参数值 value 要占几条真指令（含它自己那条），差额就是 EXTENDED_ARG 前缀数
std::uint32_t instr_width(const std::uint32_t value) {
    if (value <= 0xFFu) return 1;
    if (value <= 0xFFFFu) return 2;
    if (value <= 0xFFFFFFu) return 3;

    return 1 + nMaxExtendedArgs;
}

std::uint16_t encode_instr(const OpCode op, const std::uint32_t low_byte) {
    return static_cast<std::uint16_t>((static_cast<std::uint32_t>(op) << 8u) | (low_byte & 0xFFu));
}

// 按 width 展开一条指令：先由高到低发前缀，最后发指令自己
void write_instr(
    std::vector<std::uint16_t> &out, const OpCode op, const std::uint32_t arg,
    const std::uint32_t width
) {
    for (std::uint32_t i{width - 1}; i > 0; --i)
        out.push_back(encode_instr(OpCode::EXTENDED_ARG, arg >> (8u * i)));
    out.push_back(encode_instr(op, arg));
}

} // namespace

CodeBuilder::CodeBuilder(std::string source_name) : source_name_{std::move(source_name)} {}

std::uint32_t CodeBuilder::intern_const(const ObjectRef &value) {
    return const_pool_.intern(value);
}

std::uint32_t CodeBuilder::intern_name(const std::u32string &name) {
    if (const auto found{name_indices_.find(name)}; found != name_indices_.end())
        return found->second;

    const auto index{static_cast<std::uint32_t>(names_.size())};
    names_.push_back(make_ref<Str>(name));
    name_indices_.emplace(name, index);

    return index;
}

std::uint32_t CodeBuilder::add_nested_code(Ref<Code> code) {
    const auto index{static_cast<std::uint32_t>(nested_codes_.size())};
    nested_codes_.push_back(std::move(code));

    return index;
}

void CodeBuilder::mark_ref_captured(const std::uint32_t index) {
    assert(index < names_.size());

    if (index >= ref_captured_.size()) ref_captured_.resize(index + 1, false);
    ref_captured_[index] = true;
}

void CodeBuilder::add_value_captured(const std::u32string &name) {
    value_captured_.push_back(make_ref<Str>(name));
}

void CodeBuilder::set_params(Code::ParamShape params) { params_ = std::move(params); }

CodeBuilder::Label CodeBuilder::new_label() {
    const auto label{static_cast<Label>(label_targets_.size())};
    label_targets_.emplace_back();
    label_stacks_.emplace_back();

    return label;
}

void CodeBuilder::place_label(const Label label) {
    assert(label < label_targets_.size());
    assert(!label_targets_[label] && "一个标签只能放置一次");

    label_targets_[label] = static_cast<std::uint32_t>(instrs_.size());

    if (const std::optional<std::uint32_t> known{label_stacks_[label]}) {
        // 有人跳到这里，栈深以跳转方为准
        assert((unreachable_ || cur_stack_ == *known) && "标签处两条路径的栈深不一致");
        cur_stack_ = *known;
        max_stack_ = std::max(max_stack_, cur_stack_);
        unreachable_ = false;
    } else if (!unreachable_) {
        // 只有顺序流走得到这里
        label_stacks_[label] = cur_stack_;
    }

    // 既没人跳过来、顺序流也走不到：这段确实不可达，保持 unreachable_
}

void CodeBuilder::apply_effect(const int effect) {
    assert(static_cast<int>(cur_stack_) + effect >= 0 && "操作数栈被弹空了");

    cur_stack_ = static_cast<std::uint32_t>(static_cast<int>(cur_stack_) + effect);
    max_stack_ = std::max(max_stack_, cur_stack_);
}

void CodeBuilder::note_label_stack(const Label label, const std::uint32_t depth) {
    std::optional<std::uint32_t> &known{label_stacks_[label]};
    assert((!known || *known == depth) && "同一个标签处两条路径的栈深不一致");

    known = depth;
}

void CodeBuilder::push_instr(PseudoInstr instr) {
    const OpCode op{instr.op_};
    const std::uint32_t arg{instr.arg_};
    instrs_.push_back(std::move(instr));

    // 不可达的代码不参与栈跟踪
    if (unreachable_) return;

    apply_effect(stack_effect(op, arg));
    if (is_terminator(op)) unreachable_ = true;
}

void CodeBuilder::emit(const OpCode op, const Position pos) {
    assert(opcode_arg_kind(op) == ArgKind::None);

    push_instr(PseudoInstr{op, 0, false, pos});
}

void CodeBuilder::emit(const OpCode op, const std::uint32_t arg, const Position pos) {
    assert(opcode_arg_kind(op) != ArgKind::None && "无参指令别传参数");
    assert(opcode_arg_kind(op) != ArgKind::Jump && "跳转指令走 emit_jump");
    assert(
        opcode_arg_kind(op) != ArgKind::UnaryOp && opcode_arg_kind(op) != ArgKind::BinaryOp &&
        "运算符指令传枚举，别传裸数"
    );

    push_instr(PseudoInstr{op, arg, false, pos});
}

void CodeBuilder::emit(const OpCode op, const UnaryOp arg, const Position pos) {
    assert(opcode_arg_kind(op) == ArgKind::UnaryOp);

    push_instr(PseudoInstr{op, static_cast<std::uint32_t>(arg), false, pos});
}

void CodeBuilder::emit(const OpCode op, const BinaryOp arg, const Position pos) {
    assert(opcode_arg_kind(op) == ArgKind::BinaryOp);

    push_instr(PseudoInstr{op, static_cast<std::uint32_t>(arg), false, pos});
}

void CodeBuilder::emit_jump(const OpCode op, const Label target, const Position pos) {
    assert(opcode_arg_kind(op) == ArgKind::Jump);
    assert(target < label_targets_.size());

    const bool tracked{!unreachable_};
    instrs_.push_back(PseudoInstr{op, target, true, pos});
    if (!tracked) return;

    // 跳转指令的顺序效应不依赖参数
    apply_effect(stack_effect(op, 0));
    note_label_stack(
        target,
        static_cast<std::uint32_t>(static_cast<int>(cur_stack_) + stack_effect_extra_on_jump(op))
    );
    if (is_terminator(op)) unreachable_ = true;
}

Ref<Code> CodeBuilder::build() && {
    const std::size_t count{instrs_.size()};

    // 每条伪指令展开成几条真指令。非跳转的参数是定值，一次算定
    std::vector<std::uint32_t> widths(count, 1);
    for (std::size_t i{0}; i < count; ++i)
        if (!instrs_[i].arg_is_label_) widths[i] = instr_width(instrs_[i].arg_);

    // 每条伪指令的起始 pc，末项是总长度
    std::vector<std::uint32_t> offsets(count + 1, 0);
    const auto recompute_offsets{[&] {
        for (std::size_t i{0}; i < count; ++i) offsets[i + 1] = offsets[i] + widths[i];
    }};

    // 跳转目标由前面所有指令的宽度决定，而补前缀又会把目标推远。前缀只增不减，必收敛
    for (bool changed{true}; changed;) {
        changed = false;
        recompute_offsets();

        for (std::size_t i{0}; i < count; ++i) {
            if (!instrs_[i].arg_is_label_) continue;

            const std::optional<std::uint32_t> &target{label_targets_[instrs_[i].arg_]};
            assert(target && "跳转指向了一个没放置过的标签");

            if (const std::uint32_t want{instr_width(offsets[*target])}; want > widths[i]) {
                widths[i] = want;
                changed = true;
            }
        }
    }
    recompute_offsets();

    std::vector<std::uint16_t> bytecode;
    bytecode.reserve(offsets[count]);
    std::vector<Code::LineEntry> lines;

    for (std::size_t i{0}; i < count; ++i) {
        const PseudoInstr &instr{instrs_[i]};
        const std::uint32_t arg{
            instr.arg_is_label_ ? offsets[*label_targets_[instr.arg_]] : instr.arg_
        };

        // 行位置只在变化时记一项
        if (lines.empty() || lines.back().row != instr.pos_.row ||
            lines.back().col != instr.pos_.col)
            lines.push_back(Code::LineEntry{offsets[i], instr.pos_.row, instr.pos_.col});

        write_instr(bytecode, instr.op_, arg, widths[i]);
    }

    return make_ref<Code>(Code::Parts{
        .bytecode = std::move(bytecode),
        .constants_table = const_pool_.take_table(),
        .codes_table = std::move(nested_codes_),
        .names_table = std::move(names_),
        .max_stack = max_stack_,
        .ref_captured = std::move(ref_captured_),
        .value_captured = std::move(value_captured_),
        .params = std::move(params_),
        .lines_table = std::move(lines),
        .source_name = std::move(source_name_),
    });
}
