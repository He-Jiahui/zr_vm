if (NOT CMAKE_CXX_COMPILER_LOADED)
    enable_language(CXX)
endif ()

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
add_test(NAME language_server_wasm_response
        COMMAND $<TARGET_FILE:zr_vm_language_server_wasm_response_test>)

zr_vm_add_support_target(zr_vm_language_server_wasm_exports_test
        ${CMAKE_SOURCE_DIR}/tests/language_server/test_wasm_exports.c
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/wasm/wasm_exports.cpp
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/wasm/wasm_diagnostic_json.cpp
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/wasm/wasm_response.c
)
target_compile_definitions(zr_vm_language_server_wasm_exports_test PRIVATE ZR_WASM_BUILD)
target_compile_options(zr_vm_language_server_wasm_exports_test PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:-fpermissive>"
        "$<$<COMPILE_LANGUAGE:CXX>:-Wno-error>"
        "$<$<COMPILE_LANGUAGE:CXX>:-Wno-c++11-narrowing>"
)
target_compile_definitions(zr_vm_language_server_wasm_exports_test PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:_Thread_local=thread_local>"
        "$<$<COMPILE_LANGUAGE:CXX>:_Alignof=alignof>"
)
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
add_test(NAME language_server_wasm_exports
        COMMAND $<TARGET_FILE:zr_vm_language_server_wasm_exports_test>)
