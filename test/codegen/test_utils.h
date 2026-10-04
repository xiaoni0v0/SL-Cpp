#pragma once

// 跑完整条流水线（分词、解析、语义检查、常量折叠、代码生成），把产物反汇编成文本来比对。
// 虚拟机还没有，反汇编是检查 codegen 的唯一手段。

#include "../../compiler/Compiler.h"
#include "../../compiler/codegen/disassembler.h"
#include "../../runtime/Runtime.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

// 一个用例起落一次运行时。codegen 要把真 SL 对象放进常量表，所以运行时必须先在
struct RuntimeFixture {
    RuntimeFixture() { Runtime::init(); }
    ~RuntimeFixture() { Runtime::shutdown(); }
    RuntimeFixture(const RuntimeFixture &) = delete;
    RuntimeFixture &operator=(const RuntimeFixture &) = delete;
};

// 指令流，一行一条。EXTENDED_ARG 前缀已经折进后面那条的参数里
inline std::string codegen_instrs(const std::u32string &source) {
    const RuntimeFixture runtime;
    const Ref<Code> code{Compiler::compile(source, "<test>")};

    return disassemble_instrs(*code);
}

// 完整反汇编：头部、四张表、指令，嵌套的 Code 跟在后面
inline std::string codegen_dump(const std::u32string &source) {
    const RuntimeFixture runtime;
    const Ref<Code> code{Compiler::compile(source, "<test>")};

    return disassemble(*code);
}

// 从完整反汇编里把某一段（constants / names / value captured）抽出来，一项一行
inline std::vector<std::string> section_of(const std::string &dump, const std::string &title) {
    std::vector<std::string> result;
    std::size_t at{dump.find(title + ":\n")};
    if (at == std::string::npos) return result;

    at += title.size() + 2;
    // 段里每一项都是 "  序号: 内容"，遇到不以两个空格起头的就是下一段了
    while (at < dump.size() && dump.compare(at, 2, "  ") == 0) {
        const std::size_t line_end{dump.find('\n', at)};
        if (line_end == std::string::npos) break;

        // 只要序号后面那截内容
        const std::size_t colon{dump.find(": ", at)};
        const std::size_t value{
            colon != std::string::npos && colon < line_end ? colon + 2 : at + 2
        };
        result.emplace_back(dump, value, line_end - value);
        at = line_end + 1;
    }

    return result;
}

inline std::vector<std::string> constants_of(const std::u32string &source) {
    return section_of(codegen_dump(source), "constants");
}
