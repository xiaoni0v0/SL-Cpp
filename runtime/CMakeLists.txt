sl_add_module(
    sl_runtime
    Heap.cpp
    Heap.h
    host_error_conversion.cpp
    host_error_conversion.h
    Object.cpp
    Object.h
    RaisedException.h
    Runtime.cpp
    Runtime.h
    Type.cpp
    Type.h
    x_builtin_types.inc
    objects/Decimal.cpp
    objects/Decimal.h
    objects/BaseException.cpp
    objects/BaseException.h
    objects/Bool.cpp
    objects/Bool.h
    objects/Code.cpp
    objects/Code.h
    objects/Int.cpp
    objects/Int.h
    objects/Str.cpp
    objects/Str.h
    objects/Tuple.cpp
    objects/Tuple.h
    objects/NamedSingleton.cpp
    objects/NamedSingleton.h
)

target_link_libraries(
    sl_runtime
    PUBLIC sl_numeric sl_utils
    PRIVATE sl_cpp_exceptions
)
