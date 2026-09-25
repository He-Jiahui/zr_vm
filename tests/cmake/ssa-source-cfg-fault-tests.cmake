# Scratch-allocation fault injection around the actual source finalizer.
if (NOT TARGET zr_vm_ssa_source_cfg_faults_test)
    zr_vm_add_unity_test_target(zr_vm_ssa_source_cfg_faults_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_source_cfg_faults.c)
    if (WIN32 AND BUILD_SHARED_LIB)
        target_sources(zr_vm_parser_shared PRIVATE
                ${CMAKE_SOURCE_DIR}/tests/parser/ssa_source_cfg_faults.c)
    else ()
        target_sources(zr_vm_ssa_source_cfg_faults_test PRIVATE
                ${CMAKE_SOURCE_DIR}/tests/parser/ssa_source_cfg_faults.c)
    endif ()
    target_include_directories(zr_vm_ssa_source_cfg_faults_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include)
    zr_vm_link_parser_core_plus_library(zr_vm_ssa_source_cfg_faults_test)
    add_test(NAME ssa_source_cfg_faults COMMAND zr_vm_ssa_source_cfg_faults_test)
    set_tests_properties(ssa_source_cfg_faults PROPERTIES LABELS "ssa")
endif ()
