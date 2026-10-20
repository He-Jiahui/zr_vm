include(${CMAKE_SOURCE_DIR}/zr_vm_common/CommonMacros.cmake)

# Release 会定义 NDEBUG，assert() 连同参数里的调用一起被抹掉。
# 测试把必要的构造放在 assert() 里；这些调用必须留下，否则 Release 会跳过初始化然后崩溃。
if (MSVC)
    add_compile_options(
            "$<$<CONFIG:Release>:/UNDEBUG>"
            "$<$<CONFIG:RelWithDebInfo>:/UNDEBUG>"
            "$<$<CONFIG:MinSizeRel>:/UNDEBUG>")
else ()
    add_compile_options(
            "$<$<CONFIG:Release>:-UNDEBUG>"
            "$<$<CONFIG:RelWithDebInfo>:-UNDEBUG>"
            "$<$<CONFIG:MinSizeRel>:-UNDEBUG>")
endif ()

# FFI fixture path for tests: generated header (tests/cmake/zr_vm_write_ffi_fixture_path.cmake).
# Avoids MSVC cl /D where backslashes in -D strings become invalid macro escapes (\a \b \f …).
set(ZR_VM_TESTS_FFI_FIXTURE_PATH_HEADER "${CMAKE_BINARY_DIR}/tests_generated/zr_vm_ffi_fixture_path.h")

add_subdirectory(third_party)

set(ZR_VM_TESTS_SOURCE_DIR_NORMALIZED "${CMAKE_SOURCE_DIR}/tests")
file(TO_CMAKE_PATH "${ZR_VM_TESTS_SOURCE_DIR_NORMALIZED}" ZR_VM_TESTS_SOURCE_DIR_NORMALIZED)

set(ZR_VM_TESTS_BINARY_DIR_NORMALIZED "${CMAKE_BINARY_DIR}/tests_generated")
file(TO_CMAKE_PATH "${ZR_VM_TESTS_BINARY_DIR_NORMALIZED}" ZR_VM_TESTS_BINARY_DIR_NORMALIZED)
file(MAKE_DIRECTORY "${ZR_VM_TESTS_BINARY_DIR_NORMALIZED}")

set(ZR_VM_TESTS_REPO_ROOT_NORMALIZED "${CMAKE_SOURCE_DIR}")
file(TO_CMAKE_PATH "${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}" ZR_VM_TESTS_REPO_ROOT_NORMALIZED)

set(ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED "${CMAKE_BINARY_DIR}/lib")
file(TO_CMAKE_PATH "${ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED}" ZR_VM_TESTS_BUILD_LIB_DIR_NORMALIZED)

option(ZR_VM_REGISTER_PERFORMANCE_CTEST
        "Register performance_report in CTest (long-running cross-language benchmark suite; default OFF for fast ctest)"
        OFF)

set(ZR_VM_TESTS_COMMON_HARNESS_SOURCES
        ${CMAKE_SOURCE_DIR}/tests/harness/runtime_support.c
        ${CMAKE_SOURCE_DIR}/tests/harness/path_support.c
)

set(ZR_VM_TESTS_UNITY_HARNESS_SOURCES
        ${CMAKE_SOURCE_DIR}/tests/harness/unity_crash_guard.c
)

set(ZR_VM_TESTS_REFERENCE_HARNESS_SOURCES
        ${CMAKE_SOURCE_DIR}/tests/harness/reference_support.c
)

set(ZR_VM_SUITE_RUNNER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_executable_suite.cmake)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/zr_vm_register_executable_suite.cmake")
set(ZR_VM_PROJECTS_RUNNER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_projects_suite.cmake)
set(ZR_VM_PERFORMANCE_RUNNER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_performance_suite.cmake)
find_program(ZR_VM_NODE_EXECUTABLE node)
find_program(ZR_VM_BENCHMARK_PYTHON_EXECUTABLE NAMES python3 python)

