if (NOT TARGET zr_vm_ssa_aot_scalar_arithmetic_test)
    add_executable(zr_vm_ssa_aot_scalar_arithmetic_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_ssa_aot_scalar_arithmetic.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/aot_ir.c
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
            ${CMAKE_SOURCE_DIR}/zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_text.c
            ${CMAKE_SOURCE_DIR}/zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_arithmetic.c
            ${CMAKE_SOURCE_DIR}/zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_conditional.c
            ${CMAKE_SOURCE_DIR}/zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_c.c
            ${CMAKE_SOURCE_DIR}/zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_llvm.c)
    target_include_directories(zr_vm_ssa_aot_scalar_arithmetic_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include
            ${CMAKE_SOURCE_DIR}/zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot)
    target_compile_definitions(zr_vm_ssa_aot_scalar_arithmetic_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    zr_vm_apply_common_test_settings(zr_vm_ssa_aot_scalar_arithmetic_test)
    add_test(NAME ssa_aot_scalar_arithmetic COMMAND zr_vm_ssa_aot_scalar_arithmetic_test)
    set_tests_properties(ssa_aot_scalar_arithmetic PROPERTIES LABELS "ssa")
endif ()
