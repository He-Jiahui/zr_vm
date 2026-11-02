if(TARGET zr_vm_cli_executable)
    foreach(case expression assignment field_construct constructor_contracts local_native_control)
        foreach(mode interp compile binary)
            set(test_name imported_struct_init_${case}_${mode})
            add_test(NAME ${test_name}
                COMMAND ${CMAKE_COMMAND}
                "-DCLI_EXE=$<TARGET_FILE:zr_vm_cli_executable>"
                "-DSOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/parser/gd_h2_e2/${case}.zr"
                "-DSUPPORT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/parser/gd_h2_e2/modules"
                "-DTEST_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/${test_name}"
                "-DEXECUTION_MODE=${mode}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_inline_struct_regression.cmake")
            set_tests_properties(${test_name} PROPERTIES
                TIMEOUT 90 LABELS "gd_h2_e2;inline_struct")
        endforeach()
    endforeach()
endif()
