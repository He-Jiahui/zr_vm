zr_vm_add_support_target(
        zr_vm_language_server_inlay_fallback_ast_test
        ${CMAKE_SOURCE_DIR}/tests/language_server/test_lsp_inlay_fallback_ast.c
)
target_include_directories(zr_vm_language_server_inlay_fallback_ast_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include
)
zr_vm_link_language_server(zr_vm_language_server_inlay_fallback_ast_test)
add_test(NAME language_server_inlay_fallback_ast
        COMMAND $<TARGET_FILE:zr_vm_language_server_inlay_fallback_ast_test>)
