if(TARGET zr_vm_cli_executable)
    set(inline_struct_regressions
        p01_init_only_read p02_control_class_initialized p03_control_class_explicit_ctor
        p04_init_only_via_local p05_store_then_pass_to_fn p06_init_only_pass_to_fn
        p07_store_from_fn_return p08_fnret_into_native_arg p09_class_getter_struct
        p10_V1_original p11_V2_class_top_level
        p14_control_struct_local p15_control_class_scalar p18_method_reads_field_scalar
        p19_fnret_to_local_control p20_struct_in_struct_field_read
        q_m1_method_return_into_class_field q_r4_fnret_to_class_field_readfn
        q_v1_copy_semantics_after_fix q_v2_gc_string_struct_field q_v3_generic_class_struct_field
        r1_native_callable_in_inline_result_slot
        v3_ref_parameter v3_ref_parameter_control
        v4_borrowed_setter v4_borrowed_setter_control)
    foreach(case IN LISTS inline_struct_regressions)
        foreach(mode interp compile binary)
            add_test(NAME inline_struct_${case}_${mode}
                COMMAND ${CMAKE_COMMAND}
                "-DCLI_EXE=$<TARGET_FILE:zr_vm_cli_executable>"
                "-DSOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/parser/gd_h2_e1/${case}.zr"
                "-DTEST_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/inline_struct_${case}_${mode}"
                "-DEXECUTION_MODE=${mode}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_inline_struct_regression.cmake")
            set_tests_properties(inline_struct_${case}_${mode} PROPERTIES
                TIMEOUT 90 LABELS "gd_h2_e1;inline_struct")
        endforeach()
    endforeach()

    file(GLOB e4_regression_sources "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/parser/gd_h2_e4/*.zr")
    foreach(source IN LISTS e4_regression_sources)
        get_filename_component(case "${source}" NAME_WE)
        set(defect R3)
        set(expected_exit 0)
        set(expected_diagnostic "")
        set(expected_result "")
        if(case MATCHES "^(r4_|p17_|x2_|x3_|x6_|w3_)")
            set(defect R4)
        endif()
        if(case STREQUAL "x6_struct_setter_then_return_backing")
            set(expected_result 42)
        elseif(case MATCHES "^(r4_19_|r4_20_)")
            # Array-element receiver typing is the separately diagnosed R7.
            # These assertions require an error; they are ordinary passing tests.
            set(expected_exit 1)
            set(expected_diagnostic "FUNCTION_PRECALL: failed to bind inline receiver/parameters")
        endif()
        foreach(mode interp compile binary)
            add_test(NAME inline_struct_${case}_${mode}
                COMMAND ${CMAKE_COMMAND}
                "-DCLI_EXE=$<TARGET_FILE:zr_vm_cli_executable>"
                "-DSOURCE_FILE=${source}"
                "-DTEST_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/inline_struct_${case}_${mode}"
                "-DEXECUTION_MODE=${mode}"
                "-DEXPECTED_EXIT=${expected_exit}"
                "-DEXPECTED_DIAGNOSTIC=${expected_diagnostic}"
                "-DEXPECTED_RESULT=${expected_result}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_inline_struct_regression.cmake")
            set_tests_properties(inline_struct_${case}_${mode} PROPERTIES
                TIMEOUT 90 LABELS "gd_h2_e4;inline_struct;${defect}")
        endforeach()
    endforeach()
    file(GLOB e5_regression_sources
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/parser/gd_h2_e5/*.zr"
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/parser/gd_h2_e5/*/src/main.zr")
    foreach(source IN LISTS e5_regression_sources)
        get_filename_component(case "${source}" NAME_WE)
        get_filename_component(source_dir "${source}" DIRECTORY)
        set(runner "${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_inline_struct_regression.cmake")
        if(case STREQUAL "main")
            get_filename_component(project_dir "${source_dir}" DIRECTORY)
            get_filename_component(case "${project_dir}" NAME)
            set(runner "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/parser/gd_h2_e5/run_project.cmake")
        endif()
        set(expected_exit 0)
        set(expected_diagnostic "")
        set(expectation positive)
        if(case MATCHES "^r5_10_" OR case STREQUAL "r5_14_untyped_tail_receiver" OR
           case STREQUAL "m1_untyped_param_method" OR case MATCHES "^m1[01]_untyped_native_")
            # R5-U-B: these formerly pinned failures now use the optional receiver ABI.
            set(expectation R5-U-B)
        endif()
        foreach(mode interp compile binary)
            add_test(NAME inline_struct_${case}_${mode}
                COMMAND ${CMAKE_COMMAND}
                "-DCLI_EXE=$<TARGET_FILE:zr_vm_cli_executable>"
                "-DSOURCE_FILE=${source}"
                "-DSOURCE_DIR=${source_dir}"
                "-DTEST_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/inline_struct_${case}_${mode}"
                "-DEXECUTION_MODE=${mode}"
                "-DEXPECTED_EXIT=${expected_exit}"
                "-DEXPECTED_DIAGNOSTIC=${expected_diagnostic}"
                -P "${runner}")
            set_tests_properties(inline_struct_${case}_${mode} PROPERTIES
                TIMEOUT 90 LABELS "gd_h2_e5;inline_struct;R5;${expectation}")
        endforeach()
    endforeach()
endif()
