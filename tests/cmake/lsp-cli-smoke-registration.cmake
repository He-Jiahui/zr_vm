if (ZR_VM_NODE_EXECUTABLE)
    add_test(
            NAME language_server_baseline_collector
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/language_server/collect_lsp_baseline_test.js
    )
    add_test(
            NAME language_server_stdio_smoke_outcome
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/language_server/stdio_smoke_outcome.test.js
    )
endif ()

if (TARGET zr_vm_language_server_stdio AND ZR_VM_NODE_EXECUTABLE)
    find_program(ZR_VM_CTEST_EXECUTABLE NAMES ctest REQUIRED)
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
            NAME language_server_stdio_cyclic_import_smoke
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/language_server/stdio_cyclic_import_smoke.js
                    $<TARGET_FILE:zr_vm_language_server_stdio>
    )
    add_test(
            NAME language_server_stdio_inline_value_semantic_smoke
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/language_server/stdio_inline_value_semantic_smoke.js
                    $<TARGET_FILE:zr_vm_language_server_stdio>
    )
    add_test(
            NAME language_server_stdio_diagnostic_fix_smoke
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/language_server/stdio_diagnostic_fix_smoke.js
                    $<TARGET_FILE:zr_vm_language_server_stdio>
    )
    add_test(
            NAME language_server_stdio_position_encoding_smoke
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/language_server/stdio_position_encoding_smoke.js
                    $<TARGET_FILE:zr_vm_language_server_stdio>
    )
    add_test(
            NAME language_server_stdio_type_hierarchy_smoke
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/language_server/stdio_type_hierarchy_smoke.js
                    $<TARGET_FILE:zr_vm_language_server_stdio>
    )
    add_test(
            NAME language_server_stdio_protocol_inventory
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/language_server/stdio_protocol_inventory.js
                    $<TARGET_FILE:zr_vm_language_server_stdio>
                    $<TARGET_FILE:zr_vm_language_server_lsp_capability_inventory_probe>
                    ${CMAKE_BINARY_DIR}
                    ${ZR_VM_CTEST_EXECUTABLE}
                    $<CONFIG>
    )
    add_test(
            NAME language_server_wasm_capability_inventory
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/language_server/wasm_capability_inventory.js
                    ${CMAKE_SOURCE_DIR}
    )
    add_test(
            NAME language_server_wasm_capability_inventory_regression
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/language_server/wasm_capability_inventory_test.js
    )
    add_test(
            NAME language_server_stdio_protocol_conformance
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/language_server/stdio_protocol_conformance.js
                    $<TARGET_FILE:zr_vm_language_server_stdio>
    )
    add_test(
            NAME language_server_stdio_protocol_envelope_mutations
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/language_server/stdio_protocol_envelope_mutations.js
                    $<TARGET_FILE:zr_vm_language_server_stdio>
    )
    add_test(
            NAME language_server_stdio_resolve_capabilities_smoke
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/language_server/stdio_resolve_capabilities_smoke.js
                    $<TARGET_FILE:zr_vm_language_server_stdio>
    )
    add_test(
            NAME language_server_stdio_optional_capabilities_smoke
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/language_server/stdio_optional_capabilities_smoke.js
                    $<TARGET_FILE:zr_vm_language_server_stdio>
    )
    add_test(
            NAME language_server_stdio_color_capability_smoke
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/language_server/stdio_color_capability_smoke.js
                    $<TARGET_FILE:zr_vm_language_server_stdio>
    )
    add_test(
            NAME language_server_stdio_save_capabilities_smoke
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/language_server/stdio_save_capabilities_smoke.js
                    $<TARGET_FILE:zr_vm_language_server_stdio>
    )
    add_test(
            NAME language_server_stdio_client_commands_smoke
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/language_server/stdio_client_commands_smoke.js
                    $<TARGET_FILE:zr_vm_language_server_stdio>
    )
    add_test(
            NAME language_server_stdio_navigation_capabilities_smoke
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/language_server/stdio_navigation_capabilities_smoke.js
                    $<TARGET_FILE:zr_vm_language_server_stdio>
    )
    add_test(
            NAME language_server_stdio_file_operation_capabilities_smoke
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/language_server/stdio_file_operation_capabilities_smoke.js
                    $<TARGET_FILE:zr_vm_language_server_stdio>
    )
    add_test(
            NAME language_server_stdio_document_sync_conformance
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/language_server/stdio_document_sync_conformance.js
                    $<TARGET_FILE:zr_vm_language_server_stdio>
    )
    add_test(
            NAME language_server_stdio_workspace_folders_smoke
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/language_server/stdio_workspace_folders_smoke.js
                    $<TARGET_FILE:zr_vm_language_server_stdio>
    )
    add_test(
            NAME language_server_stdio_snapshot_workspace_diagnostics_smoke
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/language_server/stdio_snapshot_workspace_diagnostics_smoke.js
                    $<TARGET_FILE:zr_vm_language_server_stdio>
    )
endif ()

