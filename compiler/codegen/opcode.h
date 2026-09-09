#pragma once

#include <cstdint>
#include <utility>

/**
 * 操作码。一条指令固定 2 字节：高字节是它，低字节是参数。
 *
 * 全集与语义见 x_opcodes.inc 和 bytecode.md。
 */
enum class OpCode : std::uint8_t {
#define X(id, mnemonic, argkind, effect) id,
#include "x_opcodes.inc"

#undef X
};

// UNARY_OP 的参数
enum class UnaryOp : std::uint8_t {
#define X(id, mnemonic) id,
#include "x_unary_ops.inc"

#undef X
};

// BINARY_OP 的参数
enum class BinaryOp : std::uint8_t {
#define X(id, mnemonic) id,
#include "x_binary_ops.inc"

#undef X
};

/**
 * 参数怎么解读。只影响反汇编的呈现，不影响编码。
 */
enum class ArgKind : std::uint8_t {
    None,       // 无参，恒填 0
    Raw,        // 裸值，自己解释（EXTENDED_ARG、UNPACK_EX 的打包值、LOAD_GL 的 0/1）
    Count,      // 个数
    Depth,      // 从栈顶数第几项，1 即栈顶
    Const,      // 常量表下标
    Name,       // 名字表下标
    NestedCode, // 嵌套 Code 表下标
    Jump,       // 跳转目标，绝对、以指令为单位
    UnaryOp,    // UnaryOp
    BinaryOp,   // BinaryOp
};

// 一条指令自带的参数字节能装下的最大值。再大要靠 EXTENDED_ARG 前缀
inline constexpr std::uint32_t nMaxInlineArg{0xFF};

// EXTENDED_ARG 前缀最多连用几条。加上指令自己那一字节，参数最宽 32 位
inline constexpr std::uint32_t nMaxExtendedArgs{3};

[[nodiscard]] constexpr const char *opcode_name(const OpCode op) {
    switch (op) {
#define X(id, mnemonic, argkind, effect)                                                           \
    case OpCode::id:                                                                               \
        return mnemonic;
#include "x_opcodes.inc"

#undef X
    }

    std::unreachable();
}

[[nodiscard]] constexpr const char *unary_op_name(const UnaryOp op) {
    switch (op) {
#define X(id, mnemonic)                                                                            \
    case UnaryOp::id:                                                                              \
        return mnemonic;
#include "x_unary_ops.inc"

#undef X
    }

    std::unreachable();
}

[[nodiscard]] constexpr const char *binary_op_name(const BinaryOp op) {
    switch (op) {
#define X(id, mnemonic)                                                                            \
    case BinaryOp::id:                                                                             \
        return mnemonic;
#include "x_binary_ops.inc"

#undef X
    }

    std::unreachable();
}

[[nodiscard]] constexpr ArgKind opcode_arg_kind(const OpCode op) {
    switch (op) {
#define X(id, mnemonic, argkind, effect)                                                           \
    case OpCode::id:                                                                               \
        return ArgKind::argkind;
#include "x_opcodes.inc"

#undef X
    }

    std::unreachable();
}

// 顺序执行完这条指令，操作数栈的深度变化
[[nodiscard]] constexpr int stack_effect(const OpCode op, const std::uint32_t arg) {
    switch (op) {
#define X(id, mnemonic, argkind, effect)                                                           \
    case OpCode::id:                                                                               \
        return (effect);
#include "x_opcodes.inc"

#undef X
    }

    std::unreachable();
}

// 跳转那条路径比顺序流多出来的栈深变化，也就是"跳到目标处的栈深"减去"顺序走下去的栈深"
[[nodiscard]] constexpr int stack_effect_extra_on_jump(const OpCode op) {
    switch (op) {
    // 顺序流压入取到的元素，耗尽那条路反而弹掉迭代器
    case OpCode::FOR_ITER:
        return -2;
    // 处理器入口的栈上多一个异常对象。finally 体入口则是"这条路径要带出去的那个值"，
    // 异常路径带的正是异常对象，深度跟别的路径对得上
    case OpCode::SETUP_EXCEPT:
    case OpCode::SETUP_FINALLY:
        return +1;
    default:
        return 0;
    }
}

// 这条指令之后，顺序流还会不会继续
[[nodiscard]] constexpr bool is_terminator(const OpCode op) {
    switch (op) {
    case OpCode::JUMP:
    case OpCode::RETURN_VALUE:
    case OpCode::RAISE:
    case OpCode::RERAISE:
    // 跳回调用点或者继续往外传播，两条路都不是顺序往下
    case OpCode::END_FINALLY:
        return true;
    default:
        return false;
    }
}
