zr_vm_add_unity_test_target(
        zr_vm_language_server_provider_cancellation_test
        ${CMAKE_SOURCE_DIR}/tests/language_server/test_lsp_provider_cancellation.c
)
target_include_directories(zr_vm_language_server_provider_cancellation_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
)
zr_vm_link_language_server(zr_vm_language_server_provider_cancellation_test)
add_test(
        NAME language_server_provider_cancellation
        COMMAND $<TARGET_FILE:zr_vm_language_server_provider_cancellation_test>
)
set_tests_properties(language_server_provider_cancellation PROPERTIES
        ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1"
)
