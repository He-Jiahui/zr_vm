zr_vm_add_unity_test_target(
        zr_vm_parser_recovery_ownership_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_parser_recovery_ownership.c
)
target_include_directories(zr_vm_parser_recovery_ownership_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include
)
zr_vm_link_parser_core(zr_vm_parser_recovery_ownership_test)
add_test(
        NAME parser_recovery_ownership
        COMMAND $<TARGET_FILE:zr_vm_parser_recovery_ownership_test>
)
set_tests_properties(parser_recovery_ownership PROPERTIES
        ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1"
)
