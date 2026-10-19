    zr_vm_add_unity_test_target(
            zr_vm_debug_introspection_test
            ${CMAKE_SOURCE_DIR}/tests/debug/test_debug_introspection.c
    )
    target_include_directories(zr_vm_debug_introspection_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_parser_core(zr_vm_debug_introspection_test)

    zr_vm_add_unity_test_target(
            zr_vm_debug_snapshot_contracts_test
            ${CMAKE_SOURCE_DIR}/tests/debug/test_debug_snapshot_contracts.c
    )
    target_compile_definitions(zr_vm_debug_snapshot_contracts_test PRIVATE
            ZR_VM_TESTS_REPO_ROOT="${ZR_VM_TESTS_REPO_ROOT_NORMALIZED}"
    )
    target_include_directories(zr_vm_debug_snapshot_contracts_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
    )
    zr_vm_link_core(zr_vm_debug_snapshot_contracts_test)

    if ((TARGET zr_vm_debug_shared OR TARGET zr_vm_debug_static) AND
            (TARGET zr_vm_lib_network_shared OR TARGET zr_vm_lib_network_static))
        zr_vm_add_unity_test_target(
                zr_vm_debug_expression_diagnostics_test
                ${CMAKE_SOURCE_DIR}/tests/debug/test_debug_expression_diagnostics.c
        )
        target_include_directories(zr_vm_debug_expression_diagnostics_test PRIVATE
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
                ${CMAKE_SOURCE_DIR}/zr_vm_core/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_debug/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_debug/src/zr_vm_lib_debug
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_network/include
        )
        zr_vm_link_parser_core(zr_vm_debug_expression_diagnostics_test)
        if (BUILD_SHARED_LIB)
            target_link_libraries(zr_vm_debug_expression_diagnostics_test PRIVATE
                    zr_vm_debug_shared
                    zr_vm_lib_network_shared
            )
        else ()
            target_link_libraries(zr_vm_debug_expression_diagnostics_test PRIVATE
                    zr_vm_debug_static
                    zr_vm_lib_network_static
            )
        endif ()
        zr_link_third_party_for_target(zr_vm_debug_expression_diagnostics_test "zr_c_json")
        target_link_libraries(zr_vm_debug_expression_diagnostics_test PRIVATE Threads::Threads)

        zr_vm_add_unity_test_target(
                zr_vm_debug_truncation_test
                ${CMAKE_SOURCE_DIR}/tests/debug/test_debug_truncation.c
        )
        target_include_directories(zr_vm_debug_truncation_test PRIVATE
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
                ${CMAKE_SOURCE_DIR}/zr_vm_core/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_debug/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_debug/src/zr_vm_lib_debug
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_network/include
        )
        zr_vm_link_parser_core(zr_vm_debug_truncation_test)
        if (BUILD_SHARED_LIB)
            target_link_libraries(zr_vm_debug_truncation_test PRIVATE
                    zr_vm_debug_shared
                    zr_vm_lib_network_shared
            )
        else ()
            target_link_libraries(zr_vm_debug_truncation_test PRIVATE
                    zr_vm_debug_static
                    zr_vm_lib_network_static
            )
        endif ()
        zr_link_third_party_for_target(zr_vm_debug_truncation_test "zr_c_json")

        if (TARGET zr_vm_lib_system_shared OR TARGET zr_vm_lib_system_static)
            zr_vm_add_unity_test_target(
                    zr_vm_debug_variable_child_shape_test
                    ${CMAKE_SOURCE_DIR}/tests/debug/test_debug_variable_child_shape.c
            )
            target_include_directories(zr_vm_debug_variable_child_shape_test PRIVATE
                    ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
                    ${CMAKE_SOURCE_DIR}/zr_vm_core/include
                    ${CMAKE_SOURCE_DIR}/zr_vm_lib_debug/include
                    ${CMAKE_SOURCE_DIR}/zr_vm_lib_network/include
                    ${CMAKE_SOURCE_DIR}/zr_vm_library/include
                    ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include
            )
            zr_vm_link_parser_core(zr_vm_debug_variable_child_shape_test)
            if (BUILD_SHARED_LIB)
                target_link_libraries(zr_vm_debug_variable_child_shape_test PRIVATE
                        zr_vm_debug_shared
                        zr_vm_lib_network_shared
                        zr_vm_lib_system_shared
                )
            else ()
                target_link_libraries(zr_vm_debug_variable_child_shape_test PRIVATE
                        zr_vm_debug_static
                        zr_vm_lib_network_static
                        zr_vm_lib_system_static
                )
            endif ()
            zr_link_third_party_for_target(zr_vm_debug_variable_child_shape_test "zr_c_json")
            target_link_libraries(zr_vm_debug_variable_child_shape_test PRIVATE Threads::Threads)
        endif ()
    endif ()

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
                zr_vm_debug_step_edges_test
                ${CMAKE_SOURCE_DIR}/tests/debug/test_debug_step_edges.c
        )
        target_include_directories(zr_vm_debug_step_edges_test PRIVATE
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
                ${CMAKE_SOURCE_DIR}/zr_vm_core/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_debug/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_network/include
        )
        zr_vm_link_parser_core(zr_vm_debug_step_edges_test)
        if (BUILD_SHARED_LIB)
            target_link_libraries(zr_vm_debug_step_edges_test PRIVATE
                    zr_vm_debug_shared
                    zr_vm_lib_network_shared
                    zr_vm_lib_system_shared
            )
        else ()
            target_link_libraries(zr_vm_debug_step_edges_test PRIVATE
                    zr_vm_debug_static
                    zr_vm_lib_network_static
                    zr_vm_lib_system_static
            )
        endif ()
        zr_link_third_party_for_target(zr_vm_debug_step_edges_test "zr_c_json")
        target_link_libraries(zr_vm_debug_step_edges_test PRIVATE Threads::Threads)

        zr_vm_add_unity_test_target(
                zr_vm_debug_threads_test
                ${CMAKE_SOURCE_DIR}/tests/debug/test_debug_threads.c
        )
        target_include_directories(zr_vm_debug_threads_test PRIVATE
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
                ${CMAKE_SOURCE_DIR}/zr_vm_core/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_debug/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_network/include
        )
        zr_vm_link_parser_core(zr_vm_debug_threads_test)
        if (BUILD_SHARED_LIB)
            target_link_libraries(zr_vm_debug_threads_test PRIVATE
                    zr_vm_debug_shared
                    zr_vm_lib_network_shared
                    zr_vm_lib_system_shared
            )
        else ()
            target_link_libraries(zr_vm_debug_threads_test PRIVATE
                    zr_vm_debug_static
                    zr_vm_lib_network_static
                    zr_vm_lib_system_static
            )
        endif ()
        zr_link_third_party_for_target(zr_vm_debug_threads_test "zr_c_json")
        target_link_libraries(zr_vm_debug_threads_test PRIVATE Threads::Threads)

        zr_vm_add_unity_test_target(
                zr_vm_debug_data_breakpoint_test
                ${CMAKE_SOURCE_DIR}/tests/debug/test_debug_data_breakpoint.c
        )
        target_include_directories(zr_vm_debug_data_breakpoint_test PRIVATE
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
                ${CMAKE_SOURCE_DIR}/zr_vm_core/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_debug/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_network/include
        )
        zr_vm_link_parser_core(zr_vm_debug_data_breakpoint_test)
        if (BUILD_SHARED_LIB)
            target_link_libraries(zr_vm_debug_data_breakpoint_test PRIVATE
                    zr_vm_debug_shared
                    zr_vm_lib_network_shared
                    zr_vm_lib_system_shared
            )
        else ()
            target_link_libraries(zr_vm_debug_data_breakpoint_test PRIVATE
                    zr_vm_debug_static
                    zr_vm_lib_network_static
                    zr_vm_lib_system_static
            )
        endif ()
        zr_link_third_party_for_target(zr_vm_debug_data_breakpoint_test "zr_c_json")
        target_link_libraries(zr_vm_debug_data_breakpoint_test PRIVATE Threads::Threads)

        zr_vm_add_unity_test_target(
                zr_vm_profile_deterministic_test
                ${CMAKE_SOURCE_DIR}/tests/profile/test_profile_deterministic.c
        )
        target_include_directories(zr_vm_profile_deterministic_test PRIVATE
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
                ${CMAKE_SOURCE_DIR}/zr_vm_core/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_debug/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_network/include
        )
        zr_vm_link_parser_core(zr_vm_profile_deterministic_test)
        if (BUILD_SHARED_LIB)
            target_link_libraries(zr_vm_profile_deterministic_test PRIVATE
                    zr_vm_debug_shared
                    zr_vm_lib_network_shared
                    zr_vm_lib_system_shared
            )
        else ()
            target_link_libraries(zr_vm_profile_deterministic_test PRIVATE
                    zr_vm_debug_static
                    zr_vm_lib_network_static
                    zr_vm_lib_system_static
            )
        endif ()
        zr_link_third_party_for_target(zr_vm_profile_deterministic_test "zr_c_json")
        target_link_libraries(zr_vm_profile_deterministic_test PRIVATE Threads::Threads)

        zr_vm_add_unity_test_target(
                zr_vm_coverage_test
                ${CMAKE_SOURCE_DIR}/tests/profile/test_coverage.c
        )
        target_include_directories(zr_vm_coverage_test PRIVATE
                ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
                ${CMAKE_SOURCE_DIR}/zr_vm_core/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_debug/include
                ${CMAKE_SOURCE_DIR}/zr_vm_lib_network/include
        )
        zr_vm_link_parser_core(zr_vm_coverage_test)
        if (BUILD_SHARED_LIB)
            target_link_libraries(zr_vm_coverage_test PRIVATE
                    zr_vm_debug_shared
                    zr_vm_lib_network_shared
                    zr_vm_lib_system_shared
            )
        else ()
            target_link_libraries(zr_vm_coverage_test PRIVATE
                    zr_vm_debug_static
                    zr_vm_lib_network_static
                    zr_vm_lib_system_static
            )
        endif ()
        zr_link_third_party_for_target(zr_vm_coverage_test "zr_c_json")
        target_link_libraries(zr_vm_coverage_test PRIVATE Threads::Threads)

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

