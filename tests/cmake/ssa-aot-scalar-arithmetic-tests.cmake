get_filename_component(ZR_SSA_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

if (NOT TARGET zr_vm_ssa_aot_scalar_arithmetic_test)
    add_executable(zr_vm_ssa_aot_scalar_arithmetic_test
            ${ZR_SSA_SOURCE_ROOT}/tests/parser/test_ssa_aot_scalar_arithmetic.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_core/src/zr_vm_core/aot_ir.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_text.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_arithmetic.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_conditional.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_c.c
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_llvm.c)
    target_include_directories(zr_vm_ssa_aot_scalar_arithmetic_test PRIVATE
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_parser/include
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_core/include
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_common/include
            ${ZR_SSA_SOURCE_ROOT}/zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot)
    target_compile_definitions(zr_vm_ssa_aot_scalar_arithmetic_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    zr_vm_apply_common_test_settings(zr_vm_ssa_aot_scalar_arithmetic_test)
    add_test(NAME ssa_aot_scalar_arithmetic COMMAND zr_vm_ssa_aot_scalar_arithmetic_test)
    set_tests_properties(ssa_aot_scalar_arithmetic PROPERTIES LABELS "ssa")
endif ()
