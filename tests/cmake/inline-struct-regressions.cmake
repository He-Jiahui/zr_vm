if(TARGET zr_vm_cli_executable)
    set(inline_struct_regressions
        p01_init_only_read p02_control_class_initialized p03_control_class_explicit_ctor
        p04_init_only_via_local p05_store_then_pass_to_fn p06_init_only_pass_to_fn
        p07_store_from_fn_return p08_fnret_into_native_arg p09_class_getter_struct
        p14_control_struct_local p15_control_class_scalar p18_method_reads_field_scalar
        p19_fnret_to_local_control p20_struct_in_struct_field_read
        q_m1_method_return_into_class_field q_r4_fnret_to_class_field_readfn
        q_v1_copy_semantics_after_fix q_v2_gc_string_struct_field q_v3_generic_class_struct_field
        r1_native_callable_in_inline_result_slot)
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
endif()
