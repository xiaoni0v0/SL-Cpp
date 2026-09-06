# 建一个测试目标：链上被测模块，套测试专用的编译参数，注册进 ctest。
function(sl_add_test_target target)
    cmake_parse_arguments(ARG "" "" "LIBS;SOURCES" ${ARGN})

    add_executable(${target} ${ARG_SOURCES})
    sl_apply_target_options(${target})
    target_link_libraries(${target} PRIVATE ${ARG_LIBS} doctest::doctest)
    target_precompile_headers(${target} PRIVATE <doctest/doctest.h>)

    target_sources(${target} PRIVATE ../main_test.cpp)
    set_source_files_properties(
        ../main_test.cpp
        PROPERTIES SKIP_PRECOMPILE_HEADERS ON
    )

    add_test(NAME ${target} COMMAND ${target})
endfunction()

add_subdirectory(numeric)
add_subdirectory(lexer)
add_subdirectory(parser)
add_subdirectory(analyzer)
