# 数值/位置解析测试单独编入 stdio 解析源，另链接语言服务器与 JSON 依赖，保留独立 Unity 入口。
zr_vm_add_unity_test_target(
        zr_vm_language_server_stdio_lsp_parse_test
        ${CMAKE_SOURCE_DIR}/tests/language_server/test_stdio_lsp_parse.c
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/stdio/stdio_lsp_parse.c
)
target_include_directories(zr_vm_language_server_stdio_lsp_parse_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/stdio
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server
)
zr_vm_link_language_server(zr_vm_language_server_stdio_lsp_parse_test)
zr_link_third_party_for_target(zr_vm_language_server_stdio_lsp_parse_test "zr_c_json")
# 非 Windows 构建显式补 libm，兼容解析源使用的浮点分类接口。
if (NOT WIN32)
    target_link_libraries(zr_vm_language_server_stdio_lsp_parse_test PRIVATE m)
endif ()
# CTest 与其它 stdio 处理器分开执行解析边界回归；UBSan 错误按失败处理。
add_test(
        NAME language_server_stdio_lsp_parse
        COMMAND $<TARGET_FILE:zr_vm_language_server_stdio_lsp_parse_test>
)
set_tests_properties(language_server_stdio_lsp_parse PROPERTIES
        ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1"
)
