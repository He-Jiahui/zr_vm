# 原生整数幂不启动 VM；普通 CTest 与独立验证共用此注册。
add_executable(zr_vm_native_math_failures_test
        "${ZR_NATIVE_MATH_SOURCE_ROOT}/tests/core/test_native_math_failures.c")
target_include_directories(zr_vm_native_math_failures_test PRIVATE
        "${ZR_NATIVE_MATH_SOURCE_ROOT}/zr_vm_core/include"
        "${ZR_NATIVE_MATH_SOURCE_ROOT}/zr_vm_common/include")
target_compile_definitions(zr_vm_native_math_failures_test PRIVATE $<$<CONFIG:Debug>:ZR_DEBUG>)
set_property(SOURCE "${ZR_NATIVE_MATH_SOURCE_ROOT}/tests/core/test_native_math_failures.c" PROPERTY
        OBJECT_DEPENDS "${ZR_NATIVE_MATH_SOURCE_ROOT}/zr_vm_core/include/zr_vm_core/math.h")
set_target_properties(zr_vm_native_math_failures_test PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
if(MSVC)
    target_compile_options(zr_vm_native_math_failures_test PRIVATE /W4 /utf-8)
else()
    target_compile_options(zr_vm_native_math_failures_test PRIVATE -Wall -Wextra -Werror)
endif()
foreach(case IN ITEMS uint_zero uint_one_and_precision uint_exact_large uint_accumulated_overflow
        signed_edges signed_min_exact signed_min_overflow)
    add_test(NAME native_math_${case} COMMAND zr_vm_native_math_failures_test ${case})
    set_tests_properties(native_math_${case} PROPERTIES TIMEOUT 10)
endforeach()