# LLVM 14 needs an opt-in for ptr IR; newer Clang releases removed that flag.
# Probe the IR dialect instead of passing a removed backend option everywhere.
find_program(ZR_VM_TESTS_CLANG_EXECUTABLE NAMES clang)
set(ZR_VM_TESTS_LLVM_POINTER_FLAGS "")
if (ZR_VM_TESTS_CLANG_EXECUTABLE)
    set(_zr_pointer_probe "${ZR_VM_TESTS_BINARY_DIR_NORMALIZED}/llvm-pointer-probe")
    file(WRITE "${_zr_pointer_probe}.ll" "define ptr @identity(ptr %p) { ret ptr %p }\n")
    execute_process(COMMAND "${ZR_VM_TESTS_CLANG_EXECUTABLE}"
            -Wno-override-module -c "${_zr_pointer_probe}.ll" -o "${_zr_pointer_probe}.o"
            RESULT_VARIABLE _zr_pointer_result OUTPUT_QUIET ERROR_QUIET)
    if (NOT _zr_pointer_result EQUAL 0)
        set(ZR_VM_TESTS_LLVM_POINTER_FLAGS "-mllvm -opaque-pointers")
    endif ()
endif ()
add_compile_definitions(ZR_VM_TESTS_LLVM_POINTER_FLAGS="${ZR_VM_TESTS_LLVM_POINTER_FLAGS}")

