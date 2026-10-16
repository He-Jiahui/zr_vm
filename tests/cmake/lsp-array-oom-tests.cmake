if ((TARGET zr_vm_language_server_shared OR TARGET zr_vm_language_server_static) AND
    (TARGET zr_vm_parser_shared OR TARGET zr_vm_parser_static) AND
    (TARGET zr_vm_core_shared OR TARGET zr_vm_core_static))
    zr_vm_add_support_target(
            zr_vm_language_server_lsp_array_oom_test
            ${CMAKE_SOURCE_DIR}/tests/language_server/test_lsp_array_oom.c
    )
    target_include_directories(zr_vm_language_server_lsp_array_oom_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server
    )
    zr_vm_link_language_server(zr_vm_language_server_lsp_array_oom_test)
    foreach (zr_vm_lsp_array_oom_case IN ITEMS
            cache_new
            symbol_table_new
            reference_tracker_new
            symbol_new
            add_reference
            add_reference_symbol
            symbol_reference
            snapshot_capture)
        add_test(
                NAME language_server_lsp_array_oom_${zr_vm_lsp_array_oom_case}
                COMMAND $<TARGET_FILE:zr_vm_language_server_lsp_array_oom_test>
                        ${zr_vm_lsp_array_oom_case}
        )
        set_tests_properties(
                language_server_lsp_array_oom_${zr_vm_lsp_array_oom_case}
                PROPERTIES TIMEOUT 30
        )
    endforeach ()
endif ()
