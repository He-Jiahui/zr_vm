# Keep the capability-validation test focused while linking the real Apply path.
if (TARGET zr_vm_ssa_capability_validation_test)
    target_sources(zr_vm_ssa_capability_validation_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_rollback.c)
endif ()