if (TARGET zr_vm_cli_executable AND ZR_VM_NODE_EXECUTABLE)
    add_test(
            NAME cli_repl_expression_aggregate_smoke
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/cli/repl_expression_aggregate_smoke.js
                    $<TARGET_FILE:zr_vm_cli_executable>
    )
    add_test(
            NAME cli_repl_expression_array_display_smoke
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/cli/repl_expression_array_display_smoke.js
                    $<TARGET_FILE:zr_vm_cli_executable>
    )
    add_test(
            NAME cli_repl_expression_object_smoke
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/cli/repl_expression_object_smoke.js
                    $<TARGET_FILE:zr_vm_cli_executable>
    )
    add_test(
            NAME cli_repl_expression_assignment_context_smoke
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/cli/repl_expression_assignment_context_smoke.js
                    $<TARGET_FILE:zr_vm_cli_executable>
    )
    add_test(
            NAME cli_repl_type_ownership_smoke
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/cli/repl_type_ownership_smoke.js
                    $<TARGET_FILE:zr_vm_cli_executable>
    )
    add_test(
            NAME cli_repl_type_call_member_smoke
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/cli/repl_type_call_member_smoke.js
                    $<TARGET_FILE:zr_vm_cli_executable>
    )
    add_test(
            NAME cli_repl_type_call_reference_smoke
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/cli/repl_type_call_reference_smoke.js
                    $<TARGET_FILE:zr_vm_cli_executable>
    )
    add_test(
            NAME cli_repl_type_reachability_smoke
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/cli/repl_type_reachability_smoke.js
                    $<TARGET_FILE:zr_vm_cli_executable>
    )
    add_test(
            NAME cli_repl_type_conditional_branch_smoke
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/cli/repl_type_conditional_branch_smoke.js
                    $<TARGET_FILE:zr_vm_cli_executable>
    )
    add_test(
            NAME cli_repl_type_nested_numeric_smoke
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/cli/repl_type_nested_numeric_smoke.js
                    $<TARGET_FILE:zr_vm_cli_executable>
    )
    add_test(
            NAME cli_repl_type_nested_expression_fact_smoke
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/cli/repl_type_nested_expression_fact_smoke.js
                    $<TARGET_FILE:zr_vm_cli_executable>
    )
    add_test(
            NAME cli_repl_type_member_receiver_fact_smoke
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/cli/repl_type_member_receiver_fact_smoke.js
                    $<TARGET_FILE:zr_vm_cli_executable>
    )
    add_test(
            NAME cli_repl_type_lambda_body_fact_smoke
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/cli/repl_type_lambda_body_fact_smoke.js
                    $<TARGET_FILE:zr_vm_cli_executable>
    )
    add_test(
            NAME cli_repl_type_lambda_local_fact_smoke
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/cli/repl_type_lambda_local_fact_smoke.js
                    $<TARGET_FILE:zr_vm_cli_executable>
    )
    add_test(
            NAME cli_repl_type_nested_ownership_smoke
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/cli/repl_type_nested_ownership_smoke.js
                    $<TARGET_FILE:zr_vm_cli_executable>
    )
    add_test(
            NAME cli_repl_type_query_reference_smoke
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/cli/repl_type_query_reference_smoke.js
                    $<TARGET_FILE:zr_vm_cli_executable>
    )
    add_test(
            NAME cli_repl_type_logical_comparison_smoke
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/cli/repl_type_logical_comparison_smoke.js
                    $<TARGET_FILE:zr_vm_cli_executable>
    )
    add_test(
            NAME cli_repl_type_expression_fact_smoke
            COMMAND ${ZR_VM_NODE_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/cli/repl_type_expression_fact_smoke.js
                    $<TARGET_FILE:zr_vm_cli_executable>
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
    add_test(
            NAME testing_reference
            COMMAND ${CMAKE_COMMAND}
            "-DCLI_EXE=$<TARGET_FILE:zr_vm_cli_executable>"
            "-DPROJECT_FILE=${ZR_VM_TESTS_SOURCE_DIR_NORMALIZED}/fixtures/projects/testing_reference/testing_reference.zrp"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_testing_reference.cmake
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

if (TARGET zr_vm_cli_syntax_migration_test)
    add_test(
            NAME cli_syntax_migration
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=cli_syntax_migration"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_cli_syntax_migration_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_cli_syntax_migration_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_cli_syntax_migration_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_cli_syntax_migration_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            "-DRUN_WORKING_DIRECTORY=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_legacy_migration_test)
    add_test(
            NAME legacy_migration
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=legacy_migration"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_legacy_migration_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_legacy_migration_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_legacy_migration_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_legacy_migration_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            "-DRUN_WORKING_DIRECTORY=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_property_consumer_contracts_test)
    add_test(
            NAME property_consumer_contracts
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=property_consumer_contracts"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_property_consumer_contracts_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_property_consumer_contracts_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_property_consumer_contracts_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_property_consumer_contracts_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            "-DRUN_WORKING_DIRECTORY=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_cli_zrp_metadata_dump_test)
    add_test(
            NAME cli_zrp_metadata_dump
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=cli_zrp_metadata_dump"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_cli_zrp_metadata_dump_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_cli_zrp_metadata_dump_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_cli_zrp_metadata_dump_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_cli_zrp_metadata_dump_test>"
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

if (TARGET zr_vm_cli_aot_writer_options_test)
    add_test(
            NAME cli_aot_writer_options
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=cli_aot_writer_options"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_cli_aot_writer_options_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_cli_aot_writer_options_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_cli_aot_writer_options_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_cli_aot_writer_options_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            "-DRUN_WORKING_DIRECTORY=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_cli_aot_compacted_metadata_sidecar_test)
    add_test(
            NAME cli_aot_compacted_metadata_sidecar
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=cli_aot_compacted_metadata_sidecar"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_cli_aot_compacted_metadata_sidecar_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_cli_aot_compacted_metadata_sidecar_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_cli_aot_compacted_metadata_sidecar_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_cli_aot_compacted_metadata_sidecar_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            "-DRUN_WORKING_DIRECTORY=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_cli_import_basic_fixture_test)
    add_test(
            NAME cli_import_basic_fixture
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=cli_import_basic_fixture"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_cli_import_basic_fixture_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_cli_import_basic_fixture_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_cli_import_basic_fixture_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_cli_import_basic_fixture_test>"
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
