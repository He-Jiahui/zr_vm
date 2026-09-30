if (NOT TARGET zr_vm_ssa_cfg_effects_faults_test)
    set(_zr_vm_ssa_cfg_effects_fault_sources ${_zr_vm_ssa_builder_sources})
    list(REMOVE_ITEM _zr_vm_ssa_cfg_effects_fault_sources
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c)
    list(APPEND _zr_vm_ssa_cfg_effects_fault_sources
            ${CMAKE_SOURCE_DIR}/tests/parser/ssa_cfg_effects_fault_core.c)

    add_executable(zr_vm_ssa_cfg_effects_faults_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_cfg_effects_faults.c
            ${_zr_vm_ssa_cfg_effects_fault_sources})
    target_include_directories(zr_vm_ssa_cfg_effects_faults_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/parser
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
    target_compile_definitions(zr_vm_ssa_cfg_effects_faults_test PRIVATE
            _CRT_SECURE_NO_WARNINGS)
    add_test(NAME ssa_cfg_effects_faults
            COMMAND zr_vm_ssa_cfg_effects_faults_test)
    set_tests_properties(ssa_cfg_effects_faults PROPERTIES LABELS "ssa")
endif ()
