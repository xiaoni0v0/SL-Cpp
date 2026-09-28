sl_add_module(
    sl_common
    common.cpp
    common.h
    index_of.h
    overflow.h
    Position.h
)

target_link_libraries(sl_common PUBLIC SafeInt)
