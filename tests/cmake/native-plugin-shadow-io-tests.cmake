# Private stream helper is provided by the actual Library after mechanical extraction.
add_executable(zr_vm_native_plugin_shadow_io_test
        "${CMAKE_SOURCE_DIR}/tests/library/test_native_plugin_shadow_io.c")
target_include_directories(zr_vm_native_plugin_shadow_io_test PRIVATE
        "${CMAKE_SOURCE_DIR}/zr_vm_core/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_common/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_library/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_library/src/zr_vm_library")
zr_vm_link_parser_core_plus_library(zr_vm_native_plugin_shadow_io_test)
set_target_properties(zr_vm_native_plugin_shadow_io_test PROPERTIES
        C_STANDARD 11 C_STANDARD_REQUIRED YES)
if (MSVC)
    target_compile_options(zr_vm_native_plugin_shadow_io_test PRIVATE /W4 /utf-8)
else ()
    target_compile_options(zr_vm_native_plugin_shadow_io_test PRIVATE -Wall -Wextra -Werror)
endif ()
foreach(case IN ITEMS open_failure target_close_failure)
    set(shadow_io_workspace "${CMAKE_CURRENT_BINARY_DIR}/native-plugin-shadow-io-tmp/${case}")
    file(MAKE_DIRECTORY "${shadow_io_workspace}/zr_vm_native_plugin_cache")
    add_test(NAME native_plugin_shadow_io_${case}
            COMMAND zr_vm_native_plugin_shadow_io_test ${case} "${shadow_io_workspace}")
    set_tests_properties(native_plugin_shadow_io_${case} PROPERTIES TIMEOUT 30
            WORKING_DIRECTORY "${shadow_io_workspace}"
            ENVIRONMENT "TMP=${shadow_io_workspace};TEMP=${shadow_io_workspace};TMPDIR=${shadow_io_workspace}")
endforeach ()
