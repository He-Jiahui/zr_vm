if(TARGET zr_vm_cli_executable)
    foreach(case IN ITEMS destructor_stack_growth)
        foreach(mode IN ITEMS interp binary)
            add_test(NAME swarm_${case}_${mode}
                COMMAND ${CMAKE_COMMAND}
                "-DCLI_EXE=$<TARGET_FILE:zr_vm_cli_executable>"
                "-DSOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/parser/swarm_${case}.zr"
                "-DTEST_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/swarm_${case}_${mode}"
                "-DEXECUTION_MODE=${mode}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_swarm_regression.cmake")
            set_tests_properties(swarm_${case}_${mode} PROPERTIES TIMEOUT 90)
        endforeach()
    endforeach()
endif()
