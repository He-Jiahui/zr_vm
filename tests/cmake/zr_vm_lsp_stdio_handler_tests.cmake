if(TARGET zr_vm_language_server_stdio)
    get_target_property(_zr_vm_stdio_handler_sources zr_vm_language_server_stdio SOURCES)
    zr_vm_add_unity_test_target(
            zr_vm_language_server_stdio_handler_cancellation_test
            ${CMAKE_SOURCE_DIR}/tests/language_server/test_stdio_handler_cancellation.c
            ${_zr_vm_stdio_handler_sources}
    )
    # Source properties in this tests directory leave the production target's entrypoint intact.
    set_source_files_properties(
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/stdio/zr_vm_language_server_stdio.c
            PROPERTIES COMPILE_DEFINITIONS main=zr_tests_stdio_entry
    )
    target_include_directories(zr_vm_language_server_stdio_handler_cancellation_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/stdio
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server
    )
    zr_vm_link_language_server(zr_vm_language_server_stdio_handler_cancellation_test)
    zr_link_third_party_for_target(zr_vm_language_server_stdio_handler_cancellation_test "zr_c_json")
    add_test(
            NAME language_server_stdio_handler_cancellation
            COMMAND $<TARGET_FILE:zr_vm_language_server_stdio_handler_cancellation_test>
    )
    set_tests_properties(language_server_stdio_handler_cancellation PROPERTIES
            ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1"
    )
endif()
