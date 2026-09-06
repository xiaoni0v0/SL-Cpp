sl_add_module(
    sl_utils
    file_utils.cpp
    file_utils.h
    memory_utils.h
    string_utils.cpp
    string_utils.h
)

target_link_libraries(sl_utils PUBLIC sl_cpp_exceptions)
