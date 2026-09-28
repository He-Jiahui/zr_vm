# 本文件也编译 wasm_exports.cpp 的宿主测试；仅在测试目录尚无 C++ 编译器时启用语言。
if (NOT CMAKE_CXX_COMPILER_LOADED)
    enable_language(CXX)
endif ()

# 响应封装先用小型 C 可执行文件检查 JSON 所有权与分配失败，不需要启动浏览器 worker。
add_executable(zr_vm_language_server_wasm_response_test
        ${CMAKE_SOURCE_DIR}/tests/language_server/test_wasm_response.c
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/wasm/wasm_response.c
)
target_include_directories(zr_vm_language_server_wasm_response_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_common/include
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/wasm
)
zr_link_third_party_for_target(zr_vm_language_server_wasm_response_test "zr_c_json")
# 与含 VM 的导出测试分开报告，便于定位 JSON 封装层的退化。
add_test(NAME language_server_wasm_response
        COMMAND $<TARGET_FILE:zr_vm_language_server_wasm_response_test>)

# 宿主测试把 WASM 导出和诊断投影直接编入可执行文件，以检查文档快照与 JSON 响应契约。
zr_vm_add_support_target(zr_vm_language_server_wasm_exports_test
        ${CMAKE_SOURCE_DIR}/tests/language_server/test_wasm_exports.c
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/wasm/wasm_exports.cpp
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/wasm/wasm_diagnostic_json.cpp
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/wasm/wasm_response.c
)
# TODO: 此目标只在宿主编译器下定义 ZR_WASM_BUILD，未覆盖 __EMSCRIPTEN__ 和最终导出表；
# 发布前须由真实 Emscripten 构建或 worker 探针验证浏览器 ABI。
target_compile_definitions(zr_vm_language_server_wasm_exports_test PRIVATE ZR_WASM_BUILD)
# 宿主 C++ 编译导出胶水时沿用当前 C 兼容宽松选项；这些选项不属于产品 WASM 构建契约。
# TODO: 选项只按 CXX 语言过滤，未按编译器过滤；需在 MSVC 目标上验证 -fpermissive 等选项的兼容性。
target_compile_options(zr_vm_language_server_wasm_exports_test PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:-fpermissive>"
        "$<$<COMPILE_LANGUAGE:CXX>:-Wno-error>"
        "$<$<COMPILE_LANGUAGE:CXX>:-Wno-c++11-narrowing>"
)
# 宿主 C++ 编译单元包含 C 风格声明；按语言映射线程局部与对齐关键字以编译导出胶水。
target_compile_definitions(zr_vm_language_server_wasm_exports_test PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:_Thread_local=thread_local>"
        "$<$<COMPILE_LANGUAGE:CXX>:_Alignof=alignof>"
)
# 解析导出文件的 private include 与语言服务器/JSON 依赖只授予宿主测试目标。
target_include_directories(zr_vm_language_server_wasm_exports_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/wasm
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include
        ${CMAKE_SOURCE_DIR}/zr_vm_library/include
)
zr_vm_link_language_server(zr_vm_language_server_wasm_exports_test)
zr_link_third_party_for_target(zr_vm_language_server_wasm_exports_test "zr_c_json")
# 单独命名该 CTest，区分宿主 ABI 胶水失败与轻量响应封装失败。
add_test(NAME language_server_wasm_exports
        COMMAND $<TARGET_FILE:zr_vm_language_server_wasm_exports_test>)