function(zr_vm_apply_common_test_settings target_name)
    target_include_directories(${target_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/tests/harness
            ${CMAKE_SOURCE_DIR}/tests/third_party/zr_unity/Unity/src
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include
    )

    target_compile_definitions(${target_name} PRIVATE
            UNITY_INCLUDE_DOUBLE
            UNITY_DOUBLE_PRECISION=1e-6
            UNITY_USE_FLUSH_STDOUT
            ZR_VM_TESTS_SOURCE_DIR="${ZR_VM_TESTS_SOURCE_DIR_NORMALIZED}"
            ZR_VM_TESTS_BINARY_DIR="${ZR_VM_TESTS_BINARY_DIR_NORMALIZED}"
            ZR_VM_TESTS_C_FLAGS="${CMAKE_C_FLAGS}"
    )
endfunction()

function(zr_vm_add_unity_test_target target_name)
    add_executable(${target_name}
            ${ARGN}
            ${ZR_VM_TESTS_COMMON_HARNESS_SOURCES}
            ${ZR_VM_TESTS_UNITY_HARNESS_SOURCES}
            ${ZR_VM_TESTS_REFERENCE_HARNESS_SOURCES}
    )
    zr_vm_apply_common_test_settings(${target_name})
    target_compile_definitions(${target_name} PRIVATE UNITY_INCLUDE_CONFIG_H)
    zr_link_third_party_for_target(${target_name} "zr_unity")
endfunction()

function(zr_vm_add_support_target target_name)
    add_executable(${target_name}
            ${ARGN}
            ${ZR_VM_TESTS_COMMON_HARNESS_SOURCES}
    )
    zr_vm_apply_common_test_settings(${target_name})
endfunction()

add_executable(
        zr_vm_test_log_failure_probe
        ${CMAKE_SOURCE_DIR}/tests/harness/test_zr_test_log_failure_probe.c
)
zr_vm_apply_common_test_settings(zr_vm_test_log_failure_probe)
target_compile_definitions(zr_vm_test_log_failure_probe PRIVATE UNITY_INCLUDE_CONFIG_H)
zr_link_third_party_for_target(zr_vm_test_log_failure_probe "zr_unity")
add_test(
        NAME test_log_failure_contract
        COMMAND ${CMAKE_COMMAND}
                "-DPROBE_EXE=$<TARGET_FILE:zr_vm_test_log_failure_probe>"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_test_log_failure.cmake
)

add_executable(
        zr_vm_perf_runner
        ${CMAKE_SOURCE_DIR}/tests/performance/perf_runner.c
        ${CMAKE_SOURCE_DIR}/tests/performance/perf_process.c
        ${CMAKE_SOURCE_DIR}/tests/performance/persistent_protocol.c
        ${CMAKE_SOURCE_DIR}/tests/performance/perf_report.c
        ${CMAKE_SOURCE_DIR}/tests/performance/perf_statistics.c
)
target_include_directories(zr_vm_perf_runner PRIVATE
        ${CMAKE_SOURCE_DIR}/tests/performance
        ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
target_compile_definitions(zr_vm_perf_runner PRIVATE _CRT_SECURE_NO_WARNINGS)

add_executable(
        zr_vm_perf_statistics_test
        ${CMAKE_SOURCE_DIR}/tests/performance/test_perf_statistics.c
        ${CMAKE_SOURCE_DIR}/tests/performance/perf_statistics.c
)
target_include_directories(zr_vm_perf_statistics_test PRIVATE ${CMAKE_SOURCE_DIR}/tests/performance)
target_compile_definitions(zr_vm_perf_statistics_test PRIVATE _CRT_SECURE_NO_WARNINGS)
if (NOT WIN32)
    target_link_libraries(zr_vm_perf_statistics_test PRIVATE m)
endif ()

add_executable(
        zr_vm_perf_report_test
        ${CMAKE_SOURCE_DIR}/tests/performance/test_perf_report.c
        ${CMAKE_SOURCE_DIR}/tests/performance/perf_report.c
        ${CMAKE_SOURCE_DIR}/tests/performance/perf_statistics.c
)
target_include_directories(zr_vm_perf_report_test PRIVATE
        ${CMAKE_SOURCE_DIR}/tests/performance
        ${CMAKE_SOURCE_DIR}/zr_vm_common/include)
target_compile_definitions(zr_vm_perf_report_test PRIVATE _CRT_SECURE_NO_WARNINGS)
if (NOT WIN32)
    target_link_libraries(zr_vm_perf_report_test PRIVATE m)
endif ()

add_executable(
        zr_vm_perf_runner_persistent_protocol_test
        ${CMAKE_SOURCE_DIR}/tests/performance/test_perf_runner_persistent_protocol.c
)
target_compile_definitions(zr_vm_perf_runner_persistent_protocol_test PRIVATE _CRT_SECURE_NO_WARNINGS)
add_dependencies(zr_vm_perf_runner_persistent_protocol_test zr_vm_perf_runner)

add_executable(
        zr_vm_native_benchmark_runner
        ${CMAKE_SOURCE_DIR}/tests/benchmarks/native_runner/main.c
        ${CMAKE_SOURCE_DIR}/tests/benchmarks/native_runner/benchmark_support.c
        ${CMAKE_SOURCE_DIR}/tests/benchmarks/cases/numeric_loops/c/benchmark_case.c
        ${CMAKE_SOURCE_DIR}/tests/benchmarks/cases/dispatch_loops/c/benchmark_case.c
        ${CMAKE_SOURCE_DIR}/tests/benchmarks/cases/container_pipeline/c/benchmark_case.c
        ${CMAKE_SOURCE_DIR}/tests/benchmarks/cases/sort_array/c/benchmark_case.c
        ${CMAKE_SOURCE_DIR}/tests/benchmarks/cases/prime_trial_division/c/benchmark_case.c
        ${CMAKE_SOURCE_DIR}/tests/benchmarks/cases/matrix_add_2d/c/benchmark_case.c
        ${CMAKE_SOURCE_DIR}/tests/benchmarks/cases/string_build/c/benchmark_case.c
        ${CMAKE_SOURCE_DIR}/tests/benchmarks/cases/map_object_access/c/benchmark_case.c
        ${CMAKE_SOURCE_DIR}/tests/benchmarks/cases/fib_recursive/c/benchmark_case.c
        ${CMAKE_SOURCE_DIR}/tests/benchmarks/cases/call_chain_polymorphic/c/benchmark_case.c
        ${CMAKE_SOURCE_DIR}/tests/benchmarks/cases/object_field_hot/c/benchmark_case.c
        ${CMAKE_SOURCE_DIR}/tests/benchmarks/cases/array_index_dense/c/benchmark_case.c
        ${CMAKE_SOURCE_DIR}/tests/benchmarks/cases/branch_jump_dense/c/benchmark_case.c
        ${CMAKE_SOURCE_DIR}/tests/benchmarks/cases/mixed_service_loop/c/benchmark_case.c
        ${CMAKE_SOURCE_DIR}/tests/benchmarks/cases/gc_fragment_baseline/c/benchmark_case.c
        ${CMAKE_SOURCE_DIR}/tests/benchmarks/cases/gc_fragment_stress/c/benchmark_case.c
)
target_include_directories(zr_vm_native_benchmark_runner PRIVATE
        ${CMAKE_SOURCE_DIR}/tests/benchmarks/native_runner
)

zr_vm_add_support_target(
        zr_vm_benchmark_registry_test
        ${CMAKE_SOURCE_DIR}/tests/benchmarks/test_benchmark_registry.c
)

add_executable(
        zr_vm_benchmark_support_test
        ${CMAKE_SOURCE_DIR}/tests/benchmarks/test_benchmark_support.c
)
add_dependencies(zr_vm_benchmark_support_test zr_vm_perf_runner)

if (WIN32)
    target_link_libraries(zr_vm_perf_runner PRIVATE Psapi)
elseif (NOT APPLE)
    target_link_libraries(zr_vm_perf_runner PRIVATE m)
endif ()

function(zr_vm_link_core target_name)
    if (BUILD_SHARED_LIB)
        target_link_libraries(${target_name} PRIVATE zr_vm_core_shared)
    else ()
        target_link_libraries(${target_name} PRIVATE zr_vm_core_static)
    endif ()
endfunction()

function(zr_vm_link_parser_core target_name)
    if (BUILD_SHARED_LIB)
        target_link_libraries(${target_name} PRIVATE zr_vm_parser_shared zr_vm_core_shared)
    else ()
        target_link_libraries(${target_name} PRIVATE zr_vm_parser_static zr_vm_core_static)
    endif ()
endfunction()

function(zr_vm_link_parser_core_plus_library target_name)
    if (BUILD_SHARED_LIB)
        target_link_libraries(${target_name} PRIVATE
                zr_vm_parser_shared
                zr_vm_core_shared
                zr_vm_library_shared
                zr_vm_lib_math_shared
                zr_vm_lib_system_shared
                zr_vm_lib_container_shared
                zr_vm_lib_ffi_shared
        )
    else ()
        target_link_libraries(${target_name} PRIVATE
                zr_vm_parser_static
                zr_vm_core_static
                zr_vm_library_static
                zr_vm_lib_math_static
                zr_vm_lib_system_static
                zr_vm_lib_container_static
                zr_vm_lib_ffi_static
        )
    endif ()
endfunction()

option(ZR_VM_TESTS_MACRO_FOUNDATION_ONLY
        "Register only macro foundation and comptime regression tests" OFF)
if (ZR_VM_TESTS_MACRO_FOUNDATION_ONLY)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/macro-foundation-tests.cmake)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/macro-foundation-regressions.cmake)
    return()
