add_executable(zr_vm_native_string_publication_oom_test
        "${CMAKE_SOURCE_DIR}/tests/library/test_native_string_publication_oom.c")
target_include_directories(zr_vm_native_string_publication_oom_test PRIVATE
        "${CMAKE_SOURCE_DIR}/zr_vm_core/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_common/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_library/include")
zr_vm_link_parser_core_plus_library(zr_vm_native_string_publication_oom_test)
set_target_properties(zr_vm_native_string_publication_oom_test PROPERTIES
        C_STANDARD 11 C_STANDARD_REQUIRED YES)
if(MSVC)
    target_compile_options(zr_vm_native_string_publication_oom_test PRIVATE /W4 /utf-8)
else()
    target_compile_options(zr_vm_native_string_publication_oom_test PRIVATE -Wall -Wextra -Werror)
endif()
foreach(case IN ITEMS short_rehash long_payload normal_controls)
    add_test(NAME native_string_publication_${case}
            COMMAND zr_vm_native_string_publication_oom_test ${case})
    set_tests_properties(native_string_publication_${case} PROPERTIES TIMEOUT 30)
endforeach()
