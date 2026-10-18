add_executable(zr_vm_function_instruction_verification_test
        "${CMAKE_SOURCE_DIR}/tests/core/test_function_instruction_verification.c")
target_include_directories(zr_vm_function_instruction_verification_test PRIVATE
        "${CMAKE_SOURCE_DIR}/zr_vm_common/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core")
zr_vm_link_core(zr_vm_function_instruction_verification_test)
set_target_properties(zr_vm_function_instruction_verification_test PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
add_test(NAME function_instruction_verify COMMAND zr_vm_function_instruction_verification_test)
