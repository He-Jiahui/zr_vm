# 原生 stdio 可执行目标可用时才展开处理器回归矩阵；BUILD_LANGUAGE_SERVER_STDIO=OFF 不注册这些测试。
if(TARGET zr_vm_language_server_stdio)
    get_target_property(_zr_vm_stdio_handler_sources zr_vm_language_server_stdio SOURCES)
    # 测试副本链接完整 stdio 源集，却保留测试文件自己的 main；改名只作用于 tests 目录中的源属性。
    # 生产目标在 zr_vm_language_server 目录创建，入口不应受测试目录的源属性影响。
    set_source_files_properties(
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/stdio/zr_vm_language_server_stdio.c
            PROPERTIES COMPILE_DEFINITIONS main=zr_tests_stdio_entry
    )
    # 每个 handler 场景独占进程和 Unity 状态，复用相同生产源及 JSON 依赖。
    foreach(_zr_vm_stdio_handler_test IN ITEMS
            handler_cancellation initialize transport_output diagnostic_publication diagnostic_json document_close)
        set(_zr_vm_stdio_handler_target zr_vm_language_server_stdio_${_zr_vm_stdio_handler_test}_test)
        zr_vm_add_unity_test_target(
                ${_zr_vm_stdio_handler_target}
                ${CMAKE_SOURCE_DIR}/tests/language_server/test_stdio_${_zr_vm_stdio_handler_test}.c
                ${_zr_vm_stdio_handler_sources}
        )
        target_include_directories(${_zr_vm_stdio_handler_target} PRIVATE
                ${CMAKE_SOURCE_DIR}/zr_vm_language_server/stdio
                ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
                ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server
        )
        zr_vm_link_language_server(${_zr_vm_stdio_handler_target})
        zr_link_third_party_for_target(${_zr_vm_stdio_handler_target} "zr_c_json")
        # 用生成表达式绑定目标产物，避免多配置生成器把 CTest 指到错误配置。
        add_test(
                NAME language_server_stdio_${_zr_vm_stdio_handler_test}
                COMMAND $<TARGET_FILE:${_zr_vm_stdio_handler_target}>
        )
        # 每个进程内的 UBSan 故障直接使对应 CTest 失败。
        set_tests_properties(language_server_stdio_${_zr_vm_stdio_handler_test} PROPERTIES
                ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1"
        )
    endforeach()
endif()
