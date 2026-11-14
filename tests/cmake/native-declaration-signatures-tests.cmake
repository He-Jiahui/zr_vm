add_executable(zr_vm_native_declaration_signatures_test
        ${CMAKE_SOURCE_DIR}/tests/language_server/test_native_declaration_signatures.c)
target_include_directories(zr_vm_native_declaration_signatures_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_common/include
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
        ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server
        ${CMAKE_SOURCE_DIR}/zr_vm_library/include
        ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
        ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/include
        ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
        ${CMAKE_SOURCE_DIR}/zr_vm_lib_network/include)
if (BUILD_SHARED_LIB)
    target_link_libraries(zr_vm_native_declaration_signatures_test PRIVATE
            zr_vm_language_server_shared zr_vm_parser_shared zr_vm_core_shared
            zr_vm_lib_math_shared zr_vm_lib_container_shared zr_vm_lib_system_shared)
    if (TARGET zr_vm_lib_network_shared)
        target_link_libraries(zr_vm_native_declaration_signatures_test PRIVATE zr_vm_lib_network_shared)
        target_compile_definitions(zr_vm_native_declaration_signatures_test PRIVATE ZR_TEST_NATIVE_NETWORK=1)
    endif ()
else ()
    target_link_libraries(zr_vm_native_declaration_signatures_test PRIVATE
            zr_vm_language_server_static zr_vm_parser_static zr_vm_core_static
            zr_vm_lib_math_static zr_vm_lib_container_static zr_vm_lib_system_static)
    if (TARGET zr_vm_lib_network_static)
        target_link_libraries(zr_vm_native_declaration_signatures_test PRIVATE zr_vm_lib_network_static)
        target_compile_definitions(zr_vm_native_declaration_signatures_test PRIVATE ZR_TEST_NATIVE_NETWORK=1)
    endif ()
endif ()
add_test(NAME native_declaration_signatures COMMAND zr_vm_native_declaration_signatures_test)
set_tests_properties(native_declaration_signatures PROPERTIES LABELS "language_server;native" TIMEOUT 30)
