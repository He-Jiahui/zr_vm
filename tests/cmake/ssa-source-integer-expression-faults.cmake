# A separate producer observes expression windows without the literal-only shim.
if (TARGET zr_vm_ssa_source_integer_expression_faults_test OR
        NOT TARGET zr_vm_ssa_source_returned_execir_retention_test)
    return()
endif ()
include("${CMAKE_CURRENT_LIST_DIR}/ssa-source-integer-expression-support.cmake")

add_library(zr_vm_ssa_integer_fault_private_parser OBJECT
        ${_zr_returned_execir_private_parser_sources})
add_library(zr_vm_ssa_integer_fault_producer OBJECT "${_zr_returned_execir_producer}")
foreach (_target IN ITEMS zr_vm_ssa_integer_fault_private_parser zr_vm_ssa_integer_fault_producer)
    zr_ssa_integer_copy_settings(${_target} zr_vm_ssa_returned_execir_private_parser zr_vm_parser)
endforeach ()
if (MSVC)
    target_compile_options(zr_vm_ssa_integer_fault_private_parser PRIVATE
            "/FI${_zr_returned_execir_root}/tests/parser/support/ssa_source_integer_expression_faults_graph_shim.h")
    target_compile_options(zr_vm_ssa_integer_fault_producer PRIVATE
            "/FI${_zr_returned_execir_root}/tests/parser/support/ssa_source_integer_expression_faults_producer_shim.h")
else ()
    target_compile_options(zr_vm_ssa_integer_fault_private_parser PRIVATE -include
            "${_zr_returned_execir_root}/tests/parser/support/ssa_source_integer_expression_faults_graph_shim.h")
    target_compile_options(zr_vm_ssa_integer_fault_producer PRIVATE -include
            "${_zr_returned_execir_root}/tests/parser/support/ssa_source_integer_expression_faults_producer_shim.h")
endif ()

add_executable(zr_vm_ssa_source_integer_expression_faults_test
        "${_zr_returned_execir_root}/tests/parser/test_ssa_source_integer_expression_faults.c"
        "${_zr_returned_execir_root}/tests/parser/support/ssa_source_integer_expression_faults_fixture.c"
        "${_zr_returned_execir_root}/tests/parser/support/ssa_source_returned_crt_ledger.c"
        "${_zr_returned_execir_root}/tests/parser/support/ssa_source_returned_retention_fixture.c"
        "${_zr_returned_execir_root}/tests/parser/support/ssa_source_returned_execir_cleanup.c"
        "${_zr_returned_execir_root}/tests/core/support/ssa_exec_ir_owned_module_allocator.c"
        "${_zr_returned_execir_root}/tests/core/support/ssa_exec_ir_owner_capture.c")
zr_ssa_integer_copy_settings(zr_vm_ssa_source_integer_expression_faults_test
        zr_vm_ssa_source_returned_execir_retention_test zr_vm_ssa_source_integer_expression_faults_test)
zr_ssa_integer_link_observed_source(zr_vm_ssa_source_integer_expression_faults_test
        zr_vm_ssa_integer_fault_private_parser zr_vm_ssa_integer_fault_producer)
zr_ssa_integer_register_observed(zr_vm_ssa_source_integer_expression_faults_test
        ssa_source_integer_expression_faults source-integer-expression-faults-link.map)
