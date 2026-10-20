    zr_vm_add_unity_test_target(
            zr_vm_attribute_contract_test
            ${CMAKE_SOURCE_DIR}/tests/compileTime/test_attribute_contract.c
            ${CMAKE_SOURCE_DIR}/tests/unity_lifecycle_noop.c
    )
    target_include_directories(zr_vm_attribute_contract_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_attribute_contract_test)

    zr_vm_add_unity_test_target(
            zr_vm_test_role_binding_test
            ${CMAKE_SOURCE_DIR}/tests/testing/test_test_role_binding.c
            ${CMAKE_SOURCE_DIR}/tests/unity_lifecycle_noop.c
    )
    target_include_directories(zr_vm_test_role_binding_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_test_role_binding_test)

    zr_vm_add_unity_test_target(
            zr_vm_test_manifest_roundtrip_test
            ${CMAKE_SOURCE_DIR}/tests/artifact/test_manifest_roundtrip.c
            ${CMAKE_SOURCE_DIR}/tests/unity_lifecycle_noop.c
    )
    target_include_directories(zr_vm_test_manifest_roundtrip_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_test_manifest_roundtrip_test)

    zr_vm_add_unity_test_target(
            zr_vm_test_runner_test
            ${CMAKE_SOURCE_DIR}/tests/testing/test_runner.c
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli/testing/test_runner.c
            ${CMAKE_SOURCE_DIR}/tests/unity_lifecycle_noop.c
    )
    target_include_directories(zr_vm_test_runner_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_cli/src/zr_vm_cli
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_test_runner_test)
    target_link_libraries(zr_vm_test_runner_test PRIVATE Threads::Threads)

    zr_vm_add_unity_test_target(
            zr_vm_testing_assertions_test
            ${CMAKE_SOURCE_DIR}/tests/testing/test_assertions.c
            ${CMAKE_SOURCE_DIR}/tests/unity_lifecycle_noop.c
    )
    target_include_directories(zr_vm_testing_assertions_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_testing/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_testing_assertions_test)
    if (BUILD_SHARED_LIB)
        target_link_libraries(zr_vm_testing_assertions_test PRIVATE zr_vm_lib_testing_shared)
    else ()
        target_link_libraries(zr_vm_testing_assertions_test PRIVATE zr_vm_lib_testing_static)
    endif ()

    zr_vm_add_unity_test_target(
            zr_vm_declaration_transform_contract_test
            ${CMAKE_SOURCE_DIR}/tests/compileTime/test_declaration_transform_contract.c
            ${CMAKE_SOURCE_DIR}/tests/unity_lifecycle_noop.c
    )
    target_include_directories(zr_vm_declaration_transform_contract_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_declaration_transform_contract_test)

    zr_vm_add_unity_test_target(
            zr_vm_comptime_runtime_contract_test
            ${CMAKE_SOURCE_DIR}/tests/compileTime/test_comptime_runtime_contract.c
    )
    target_include_directories(zr_vm_comptime_runtime_contract_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/compiler
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_comptime_runtime_contract_test)

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
            zr_vm_enumerator_protocol_test
            ${CMAKE_SOURCE_DIR}/tests/iterator/test_enumerator_protocol.c
            ${CMAKE_SOURCE_DIR}/tests/container/container_test_common.c
    )
    target_include_directories(zr_vm_enumerator_protocol_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/container
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_iteration/include
    )
    zr_vm_link_parser_core_plus_library(zr_vm_enumerator_protocol_test)
    if (BUILD_SHARED_LIB)
        target_link_libraries(zr_vm_enumerator_protocol_test PRIVATE zr_vm_lib_iteration_shared)
    else ()
        target_link_libraries(zr_vm_enumerator_protocol_test PRIVATE zr_vm_lib_iteration_static)
    endif ()
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
            zr_vm_span_core_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_span_core.c
            ${CMAKE_SOURCE_DIR}/tests/parser/test_span_gc_cases.c
            ${CMAKE_SOURCE_DIR}/tests/parser/test_span_semantic_ir_cases.c
            ${CMAKE_SOURCE_DIR}/tests/container/container_test_common.c
    )
    add_test(
            NAME parser_span_inline_receiver
            COMMAND $<TARGET_FILE:zr_vm_span_core_test> --inline-receiver
    )
    zr_vm_add_unity_test_target(
            zr_vm_buffer_pool_ffi_test
            ${CMAKE_SOURCE_DIR}/tests/parser/test_buffer_pool_ffi.c
            ${CMAKE_SOURCE_DIR}/tests/container/container_test_common.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_generational_pool_test
            ${CMAKE_SOURCE_DIR}/tests/container/test_generational_pool.c
            ${CMAKE_SOURCE_DIR}/tests/container/container_test_common.c
            ${CMAKE_SOURCE_DIR}/tests/unity_lifecycle_noop.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_generational_pool_gc_stress_test
            ${CMAKE_SOURCE_DIR}/tests/container/test_generational_pool_gc_stress.c
            ${CMAKE_SOURCE_DIR}/tests/unity_lifecycle_noop.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_generational_pool_performance_matrix_test
            ${CMAKE_SOURCE_DIR}/tests/container/test_generational_pool_performance_matrix.c
            ${CMAKE_SOURCE_DIR}/tests/unity_lifecycle_noop.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_generational_pool_type_layout_test
            ${CMAKE_SOURCE_DIR}/tests/container/test_generational_pool_type_layout.c
            ${CMAKE_SOURCE_DIR}/tests/unity_lifecycle_noop.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_pooling_closed_type_runtime_test
            ${CMAKE_SOURCE_DIR}/tests/container/test_pooling_closed_type_runtime.c
            ${CMAKE_SOURCE_DIR}/tests/container/container_test_common.c
            ${CMAKE_SOURCE_DIR}/tests/unity_lifecycle_noop.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_generational_pool_artifact_test
            ${CMAKE_SOURCE_DIR}/tests/container/test_generational_pool_artifact.c
            ${CMAKE_SOURCE_DIR}/tests/container/container_test_common.c
            ${CMAKE_SOURCE_DIR}/tests/unity_lifecycle_noop.c
    )
    target_include_directories(zr_vm_buffer_pool_ffi_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/backend_aot
    )
    zr_vm_add_unity_test_target(
            zr_vm_task_runtime_test
            ${CMAKE_SOURCE_DIR}/tests/task/test_task_runtime.c
            ${CMAKE_SOURCE_DIR}/tests/unity_lifecycle_noop.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_task_job_scheduler_test
            ${CMAKE_SOURCE_DIR}/tests/task/test_task_job_scheduler.c
            ${CMAKE_SOURCE_DIR}/tests/unity_lifecycle_noop.c
    )
    zr_vm_add_unity_test_target(
            zr_vm_task_frame_runtime_test
            ${CMAKE_SOURCE_DIR}/tests/task/test_task_frame_runtime.c
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
            zr_vm_container_temp_value_root_test
            zr_vm_span_core_test
            zr_vm_buffer_pool_ffi_test
            zr_vm_generational_pool_test
            zr_vm_generational_pool_gc_stress_test
            zr_vm_generational_pool_performance_matrix_test
            zr_vm_pooling_closed_type_runtime_test
            zr_vm_generational_pool_artifact_test)
        target_include_directories(${container_target} PRIVATE
                ${CMAKE_SOURCE_DIR}/tests/container
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
                ${CMAKE_SOURCE_DIR}/zr_vm_core/include
                ${CMAKE_SOURCE_DIR}/zr_vm_library/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_math/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_iteration/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_ffi/include
        )
        zr_vm_link_parser_core_plus_library(${container_target})
        if (BUILD_SHARED_LIB)
            target_link_libraries(${container_target} PRIVATE zr_vm_lib_iteration_shared)
        else ()
            target_link_libraries(${container_target} PRIVATE zr_vm_lib_iteration_static)
        endif ()
    endforeach ()

    target_include_directories(zr_vm_generational_pool_type_layout_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
            ${CMAKE_SOURCE_DIR}/zr_vm_lib_container/include
    )
    if (BUILD_SHARED_LIB)
        target_link_libraries(zr_vm_generational_pool_type_layout_test PRIVATE
                zr_vm_core_shared
                zr_vm_lib_container_shared
        )
    else ()
        target_link_libraries(zr_vm_generational_pool_type_layout_test PRIVATE
                zr_vm_core_static
                zr_vm_lib_container_static
        )
    endif ()

    target_include_directories(zr_vm_task_runtime_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_include_directories(zr_vm_task_job_scheduler_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_library/include
    )
    target_include_directories(zr_vm_task_frame_runtime_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
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
    zr_vm_link_core(zr_vm_task_job_scheduler_test)
    if (BUILD_SHARED_LIB)
        target_link_libraries(zr_vm_task_job_scheduler_test PRIVATE
                zr_vm_parser_shared
                zr_vm_core_shared
                zr_vm_library_shared
        )
    else ()
        target_link_libraries(zr_vm_task_job_scheduler_test PRIVATE
                zr_vm_parser_static
                zr_vm_core_static
                zr_vm_library_static
        )
    endif ()
    zr_vm_link_core(zr_vm_task_frame_runtime_test)

    if (TARGET zr_vm_thread_runtime_test)
        add_test(NAME thread_attached_domain COMMAND zr_vm_thread_runtime_test --attached)
        set_tests_properties(thread_attached_domain PROPERTIES TIMEOUT 60)
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
