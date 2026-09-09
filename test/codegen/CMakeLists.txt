sl_add_test_target(
    sl_test_codegen
    LIBS sl_compiler
    SOURCES
        test_utils.h
        literal_const_test.cpp
        operator_test.cpp
        container_test.cpp
        assign_test.cpp
        control_flow_test.cpp
        try_test.cpp
        call_func_class_test.cpp
        encoding_test.cpp
)
