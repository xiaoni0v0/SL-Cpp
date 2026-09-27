add_library(sl_cppexceptions INTERFACE)
target_link_libraries(sl_cppexceptions INTERFACE sl_common)

add_custom_target(
    sl_cppexceptions_headers
    SOURCES
        EncodingError.h
        FileNotFoundError.h
        InternalError.h
        SLException.h
        SyntaxError.h
)
