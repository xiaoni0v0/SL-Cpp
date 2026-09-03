add_library(sl_diagnostics INTERFACE)

add_custom_target(sl_diagnostics_headers SOURCES
        EncodingError.h
        FileNotFoundError.h
        InternalError.h
        SLException.h
        SyntaxError.h
)
