zr_vm_add_support_target(
        zr_vm_language_server_position_codec_line_index_test
        ${CMAKE_SOURCE_DIR}/tests/language_server/test_lsp_position_codec_line_index.c
)
target_include_directories(zr_vm_language_server_position_codec_line_index_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include
)
zr_vm_link_language_server(zr_vm_language_server_position_codec_line_index_test)
add_test(NAME language_server_position_codec_line_index
        COMMAND $<TARGET_FILE:zr_vm_language_server_position_codec_line_index_test>)
