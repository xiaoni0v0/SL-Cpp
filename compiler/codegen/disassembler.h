#pragma once

#include <string>

class Code;

/**
 * 反汇编，开发期用。
 *
 * 虚拟机落地之前，这是检查 codegen 产物的唯一手段：测试比对文本、命令行打印中间产物都走这里。
 */

// 完整形态：头部、四张表、指令，嵌套的 Code 逐个跟在后面
[[nodiscard]] std::string disassemble(const Code &code);

// 只有指令，一行一条，形如 "LOAD_CONST 0"。EXTENDED_ARG 前缀折进后面那条指令的参数里，
// 于是测试写的是逻辑指令流，不受参数宽度影响
[[nodiscard]] std::string disassemble_instrs(const Code &code);
