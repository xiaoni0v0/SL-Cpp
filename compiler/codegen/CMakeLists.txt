sl_add_module(
    sl_compiler_codegen
    CodeBuilder.cpp
    CodeBuilder.h
    CodeGen.cpp
    CodeGen.h
    ConstPool.cpp
    ConstPool.h
    disassembler.cpp
    disassembler.h
    opcode.h
    x_opcodes.inc
    x_unary_ops.inc
    x_binary_ops.inc
    bytecode.md
)

target_link_libraries(
    sl_compiler_codegen
    PUBLIC sl_runtime sl_compiler_parser
    PRIVATE sl_cppexceptions
)
