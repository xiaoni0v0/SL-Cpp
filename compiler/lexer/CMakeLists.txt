sl_add_module(
    sl_compiler_lexer
    Lexer.cpp
    Lexer.h
    token.h
    x_keyword.inc
    x_reservedword.inc
    x_token_type.inc
)

target_link_libraries(sl_compiler_lexer PUBLIC sl_cpp_exceptions sl_utils)
