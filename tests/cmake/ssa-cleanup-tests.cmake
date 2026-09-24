# Conditional cleanup uses the same production dispatcher and allocation-fault
# wrapper as oracle resume; no runtime or native provider is mocked here.
if (NOT TARGET zr_vm_ssa_conditional_cleanup_test)
    get_target_property(zr_cleanup_oracle_sources zr_vm_ssa_oracle_resume_test SOURCES)
    list(REMOVE_ITEM zr_cleanup_oracle_sources
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_oracle_resume.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c)
    add_executable(zr_vm_ssa_conditional_cleanup_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_conditional_cleanup.c
            ${CMAKE_SOURCE_DIR}/tests/parser/ssa_owner_fault_allocator.c
            ${zr_cleanup_oracle_sources})
    zr_vm_apply_common_test_settings(zr_vm_ssa_conditional_cleanup_test)
    target_include_directories(zr_vm_ssa_conditional_cleanup_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_conditional_cleanup_test PRIVATE UNITY_INCLUDE_CONFIG_H)
    zr_link_third_party_for_target(zr_vm_ssa_conditional_cleanup_test "zr_unity")
    add_test(NAME ssa_conditional_cleanup COMMAND zr_vm_ssa_conditional_cleanup_test)
    set_tests_properties(ssa_conditional_cleanup PROPERTIES LABELS "ssa" TIMEOUT 30)
endif ()
