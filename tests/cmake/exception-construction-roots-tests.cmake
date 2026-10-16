add_executable(zr_vm_exception_construction_roots_test
        "${CMAKE_SOURCE_DIR}/tests/core/test_exception_construction_roots.c")
target_include_directories(zr_vm_exception_construction_roots_test PRIVATE
        "${CMAKE_SOURCE_DIR}/zr_vm_core/include"
        "${CMAKE_SOURCE_DIR}/zr_vm_common/include")
zr_vm_link_core(zr_vm_exception_construction_roots_test)
set_target_properties(zr_vm_exception_construction_roots_test PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
if(MSVC)
    target_compile_options(zr_vm_exception_construction_roots_test PRIVATE /W4 /utf-8)
else()
    target_compile_options(zr_vm_exception_construction_roots_test PRIVATE -Wall -Wextra -Werror)
endif()
foreach(case IN ITEMS error_during_frames_gc frames_during_key_gc normal_control)
    add_test(NAME core_exception_construction_roots_${case}
            COMMAND zr_vm_exception_construction_roots_test ${case})
    set_tests_properties(core_exception_construction_roots_${case} PROPERTIES TIMEOUT 30)
endforeach()
