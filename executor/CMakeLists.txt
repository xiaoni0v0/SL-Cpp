sl_add_module(sl_executor Executor.cpp Executor.h)

target_link_libraries(sl_executor PUBLIC sl_compiler_analyzer sl_utils)
