#include "disassembler.h"

#include "../../runtime/Object.h"
#include "../../runtime/objects/Bool.h"
#include "../../runtime/objects/Code.h"
#include "../../runtime/objects/Decimal.h"
#include "../../runtime/objects/Int.h"
#include "../../runtime/objects/NamedSingleton.h"
#include "../../runtime/objects/Str.h"
#include "../../runtime/objects/Tuple.h"
#include "../../utils/string_utils.h"
#include "opcode.h"

#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <vector>

namespace {

// 一条逻辑指令：前缀累积完之后的样子
struct DecodedInstr {
    std::uint32_t pc_{0}; // 起始位置，指向第一条 EXTENDED_ARG 前缀
    OpCode op_{};
    std::uint32_t arg_{0}; // 累积完的完整参数
};

std::vector<DecodedInstr> decode(const std::vector<std::uint16_t> &bytecode) {
    std::vector<DecodedInstr> result;
    std::uint32_t accumulated{0};
    std::uint32_t start{0};

    for (std::uint32_t pc{0}; pc < bytecode.size(); ++pc) {
        const auto op{static_cast<OpCode>(bytecode[pc] >> 8u)};
        const std::uint32_t low{bytecode[pc] & 0xFFu};

        if (op == OpCode::EXTENDED_ARG) {
            accumulated = (accumulated << 8u) | low;
            continue;
        }

        result.push_back(DecodedInstr{.pc_ = start, .op_ = op, .arg_ = (accumulated << 8u) | low});
        accumulated = 0;
        start = pc + 1;
    }

    return result;
}

std::string escape_str(const std::u32string &value) {
    std::string result;

    for (const char32_t ch : value) {
        switch (ch) {
        case U'\\':
            result += "\\\\";
            break;
        case U'\'':
            result += "\\'";
            break;
        case U'\n':
            result += "\\n";
            break;
        case U'\r':
            result += "\\r";
            break;
        case U'\t':
            result += "\\t";
            break;
        default:
            result += u32_to_utf8(ch);
        }
    }

    return result;
}

// 常量表一项长什么样。带类型前缀，因为去重按结构标识，1 / True / 1.0 是三个不同的槽
std::string const_repr(const Object *value) {
    if (!value) return "<null>";

    if (const auto *number{dynamic_cast<const Int *>(value)})
        return std::format("int {}", number->value().to_decimal_string());
    if (const auto *number{dynamic_cast<const Decimal *>(value)})
        return std::format("decimal {}", number->value().to_string());
    if (const auto *flag{dynamic_cast<const Bool *>(value)})
        return std::format("bool {}", flag->value() ? "True" : "False");
    if (const auto *text{dynamic_cast<const Str *>(value)})
        return std::format("str '{}'", escape_str(text->value()));
    if (const auto *singleton{dynamic_cast<const NamedSingleton *>(value)})
        return singleton->name();

    if (const auto *tuple{dynamic_cast<const Tuple *>(value)}) {
        std::string result{"tuple ("};
        for (std::size_t i{0}; i < tuple->size(); ++i) {
            if (i > 0) result += ", ";
            result += const_repr(tuple->at(i));
        }

        return result + ")";
    }

    return "<unknown>";
}

std::string name_at(const Code &code, const std::uint32_t index) {
    if (index >= code.names_table().size()) return "<out of range>";

    return code.names_table()[index]->to_utf8();
}

// 参数后面跟的那句说明。没什么可说的就返回空
std::string arg_comment(const Code &code, const DecodedInstr &instr) {
    const std::uint32_t arg{instr.arg_};

    switch (opcode_arg_kind(instr.op_)) {
    case ArgKind::Const:
        return arg < code.constants_table().size()
                   ? const_repr(code.constants_table()[arg].target())
                   : "<out of range>";
    case ArgKind::Name:
        return name_at(code, arg);
    case ArgKind::NestedCode:
        return std::format("code[{}]", arg);
    case ArgKind::Raw:
        if (instr.op_ == OpCode::UNPACK_EX)
            return std::format("{} before, {} after", arg & 0xFFu, arg >> 8u);
        if (instr.op_ == OpCode::LOAD_GL) return arg == 0 ? "_L" : "_G";
        return {};
    // 运算符直接印在参数位上，不用再说明一遍
    case ArgKind::UnaryOp:
    case ArgKind::BinaryOp:
    case ArgKind::None:
    case ArgKind::Count:
    case ArgKind::Depth:
    case ArgKind::Jump:
        return {};
    }

    return {};
}

// 一条指令的 "助记符 参数" 部分，无参指令只有助记符
std::string instr_text(const DecodedInstr &instr) {
    const char *name{opcode_name(instr.op_)};

    switch (opcode_arg_kind(instr.op_)) {
    case ArgKind::None:
        return name;
    // 运算符印名字不印编号：那个编号是字节码格式的内部编码，读的人不认识
    case ArgKind::UnaryOp:
        return std::format("{} {}", name, unary_op_name(static_cast<UnaryOp>(instr.arg_)));
    case ArgKind::BinaryOp:
        return std::format("{} {}", name, binary_op_name(static_cast<BinaryOp>(instr.arg_)));
    default:
        return std::format("{} {}", name, instr.arg_);
    }
}

// 一份 Code 自己的完整文本，不含嵌套的
std::string dump_one(const Code &code, const std::string &title) {
    std::string out{std::format("=== {} ===  max_stack={}\n", title, code.max_stack())};

    if (!code.constants_table().empty()) {
        out += "constants:\n";
        for (std::size_t i{0}; i < code.constants_table().size(); ++i)
            out += std::format("  {}: {}\n", i, const_repr(code.constants_table()[i].target()));
    }

    if (!code.names_table().empty()) {
        out += "names:\n";
        for (std::size_t i{0}; i < code.names_table().size(); ++i) {
            const std::string mark{code.is_ref_captured(i) ? " (ref captured)" : ""};
            out += std::format("  {}: {}{}\n", i, code.names_table()[i]->to_utf8(), mark);
        }
    }

    if (!code.value_captured().empty()) {
        out += "value captured:\n";
        for (std::size_t i{0}; i < code.value_captured().size(); ++i)
            out += std::format("  {}: {}\n", i, code.value_captured()[i]->to_utf8());
    }

    out += "code:\n";
    for (const DecodedInstr &instr : decode(code.bytecode())) {
        const std::string comment{arg_comment(code, instr)};
        out += std::format(
            "{:>6}  {}{}\n",
            instr.pc_,
            instr_text(instr),
            comment.empty() ? "" : std::format("  ; {}", comment)
        );
    }

    return out;
}

// 嵌套的 Code 逐个跟在后面，标题带上从根算起的路径
std::string dump_nested(const Code &code, const std::string &title) {
    std::string out;

    for (std::size_t i{0}; i < code.codes_table().size(); ++i) {
        const Code &nested{*code.codes_table()[i]};
        const std::string nested_title{std::format("{}.code[{}]", title, i)};

        out += '\n';
        out += dump_one(nested, nested_title);
        out += dump_nested(nested, nested_title);
    }

    return out;
}

} // namespace

std::string disassemble(const Code &code) {
    return dump_one(code, "<main>") + dump_nested(code, "<main>");
}

std::string disassemble_instrs(const Code &code) {
    std::string out;
    for (const DecodedInstr &instr : decode(code.bytecode())) out += instr_text(instr) + '\n';

    return out;
}
