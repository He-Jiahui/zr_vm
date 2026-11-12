zr_vm_add_support_target(
        zr_vm_fused_add_operand_packing_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_fused_add_operand_packing.c
)
zr_vm_link_parser_core(zr_vm_fused_add_operand_packing_test)
add_test(NAME fused_add_operand_packing COMMAND zr_vm_fused_add_operand_packing_test)
set_tests_properties(fused_add_operand_packing PROPERTIES TIMEOUT 60 LABELS "parser;optimizer")

if(TARGET zr_vm_cli_executable)
    foreach(case IN ITEMS repro callback_result_repro)
        foreach(mode IN ITEMS interp binary)
            add_test(NAME fused_add_loop_${case}_${mode}
                    COMMAND ${CMAKE_COMMAND}
                    "-DCLI_EXE=$<TARGET_FILE:zr_vm_cli_executable>"
                    "-DSOURCE_FILE=${CMAKE_SOURCE_DIR}/tests/fixtures/parser/interp_fused_add_loop/${case}.zr"
                    "-DTEST_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/fused_add_loop_${case}_${mode}"
                    "-DEXECUTION_MODE=${mode}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_fused_add_loop.cmake")
            set_tests_properties(fused_add_loop_${case}_${mode} PROPERTIES TIMEOUT 60 LABELS "parser;cli")
        endforeach()
    endforeach()
endif()
