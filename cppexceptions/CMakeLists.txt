add_library(sl_cppexceptions INTERFACE)

add_custom_target(
    sl_cppexceptions_headers
    SOURCES
        EncodingError.h
        FileNotFoundError.h
        InternalError.h
        SLException.h
        SourceLocation.h
        SyntaxError.h
)
