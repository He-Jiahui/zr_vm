# Include after ssa-tests.cmake: its manual interprocedural source list also
# consumes the graph helper. Product parser sources use CommonMacros.cmake.
set(_zr_owned_target_source
    ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_call_target.c)
if (TARGET zr_vm_ssa_interprocedural_inlining_test)
    get_target_property(_zr_owned_existing_sources zr_vm_ssa_interprocedural_inlining_test SOURCES)
    if (NOT _zr_owned_target_source IN_LIST _zr_owned_existing_sources)
        target_sources(zr_vm_ssa_interprocedural_inlining_test PRIVATE ${_zr_owned_target_source})
    endif ()
endif ()

if (NOT TARGET zr_vm_ssa_owned_row_direct_call_graph_test)
    add_executable(zr_vm_ssa_owned_row_direct_call_graph_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_owned_row_direct_call_graph.c
        ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
        ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c
        ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
        ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c
        ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
        ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
        ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c
        ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
        ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c
        ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c
        ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c
        ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
        ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
        ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/execution_contract.c
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_call_graph.c
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_call_target.c
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_inline.c
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_devirtualize.c
    )
    target_include_directories(zr_vm_ssa_owned_row_direct_call_graph_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include
        ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_owned_row_direct_call_graph_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_owned_row_direct_call_graph COMMAND zr_vm_ssa_owned_row_direct_call_graph_test)
    set_tests_properties(ssa_owned_row_direct_call_graph PROPERTIES LABELS "ssa")
endif ()
unset(_zr_owned_target_source)
unset(_zr_owned_existing_sources)
