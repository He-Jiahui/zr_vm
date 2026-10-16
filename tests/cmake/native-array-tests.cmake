# 原生数组不需要启动 VM；同一组故障用例用于普通 CTest 与独立验证。
add_executable(zr_vm_native_array_failures_test "${ZR_NATIVE_ARRAY_SOURCE_ROOT}/tests/core/test_native_array_failures.c")
target_include_directories(zr_vm_native_array_failures_test PRIVATE
        "${ZR_NATIVE_ARRAY_SOURCE_ROOT}/zr_vm_core/include" "${ZR_NATIVE_ARRAY_SOURCE_ROOT}/zr_vm_common/include")
target_compile_definitions(zr_vm_native_array_failures_test PRIVATE $<$<CONFIG:Debug>:ZR_DEBUG>)
set_property(SOURCE "${ZR_NATIVE_ARRAY_SOURCE_ROOT}/tests/core/test_native_array_failures.c" PROPERTY
        OBJECT_DEPENDS "${ZR_NATIVE_ARRAY_SOURCE_ROOT}/zr_vm_core/include/zr_vm_core/array.h")
set_target_properties(zr_vm_native_array_failures_test PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
if(MSVC)
    target_compile_options(zr_vm_native_array_failures_test PRIVATE /W4 /utf-8)
else()
    target_compile_options(zr_vm_native_array_failures_test PRIVATE -Wall -Wextra -Werror)
endif()
foreach(case IN ITEMS normal init_overflow init_oom push_oom append_oom
        length_overflow capacity_limit free_null_head append_empty growth_saturation
        try_init_failure try_push_failure try_append_failure try_reserve)
    add_test(NAME native_array_${case} COMMAND zr_vm_native_array_failures_test ${case})
    set_tests_properties(native_array_${case} PROPERTIES TIMEOUT 10)
endforeach()
