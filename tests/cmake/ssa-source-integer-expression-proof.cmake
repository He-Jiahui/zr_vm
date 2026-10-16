# The matrix mutates actual compiler inputs immediately around the real finalizer.
if (TARGET zr_vm_ssa_source_integer_expression_proof_test OR
        NOT TARGET zr_vm_ssa_source_returned_execir_retention_test)
    return()
endif ()
include("${CMAKE_CURRENT_LIST_DIR}/ssa-source-integer-expression-support.cmake")

add_library(zr_vm_ssa_integer_proof_producer OBJECT "${_zr_returned_execir_producer}")
zr_ssa_integer_copy_settings(zr_vm_ssa_integer_proof_producer
        zr_vm_ssa_returned_execir_private_parser zr_vm_parser)
if (MSVC)
    target_compile_options(zr_vm_ssa_integer_proof_producer PRIVATE
            "/FI${_zr_returned_execir_root}/tests/parser/support/ssa_source_integer_expression_proof_shim.h")
else ()
    target_compile_options(zr_vm_ssa_integer_proof_producer PRIVATE -include
            "${_zr_returned_execir_root}/tests/parser/support/ssa_source_integer_expression_proof_shim.h")
endif ()

add_executable(zr_vm_ssa_source_integer_expression_proof_test
        "${_zr_returned_execir_root}/tests/parser/test_ssa_source_integer_expression_proof.c"
        "${_zr_returned_execir_root}/tests/parser/support/ssa_source_returned_crt_ledger.c"
        "${_zr_returned_execir_root}/tests/parser/support/ssa_source_returned_retention_fixture.c"
        "${_zr_returned_execir_root}/tests/core/support/ssa_exec_ir_owned_module_allocator.c"
        "${_zr_returned_execir_root}/tests/core/support/ssa_exec_ir_owner_capture.c")
zr_ssa_integer_copy_settings(zr_vm_ssa_source_integer_expression_proof_test
        zr_vm_ssa_source_returned_execir_retention_test zr_vm_ssa_source_integer_expression_proof_test)
zr_ssa_integer_link_observed_source(zr_vm_ssa_source_integer_expression_proof_test
        zr_vm_ssa_returned_execir_private_parser zr_vm_ssa_integer_proof_producer)
zr_ssa_integer_register_observed(zr_vm_ssa_source_integer_expression_proof_test
        ssa_source_integer_expression_proof source-integer-expression-proof-link.map)
