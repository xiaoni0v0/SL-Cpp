add_library(sl_cpp_exceptions INTERFACE)

add_custom_target(sl_cpp_exceptions_headers SOURCES
        EncodingError.h
        FileNotFoundError.h
        InternalError.h
        SLException.h
        SyntaxError.h
)
