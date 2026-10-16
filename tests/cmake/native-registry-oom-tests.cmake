# Failure-injection coverage for registry attachment and append ownership.
add_executable(zr_vm_native_registry_oom_test
        "${CMAKE_SOURCE_DIR}/tests/library/test_native_registry_oom.c")
target_include_directories(zr_vm_native_registry_oom_test PRIVATE
        "${CMAKE_SOURCE_DIR}/tests"
        "${CMAKE_SOURCE_DIR}/zr_vm_core/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_common/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_library/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_library/src"
        "${CMAKE_SOURCE_DIR}/zr_vm_library/src/zr_vm_library")
zr_vm_link_parser_core_plus_library(zr_vm_native_registry_oom_test)
set_target_properties(zr_vm_native_registry_oom_test PROPERTIES
        C_STANDARD 11 C_STANDARD_REQUIRED YES)
if (MSVC)
    target_compile_options(zr_vm_native_registry_oom_test PRIVATE /W4 /utf-8)
else ()
    target_compile_options(zr_vm_native_registry_oom_test PRIVATE -Wall -Wextra -Werror)
endif ()
foreach(case IN ITEMS attach_array_1 attach_array_2 attach_array_3
        module_record_growth binding_entry_growth)
    add_test(NAME native_registry_oom_${case}
            COMMAND zr_vm_native_registry_oom_test ${case})
    set_tests_properties(native_registry_oom_${case} PROPERTIES TIMEOUT 30)
endforeach ()

add_dependencies(zr_vm_native_registry_oom_test zr_vm_descriptor_plugin_fixture_int)
set(native_registry_oom_tmp "${CMAKE_CURRENT_BINARY_DIR}/native-registry-oom-tmp")
file(MAKE_DIRECTORY "${native_registry_oom_tmp}")
foreach(case IN ITEMS plugin_handle_growth plugin_string_allocation
        plugin_record_growth plugin_reload_allocation)
    add_test(NAME native_registry_oom_${case}
            COMMAND zr_vm_native_registry_oom_test ${case}
            "$<TARGET_FILE:zr_vm_descriptor_plugin_fixture_int>")
    set_tests_properties(native_registry_oom_${case} PROPERTIES TIMEOUT 30
            ENVIRONMENT "TMP=${native_registry_oom_tmp};TEMP=${native_registry_oom_tmp};TMPDIR=${native_registry_oom_tmp}")
endforeach ()
