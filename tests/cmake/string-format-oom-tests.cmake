# Real Core public formatting/storage APIs; no copied source or mock boundary.
add_executable(zr_vm_string_format_oom_test
        "${CMAKE_SOURCE_DIR}/tests/core/test_string_format_oom.c")
target_include_directories(zr_vm_string_format_oom_test PRIVATE
        "${CMAKE_SOURCE_DIR}/zr_vm_core/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_common/include")
zr_vm_link_core(zr_vm_string_format_oom_test)
set_target_properties(zr_vm_string_format_oom_test PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
if(MSVC)
    target_compile_options(zr_vm_string_format_oom_test PRIVATE /W4 /utf-8)
else()
    target_compile_options(zr_vm_string_format_oom_test PRIVATE -Wall -Wextra -Werror)
endif()
foreach(case IN ITEMS format_push_null vformat_concat_null number_pair_throw short_pair_throw normal_format)
    add_test(NAME core_string_format_oom_${case}
            COMMAND zr_vm_string_format_oom_test ${case})
    set_tests_properties(core_string_format_oom_${case} PROPERTIES TIMEOUT 30)
endforeach()
