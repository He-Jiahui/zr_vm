include(${CMAKE_SOURCE_DIR}/zr_vm_common/CommonMacros.cmake)

# FFI fixture path for tests: generated header (tests/cmake/zr_vm_write_ffi_fixture_path.cmake).
# Avoids MSVC cl /D where backslashes in -D strings become invalid macro escapes (\a \b \f …).
set(ZR_VM_TESTS_FFI_FIXTURE_PATH_HEADER "${CMAKE_BINARY_DIR}/tests_generated/zr_vm_ffi_fixture_path.h")

add_subdirectory(third_party)

set(ZR_VM_TESTS_SOURCE_DIR_NORMALIZED "${CMAKE_SOURCE_DIR}/tests")
file(TO_CMAKE_PATH "${ZR_VM_TESTS_SOURCE_DIR_NORMALIZED}" ZR_VM_TESTS_SOURCE_DIR_NORMALIZED)

set(ZR_VM_TESTS_BINARY_DIR_NORMALIZED "${CMAKE_BINARY_DIR}/tests_generated")
file(TO_CMAKE_PATH "${ZR_VM_TESTS_BINARY_DIR_NORMALIZED}" ZR_VM_TESTS_BINARY_DIR_NORMALIZED)
file(MAKE_DIRECTORY "${ZR_VM_TESTS_BINARY_DIR_NORMALIZED}")

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
set(ZR_VM_PROJECTS_RUNNER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_projects_suite.cmake)
set(ZR_VM_PERFORMANCE_RUNNER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_performance_suite.cmake)
find_program(ZR_VM_NODE_EXECUTABLE node)

function(zr_vm_apply_common_test_settings target_name)
    target_include_directories(${target_name} PRIVATE
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
        zr_vm_perf_runner
        ${CMAKE_SOURCE_DIR}/tests/performance/perf_runner.c
)

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
            zr_vm_runtime_crash_recovery_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_runtime_crash_recovery.c
            ${CMAKE_SOURCE_DIR}/tests/unity_lifecycle_noop.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_value_copy_fast_paths_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_value_copy_fast_paths.c
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
    zr_vm_add_unity_test_target(
            zr_vm_execution_member_access_fast_paths_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_execution_member_access_fast_paths.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_execution_dispatch_callable_metadata_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_execution_dispatch_callable_metadata.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_object_call_known_native_fast_path_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_object_call_known_native_fast_path.c
    )
    target_include_directories(zr_vm_native_closure_value_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_native_closure_value_test)
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
    zr_vm_link_core(zr_vm_object_call_known_native_fast_path_test)
    if (BUILD_SHARED_LIB)
        target_link_libraries(zr_vm_object_call_known_native_fast_path_test PRIVATE zr_vm_library_shared)
    else ()
        target_link_libraries(zr_vm_object_call_known_native_fast_path_test PRIVATE zr_vm_library_static)
    endif ()
endif ()

