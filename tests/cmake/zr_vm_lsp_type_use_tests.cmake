# 类型使用点的补全与 hover 测试显式带入 canonical completion，验证 parser 事实到 LSP 展示的连接。
zr_vm_add_unity_test_target(
        zr_vm_language_server_type_use_test
        ${CMAKE_SOURCE_DIR}/tests/language_server/test_lsp_type_use.c
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_canonical_completion.c
)
target_include_directories(zr_vm_language_server_type_use_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server
)
zr_vm_link_language_server(zr_vm_language_server_type_use_test)
# 独立 Unity 目标不依赖 stdio 传输，CTest 在语义层直接验证类型投影。
add_test(NAME language_server_type_use COMMAND $<TARGET_FILE:zr_vm_language_server_type_use_test>)
set_tests_properties(language_server_type_use PROPERTIES
        ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1"
)
