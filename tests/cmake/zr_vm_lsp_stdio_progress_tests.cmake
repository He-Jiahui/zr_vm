# 进度与请求注册表一起编入同一 Unity 目标，检查取消 token 生命周期和通知投影。
zr_vm_add_unity_test_target(
        zr_vm_language_server_stdio_request_progress_test
        ${CMAKE_SOURCE_DIR}/tests/language_server/test_stdio_request_progress.c
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/stdio/stdio_request_progress.c
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/stdio/stdio_request_registry.c
)
target_include_directories(zr_vm_language_server_stdio_request_progress_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/stdio
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server
)
zr_vm_link_language_server(zr_vm_language_server_stdio_request_progress_test)
zr_link_third_party_for_target(zr_vm_language_server_stdio_request_progress_test "zr_c_json")
# 单独注册进度回归，防止其他 stdio 夹具的全局请求状态影响结果。
add_test(
        NAME language_server_stdio_request_progress
        COMMAND $<TARGET_FILE:zr_vm_language_server_stdio_request_progress_test>
)
set_tests_properties(language_server_stdio_request_progress PROPERTIES
        ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1"
)
