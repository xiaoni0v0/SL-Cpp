sl_add_module(sl_compiler_codegen ConstPool.cpp ConstPool.h bytecode.md)

target_link_libraries(
    sl_compiler_codegen
    PUBLIC sl_runtime
    PRIVATE sl_cppexceptions
)