endif ()

add_executable(
        zr_vm_zr_benchmark_server
        ${CMAKE_SOURCE_DIR}/tests/benchmarks/zr_runner/benchmark_server.c
)
target_include_directories(zr_vm_zr_benchmark_server PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_common/include
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include
        ${CMAKE_SOURCE_DIR}/zr_vm_library/include
        ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
)
target_compile_definitions(zr_vm_zr_benchmark_server PRIVATE _CRT_SECURE_NO_WARNINGS)
if (BUILD_SHARED_LIB)
    target_link_libraries(zr_vm_zr_benchmark_server PRIVATE
            zr_vm_core_shared
            zr_vm_library_shared
            zr_vm_lib_system_shared
    )
else ()
    target_link_libraries(zr_vm_zr_benchmark_server PRIVATE
            zr_vm_core_static
            zr_vm_library_static
            zr_vm_lib_system_static
    )
endif ()

function(zr_vm_add_execbc_aot_pipeline_test target_name source_path)
    zr_vm_add_unity_test_target(${target_name} ${source_path})
    target_include_directories(${target_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot
    )
    zr_vm_link_parser_core_plus_library(${target_name})
endfunction()

function(zr_vm_link_language_server target_name)
    if (BUILD_SHARED_LIB)
        target_link_libraries(${target_name} PRIVATE
                zr_vm_language_server_shared
                zr_vm_parser_shared
                zr_vm_core_shared
        )
    else ()
        target_link_libraries(${target_name} PRIVATE
                zr_vm_language_server_static
                zr_vm_parser_static
                zr_vm_core_static
        )
    endif ()
endfunction()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/checked-multiply-tests.cmake")

if (TARGET zr_vm_core_shared OR TARGET zr_vm_core_static)
    target_include_directories(zr_vm_benchmark_registry_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_benchmark_registry_test)

    zr_vm_add_unity_test_target(
            zr_vm_gc_test
            ${CMAKE_SOURCE_DIR}/tests/gc/gc_tests.c
            ${CMAKE_SOURCE_DIR}/tests/gc/gc_test_utils.c
    )
    target_include_directories(zr_vm_gc_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    zr_vm_link_core(zr_vm_gc_test)
    if (TARGET zr_vm_library_shared)
        target_link_libraries(zr_vm_gc_test PRIVATE zr_vm_library_shared)
    elseif (TARGET zr_vm_library_static)
        target_link_libraries(zr_vm_gc_test PRIVATE zr_vm_library_static)
    endif ()
    if (TARGET zr_vm_lib_system_shared)
        target_link_libraries(zr_vm_gc_test PRIVATE zr_vm_lib_system_shared)
    elseif (TARGET zr_vm_lib_system_static)
        target_link_libraries(zr_vm_gc_test PRIVATE zr_vm_lib_system_static)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_instructions_test
            ${CMAKE_SOURCE_DIR}/tests/instructions/test_instructions.c
    )
    target_include_directories(zr_vm_instructions_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_instructions_test)

    zr_vm_add_unity_test_target(
            zr_vm_meta_test
            ${CMAKE_SOURCE_DIR}/tests/meta/test_meta.c
    )
    target_include_directories(zr_vm_meta_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_meta_test)

    zr_vm_add_unity_test_target(
            zr_vm_native_closure_value_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_native_closure_value.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_string_layout_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_string_layout.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_string_builder_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_string_builder.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_runtime_crash_recovery_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_runtime_crash_recovery.c
            ${CMAKE_SOURCE_DIR}/tests/unity_lifecycle_noop.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_value_copy_fast_paths_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_value_copy_fast_paths.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_type_layout_inline_copy_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_type_layout_inline_copy.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_type_layout_metadata_contracts_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_type_layout_metadata_contracts.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_frame_slot_layout_lookup_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_frame_slot_layout_lookup.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_inline_struct_array_layout_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_inline_struct_array_layout.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_super_array_raw_int_canonical_storage_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_super_array_raw_int_canonical_storage.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_session_checkpoint_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_session_checkpoint.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_aot_gc_root_frame_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_aot_gc_root_frame.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_value_construction_profile_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_value_construction_profile.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_execution_add_stack_relocation_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_execution_add_stack_relocation.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_execution_numeric_fast_paths_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_execution_numeric_fast_paths.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_execution_unsigned_bitwise_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_execution_unsigned_bitwise.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_stateless_function_closure_cache_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_stateless_function_closure_cache.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_precall_frame_slot_reset_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_precall_frame_slot_reset.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_postcall_fast_paths_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_postcall_fast_paths.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_tail_reuse_callinfo_reset_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_tail_reuse_callinfo_reset.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_vm_closure_precall_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_vm_closure_precall.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_hash_set_dense_paths_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_hash_set_dense_paths.c
    )
    add_test(NAME hash_set_dense_paths
            COMMAND zr_vm_hash_set_dense_paths_test)
    set_tests_properties(hash_set_dense_paths PROPERTIES LABELS "core")
    zr_vm_add_unity_test_target(
            zr_vm_execution_member_access_fast_paths_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_execution_member_access_fast_paths.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_object_shape_transition_cache_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_object_shape_transition_cache.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_execution_dispatch_callable_metadata_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_execution_dispatch_callable_metadata.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_object_call_known_native_fast_path_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_object_call_known_native_fast_path.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_native_inline_span_dispatch_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_native_inline_span_dispatch.c
    )
    target_include_directories(zr_vm_native_closure_value_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_native_closure_value_test)
    target_include_directories(zr_vm_string_layout_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_string_layout_test)
    target_include_directories(zr_vm_string_builder_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_string_builder_test)
    add_test(
            NAME string_builder
            COMMAND $<TARGET_FILE:zr_vm_string_builder_test>
    )
    target_include_directories(zr_vm_runtime_crash_recovery_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_runtime_crash_recovery_test)
    target_include_directories(zr_vm_value_copy_fast_paths_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_value_copy_fast_paths_test)
    target_include_directories(zr_vm_type_layout_inline_copy_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_type_layout_inline_copy_test)
    target_include_directories(zr_vm_type_layout_metadata_contracts_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_type_layout_metadata_contracts_test)
    target_include_directories(zr_vm_frame_slot_layout_lookup_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_frame_slot_layout_lookup_test)
    add_test(
            NAME frame_slot_layout_lookup
            COMMAND $<TARGET_FILE:zr_vm_frame_slot_layout_lookup_test>
    )
    target_include_directories(zr_vm_inline_struct_array_layout_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_inline_struct_array_layout_test)
    add_test(
            NAME inline_struct_array_layout
            COMMAND $<TARGET_FILE:zr_vm_inline_struct_array_layout_test>
    )
    target_include_directories(zr_vm_super_array_raw_int_canonical_storage_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_super_array_raw_int_canonical_storage_test)
    target_include_directories(zr_vm_session_checkpoint_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_session_checkpoint_test)
    add_test(
            NAME session_checkpoint
            COMMAND $<TARGET_FILE:zr_vm_session_checkpoint_test>
    )
    target_include_directories(zr_vm_aot_gc_root_frame_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_aot_gc_root_frame_test)
    target_include_directories(zr_vm_value_construction_profile_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_value_construction_profile_test)
    target_include_directories(zr_vm_execution_add_stack_relocation_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_execution_add_stack_relocation_test)
    target_include_directories(zr_vm_execution_numeric_fast_paths_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_execution_numeric_fast_paths_test)
    target_include_directories(zr_vm_execution_unsigned_bitwise_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_execution_unsigned_bitwise_test)
    target_include_directories(zr_vm_stateless_function_closure_cache_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_stateless_function_closure_cache_test)
    target_include_directories(zr_vm_precall_frame_slot_reset_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_precall_frame_slot_reset_test)
    target_include_directories(zr_vm_postcall_fast_paths_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_postcall_fast_paths_test)
    target_include_directories(zr_vm_tail_reuse_callinfo_reset_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_tail_reuse_callinfo_reset_test)
    target_include_directories(zr_vm_vm_closure_precall_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_vm_closure_precall_test)
    target_include_directories(zr_vm_hash_set_dense_paths_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_hash_set_dense_paths_test)
    target_include_directories(zr_vm_execution_member_access_fast_paths_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core
    )
    zr_vm_link_core(zr_vm_execution_member_access_fast_paths_test)
    target_include_directories(zr_vm_object_shape_transition_cache_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core
    )
    zr_vm_link_core(zr_vm_object_shape_transition_cache_test)
    add_test(
            NAME object_shape_transition_cache
            COMMAND $<TARGET_FILE:zr_vm_object_shape_transition_cache_test>
    )
    target_include_directories(zr_vm_execution_dispatch_callable_metadata_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_execution_dispatch_callable_metadata_test)
    target_include_directories(zr_vm_object_call_known_native_fast_path_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/src/zr_vm_library
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core
    )
    target_include_directories(zr_vm_native_inline_span_dispatch_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/src/zr_vm_library
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core
    )
    zr_vm_link_core(zr_vm_object_call_known_native_fast_path_test)
    zr_vm_link_core(zr_vm_native_inline_span_dispatch_test)
    if (BUILD_SHARED_LIB)
        target_link_libraries(zr_vm_object_call_known_native_fast_path_test PRIVATE zr_vm_library_shared)
        target_link_libraries(zr_vm_native_inline_span_dispatch_test PRIVATE zr_vm_library_shared)
    else ()
        target_link_libraries(zr_vm_object_call_known_native_fast_path_test PRIVATE zr_vm_library_static)
        target_link_libraries(zr_vm_native_inline_span_dispatch_test PRIVATE zr_vm_library_static)
    endif ()
endif ()

if ((TARGET zr_vm_parser_shared OR TARGET zr_vm_parser_static) AND
(TARGET zr_vm_core_shared OR TARGET zr_vm_core_static))
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/parser-foundation-targets.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/aot-pipeline-targets.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/parser-semantic-targets.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/aot-contract-targets.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/aot-generated-targets.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/debug-runtime-targets.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/compiler-project-targets.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/container-task-ffi-targets.cmake")
endif ()

if ((TARGET zr_vm_language_server_shared OR TARGET zr_vm_language_server_static) AND
(TARGET zr_vm_parser_shared OR TARGET zr_vm_parser_static) AND
(TARGET zr_vm_core_shared OR TARGET zr_vm_core_static))
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/lsp-component-targets.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/lsp-query-targets.cmake")
endif ()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/cli-fixture-targets.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/manifest-suite-registration.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/lsp-cli-smoke-registration.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/debug-metadata-aot-registration.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/benchmark-registration.cmake")

# CTest parallel runs (-j N) share HOST_BINARY_DIR / tests_generated / CLI working dirs; without
# coordination, unrelated suites can race on fixed paths and flake. RESOURCE_LOCK (CMake 3.16+)
# serializes these suites so ctest -j remains safe. Wall time is similar to -j1 for this group.
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.16")
    foreach (zr_vm_ctest_exclusive_name IN ITEMS
            core_runtime
            language_pipeline
            containers
            language_server
            language_server_stdio_smoke
            language_server_stdio_cyclic_import_smoke
            language_server_stdio_inline_value_semantic_smoke
            language_server_stdio_diagnostic_fix_smoke
            language_server_stdio_position_encoding_smoke
            language_server_stdio_type_hierarchy_smoke
            projects
            cli_args
            cli_zrp_metadata_dump
            cli_project_incremental
            cli_import_basic_fixture
            escape_pipeline
            cli_debug_e2e
            cli_repl_expression_aggregate_smoke
            cli_repl_expression_assignment_context_smoke
            cli_repl_expression_array_display_smoke
            cli_repl_expression_object_smoke
            cli_repl_e2e
            cli_repl_type_call_member_smoke
            cli_repl_type_call_reference_smoke
            cli_repl_type_conditional_branch_smoke
            cli_repl_type_lambda_body_fact_smoke
            cli_repl_type_lambda_local_fact_smoke
            cli_repl_type_member_receiver_fact_smoke
            cli_repl_type_nested_expression_fact_smoke
            cli_repl_type_nested_numeric_smoke
            cli_repl_type_nested_ownership_smoke
            cli_repl_type_ownership_smoke
            cli_repl_type_logical_comparison_smoke
            cli_repl_type_expression_fact_smoke
            cli_repl_type_reachability_smoke
            debug_metadata
            debug_trace
            debug_traceback
            debug_disassemble
            debug_heap_summary
            debug_library
            debug_snapshot_contracts
            debug_step_edges
            debug_truncation
            debug_threads
            debug_data_breakpoint
            profile_deterministic
            coverage
            system_fs
            debug_agent
            debug_agent_protocol
            debug_variable_child_shape
            cli_integration
            benchmark_registry
            performance_report
            )
        if (TEST ${zr_vm_ctest_exclusive_name})
            set_tests_properties(${zr_vm_ctest_exclusive_name} PROPERTIES
                    RESOURCE_LOCK zr_vm_ctest_shared_host
            )
        endif ()
    endforeach ()
endif ()

add_executable(zr_vm_test_runner ${CMAKE_SOURCE_DIR}/tests/test_runner.c)

# SSA plan tests are registered in a dedicated include after the legacy
# targets/helpers have been declared.  This keeps future SSA slices isolated
# from the large historical test list.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ssa-tests.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ssa-argument-staging-tests.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ssa-analysis-fact-capacity-tests.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ssa-branch-range-null-tests.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ssa-branch-arithmetic-constraints-tests.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ssa-branch-loop-ranges-tests.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ssa-aot-scalar-arithmetic-tests.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ssa-aot-scalar-conditional-tests.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ssa-licm-scalar-legality.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ssa-owned-row-direct-call-graph-tests.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ssa-sccp-conversion-tests.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ssa-sccp-phi-copy-edges-tests.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ssa-compare-predicate-domain.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ssa-compare-metadata-guards.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/gc-nested-mutation-tests.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/tail-dispatch-runtime-tests.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/aot-generated-root-tests.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ssa-cross-domain-clone-oom.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/exception-gc-scopes-tests.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ssa-hotpatch-apply-sources.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/close-proxy-tests.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/task-scheduler-queue-tests.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ssa-invoke-result-tests.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ssa-sccp-copy-availability-tests.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/checked-divide-tests.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/macro-foundation-tests.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/audit-regression-tests.cmake")

# Audited private include roots for consumers of moved implementation files.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/source-layout-private-consumers.cmake")

set(ZR_NATIVE_ARRAY_SOURCE_ROOT "${CMAKE_SOURCE_DIR}")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/native-array-tests.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/core-allocation-lifecycle-tests.cmake")
set(ZR_NATIVE_MATH_SOURCE_ROOT "${CMAKE_SOURCE_DIR}")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/native-math-tests.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/native-registry-oom-tests.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/lsp-array-oom-tests.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/benchmark-scope-types-tests.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/core-exception-oom-tests.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/string-native-layout-tests.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/syntax-embedded-positions-tests.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/core-bootstrap-support-oom-tests.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/string-storage-oom-tests.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/gc-allocation-debt-tests.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/native-plugin-shadow-io-tests.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/gc-root-leases-tests.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/object-write-failure-tests.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/artifact-row-boundary-tests.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/math-input-boundary-tests.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/interpreter-budget-tests.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/native-string-publication-tests.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/setter-caller-cleanup-tests.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/native-numeric-boundary-tests.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/string-format-oom-tests.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/benchmark-viewer-tests.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/thread-transport-string-tests.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/meta-string-oom-tests.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/file-path-operations-tests.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/native-dispatch-self-oom-tests.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/exception-construction-roots-tests.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/system-fs-failure-tests.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/thread-worker-count-tests.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/thread-transport-facade-oom-tests.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/gc-contract-boundary-tests.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/gc-scan-exception-cleanup-tests.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/system-fs-resource-cleanup-tests.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/cli-command-boundary-tests.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/container-pair-numeric-tests.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/value-debug-string-boundary-tests.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/cli-repl-physical-line-tests.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/native-call-binding-status-tests.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/runtime-compiler-boundary-tests.cmake")

if(TARGET zr_vm_cli_executable)
    foreach(case IN ITEMS writeback field_write nested_init)
        foreach(mode IN ITEMS interp binary)
            add_test(NAME struct_array_${case}_${mode}
                    COMMAND ${CMAKE_COMMAND}
                    "-DCLI_EXE=$<TARGET_FILE:zr_vm_cli_executable>"
                    "-DSOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/parser/struct_array_${case}.zr"
                    "-DTEST_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/struct_array_${case}_${mode}"
                    "-DEXECUTION_MODE=${mode}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_struct_array.cmake")
            set_tests_properties(struct_array_${case}_${mode} PROPERTIES TIMEOUT 90)
        endforeach()
    endforeach()
endif()

if(TARGET zr_vm_parser_shared OR TARGET zr_vm_parser_static)
    zr_vm_add_unity_test_target(zr_vm_interpreter_gc_test
        ${CMAKE_SOURCE_DIR}/tests/core/test_interpreter_gc.c)
    target_include_directories(zr_vm_interpreter_gc_test PRIVATE ${CMAKE_SOURCE_DIR}/tests)
    zr_vm_link_parser_core(zr_vm_interpreter_gc_test)
    add_test(NAME interpreter_gc COMMAND zr_vm_interpreter_gc_test)
    set_tests_properties(interpreter_gc PROPERTIES TIMEOUT 90 LABELS "core;gc")
endif()

if(TARGET zr_vm_compiler_w2_performance_quickening_test)
    add_test(NAME interpreter_quickening COMMAND zr_vm_compiler_w2_performance_quickening_test)
    set_tests_properties(interpreter_quickening PROPERTIES TIMEOUT 120 LABELS "core;benchmark")
endif()

if(TARGET zr_vm_cli_executable AND ZR_VM_BENCHMARK_PYTHON_EXECUTABLE)
    add_test(NAME interpreter_gc_cli
        COMMAND ${ZR_VM_BENCHMARK_PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/benchmarks/run_interpreter_probes.py
            --cli $<TARGET_FILE:zr_vm_cli_executable>
            --work-dir ${CMAKE_BINARY_DIR}/tests_generated/interpreter_gc_cli
            --cases objauto,strdrop --repeat 1 --smoke)
    set_tests_properties(interpreter_gc_cli PROPERTIES TIMEOUT 90 LABELS "core;gc;benchmark")
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/function-instruction-verification.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/runtime-value-layout.cmake")
