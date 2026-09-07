add_subdirectory(lexer)
add_subdirectory(parser)
add_subdirectory(analyzer)
add_subdirectory(codegen)

sl_add_module(sl_compiler Compiler.cpp Compiler.h)

target_link_libraries(
    sl_compiler
    PUBLIC sl_compiler_analyzer
    PRIVATE sl_cppexceptions sl_utils
)
