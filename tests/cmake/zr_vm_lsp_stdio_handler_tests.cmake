if(TARGET zr_vm_language_server_stdio)
    get_target_property(_zr_vm_stdio_handler_sources zr_vm_language_server_stdio SOURCES)
    # Source properties in this tests directory leave the production target's entrypoint intact.
    set_source_files_properties(
            ${CMAKE_SOURCE_DIR}/zr_vm_language_server/stdio/zr_vm_language_server_stdio.c
            PROPERTIES COMPILE_DEFINITIONS main=zr_tests_stdio_entry
    )
    foreach(_zr_vm_stdio_handler_test IN ITEMS handler_cancellation initialize)
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
        add_test(
                NAME language_server_stdio_${_zr_vm_stdio_handler_test}
                COMMAND $<TARGET_FILE:${_zr_vm_stdio_handler_target}>
        )
        set_tests_properties(language_server_stdio_${_zr_vm_stdio_handler_test} PROPERTIES
                ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1"
        )
    endforeach()
endif()
