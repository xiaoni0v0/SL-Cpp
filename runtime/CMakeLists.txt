sl_add_module(
    sl_runtime
    Heap.cpp
    Heap.h
    HostErrorConversion.cpp
    HostErrorConversion.h
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
    objects/Exception.cpp
    objects/Exception.h
    objects/Int.cpp
    objects/Int.h
    objects/Str.cpp
    objects/Str.h
    objects/Tuple.cpp
    objects/Tuple.h
    objects/singletons.cpp
    objects/singletons.h
)

target_link_libraries(
    sl_runtime
    PUBLIC sl_numeric sl_cpp_exceptions
    PRIVATE sl_utils
)