if ((TARGET zr_vm_parser_shared OR TARGET zr_vm_parser_static) AND
(TARGET zr_vm_core_shared OR TARGET zr_vm_core_static))
    zr_vm_add_unity_test_target(
            zr_vm_parser_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_parser.c
            ${CMAKE_SOURCE_DIR}/tests/parser/test_parser_extern.c
    )
    target_include_directories(zr_vm_parser_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_parser_test)

    zr_vm_add_unity_test_target(
            zr_vm_instruction_execution_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_instruction_execution.c
    )
    target_include_directories(zr_vm_instruction_execution_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_instruction_execution_test)

    zr_vm_add_unity_test_target(
            zr_vm_type_inference_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_type_inference.c
    )
    target_include_directories(zr_vm_type_inference_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_type_inference_test)

    zr_vm_add_unity_test_target(
            zr_vm_semir_pipeline_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_semir_pipeline.c
    )
    target_include_directories(zr_vm_semir_pipeline_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_semir_pipeline_test)

    zr_vm_add_unity_test_target(
            zr_vm_debug_metadata_test
            ${CMAKE_SOURCE_DIR}/tests/debug/test_debug_metadata.c
    )
    target_include_directories(zr_vm_debug_metadata_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_debug_metadata_test)

    zr_vm_add_unity_test_target(
            zr_vm_debug_trace_test
            ${CMAKE_SOURCE_DIR}/tests/debug/test_debug_trace.c
    )
    target_include_directories(zr_vm_debug_trace_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_debug_trace_test)

    if ((TARGET zr_vm_debug_shared OR TARGET zr_vm_debug_static) AND
            (TARGET zr_vm_lib_network_shared OR TARGET zr_vm_lib_network_static) AND
            (TARGET zr_vm_lib_system_shared OR TARGET zr_vm_lib_system_static) AND
            (TARGET zr_vm_lib_container_shared OR TARGET zr_vm_lib_container_static))
        zr_vm_add_unity_test_target(
                zr_vm_debug_agent_test
                ${CMAKE_SOURCE_DIR}/tests/debug/test_debug_agent.c
        )
        target_include_directories(zr_vm_debug_agent_test PRIVATE
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
                ${CMAKE_SOURCE_DIR}/zr_vm_core/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_debug/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_network/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/include
        )
        zr_vm_link_parser_core(zr_vm_debug_agent_test)
        if (BUILD_SHARED_LIB)
            target_link_libraries(zr_vm_debug_agent_test PRIVATE
                    zr_vm_debug_shared
                    zr_vm_lib_network_shared
                    zr_vm_lib_system_shared
                    zr_vm_lib_container_shared
            )
        else ()
            target_link_libraries(zr_vm_debug_agent_test PRIVATE
                    zr_vm_debug_static
                    zr_vm_lib_network_static
                    zr_vm_lib_system_static
                    zr_vm_lib_container_static
            )
        endif ()
        zr_link_third_party_for_target(zr_vm_debug_agent_test "zr_c_json")
        target_link_libraries(zr_vm_debug_agent_test PRIVATE Threads::Threads)

        zr_vm_add_unity_test_target(
                zr_vm_debug_agent_protocol_test
                ${CMAKE_SOURCE_DIR}/tests/debug/test_debug_agent_protocol.c
        )
        target_include_directories(zr_vm_debug_agent_protocol_test PRIVATE
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
                ${CMAKE_SOURCE_DIR}/zr_vm_core/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_debug/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_network/include
        )
        zr_vm_link_parser_core(zr_vm_debug_agent_protocol_test)
        if (BUILD_SHARED_LIB)
            target_link_libraries(zr_vm_debug_agent_protocol_test PRIVATE zr_vm_debug_shared zr_vm_lib_network_shared)
        else ()
            target_link_libraries(zr_vm_debug_agent_protocol_test PRIVATE zr_vm_debug_static zr_vm_lib_network_static)
        endif ()
        zr_link_third_party_for_target(zr_vm_debug_agent_protocol_test "zr_c_json")
        target_link_libraries(zr_vm_debug_agent_protocol_test PRIVATE Threads::Threads)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_escape_pipeline_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_escape_metadata_pipeline.c
            ${CMAKE_SOURCE_DIR}/tests/parser/test_escape_metadata_pipeline_main.c
            ${CMAKE_SOURCE_DIR}/tests/unity_lifecycle_noop.c
    )
    target_include_directories(zr_vm_escape_pipeline_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_escape_pipeline_test)

    zr_vm_add_unity_test_target(
            zr_vm_compiler_integration_test
             ${CMAKE_SOURCE_DIR}/tests/parser/test_compiler_features.c
             ${CMAKE_SOURCE_DIR}/tests/parser/test_compiler_regressions.c
             ${CMAKE_SOURCE_DIR}/tests/parser/test_compiler_w2_performance_quickening.c
             ${CMAKE_SOURCE_DIR}/tests/parser/test_escape_metadata_pipeline.c
             ${CMAKE_SOURCE_DIR}/tests/parser/call_chain_polymorphic_compile_fixture.c
             ${CMAKE_SOURCE_DIR}/tests/parser/matrix_add_2d_compile_fixture.c
            ${CMAKE_SOURCE_DIR}/tests/parser/test_compiler_integration_main.c
    )
    target_include_directories(zr_vm_compiler_integration_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_network/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_compiler_integration_test)
    if (TARGET zr_vm_lib_math_shared)
        target_link_libraries(zr_vm_compiler_integration_test PRIVATE zr_vm_lib_math_shared)
    elseif (TARGET zr_vm_lib_math_static)
        target_link_libraries(zr_vm_compiler_integration_test PRIVATE zr_vm_lib_math_static)
    endif ()
    if (TARGET zr_vm_lib_system_shared)
        target_link_libraries(zr_vm_compiler_integration_test PRIVATE zr_vm_lib_system_shared)
    elseif (TARGET zr_vm_lib_system_static)
        target_link_libraries(zr_vm_compiler_integration_test PRIVATE zr_vm_lib_system_static)
    endif ()
    if (TARGET zr_vm_lib_container_shared)
        target_link_libraries(zr_vm_compiler_integration_test PRIVATE zr_vm_lib_container_shared)
    elseif (TARGET zr_vm_lib_container_static)
        target_link_libraries(zr_vm_compiler_integration_test PRIVATE zr_vm_lib_container_static)
    endif ()
    if (TARGET zr_vm_lib_ffi_shared)
        target_link_libraries(zr_vm_compiler_integration_test PRIVATE zr_vm_lib_ffi_shared)
    elseif (TARGET zr_vm_lib_ffi_static)
        target_link_libraries(zr_vm_compiler_integration_test PRIVATE zr_vm_lib_ffi_static)
    endif ()
    if (TARGET zr_vm_lib_network_shared)
        target_link_libraries(zr_vm_compiler_integration_test PRIVATE zr_vm_lib_network_shared)
    elseif (TARGET zr_vm_lib_network_static)
        target_link_libraries(zr_vm_compiler_integration_test PRIVATE zr_vm_lib_network_static)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_compiler_w2_performance_quickening_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_compiler_w2_performance_quickening.c
            ${CMAKE_SOURCE_DIR}/tests/parser/matrix_add_2d_compile_fixture.c
            ${CMAKE_SOURCE_DIR}/tests/parser/test_compiler_w2_performance_quickening_main.c
            ${CMAKE_SOURCE_DIR}/tests/unity_lifecycle_noop.c
    )
    target_include_directories(zr_vm_compiler_w2_performance_quickening_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_network/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_compiler_w2_performance_quickening_test)
    if (TARGET zr_vm_lib_math_shared)
        target_link_libraries(zr_vm_compiler_w2_performance_quickening_test PRIVATE zr_vm_lib_math_shared)
    elseif (TARGET zr_vm_lib_math_static)
        target_link_libraries(zr_vm_compiler_w2_performance_quickening_test PRIVATE zr_vm_lib_math_static)
    endif ()
    if (TARGET zr_vm_lib_system_shared)
        target_link_libraries(zr_vm_compiler_w2_performance_quickening_test PRIVATE zr_vm_lib_system_shared)
    elseif (TARGET zr_vm_lib_system_static)
        target_link_libraries(zr_vm_compiler_w2_performance_quickening_test PRIVATE zr_vm_lib_system_static)
    endif ()
    if (TARGET zr_vm_lib_container_shared)
        target_link_libraries(zr_vm_compiler_w2_performance_quickening_test PRIVATE zr_vm_lib_container_shared)
    elseif (TARGET zr_vm_lib_container_static)
        target_link_libraries(zr_vm_compiler_w2_performance_quickening_test PRIVATE zr_vm_lib_container_static)
    endif ()
    if (TARGET zr_vm_lib_ffi_shared)
        target_link_libraries(zr_vm_compiler_w2_performance_quickening_test PRIVATE zr_vm_lib_ffi_shared)
    elseif (TARGET zr_vm_lib_ffi_static)
        target_link_libraries(zr_vm_compiler_w2_performance_quickening_test PRIVATE zr_vm_lib_ffi_static)
    endif ()
    if (TARGET zr_vm_lib_network_shared)
        target_link_libraries(zr_vm_compiler_w2_performance_quickening_test PRIVATE zr_vm_lib_network_shared)
    elseif (TARGET zr_vm_lib_network_static)
        target_link_libraries(zr_vm_compiler_w2_performance_quickening_test PRIVATE zr_vm_lib_network_static)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_compiler_call_lowering_focus_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_compiler_regressions.c
            ${CMAKE_SOURCE_DIR}/tests/parser/call_chain_polymorphic_compile_fixture.c
            ${CMAKE_SOURCE_DIR}/tests/parser/matrix_add_2d_compile_fixture.c
            ${CMAKE_SOURCE_DIR}/tests/parser/test_compiler_call_lowering_focus_main.c
            ${CMAKE_SOURCE_DIR}/tests/unity_lifecycle_noop.c
    )
    target_include_directories(zr_vm_compiler_call_lowering_focus_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_network/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_compiler_call_lowering_focus_test)
    if (TARGET zr_vm_lib_math_shared)
        target_link_libraries(zr_vm_compiler_call_lowering_focus_test PRIVATE zr_vm_lib_math_shared)
    elseif (TARGET zr_vm_lib_math_static)
        target_link_libraries(zr_vm_compiler_call_lowering_focus_test PRIVATE zr_vm_lib_math_static)
    endif ()
    if (TARGET zr_vm_lib_system_shared)
        target_link_libraries(zr_vm_compiler_call_lowering_focus_test PRIVATE zr_vm_lib_system_shared)
    elseif (TARGET zr_vm_lib_system_static)
        target_link_libraries(zr_vm_compiler_call_lowering_focus_test PRIVATE zr_vm_lib_system_static)
    endif ()
    if (TARGET zr_vm_lib_container_shared)
        target_link_libraries(zr_vm_compiler_call_lowering_focus_test PRIVATE zr_vm_lib_container_shared)
    elseif (TARGET zr_vm_lib_container_static)
        target_link_libraries(zr_vm_compiler_call_lowering_focus_test PRIVATE zr_vm_lib_container_static)
    endif ()
    if (TARGET zr_vm_lib_ffi_shared)
        target_link_libraries(zr_vm_compiler_call_lowering_focus_test PRIVATE zr_vm_lib_ffi_shared)
    elseif (TARGET zr_vm_lib_ffi_static)
        target_link_libraries(zr_vm_compiler_call_lowering_focus_test PRIVATE zr_vm_lib_ffi_static)
    endif ()
    if (TARGET zr_vm_lib_network_shared)
        target_link_libraries(zr_vm_compiler_call_lowering_focus_test PRIVATE zr_vm_lib_network_shared)
    elseif (TARGET zr_vm_lib_network_static)
        target_link_libraries(zr_vm_compiler_call_lowering_focus_test PRIVATE zr_vm_lib_network_static)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_literal_surface_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_prototype.c
            ${CMAKE_SOURCE_DIR}/tests/parser/test_const_keyword.c
            ${CMAKE_SOURCE_DIR}/tests/parser/test_char_and_type_cast.c
            ${CMAKE_SOURCE_DIR}/tests/parser/test_literal_surface_main.c
            ${CMAKE_SOURCE_DIR}/tests/unity_lifecycle_noop.c
    )
    target_include_directories(zr_vm_literal_surface_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_literal_surface_test)

    zr_vm_add_unity_test_target(
            zr_vm_reference_manifest_contracts_test
            ${CMAKE_SOURCE_DIR}/tests/reference/test_reference_manifest_contracts.c
            ${CMAKE_SOURCE_DIR}/tests/unity_lifecycle_noop.c
    )
    target_include_directories(zr_vm_reference_manifest_contracts_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_reference_manifest_contracts_test)

    zr_vm_add_unity_test_target(
            zr_vm_lexer_parser_compiler_execution_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_lexer_parser_compiler_execution.c
    )
    target_include_directories(zr_vm_lexer_parser_compiler_execution_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_lexer_parser_compiler_execution_test)

    zr_vm_add_unity_test_target(
            zr_vm_exceptions_test
            ${CMAKE_SOURCE_DIR}/tests/exceptions/test_exceptions.c
    )
    target_include_directories(zr_vm_exceptions_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_exceptions_test)

    zr_vm_add_unity_test_target(
            zr_vm_system_fs_test
            ${CMAKE_SOURCE_DIR}/tests/system/test_system_fs_module.c
    )
    target_include_directories(zr_vm_system_fs_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
            "${CMAKE_BINARY_DIR}/tests_generated"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_system_fs_test)

    zr_vm_add_unity_test_target(
            zr_vm_module_system_test
            ${CMAKE_SOURCE_DIR}/tests/module/test_module_system.c
    )
    target_include_directories(zr_vm_module_system_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_module_system_test)

    zr_vm_add_unity_test_target(
            zr_vm_project_import_resolver_test
            ${CMAKE_SOURCE_DIR}/tests/library/test_project_import_resolver.c
    )
    target_include_directories(zr_vm_project_import_resolver_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_project_import_resolver_test)

    zr_vm_add_unity_test_target(
            zr_vm_native_binding_direct_call_test
            ${CMAKE_SOURCE_DIR}/tests/library/test_native_binding_direct_call.c
    )
    target_include_directories(zr_vm_native_binding_direct_call_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_native_binding_direct_call_test)

    zr_vm_add_unity_test_target(
            zr_vm_rust_binding_api_test
            ${CMAKE_SOURCE_DIR}/tests/rust_binding/test_rust_binding_api.c
    )
    target_include_directories(zr_vm_rust_binding_api_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_rust_binding/include
    )
    if (BUILD_SHARED_LIB)
        target_link_libraries(zr_vm_rust_binding_api_test PRIVATE
                zr_vm_parser_shared
                zr_vm_core_shared
                zr_vm_library_shared
                zr_vm_rust_binding_shared
        )
    else ()
        target_link_libraries(zr_vm_rust_binding_api_test PRIVATE
                zr_vm_parser_static
                zr_vm_core_static
                zr_vm_library_static
                zr_vm_rust_binding_static
        )
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_project_import_canonicalization_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_project_import_canonicalization.c
    )
    target_include_directories(zr_vm_project_import_canonicalization_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_project_import_canonicalization_test)

    zr_vm_add_unity_test_target(
            zr_vm_named_arguments_test
            ${CMAKE_SOURCE_DIR}/tests/function/test_named_arguments.c
    )
    target_include_directories(zr_vm_named_arguments_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_named_arguments_test)

    zr_vm_add_unity_test_target(
            zr_vm_compile_time_test
            ${CMAKE_SOURCE_DIR}/tests/compileTime/test_compile_time_execution.c
    )
    target_include_directories(zr_vm_compile_time_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_compile_time_test)

    zr_vm_add_unity_test_target(
            zr_vm_decorator_pipeline_test
            ${CMAKE_SOURCE_DIR}/tests/decorators/test_compile_time_member_decorators.c
    )
    target_include_directories(zr_vm_decorator_pipeline_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    zr_vm_link_parser_core(zr_vm_decorator_pipeline_test)

    zr_vm_add_unity_test_target(
            zr_vm_scripts_test
            ${CMAKE_SOURCE_DIR}/tests/scripts/test_comprehensive.c
            ${CMAKE_SOURCE_DIR}/tests/scripts/test_utils.c
    )
    target_include_directories(zr_vm_scripts_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/scripts
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_scripts_test)
    zr_link_third_party_for_target(zr_vm_scripts_test "zr_c_json")

    zr_vm_add_unity_test_target(
            zr_vm_container_runtime_test
            ${CMAKE_SOURCE_DIR}/tests/container/test_container_runtime.c
            ${CMAKE_SOURCE_DIR}/tests/container/container_test_common.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_container_metadata_test
            ${CMAKE_SOURCE_DIR}/tests/container/test_container_metadata.c
            ${CMAKE_SOURCE_DIR}/tests/container/container_test_common.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_container_type_inference_test
            ${CMAKE_SOURCE_DIR}/tests/container/test_container_type_inference.c
            ${CMAKE_SOURCE_DIR}/tests/container/container_test_common.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_container_temp_value_root_test
            ${CMAKE_SOURCE_DIR}/tests/container/test_temp_value_root.c
            ${CMAKE_SOURCE_DIR}/tests/container/container_test_common.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_task_runtime_test
            ${CMAKE_SOURCE_DIR}/tests/task/test_task_runtime.c
            ${CMAKE_SOURCE_DIR}/tests/unity_lifecycle_noop.c
    )
    if (TARGET zr_vm_thread_shared OR TARGET zr_vm_thread_static)
        zr_vm_add_unity_test_target(
                zr_vm_thread_runtime_test
                ${CMAKE_SOURCE_DIR}/tests/thread/test_thread_runtime.c
                ${CMAKE_SOURCE_DIR}/tests/unity_lifecycle_noop.c
        )
    endif ()

    foreach (container_target
            zr_vm_container_runtime_test
            zr_vm_container_metadata_test
            zr_vm_container_type_inference_test
            zr_vm_container_temp_value_root_test)
        target_include_directories(${container_target} PRIVATE
                ${CMAKE_SOURCE_DIR}/tests/container
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
                ${CMAKE_SOURCE_DIR}/zr_vm_core/include
                ${CMAKE_SOURCE_DIR}/zr_vm_library/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
        )
        zr_vm_link_parser_core_plus_library(${container_target})
    endforeach ()

    target_include_directories(zr_vm_task_runtime_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    if (BUILD_SHARED_LIB)
        target_link_libraries(zr_vm_task_runtime_test PRIVATE
                zr_vm_parser_shared
                zr_vm_core_shared
                zr_vm_library_shared
        )
    else ()
        target_link_libraries(zr_vm_task_runtime_test PRIVATE
                zr_vm_parser_static
                zr_vm_core_static
                zr_vm_library_static
        )
    endif ()

    if (TARGET zr_vm_thread_runtime_test)
        target_include_directories(zr_vm_thread_runtime_test PRIVATE
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
                ${CMAKE_SOURCE_DIR}/zr_vm_core/include
                ${CMAKE_SOURCE_DIR}/zr_vm_library/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_thread/include
        )
        if (BUILD_SHARED_LIB)
            target_link_libraries(zr_vm_thread_runtime_test PRIVATE
                    zr_vm_parser_shared
                    zr_vm_core_shared
                    zr_vm_library_shared
                    zr_vm_thread_shared
            )
        else ()
            target_link_libraries(zr_vm_thread_runtime_test PRIVATE
                    zr_vm_parser_static
                    zr_vm_core_static
                    zr_vm_library_static
                    zr_vm_thread_static
            )
        endif ()
    endif ()
endif ()

if ((TARGET zr_vm_language_server_shared OR TARGET zr_vm_language_server_static) AND
(TARGET zr_vm_parser_shared OR TARGET zr_vm_parser_static) AND
(TARGET zr_vm_core_shared OR TARGET zr_vm_core_static))
    zr_vm_add_support_target(
            zr_vm_language_server_symbol_table_test
            ${CMAKE_SOURCE_DIR}/tests/language_server/test_symbol_table.c
    )
    zr_vm_add_support_target(
            zr_vm_language_server_reference_tracker_test
            ${CMAKE_SOURCE_DIR}/tests/language_server/test_reference_tracker.c
    )
    zr_vm_add_support_target(
            zr_vm_language_server_semantic_analyzer_test
            ${CMAKE_SOURCE_DIR}/tests/language_server/test_semantic_analyzer.c
    )
    zr_vm_add_support_target(
            zr_vm_language_server_incremental_parser_test
            ${CMAKE_SOURCE_DIR}/tests/language_server/test_incremental_parser.c
    )
    zr_vm_add_support_target(
            zr_vm_language_server_lsp_interface_test
            ${CMAKE_SOURCE_DIR}/tests/language_server/test_lsp_interface.c
    )
    zr_vm_add_support_target(
            zr_vm_language_server_lsp_project_features_test
            ${CMAKE_SOURCE_DIR}/tests/language_server/test_lsp_project_features.c
    )
    zr_vm_add_support_target(
            zr_vm_language_server_lsp_advanced_editor_features_test
            ${CMAKE_SOURCE_DIR}/tests/language_server/test_lsp_advanced_editor_features.c
    )
    zr_vm_add_support_target(
            zr_vm_language_server_lsp_language_feature_matrix_test
            ${CMAKE_SOURCE_DIR}/tests/language_server/test_lsp_language_feature_matrix.c
    )

    foreach (language_server_target
            zr_vm_language_server_symbol_table_test
            zr_vm_language_server_reference_tracker_test
            zr_vm_language_server_semantic_analyzer_test
            zr_vm_language_server_incremental_parser_test
            zr_vm_language_server_lsp_interface_test
            zr_vm_language_server_lsp_project_features_test
            zr_vm_language_server_lsp_advanced_editor_features_test
            zr_vm_language_server_lsp_language_feature_matrix_test)
        target_include_directories(${language_server_target} PRIVATE
                ${CMAKE_SOURCE_DIR}/zr_vm_language_server/include
                ${CMAKE_SOURCE_DIR}/zr_vm_language_server/src/zr_vm_language_server
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
                ${CMAKE_SOURCE_DIR}/zr_vm_core/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/include
        )
        zr_vm_link_language_server(${language_server_target})
    endforeach ()

    target_include_directories(zr_vm_language_server_semantic_analyzer_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
    )

    target_include_directories(zr_vm_language_server_lsp_interface_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
    )
    target_include_directories(zr_vm_language_server_lsp_project_features_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
    )
    target_include_directories(zr_vm_language_server_lsp_advanced_editor_features_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
    )
    target_include_directories(zr_vm_language_server_lsp_language_feature_matrix_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
    )

    target_compile_definitions(zr_vm_language_server_lsp_interface_test PRIVATE
            ZR_VM_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
    )
    target_compile_definitions(zr_vm_language_server_lsp_project_features_test PRIVATE
            ZR_VM_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
            ZR_VM_DESCRIPTOR_PLUGIN_FIXTURE_INT_PATH="$<TARGET_FILE:zr_vm_descriptor_plugin_fixture_int>"
            ZR_VM_DESCRIPTOR_PLUGIN_FIXTURE_FLOAT_PATH="$<TARGET_FILE:zr_vm_descriptor_plugin_fixture_float>"
    )
    target_compile_definitions(zr_vm_language_server_lsp_language_feature_matrix_test PRIVATE
            ZR_VM_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
    )

    if (BUILD_SHARED_LIB)
        target_link_libraries(zr_vm_language_server_semantic_analyzer_test PRIVATE
                zr_vm_library_shared
                zr_vm_lib_math_shared
                zr_vm_lib_container_shared
                zr_vm_lib_system_shared
        )
        target_link_libraries(zr_vm_language_server_lsp_interface_test PRIVATE
                zr_vm_library_shared
                zr_vm_lib_math_shared
                zr_vm_lib_system_shared
                zr_vm_lib_ffi_shared
        )
        target_link_libraries(zr_vm_language_server_lsp_project_features_test PRIVATE
                zr_vm_library_shared
                zr_vm_lib_math_shared
                zr_vm_lib_container_shared
                zr_vm_lib_system_shared
                zr_vm_lib_ffi_shared
        )
        target_link_libraries(zr_vm_language_server_lsp_advanced_editor_features_test PRIVATE
                zr_vm_library_shared
                zr_vm_lib_math_shared
                zr_vm_lib_container_shared
                zr_vm_lib_system_shared
                zr_vm_lib_ffi_shared
        )
        target_link_libraries(zr_vm_language_server_lsp_language_feature_matrix_test PRIVATE
                zr_vm_library_shared
                zr_vm_lib_math_shared
                zr_vm_lib_container_shared
                zr_vm_lib_system_shared
                zr_vm_lib_ffi_shared
        )
    else ()
        target_link_libraries(zr_vm_language_server_semantic_analyzer_test PRIVATE
                zr_vm_library_static
                zr_vm_lib_math_static
                zr_vm_lib_container_static
                zr_vm_lib_system_static
        )
        target_link_libraries(zr_vm_language_server_lsp_interface_test PRIVATE
                zr_vm_library_static
                zr_vm_lib_math_static
                zr_vm_lib_system_static
                zr_vm_lib_ffi_static
        )
        target_link_libraries(zr_vm_language_server_lsp_project_features_test PRIVATE
                zr_vm_library_static
                zr_vm_lib_math_static
                zr_vm_lib_container_static
                zr_vm_lib_system_static
                zr_vm_lib_ffi_static
        )
        target_link_libraries(zr_vm_language_server_lsp_advanced_editor_features_test PRIVATE
                zr_vm_library_static
                zr_vm_lib_math_static
                zr_vm_lib_container_static
                zr_vm_lib_system_static
                zr_vm_lib_ffi_static
        )
        target_link_libraries(zr_vm_language_server_lsp_language_feature_matrix_test PRIVATE
                zr_vm_library_static
                zr_vm_lib_math_static
                zr_vm_lib_container_static
                zr_vm_lib_system_static
                zr_vm_lib_ffi_static
        )
    endif ()

endif ()

if (TARGET zr_vm_cli_executable AND
(TARGET zr_vm_parser_shared OR TARGET zr_vm_parser_static) AND
(TARGET zr_vm_core_shared OR TARGET zr_vm_core_static))
    add_executable(
            zr_vm_cli_args_test
            ${CMAKE_SOURCE_DIR}/tests/cli/test_cli_args.c
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/command/command.c
    )
    target_include_directories(zr_vm_cli_args_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/include
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli
            ${CMAKE_SOURCE_DIR}/zr_vm_common/include
    )
    zr_vm_link_core(zr_vm_cli_args_test)
endif ()

if (TARGET zr_vm_cli_executable AND
        (TARGET zr_vm_parser_shared OR TARGET zr_vm_parser_static) AND
        (TARGET zr_vm_core_shared OR TARGET zr_vm_core_static) AND
        (TARGET zr_vm_library_shared OR TARGET zr_vm_library_static))
    zr_vm_add_unity_test_target(
            zr_vm_cli_project_incremental_test
            ${CMAKE_SOURCE_DIR}/tests/cli/test_cli_project_incremental.c
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/compiler/compiler.c
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/project/project.c
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/runtime/runtime.c
    )
    target_include_directories(zr_vm_cli_project_incremental_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/include
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_network/include
    )
    if (BUILD_SHARED_LIB)
        target_link_libraries(zr_vm_cli_project_incremental_test PRIVATE
                zr_vm_parser_shared
                zr_vm_core_shared
                zr_vm_library_shared
                zr_vm_lib_math_shared
                zr_vm_lib_system_shared
                zr_vm_lib_container_shared
                zr_vm_lib_ffi_shared
                zr_vm_lib_network_shared
        )
    else ()
        target_link_libraries(zr_vm_cli_project_incremental_test PRIVATE
                zr_vm_parser_static
                zr_vm_core_static
                zr_vm_library_static
                zr_vm_lib_math_static
                zr_vm_lib_system_static
                zr_vm_lib_container_static
                zr_vm_lib_ffi_static
                zr_vm_lib_network_static
        )
    endif ()
    if (TARGET zr_vm_lib_thread_shared)
        target_link_libraries(zr_vm_cli_project_incremental_test PRIVATE zr_vm_lib_thread_shared)
        target_compile_definitions(zr_vm_cli_project_incremental_test PRIVATE ZR_VM_HAS_THREAD_MODULE=1)
    elseif (TARGET zr_vm_lib_thread_static)
        target_link_libraries(zr_vm_cli_project_incremental_test PRIVATE zr_vm_lib_thread_static)
        target_compile_definitions(zr_vm_cli_project_incremental_test PRIVATE ZR_VM_HAS_THREAD_MODULE=1)
    endif ()
endif ()

if (TARGET zr_vm_cli_executable)
    add_executable(
            zr_vm_cli_repl_e2e_test
            ${CMAKE_SOURCE_DIR}/tests/cli/test_cli_repl_e2e.c
            ${CMAKE_SOURCE_DIR}/tests/harness/path_support.c
    )
    zr_vm_apply_common_test_settings(zr_vm_cli_repl_e2e_test)
    target_include_directories(zr_vm_cli_repl_e2e_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/harness
    )
endif ()

if (TARGET zr_vm_cli_executable AND
        ((TARGET zr_vm_lib_network_shared AND TARGET zr_vm_debug_shared) OR
         (TARGET zr_vm_lib_network_static AND TARGET zr_vm_debug_static)))
    add_executable(
            zr_vm_cli_debug_e2e_test
            ${CMAKE_SOURCE_DIR}/tests/cli/test_cli_debug_e2e.c
            ${CMAKE_SOURCE_DIR}/tests/harness/path_support.c
    )
    zr_vm_apply_common_test_settings(zr_vm_cli_debug_e2e_test)
    target_include_directories(zr_vm_cli_debug_e2e_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/harness
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_network/include
    )
    if (BUILD_SHARED_LIB)
        target_link_libraries(zr_vm_cli_debug_e2e_test PRIVATE zr_vm_lib_network_shared)
    else ()
        target_link_libraries(zr_vm_cli_debug_e2e_test PRIVATE zr_vm_lib_network_static)
    endif ()
    zr_link_third_party_for_target(zr_vm_cli_debug_e2e_test "zr_c_json")
endif ()

find_package(Threads REQUIRED)

add_library(
        zr_vm_descriptor_plugin_fixture_int
        SHARED
        ${CMAKE_SOURCE_DIR}/tests/language_server/descriptor_plugin_fixture_int.c
)
target_include_directories(zr_vm_descriptor_plugin_fixture_int PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_library/include
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include
        ${CMAKE_SOURCE_DIR}/zr_vm_common/include
)
set_target_properties(zr_vm_descriptor_plugin_fixture_int PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
)

add_library(
        zr_vm_descriptor_plugin_fixture_float
        SHARED
        ${CMAKE_SOURCE_DIR}/tests/language_server/descriptor_plugin_fixture_float.c
)
target_include_directories(zr_vm_descriptor_plugin_fixture_float PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_library/include
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include
        ${CMAKE_SOURCE_DIR}/zr_vm_common/include
)
set_target_properties(zr_vm_descriptor_plugin_fixture_float PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
)

if (TARGET zr_vm_language_server_lsp_project_features_test)
    add_dependencies(zr_vm_language_server_lsp_project_features_test
            zr_vm_descriptor_plugin_fixture_int
            zr_vm_descriptor_plugin_fixture_float)
endif ()

add_library(
        zr_vm_ffi_fixture
        SHARED
        ${CMAKE_SOURCE_DIR}/tests/ffi/ffi_fixture.c
)
target_include_directories(zr_vm_ffi_fixture PRIVATE
        ${CMAKE_SOURCE_DIR}/tests/ffi
)
target_link_libraries(zr_vm_ffi_fixture PRIVATE Threads::Threads)
set_target_properties(zr_vm_ffi_fixture PROPERTIES
        OUTPUT_NAME zr_vm_ffi_fixture
)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/tests_generated")
add_custom_command(
        OUTPUT "${ZR_VM_TESTS_FFI_FIXTURE_PATH_HEADER}"
        COMMAND ${CMAKE_COMMAND}
                "-DIN_DLL=$<TARGET_FILE:zr_vm_ffi_fixture>"
                "-DOUT_FILE=${ZR_VM_TESTS_FFI_FIXTURE_PATH_HEADER}"
                "-P"
                "${CMAKE_CURRENT_SOURCE_DIR}/cmake/zr_vm_write_ffi_fixture_path.cmake"
        DEPENDS zr_vm_ffi_fixture
        COMMENT "Generate zr_vm_ffi_fixture_path.h")
add_custom_target(zr_vm_ffi_fixture_path_header DEPENDS "${ZR_VM_TESTS_FFI_FIXTURE_PATH_HEADER}")

if (TARGET zr_vm_system_fs_test)
    add_dependencies(zr_vm_system_fs_test zr_vm_ffi_fixture_path_header zr_vm_ffi_fixture)
endif ()
if (TARGET zr_vm_ffi_test)
    add_dependencies(zr_vm_ffi_test zr_vm_ffi_fixture_path_header zr_vm_ffi_fixture)
endif ()
if (TARGET zr_vm_ffi_probe)
    add_dependencies(zr_vm_ffi_probe zr_vm_ffi_fixture_path_header zr_vm_ffi_fixture)
endif ()

if ((TARGET zr_vm_parser_shared OR TARGET zr_vm_parser_static) AND
(TARGET zr_vm_core_shared OR TARGET zr_vm_core_static))
    zr_vm_add_unity_test_target(
            zr_vm_ffi_test
            ${CMAKE_SOURCE_DIR}/tests/ffi/test_ffi_module.c
    )
    target_include_directories(zr_vm_ffi_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/ffi
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
            "${CMAKE_BINARY_DIR}/tests_generated"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_ffi_test)
    add_dependencies(zr_vm_ffi_test zr_vm_ffi_fixture_path_header zr_vm_ffi_fixture)

    zr_vm_add_support_target(
            zr_vm_ffi_probe
            ${CMAKE_SOURCE_DIR}/tests/ffi/debug_ffi_close_probe.c
    )
    target_include_directories(zr_vm_ffi_probe PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/ffi
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
            "${CMAKE_BINARY_DIR}/tests_generated"
    )
    zr_vm_link_parser_core_plus_library(zr_vm_ffi_probe)
    add_dependencies(zr_vm_ffi_probe zr_vm_ffi_fixture_path_header zr_vm_ffi_fixture)
endif ()

set(core_runtime_executables "")
if (TARGET zr_vm_gc_test AND TARGET zr_vm_instructions_test AND TARGET zr_vm_meta_test AND TARGET zr_vm_native_closure_value_test AND TARGET zr_vm_runtime_crash_recovery_test AND TARGET zr_vm_value_copy_fast_paths_test AND TARGET zr_vm_execution_member_access_fast_paths_test AND TARGET zr_vm_precall_frame_slot_reset_test AND TARGET zr_vm_tail_reuse_callinfo_reset_test AND TARGET zr_vm_vm_closure_precall_test)
    set(core_runtime_executables
            "$<TARGET_FILE:zr_vm_gc_test>;$<TARGET_FILE:zr_vm_instructions_test>;$<TARGET_FILE:zr_vm_meta_test>;$<TARGET_FILE:zr_vm_native_closure_value_test>;$<TARGET_FILE:zr_vm_runtime_crash_recovery_test>;$<TARGET_FILE:zr_vm_value_copy_fast_paths_test>;$<TARGET_FILE:zr_vm_execution_member_access_fast_paths_test>;$<TARGET_FILE:zr_vm_precall_frame_slot_reset_test>;$<TARGET_FILE:zr_vm_postcall_fast_paths_test>;$<TARGET_FILE:zr_vm_tail_reuse_callinfo_reset_test>;$<TARGET_FILE:zr_vm_vm_closure_precall_test>"
    )
    set(core_runtime_smoke_executables
            "$<TARGET_FILE:zr_vm_instructions_test>;$<TARGET_FILE:zr_vm_meta_test>;$<TARGET_FILE:zr_vm_runtime_crash_recovery_test>;$<TARGET_FILE:zr_vm_value_copy_fast_paths_test>;$<TARGET_FILE:zr_vm_execution_member_access_fast_paths_test>;$<TARGET_FILE:zr_vm_precall_frame_slot_reset_test>;$<TARGET_FILE:zr_vm_postcall_fast_paths_test>;$<TARGET_FILE:zr_vm_tail_reuse_callinfo_reset_test>;$<TARGET_FILE:zr_vm_vm_closure_precall_test>"
    )
    set(core_runtime_core_executables "${core_runtime_executables}")
    set(core_runtime_stress_executables "${core_runtime_executables}")
    add_test(
            NAME core_runtime
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=core_runtime"
            "-DEXECUTABLES=${core_runtime_executables}"
            "-DEXECUTABLES_SMOKE=${core_runtime_smoke_executables}"
            "-DEXECUTABLES_CORE=${core_runtime_core_executables}"
            "-DEXECUTABLES_STRESS=${core_runtime_stress_executables}"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            "-DRUN_WORKING_DIRECTORY=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

set(language_pipeline_executables "")
if (TARGET zr_vm_parser_test AND
        TARGET zr_vm_instruction_execution_test AND
        TARGET zr_vm_type_inference_test AND
        TARGET zr_vm_compiler_integration_test AND
        TARGET zr_vm_semir_pipeline_test AND
        TARGET zr_vm_literal_surface_test AND
        TARGET zr_vm_reference_manifest_contracts_test AND
        TARGET zr_vm_lexer_parser_compiler_execution_test AND
        TARGET zr_vm_exceptions_test AND
        TARGET zr_vm_module_system_test AND
        TARGET zr_vm_named_arguments_test AND
        TARGET zr_vm_compile_time_test AND
        TARGET zr_vm_decorator_pipeline_test AND
        TARGET zr_vm_scripts_test)
    set(language_pipeline_executables
            "$<TARGET_FILE:zr_vm_parser_test>;$<TARGET_FILE:zr_vm_instruction_execution_test>;$<TARGET_FILE:zr_vm_type_inference_test>;$<TARGET_FILE:zr_vm_compiler_integration_test>;$<TARGET_FILE:zr_vm_semir_pipeline_test>;$<TARGET_FILE:zr_vm_literal_surface_test>;$<TARGET_FILE:zr_vm_reference_manifest_contracts_test>;$<TARGET_FILE:zr_vm_lexer_parser_compiler_execution_test>;$<TARGET_FILE:zr_vm_exceptions_test>;$<TARGET_FILE:zr_vm_module_system_test>;$<TARGET_FILE:zr_vm_named_arguments_test>;$<TARGET_FILE:zr_vm_compile_time_test>;$<TARGET_FILE:zr_vm_decorator_pipeline_test>;$<TARGET_FILE:zr_vm_scripts_test>"
    )
    set(language_pipeline_smoke_executables
            "$<TARGET_FILE:zr_vm_instruction_execution_test>;$<TARGET_FILE:zr_vm_compiler_integration_test>;$<TARGET_FILE:zr_vm_semir_pipeline_test>;$<TARGET_FILE:zr_vm_literal_surface_test>;$<TARGET_FILE:zr_vm_reference_manifest_contracts_test>;$<TARGET_FILE:zr_vm_module_system_test>;$<TARGET_FILE:zr_vm_decorator_pipeline_test>"
    )
    set(language_pipeline_core_executables "${language_pipeline_executables}")
    set(language_pipeline_stress_executables "${language_pipeline_executables}")
    add_test(
            NAME language_pipeline
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=language_pipeline"
            "-DEXECUTABLES=${language_pipeline_executables}"
            "-DEXECUTABLES_SMOKE=${language_pipeline_smoke_executables}"
            "-DEXECUTABLES_CORE=${language_pipeline_core_executables}"
            "-DEXECUTABLES_STRESS=${language_pipeline_stress_executables}"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            "-DRUN_WORKING_DIRECTORY=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

set(container_executables "")
if (TARGET zr_vm_container_runtime_test AND
        TARGET zr_vm_container_metadata_test AND
        TARGET zr_vm_container_type_inference_test AND
        TARGET zr_vm_container_temp_value_root_test)
    set(container_executables
            "$<TARGET_FILE:zr_vm_container_runtime_test>;$<TARGET_FILE:zr_vm_container_metadata_test>;$<TARGET_FILE:zr_vm_container_type_inference_test>;$<TARGET_FILE:zr_vm_container_temp_value_root_test>"
    )
    set(container_smoke_executables
            "$<TARGET_FILE:zr_vm_container_runtime_test>;$<TARGET_FILE:zr_vm_container_type_inference_test>;$<TARGET_FILE:zr_vm_container_temp_value_root_test>"
    )
    add_test(
            NAME containers
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=containers"
            "-DEXECUTABLES=${container_executables}"
            "-DEXECUTABLES_SMOKE=${container_smoke_executables}"
            "-DEXECUTABLES_CORE=${container_executables}"
            "-DEXECUTABLES_STRESS=${container_executables}"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            "-DRUN_WORKING_DIRECTORY=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

set(language_server_executables "")
if (TARGET zr_vm_language_server_symbol_table_test AND
        TARGET zr_vm_language_server_reference_tracker_test AND
        TARGET zr_vm_language_server_semantic_analyzer_test AND
        TARGET zr_vm_language_server_incremental_parser_test AND
        TARGET zr_vm_language_server_lsp_interface_test AND
        TARGET zr_vm_language_server_lsp_project_features_test AND
        TARGET zr_vm_language_server_lsp_advanced_editor_features_test)
    set(language_server_executables
            "$<TARGET_FILE:zr_vm_language_server_symbol_table_test>;$<TARGET_FILE:zr_vm_language_server_reference_tracker_test>;$<TARGET_FILE:zr_vm_language_server_semantic_analyzer_test>;$<TARGET_FILE:zr_vm_language_server_incremental_parser_test>;$<TARGET_FILE:zr_vm_language_server_lsp_interface_test>;$<TARGET_FILE:zr_vm_language_server_lsp_project_features_test>;$<TARGET_FILE:zr_vm_language_server_lsp_advanced_editor_features_test>"
    )
    add_test(
            NAME language_server
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=language_server"
            "-DEXECUTABLES=${language_server_executables}"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            "-DRUN_WORKING_DIRECTORY=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_language_server_stdio AND ZR_VM_NODE_EXECUTABLE)
    if (TARGET zr_vm_cli_executable)
        add_test(
                NAME language_server_stdio_smoke
                COMMAND ${ZR_VM_NODE_EXECUTABLE}
                        ${CMAKE_SOURCE_DIR}/tests/language_server/stdio_smoke.js
                        $<TARGET_FILE:zr_vm_language_server_stdio>
                        $<TARGET_FILE:zr_vm_cli_executable>
        )
    else ()
        add_test(
                NAME language_server_stdio_smoke
                COMMAND ${ZR_VM_NODE_EXECUTABLE}
                        ${CMAKE_SOURCE_DIR}/tests/language_server/stdio_smoke.js
                        $<TARGET_FILE:zr_vm_language_server_stdio>
        )
    endif ()
    add_test(
            NAME language_server_stdio_type_hierarchy_smoke
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/language_server/stdio_type_hierarchy_smoke.js
                    $<TARGET_FILE:zr_vm_language_server_stdio>
    )
endif ()

if (TARGET zr_vm_cli_executable)
    add_test(
            NAME projects
            COMMAND ${CMAKE_COMMAND}
            "-DCLI_EXE=$<TARGET_FILE:zr_vm_cli_executable>"
            "-DREPO_ROOT=${CMAKE_SOURCE_DIR}"
            "-DPROJECT_FIXTURES_DIR=${ZR_VM_TESTS_SOURCE_DIR_NORMALIZED}/fixtures/projects"
            "-DGENERATED_DIR=${ZR_VM_TESTS_BINARY_DIR_NORMALIZED}"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_PROJECTS_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_cli_args_test)
    add_test(
            NAME cli_args
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=cli_args"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_cli_args_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_cli_args_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_cli_args_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_cli_args_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            "-DRUN_WORKING_DIRECTORY=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_cli_project_incremental_test)
    add_test(
            NAME cli_project_incremental
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=cli_project_incremental"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_cli_project_incremental_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_cli_project_incremental_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_cli_project_incremental_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_cli_project_incremental_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            "-DRUN_WORKING_DIRECTORY=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_escape_pipeline_test)
    add_test(
            NAME escape_pipeline
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=escape_pipeline"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_escape_pipeline_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_escape_pipeline_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_escape_pipeline_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_escape_pipeline_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            "-DRUN_WORKING_DIRECTORY=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_cli_debug_e2e_test)
    add_test(
            NAME cli_debug_e2e
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=cli_debug_e2e"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_cli_debug_e2e_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_cli_debug_e2e_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_cli_debug_e2e_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_cli_debug_e2e_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            "-DRUN_WORKING_DIRECTORY=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_cli_repl_e2e_test)
    add_test(
            NAME cli_repl_e2e
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=cli_repl_e2e"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_cli_repl_e2e_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_cli_repl_e2e_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_cli_repl_e2e_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_cli_repl_e2e_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            "-DRUN_WORKING_DIRECTORY=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_debug_metadata_test)
    add_test(
            NAME debug_metadata
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=debug_metadata"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_debug_metadata_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_debug_metadata_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_debug_metadata_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_debug_metadata_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_debug_trace_test)
    add_test(
            NAME debug_trace
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=debug_trace"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_debug_trace_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_debug_trace_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_debug_trace_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_debug_trace_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_system_fs_test)
    add_test(
            NAME system_fs
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=system_fs"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_system_fs_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_system_fs_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_system_fs_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_system_fs_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            "-DRUN_WORKING_DIRECTORY=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_debug_agent_test)
    add_test(
            NAME debug_agent
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=debug_agent"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_debug_agent_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_debug_agent_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_debug_agent_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_debug_agent_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_debug_agent_protocol_test)
    add_test(
            NAME debug_agent_protocol
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=debug_agent_protocol"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_debug_agent_protocol_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_debug_agent_protocol_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_debug_agent_protocol_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_debug_agent_protocol_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_cli_executable)
    add_test(
            NAME cli_integration
            COMMAND ${CMAKE_COMMAND}
            "-DCLI_EXE=$<TARGET_FILE:zr_vm_cli_executable>"
            "-DPROJECT_FIXTURES_DIR=${ZR_VM_TESTS_SOURCE_DIR_NORMALIZED}/fixtures/projects"
            "-DGENERATED_DIR=${ZR_VM_TESTS_BINARY_DIR_NORMALIZED}"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_cli_suite.cmake
    )
endif ()

if (TARGET zr_vm_cli_executable AND TARGET zr_vm_perf_runner AND TARGET zr_vm_native_benchmark_runner)
    if (ZR_VM_REGISTER_PERFORMANCE_CTEST)
        add_test(
                NAME performance_report
                COMMAND ${CMAKE_COMMAND}
                "-DCLI_EXE=$<TARGET_FILE:zr_vm_cli_executable>"
                "-DPERF_RUNNER_EXE=$<TARGET_FILE:zr_vm_perf_runner>"
                "-DNATIVE_BENCHMARK_EXE=$<TARGET_FILE:zr_vm_native_benchmark_runner>"
                "-DBENCHMARKS_DIR=${ZR_VM_TESTS_SOURCE_DIR_NORMALIZED}/benchmarks"
                "-DGENERATED_DIR=${ZR_VM_TESTS_BINARY_DIR_NORMALIZED}"
                "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
                -P ${ZR_VM_PERFORMANCE_RUNNER_SCRIPT}
        )
        set_tests_properties(performance_report PROPERTIES
                LABELS "benchmark;long_running"
                RUN_SERIAL TRUE
                TIMEOUT 1800
        )
    endif ()

    add_custom_target(run_performance_suite
            COMMAND ${CMAKE_COMMAND}
                    "-DCLI_EXE=$<TARGET_FILE:zr_vm_cli_executable>"
                    "-DPERF_RUNNER_EXE=$<TARGET_FILE:zr_vm_perf_runner>"
                    "-DNATIVE_BENCHMARK_EXE=$<TARGET_FILE:zr_vm_native_benchmark_runner>"
                    "-DBENCHMARKS_DIR=${ZR_VM_TESTS_SOURCE_DIR_NORMALIZED}/benchmarks"
                    "-DGENERATED_DIR=${ZR_VM_TESTS_BINARY_DIR_NORMALIZED}"
                    "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
                    -P ${ZR_VM_PERFORMANCE_RUNNER_SCRIPT}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Run cross-language performance suite (same script as CTest performance_report)"
    )
    add_dependencies(run_performance_suite
            zr_vm_cli_executable
            zr_vm_perf_runner
            zr_vm_native_benchmark_runner
    )
endif ()

if (TARGET zr_vm_benchmark_registry_test)
    add_test(
            NAME benchmark_registry
            COMMAND ${CMAKE_COMMAND}
                    "-DEXE=$<TARGET_FILE:zr_vm_benchmark_registry_test>"
                    "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_benchmark_registry_test.cmake
    )
endif ()

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
            language_server_stdio_type_hierarchy_smoke
            projects
            cli_args
            cli_project_incremental
            escape_pipeline
            cli_debug_e2e
            cli_repl_e2e
            debug_metadata
            debug_trace
            system_fs
            debug_agent
            debug_agent_protocol
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
