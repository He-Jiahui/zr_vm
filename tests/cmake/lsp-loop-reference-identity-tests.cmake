zr_vm_add_support_target(
        zr_vm_language_server_loop_reference_identity_test
        ${CMAKE_SOURCE_DIR}/tests/language_server/test_lsp_loop_reference_identity.c
)
target_include_directories(zr_vm_language_server_loop_reference_identity_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include
)
zr_vm_link_language_server(zr_vm_language_server_loop_reference_identity_test)
add_test(NAME language_server_loop_reference_identity
        COMMAND $<TARGET_FILE:zr_vm_language_server_loop_reference_identity_test>)

if (TARGET zr_vm_language_server_stdio AND ZR_VM_NODE_EXECUTABLE)
    add_test(NAME language_server_stdio_loop_reference_identity
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/language_server/stdio_loop_reference_identity_smoke.js
                    $<TARGET_FILE:zr_vm_language_server_stdio>)
    set_tests_properties(language_server_stdio_loop_reference_identity PROPERTIES TIMEOUT 60)
endif ()
