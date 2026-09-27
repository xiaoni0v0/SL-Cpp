#include "CodeBuilder.h"

#include "../../cppexceptions/InternalError.h"
#include "../../runtime/Object.h"
#include "../../runtime/objects/Code.h"
#include "../../runtime/objects/Str.h"
#include "../parser/ast_nodes/ast_nodes.h"
#include "opcode.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

// 参数值 value 要占几条真指令（含它自己那条），差额就是 EXTENDED_ARG 前缀数
std::uint32_t instr_width(const std::uint32_t value) {
    if (value <= kMaxInlineArg) return 1;
    if (value <= 0xFFFFu) return 2;
    if (value <= 0xFFFFFFu) return 3;

    return 1 + kMaxExtendedArgs;
}

// 每条伪指令的起始 pc，末项是总长度
std::vector<std::uint32_t> compute_offsets(const std::vector<std::uint32_t> &widths) {
    std::vector<std::uint32_t> offsets(widths.size() + 1, 0);
    for (std::size_t i{0}; i < widths.size(); ++i) offsets[i + 1] = offsets[i] + widths[i];

    return offsets;
}

// 栈深 depth 加上有符号增量 delta。结果为负是操作数栈弹空，超出 uint32 的范围同样不可能是正常代码
std::uint32_t shift_depth(const std::uint32_t depth, const std::int64_t delta) {
    const std::int64_t result{static_cast<std::int64_t>(depth) + delta};
    if (result < 0) throw InternalError{"CodeBuilder: operand stack underflow"};
    if (std::cmp_greater(result, UINT32_MAX))
        throw InternalError{"CodeBuilder: operand stack overflow"};

    return static_cast<std::uint32_t>(result);
}

std::uint16_t encode_instr(const OpCode op, const std::uint32_t low_byte) {
    return static_cast<std::uint16_t>((static_cast<std::uint32_t>(op) << 8u) | (low_byte & 0xFFu));
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
    assert(!label_targets_[label]);

    label_targets_[label] = static_cast<std::uint32_t>(instrs_.size());

    if (const std::optional<std::uint32_t> known{label_stacks_[label]}) {
        // 有人跳到这里，栈深以跳转方为准
        if (!unreachable_ && cur_stack_ != *known)
            throw InternalError{"CodeBuilder: stack depth mismatch at label"};
        cur_stack_ = *known;
        max_stack_ = std::max(max_stack_, cur_stack_);
        unreachable_ = false;
    } else if (!unreachable_) {
        // 只有顺序流走得到这里
        label_stacks_[label] = cur_stack_;
    }

    // 既没人跳过来、顺序流也走不到：这段确实不可达，保持 unreachable_
}

void CodeBuilder::apply_effect(const std::int64_t effect) {
    cur_stack_ = shift_depth(cur_stack_, effect);
    max_stack_ = std::max(max_stack_, cur_stack_);
}

void CodeBuilder::note_label_stack(const Label label, const std::uint32_t depth) {
    std::optional<std::uint32_t> &known{label_stacks_[label]};
    if (known && *known != depth) throw InternalError{"CodeBuilder: stack depth mismatch at label"};

    // 标签已经放过却没记下栈深，说明放它的时候顺序流走不到：往回跳到这里，
    // 中间那段就被当成不可达跳过了栈跟踪
    if (!known && label_targets_[label])
        throw InternalError{"CodeBuilder: backward jump to untracked label"};

    known = depth;
}

void CodeBuilder::push_instr(const PseudoInstr &instr) {
    instrs_.push_back(instr);

    // 不可达的代码不参与栈跟踪
    if (unreachable_) return;

    apply_effect(stack_effect(instr.op_, instr.arg_));
    if (is_terminator(instr.op_)) unreachable_ = true;
}

void CodeBuilder::emit(const OpCode op, const Position pos) {
    assert(opcode_arg_kind(op) == ArgKind::None);

    push_instr(PseudoInstr{.op_ = op, .pos_ = pos});
}

void CodeBuilder::emit(const OpCode op, const std::uint32_t arg, const Position pos) {
    assert(opcode_arg_kind(op) != ArgKind::None);
    assert(opcode_arg_kind(op) != ArgKind::Jump && "use emit_jump");
    assert(
        opcode_arg_kind(op) != ArgKind::UnaryOp && opcode_arg_kind(op) != ArgKind::BinaryOp &&
        "use the enum overload"
    );

    push_instr(PseudoInstr{.op_ = op, .arg_ = arg, .pos_ = pos});
}

void CodeBuilder::emit(const OpCode op, const UnaryOp arg, const Position pos) {
    assert(opcode_arg_kind(op) == ArgKind::UnaryOp);

    push_instr(PseudoInstr{.op_ = op, .arg_ = static_cast<std::uint32_t>(arg), .pos_ = pos});
}

void CodeBuilder::emit(const OpCode op, const BinaryOp arg, const Position pos) {
    assert(opcode_arg_kind(op) == ArgKind::BinaryOp);

    push_instr(PseudoInstr{.op_ = op, .arg_ = static_cast<std::uint32_t>(arg), .pos_ = pos});
}

void CodeBuilder::emit_jump(const OpCode op, const Label target, const Position pos) {
    assert(opcode_arg_kind(op) == ArgKind::Jump);
    assert(target < label_targets_.size());

    const bool tracked{!unreachable_};
    instrs_.push_back(PseudoInstr{.op_ = op, .arg_ = target, .arg_is_label_ = true, .pos_ = pos});
    if (!tracked) return;

    // 跳转指令的顺序效应不依赖参数
    apply_effect(stack_effect(op, 0));
    note_label_stack(target, shift_depth(cur_stack_, stack_effect_extra_on_jump(op)));
    if (is_terminator(op)) unreachable_ = true;
}

Ref<Code> CodeBuilder::build() && {
    // 顺序流不能走出末尾，否则虚拟机的 pc 会越界。gen_program 保证末尾有 return
    if (!unreachable_) throw InternalError{"CodeBuilder: control falls off the end"};

    const std::size_t count{instrs_.size()};

    // 每条伪指令展开成几条真指令。非跳转的参数是定值，一次算定
    std::vector<std::uint32_t> widths(count, 1);
    for (std::size_t i{0}; i < count; ++i)
        if (!instrs_[i].arg_is_label_) widths[i] = instr_width(instrs_[i].arg_);

    std::vector<std::uint32_t> offsets{compute_offsets(widths)};

    // 跳转目标由前面所有指令的宽度决定，而补前缀又会把目标推远。前缀只增不减，必收敛
    for (bool changed{true}; changed;) {
        changed = false;

        for (std::size_t i{0}; i < count; ++i) {
            if (!instrs_[i].arg_is_label_) continue;

            const std::optional<std::uint32_t> &target{label_targets_[instrs_[i].arg_]};
            if (!target) throw InternalError{"CodeBuilder: jump to unplaced label"};

            if (const std::uint32_t want{instr_width(offsets[*target])}; want > widths[i]) {
                widths[i] = want;
                changed = true;
            }
        }

        if (changed) offsets = compute_offsets(widths);
    }

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
            lines.push_back(
                Code::LineEntry{.pc = offsets[i], .row = instr.pos_.row, .col = instr.pos_.col}
            );

        // 按宽度展开：先由高到低发前缀，最后发指令自己
        for (std::uint32_t k{widths[i] - 1}; k > 0; --k)
            bytecode.push_back(encode_instr(OpCode::EXTENDED_ARG, arg >> (8u * k)));
        bytecode.push_back(encode_instr(instr.op_, arg));
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
