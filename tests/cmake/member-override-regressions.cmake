if(TARGET zr_vm_semantic_query_relations_test)
    add_test(NAME member_override_relations COMMAND zr_vm_semantic_query_relations_test)
    set_tests_properties(member_override_relations PROPERTIES
        TIMEOUT 90 LABELS "gd_h2_e3;parser;member_override")
endif()

if(TARGET zr_vm_cli_executable)
    foreach(case imported local)
        foreach(mode interp compile binary)
            add_test(NAME member_override_${case}_${mode}
                COMMAND ${CMAKE_COMMAND}
                "-DCLI_EXE=$<TARGET_FILE:zr_vm_cli_executable>"
                "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/parser/gd_h2_e3/${case}"
                "-DTEST_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/member_override_${case}_${mode}"
                "-DEXECUTION_MODE=${mode}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_member_override_regression.cmake")
            set_tests_properties(member_override_${case}_${mode} PROPERTIES
                TIMEOUT 90 LABELS "gd_h2_e3;member_override")
        endforeach()
    endforeach()
endif()
