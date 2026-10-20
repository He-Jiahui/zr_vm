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

if (TARGET zr_vm_debug_hook_core_test)
    add_test(
            NAME debug_hook_core
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=debug_hook_core"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_debug_hook_core_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_debug_hook_core_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_debug_hook_core_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_debug_hook_core_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_debug_introspection_test)
    add_test(
            NAME debug_introspection
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=debug_introspection"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_debug_introspection_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_debug_introspection_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_debug_introspection_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_debug_introspection_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_debug_snapshot_contracts_test)
    add_test(
            NAME debug_snapshot_contracts
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=debug_snapshot_contracts"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_debug_snapshot_contracts_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_debug_snapshot_contracts_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_debug_snapshot_contracts_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_debug_snapshot_contracts_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_metadata_token_model_test)
    add_test(
            NAME metadata_token_model
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=metadata_token_model"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_metadata_token_model_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_metadata_token_model_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_metadata_token_model_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_metadata_token_model_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_metadata_type_ref_binding_test)
    add_test(
            NAME metadata_type_ref_binding
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=metadata_type_ref_binding"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_metadata_type_ref_binding_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_metadata_type_ref_binding_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_metadata_type_ref_binding_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_metadata_type_ref_binding_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_metadata_runtime_query_test)
    add_test(
            NAME metadata_runtime_query
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=metadata_runtime_query"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_metadata_runtime_query_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_metadata_runtime_query_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_metadata_runtime_query_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_metadata_runtime_query_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_metadata_runtime_manifest_exports_test)
    add_test(
            NAME metadata_runtime_manifest_exports
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=metadata_runtime_manifest_exports"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_metadata_runtime_manifest_exports_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_metadata_runtime_manifest_exports_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_metadata_runtime_manifest_exports_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_metadata_runtime_manifest_exports_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_metadata_runtime_method_binding_test)
    add_test(
            NAME metadata_runtime_method_binding
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=metadata_runtime_method_binding"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_metadata_runtime_method_binding_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_metadata_runtime_method_binding_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_metadata_runtime_method_binding_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_metadata_runtime_method_binding_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_reflection_token_resolve_test)
    add_test(
            NAME reflection_token_resolve
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=reflection_token_resolve"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_reflection_token_resolve_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_reflection_token_resolve_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_reflection_token_resolve_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_reflection_token_resolve_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_reflection_method_invoke_test)
    add_test(
            NAME reflection_method_invoke
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=reflection_method_invoke"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_reflection_method_invoke_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_reflection_method_invoke_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_reflection_method_invoke_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_reflection_method_invoke_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_metadata_runtime_binding_compatibility_test)
    add_test(
            NAME metadata_runtime_binding_compatibility
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=metadata_runtime_binding_compatibility"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_metadata_runtime_binding_compatibility_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_metadata_runtime_binding_compatibility_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_metadata_runtime_binding_compatibility_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_metadata_runtime_binding_compatibility_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_aot_runtime_typed_direct_call_compatibility_test)
    add_test(
            NAME aot_runtime_typed_direct_call_compatibility
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=aot_runtime_typed_direct_call_compatibility"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_aot_runtime_typed_direct_call_compatibility_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_aot_runtime_typed_direct_call_compatibility_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_aot_runtime_typed_direct_call_compatibility_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_aot_runtime_typed_direct_call_compatibility_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_metadata_runtime_typespec_layout_test)
    add_test(
            NAME metadata_runtime_typespec_layout
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=metadata_runtime_typespec_layout"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_metadata_runtime_typespec_layout_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_metadata_runtime_typespec_layout_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_metadata_runtime_typespec_layout_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_metadata_runtime_typespec_layout_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_reflection_dynamic_generic_instance_test)
    add_test(
            NAME reflection_dynamic_generic_instance
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=reflection_dynamic_generic_instance"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_reflection_dynamic_generic_instance_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_reflection_dynamic_generic_instance_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_reflection_dynamic_generic_instance_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_reflection_dynamic_generic_instance_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_metadata_runtime_type_layout_test)
    add_test(
            NAME metadata_runtime_type_layout
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=metadata_runtime_type_layout"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_metadata_runtime_type_layout_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_metadata_runtime_type_layout_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_metadata_runtime_type_layout_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_metadata_runtime_type_layout_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_zrp_metadata_format_test)
    add_test(
            NAME zrp_metadata_format
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=zrp_metadata_format"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_zrp_metadata_format_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_zrp_metadata_format_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_zrp_metadata_format_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_zrp_metadata_format_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_aot_reachability_test)
    add_test(
            NAME aot_reachability
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=aot_reachability"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_aot_reachability_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_aot_reachability_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_aot_reachability_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_aot_reachability_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_aot_c_code_stripping_test)
    add_test(
            NAME aot_c_code_stripping
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=aot_c_code_stripping"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_aot_c_code_stripping_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_aot_c_code_stripping_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_aot_c_code_stripping_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_aot_c_code_stripping_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_aot_c_zrp_metadata_publication_test)
    add_test(
            NAME aot_c_zrp_metadata_publication
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=aot_c_zrp_metadata_publication"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_aot_c_zrp_metadata_publication_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_aot_c_zrp_metadata_publication_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_aot_c_zrp_metadata_publication_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_aot_c_zrp_metadata_publication_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_aot_c_reflection_annotation_preserve_test)
    add_test(
            NAME aot_c_reflection_annotation_preserve
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=aot_c_reflection_annotation_preserve"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_aot_c_reflection_annotation_preserve_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_aot_c_reflection_annotation_preserve_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_aot_c_reflection_annotation_preserve_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_aot_c_reflection_annotation_preserve_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_aot_c_zrp_metadata_pruning_test)
    add_test(
            NAME aot_c_zrp_metadata_pruning
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=aot_c_zrp_metadata_pruning"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_aot_c_zrp_metadata_pruning_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_aot_c_zrp_metadata_pruning_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_aot_c_zrp_metadata_pruning_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_aot_c_zrp_metadata_pruning_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_aot_c_zrp_metadata_methodspec_pruning_test)
    add_test(
            NAME aot_c_zrp_metadata_methodspec_pruning
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=aot_c_zrp_metadata_methodspec_pruning"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_aot_c_zrp_metadata_methodspec_pruning_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_aot_c_zrp_metadata_methodspec_pruning_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_aot_c_zrp_metadata_methodspec_pruning_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_aot_c_zrp_metadata_methodspec_pruning_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_aot_c_zrp_metadata_typespec_pruning_test)
    add_test(
            NAME aot_c_zrp_metadata_typespec_pruning
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=aot_c_zrp_metadata_typespec_pruning"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_aot_c_zrp_metadata_typespec_pruning_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_aot_c_zrp_metadata_typespec_pruning_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_aot_c_zrp_metadata_typespec_pruning_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_aot_c_zrp_metadata_typespec_pruning_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_aot_c_zrp_metadata_module_ref_pruning_test)
    add_test(
            NAME aot_c_zrp_metadata_module_ref_pruning
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=aot_c_zrp_metadata_module_ref_pruning"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_aot_c_zrp_metadata_module_ref_pruning_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_aot_c_zrp_metadata_module_ref_pruning_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_aot_c_zrp_metadata_module_ref_pruning_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_aot_c_zrp_metadata_module_ref_pruning_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_aot_c_zrp_metadata_export_token_remap_test)
    add_test(
            NAME aot_c_zrp_metadata_export_token_remap
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=aot_c_zrp_metadata_export_token_remap"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_aot_c_zrp_metadata_export_token_remap_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_aot_c_zrp_metadata_export_token_remap_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_aot_c_zrp_metadata_export_token_remap_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_aot_c_zrp_metadata_export_token_remap_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_aot_c_zrp_metadata_size_deltas_test)
    add_test(
            NAME aot_c_zrp_metadata_size_deltas
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=aot_c_zrp_metadata_size_deltas"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_aot_c_zrp_metadata_size_deltas_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_aot_c_zrp_metadata_size_deltas_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_aot_c_zrp_metadata_size_deltas_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_aot_c_zrp_metadata_size_deltas_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_aot_c_zrp_metadata_pool_pruning_test)
    add_test(
            NAME aot_c_zrp_metadata_pool_pruning
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=aot_c_zrp_metadata_pool_pruning"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_aot_c_zrp_metadata_pool_pruning_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_aot_c_zrp_metadata_pool_pruning_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_aot_c_zrp_metadata_pool_pruning_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_aot_c_zrp_metadata_pool_pruning_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_aot_llvm_symbol_stripping_test)
    add_test(
            NAME aot_llvm_symbol_stripping
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=aot_llvm_symbol_stripping"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_aot_llvm_symbol_stripping_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_aot_llvm_symbol_stripping_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_aot_llvm_symbol_stripping_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_aot_llvm_symbol_stripping_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_metadata_module_hash_golden_test)
    add_test(
            NAME metadata_module_hash_golden
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=metadata_module_hash_golden"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_metadata_module_hash_golden_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_metadata_module_hash_golden_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_metadata_module_hash_golden_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_metadata_module_hash_golden_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_native_registry_descriptor_invalidation_test)
    add_test(
            NAME native_registry_descriptor_invalidation
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=native_registry_descriptor_invalidation"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_native_registry_descriptor_invalidation_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_native_registry_descriptor_invalidation_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_native_registry_descriptor_invalidation_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_native_registry_descriptor_invalidation_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_aot_c_descriptor_diagnostics_test)
    add_test(
            NAME aot_c_descriptor_diagnostics
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=aot_c_descriptor_diagnostics"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_aot_c_descriptor_diagnostics_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_aot_c_descriptor_diagnostics_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_aot_c_descriptor_diagnostics_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_aot_c_descriptor_diagnostics_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_official_provider_convergence_test)
    add_test(
            NAME official_provider_convergence
            COMMAND $<TARGET_FILE:zr_vm_official_provider_convergence_test>
    )
endif ()

if (TARGET zr_vm_aot_c_metadata_binding_loader_test)
    add_test(
            NAME aot_c_metadata_binding_loader
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=aot_c_metadata_binding_loader"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_aot_c_metadata_binding_loader_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_aot_c_metadata_binding_loader_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_aot_c_metadata_binding_loader_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_aot_c_metadata_binding_loader_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_type_layout_metadata_contracts_test)
    add_test(
            NAME type_layout_metadata_contracts
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=type_layout_metadata_contracts"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_type_layout_metadata_contracts_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_type_layout_metadata_contracts_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_type_layout_metadata_contracts_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_type_layout_metadata_contracts_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_aot_gc_root_frame_test)
    add_test(
            NAME aot_gc_root_frame
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=aot_gc_root_frame"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_aot_gc_root_frame_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_aot_gc_root_frame_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_aot_gc_root_frame_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_aot_gc_root_frame_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_value_construction_profile_test)
    add_test(
            NAME value_construction_profile
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=value_construction_profile"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_value_construction_profile_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_value_construction_profile_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_value_construction_profile_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_value_construction_profile_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_super_array_raw_int_canonical_storage_test)
    add_test(
            NAME super_array_raw_int_canonical_storage
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=super_array_raw_int_canonical_storage"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_super_array_raw_int_canonical_storage_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_super_array_raw_int_canonical_storage_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_super_array_raw_int_canonical_storage_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_super_array_raw_int_canonical_storage_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_semir_static_c_types_test)
    add_test(
            NAME semir_static_c_types
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=semir_static_c_types"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_semir_static_c_types_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_semir_static_c_types_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_semir_static_c_types_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_semir_static_c_types_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_semir_typed_opcode_guardrails_test)
    add_test(
            NAME semir_typed_opcode_guardrails
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=semir_typed_opcode_guardrails"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_semir_typed_opcode_guardrails_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_semir_typed_opcode_guardrails_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_semir_typed_opcode_guardrails_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_semir_typed_opcode_guardrails_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_semir_dynamic_arithmetic_deopt_test)
    add_test(
            NAME semir_dynamic_arithmetic_deopt
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=semir_dynamic_arithmetic_deopt"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_semir_dynamic_arithmetic_deopt_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_semir_dynamic_arithmetic_deopt_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_semir_dynamic_arithmetic_deopt_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_semir_dynamic_arithmetic_deopt_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_semir_type_conflict_deopt_test)
    add_test(
            NAME semir_type_conflict_deopt
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=semir_type_conflict_deopt"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_semir_type_conflict_deopt_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_semir_type_conflict_deopt_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_semir_type_conflict_deopt_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_semir_type_conflict_deopt_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_semir_dynamic_member_deopt_test)
    add_test(
            NAME semir_dynamic_member_deopt
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=semir_dynamic_member_deopt"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_semir_dynamic_member_deopt_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_semir_dynamic_member_deopt_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_semir_dynamic_member_deopt_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_semir_dynamic_member_deopt_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_semir_dynamic_call_deopt_test)
    add_test(
            NAME semir_dynamic_call_deopt
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=semir_dynamic_call_deopt"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_semir_dynamic_call_deopt_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_semir_dynamic_call_deopt_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_semir_dynamic_call_deopt_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_semir_dynamic_call_deopt_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_semir_dynamic_iter_deopt_test)
    add_test(
            NAME semir_dynamic_iter_deopt
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=semir_dynamic_iter_deopt"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_semir_dynamic_iter_deopt_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_semir_dynamic_iter_deopt_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_semir_dynamic_iter_deopt_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_semir_dynamic_iter_deopt_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_semir_dynamic_index_deopt_test)
    add_test(
            NAME semir_dynamic_index_deopt
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=semir_dynamic_index_deopt"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_semir_dynamic_index_deopt_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_semir_dynamic_index_deopt_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_semir_dynamic_index_deopt_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_semir_dynamic_index_deopt_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_aot_c_type_layout_contracts_test)
    add_test(
            NAME aot_c_type_layout_contracts
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=aot_c_type_layout_contracts"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_aot_c_type_layout_contracts_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_aot_c_type_layout_contracts_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_aot_c_type_layout_contracts_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_aot_c_type_layout_contracts_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_aot_c_guardrail_contracts_test)
    add_test(
            NAME aot_c_guardrail_contracts
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=aot_c_guardrail_contracts"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_aot_c_guardrail_contracts_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_aot_c_guardrail_contracts_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_aot_c_guardrail_contracts_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_aot_c_guardrail_contracts_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_aot_c_value_construction_guardrail_test)
    add_test(
            NAME aot_c_value_construction_guardrail
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=aot_c_value_construction_guardrail"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_aot_c_value_construction_guardrail_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_aot_c_value_construction_guardrail_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_aot_c_value_construction_guardrail_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_aot_c_value_construction_guardrail_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_generic_instantiation_test)
    add_test(
            NAME generic_instantiation
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=generic_instantiation"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_generic_instantiation_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_generic_instantiation_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_generic_instantiation_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_generic_instantiation_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_generic_constraints_test)
    add_test(
            NAME generic_constraints
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=generic_constraints"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_generic_constraints_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_generic_constraints_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_generic_constraints_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_generic_constraints_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_aot_c_generic_monomorphization_test)
    add_test(
            NAME aot_c_generic_monomorphization
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=aot_c_generic_monomorphization"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_aot_c_generic_monomorphization_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_aot_c_generic_monomorphization_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_aot_c_generic_monomorphization_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_aot_c_generic_monomorphization_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_aot_c_generic_reference_sharing_test)
    add_test(
            NAME aot_c_generic_reference_sharing
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=aot_c_generic_reference_sharing"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_aot_c_generic_reference_sharing_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_aot_c_generic_reference_sharing_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_aot_c_generic_reference_sharing_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_aot_c_generic_reference_sharing_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_aot_c_generic_call_typed_test)
    add_test(
            NAME aot_c_generic_call_typed
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=aot_c_generic_call_typed"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_aot_c_generic_call_typed_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_aot_c_generic_call_typed_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_aot_c_generic_call_typed_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_aot_c_generic_call_typed_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_aot_c_golden_scalar_smoke_test)
    add_test(
            NAME aot_c_golden_scalar_smoke
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=aot_c_golden_scalar_smoke"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_aot_c_golden_scalar_smoke_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_aot_c_golden_scalar_smoke_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_aot_c_golden_scalar_smoke_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_aot_c_golden_scalar_smoke_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_aot_c_typed_scalar_test)
    add_test(
            NAME aot_c_typed_scalar
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=aot_c_typed_scalar"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_aot_c_typed_scalar_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_aot_c_typed_scalar_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_aot_c_typed_scalar_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_aot_c_typed_scalar_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_aot_c_load_const_scalar_test)
    add_test(
            NAME aot_c_load_const_scalar
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=aot_c_load_const_scalar"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_aot_c_load_const_scalar_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_aot_c_load_const_scalar_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_aot_c_load_const_scalar_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_aot_c_load_const_scalar_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_aot_c_method_info_signature_test)
    add_test(
            NAME aot_c_method_info_signature
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=aot_c_method_info_signature"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_aot_c_method_info_signature_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_aot_c_method_info_signature_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_aot_c_method_info_signature_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_aot_c_method_info_signature_test>"
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

if (TARGET zr_vm_debug_traceback_test)
    add_test(
            NAME debug_traceback
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=debug_traceback"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_debug_traceback_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_debug_traceback_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_debug_traceback_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_debug_traceback_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_debug_disassemble_test)
    add_test(
            NAME debug_disassemble
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=debug_disassemble"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_debug_disassemble_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_debug_disassemble_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_debug_disassemble_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_debug_disassemble_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_debug_heap_summary_test)
    add_test(
            NAME debug_heap_summary
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=debug_heap_summary"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_debug_heap_summary_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_debug_heap_summary_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_debug_heap_summary_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_debug_heap_summary_test>"
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

if (TARGET zr_vm_debug_step_edges_test)
    add_test(
            NAME debug_step_edges
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=debug_step_edges"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_debug_step_edges_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_debug_step_edges_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_debug_step_edges_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_debug_step_edges_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_debug_threads_test)
    add_test(
            NAME debug_threads
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=debug_threads"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_debug_threads_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_debug_threads_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_debug_threads_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_debug_threads_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_debug_data_breakpoint_test)
    add_test(
            NAME debug_data_breakpoint
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=debug_data_breakpoint"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_debug_data_breakpoint_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_debug_data_breakpoint_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_debug_data_breakpoint_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_debug_data_breakpoint_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_profile_deterministic_test)
    add_test(
            NAME profile_deterministic
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=profile_deterministic"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_profile_deterministic_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_profile_deterministic_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_profile_deterministic_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_profile_deterministic_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_coverage_test)
    add_test(
            NAME coverage
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=coverage"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_coverage_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_coverage_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_coverage_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_coverage_test>"
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

if (TARGET zr_vm_debug_expression_diagnostics_test)
    add_test(
            NAME debug_expression_diagnostics
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=debug_expression_diagnostics"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_debug_expression_diagnostics_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_debug_expression_diagnostics_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_debug_expression_diagnostics_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_debug_expression_diagnostics_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_debug_truncation_test)
    add_test(
            NAME debug_truncation
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=debug_truncation"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_debug_truncation_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_debug_truncation_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_debug_truncation_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_debug_truncation_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_debug_variable_child_shape_test)
    add_test(
            NAME debug_variable_child_shape
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=debug_variable_child_shape"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_debug_variable_child_shape_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_debug_variable_child_shape_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_debug_variable_child_shape_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_debug_variable_child_shape_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()

if (TARGET zr_vm_debug_library_test)
    add_test(
            NAME debug_library
            COMMAND ${CMAKE_COMMAND}
            "-DSUITE_NAME=debug_library"
            "-DEXECUTABLES=$<TARGET_FILE:zr_vm_debug_library_test>"
            "-DEXECUTABLES_SMOKE=$<TARGET_FILE:zr_vm_debug_library_test>"
            "-DEXECUTABLES_CORE=$<TARGET_FILE:zr_vm_debug_library_test>"
            "-DEXECUTABLES_STRESS=$<TARGET_FILE:zr_vm_debug_library_test>"
            "-DHOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
            -P ${ZR_VM_SUITE_RUNNER_SCRIPT}
    )
endif ()
