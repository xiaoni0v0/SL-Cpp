# 建一个测试目标：链上被测模块，套测试专用的编译参数，注册进 ctest。
function(sl_add_test_target target)
    cmake_parse_arguments(ARG "" "" "LIBS;SOURCES" ${ARGN})

    add_executable(${target} ${ARG_SOURCES})
    sl_apply_test_target_options(${target})
    target_link_libraries(${target} PRIVATE ${ARG_LIBS} doctest::doctest)
    target_precompile_headers(${target} PRIVATE <doctest/doctest.h>)

    foreach(src IN LISTS ARG_SOURCES)
        if(src MATCHES "main_test\.cpp$")
            # 每个测试目标的 main 都是 doctest 的入口，不走预编译头。
            set_source_files_properties(
                ${src}
                PROPERTIES SKIP_PRECOMPILE_HEADERS ON
            )
        endif()
    endforeach()

    add_test(NAME ${target} COMMAND ${target})
endfunction()

add_subdirectory(numeric)
add_subdirectory(lexer)
add_subdirectory(parser)
add_subdirectory(analyzer)
