# Regressions for the externally reviewed build and bytecode/runtime defects.
zr_vm_add_unity_test_target(zr_vm_identity_protocol_unicode_test
        ${CMAKE_SOURCE_DIR}/tests/core/test_identity_protocol_unicode.c)
zr_vm_link_parser_core(zr_vm_identity_protocol_unicode_test)
target_include_directories(zr_vm_identity_protocol_unicode_test PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/include)
add_test(NAME identity_protocol_unicode COMMAND zr_vm_identity_protocol_unicode_test)

zr_vm_add_unity_test_target(zr_vm_io_validation_test
        ${CMAKE_SOURCE_DIR}/tests/core/test_io_validation.c)
zr_vm_link_parser_core(zr_vm_io_validation_test)
target_include_directories(zr_vm_io_validation_test PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/include)
add_test(NAME bytecode_validation COMMAND zr_vm_io_validation_test)

if (TARGET zr_vm_language_server_shared OR TARGET zr_vm_language_server_static)
    zr_vm_add_unity_test_target(zr_vm_lsp_workspace_path_security_test
            ${CMAKE_SOURCE_DIR}/tests/language_server/test_lsp_workspace_path_security.c)
    zr_vm_link_language_server(zr_vm_lsp_workspace_path_security_test)
    target_include_directories(zr_vm_lsp_workspace_path_security_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include)
    add_test(NAME lsp_workspace_path_security COMMAND zr_vm_lsp_workspace_path_security_test)
endif ()

if (TARGET zr_vm_lib_network_shared OR TARGET zr_vm_lib_network_static)
    zr_vm_add_unity_test_target(zr_vm_debug_token_generation_test
            ${CMAKE_SOURCE_DIR}/tests/debug/test_debug_token_generation.c)
    zr_vm_link_parser_core(zr_vm_debug_token_generation_test)
    if (TARGET zr_vm_lib_network_shared)
        target_link_libraries(zr_vm_debug_token_generation_test PRIVATE zr_vm_lib_network_shared)
    else ()
        target_link_libraries(zr_vm_debug_token_generation_test PRIVATE zr_vm_lib_network_static)
    endif ()
    target_include_directories(zr_vm_debug_token_generation_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_network/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include)
    add_test(NAME debug_token_generation COMMAND zr_vm_debug_token_generation_test)
endif ()

add_test(NAME build_macro_keyword_link_contract
        COMMAND ${CMAKE_COMMAND}
        -S ${CMAKE_SOURCE_DIR}/tests/cmake/build-macro-contract
        -B ${CMAKE_BINARY_DIR}/tests_generated/build-macro-contract
        -G "${CMAKE_GENERATOR}"
        "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
        "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}")
