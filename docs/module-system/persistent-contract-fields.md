---
related_code:
  - CMakeLists.txt
  - zr_vm_aot/README.md
  - zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/aot_c/src/main.c
  - zr_vm_aot/tests/fixtures/projects/aot_eh_tail_gc_stress/bin/aot_c/src/main.c
  - zr_vm_aot/tests/fixtures/projects/aot_module_graph_pipeline/bin/aot_c/src/graph_binary_stage.c
  - zr_vm_aot/tests/fixtures/projects/aot_module_graph_pipeline/bin/aot_c/src/graph_stage_a.c
  - zr_vm_aot/tests/fixtures/projects/aot_module_graph_pipeline/bin/aot_c/src/graph_stage_b.c
  - zr_vm_aot/tests/fixtures/projects/aot_module_graph_pipeline/bin/aot_c/src/main.c
  - zr_vm_aot/tests/scripts/CMakeLists.txt
  - zr_vm_aot/tests/scripts/test_utils.c
  - zr_vm_aot/zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - zr_vm_aot/zr_vm_library/include/zr_vm_library/aot_runtime.h
  - zr_vm_cli/src/zr_vm_cli/commands/explain_optimize_command.c
  - zr_vm_common/CommonMacros.cmake
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - zr_vm_common/include/zr_vm_common/zr_common_conf.h
  - zr_vm_common/include/zr_vm_common/zr_ffi_contract.h
  - zr_vm_common/include/zr_vm_common/zr_io_conf.h
  - zr_vm_common/include/zr_vm_common/zr_version_info.h
  - zr_vm_core/include/zr_vm_core/artifact_exec_ir.h
  - zr_vm_core/include/zr_vm_core/artifact_schema.h
  - zr_vm_core/include/zr_vm_core/call_binding.h
  - zr_vm_core/include/zr_vm_core/capability_manifest.h
  - zr_vm_core/include/zr_vm_core/constant_reference.h
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/include/zr_vm_core/execution_backend.h
  - zr_vm_core/include/zr_vm_core/execution_contract.h
  - zr_vm_core/include/zr_vm_core/execution_control.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/gc.h
  - zr_vm_core/include/zr_vm_core/gc_domain.h
  - zr_vm_core/include/zr_vm_core/gc_domain_clone.h
  - zr_vm_core/include/zr_vm_core/hotpatch_generation.h
  - zr_vm_core/include/zr_vm_core/hotpatch_rollback.h
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/include/zr_vm_core/metadata_runtime.h
  - zr_vm_core/include/zr_vm_core/metadata_token.h
  - zr_vm_core/include/zr_vm_core/optimization_remark.h
  - zr_vm_core/include/zr_vm_core/reflection.h
  - zr_vm_core/src/zr_vm_core/aot/aot_ir.c
  - zr_vm_core/src/zr_vm_core/artifact/artifact_call_binding.c
  - zr_vm_core/src/zr_vm_core/artifact/artifact_encoding.c
  - zr_vm_core/src/zr_vm_core/artifact/artifact_exec_ir.c
  - zr_vm_core/src/zr_vm_core/artifact/artifact_identity.c
  - zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c
  - zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c
  - zr_vm_core/src/zr_vm_core/artifact/artifact_schema_metadata_graph.c
  - zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c
  - zr_vm_core/src/zr_vm_core/artifact/canonical_consumer.c
  - zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c
  - zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c
  - zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c
  - zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5.c
  - zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5_read.c
  - zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5_write.c
  - zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6.c
  - zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6_read.c
  - zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6_write.c
  - zr_vm_core/src/zr_vm_core/artifact/zrp_metadata.c
  - zr_vm_core/src/zr_vm_core/base/hash.c
  - zr_vm_core/src/zr_vm_core/call/call_binding.c
  - zr_vm_core/src/zr_vm_core/call/call_binding_contract_internal.h
  - zr_vm_core/src/zr_vm_core/call/call_binding_encoding.c
  - zr_vm_core/src/zr_vm_core/call/call_binding_inline_frame.c
  - zr_vm_core/src/zr_vm_core/call/call_binding_layout.c
  - zr_vm_core/src/zr_vm_core/call/call_binding_link.c
  - zr_vm_core/src/zr_vm_core/call/call_binding_member.c
  - zr_vm_core/src/zr_vm_core/call/call_binding_signature.c
  - zr_vm_core/src/zr_vm_core/call/typed_call_binding.c
  - zr_vm_core/src/zr_vm_core/closure/closure_close_meta_guard.c
  - zr_vm_core/src/zr_vm_core/exception/exception_roots_internal.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_compile.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_facade.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_internal.h
  - zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c
  - zr_vm_core/src/zr_vm_core/execution/execution_control.c
  - zr_vm_core/src/zr_vm_core/function/function.c
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/gc/gc_budget_runtime.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_clone.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_share.c
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_capability.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_rollback.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_validate.c
  - zr_vm_core/src/zr_vm_core/io/io.c
  - zr_vm_core/src/zr_vm_core/io/io_bytecode_verify.h
  - zr_vm_core/src/zr_vm_core/io/io_call_binding.c
  - zr_vm_core/src/zr_vm_core/io/io_runtime.c
  - zr_vm_core/src/zr_vm_core/module/module.c
  - zr_vm_core/src/zr_vm_core/module/module_call_binding.c
  - zr_vm_core/src/zr_vm_core/module/module_exec_ir_artifact.c
  - zr_vm_core/src/zr_vm_core/ownership/ownership_resource.c
  - zr_vm_core/src/zr_vm_core/ownership/ownership_transfer.c
  - zr_vm_core/src/zr_vm_core/ownership/ownership_transfer_cross_domain.c
  - zr_vm_core/src/zr_vm_core/ownership/ownership_transfer_cross_domain_internal.h
  - zr_vm_core/src/zr_vm_core/ownership/ownership_transfer_value_copy.c
  - zr_vm_core/src/zr_vm_core/runtime/execution_contract.c
  - zr_vm_core/src/zr_vm_core/runtime/optimization_remark.c
  - zr_vm_core/src/zr_vm_core/type/type_layout.c
  - zr_vm_jit/src/orc_backend.cpp
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_optimization_remarks.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_isolated_domain.c
  - zr_vm_library/include/zr_vm_library/aot_runtime.h
  - zr_vm_library/include/zr_vm_library/project.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_typed_call.c
  - zr_vm_parser/CMakeLists.txt
  - zr_vm_parser/include/zr_vm_parser/aot_ir_lowering.h
  - zr_vm_parser/include/zr_vm_parser/canonical_type.h
  - zr_vm_parser/include/zr_vm_parser/compile_ir_cache.h
  - zr_vm_parser/include/zr_vm_parser/compile_optimization_profile.h
  - zr_vm_parser/include/zr_vm_parser/exec_ir_builder.h
  - zr_vm_parser/include/zr_vm_parser/exec_ir_pass_manager.h
  - zr_vm_parser/include/zr_vm_parser/exec_ir_profile.h
  - zr_vm_parser/include/zr_vm_parser/writer.h
  - zr_vm_parser/src/zr_vm_parser/artifact/artifact_call_binding_projection.c
  - zr_vm_parser/src/zr_vm_parser/artifact/artifact_exec_ir.h
  - zr_vm_parser/src/zr_vm_parser/artifact/artifact_metadata_projection.c
  - zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_call_bindings.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_method_metadata.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_manifest_export.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_prune.h
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_adapter.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_module_artifacts.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_ir_cache.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_optimization_profile.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_module_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_callable_identity.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_source_callable_identity.c
  - zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_callable_binding_refinement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler/extern/compiler_ffi_callable_contract.c
  - zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_metadata_module_hash.c
  - zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_metadata_signature.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_source_retention.c
  - zr_vm_parser/src/zr_vm_parser/contract/syntax_contract.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_branch_facts.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_call_graph.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_escape.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_lowering.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_cleanup_drops.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_dead_source_places.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effects.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_host_aot_target.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_aot.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_profile.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_common.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projections.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa_promotion.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/execbc/exec_ir_lower_execbc.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_allocation.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_licm.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_ownership_elision.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_callable_query.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module_validation.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_frame.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_frame_validation.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_integer_expression.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_module_contract.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_scalar_constant.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_super_constructor_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/types/canonical_type.c
  - zr_vm_parser/src/zr_vm_parser/types/canonical_type_adapter.c
  - zr_vm_parser/src/zr_vm_parser/types/canonical_type_projection.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c
implementation_files:
  - zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/aot_c/src/main.c
  - zr_vm_aot/tests/fixtures/projects/aot_eh_tail_gc_stress/bin/aot_c/src/main.c
  - zr_vm_aot/tests/fixtures/projects/aot_module_graph_pipeline/bin/aot_c/src/graph_binary_stage.c
  - zr_vm_aot/tests/fixtures/projects/aot_module_graph_pipeline/bin/aot_c/src/graph_stage_a.c
  - zr_vm_aot/tests/fixtures/projects/aot_module_graph_pipeline/bin/aot_c/src/graph_stage_b.c
  - zr_vm_aot/tests/fixtures/projects/aot_module_graph_pipeline/bin/aot_c/src/main.c
  - zr_vm_aot/tests/scripts/test_utils.c
  - zr_vm_aot/zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - zr_vm_aot/zr_vm_library/include/zr_vm_library/aot_runtime.h
  - zr_vm_cli/src/zr_vm_cli/commands/explain_optimize_command.c
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - zr_vm_common/include/zr_vm_common/zr_common_conf.h
  - zr_vm_common/include/zr_vm_common/zr_ffi_contract.h
  - zr_vm_common/include/zr_vm_common/zr_io_conf.h
  - zr_vm_common/include/zr_vm_common/zr_version_info.h
  - zr_vm_core/include/zr_vm_core/artifact_exec_ir.h
  - zr_vm_core/include/zr_vm_core/artifact_schema.h
  - zr_vm_core/include/zr_vm_core/call_binding.h
  - zr_vm_core/include/zr_vm_core/capability_manifest.h
  - zr_vm_core/include/zr_vm_core/constant_reference.h
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/include/zr_vm_core/execution_backend.h
  - zr_vm_core/include/zr_vm_core/execution_contract.h
  - zr_vm_core/include/zr_vm_core/execution_control.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/gc.h
  - zr_vm_core/include/zr_vm_core/gc_domain.h
  - zr_vm_core/include/zr_vm_core/gc_domain_clone.h
  - zr_vm_core/include/zr_vm_core/hotpatch_generation.h
  - zr_vm_core/include/zr_vm_core/hotpatch_rollback.h
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/include/zr_vm_core/metadata_runtime.h
  - zr_vm_core/include/zr_vm_core/metadata_token.h
  - zr_vm_core/include/zr_vm_core/optimization_remark.h
  - zr_vm_core/include/zr_vm_core/reflection.h
  - zr_vm_core/src/zr_vm_core/aot/aot_ir.c
  - zr_vm_core/src/zr_vm_core/artifact/artifact_call_binding.c
  - zr_vm_core/src/zr_vm_core/artifact/artifact_encoding.c
  - zr_vm_core/src/zr_vm_core/artifact/artifact_exec_ir.c
  - zr_vm_core/src/zr_vm_core/artifact/artifact_identity.c
  - zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c
  - zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c
  - zr_vm_core/src/zr_vm_core/artifact/artifact_schema_metadata_graph.c
  - zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c
  - zr_vm_core/src/zr_vm_core/artifact/canonical_consumer.c
  - zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c
  - zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c
  - zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c
  - zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5.c
  - zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5_read.c
  - zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5_write.c
  - zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6.c
  - zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6_read.c
  - zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6_write.c
  - zr_vm_core/src/zr_vm_core/artifact/zrp_metadata.c
  - zr_vm_core/src/zr_vm_core/base/hash.c
  - zr_vm_core/src/zr_vm_core/call/call_binding.c
  - zr_vm_core/src/zr_vm_core/call/call_binding_contract_internal.h
  - zr_vm_core/src/zr_vm_core/call/call_binding_encoding.c
  - zr_vm_core/src/zr_vm_core/call/call_binding_inline_frame.c
  - zr_vm_core/src/zr_vm_core/call/call_binding_layout.c
  - zr_vm_core/src/zr_vm_core/call/call_binding_link.c
  - zr_vm_core/src/zr_vm_core/call/call_binding_member.c
  - zr_vm_core/src/zr_vm_core/call/call_binding_signature.c
  - zr_vm_core/src/zr_vm_core/call/typed_call_binding.c
  - zr_vm_core/src/zr_vm_core/closure/closure_close_meta_guard.c
  - zr_vm_core/src/zr_vm_core/exception/exception_roots_internal.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_compile.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_facade.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_internal.h
  - zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c
  - zr_vm_core/src/zr_vm_core/execution/execution_control.c
  - zr_vm_core/src/zr_vm_core/function/function.c
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/gc/gc_budget_runtime.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_clone.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_share.c
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_capability.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_rollback.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_validate.c
  - zr_vm_core/src/zr_vm_core/io/io.c
  - zr_vm_core/src/zr_vm_core/io/io_bytecode_verify.h
  - zr_vm_core/src/zr_vm_core/io/io_call_binding.c
  - zr_vm_core/src/zr_vm_core/io/io_runtime.c
  - zr_vm_core/src/zr_vm_core/module/module.c
  - zr_vm_core/src/zr_vm_core/module/module_call_binding.c
  - zr_vm_core/src/zr_vm_core/module/module_exec_ir_artifact.c
  - zr_vm_core/src/zr_vm_core/ownership/ownership_resource.c
  - zr_vm_core/src/zr_vm_core/ownership/ownership_transfer.c
  - zr_vm_core/src/zr_vm_core/ownership/ownership_transfer_cross_domain.c
  - zr_vm_core/src/zr_vm_core/ownership/ownership_transfer_cross_domain_internal.h
  - zr_vm_core/src/zr_vm_core/ownership/ownership_transfer_value_copy.c
  - zr_vm_core/src/zr_vm_core/runtime/execution_contract.c
  - zr_vm_core/src/zr_vm_core/runtime/optimization_remark.c
  - zr_vm_core/src/zr_vm_core/type/type_layout.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_optimization_remarks.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_isolated_domain.c
  - zr_vm_library/include/zr_vm_library/aot_runtime.h
  - zr_vm_library/include/zr_vm_library/project.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_typed_call.c
  - zr_vm_parser/include/zr_vm_parser/aot_ir_lowering.h
  - zr_vm_parser/include/zr_vm_parser/canonical_type.h
  - zr_vm_parser/include/zr_vm_parser/compile_ir_cache.h
  - zr_vm_parser/include/zr_vm_parser/compile_optimization_profile.h
  - zr_vm_parser/include/zr_vm_parser/exec_ir_builder.h
  - zr_vm_parser/include/zr_vm_parser/exec_ir_pass_manager.h
  - zr_vm_parser/include/zr_vm_parser/exec_ir_profile.h
  - zr_vm_parser/include/zr_vm_parser/writer.h
  - zr_vm_parser/src/zr_vm_parser/artifact/artifact_call_binding_projection.c
  - zr_vm_parser/src/zr_vm_parser/artifact/artifact_exec_ir.h
  - zr_vm_parser/src/zr_vm_parser/artifact/artifact_metadata_projection.c
  - zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_call_bindings.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_method_metadata.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_manifest_export.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_prune.h
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_adapter.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_module_artifacts.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_ir_cache.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_optimization_profile.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_module_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_callable_identity.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_source_callable_identity.c
  - zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_callable_binding_refinement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler/extern/compiler_ffi_callable_contract.c
  - zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_metadata_module_hash.c
  - zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_metadata_signature.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_source_retention.c
  - zr_vm_parser/src/zr_vm_parser/contract/syntax_contract.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_branch_facts.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_call_graph.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_escape.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_lowering.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_cleanup_drops.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_dead_source_places.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effects.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_host_aot_target.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_aot.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_profile.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_common.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projections.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa_promotion.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/execbc/exec_ir_lower_execbc.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_allocation.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_licm.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_ownership_elision.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_callable_query.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module_validation.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_frame.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_frame_validation.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_integer_expression.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_module_contract.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_scalar_constant.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_super_constructor_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/types/canonical_type.c
  - zr_vm_parser/src/zr_vm_parser/types/canonical_type_adapter.c
  - zr_vm_parser/src/zr_vm_parser/types/canonical_type_projection.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c
plan_sources:
  - docs/plans/ssa/00-measurement-contracts/02-contract-freeze.md
  - .codex/plans/20261018-ssa-contract-freeze.md
  - user: 2026-10-18 Source42 contract dictionary text freeze
tests:
  - tests/core/ssa_runtime_objects_concurrency.c
  - tests/core/test_aot_gc_root_frame.c
  - tests/core/test_close_proxy.c
  - tests/core/test_gc_concurrent_major.c
  - tests/core/test_gc_domain_bridge.c
  - tests/core/test_gc_domain_multimutator.c
  - tests/core/test_resource_cross_domain_transfer.c
  - tests/core/test_resource_cross_domain_transfer_races.c
  - tests/core/test_resource_cross_domain_transfer_review_cases.h
  - tests/core/test_resource_cross_domain_transfer_value_copy_cases.h
  - tests/core/test_resource_same_domain_handoff.c
  - tests/core/test_session_checkpoint.c
  - tests/core/test_ssa_backend_admission.c
  - tests/core/test_ssa_backend_service.c
  - tests/core/test_ssa_cross_domain_clone.c
  - tests/core/test_ssa_cross_domain_clone_oom.c
  - tests/core/test_ssa_host_jit_optional.c
  - tests/core/test_ssa_major_budget_runtime.c
  - tests/core/test_ssa_runtime_objects.c
  - tests/ffi/test_native_extern_contract.c
  - tests/library/test_ssa_capability_validation.c
  - tests/library/test_ssa_exec_ir_artifact_v6.c
  - tests/library/test_ssa_rollback_restricted.c
  - tests/library/test_ssa_schema_relocation.c
  - tests/module/test_metadata_runtime_manifest_exports.c
  - tests/module/test_zrp_metadata_format.c
  - tests/parser/ssa_state_map_fixture.h
  - tests/parser/support/ssa_dce_dead_phis_faults.c
  - tests/parser/support/ssa_dce_dead_phis_fixture.c
  - tests/parser/support/ssa_gc_map_source_preflight_fixture.c
  - tests/parser/support/ssa_literal_script_fixture.c
  - tests/parser/support/ssa_phi_identity_fixture.c
  - tests/parser/support/ssa_phi_identity_oracle.c
  - tests/parser/support/ssa_phi_identity_transactions.c
  - tests/parser/support/ssa_primitive_frame_validation_fixture.c
  - tests/parser/support/ssa_sccp_branch_sensitive_pool_fixture.c
  - tests/parser/support/ssa_sccp_pooled_bool_faults.c
  - tests/parser/support/ssa_sccp_pooled_bool_fixture.c
  - tests/parser/support/ssa_sccp_pooled_int64_fixture.c
  - tests/parser/support/ssa_source_finalized_module_fixture.c
  - tests/parser/support/ssa_source_integer_expression_faults_fixture.c
  - tests/parser/test_aot_c_code_stripping.c
  - tests/parser/test_aot_c_descriptor_diagnostics.c
  - tests/parser/test_aot_c_generic_bool_equality_local_smoke.c
  - tests/parser/test_aot_c_generic_call_result_stack_copy_equality_local_smoke.c
  - tests/parser/test_aot_c_generic_equality_stack_copy_local_smoke.c
  - tests/parser/test_aot_c_generic_jump_if_bool_local_smoke.c
  - tests/parser/test_aot_c_generic_logical_not_numeric_local_smoke.c
  - tests/parser/test_aot_c_generic_not_equal_stack_copy_jump_if_local_smoke.c
  - tests/parser/test_aot_c_golden_scalar_smoke.c
  - tests/parser/test_aot_c_load_const_scalar.c
  - tests/parser/test_aot_c_logical_shared_library_smoke.c
  - tests/parser/test_aot_c_method_info_signature.c
  - tests/parser/test_aot_c_shared_library_smoke.c
  - tests/parser/test_aot_c_source_contracts.c
  - tests/parser/test_aot_c_typed_scalar.c
  - tests/parser/test_aot_c_value_type_shared_library_smoke.c
  - tests/parser/test_aot_c_zrp_metadata_methodspec_pruning.c
  - tests/parser/test_aot_c_zrp_metadata_pruning.c
  - tests/parser/test_ownership_suspended_roots_cases.h
  - tests/parser/test_ssa_aot_canonical_abi.c
  - tests/parser/test_ssa_aot_owned_module_faults.c
  - tests/parser/test_ssa_aot_projection_branch_descriptor.c
  - tests/parser/test_ssa_aotir_state_map.c
  - tests/parser/test_ssa_branch_arithmetic_constraints.c
  - tests/parser/test_ssa_branch_loop_ranges.c
  - tests/parser/test_ssa_builder_cfg.c
  - tests/parser/test_ssa_builder_cleanup_dispatch.c
  - tests/parser/test_ssa_builder_control_edges.c
  - tests/parser/test_ssa_builder_dominance.c
  - tests/parser/test_ssa_builder_fact_identity.c
  - tests/parser/test_ssa_builder_iterator_invokes.c
  - tests/parser/test_ssa_c_llvm_lowering.c
  - tests/parser/test_ssa_cleanup_exception_state.c
  - tests/parser/test_ssa_compare_metadata_guards.c
  - tests/parser/test_ssa_compare_predicate_domain.c
  - tests/parser/test_ssa_conditional_cleanup.c
  - tests/parser/test_ssa_construction.c
  - tests/parser/test_ssa_contract_freeze.c
  - tests/parser/test_ssa_core_model.c
  - tests/parser/test_ssa_dce_phi_liveness.c
  - tests/parser/test_ssa_dead_source_places.c
  - tests/parser/test_ssa_deopt_aggregates.c
  - tests/parser/test_ssa_deopt_validation.c
  - tests/parser/test_ssa_effects_verifier.c
  - tests/parser/test_ssa_escape_ownership.c
  - tests/parser/test_ssa_execbc_place.c
  - tests/parser/test_ssa_execbc_vm.c
  - tests/parser/test_ssa_gvn_range.c
  - tests/parser/test_ssa_host_noargs_i64_aot_target.c
  - tests/parser/test_ssa_interprocedural_inlining.c
  - tests/parser/test_ssa_invoke_result_availability.c
  - tests/parser/test_ssa_licm_scalar_legality.c
  - tests/parser/test_ssa_loops_specialization.c
  - tests/parser/test_ssa_optimization_remarks.c
  - tests/parser/test_ssa_oracle_barrier_differential.c
  - tests/parser/test_ssa_oracle_call_differential.c
  - tests/parser/test_ssa_oracle_drop_differential.c
  - tests/parser/test_ssa_oracle_invoke_differential.c
  - tests/parser/test_ssa_oracle_iterator_differential.c
  - tests/parser/test_ssa_oracle_memory_differential.c
  - tests/parser/test_ssa_oracle_parallel_edges.c
  - tests/parser/test_ssa_oracle_projections.c
  - tests/parser/test_ssa_oracle_resume.c
  - tests/parser/test_ssa_oracle_suspend_differential.c
  - tests/parser/test_ssa_oracle_throw_differential.c
  - tests/parser/test_ssa_pass_manager_scalar.c
  - tests/parser/test_ssa_place_eligibility.c
  - tests/parser/test_ssa_place_promotion.c
  - tests/parser/test_ssa_primitive_frame_validation.c
  - tests/parser/test_ssa_primitive_source_frame.c
  - tests/parser/test_ssa_primitive_source_frame_oom.c
  - tests/parser/test_ssa_primitive_source_frame_storage.c
  - tests/parser/test_ssa_sccp_conversion.c
  - tests/parser/test_ssa_sccp_copy_availability.c
  - tests/parser/test_ssa_sccp_phi_copy_edges.c
  - tests/parser/test_ssa_source_aot_descriptor.c
  - tests/parser/test_ssa_source_aot_owned_module.c
  - tests/parser/test_ssa_source_aot_scalar_text.c
  - tests/parser/test_ssa_source_cfg_faults.c
  - tests/parser/test_ssa_source_cleanup_cfg.c
  - tests/parser/test_ssa_source_descriptor_oom.c
  - tests/parser/test_ssa_source_execbc_vm.c
  - tests/parser/test_ssa_source_for_short_circuit.c
  - tests/parser/test_ssa_source_integer_expression_proof.c
  - tests/parser/test_ssa_source_integer_expression_retention.c
  - tests/parser/test_ssa_source_scalar_constant.c
  - tests/parser/test_ssa_source_straight_line_cfg.c
  - tests/parser/test_ssa_source_value_facts.c
  - tests/parser/test_ssa_source_while_short_circuit.c
  - tests/parser/test_ssa_value_validation.c
  - tests/parser/test_token_source_diagnostic_evidence_cases.h
  - tests/thread/test_thread_runtime.c
doc_type: module-detail
---


# 持久契约字段字典

## 导航与冻结范围

[运行期 owner](../core-runtime/execution-contract-ownership.md) · [持久字段](../module-system/persistent-contract-fields.md) · [接口矩阵](../testing-and-validation/ssa-interface-contract-matrix.md) · [兼容政策](../testing-and-validation/ssa-contract-compatibility.md)。四页共同构成 00.02 的文字字典；[47 叶依赖图](../plans/ssa/index.md)与各叶运行验收保持独立。文档冻结不授予实现、加载、安全测试或运行权限；planned API 不据此成为已实现。


## 计数、owner 与字段寻址

本页给出全部已审计 record 和字段路径；它不把 opaque byte window 宣称为未实现格式的完整内层语法。每个字段继承所在 record 的唯一 encoding owner、坐标域、writer、reader/admission 和 hash 规则。嵌套语义 owner 以运行期页的类型/domain 政策为准；wire encoder 不获得 metadata/capability 权威。

| 有限清单 | 分母及解释 |
| --- | --- |
| base F | 117 record templates；824 wire leaf templates + 12 execution semantic fields = 836 目录行 |
| coordinate overlay | 8 records / 208字段修正 offset 基准；新增字段 0；其余 109 records 按 dispositions 保留 |
| old IO | 117 source-function grammar records（52 writer、65 reader）；976 events；706 direct primitives + 269 nested + 1 源语法 synthetic event |
| independent site check | reader358 / writer345 是独立 source-site 视图；不能与824或976相加 |
| old logical declarations | 65 records /527 fields；C 声明不是 wire 字段数 |
| base declarations | 60 structs /563 fields；与527重叠，不能机械相加 |
| packed prototype | 已有 pack1：14 header fields/60B +31 u32 member fields/124B =45 inner fields |
| H/C | 28+3=31 hash entries、215+100=315 selected functions 的有限 union，不是全项目分母 |

legacy 原 counts.source_pins=35 为错误值；current view 固定为实际18 unique paths。coordinate15 与该18互斥，合并33 unique，不授35信用。旧报告保持不可变。CBND21已在base，不重复加作旧IO新增。

### 坐标与 domain

固定 codec 字段标注为 field@offset:width，单位字节；little-endian grammar 明确按 codec，不使用 C offsetof。record-relative 的容器地址 = record_base + field.offset。重复记录还须按 stride/index 或实际前缀和计算。只有表中标明 container-absolute 的 ZRAF 三组保留绝对位置，不能再加 record_base。execution 12 semantic 行的“offset”只是声明次序的字典坐标，绝不是 native C ABI、独立 disk record 或 sizeof 证明。

旧 IO 使用 native fwrite/reader 语法，cursor 从每个函数调用起点0开始；按 active branch/patch/loop 顺序执行：leaf += 实际 width_formula；nested += 该 callee 本次 encoded_size；未选分支消耗0，错误退出不构成合法后缀。变长字符串、数组和嵌套记录必须计算前缀和，不给伪固定 offset。native size/endianness/enum/float 限制仍有效。

语义域按字段名和 record 共同决定：token = metadata table/RID（非全局 TypeId）；canonicalTypeId = 生产 context 身份（跨 context 需映射）；signature/layout/module/policy/hash = 各自 H 根的值（同为u64不互换）；value/block/instruction/source IDs = 所属 IR/module 局部索引；count/offset/length = 对应 bounded span，必须防溢出并验证范围；reserved = 所属 codec 当前准入政策，不从 writer 零推导 reader 拒绝；generation = 持久语义值，不能作为 runtime epoch；flags/effects/capabilities = 所属枚举/版本域，effect 不授 host 权限。不能归入这些域的字段保持其 record 的专用 grammar 含义，不按名字推断新 ABI。

### hash 覆盖：H0/Hspan/Hstate/Hrecord

- **H0**：该 record 没有自动逐字段重算的 digest 合同。携带一个 hash 不证明当前 reader 重新生成它；具体 provenance 见 H31 索引。
- **Hspan**：ERI/EIS payload 可由 ArtifactExecIr_HashBytes 覆盖所选 byte span，非零 expected hash 才触发对应重算；不能扩展为整个 ZRAF 或运行期对象真实性证明。
- **Hstate**：MetadataStateHash 的明确 u32/u64序列，排除 metadataHash 自身；非任意 raw struct bytes。
- **Hrecord**：MetadataRecordHash 明确八个u32再 exact payload bytes，排除 recordHash；payloadLength 需匹配。
- 外层 HotPatch content witness 对选定完整内容字节重算是另一 admission 层；不能用它填补任意内层 semantic validation 或签名/授权缺口。完整有序 hash 根在兼容页，下面每个字段继承该行覆盖范围。

## 固定 codec 全 117 record /836字段索引

F key 由 format/record/field 构成，可精确查询 base.F；下列已应用208坐标 overlay。各行列出所有字段；writer、reader 和 admission 均保留真实 symbol，不把声明当执行或链接。

### F001 ZRAF6 / header

Owner: Core artifact grammar；size/stride=`32`；repeat=`None`；retain_container_absolute; offset 已是相应 container 内绝对值；hash=H0。

字段：`magic@0:4`; `schemaVersion@4:2`; `headerSize@6:2`; `kind@8:4`; `flags@12:4`; `sectionCount@16:4`; `directoryEntrySize@20:4`; `totalSize@24:4`; `reserved28@28:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 

### F002 ZRAF6 / directory

Owner: Core artifact grammar；size/stride=`24`；repeat=`sectionCount`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`kind@0:4`; `flags@4:4`; `byteOffset@8:4`; `byteLength@12:4`; `elementCount@16:4`; `elementSize@20:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 

### F003 ZRAF6 / SZrArtifactPublicIdentity

Owner: Core artifact grammar；size/stride=`72`；repeat=`None`；retain_container_absolute; offset 已是相应 container 内绝对值；hash=H0。

字段：`canonicalTypeId@32:4`; `typeRefToken@36:4`; `typeSpecToken@40:4`; `signatureToken@44:4`; `typeRefHash@48:8`; `typeSpecHash@56:8`; `signatureHash@64:8`; `layoutVersion@72:4`; `reserved0@76:4`; `layoutHash@80:8`; `callableContractHash@88:8`; `moduleHash@96:8`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::artifact_write_identity`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::artifact_read_identity`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 

### F004 ZRAF6 / SZrArtifactTypeDefRow

Owner: Core artifact grammar；size/stride=`48`；repeat=`[*]`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`token@0:4`; `flags@4:4`; `canonicalTypeId@8:4`; `constructorToken@12:4`; `constructorSignatureToken@16:4`; `reserved0@20:4`; `typeSignatureHash@24:8`; `constructorContractHash@32:8`; `reserved1@40:8`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::artifact_write_type_def_row`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::ZrCore_Artifact_ReadTypeDefRow`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 

### F005 ZRAF6 / SZrArtifactTypeIdentityRow.TYPE_REF

Owner: Core artifact grammar；size/stride=`48`；repeat=`[*]`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`token@0:4`; `signatureToken@4:4`; `canonicalTypeId@8:4`; `flags@12:4`; `signatureOffset@16:4`; `signatureLength@20:4`; `signatureHash@24:8`; `layoutVersion@32:4`; `reserved0@36:4`; `layoutHash@40:8`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::artifact_write_type_identity_row`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::ZrCore_Artifact_ReadTypeIdentityRow`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 

### F006 ZRAF6 / SZrArtifactMemberDefRow

Owner: Core artifact grammar；size/stride=`40`；repeat=`[*]`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`token@0:4`; `ownerTypeToken@4:4`; `signatureToken@8:4`; `flags@12:4`; `nameStringOffset@16:4`; `reserved0@20:4`; `signatureHash@24:8`; `contractHash@32:8`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::artifact_write_member_row`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::ZrCore_Artifact_ReadMemberDefRow`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 

### F007 ZRAF6 / SZrArtifactPropertyDefRow

Owner: Core artifact grammar；size/stride=`48`；repeat=`[*]`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`token@0:4`; `ownerTypeToken@4:4`; `getterToken@8:4`; `setterToken@12:4`; `signatureToken@16:4`; `flags@20:4`; `signatureHash@24:8`; `contractHash@32:8`; `initializerToken@40:4`; `nameStringOffset@44:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::artifact_write_property_row`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::ZrCore_Artifact_ReadPropertyDefRow`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 

### F008 ZRAF6 / SZrArtifactContractRow

Owner: Core artifact grammar；size/stride=`40`；repeat=`[*]`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`memberToken@0:4`; `signatureToken@4:4`; `parameterCount@8:4`; `flags@12:4`; `receiverEffect@16:4`; `refExportEffect@20:4`; `escapeFlags@24:4`; `abiLoweringKind@28:4`; `contractHash@32:8`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::artifact_write_contract_row`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::ZrCore_Artifact_ReadContractRow`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 

### F009 ZRAF6 / SZrArtifactLayoutRow

Owner: Core artifact grammar；size/stride=`48`；repeat=`[*]`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`typeToken@0:4`; `version@4:4`; `byteSize@8:4`; `byteAlignment@12:4`; `gcScanKind@16:4`; `capabilityFlags@20:4`; `ownershipMapOffset@24:4`; `ownershipMapLength@28:4`; `layoutHash@32:8`; `stableSlotContractHash@40:8`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::artifact_write_layout_row`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::ZrCore_Artifact_ReadLayoutRow`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 

### F010 ZRAF6 / SZrArtifactMetadataStateRow

Owner: Core artifact grammar；size/stride=`64`；repeat=`[*]`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hstate。

字段：`typeToken@0:4`; `preservationState@4:4`; `category@8:4`; `metadataGeneration@12:4`; `retainedMemberCount@16:4`; `retainedPropertyCount@20:4`; `retainedMetaRecordCount@24:4`; `flags@28:4`; `typeSignatureHash@32:8`; `layoutHash@40:8`; `callableContractHash@48:8`; `metadataHash@56:8`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::artifact_write_metadata_state_row`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::ZrCore_Artifact_ReadMetadataStateRow`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 

### F011 ZRAF6 / SZrArtifactMetadataRecordRow

Owner: Core artifact grammar；size/stride=`40`；repeat=`[*]`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hrecord。

字段：`ownerToken@0:4`; `kind@4:4`; `retention@8:4`; `flags@12:4`; `payloadOffset@16:4`; `payloadLength@20:4`; `metadataGeneration@24:4`; `reserved0@28:4`; `recordHash@32:8`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::artifact_write_metadata_record_row`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::ZrCore_Artifact_ReadMetadataRecordRow`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 

### F012 ZRAF6 / SZrArtifactDomainTransferRow

Owner: Core artifact grammar；size/stride=`48`；repeat=`[*]`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`typeToken@0:4`; `kind@4:4`; `schemaVersion@8:4`; `flags@12:4`; `schemaOffset@16:4`; `schemaLength@20:4`; `providerToken@24:4`; `reserved0@28:4`; `schemaHash@32:8`; `providerContractHash@40:8`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::artifact_write_domain_transfer_row`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::ZrCore_Artifact_ReadDomainTransferRow`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 

### F013 ZRAF6 / SZrArtifactSchedulerContractRow

Owner: Core artifact grammar；size/stride=`48`；repeat=`[*]`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`schedulerTypeToken@0:4`; `taskTypeToken@4:4`; `jobTypeToken@8:4`; `abiVersion@12:4`; `policyMask@16:4`; `attachedRequirementFlags@20:4`; `isolatedRequirementFlags@24:4`; `reserved0@28:4`; `transportContractHash@32:8`; `schedulerContractHash@40:8`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::artifact_write_scheduler_contract_row`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::ZrCore_Artifact_ReadSchedulerContractRow`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 

### F014 ZRAF6 / SZrArtifactRelocationRow

Owner: Core artifact grammar；size/stride=`40`；repeat=`[*]`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`codeOffset@0:4`; `kind@4:4`; `targetToken@8:4`; `targetSignatureToken@12:4`; `expectedSignatureHash@16:8`; `expectedContractHash@24:8`; `expectedModuleHash@32:8`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::artifact_write_relocation_row`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::ZrCore_Artifact_ReadRelocationRow`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 

### F015 ZRAF6 / header_tail

Owner: Core artifact grammar；size/stride=`8`；repeat=`None`；retain_container_absolute; offset 已是相应 container 内绝对值；hash=H0。

字段：`reserved104@104:8`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 

限制：memset writes zero; reader does not enforce。

### F016 CallBinding1 / contract

Owner: Core call-binding codec；size/stride=`64`；repeat=`None`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`bindingKind@0:4`; `targetMetadataToken@4:4`; `signatureToken@8:4`; `ownerTypeToken@12:4`; `signatureHash@16:8`; `moduleSignatureHash@24:8`; `layoutVersion@32:4`; `dispatchSlot@36:4`; `layoutHash@40:8`; `operation@48:4`; `reserved0@52:4`; `reserved1@56:8`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/call/call_binding_encoding.c::ZrCore_CallBinding_EncodeContract`。 Reader: `zr_vm_core/src/zr_vm_core/call/call_binding_encoding.c::zr_call_binding_decode_contract_status`。 Admission: `zr_vm_core/src/zr_vm_core/call/call_binding_contract_internal.h::zr_core_call_binding_check_contract`。 

### F017 CallBinding1 / location

Owner: Core call-binding codec；size/stride=`16`；repeat=`None`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`kind@0:4`; `targetIndex@4:4`; `ownerDepth@8:4`; `flags@12:4`。

Producer/encoder: `zr_vm_parser/src/zr_vm_parser/writer/writer_call_binding.c::ZrParser_Writer_WriteCallBindings`。 Reader: `zr_vm_core/src/zr_vm_core/io/io_call_binding.c::ZrCore_Io_ReadCallBindings`。 Admission: `zr_vm_core/src/zr_vm_core/call/call_binding_contract_internal.h::zr_core_call_binding_check_contract`。 

限制：Standalone64 contract plus per-container location; actual writer/reader bridge named。

### F018 ZRAF6 / SZrArtifactCallBindingRow

Owner: Core artifact grammar；size/stride=`96`；repeat=`[*]`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`schemaVersion@0:4`; `functionIndex@4:4`; `cacheIndex@8:4`; `instructionIndex@12:4`; `contract.bindingKind@16:4`; `contract.targetMetadataToken@20:4`; `contract.signatureToken@24:4`; `contract.ownerTypeToken@28:4`; `contract.signatureHash@32:8`; `contract.moduleSignatureHash@40:8`; `contract.layoutVersion@48:4`; `contract.dispatchSlot@52:4`; `contract.layoutHash@56:8`; `contract.operation@64:4`; `contract.reserved0@68:4`; `contract.reserved1@72:8`; `location.kind@80:4`; `location.targetIndex@84:4`; `location.ownerDepth@88:4`; `location.flags@92:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/artifact_call_binding.c::ZrCore_Artifact_WriteCallBindingRow`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_call_binding.c::ZrCore_Artifact_ReadCallBindingRow`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 

### F019 ERI1 / header

Owner: Core artifact grammar；size/stride=`48`；repeat=`None`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`magic@0:4`; `schemaVersion@4:2`; `flags@6:2`; `abiVersion@8:2`; `reserved10@10:2`; `moduleHash@12:8`; `execIrHash@20:8`; `execBcHash@28:8`; `sectionCount@36:4`; `directoryOffset@40:4`; `totalSize@44:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/artifact_exec_ir.c::ZrCore_ArtifactExecIr_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_exec_ir.c::ZrCore_ArtifactExecIr_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_exec_ir.c::ZrCore_ArtifactExecIr_Read`。 

### F020 ERI1 / directory

Owner: Core artifact grammar；size/stride=`24`；repeat=`sectionCount`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`kind@0:4`; `flags@4:4`; `elementCount@8:4`; `elementSize@12:4`; `byteOffset@16:4`; `byteLength@20:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/artifact_exec_ir.c::ZrCore_ArtifactExecIr_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_exec_ir.c::ZrCore_ArtifactExecIr_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_exec_ir.c::ZrCore_ArtifactExecIr_Read`。 

### F021 ERI1 / relocation

Owner: Core artifact grammar；size/stride=`32`；repeat=`[*]`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`targetToken@0:4`; `targetKind@4:4`; `codeOffset@8:4`; `targetIndex@12:4`; `expectedHash@16:8`; `expectedContractHash@24:8`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/artifact_exec_ir.c::ZrCore_ArtifactExecIr_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_exec_ir.c::ZrCore_ArtifactExecIr_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_exec_ir.c::ZrCore_ArtifactExecIr_ValidateRelocations`。 

限制：writer borrows encoded relocation payload; resolver never publishes executable target。

### F022 EIS1 / header

Owner: Core artifact scalar codec；size/stride=`8`；repeat=`None`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`magic@0:4`; `version@4:2`; `totalSize@6:2`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::scalar_select_wire_format`。 

### F023 EIS1 / metadata

Owner: Core artifact scalar codec；size/stride=`168`；repeat=`None`；corrected_absolute_values_to_record_relative; record_base=payload_start + 8；hash=Hspan。

字段：`module.id@0:4`; `module.moduleToken@4:4`; `module.moduleHash@8:8`; `module.contract.schemaVersion@16:4`; `module.contract.abiVersion@20:4`; `module.contract.logicalVersion@24:4`; `module.contract.reserved0@28:4`; `module.contract.generation@32:8`; `module.contract.targetToken@40:4`; `module.contract.reserved1@44:4`; `module.contract.signatureHash@48:8`; `module.contract.layoutHash@56:8`; `module.contract.moduleHash@64:8`; `module.contract.requiredCapabilities@72:4`; `module.contract.declaredEffects@76:4`; `function.id@80:4`; `function.functionToken@84:4`; `function.signatureHash@88:8`; `function.entryBlockId@96:4`; `function.sealed@100:4`; `function.contract.schemaVersion@104:4`; `function.contract.abiVersion@108:4`; `function.contract.logicalVersion@112:4`; `function.contract.reserved0@116:4`; `function.contract.generation@120:8`; `function.contract.targetToken@128:4`; `function.contract.reserved1@132:4`; `function.contract.signatureHash@136:8`; `function.contract.layoutHash@144:8`; `function.contract.moduleHash@152:8`; `function.contract.requiredCapabilities@160:4`; `function.contract.declaredEffects@164:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::scalar_select_wire_format`。 

### F024 EIS1 / constant

Owner: Core artifact scalar codec；size/stride=`16`；repeat=`1`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`typeToken@0:4`; `flags@4:4`; `bits@8:8`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::scalar_select_wire_format`。 

### F025 EIS1 / value

Owner: Core artifact scalar codec；size/stride=`24`；repeat=`1`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`id@0:4`; `definition@4:4`; `typeToken@8:4`; `ownership@12:4`; `nullability@16:4`; `flags@20:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::scalar_select_wire_format`。 

### F026 EIS1 / block

Owner: Core artifact scalar codec；size/stride=`20`；repeat=`blockCount or fixed profile count`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`id@0:4`; `flags@4:4`; `instructions.start@8:4`; `instructions.count@12:4`; `terminatorInstructionId@16:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::scalar_select_wire_format`。 

### F027 EIS1 / instruction

Owner: Core artifact scalar codec；size/stride=`84`；repeat=`instructionCount or fixed profile count`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`opcode@0:2`; `flags@2:2`; `results.start@4:4`; `results.count@8:4`; `operands.start@12:4`; `operands.count@16:4`; `phiRange.start@20:4`; `phiRange.count@24:4`; `successorRange.start@28:4`; `successorRange.count@32:4`; `typeToken@36:4`; `matchTypeToken@40:4`; `layoutId@44:4`; `memoryIn.start@48:4`; `memoryIn.count@52:4`; `memoryOut.start@56:4`; `memoryOut.count@60:4`; `effectIn@64:4`; `effectOut@68:4`; `sourceId@72:4`; `deoptId@76:4`; `bindingRow@80:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::scalar_select_wire_format`。 

### F028 EIS1 / results

Owner: Core artifact scalar codec；size/stride=`4`；repeat=`results count`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`id@0:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::scalar_select_wire_format`。 

### F029 EIS1 / operands

Owner: Core artifact scalar codec；size/stride=`4`；repeat=`operands count`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`id@0:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::scalar_select_wire_format`。 

### F030 EIS2 / header

Owner: Core artifact scalar codec；size/stride=`8`；repeat=`None`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`magic@0:4`; `version@4:2`; `totalSize@6:2`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::scalar_select_wire_format`。 

### F031 EIS2 / metadata

Owner: Core artifact scalar codec；size/stride=`168`；repeat=`None`；corrected_absolute_values_to_record_relative; record_base=payload_start + 8；hash=Hspan。

字段：`module.id@0:4`; `module.moduleToken@4:4`; `module.moduleHash@8:8`; `module.contract.schemaVersion@16:4`; `module.contract.abiVersion@20:4`; `module.contract.logicalVersion@24:4`; `module.contract.reserved0@28:4`; `module.contract.generation@32:8`; `module.contract.targetToken@40:4`; `module.contract.reserved1@44:4`; `module.contract.signatureHash@48:8`; `module.contract.layoutHash@56:8`; `module.contract.moduleHash@64:8`; `module.contract.requiredCapabilities@72:4`; `module.contract.declaredEffects@76:4`; `function.id@80:4`; `function.functionToken@84:4`; `function.signatureHash@88:8`; `function.entryBlockId@96:4`; `function.sealed@100:4`; `function.contract.schemaVersion@104:4`; `function.contract.abiVersion@108:4`; `function.contract.logicalVersion@112:4`; `function.contract.reserved0@116:4`; `function.contract.generation@120:8`; `function.contract.targetToken@128:4`; `function.contract.reserved1@132:4`; `function.contract.signatureHash@136:8`; `function.contract.layoutHash@144:8`; `function.contract.moduleHash@152:8`; `function.contract.requiredCapabilities@160:4`; `function.contract.declaredEffects@164:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::scalar_select_wire_format`。 

### F032 EIS2 / constant

Owner: Core artifact scalar codec；size/stride=`16`；repeat=`1`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`typeToken@0:4`; `flags@4:4`; `bits@8:8`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::scalar_select_wire_format`。 

### F033 EIS2 / value

Owner: Core artifact scalar codec；size/stride=`24`；repeat=`1`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`id@0:4`; `definition@4:4`; `typeToken@8:4`; `ownership@12:4`; `nullability@16:4`; `flags@20:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::scalar_select_wire_format`。 

### F034 EIS2 / block

Owner: Core artifact scalar codec；size/stride=`40`；repeat=`blockCount or fixed profile count`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`id@0:4`; `flags@4:4`; `instructions.start@8:4`; `instructions.count@12:4`; `predecessors.start@16:4`; `predecessors.count@20:4`; `successors.start@24:4`; `successors.count@28:4`; `immediateDominator@32:4`; `terminatorInstructionId@36:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::scalar_select_wire_format`。 

### F035 EIS2 / instruction

Owner: Core artifact scalar codec；size/stride=`84`；repeat=`instructionCount or fixed profile count`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`opcode@0:2`; `flags@2:2`; `results.start@4:4`; `results.count@8:4`; `operands.start@12:4`; `operands.count@16:4`; `phiRange.start@20:4`; `phiRange.count@24:4`; `successorRange.start@28:4`; `successorRange.count@32:4`; `typeToken@36:4`; `matchTypeToken@40:4`; `layoutId@44:4`; `memoryIn.start@48:4`; `memoryIn.count@52:4`; `memoryOut.start@56:4`; `memoryOut.count@60:4`; `effectIn@64:4`; `effectOut@68:4`; `sourceId@72:4`; `deoptId@76:4`; `bindingRow@80:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::scalar_select_wire_format`。 

### F036 EIS2 / results

Owner: Core artifact scalar codec；size/stride=`4`；repeat=`results count`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`id@0:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::scalar_select_wire_format`。 

### F037 EIS2 / operands

Owner: Core artifact scalar codec；size/stride=`4`；repeat=`operands count`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`id@0:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::scalar_select_wire_format`。 

### F038 EIS2 / successors

Owner: Core artifact scalar codec；size/stride=`4`；repeat=`successors count`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`id@0:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::scalar_select_wire_format`。 

### F039 EIS2 / predecessors

Owner: Core artifact scalar codec；size/stride=`4`；repeat=`predecessors count`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`id@0:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::scalar_select_wire_format`。 

### F040 EIS3 / header

Owner: Core artifact scalar codec；size/stride=`12`；repeat=`None`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`magic@0:4`; `version@4:2`; `reserved@6:2`; `totalSize@8:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c::ZrCore_ArtifactExecIrScalarEis3_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c::ZrCore_ArtifactExecIrScalarEis3_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c::ZrCore_ArtifactExecIrScalarEis3_GetEncodedSize`。 

### F041 EIS3 / metadata

Owner: Core artifact scalar codec；size/stride=`168`；repeat=`None`；corrected_absolute_values_to_record_relative; record_base=payload_start + 12；hash=Hspan。

字段：`module.id@0:4`; `module.moduleToken@4:4`; `module.moduleHash@8:8`; `module.contract.schemaVersion@16:4`; `module.contract.abiVersion@20:4`; `module.contract.logicalVersion@24:4`; `module.contract.reserved0@28:4`; `module.contract.generation@32:8`; `module.contract.targetToken@40:4`; `module.contract.reserved1@44:4`; `module.contract.signatureHash@48:8`; `module.contract.layoutHash@56:8`; `module.contract.moduleHash@64:8`; `module.contract.requiredCapabilities@72:4`; `module.contract.declaredEffects@76:4`; `function.id@80:4`; `function.functionToken@84:4`; `function.signatureHash@88:8`; `function.entryBlockId@96:4`; `function.sealed@100:4`; `function.contract.schemaVersion@104:4`; `function.contract.abiVersion@108:4`; `function.contract.logicalVersion@112:4`; `function.contract.reserved0@116:4`; `function.contract.generation@120:8`; `function.contract.targetToken@128:4`; `function.contract.reserved1@132:4`; `function.contract.signatureHash@136:8`; `function.contract.layoutHash@144:8`; `function.contract.moduleHash@152:8`; `function.contract.requiredCapabilities@160:4`; `function.contract.declaredEffects@164:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c::ZrCore_ArtifactExecIrScalarEis3_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c::ZrCore_ArtifactExecIrScalarEis3_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c::ZrCore_ArtifactExecIrScalarEis3_GetEncodedSize`。 

### F042 EIS3 / counts

Owner: Core artifact scalar codec；size/stride=`32`；repeat=`None`；corrected_absolute_values_to_record_relative; record_base=payload_start + 180；hash=Hspan。

字段：`constantCount@0:4`; `valueCount@4:4`; `blockCount@8:4`; `instructionCount@12:4`; `resultCount@16:4`; `operandCount@20:4`; `successorCount@24:4`; `predecessorCount@28:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c::ZrCore_ArtifactExecIrScalarEis3_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c::ZrCore_ArtifactExecIrScalarEis3_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c::ZrCore_ArtifactExecIrScalarEis3_GetEncodedSize`。 

### F043 EIS3 / constant

Owner: Core artifact scalar codec；size/stride=`16`；repeat=`constantCount`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`typeToken@0:4`; `flags@4:4`; `bits@8:8`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c::ZrCore_ArtifactExecIrScalarEis3_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c::ZrCore_ArtifactExecIrScalarEis3_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c::ZrCore_ArtifactExecIrScalarEis3_GetEncodedSize`。 

### F044 EIS3 / value

Owner: Core artifact scalar codec；size/stride=`24`；repeat=`valueCount`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`id@0:4`; `definition@4:4`; `typeToken@8:4`; `ownership@12:4`; `nullability@16:4`; `flags@20:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c::ZrCore_ArtifactExecIrScalarEis3_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c::ZrCore_ArtifactExecIrScalarEis3_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c::ZrCore_ArtifactExecIrScalarEis3_GetEncodedSize`。 

### F045 EIS3 / block

Owner: Core artifact scalar codec；size/stride=`40`；repeat=`blockCount or fixed profile count`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`id@0:4`; `flags@4:4`; `instructions.start@8:4`; `instructions.count@12:4`; `predecessors.start@16:4`; `predecessors.count@20:4`; `successors.start@24:4`; `successors.count@28:4`; `immediateDominator@32:4`; `terminatorInstructionId@36:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c::ZrCore_ArtifactExecIrScalarEis3_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c::ZrCore_ArtifactExecIrScalarEis3_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c::ZrCore_ArtifactExecIrScalarEis3_GetEncodedSize`。 

### F046 EIS3 / instruction

Owner: Core artifact scalar codec；size/stride=`84`；repeat=`instructionCount or fixed profile count`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`opcode@0:2`; `flags@2:2`; `results.start@4:4`; `results.count@8:4`; `operands.start@12:4`; `operands.count@16:4`; `phiRange.start@20:4`; `phiRange.count@24:4`; `successorRange.start@28:4`; `successorRange.count@32:4`; `typeToken@36:4`; `matchTypeToken@40:4`; `layoutId@44:4`; `memoryIn.start@48:4`; `memoryIn.count@52:4`; `memoryOut.start@56:4`; `memoryOut.count@60:4`; `effectIn@64:4`; `effectOut@68:4`; `sourceId@72:4`; `deoptId@76:4`; `bindingRow@80:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c::ZrCore_ArtifactExecIrScalarEis3_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c::ZrCore_ArtifactExecIrScalarEis3_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c::ZrCore_ArtifactExecIrScalarEis3_GetEncodedSize`。 

### F047 EIS3 / results

Owner: Core artifact scalar codec；size/stride=`4`；repeat=`results count`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`id@0:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c::ZrCore_ArtifactExecIrScalarEis3_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c::ZrCore_ArtifactExecIrScalarEis3_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c::ZrCore_ArtifactExecIrScalarEis3_GetEncodedSize`。 

### F048 EIS3 / operands

Owner: Core artifact scalar codec；size/stride=`4`；repeat=`operands count`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`id@0:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c::ZrCore_ArtifactExecIrScalarEis3_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c::ZrCore_ArtifactExecIrScalarEis3_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c::ZrCore_ArtifactExecIrScalarEis3_GetEncodedSize`。 

### F049 EIS3 / successors

Owner: Core artifact scalar codec；size/stride=`4`；repeat=`successors count`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`id@0:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c::ZrCore_ArtifactExecIrScalarEis3_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c::ZrCore_ArtifactExecIrScalarEis3_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c::ZrCore_ArtifactExecIrScalarEis3_GetEncodedSize`。 

### F050 EIS3 / predecessors

Owner: Core artifact scalar codec；size/stride=`4`；repeat=`predecessors count`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`id@0:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c::ZrCore_ArtifactExecIrScalarEis3_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c::ZrCore_ArtifactExecIrScalarEis3_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c::ZrCore_ArtifactExecIrScalarEis3_GetEncodedSize`。 

### F051 EIS4 / header

Owner: Core artifact scalar codec；size/stride=`12`；repeat=`None`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`magic@0:4`; `version@4:2`; `reserved@6:2`; `totalSize@8:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c::ZrCore_ArtifactExecIrScalarEis4_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c::ZrCore_ArtifactExecIrScalarEis4_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c::ZrCore_ArtifactExecIrScalarEis4_GetEncodedSize`。 

### F052 EIS4 / metadata

Owner: Core artifact scalar codec；size/stride=`168`；repeat=`None`；corrected_absolute_values_to_record_relative; record_base=payload_start + 12；hash=Hspan。

字段：`module.id@0:4`; `module.moduleToken@4:4`; `module.moduleHash@8:8`; `module.contract.schemaVersion@16:4`; `module.contract.abiVersion@20:4`; `module.contract.logicalVersion@24:4`; `module.contract.reserved0@28:4`; `module.contract.generation@32:8`; `module.contract.targetToken@40:4`; `module.contract.reserved1@44:4`; `module.contract.signatureHash@48:8`; `module.contract.layoutHash@56:8`; `module.contract.moduleHash@64:8`; `module.contract.requiredCapabilities@72:4`; `module.contract.declaredEffects@76:4`; `function.id@80:4`; `function.functionToken@84:4`; `function.signatureHash@88:8`; `function.entryBlockId@96:4`; `function.sealed@100:4`; `function.contract.schemaVersion@104:4`; `function.contract.abiVersion@108:4`; `function.contract.logicalVersion@112:4`; `function.contract.reserved0@116:4`; `function.contract.generation@120:8`; `function.contract.targetToken@128:4`; `function.contract.reserved1@132:4`; `function.contract.signatureHash@136:8`; `function.contract.layoutHash@144:8`; `function.contract.moduleHash@152:8`; `function.contract.requiredCapabilities@160:4`; `function.contract.declaredEffects@164:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c::ZrCore_ArtifactExecIrScalarEis4_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c::ZrCore_ArtifactExecIrScalarEis4_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c::ZrCore_ArtifactExecIrScalarEis4_GetEncodedSize`。 

### F053 EIS4 / counts

Owner: Core artifact scalar codec；size/stride=`32`；repeat=`None`；corrected_absolute_values_to_record_relative; record_base=payload_start + 180；hash=Hspan。

字段：`constantCount@0:4`; `valueCount@4:4`; `blockCount@8:4`; `instructionCount@12:4`; `resultCount@16:4`; `operandCount@20:4`; `successorCount@24:4`; `predecessorCount@28:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c::ZrCore_ArtifactExecIrScalarEis4_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c::ZrCore_ArtifactExecIrScalarEis4_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c::ZrCore_ArtifactExecIrScalarEis4_GetEncodedSize`。 

### F054 EIS4 / constant

Owner: Core artifact scalar codec；size/stride=`16`；repeat=`constantCount`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`typeToken@0:4`; `flags@4:4`; `bits@8:8`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c::ZrCore_ArtifactExecIrScalarEis4_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c::ZrCore_ArtifactExecIrScalarEis4_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c::ZrCore_ArtifactExecIrScalarEis4_GetEncodedSize`。 

### F055 EIS4 / value

Owner: Core artifact scalar codec；size/stride=`24`；repeat=`valueCount`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`id@0:4`; `definition@4:4`; `typeToken@8:4`; `ownership@12:4`; `nullability@16:4`; `flags@20:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c::ZrCore_ArtifactExecIrScalarEis4_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c::ZrCore_ArtifactExecIrScalarEis4_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c::ZrCore_ArtifactExecIrScalarEis4_GetEncodedSize`。 

### F056 EIS4 / block

Owner: Core artifact scalar codec；size/stride=`40`；repeat=`blockCount or fixed profile count`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`id@0:4`; `flags@4:4`; `instructions.start@8:4`; `instructions.count@12:4`; `predecessors.start@16:4`; `predecessors.count@20:4`; `successors.start@24:4`; `successors.count@28:4`; `immediateDominator@32:4`; `terminatorInstructionId@36:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c::ZrCore_ArtifactExecIrScalarEis4_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c::ZrCore_ArtifactExecIrScalarEis4_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c::ZrCore_ArtifactExecIrScalarEis4_GetEncodedSize`。 

### F057 EIS4 / instruction

Owner: Core artifact scalar codec；size/stride=`84`；repeat=`instructionCount or fixed profile count`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`opcode@0:2`; `flags@2:2`; `results.start@4:4`; `results.count@8:4`; `operands.start@12:4`; `operands.count@16:4`; `phiRange.start@20:4`; `phiRange.count@24:4`; `successorRange.start@28:4`; `successorRange.count@32:4`; `typeToken@36:4`; `matchTypeToken@40:4`; `layoutId@44:4`; `memoryIn.start@48:4`; `memoryIn.count@52:4`; `memoryOut.start@56:4`; `memoryOut.count@60:4`; `effectIn@64:4`; `effectOut@68:4`; `sourceId@72:4`; `deoptId@76:4`; `bindingRow@80:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c::ZrCore_ArtifactExecIrScalarEis4_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c::ZrCore_ArtifactExecIrScalarEis4_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c::ZrCore_ArtifactExecIrScalarEis4_GetEncodedSize`。 

### F058 EIS4 / results

Owner: Core artifact scalar codec；size/stride=`4`；repeat=`results count`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`id@0:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c::ZrCore_ArtifactExecIrScalarEis4_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c::ZrCore_ArtifactExecIrScalarEis4_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c::ZrCore_ArtifactExecIrScalarEis4_GetEncodedSize`。 

### F059 EIS4 / operands

Owner: Core artifact scalar codec；size/stride=`4`；repeat=`operands count`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`id@0:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c::ZrCore_ArtifactExecIrScalarEis4_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c::ZrCore_ArtifactExecIrScalarEis4_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c::ZrCore_ArtifactExecIrScalarEis4_GetEncodedSize`。 

### F060 EIS4 / successors

Owner: Core artifact scalar codec；size/stride=`4`；repeat=`successors count`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`id@0:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c::ZrCore_ArtifactExecIrScalarEis4_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c::ZrCore_ArtifactExecIrScalarEis4_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c::ZrCore_ArtifactExecIrScalarEis4_GetEncodedSize`。 

### F061 EIS4 / predecessors

Owner: Core artifact scalar codec；size/stride=`4`；repeat=`predecessors count`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`id@0:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c::ZrCore_ArtifactExecIrScalarEis4_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c::ZrCore_ArtifactExecIrScalarEis4_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c::ZrCore_ArtifactExecIrScalarEis4_GetEncodedSize`。 

### F062 EIS5 / header

Owner: Core artifact scalar codec；size/stride=`44`；repeat=`None`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`magic@0:4`; `version@4:2`; `reserved@6:2`; `totalSize@8:4`; `constantCount@12:4`; `valueCount@16:4`; `blockCount@20:4`; `instructionCount@24:4`; `operandCount@28:4`; `resultCount@32:4`; `successorCount@36:4`; `predecessorCount@40:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5_write.c::ZrCore_ArtifactExecIrScalarEis5_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5_read.c::ZrCore_ArtifactExecIrScalarEis5_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5.c::ZrCore_ArtifactExecIrScalarEis5_ValidateModule`。 

### F063 EIS5 / metadata

Owner: Core artifact scalar codec；size/stride=`168`；repeat=`None`；corrected_absolute_values_to_record_relative; record_base=payload_start + 44；hash=Hspan。

字段：`module.id@0:4`; `module.moduleToken@4:4`; `module.moduleHash@8:8`; `module.contract.schemaVersion@16:4`; `module.contract.abiVersion@20:4`; `module.contract.logicalVersion@24:4`; `module.contract.reserved0@28:4`; `module.contract.generation@32:8`; `module.contract.targetToken@40:4`; `module.contract.reserved1@44:4`; `module.contract.signatureHash@48:8`; `module.contract.layoutHash@56:8`; `module.contract.moduleHash@64:8`; `module.contract.requiredCapabilities@72:4`; `module.contract.declaredEffects@76:4`; `function.id@80:4`; `function.functionToken@84:4`; `function.signatureHash@88:8`; `function.entryBlockId@96:4`; `function.sealed@100:4`; `function.contract.schemaVersion@104:4`; `function.contract.abiVersion@108:4`; `function.contract.logicalVersion@112:4`; `function.contract.reserved0@116:4`; `function.contract.generation@120:8`; `function.contract.targetToken@128:4`; `function.contract.reserved1@132:4`; `function.contract.signatureHash@136:8`; `function.contract.layoutHash@144:8`; `function.contract.moduleHash@152:8`; `function.contract.requiredCapabilities@160:4`; `function.contract.declaredEffects@164:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5_write.c::ZrCore_ArtifactExecIrScalarEis5_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5_read.c::ZrCore_ArtifactExecIrScalarEis5_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5.c::ZrCore_ArtifactExecIrScalarEis5_ValidateModule`。 

### F064 EIS5 / constant

Owner: Core artifact scalar codec；size/stride=`16`；repeat=`constantCount`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`typeToken@0:4`; `flags@4:4`; `bits@8:8`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5_write.c::ZrCore_ArtifactExecIrScalarEis5_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5_read.c::ZrCore_ArtifactExecIrScalarEis5_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5.c::ZrCore_ArtifactExecIrScalarEis5_ValidateModule`。 

### F065 EIS5 / value

Owner: Core artifact scalar codec；size/stride=`24`；repeat=`valueCount`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`id@0:4`; `definition@4:4`; `typeToken@8:4`; `ownership@12:4`; `nullability@16:4`; `flags@20:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5_write.c::ZrCore_ArtifactExecIrScalarEis5_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5_read.c::ZrCore_ArtifactExecIrScalarEis5_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5.c::ZrCore_ArtifactExecIrScalarEis5_ValidateModule`。 

### F066 EIS5 / block

Owner: Core artifact scalar codec；size/stride=`40`；repeat=`blockCount or fixed profile count`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`id@0:4`; `flags@4:4`; `instructions.start@8:4`; `instructions.count@12:4`; `predecessors.start@16:4`; `predecessors.count@20:4`; `successors.start@24:4`; `successors.count@28:4`; `immediateDominator@32:4`; `terminatorInstructionId@36:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5_write.c::ZrCore_ArtifactExecIrScalarEis5_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5_read.c::ZrCore_ArtifactExecIrScalarEis5_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5.c::ZrCore_ArtifactExecIrScalarEis5_ValidateModule`。 

### F067 EIS5 / instruction

Owner: Core artifact scalar codec；size/stride=`84`；repeat=`instructionCount or fixed profile count`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`opcode@0:2`; `flags@2:2`; `results.start@4:4`; `results.count@8:4`; `operands.start@12:4`; `operands.count@16:4`; `phiRange.start@20:4`; `phiRange.count@24:4`; `successorRange.start@28:4`; `successorRange.count@32:4`; `typeToken@36:4`; `matchTypeToken@40:4`; `layoutId@44:4`; `memoryIn.start@48:4`; `memoryIn.count@52:4`; `memoryOut.start@56:4`; `memoryOut.count@60:4`; `effectIn@64:4`; `effectOut@68:4`; `sourceId@72:4`; `deoptId@76:4`; `bindingRow@80:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5_write.c::ZrCore_ArtifactExecIrScalarEis5_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5_read.c::ZrCore_ArtifactExecIrScalarEis5_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5.c::ZrCore_ArtifactExecIrScalarEis5_ValidateModule`。 

### F068 EIS5 / results

Owner: Core artifact scalar codec；size/stride=`4`；repeat=`results count`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`id@0:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5_write.c::ZrCore_ArtifactExecIrScalarEis5_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5_read.c::ZrCore_ArtifactExecIrScalarEis5_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5.c::ZrCore_ArtifactExecIrScalarEis5_ValidateModule`。 

### F069 EIS5 / operands

Owner: Core artifact scalar codec；size/stride=`4`；repeat=`operands count`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`id@0:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5_write.c::ZrCore_ArtifactExecIrScalarEis5_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5_read.c::ZrCore_ArtifactExecIrScalarEis5_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5.c::ZrCore_ArtifactExecIrScalarEis5_ValidateModule`。 

### F070 EIS5 / successors

Owner: Core artifact scalar codec；size/stride=`4`；repeat=`successors count`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`id@0:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5_write.c::ZrCore_ArtifactExecIrScalarEis5_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5_read.c::ZrCore_ArtifactExecIrScalarEis5_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5.c::ZrCore_ArtifactExecIrScalarEis5_ValidateModule`。 

### F071 EIS5 / predecessors

Owner: Core artifact scalar codec；size/stride=`4`；repeat=`predecessors count`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`id@0:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5_write.c::ZrCore_ArtifactExecIrScalarEis5_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5_read.c::ZrCore_ArtifactExecIrScalarEis5_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5.c::ZrCore_ArtifactExecIrScalarEis5_ValidateModule`。 

### F072 EIS6 / header

Owner: Core artifact scalar codec；size/stride=`56`；repeat=`None`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`magic@0:4`; `version@4:2`; `reserved@6:2`; `totalSize@8:4`; `constantCount@12:4`; `valueCount@16:4`; `blockCount@20:4`; `instructionCount@24:4`; `operandCount@28:4`; `resultCount@32:4`; `successorCount@36:4`; `predecessorCount@40:4`; `memoryTokenCount@44:4`; `bindingRowsSchemaVersion@48:4`; `bindingRowCount@52:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6_write.c::ZrCore_ArtifactExecIrScalarEis6_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6_read.c::ZrCore_ArtifactExecIrScalarEis6_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6.c::ZrCore_ArtifactExecIrScalarEis6_ValidateModule`。 

### F073 EIS6 / metadata

Owner: Core artifact scalar codec；size/stride=`168`；repeat=`None`；corrected_absolute_values_to_record_relative; record_base=payload_start + 56；hash=Hspan。

字段：`module.id@0:4`; `module.moduleToken@4:4`; `module.moduleHash@8:8`; `module.contract.schemaVersion@16:4`; `module.contract.abiVersion@20:4`; `module.contract.logicalVersion@24:4`; `module.contract.reserved0@28:4`; `module.contract.generation@32:8`; `module.contract.targetToken@40:4`; `module.contract.reserved1@44:4`; `module.contract.signatureHash@48:8`; `module.contract.layoutHash@56:8`; `module.contract.moduleHash@64:8`; `module.contract.requiredCapabilities@72:4`; `module.contract.declaredEffects@76:4`; `function.id@80:4`; `function.functionToken@84:4`; `function.signatureHash@88:8`; `function.entryBlockId@96:4`; `function.sealed@100:4`; `function.contract.schemaVersion@104:4`; `function.contract.abiVersion@108:4`; `function.contract.logicalVersion@112:4`; `function.contract.reserved0@116:4`; `function.contract.generation@120:8`; `function.contract.targetToken@128:4`; `function.contract.reserved1@132:4`; `function.contract.signatureHash@136:8`; `function.contract.layoutHash@144:8`; `function.contract.moduleHash@152:8`; `function.contract.requiredCapabilities@160:4`; `function.contract.declaredEffects@164:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6_write.c::ZrCore_ArtifactExecIrScalarEis6_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6_read.c::ZrCore_ArtifactExecIrScalarEis6_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6.c::ZrCore_ArtifactExecIrScalarEis6_ValidateModule`。 

### F074 EIS6 / constant

Owner: Core artifact scalar codec；size/stride=`16`；repeat=`constantCount`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`typeToken@0:4`; `flags@4:4`; `bits@8:8`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6_write.c::ZrCore_ArtifactExecIrScalarEis6_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6_read.c::ZrCore_ArtifactExecIrScalarEis6_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6.c::ZrCore_ArtifactExecIrScalarEis6_ValidateModule`。 

### F075 EIS6 / value

Owner: Core artifact scalar codec；size/stride=`24`；repeat=`valueCount`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`id@0:4`; `definition@4:4`; `typeToken@8:4`; `ownership@12:4`; `nullability@16:4`; `flags@20:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6_write.c::ZrCore_ArtifactExecIrScalarEis6_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6_read.c::ZrCore_ArtifactExecIrScalarEis6_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6.c::ZrCore_ArtifactExecIrScalarEis6_ValidateModule`。 

### F076 EIS6 / block

Owner: Core artifact scalar codec；size/stride=`40`；repeat=`blockCount or fixed profile count`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`id@0:4`; `flags@4:4`; `instructions.start@8:4`; `instructions.count@12:4`; `predecessors.start@16:4`; `predecessors.count@20:4`; `successors.start@24:4`; `successors.count@28:4`; `immediateDominator@32:4`; `terminatorInstructionId@36:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6_write.c::ZrCore_ArtifactExecIrScalarEis6_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6_read.c::ZrCore_ArtifactExecIrScalarEis6_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6.c::ZrCore_ArtifactExecIrScalarEis6_ValidateModule`。 

### F077 EIS6 / instruction

Owner: Core artifact scalar codec；size/stride=`84`；repeat=`instructionCount or fixed profile count`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`opcode@0:2`; `flags@2:2`; `results.start@4:4`; `results.count@8:4`; `operands.start@12:4`; `operands.count@16:4`; `phiRange.start@20:4`; `phiRange.count@24:4`; `successorRange.start@28:4`; `successorRange.count@32:4`; `typeToken@36:4`; `matchTypeToken@40:4`; `layoutId@44:4`; `memoryIn.start@48:4`; `memoryIn.count@52:4`; `memoryOut.start@56:4`; `memoryOut.count@60:4`; `effectIn@64:4`; `effectOut@68:4`; `sourceId@72:4`; `deoptId@76:4`; `bindingRow@80:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6_write.c::ZrCore_ArtifactExecIrScalarEis6_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6_read.c::ZrCore_ArtifactExecIrScalarEis6_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6.c::ZrCore_ArtifactExecIrScalarEis6_ValidateModule`。 

### F078 EIS6 / results

Owner: Core artifact scalar codec；size/stride=`4`；repeat=`results count`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`id@0:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6_write.c::ZrCore_ArtifactExecIrScalarEis6_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6_read.c::ZrCore_ArtifactExecIrScalarEis6_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6.c::ZrCore_ArtifactExecIrScalarEis6_ValidateModule`。 

### F079 EIS6 / operands

Owner: Core artifact scalar codec；size/stride=`4`；repeat=`operands count`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`id@0:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6_write.c::ZrCore_ArtifactExecIrScalarEis6_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6_read.c::ZrCore_ArtifactExecIrScalarEis6_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6.c::ZrCore_ArtifactExecIrScalarEis6_ValidateModule`。 

### F080 EIS6 / successors

Owner: Core artifact scalar codec；size/stride=`4`；repeat=`successors count`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`id@0:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6_write.c::ZrCore_ArtifactExecIrScalarEis6_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6_read.c::ZrCore_ArtifactExecIrScalarEis6_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6.c::ZrCore_ArtifactExecIrScalarEis6_ValidateModule`。 

### F081 EIS6 / predecessors

Owner: Core artifact scalar codec；size/stride=`4`；repeat=`predecessors count`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`id@0:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6_write.c::ZrCore_ArtifactExecIrScalarEis6_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6_read.c::ZrCore_ArtifactExecIrScalarEis6_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6.c::ZrCore_ArtifactExecIrScalarEis6_ValidateModule`。 

### F082 EIS6 / memoryTokenPool

Owner: Core artifact scalar codec；size/stride=`4`；repeat=`memoryTokenPool count`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`id@0:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6_write.c::ZrCore_ArtifactExecIrScalarEis6_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6_read.c::ZrCore_ArtifactExecIrScalarEis6_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6.c::ZrCore_ArtifactExecIrScalarEis6_ValidateModule`。 

### F083 EIS6 / bindingRow

Owner: Core artifact scalar codec；size/stride=`96`；repeat=`bindingRowCount`；retain_record_relative; record_base=该 record 本次编码起点；hash=Hspan。

字段：`rowIndex@0:4`; `instructionId@4:4`; `segmentIndex@8:4`; `contract.bindingKind@12:4`; `contract.targetMetadataToken@16:4`; `contract.signatureToken@20:4`; `contract.ownerTypeToken@24:4`; `contract.signatureHash@28:8`; `contract.moduleSignatureHash@36:8`; `contract.layoutVersion@44:4`; `contract.dispatchSlot@48:4`; `contract.layoutHash@52:8`; `contract.operation@60:4`; `contract.reserved0@64:4`; `contract.reserved1@68:8`; `location.kind@76:4`; `location.targetIndex@80:4`; `location.ownerDepth@84:4`; `location.flags@88:4`; `sourceId@92:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6_write.c::ZrCore_ArtifactExecIrScalarEis6_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6_read.c::ZrCore_ArtifactExecIrScalarEis6_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6.c::ZrCore_ArtifactExecIrScalarEis6_ValidateModule`。 

### F084 Execution6_17_1 / contract_semantics

Owner: Core runtime execution contract；size/stride=`64`；repeat=`None`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`schemaVersion@0:4`; `abiVersion@4:4`; `logicalVersion@8:4`; `reserved0@12:4`; `generation@16:8`; `targetToken@24:4`; `reserved1@28:4`; `signatureHash@32:8`; `layoutHash@40:8`; `moduleHash@48:8`; `requiredCapabilities@56:4`; `declaredEffects@60:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c::ZrCore_ExecIr_ModuleAddFunction`。 Reader: `zr_vm_core/src/zr_vm_core/runtime/execution_contract.c::ZrCore_ExecutionContract_Check`。 Admission: `zr_vm_core/src/zr_vm_core/runtime/execution_contract.c::ZrCore_ExecutionContract_Check`。 

限制：offsets here are canonical64 scalar codec layout only, never sizeof/memcpy contract; consumer policies differ。

### F085 ZRAF6.signature / node.PRIMITIVE

Owner: Core artifact grammar；size/stride=`5`；repeat=`None`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`tag@0:1`; `valueType@1:4`。

Producer/encoder: `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::ZrParser_ArtifactType_WriteSignature`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::ZrCore_Artifact_ValidateSignature`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::ZrCore_Artifact_ValidateSignature`。 

限制：recursive node offsets become variable after node terms; actual projection aliases require review。

### F086 ZRAF6.signature / node.TYPE_DEF

Owner: Core artifact grammar；size/stride=`5`；repeat=`None`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`tag@0:1`; `token@1:4`。

Producer/encoder: `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::ZrParser_ArtifactType_WriteSignature`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::ZrCore_Artifact_ValidateSignature`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::ZrCore_Artifact_ValidateSignature`。 

限制：recursive node offsets become variable after node terms; actual projection aliases require review。

### F087 ZRAF6.signature / node.GENERIC_PARAMETER

Owner: Core artifact grammar；size/stride=`9`；repeat=`None`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`tag@0:1`; `owner@1:4`; `index@5:4`。

Producer/encoder: `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::ZrParser_ArtifactType_WriteSignature`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::ZrCore_Artifact_ValidateSignature`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::ZrCore_Artifact_ValidateSignature`。 

限制：recursive node offsets become variable after node terms; actual projection aliases require review。

### F088 ZRAF6.signature / node.CONST_PARAMETER

Owner: Core artifact grammar；size/stride=`9`；repeat=`None`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`tag@0:1`; `owner@1:4`; `index@5:4`。

Producer/encoder: `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::ZrParser_ArtifactType_WriteSignature`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::ZrCore_Artifact_ValidateSignature`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::ZrCore_Artifact_ValidateSignature`。 

限制：recursive node offsets become variable after node terms; actual projection aliases require review。

### F089 ZRAF6.signature / node.GENERIC_INSTANCE

Owner: Core artifact grammar；size/stride=`None`；repeat=`None`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`tag@0:1`; `definitionNode@1:node`; `argumentCount@None:4`; `arguments[*]@None:node`。

Producer/encoder: `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::ZrParser_ArtifactType_WriteSignature`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::ZrCore_Artifact_ValidateSignature`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::ZrCore_Artifact_ValidateSignature`。 

限制：recursive node offsets become variable after node terms; actual projection aliases require review。

### F090 ZRAF6.signature / node.ARRAY

Owner: Core artifact grammar；size/stride=`None`；repeat=`None`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`tag@0:1`; `rank@1:4`; `qualifier@5:1`; `element@6:node`。

Producer/encoder: `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::ZrParser_ArtifactType_WriteSignature`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::ZrCore_Artifact_ValidateSignature`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::ZrCore_Artifact_ValidateSignature`。 

限制：recursive node offsets become variable after node terms; actual projection aliases require review。

### F091 ZRAF6.signature / node.TUPLE

Owner: Core artifact grammar；size/stride=`None`；repeat=`None`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`tag@0:1`; `elementCount@1:4`; `elements[*]@5:node`。

Producer/encoder: `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::ZrParser_ArtifactType_WriteSignature`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::ZrCore_Artifact_ValidateSignature`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::ZrCore_Artifact_ValidateSignature`。 

限制：recursive node offsets become variable after node terms; actual projection aliases require review。

### F092 ZRAF6.signature / node.UNION

Owner: Core artifact grammar；size/stride=`None`；repeat=`None`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`tag@0:1`; `tagNode@1:node`; `memberCount@None:4`; `members[*]@None:node`。

Producer/encoder: `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::ZrParser_ArtifactType_WriteSignature`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::ZrCore_Artifact_ValidateSignature`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::ZrCore_Artifact_ValidateSignature`。 

限制：recursive node offsets become variable after node terms; actual projection aliases require review。

### F093 ZRAF6.signature / node.NULLABLE

Owner: Core artifact grammar；size/stride=`None`；repeat=`None`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`tag@0:1`; `target@1:node`。

Producer/encoder: `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::ZrParser_ArtifactType_WriteSignature`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::ZrCore_Artifact_ValidateSignature`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::ZrCore_Artifact_ValidateSignature`。 

限制：recursive node offsets become variable after node terms; actual projection aliases require review。

### F094 ZRAF6.signature / node.READONLY_VIEW

Owner: Core artifact grammar；size/stride=`None`；repeat=`None`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`tag@0:1`; `target@1:node`。

Producer/encoder: `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::ZrParser_ArtifactType_WriteSignature`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::ZrCore_Artifact_ValidateSignature`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::ZrCore_Artifact_ValidateSignature`。 

限制：recursive node offsets become variable after node terms; actual projection aliases require review。

### F095 ZRAF6.signature / node.REF

Owner: Core artifact grammar；size/stride=`None`；repeat=`None`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`tag@0:1`; `access@1:1`; `target@2:node`。

Producer/encoder: `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::ZrParser_ArtifactType_WriteSignature`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::ZrCore_Artifact_ValidateSignature`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::ZrCore_Artifact_ValidateSignature`。 

限制：recursive node offsets become variable after node terms; actual projection aliases require review。

### F096 ZRAF6.signature / node.OWNER

Owner: Core artifact grammar；size/stride=`None`；repeat=`None`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`tag@0:1`; `ownership@1:1`; `target@2:node`。

Producer/encoder: `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::ZrParser_ArtifactType_WriteSignature`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::ZrCore_Artifact_ValidateSignature`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::ZrCore_Artifact_ValidateSignature`。 

限制：recursive node offsets become variable after node terms; actual projection aliases require review。

### F097 ZRAF6.signature / node.NEVER

Owner: Core artifact grammar；size/stride=`1`；repeat=`None`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`tag@0:1`。

Producer/encoder: `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::ZrParser_ArtifactType_WriteSignature`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::ZrCore_Artifact_ValidateSignature`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::ZrCore_Artifact_ValidateSignature`。 

限制：recursive node offsets become variable after node terms; actual projection aliases require review。

### F098 ZRAF6.signature / node.ERROR

Owner: Core artifact grammar；size/stride=`1`；repeat=`None`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`tag@0:1`。

Producer/encoder: `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::ZrParser_ArtifactType_WriteSignature`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::ZrCore_Artifact_ValidateSignature`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::ZrCore_Artifact_ValidateSignature`。 

限制：recursive node offsets become variable after node terms; actual projection aliases require review。

### F099 ZRAF6.signature / node.CONST_INT

Owner: Core artifact grammar；size/stride=`9`；repeat=`None`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`tag@0:1`; `bits@1:8`。

Producer/encoder: `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::ZrParser_ArtifactType_WriteSignature`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::ZrCore_Artifact_ValidateSignature`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::ZrCore_Artifact_ValidateSignature`。 

限制：recursive node offsets become variable after node terms; actual projection aliases require review。

### F100 ZRAF6.signature / node.FUNCTION

Owner: Core artifact grammar；size/stride=`None`；repeat=`None`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`tag@0:1`; `receiver@1:1`; `refExport@2:1`; `effectFlags@3:1`; `parameterCount@4:4`; `returnNode@8:node`; `parameters[*]@None:parameter`。

Producer/encoder: `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::ZrParser_ArtifactType_WriteSignature`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::ZrCore_Artifact_ValidateSignature`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::ZrCore_Artifact_ValidateSignature`。 

限制：recursive node offsets become variable after node terms; actual projection aliases require review。

### F101 ZRAF6.signature / function.parameter

Owner: Core artifact grammar；size/stride=`None`；repeat=`parameterCount`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`passing@0:1`; `escape@1:1`; `entryInitialization@2:1`; `exitInitialization@3:1`; `acceptsTemporary@4:1`; `callSiteMarker@5:1`; `type@6:node`。

Producer/encoder: `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::ZrParser_ArtifactType_WriteSignature`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::ZrCore_Artifact_ValidateSignature`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::ZrCore_Artifact_ValidateSignature`。 

### F102 ZRAF6.layout_map1 / header

Owner: Core artifact grammar；size/stride=`16`；repeat=`None`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`version@0:4`; `gcCount@4:4`; `ownershipCount@8:4`; `refCount@12:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema_metadata_graph.c::artifact_metadata_validate_layout_map`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema_metadata_graph.c::artifact_metadata_validate_layout_map`。 

### F103 ZRAF6.layout_map1 / gcOffsets

Owner: Core artifact grammar；size/stride=`4`；repeat=`gcOffsetsCount`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`byteOffset@0:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema_metadata_graph.c::artifact_metadata_validate_layout_map`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema_metadata_graph.c::artifact_metadata_validate_layout_map`。 

### F104 ZRAF6.layout_map1 / ownershipOffsets

Owner: Core artifact grammar；size/stride=`4`；repeat=`ownershipOffsetsCount`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`byteOffset@0:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema_metadata_graph.c::artifact_metadata_validate_layout_map`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema_metadata_graph.c::artifact_metadata_validate_layout_map`。 

### F105 ZRAF6.layout_map1 / refOffsets

Owner: Core artifact grammar；size/stride=`4`；repeat=`refOffsetsCount`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`byteOffset@0:4`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema_metadata_graph.c::artifact_metadata_validate_layout_map`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema_metadata_graph.c::artifact_metadata_validate_layout_map`。 

### F106 ZRAF6 / SZrArtifactTypeIdentityRow.TYPE_SPEC

Owner: Core artifact grammar；size/stride=`48`；repeat=`[*]`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`token@0:4`; `signatureToken@4:4`; `canonicalTypeId@8:4`; `flags@12:4`; `signatureOffset@16:4`; `signatureLength@20:4`; `signatureHash@24:8`; `layoutVersion@32:4`; `reserved0@36:4`; `layoutHash@40:8`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::artifact_write_type_identity_row`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::ZrCore_Artifact_ReadTypeIdentityRow`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 

### F107 CBND1 / header

Owner: Core call-binding codec；size/stride=`16`；repeat=`None`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`magic@0:4`; `schemaVersion@4:4`; `rowSize@8:4`; `rowCount@12:4`。

Producer/encoder: `zr_vm_parser/src/zr_vm_parser/writer/writer_call_binding.c::ZrParser_Writer_WriteCallBindings`。 Reader: `zr_vm_core/src/zr_vm_core/io/io_call_binding.c::ZrCore_Io_ReadCallBindings`。 Admission: `zr_vm_core/src/zr_vm_core/io/io_call_binding.c::ZrCore_Io_ReadCallBindings`。 

### F108 CBND1 / row

Owner: Core call-binding codec；size/stride=`84`；repeat=`rowCount`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`cacheIndex@0:4`; `contract.bindingKind@4:4`; `contract.targetMetadataToken@8:4`; `contract.signatureToken@12:4`; `contract.ownerTypeToken@16:4`; `contract.signatureHash@20:8`; `contract.moduleSignatureHash@28:8`; `contract.layoutVersion@36:4`; `contract.dispatchSlot@40:4`; `contract.layoutHash@44:8`; `contract.operation@52:4`; `contract.reserved0@56:4`; `contract.reserved1@60:8`; `location.kind@68:4`; `location.targetIndex@72:4`; `location.ownerDepth@76:4`; `location.flags@80:4`。

Producer/encoder: `zr_vm_parser/src/zr_vm_parser/writer/writer_call_binding.c::ZrParser_Writer_WriteCallBindings`。 Reader: `zr_vm_core/src/zr_vm_core/io/io_call_binding.c::ZrCore_Io_ReadCallBindings`。 Admission: `zr_vm_core/src/zr_vm_core/io/io_call_binding.c::ZrCore_Io_ReadCallBindings`。 

限制：84-byte LE row. Emits non-NONE only. Reader clears cache contracts/locations then decodes incrementally; late failure can leave cleared/partial state. Range/duplicates/contract and typed-vs-location policy checked; no transactional output promise.。

### F109 ZRAF6 / STRING_HEAP.raw_payload

Owner: Core artifact grammar；size/stride=`1`；repeat=`byteLength`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`bytes@0:1`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 

限制：Outer byte window only; does not claim complete inner grammar. Signature/layout/EIS have separately enumerated profiles.。

### F110 ZRAF6 / SIGNATURE_HEAP.raw_payload

Owner: Core artifact grammar；size/stride=`1`；repeat=`byteLength`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`bytes@0:1`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 

限制：Outer byte window only; does not claim complete inner grammar. Signature/layout/EIS have separately enumerated profiles.。

### F111 ZRAF6 / CODE_TABLE.raw_payload

Owner: Core artifact grammar；size/stride=`1`；repeat=`byteLength`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`bytes@0:1`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 

限制：Outer byte window only; does not claim complete inner grammar. Signature/layout/EIS have separately enumerated profiles.。

### F112 ZRAF6 / DEBUG_MAP.raw_payload

Owner: Core artifact grammar；size/stride=`1`；repeat=`byteLength`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`bytes@0:1`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 

限制：Outer byte window only; does not claim complete inner grammar. Signature/layout/EIS have separately enumerated profiles.。

### F113 ZRAF6 / SYNTAX_TREE.raw_payload

Owner: Core artifact grammar；size/stride=`1`；repeat=`byteLength`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`bytes@0:1`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 

限制：Outer byte window only; does not claim complete inner grammar. Signature/layout/EIS have separately enumerated profiles.。

### F114 ZRAF6 / SEMANTIC_IR.raw_payload

Owner: Core artifact grammar；size/stride=`1`；repeat=`byteLength`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`bytes@0:1`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 

限制：Outer byte window only; does not claim complete inner grammar. Signature/layout/EIS have separately enumerated profiles.。

### F115 ZRAF6 / METADATA_BLOB_HEAP.raw_payload

Owner: Core artifact grammar；size/stride=`1`；repeat=`byteLength`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`bytes@0:1`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 

限制：Outer byte window only; does not claim complete inner grammar. Signature/layout/EIS have separately enumerated profiles.。

### F116 ZRAF6 / LAYOUT_MAP_HEAP.raw_payload

Owner: Core artifact grammar；size/stride=`1`；repeat=`byteLength`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`bytes@0:1`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 

限制：Outer byte window only; does not claim complete inner grammar. Signature/layout/EIS have separately enumerated profiles.。

### F117 ZRAF6 / EXEC_IR_BUNDLE.raw_payload

Owner: Core artifact grammar；size/stride=`1`；repeat=`byteLength`；retain_record_relative; record_base=该 record 本次编码起点；hash=H0。

字段：`bytes@0:1`。

Producer/encoder: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Write`。 Reader: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 Admission: `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read`。 

限制：Outer byte window only; does not claim complete inner grammar. Signature/layout/EIS have separately enumerated profiles.。


## 旧 IO 全 117 source grammar records

L key 使用 file::function，与 F 的 semantic record 不同。每个 event 显示 event-order、field 或 nested callee、width、源码位置和 active_control；这就是字段路径组的可复核坐标。reader 与 writer 的分支/site 不要求字面一一对应。Core IO 是该历史 native format 的唯一 grammar owner，Parser writer 是 producer；Core reader 是 admission consumer。hash 默认 H0：记录中 hash 只表示 supplied scalar，不能由 decode 推断重算；13 条 hash-owner links 在 sealed legacy.hash_owner_links，实际根由兼容页列出。

### L001 write_string_with_length

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_string_with_length`；writer；events=3；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&strLength; (sizeof(TZrSize)) * (1); line38; if:str == ZR_NULL`; `1:&strLength; (sizeof(TZrSize)) * (1); line45; unconditional`; `2:strStr; (sizeof(TZrChar)) * (strLength); line49; if:strLength > 0 & if:strStr != ZR_NULL`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L002 write_io_reference

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_io_reference`；writer；events=3；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&moduleNameLength; (sizeof(TZrSize)) * (1); line77; unconditional`; `1:&md5Length; (sizeof(TZrSize)) * (1); line81; unconditional`; `2:&referenceIndex; (sizeof(TZrSize)) * (1); line85; unconditional`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L003 write_prototype_class

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_prototype_class`；writer；events=5；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:arguments; encoded_size(write_string_with_length(state,file,className)); line92; unconditional`; `1:&superClassLength; (sizeof(TZrSize)) * (1); line96; unconditional`; `2:arguments; encoded_size(write_io_reference(state,file,inheritIndices[i],function)); line102; if:superClassLength > 0 & for:TZrUInt32 i = 0; i < superClassLength; i++`; `3:&genericLength; (sizeof(TZrSize)) * (1); line108; unconditional`; `4:&declaresLength; (sizeof(TZrSize)) * (1); line115; unconditional`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L004 write_prototype_struct

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_prototype_struct`；writer；events=5；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:arguments; encoded_size(write_string_with_length(state,file,structName)); line124; unconditional`; `1:&superStructLength; (sizeof(TZrSize)) * (1); line128; unconditional`; `2:arguments; encoded_size(write_io_reference(state,file,inheritIndices[i],function)); line134; if:superStructLength > 0 & for:TZrUInt32 i = 0; i < superStructLength; i++`; `3:&genericLength; (sizeof(TZrSize)) * (1); line140; unconditional`; `4:&declaresLength; (sizeof(TZrSize)) * (1); line146; unconditional`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L005 write_native_string_with_length

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_native_string_with_length`；writer；events=2；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&length; (sizeof(TZrSize)) * (1); line213; unconditional`; `1:text; (sizeof(TZrChar)) * (length); line215; if:length > 0`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L006 write_function_debug_infos

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_debug_infos`；writer；events=9；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&debugInfoLength; (sizeof(TZrUInt64)) * (1); line257; unconditional`; `1:arguments; encoded_size(write_native_string_with_length(file,sourceId)); line268; unconditional`; `2:arguments; encoded_size(write_native_string_with_length(file,sourceHash)); line269; unconditional`; `3:&instructionsLength; (sizeof(TZrUInt64)) * (1); line272; unconditional`; `4:&line; (sizeof(TZrUInt64)) * (1); line276; for:TZrUInt32 index = 0; index < function->instructionsLength; index++`; `5:&location.lineInSource; (sizeof(TZrUInt32)) * (1); line281; for:TZrUInt32 index = 0; index < function->instructionsLength; index++`; `6:&location.columnInSourceStart; (sizeof(TZrUInt32)) * (1); line282; for:TZrUInt32 index = 0; index < function->instructionsLength; index++`; `7:&location.lineInSourceEnd; (sizeof(TZrUInt32)) * (1); line283; for:TZrUInt32 index = 0; index < function->instructionsLength; index++`; `8:&location.columnInSourceEnd; (sizeof(TZrUInt32)) * (1); line284; for:TZrUInt32 index = 0; index < function->instructionsLength; index++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L007 write_function_name

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_name`；writer；events=3；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:arguments; encoded_size(write_string_with_length(state,file,function->functionName)); line333; if:function != ZR_NULL && function->functionName != ZR_NULL`; `1:arguments; encoded_size(write_string_with_length(state,file,fallbackName)); line339; if:defaultName != ZR_NULL`; `2:arguments; encoded_size(write_string_with_length(state,file,ZR_NULL)); line343; unconditional`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L008 write_function_local_variables

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_local_variables`；writer；events=9；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&localLength; (sizeof(TZrUInt64)) * (1); line348; unconditional`; `1:arguments; encoded_size(write_string_with_length(ZR_NULL,file,local->name)); line357; for:TZrUInt64 i = 0; i < localLength; i++`; `2:&stackSlot; (sizeof(TZrUInt32)) * (1); line358; for:TZrUInt64 i = 0; i < localLength; i++`; `3:&instructionStartValue; (sizeof(TZrUInt64)) * (1); line399; for:TZrUInt64 i = 0; i < localLength; i++`; `4:&instructionEndValue; (sizeof(TZrUInt64)) * (1); line400; for:TZrUInt64 i = 0; i < localLength; i++`; `5:&startLineLocal; (sizeof(TZrUInt64)) * (1); line402; for:TZrUInt64 i = 0; i < localLength; i++`; `6:&endLineLocal; (sizeof(TZrUInt64)) * (1); line403; for:TZrUInt64 i = 0; i < localLength; i++`; `7:&local->scopeDepth; (sizeof(TZrUInt32)) * (1); line404; for:TZrUInt64 i = 0; i < localLength; i++`; `8:&local->escapeFlags; (sizeof(TZrUInt32)) * (1); line405; for:TZrUInt64 i = 0; i < localLength; i++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L009 write_function_closure_variables

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_closure_variables`；writer；events=7；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&closureLength; (sizeof(TZrUInt64)) * (1); line411; unconditional`; `1:arguments; encoded_size(write_string_with_length(state,file,closure->name)); line419; for:TZrUInt64 i = 0; i < closureLength; i++`; `2:&inStack; (sizeof(TZrUInt8)) * (1); line420; for:TZrUInt64 i = 0; i < closureLength; i++`; `3:&index; (sizeof(TZrUInt32)) * (1); line421; for:TZrUInt64 i = 0; i < closureLength; i++`; `4:&valueType; (sizeof(TZrUInt32)) * (1); line422; for:TZrUInt64 i = 0; i < closureLength; i++`; `5:&closure->scopeDepth; (sizeof(TZrUInt32)) * (1); line423; for:TZrUInt64 i = 0; i < closureLength; i++`; `6:&closure->escapeFlags; (sizeof(TZrUInt32)) * (1); line424; for:TZrUInt64 i = 0; i < closureLength; i++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L010 write_function_exception_metadata

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_exception_metadata`；writer；events=10；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&catchClauseCount; (sizeof(TZrUInt64)) * (1); line432; unconditional`; `1:arguments; encoded_size(write_string_with_length(state,file,clause->typeName)); line436; for:TZrUInt64 i = 0; i < catchClauseCount; i++`; `2:&targetInstructionOffset; (sizeof(TZrUInt64)) * (1); line437; for:TZrUInt64 i = 0; i < catchClauseCount; i++`; `3:&exceptionHandlerCount; (sizeof(TZrUInt64)) * (1); line440; unconditional`; `4:&protectedStartInstructionOffset; (sizeof(TZrUInt64)) * (1); line448; for:TZrUInt64 i = 0; i < exceptionHandlerCount; i++`; `5:&finallyTargetInstructionOffset; (sizeof(TZrUInt64)) * (1); line449; for:TZrUInt64 i = 0; i < exceptionHandlerCount; i++`; `6:&afterFinallyInstructionOffset; (sizeof(TZrUInt64)) * (1); line450; for:TZrUInt64 i = 0; i < exceptionHandlerCount; i++`; `7:&handler->catchClauseStartIndex; (sizeof(TZrUInt32)) * (1); line451; for:TZrUInt64 i = 0; i < exceptionHandlerCount; i++`; `8:&handler->catchClauseCount; (sizeof(TZrUInt32)) * (1); line452; for:TZrUInt64 i = 0; i < exceptionHandlerCount; i++`; `9:&hasFinally; (sizeof(TZrUInt8)) * (1); line453; for:TZrUInt64 i = 0; i < exceptionHandlerCount; i++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L011 write_function_frame_layout

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_frame_layout`；writer；events=11；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&frameByteSize; (sizeof(TZrUInt32)) * (1); line463; unconditional`; `1:&frameByteAlign; (sizeof(TZrUInt32)) * (1); line464; unconditional`; `2:&frameSlotLayoutLength; (sizeof(TZrUInt64)) * (1); line465; unconditional`; `3:&layout->stackSlot; (sizeof(TZrUInt32)) * (1); line471; for:TZrUInt64 index = 0; index < frameSlotLayoutLength; index++`; `4:&layout->byteOffset; (sizeof(TZrUInt32)) * (1); line472; for:TZrUInt64 index = 0; index < frameSlotLayoutLength; index++`; `5:&layout->byteSize; (sizeof(TZrUInt32)) * (1); line473; for:TZrUInt64 index = 0; index < frameSlotLayoutLength; index++`; `6:&layout->byteAlign; (sizeof(TZrUInt32)) * (1); line474; for:TZrUInt64 index = 0; index < frameSlotLayoutLength; index++`; `7:&layout->typeLayoutId; (sizeof(TZrUInt32)) * (1); line475; for:TZrUInt64 index = 0; index < frameSlotLayoutLength; index++`; `8:&layout->slotKind; (sizeof(TZrUInt8)) * (1); line476; for:TZrUInt64 index = 0; index < frameSlotLayoutLength; index++`; `9:&layout->isParameter; (sizeof(TZrUInt8)) * (1); line477; for:TZrUInt64 index = 0; index < frameSlotLayoutLength; index++`; `10:&serializedFlags; (sizeof(TZrUInt16)) * (1); line478; for:TZrUInt64 index = 0; index < frameSlotLayoutLength; index++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L012 write_object_constant_payload

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_object_constant_payload`；writer；events=3；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&entryCount; (sizeof(TZrUInt64)) * (1); line610; unconditional`; `1:arguments; encoded_size(write_embedded_value(file,state,&pairs[index]->key)); line636; for:TZrSize index = 0; index < pairIndex; index++`; `2:arguments; encoded_size(write_embedded_value(file,state,&pairs[index]->value)); line637; for:TZrSize index = 0; index < pairIndex; index++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L013 write_array_constant_payload

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_array_constant_payload`；writer；events=2；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&elementCount; (sizeof(TZrUInt64)) * (1); line664; unconditional`; `1:arguments; encoded_size(write_embedded_value(file,state,elementValue)); line677; for:TZrUInt64 index = 0; index < elementCount; index++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L014 write_value_payload

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_value_payload`；writer；events=9；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&boolValue; (sizeof(TZrUInt8)) * (1); line703; switch:value->type`; `1:&value->value.nativeObject.nativeInt64; (sizeof(TZrInt64)) * (1); line710; switch:value->type`; `2:&value->value.nativeObject.nativeUInt64; (sizeof(TZrUInt64)) * (1); line716; switch:value->type`; `3:&value->value.nativeObject.nativeDouble; (sizeof(TZrDouble)) * (1); line720; switch:value->type`; `4:arguments; encoded_size(write_string_with_length(state,file,str)); line730; switch:value->type`; `5:arguments; encoded_size(write_object_constant_payload(file,state,value)); line734; switch:value->type`; `6:arguments; encoded_size(write_array_constant_payload(file,state,value)); line736; switch:value->type`; `7:&hasFunctionValue; (sizeof(TZrBool)) * (1); line766; switch:value->type`; `8:arguments; encoded_size(write_io_function_internal(state,file,functionValue,ZR_NULL,ZR_NULL)); line768; switch:value->type & if:hasFunctionValue`。

switch/case labels：`[{"label":"case ZR_VALUE_TYPE_NULL","line":699},{"label":"case ZR_VALUE_TYPE_BOOL","line":701},{"label":"case ZR_VALUE_TYPE_INT8","line":706},{"label":"case ZR_VALUE_TYPE_INT16","line":707},{"label":"case ZR_VALUE_TYPE_INT32","line":708},{"label":"case ZR_VALUE_TYPE_INT64","line":709},{"label":"case ZR_VALUE_TYPE_UINT8","line":712},{"label":"case ZR_VALUE_TYPE_UINT16","line":713},{"label":"case ZR_VALUE_TYPE_UINT32","line":714},{"label":"case ZR_VALUE_TYPE_UINT64","line":715},{"label":"case ZR_VALUE_TYPE_FLOAT","line":718},{"label":"case ZR_VALUE_TYPE_DOUBLE","line":719},{"label":"case ZR_VALUE_TYPE_STRING","line":722},{"label":"case ZR_VALUE_TYPE_OBJECT","line":733},{"label":"case ZR_VALUE_TYPE_ARRAY","line":735},{"label":"case ZR_VALUE_TYPE_FUNCTION","line":737},{"label":"case ZR_VALUE_TYPE_CLOSURE","line":738},{"label":"case ZR_VALUE_TYPE_NATIVE_POINTER","line":777},{"label":"default","line":788}]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L015 write_embedded_value

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_embedded_value`；writer；events=2；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&type; (sizeof(TZrUInt32)) * (1); line803; unconditional`; `1:arguments; encoded_size(write_value_payload(file,state,value,ZR_FALSE,ZR_NULL)); line804; unconditional`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L016 write_function_constant

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_constant`；writer；events=4；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&type; (sizeof(TZrUInt32)) * (1); line817; unconditional`; `1:arguments; encoded_size(write_value_payload(file,state,constant,ZR_TRUE,&helperId)); line818; unconditional`; `2:&helperId; (sizeof(TZrUInt64)) * (1); line822; unconditional`; `3:&endLineConst; (sizeof(TZrUInt64)) * (1); line823; unconditional`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L017 write_function_typed_type_ref

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_typed_type_ref`；writer；events=9；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&baseType; (sizeof(TZrUInt32)) * (1); line848; unconditional`; `1:&isNullable; (sizeof(TZrUInt8)) * (1); line849; unconditional`; `2:&ownershipQualifier; (sizeof(TZrUInt32)) * (1); line850; unconditional`; `3:&isArray; (sizeof(TZrUInt8)) * (1); line851; unconditional`; `4:arguments; encoded_size(write_string_with_length(state,file,typeRef != ZR_NULL ? typeRef->typeName : ZR_NULL)); line852; unconditional`; `5:&elementBaseType; (sizeof(TZrUInt32)) * (1); line853; unconditional`; `6:arguments; encoded_size(write_string_with_length(state,file,typeRef != ZR_NULL ? typeRef->elementTypeName : ZR_NULL)); line854; unconditional`; `7:&staticCType; (sizeof(TZrUInt32)) * (1); line855; unconditional`; `8:&staticCTypeId; (sizeof(TZrUInt32)) * (1); line856; unconditional`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L018 write_function_typed_local_bindings

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_typed_local_bindings`；writer；events=12；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&typedLocalCount; (sizeof(TZrUInt64)) * (1); line862; unconditional`; `1:arguments; encoded_size(write_string_with_length(state,file,binding->name)); line865; for:TZrUInt64 index = 0; index < typedLocalCount; index++`; `2:&binding->stackSlot; (sizeof(TZrUInt32)) * (1); line866; for:TZrUInt64 index = 0; index < typedLocalCount; index++`; `3:arguments; encoded_size(write_function_typed_type_ref(file,state,&binding->type)); line867; for:TZrUInt64 index = 0; index < typedLocalCount; index++`; `4:&binding->symbolId; (sizeof(TZrUInt32)) * (1); line868; for:TZrUInt64 index = 0; index < typedLocalCount; index++`; `5:&binding->typeId; (sizeof(TZrUInt32)) * (1); line869; for:TZrUInt64 index = 0; index < typedLocalCount; index++`; `6:&binding->placeId; (sizeof(TZrUInt32)) * (1); line870; for:TZrUInt64 index = 0; index < typedLocalCount; index++`; `7:&binding->declarationStartLine; (sizeof(TZrUInt32)) * (1); line871; for:TZrUInt64 index = 0; index < typedLocalCount; index++`; `8:&binding->declarationStartColumn; (sizeof(TZrUInt32)) * (1); line872; for:TZrUInt64 index = 0; index < typedLocalCount; index++`; `9:&binding->declarationEndLine; (sizeof(TZrUInt32)) * (1); line873; for:TZrUInt64 index = 0; index < typedLocalCount; index++`; `10:&binding->declarationEndColumn; (sizeof(TZrUInt32)) * (1); line874; for:TZrUInt64 index = 0; index < typedLocalCount; index++`; `11:&binding->roleFlags; (sizeof(TZrUInt32)) * (1); line875; for:TZrUInt64 index = 0; index < typedLocalCount; index++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L019 write_function_typed_closure_bindings

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_typed_closure_bindings`；writer；events=9；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&typedClosureCount; (sizeof(TZrUInt64)) * (1); line882; unconditional`; `1:&binding->captureIndex; (sizeof(TZrUInt32)) * (1); line886; for:TZrUInt64 index = 0; index < typedClosureCount; index++`; `2:arguments; encoded_size(write_function_typed_type_ref(file,state,&binding->type)); line887; for:TZrUInt64 index = 0; index < typedClosureCount; index++`; `3:&binding->symbolId; (sizeof(TZrUInt32)) * (1); line888; for:TZrUInt64 index = 0; index < typedClosureCount; index++`; `4:&binding->typeId; (sizeof(TZrUInt32)) * (1); line889; for:TZrUInt64 index = 0; index < typedClosureCount; index++`; `5:&binding->declarationStartLine; (sizeof(TZrUInt32)) * (1); line890; for:TZrUInt64 index = 0; index < typedClosureCount; index++`; `6:&binding->declarationStartColumn; (sizeof(TZrUInt32)) * (1); line891; for:TZrUInt64 index = 0; index < typedClosureCount; index++`; `7:&binding->declarationEndLine; (sizeof(TZrUInt32)) * (1); line892; for:TZrUInt64 index = 0; index < typedClosureCount; index++`; `8:&binding->declarationEndColumn; (sizeof(TZrUInt32)) * (1); line893; for:TZrUInt64 index = 0; index < typedClosureCount; index++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L020 write_function_typed_export_generic_parameter

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_typed_export_generic_parameter`；writer；events=11；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:arguments; encoded_size(write_string_with_length(state,file,parameter != ZR_NULL ? parameter->name : ZR_NULL)); line915; unconditional`; `1:&genericKind; (sizeof(TZrUInt8)) * (1); line916; unconditional`; `2:&variance; (sizeof(TZrUInt8)) * (1); line917; unconditional`; `3:&requiresClass; (sizeof(TZrUInt8)) * (1); line918; unconditional`; `4:&requiresStruct; (sizeof(TZrUInt8)) * (1); line919; unconditional`; `5:&requiresNew; (sizeof(TZrUInt8)) * (1); line920; unconditional`; `6:&requiresOwner; (sizeof(TZrUInt8)) * (1); line921; unconditional`; `7:&reserved0; (sizeof(TZrUInt16)) * (1); line922; unconditional`; `8:&requiredOwnershipQualifier; (sizeof(TZrUInt32)) * (1); line923; unconditional`; `9:&constraintTypeCount; (sizeof(TZrUInt32)) * (1); line924; unconditional`; `10:arguments; encoded_size(write_string_with_length(state,file,parameter->constraintTypeNames[index])); line926; for:TZrUInt32 index = 0; index < constraintTypeCount; index++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L021 write_function_typed_export_symbols

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_typed_export_symbols`；writer；events=23；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&symbolCount; (sizeof(TZrUInt64)) * (1); line933; unconditional`; `1:arguments; encoded_size(write_string_with_length(state,file,symbol->name)); line940; for:TZrUInt64 index = 0; index < symbolCount; index++`; `2:&symbol->stackSlot; (sizeof(TZrUInt32)) * (1); line941; for:TZrUInt64 index = 0; index < symbolCount; index++`; `3:&symbol->accessModifier; (sizeof(TZrUInt8)) * (1); line942; for:TZrUInt64 index = 0; index < symbolCount; index++`; `4:&symbol->symbolKind; (sizeof(TZrUInt8)) * (1); line943; for:TZrUInt64 index = 0; index < symbolCount; index++`; `5:&symbol->exportKind; (sizeof(TZrUInt8)) * (1); line944; for:TZrUInt64 index = 0; index < symbolCount; index++`; `6:&symbol->readiness; (sizeof(TZrUInt8)) * (1); line945; for:TZrUInt64 index = 0; index < symbolCount; index++`; `7:&symbol->reserved0; (sizeof(TZrUInt16)) * (1); line946; for:TZrUInt64 index = 0; index < symbolCount; index++`; `8:&symbol->callableChildIndex; (sizeof(TZrUInt32)) * (1); line947; for:TZrUInt64 index = 0; index < symbolCount; index++`; `9:arguments; encoded_size(write_function_typed_type_ref(file,state,&symbol->valueType)); line948; for:TZrUInt64 index = 0; index < symbolCount; index++`; `10:&parameterCount; (sizeof(TZrUInt64)) * (1); line949; for:TZrUInt64 index = 0; index < symbolCount; index++`; `11:arguments; encoded_size(write_function_typed_type_ref(file,state,parameterType)); line953; for:TZrUInt64 index = 0; index < symbolCount; index++ & for:TZrUInt64 paramIndex = 0; paramIndex < parameterCount; paramIndex++`; `12:&genericParameterCount; (sizeof(TZrUInt32)) * (1); line955; for:TZrUInt64 index = 0; index < symbolCount; index++`; `13:arguments; encoded_size(write_function_typed_export_generic_parameter(file,state,&symbol->genericParameters[genericIndex])); line957; for:TZrUInt64 index = 0; index < symbolCount; index++ & for:TZrUInt32 genericIndex = 0; genericIndex < genericParameterCount; genericIndex++`; `14:&symbol->lineInSourceStart; (sizeof(TZrUInt32)) * (1); line960; for:TZrUInt64 index = 0; index < symbolCount; index++`; `15:&symbol->columnInSourceStart; (sizeof(TZrUInt32)) * (1); line961; for:TZrUInt64 index = 0; index < symbolCount; index++`; `16:&symbol->lineInSourceEnd; (sizeof(TZrUInt32)) * (1); line962; for:TZrUInt64 index = 0; index < symbolCount; index++`; `17:&symbol->columnInSourceEnd; (sizeof(TZrUInt32)) * (1); line963; for:TZrUInt64 index = 0; index < symbolCount; index++`; `18:&symbol->metadataToken; (sizeof(TZrMetadataToken)) * (1); line964; for:TZrUInt64 index = 0; index < symbolCount; index++`; `19:&symbol->signatureToken; (sizeof(TZrMetadataToken)) * (1); line965; for:TZrUInt64 index = 0; index < symbolCount; index++`; `20:&symbol->signatureBlobOffset; (sizeof(TZrUInt32)) * (1); line966; for:TZrUInt64 index = 0; index < symbolCount; index++`; `21:&symbol->signatureBlobLength; (sizeof(TZrUInt32)) * (1); line967; for:TZrUInt64 index = 0; index < symbolCount; index++`; `22:&symbol->signatureHash; (sizeof(TZrUInt64)) * (1); line968; for:TZrUInt64 index = 0; index < symbolCount; index++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L022 write_metadata_token_records

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_metadata_token_records`；writer；events=18；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&recordCount; (sizeof(TZrUInt64)) * (1); line976; unconditional`; `1:&record->token; (sizeof(TZrMetadataToken)) * (1); line980; for:TZrUInt64 index = 0; index < recordCount; index++`; `2:&record->relatedToken; (sizeof(TZrMetadataToken)) * (1); line981; for:TZrUInt64 index = 0; index < recordCount; index++`; `3:&record->ownerToken; (sizeof(TZrMetadataToken)) * (1); line982; for:TZrUInt64 index = 0; index < recordCount; index++`; `4:&record->ownerIndex; (sizeof(TZrUInt32)) * (1); line983; for:TZrUInt64 index = 0; index < recordCount; index++`; `5:&record->signatureBlobOffset; (sizeof(TZrUInt32)) * (1); line984; for:TZrUInt64 index = 0; index < recordCount; index++`; `6:&record->signatureBlobLength; (sizeof(TZrUInt32)) * (1); line985; for:TZrUInt64 index = 0; index < recordCount; index++`; `7:&record->signatureHash; (sizeof(TZrUInt64)) * (1); line986; for:TZrUInt64 index = 0; index < recordCount; index++`; `8:&record->layoutVersion; (sizeof(TZrUInt32)) * (1); line987; for:TZrUInt64 index = 0; index < recordCount; index++`; `9:&record->reserved0; (sizeof(TZrUInt32)) * (1); line988; for:TZrUInt64 index = 0; index < recordCount; index++`; `10:&record->layoutHash; (sizeof(TZrUInt64)) * (1); line989; for:TZrUInt64 index = 0; index < recordCount; index++`; `11:&record->targetMetadataToken; (sizeof(TZrMetadataToken)) * (1); line990; for:TZrUInt64 index = 0; index < recordCount; index++`; `12:&record->targetSignatureToken; (sizeof(TZrMetadataToken)) * (1); line991; for:TZrUInt64 index = 0; index < recordCount; index++`; `13:&record->targetSignatureHash; (sizeof(TZrUInt64)) * (1); line992; for:TZrUInt64 index = 0; index < recordCount; index++`; `14:&record->targetModuleSignatureHash; (sizeof(TZrUInt64)) * (1); line993; for:TZrUInt64 index = 0; index < recordCount; index++`; `15:arguments; encoded_size(write_string_with_length(state,file,record->requestedModuleVersion)); line994; for:TZrUInt64 index = 0; index < recordCount; index++`; `16:arguments; encoded_size(write_string_with_length(state,file,record->minModuleVersionInclusive)); line995; for:TZrUInt64 index = 0; index < recordCount; index++`; `17:arguments; encoded_size(write_string_with_length(state,file,record->maxModuleVersionExclusive)); line996; for:TZrUInt64 index = 0; index < recordCount; index++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L023 write_module_metadata_bindings

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_module_metadata_bindings`；writer；events=18；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&bindingCount; (sizeof(TZrUInt64)) * (1); line1003; unconditional`; `1:&binding->refToken; (sizeof(TZrMetadataToken)) * (1); line1007; for:TZrUInt64 index = 0; index < bindingCount; index++`; `2:&binding->refSignatureToken; (sizeof(TZrMetadataToken)) * (1); line1008; for:TZrUInt64 index = 0; index < bindingCount; index++`; `3:&binding->refSignatureHash; (sizeof(TZrUInt64)) * (1); line1009; for:TZrUInt64 index = 0; index < bindingCount; index++`; `4:&binding->expectedMetadataToken; (sizeof(TZrMetadataToken)) * (1); line1010; for:TZrUInt64 index = 0; index < bindingCount; index++`; `5:&binding->expectedSignatureToken; (sizeof(TZrMetadataToken)) * (1); line1011; for:TZrUInt64 index = 0; index < bindingCount; index++`; `6:&binding->expectedSignatureHash; (sizeof(TZrUInt64)) * (1); line1012; for:TZrUInt64 index = 0; index < bindingCount; index++`; `7:&binding->expectedModuleSignatureHash; (sizeof(TZrUInt64)) * (1); line1013; for:TZrUInt64 index = 0; index < bindingCount; index++`; `8:&binding->expectedLayoutVersion; (sizeof(TZrUInt32)) * (1); line1014; for:TZrUInt64 index = 0; index < bindingCount; index++`; `9:&binding->reserved0; (sizeof(TZrUInt32)) * (1); line1015; for:TZrUInt64 index = 0; index < bindingCount; index++`; `10:&binding->expectedLayoutHash; (sizeof(TZrUInt64)) * (1); line1016; for:TZrUInt64 index = 0; index < bindingCount; index++`; `11:&binding->resolvedMetadataToken; (sizeof(TZrMetadataToken)) * (1); line1017; for:TZrUInt64 index = 0; index < bindingCount; index++`; `12:&binding->resolvedSignatureToken; (sizeof(TZrMetadataToken)) * (1); line1018; for:TZrUInt64 index = 0; index < bindingCount; index++`; `13:&binding->resolvedSignatureHash; (sizeof(TZrUInt64)) * (1); line1019; for:TZrUInt64 index = 0; index < bindingCount; index++`; `14:&binding->resolvedModuleSignatureHash; (sizeof(TZrUInt64)) * (1); line1020; for:TZrUInt64 index = 0; index < bindingCount; index++`; `15:&binding->resolvedLayoutVersion; (sizeof(TZrUInt32)) * (1); line1021; for:TZrUInt64 index = 0; index < bindingCount; index++`; `16:&binding->reserved1; (sizeof(TZrUInt32)) * (1); line1022; for:TZrUInt64 index = 0; index < bindingCount; index++`; `17:&binding->resolvedLayoutHash; (sizeof(TZrUInt64)) * (1); line1023; for:TZrUInt64 index = 0; index < bindingCount; index++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L024 write_metadata_string_heap

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_metadata_string_heap`；writer；events=3；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&entryCount; (sizeof(TZrUInt64)) * (1); line1031; unconditional`; `1:&entries[index].stringIndex; (sizeof(TZrUInt32)) * (1); line1033; for:TZrUInt64 index = 0; index < entryCount; index++`; `2:arguments; encoded_size(write_string_with_length(state,file,entries[index].value)); line1034; for:TZrUInt64 index = 0; index < entryCount; index++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L025 write_function_metadata_token_model

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_metadata_token_model`；writer；events=6；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:arguments; encoded_size(write_metadata_token_records(file,state,function != ZR_NULL ? function->metadataTokenRecords : ZR_NULL,recordCount)); line1057; unconditional`; `1:arguments; encoded_size(write_string_with_length(state,file,function != ZR_NULL ? function->moduleVersion : ZR_NULL)); line1058; unconditional`; `2:&heapLength; (sizeof(TZrUInt64)) * (1); line1060; unconditional`; `3:function->signatureBlobHeap; (sizeof(TZrByte)) * (heapLength); line1062; if:heapLength > 0 && function->signatureBlobHeap != ZR_NULL`; `4:arguments; encoded_size(write_metadata_string_heap(file,state,function != ZR_NULL ? function->metadataStringHeap : ZR_NULL,stringHeapLength)); line1064; unconditional`; `5:&moduleSignatureHash; (sizeof(TZrUInt64)) * (1); line1068; unconditional`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L026 write_function_module_metadata_ref_table

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_module_metadata_ref_table`；writer；events=1；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:arguments; encoded_size(write_metadata_token_records(file,state,function != ZR_NULL ? function->moduleMetadataTokenRecords : ZR_NULL,recordCount)); line1078; unconditional`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L027 write_function_module_metadata_bindings

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_module_metadata_bindings`；writer；events=1；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:arguments; encoded_size(write_module_metadata_bindings(file,function != ZR_NULL ? function->moduleMetadataBindings : ZR_NULL,bindingCount)); line1091; unconditional`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L028 write_function_module_effect

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_module_effect`；writer；events=18；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&kind; (sizeof(TZrUInt8)) * (1); line1133; unconditional`; `1:&exportKind; (sizeof(TZrUInt8)) * (1); line1134; unconditional`; `2:&readiness; (sizeof(TZrUInt8)) * (1); line1135; unconditional`; `3:&reserved0; (sizeof(TZrUInt8)) * (1); line1136; unconditional`; `4:arguments; encoded_size(write_string_with_length(state,file,effect != ZR_NULL ? effect->moduleName : ZR_NULL)); line1137; unconditional`; `5:arguments; encoded_size(write_string_with_length(state,file,assemblyName)); line1138; unconditional`; `6:arguments; encoded_size(write_string_with_length(state,file,effect != ZR_NULL ? effect->symbolName : ZR_NULL)); line1139; unconditional`; `7:&lineInSourceStart; (sizeof(TZrUInt32)) * (1); line1140; unconditional`; `8:&columnInSourceStart; (sizeof(TZrUInt32)) * (1); line1141; unconditional`; `9:&lineInSourceEnd; (sizeof(TZrUInt32)) * (1); line1142; unconditional`; `10:&columnInSourceEnd; (sizeof(TZrUInt32)) * (1); line1143; unconditional`; `11:&targetMetadataToken; (sizeof(TZrMetadataToken)) * (1); line1144; unconditional`; `12:&targetSignatureToken; (sizeof(TZrMetadataToken)) * (1); line1145; unconditional`; `13:&targetSignatureHash; (sizeof(TZrUInt64)) * (1); line1146; unconditional`; `14:&targetModuleSignatureHash; (sizeof(TZrUInt64)) * (1); line1147; unconditional`; `15:arguments; encoded_size(write_string_with_length(state,file,requestedModuleVersion)); line1148; unconditional`; `16:arguments; encoded_size(write_string_with_length(state,file,minModuleVersionInclusive)); line1149; unconditional`; `17:arguments; encoded_size(write_string_with_length(state,file,maxModuleVersionExclusive)); line1150; unconditional`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L029 write_function_static_imports

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_static_imports`；writer；events=2；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&importCount; (sizeof(TZrUInt64)) * (1); line1156; unconditional`; `1:arguments; encoded_size(write_string_with_length(state,file,function->staticImports[index])); line1158; for:TZrUInt64 index = 0; index < importCount; index++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L030 write_function_module_entry_effects

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_module_entry_effects`；writer；events=2；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&effectCount; (sizeof(TZrUInt64)) * (1); line1165; unconditional`; `1:arguments; encoded_size(write_function_module_effect(file,state,&function->moduleEntryEffects[index])); line1167; for:TZrUInt64 index = 0; index < effectCount; index++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L031 write_function_exported_callable_summaries

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_exported_callable_summaries`；writer；events=5；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&summaryCount; (sizeof(TZrUInt64)) * (1); line1174; unconditional`; `1:arguments; encoded_size(write_string_with_length(state,file,summary->name)); line1179; for:TZrUInt64 index = 0; index < summaryCount; index++`; `2:&summary->callableChildIndex; (sizeof(TZrUInt32)) * (1); line1180; for:TZrUInt64 index = 0; index < summaryCount; index++`; `3:&effectCount; (sizeof(TZrUInt64)) * (1); line1181; for:TZrUInt64 index = 0; index < summaryCount; index++`; `4:arguments; encoded_size(write_function_module_effect(file,state,effect)); line1185; for:TZrUInt64 index = 0; index < summaryCount; index++ & for:TZrUInt64 effectIndex = 0; effectIndex < effectCount; effectIndex++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L032 write_function_top_level_callable_bindings

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_top_level_callable_bindings`；writer；events=8；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&bindingCount; (sizeof(TZrUInt64)) * (1); line1193; unconditional`; `1:arguments; encoded_size(write_string_with_length(state,file,binding->name)); line1197; for:TZrUInt64 index = 0; index < bindingCount; index++`; `2:&binding->stackSlot; (sizeof(TZrUInt32)) * (1); line1198; for:TZrUInt64 index = 0; index < bindingCount; index++`; `3:&binding->callableChildIndex; (sizeof(TZrUInt32)) * (1); line1199; for:TZrUInt64 index = 0; index < bindingCount; index++`; `4:&binding->accessModifier; (sizeof(TZrUInt8)) * (1); line1200; for:TZrUInt64 index = 0; index < bindingCount; index++`; `5:&binding->exportKind; (sizeof(TZrUInt8)) * (1); line1201; for:TZrUInt64 index = 0; index < bindingCount; index++`; `6:&binding->readiness; (sizeof(TZrUInt8)) * (1); line1202; for:TZrUInt64 index = 0; index < bindingCount; index++`; `7:&binding->reserved0; (sizeof(TZrUInt8)) * (1); line1203; for:TZrUInt64 index = 0; index < bindingCount; index++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L033 write_function_metadata_parameters

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_metadata_parameters`；writer；events=9；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&count; (sizeof(TZrUInt64)) * (1); line1213; unconditional`; `1:arguments; encoded_size(write_string_with_length(state,file,parameters[index].name)); line1218; for:TZrUInt32 index = 0; index < parameterCount; index++`; `2:arguments; encoded_size(write_function_typed_type_ref(file,state,&parameters[index].type)); line1219; for:TZrUInt32 index = 0; index < parameterCount; index++`; `3:&hasDefaultValue; (sizeof(TZrUInt8)) * (1); line1220; for:TZrUInt32 index = 0; index < parameterCount; index++`; `4:arguments; encoded_size(write_function_constant(file,state,&parameters[index].defaultValue)); line1222; for:TZrUInt32 index = 0; index < parameterCount; index++ & if:hasDefaultValue`; `5:&hasDecoratorMetadata; (sizeof(TZrUInt8)) * (1); line1226; for:TZrUInt32 index = 0; index < parameterCount; index++`; `6:arguments; encoded_size(write_function_constant(file,state,&parameters[index].decoratorMetadataValue)); line1228; for:TZrUInt32 index = 0; index < parameterCount; index++ & if:hasDecoratorMetadata`; `7:&decoratorCount; (sizeof(TZrUInt64)) * (1); line1232; for:TZrUInt32 index = 0; index < parameterCount; index++`; `8:arguments; encoded_size(write_string_with_length(state,file,parameters[index].decoratorNames[decoratorIndex])); line1234; for:TZrUInt32 index = 0; index < parameterCount; index++ & for:TZrUInt64 decoratorIndex = 0; decoratorIndex < decoratorCount; decoratorIndex++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L034 write_function_parameter_metadata

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_parameter_metadata`；writer；events=2；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:arguments; encoded_size(write_function_metadata_parameters(file,state,ZR_NULL,0)); line1243; if:function == ZR_NULL`; `1:arguments; encoded_size(write_function_metadata_parameters(file,state,function->parameterMetadata,function->parameterMetadataCount)); line1246; unconditional`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L035 write_function_callable_return_metadata

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_callable_return_metadata`；writer；events=2；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&hasCallableReturnType; (sizeof(TZrUInt8)) * (1); line1256; unconditional`; `1:arguments; encoded_size(write_function_typed_type_ref(file,state,&function->callableReturnType)); line1261; unconditional`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L036 write_function_compile_time_metadata

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_compile_time_metadata`；writer；events=15；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&variableCount; (sizeof(TZrUInt64)) * (1); line1268; unconditional`; `1:arguments; encoded_size(write_string_with_length(state,file,info->name)); line1273; for:TZrUInt64 index = 0; index < variableCount; index++`; `2:arguments; encoded_size(write_function_typed_type_ref(file,state,&info->type)); line1274; for:TZrUInt64 index = 0; index < variableCount; index++`; `3:&info->lineInSourceStart; (sizeof(TZrUInt32)) * (1); line1275; for:TZrUInt64 index = 0; index < variableCount; index++`; `4:&info->lineInSourceEnd; (sizeof(TZrUInt32)) * (1); line1276; for:TZrUInt64 index = 0; index < variableCount; index++`; `5:&pathBindingCount; (sizeof(TZrUInt64)) * (1); line1277; for:TZrUInt64 index = 0; index < variableCount; index++`; `6:arguments; encoded_size(write_string_with_length(state,file,binding->path)); line1281; for:TZrUInt64 index = 0; index < variableCount; index++ & for:TZrUInt64 bindingIndex = 0; bindingIndex < pathBindingCount; bindingIndex++`; `7:&binding->targetKind; (sizeof(TZrUInt8)) * (1); line1282; for:TZrUInt64 index = 0; index < variableCount; index++ & for:TZrUInt64 bindingIndex = 0; bindingIndex < pathBindingCount; bindingIndex++`; `8:arguments; encoded_size(write_string_with_length(state,file,binding->targetName)); line1283; for:TZrUInt64 index = 0; index < variableCount; index++ & for:TZrUInt64 bindingIndex = 0; bindingIndex < pathBindingCount; bindingIndex++`; `9:&functionCount; (sizeof(TZrUInt64)) * (1); line1287; unconditional`; `10:arguments; encoded_size(write_string_with_length(state,file,info->name)); line1290; for:TZrUInt64 index = 0; index < functionCount; index++`; `11:arguments; encoded_size(write_function_typed_type_ref(file,state,&info->returnType)); line1291; for:TZrUInt64 index = 0; index < functionCount; index++`; `12:arguments; encoded_size(write_function_metadata_parameters(file,state,info->parameters,info->parameterCount)); line1292; for:TZrUInt64 index = 0; index < functionCount; index++`; `13:&info->lineInSourceStart; (sizeof(TZrUInt32)) * (1); line1295; for:TZrUInt64 index = 0; index < functionCount; index++`; `14:&info->lineInSourceEnd; (sizeof(TZrUInt32)) * (1); line1296; for:TZrUInt64 index = 0; index < functionCount; index++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L037 write_function_escape_metadata

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_escape_metadata`；writer；events=10；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&bindingCount; (sizeof(TZrUInt64)) * (1); line1306; unconditional`; `1:arguments; encoded_size(write_string_with_length(state,file,binding->name)); line1310; for:TZrUInt64 index = 0; index < bindingCount; index++`; `2:&binding->slotOrIndex; (sizeof(TZrUInt32)) * (1); line1311; for:TZrUInt64 index = 0; index < bindingCount; index++`; `3:&binding->scopeDepth; (sizeof(TZrUInt32)) * (1); line1312; for:TZrUInt64 index = 0; index < bindingCount; index++`; `4:&binding->escapeFlags; (sizeof(TZrUInt32)) * (1); line1313; for:TZrUInt64 index = 0; index < bindingCount; index++`; `5:&binding->bindingKind; (sizeof(TZrUInt8)) * (1); line1314; for:TZrUInt64 index = 0; index < bindingCount; index++`; `6:&binding->reserved0; (sizeof(TZrUInt8)) * (1); line1315; for:TZrUInt64 index = 0; index < bindingCount; index++`; `7:&binding->reserved1; (sizeof(TZrUInt16)) * (1); line1316; for:TZrUInt64 index = 0; index < bindingCount; index++`; `8:&returnSlotCount; (sizeof(TZrUInt64)) * (1); line1319; unconditional`; `9:&function->returnEscapeSlots[index]; (sizeof(TZrUInt32)) * (1); line1321; for:TZrUInt64 index = 0; index < returnSlotCount; index++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L038 write_function_decorator_metadata

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_decorator_metadata`；writer；events=4；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&hasDecoratorMetadata; (sizeof(TZrUInt8)) * (1); line1329; unconditional`; `1:arguments; encoded_size(write_function_constant(file,state,&function->decoratorMetadataValue)); line1331; if:hasDecoratorMetadata`; `2:&decoratorCount; (sizeof(TZrUInt64)) * (1); line1336; unconditional`; `3:arguments; encoded_size(write_string_with_length(state,file,function->decoratorNames[index])); line1338; for:TZrUInt64 index = 0; index < decoratorCount; index++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L039 write_function_test_manifest

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_test_manifest`；writer；events=2；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&length; (sizeof(length)) * (1); line1349; unconditional`; `1:function->testManifestData; (sizeof(TZrByte)) * ((TZrSize)length); line1351; if:length > 0U && function->testManifestData != ZR_NULL`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L040 write_function_member_entries

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_member_entries`；writer；events=7；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&memberCount; (sizeof(TZrUInt64)) * (1); line1358; unconditional`; `1:arguments; encoded_size(write_string_with_length(state,file,entry->symbol)); line1362; for:TZrUInt64 index = 0; index < memberCount; index++`; `2:&entry->entryKind; (sizeof(TZrUInt8)) * (1); line1363; for:TZrUInt64 index = 0; index < memberCount; index++`; `3:&entry->reserved0; (sizeof(TZrUInt8)) * (1); line1364; for:TZrUInt64 index = 0; index < memberCount; index++`; `4:&entry->reserved1; (sizeof(TZrUInt16)) * (1); line1365; for:TZrUInt64 index = 0; index < memberCount; index++`; `5:&entry->prototypeIndex; (sizeof(TZrUInt32)) * (1); line1366; for:TZrUInt64 index = 0; index < memberCount; index++`; `6:&entry->descriptorIndex; (sizeof(TZrUInt32)) * (1); line1367; for:TZrUInt64 index = 0; index < memberCount; index++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L041 write_function_semir_metadata

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_semir_metadata`；writer；events=25；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&typeCount; (sizeof(TZrUInt64)) * (1); line1379; unconditional`; `1:arguments; encoded_size(write_function_typed_type_ref(file,state,&function->semIrTypeTable[index])); line1381; for:TZrUInt64 index = 0; index < typeCount; index++`; `2:&ownershipCount; (sizeof(TZrUInt64)) * (1); line1384; unconditional`; `3:&function->semIrOwnershipTable[index].state; (sizeof(TZrUInt32)) * (1); line1386; for:TZrUInt64 index = 0; index < ownershipCount; index++`; `4:&effectCount; (sizeof(TZrUInt64)) * (1); line1389; unconditional`; `5:&entry->kind; (sizeof(TZrUInt32)) * (1); line1392; for:TZrUInt64 index = 0; index < effectCount; index++`; `6:&entry->instructionIndex; (sizeof(TZrUInt32)) * (1); line1393; for:TZrUInt64 index = 0; index < effectCount; index++`; `7:&entry->ownershipInputIndex; (sizeof(TZrUInt32)) * (1); line1394; for:TZrUInt64 index = 0; index < effectCount; index++`; `8:&entry->ownershipOutputIndex; (sizeof(TZrUInt32)) * (1); line1395; for:TZrUInt64 index = 0; index < effectCount; index++`; `9:&blockCount; (sizeof(TZrUInt64)) * (1); line1398; unconditional`; `10:&entry->blockId; (sizeof(TZrUInt32)) * (1); line1401; for:TZrUInt64 index = 0; index < blockCount; index++`; `11:&entry->firstInstructionIndex; (sizeof(TZrUInt32)) * (1); line1402; for:TZrUInt64 index = 0; index < blockCount; index++`; `12:&entry->instructionCount; (sizeof(TZrUInt32)) * (1); line1403; for:TZrUInt64 index = 0; index < blockCount; index++`; `13:&instructionCount; (sizeof(TZrUInt64)) * (1); line1406; unconditional`; `14:&entry->opcode; (sizeof(TZrUInt32)) * (1); line1409; for:TZrUInt64 index = 0; index < instructionCount; index++`; `15:&entry->execInstructionIndex; (sizeof(TZrUInt32)) * (1); line1410; for:TZrUInt64 index = 0; index < instructionCount; index++`; `16:&entry->typeTableIndex; (sizeof(TZrUInt32)) * (1); line1411; for:TZrUInt64 index = 0; index < instructionCount; index++`; `17:&entry->effectTableIndex; (sizeof(TZrUInt32)) * (1); line1412; for:TZrUInt64 index = 0; index < instructionCount; index++`; `18:&entry->destinationSlot; (sizeof(TZrUInt32)) * (1); line1413; for:TZrUInt64 index = 0; index < instructionCount; index++`; `19:&entry->operand0; (sizeof(TZrUInt32)) * (1); line1414; for:TZrUInt64 index = 0; index < instructionCount; index++`; `20:&entry->operand1; (sizeof(TZrUInt32)) * (1); line1415; for:TZrUInt64 index = 0; index < instructionCount; index++`; `21:&entry->deoptId; (sizeof(TZrUInt32)) * (1); line1416; for:TZrUInt64 index = 0; index < instructionCount; index++`; `22:&deoptCount; (sizeof(TZrUInt64)) * (1); line1419; unconditional`; `23:&entry->deoptId; (sizeof(TZrUInt32)) * (1); line1422; for:TZrUInt64 index = 0; index < deoptCount; index++`; `24:&entry->execInstructionIndex; (sizeof(TZrUInt32)) * (1); line1423; for:TZrUInt64 index = 0; index < deoptCount; index++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L042 write_function_callsite_cache_metadata

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_callsite_cache_metadata`；writer；events=6；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&cacheCount; (sizeof(TZrUInt64)) * (1); line1430; unconditional`; `1:&entry->kind; (sizeof(TZrUInt32)) * (1); line1433; for:TZrUInt64 index = 0; index < cacheCount; index++`; `2:&entry->instructionIndex; (sizeof(TZrUInt32)) * (1); line1434; for:TZrUInt64 index = 0; index < cacheCount; index++`; `3:&entry->memberEntryIndex; (sizeof(TZrUInt32)) * (1); line1435; for:TZrUInt64 index = 0; index < cacheCount; index++`; `4:&entry->deoptId; (sizeof(TZrUInt32)) * (1); line1436; for:TZrUInt64 index = 0; index < cacheCount; index++`; `5:&entry->argumentCount; (sizeof(TZrUInt32)) * (1); line1437; for:TZrUInt64 index = 0; index < cacheCount; index++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L043 write_ffi_type_contract

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_ffi_type_contract`；writer；events=8；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&typeKind; (sizeof(typeKind)) * (1); line1446; unconditional`; `1:&type->size; (sizeof(type->size)) * (1); line1447; unconditional`; `2:&type->alignment; (sizeof(type->alignment)) * (1); line1448; unconditional`; `3:&type->canonicalTypeHash; (sizeof(type->canonicalTypeHash)) * (1); line1449; unconditional`; `4:&type->layoutHash; (sizeof(type->layoutHash)) * (1); line1450; unconditional`; `5:&type->flags; (sizeof(type->flags)) * (1); line1451; unconditional`; `6:&type->aggregateFieldStart; (sizeof(type->aggregateFieldStart)) * (1); line1452; unconditional`; `7:&type->aggregateFieldCount; (sizeof(type->aggregateFieldCount)) * (1); line1456; unconditional`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L044 write_ffi_callable_contract

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_ffi_callable_contract`；writer；events=13；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&callable->parameterCount; (sizeof(callable->parameterCount)) * (1); line1467; unconditional`; `1:&parameter->canonicalTypeHash; (sizeof(parameter->canonicalTypeHash)) * (1); line1480; for:TZrUInt32 index = 0u; index < callable->parameterCount; index++`; `2:&passingForm; (sizeof(passingForm)) * (1); line1484; for:TZrUInt32 index = 0u; index < callable->parameterCount; index++`; `3:&escapeUpperBound; (sizeof(escapeUpperBound)) * (1); line1485; for:TZrUInt32 index = 0u; index < callable->parameterCount; index++`; `4:&entryInitialization; (sizeof(entryInitialization)) * (1); line1486; for:TZrUInt32 index = 0u; index < callable->parameterCount; index++`; `5:&exitInitialization; (sizeof(exitInitialization)) * (1); line1487; for:TZrUInt32 index = 0u; index < callable->parameterCount; index++`; `6:&acceptsTemporary; (sizeof(acceptsTemporary)) * (1); line1488; for:TZrUInt32 index = 0u; index < callable->parameterCount; index++`; `7:&callSiteMarker; (sizeof(callSiteMarker)) * (1); line1489; for:TZrUInt32 index = 0u; index < callable->parameterCount; index++`; `8:&callable->returnTypeHash; (sizeof(callable->returnTypeHash)) * (1); line1491; unconditional`; `9:&receiverEffect; (sizeof(receiverEffect)) * (1); line1494; unconditional`; `10:&callable->effectFlags; (sizeof(callable->effectFlags)) * (1); line1496; unconditional`; `11:&isVariadic; (sizeof(isVariadic)) * (1); line1497; unconditional`; `12:&callable->contractHash; (sizeof(callable->contractHash)) * (1); line1498; unconditional`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L045 write_function_native_import_contracts

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_native_import_contracts`；writer；events=43；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&contractCount; (sizeof(contractCount)) * (1); line1507; unconditional`; `1:&contract->schemaVersion; (sizeof(contract->schemaVersion)) * (1); line1527; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++`; `2:contract->libraryLocator; (1u) * (sizeof(contract->libraryLocator)); line1528; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++`; `3:contract->entryPoint; (1u) * (sizeof(contract->entryPoint)); line1529; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++`; `4:&contract->symbolId; (sizeof(contract->symbolId)) * (1); line1530; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++`; `5:&contract->declaringModuleId; (sizeof(contract->declaringModuleId)) * (1); line1531; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++`; `6:arguments; encoded_size(write_ffi_callable_contract(file,&contract->callable)); line1532; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++`; `7:&contract->availability; (sizeof(contract->availability)) * (1); line1533; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++`; `8:&contract->requiredCapabilities; (sizeof(contract->requiredCapabilities)) * (1); line1534; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++`; `9:contract->sourceMapping.document; (1u) * (sizeof(contract->sourceMapping.document)); line1538; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++`; `10:&contract->sourceMapping.startOffset; (sizeof(contract->sourceMapping.startOffset)) * (1); line1542; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++`; `11:&contract->sourceMapping.endOffset; (sizeof(contract->sourceMapping.endOffset)) * (1); line1546; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++`; `12:&contract->sourceMapping.startLine; (sizeof(contract->sourceMapping.startLine)) * (1); line1550; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++`; `13:&contract->sourceMapping.startColumn; (sizeof(contract->sourceMapping.startColumn)) * (1); line1554; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++`; `14:&contract->sourceMapping.endLine; (sizeof(contract->sourceMapping.endLine)) * (1); line1558; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++`; `15:&contract->sourceMapping.endColumn; (sizeof(contract->sourceMapping.endColumn)) * (1); line1562; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++`; `16:&abi; (sizeof(abi)) * (1); line1566; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++`; `17:&contract->signature.targetPointerSize; (sizeof(contract->signature.targetPointerSize)) * (1); line1567; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++`; `18:&targetEndianness; (sizeof(targetEndianness)) * (1); line1571; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++`; `19:contract->signature.targetTriple; (1u) * (sizeof(contract->signature.targetTriple)); line1572; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++`; `20:&contract->signature.targetAbiHash; (sizeof(contract->signature.targetAbiHash)) * (1); line1576; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++`; `21:&charset; (sizeof(charset)) * (1); line1580; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++`; `22:&errorPolicy; (sizeof(errorPolicy)) * (1); line1581; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++`; `23:&cleanupPolicy; (sizeof(cleanupPolicy)) * (1); line1582; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++`; `24:&callbackLifetime; (sizeof(callbackLifetime)) * (1); line1583; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++`; `25:&callbackThreadPolicy; (sizeof(callbackThreadPolicy)) * (1); line1584; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++`; `26:&callbackExceptionPolicy; (sizeof(callbackExceptionPolicy)) * (1); line1585; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++`; `27:&isVariadic; (sizeof(isVariadic)) * (1); line1586; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++`; `28:&contract->signature.parameterCount; (sizeof(contract->signature.parameterCount)) * (1); line1587; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++`; `29:arguments; encoded_size(write_ffi_type_contract(file,&contract->signature.returnType)); line1591; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++`; `30:arguments; encoded_size(write_ffi_type_contract(file,&parameter->type)); line1602; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++ & for:TZrUInt32 parameterIndex = 0u; parameterIndex < contract->signature.parameterCount; parameterIndex++`; `31:&direction; (sizeof(direction)) * (1); line1603; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++ & for:TZrUInt32 parameterIndex = 0u; parameterIndex < contract->signature.parameterCount; parameterIndex++`; `32:&marshalling; (sizeof(marshalling)) * (1); line1604; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++ & for:TZrUInt32 parameterIndex = 0u; parameterIndex < contract->signature.parameterCount; parameterIndex++`; `33:&ownership; (sizeof(ownership)) * (1); line1605; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++ & for:TZrUInt32 parameterIndex = 0u; parameterIndex < contract->signature.parameterCount; parameterIndex++`; `34:&isNullable; (sizeof(isNullable)) * (1); line1606; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++ & for:TZrUInt32 parameterIndex = 0u; parameterIndex < contract->signature.parameterCount; parameterIndex++`; `35:&parameter->flags; (sizeof(parameter->flags)) * (1); line1607; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++ & for:TZrUInt32 parameterIndex = 0u; parameterIndex < contract->signature.parameterCount; parameterIndex++`; `36:&contract->signature.aggregateFieldCount; (sizeof(contract->signature.aggregateFieldCount)) * (1); line1609; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++`; `37:field->name; (1u) * (sizeof(field->name)); line1620; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++ & for:TZrUInt32 fieldIndex = 0u; fieldIndex < contract->signature.aggregateFieldCount; fieldIndex++`; `38:&fieldTypeKind; (sizeof(fieldTypeKind)) * (1); line1621; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++ & for:TZrUInt32 fieldIndex = 0u; fieldIndex < contract->signature.aggregateFieldCount; fieldIndex++`; `39:&field->size; (sizeof(field->size)) * (1); line1622; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++ & for:TZrUInt32 fieldIndex = 0u; fieldIndex < contract->signature.aggregateFieldCount; fieldIndex++`; `40:&field->alignment; (sizeof(field->alignment)) * (1); line1623; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++ & for:TZrUInt32 fieldIndex = 0u; fieldIndex < contract->signature.aggregateFieldCount; fieldIndex++`; `41:&field->offset; (sizeof(field->offset)) * (1); line1624; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++ & for:TZrUInt32 fieldIndex = 0u; fieldIndex < contract->signature.aggregateFieldCount; fieldIndex++`; `42:&contract->signature.signatureHash; (sizeof(contract->signature.signatureHash)) * (1); line1626; for:TZrUInt64 contractIndex = 0u; contractIndex < contractCount; contractIndex++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L046 write_function_prototypes

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_prototypes`；writer；events=10；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&prototypesLength; (sizeof(TZrUInt64)) * (1); line1678; unconditional`; `1:&zero; (sizeof(TZrUInt64)) * (1); line1682; if:prototypesLength == 0 &#124;&#124; function->prototypeData == ZR_NULL &#124;&#124; function->prototypeDataLength == 0`; `2:&zero; (sizeof(TZrUInt64)) * (1); line1683; if:prototypesLength == 0 &#124;&#124; function->prototypeData == ZR_NULL &#124;&#124; function->prototypeDataLength == 0`; `3:&zero; (sizeof(TZrUInt64)) * (1); line1684; if:prototypesLength == 0 &#124;&#124; function->prototypeData == ZR_NULL &#124;&#124; function->prototypeDataLength == 0`; `4:&classCount; (sizeof(TZrUInt64)) * (1); line1693; unconditional`; `5:arguments; encoded_size(write_prototype_class(state,file,protoInfo,currentPos,function)); line1713; if:classCount > 0 & for:TZrUInt32 i = 0; i < prototypesLength; i++ & if:protoInfo->type == ZR_OBJECT_PROTOTYPE_TYPE_CLASS`; `6:&structCount; (sizeof(TZrUInt64)) * (1); line1723; unconditional`; `7:arguments; encoded_size(write_prototype_struct(state,file,protoInfo,currentPos,function)); line1743; if:structCount > 0 & for:TZrUInt32 i = 0; i < prototypesLength; i++ & if:protoInfo->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT`; `8:&prototypeBlobLength; (sizeof(TZrUInt64)) * (1); line1751; unconditional`; `9:function->prototypeData; (1) * ((TZrSize)prototypeBlobLength); line1753; if:prototypeBlobLength > 0`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L047 write_io_function_internal

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_io_function_internal`；writer；events=48；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:arguments; encoded_size(write_function_name(state,file,function,defaultName)); line1767; unconditional`; `1:&startLine; (sizeof(TZrUInt64)) * (1); line1779; unconditional`; `2:&endLine; (sizeof(TZrUInt64)) * (1); line1780; unconditional`; `3:&parametersLength; (sizeof(TZrUInt64)) * (1); line1781; unconditional`; `4:&hasVarArgs; (sizeof(TZrUInt64)) * (1); line1782; unconditional`; `5:&stackSize; (sizeof(TZrUInt32)) * (1); line1783; unconditional`; `6:&vmEntryClearStackSizePlusOne; (sizeof(TZrUInt32)) * (1); line1784; unconditional`; `7:arguments; encoded_size(write_function_frame_layout(file,function)); line1785; unconditional`; `8:&instructionsLength; (sizeof(TZrUInt64)) * (1); line1786; unconditional`; `9:&rawValue; (sizeof(TZrUInt64)) * (1); line1790; for:TZrUInt64 i = 0; i < instructionsLength; i++`; `10:arguments; encoded_size(write_function_local_variables(file,function)); line1794; unconditional`; `11:arguments; encoded_size(write_function_closure_variables(state,file,function)); line1795; unconditional`; `12:arguments; encoded_size(write_function_exception_metadata(state,file,function)); line1796; unconditional`; `13:&constantsLength; (sizeof(TZrUInt64)) * (1); line1800; unconditional`; `14:arguments; encoded_size(write_function_constant(file,state,&function->constantValueList[i])); line1802; for:TZrUInt64 i = 0; i < constantsLength; i++`; `15:&exportedVariablesLength; (sizeof(TZrUInt64)) * (1); line1810; unconditional`; `16:arguments; encoded_size(write_string_with_length(state,file,exported->name)); line1813; for:TZrUInt64 i = 0; i < exportedVariablesLength; i++`; `17:&exported->stackSlot; (sizeof(TZrUInt32)) * (1); line1814; for:TZrUInt64 i = 0; i < exportedVariablesLength; i++`; `18:&exported->accessModifier; (sizeof(TZrUInt8)) * (1); line1815; for:TZrUInt64 i = 0; i < exportedVariablesLength; i++`; `19:&exported->exportKind; (sizeof(TZrUInt8)) * (1); line1816; for:TZrUInt64 i = 0; i < exportedVariablesLength; i++`; `20:&exported->readiness; (sizeof(TZrUInt8)) * (1); line1817; for:TZrUInt64 i = 0; i < exportedVariablesLength; i++`; `21:&exported->reserved0; (sizeof(TZrUInt8)) * (1); line1818; for:TZrUInt64 i = 0; i < exportedVariablesLength; i++`; `22:&exported->callableChildIndex; (sizeof(TZrUInt32)) * (1); line1819; for:TZrUInt64 i = 0; i < exportedVariablesLength; i++`; `23:arguments; encoded_size(write_function_typed_local_bindings(file,state,function)); line1823; unconditional`; `24:arguments; encoded_size(write_function_typed_closure_bindings(file,state,function)); line1824; unconditional`; `25:arguments; encoded_size(write_function_typed_export_symbols(file,state,function)); line1825; unconditional`; `26:arguments; encoded_size(write_function_metadata_token_model(file,state,function)); line1826; unconditional`; `27:arguments; encoded_size(write_function_module_metadata_ref_table(file,state,function)); line1827; unconditional`; `28:arguments; encoded_size(write_function_module_metadata_bindings(file,function)); line1828; unconditional`; `29:arguments; encoded_size(write_function_static_imports(file,state,function)); line1829; unconditional`; `30:arguments; encoded_size(write_function_module_entry_effects(file,state,function)); line1830; unconditional`; `31:arguments; encoded_size(write_function_exported_callable_summaries(file,state,function)); line1831; unconditional`; `32:arguments; encoded_size(write_function_top_level_callable_bindings(file,state,function)); line1832; unconditional`; `33:arguments; encoded_size(write_function_parameter_metadata(file,state,function)); line1833; unconditional`; `34:arguments; encoded_size(write_function_callable_return_metadata(file,state,function)); line1836; unconditional`; `35:arguments; encoded_size(write_function_compile_time_metadata(file,state,function)); line1837; unconditional`; `36:arguments; encoded_size(write_function_escape_metadata(file,state,function)); line1840; unconditional`; `37:arguments; encoded_size(write_function_test_manifest(file,function)); line1841; unconditional`; `38:arguments; encoded_size(write_function_decorator_metadata(file,state,function)); line1842; unconditional`; `39:arguments; encoded_size(write_function_member_entries(file,state,function)); line1845; unconditional`; `40:arguments; encoded_size(write_function_semir_metadata(file,state,function)); line1846; unconditional`; `41:arguments; encoded_size(write_function_callsite_cache_metadata(file,function)); line1847; unconditional`; `42:arguments; encoded_size(ZrParser_Writer_WriteCallBindings(file,function)); line1848; unconditional`; `43:arguments; encoded_size(write_function_native_import_contracts(file,function)); line1851; unconditional`; `44:arguments; encoded_size(write_function_prototypes(state,file,function)); line1853; unconditional`; `45:&closuresLength; (sizeof(TZrUInt64)) * (1); line1857; unconditional`; `46:arguments; encoded_size(write_io_function_internal(state,file,&function->childFunctionList[i],ZR_NULL,options)); line1859; for:TZrUInt64 i = 0; i < closuresLength; i++`; `47:arguments; encoded_size(write_function_debug_infos(file,function,options)); line1865; unconditional`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L048 ZrParser_Writer_WriteIoFunction

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer.c::ZrParser_Writer_WriteIoFunction`；writer；events=1；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:arguments; encoded_size(write_io_function_internal(state,file,function,defaultName,options)); line1875; unconditional`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L049 writer_binary_write_native_string_with_length

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c::writer_binary_write_native_string_with_length`；writer；events=2；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&length; (sizeof(TZrSize)) * (1); line21; unconditional`; `1:text; (sizeof(TZrChar)) * (length); line23; if:length > 0`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L050 ZrParser_Writer_WriteBinaryFileWithOptions

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c::ZrParser_Writer_WriteBinaryFileWithOptions`；writer；events=17；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:ZR_IO_SOURCE_SIGNATURE; (sizeof(TZrUInt8)) * (ZR_IO_SOURCE_SIGNATURE_LENGTH); line67; unconditional`; `1:&versionMajor; (sizeof(TZrUInt32)) * (1); line68; unconditional`; `2:&versionMinor; (sizeof(TZrUInt32)) * (1); line69; unconditional`; `3:&versionPatch; (sizeof(TZrUInt32)) * (1); line70; unconditional`; `4:&format; (sizeof(TZrUInt64)) * (1); line71; unconditional`; `5:&nativeIntSize; (sizeof(TZrUInt8)) * (1); line72; unconditional`; `6:&sizeTypeSize; (sizeof(TZrUInt8)) * (1); line73; unconditional`; `7:&instructionSize; (sizeof(TZrUInt8)) * (1); line74; unconditional`; `8:&endian; (sizeof(TZrUInt8)) * (1); line75; unconditional`; `9:&debug; (sizeof(TZrUInt8)) * (1); line78; unconditional`; `10:opt; (sizeof(TZrUInt8)) * (ZR_IO_SOURCE_HEADER_OPT_BYTES); line79; unconditional`; `11:&modulesLength; (sizeof(TZrUInt64)) * (1); line80; unconditional`; `12:arguments; encoded_size(writer_binary_write_native_string_with_length(file,moduleName)); line82; unconditional`; `13:arguments; encoded_size(writer_binary_write_native_string_with_length(file,moduleHash)); line83; unconditional`; `14:&importsLength; (sizeof(TZrUInt64)) * (1); line84; unconditional`; `15:&declaresLength; (sizeof(TZrUInt64)) * (1); line85; unconditional`; `16:arguments; encoded_size(ZrParser_Writer_WriteIoFunction(state,file,function,"__entry",options)); line87; unconditional`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L051 io_read_char

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_char`；reader；events=1；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:&value; 1u; line70; unconditional`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L052 io_read_size

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_size`；reader；events=1；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:(TZrBytePtr) &size; sizeof(size); line76; unconditional`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L053 io_read_float

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_float`；reader；events=1；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:(TZrBytePtr) &value; sizeof(value); line83; unconditional`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L054 io_read_int

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_int`；reader；events=1；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:(TZrBytePtr) &value; sizeof(value); line89; unconditional`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L055 io_read_u_int

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_u_int`；reader；events=1；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:(TZrBytePtr) &value; sizeof(value); line95; unconditional`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L056 io_read_string_with_length

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_string_with_length`；reader；events=2；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:TZrSize length; encoded_size(io_read_size(io)); line140; unconditional`; `1:string.bytes; length * sizeof(TZrChar); line154; guard:length>0 && !hasReadError && length<=remained && length<=allocationBudgetRemaining`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L057 io_read_imports

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_imports`；reader；events=2；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:import->name; encoded_size(io_read_string_with_length(io)); line164; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `1:import->md5; encoded_size(io_read_string_with_length(io)); line165; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L058 io_read_value

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_value`；reader；events=10；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:TZrInt32 boolean; encoded_size(io_read_char(io)); line255; switch:type`; `1:value->nativeObject.nativeDouble; encoded_size(io_read_float(io)); line261; switch:type`; `2:value->nativeObject.nativeInt64; encoded_size(io_read_int(io)); line267; switch:type`; `3:value->nativeObject.nativeUInt64; encoded_size(io_read_u_int(io)); line273; switch:type`; `4:SZrString *stringValue; encoded_size(io_read_string_with_length(io)); line276; switch:type`; `5:entryCount; sizeof(TZrSize); line286; switch:type`; `6:arguments; encoded_size(io_read_typed_value(io,&keyValue)); line304; switch:type & for:TZrSize index = 0; !io->hasReadError && (index < entryCount); index++`; `7:arguments; encoded_size(io_read_typed_value(io,&entryValue)); line305; switch:type & for:TZrSize index = 0; !io->hasReadError && (index < entryCount); index++`; `8:elementCount; sizeof(TZrSize); line315; switch:type`; `9:arguments; encoded_size(io_read_typed_value(io,&elementValue)); line333; switch:type & for:TZrSize index = 0; !io->hasReadError && (index < elementCount); index++`。

switch/case labels：`[{"label":"case ZR_VALUE_TYPE_NULL","line":251},{"label":"case ZR_VALUE_TYPE_BOOL","line":254},{"label":"case ZR_VALUE_TYPE_FLOAT","line":259},{"label":"case ZR_VALUE_TYPE_DOUBLE","line":260},{"label":"case ZR_VALUE_TYPE_INT8","line":263},{"label":"case ZR_VALUE_TYPE_INT16","line":264},{"label":"case ZR_VALUE_TYPE_INT32","line":265},{"label":"case ZR_VALUE_TYPE_INT64","line":266},{"label":"case ZR_VALUE_TYPE_UINT8","line":269},{"label":"case ZR_VALUE_TYPE_UINT16","line":270},{"label":"case ZR_VALUE_TYPE_UINT32","line":271},{"label":"case ZR_VALUE_TYPE_UINT64","line":272},{"label":"case ZR_VALUE_TYPE_STRING","line":275},{"label":"case ZR_VALUE_TYPE_OBJECT","line":282},{"label":"case ZR_VALUE_TYPE_ARRAY","line":311},{"label":"default","line":340}]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L059 io_read_typed_value

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_typed_value`；reader；events=2；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:rawType; sizeof(TZrUInt32); line361; unconditional`; `1:arguments; encoded_size(io_read_value(io,(EZrValueType)rawType,&pureValue)); line366; if:!io->hasReadError`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L060 io_read_field

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_field`；reader；events=1；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:field->name; encoded_size(io_read_string_with_length(io)); line371; unconditional`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L061 io_read_references

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_references`；reader；events=3；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:reference->referenceModuleName; encoded_size(io_read_string_with_length(io)); line377; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `1:reference->referenceModuleMd5; encoded_size(io_read_string_with_length(io)); line378; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `2:reference->referenceIndex; sizeof(TZrSize); line379; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L062 io_read_function_local_variables

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_local_variables`；reader；events=8；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:variable->name; encoded_size(io_read_string_with_length(io)); line387; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `1:variable->stackSlot; sizeof(TZrUInt32); line388; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `2:variable->instructionStartIndex; sizeof(TZrUInt64); line389; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `3:variable->instructionEndIndex; sizeof(TZrUInt64); line390; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `4:variable->startLine; sizeof(TZrUInt64); line391; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `5:variable->endLine; sizeof(TZrUInt64); line392; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `6:variable->scopeDepth; sizeof(TZrUInt32); line394; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_ESCAPE_METADATA`; `7:variable->escapeFlags; sizeof(TZrUInt32); line395; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_ESCAPE_METADATA`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L063 io_read_function_closure_variables

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_closure_variables`；reader；events=6；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:variable->name; encoded_size(io_read_string_with_length(io)); line406; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `1:variable->inStack; sizeof(TZrUInt8); line407; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `2:variable->index; sizeof(TZrUInt32); line408; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `3:variable->valueType; sizeof(TZrUInt32); line409; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `4:variable->scopeDepth; sizeof(TZrUInt32); line411; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_ESCAPE_METADATA`; `5:variable->escapeFlags; sizeof(TZrUInt32); line412; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_ESCAPE_METADATA`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L064 io_read_function_catch_clauses

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_catch_clauses`；reader；events=2；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:clauses[i].typeName; encoded_size(io_read_string_with_length(io)); line422; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `1:clauses[i].targetInstructionOffset; sizeof(TZrUInt64); line423; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L065 io_read_function_exception_handlers

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_exception_handlers`；reader；events=6；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:handlers[i].protectedStartInstructionOffset; sizeof(TZrUInt64); line431; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `1:handlers[i].finallyTargetInstructionOffset; sizeof(TZrUInt64); line432; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `2:handlers[i].afterFinallyInstructionOffset; sizeof(TZrUInt64); line433; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `3:handlers[i].catchClauseStartIndex; sizeof(TZrUInt32); line434; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `4:handlers[i].catchClauseCount; sizeof(TZrUInt32); line435; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `5:handlers[i].hasFinally; sizeof(TZrUInt8); line436; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L066 io_read_function_constant_variables

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_constant_variables`；reader；events=6；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:variable->type; sizeof(TZrUInt32); line444; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `1:variable->hasFunctionValue; sizeof(TZrBool); line448; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:variable->type == ZR_VALUE_TYPE_FUNCTION &#124;&#124; variable->type == ZR_VALUE_TYPE_CLOSURE`; `2:arguments; encoded_size(io_read_functions(io,variable->functionValue,1)); line453; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:variable->type == ZR_VALUE_TYPE_FUNCTION &#124;&#124; variable->type == ZR_VALUE_TYPE_CLOSURE & if:variable->hasFunctionValue`; `3:arguments; encoded_size(io_read_value(io,variable->type,&variable->value)); line456; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:variable->type != ZR_VALUE_TYPE_NATIVE_POINTER`; `4:variable->startLine; sizeof(TZrUInt64); line458; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `5:variable->endLine; sizeof(TZrUInt64); line464; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L067 io_read_function_exported_variables

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_exported_variables`；reader；events=7；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:variable->name; encoded_size(io_read_string_with_length(io)); line473; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `1:variable->stackSlot; sizeof(TZrUInt32); line474; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `2:variable->accessModifier; sizeof(TZrUInt8); line475; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `3:variable->exportKind; sizeof(TZrUInt8); line476; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `4:variable->readiness; sizeof(TZrUInt8); line477; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `5:variable->reserved0; sizeof(TZrUInt8); line478; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `6:variable->callableChildIndex; sizeof(TZrUInt32); line479; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L068 io_read_function_typed_type_ref

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_typed_type_ref`；reader；events=9；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:baseType; sizeof(TZrUInt32); line496; unconditional`; `1:isNullable; sizeof(TZrUInt8); line497; unconditional`; `2:ownershipQualifier; sizeof(TZrUInt32); line498; unconditional`; `3:isArray; sizeof(TZrUInt8); line499; unconditional`; `4:typeRef->typeName; encoded_size(io_read_string_with_length(io)); line500; unconditional`; `5:elementBaseType; sizeof(TZrUInt32); line501; unconditional`; `6:typeRef->elementTypeName; encoded_size(io_read_string_with_length(io)); line502; unconditional`; `7:staticCType; sizeof(TZrUInt32); line504; if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_SEMIR_STATIC_C_TYPES`; `8:staticCTypeId; sizeof(TZrUInt32); line505; if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_SEMIR_STATIC_C_TYPES`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L069 io_read_function_typed_local_bindings

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_typed_local_bindings`；reader；events=11；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:binding->name; encoded_size(io_read_string_with_length(io)); line524; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `1:binding->stackSlot; sizeof(TZrUInt32); line525; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `2:arguments; encoded_size(io_read_function_typed_type_ref(io,&binding->type)); line526; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `3:binding->symbolId; sizeof(TZrUInt32); line529; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_TYPED_LOCAL_CANONICAL_BINDINGS`; `4:binding->typeId; sizeof(TZrUInt32); line530; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_TYPED_LOCAL_CANONICAL_BINDINGS`; `5:binding->placeId; sizeof(TZrUInt32); line531; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_TYPED_LOCAL_CANONICAL_BINDINGS`; `6:binding->declarationStartLine; sizeof(TZrUInt32); line532; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_TYPED_LOCAL_CANONICAL_BINDINGS`; `7:binding->declarationStartColumn; sizeof(TZrUInt32); line533; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_TYPED_LOCAL_CANONICAL_BINDINGS`; `8:binding->declarationEndLine; sizeof(TZrUInt32); line534; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_TYPED_LOCAL_CANONICAL_BINDINGS`; `9:binding->declarationEndColumn; sizeof(TZrUInt32); line535; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_TYPED_LOCAL_CANONICAL_BINDINGS`; `10:binding->roleFlags; sizeof(TZrUInt32); line538; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_TYPED_LOCAL_ROLE_FLAGS`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L070 io_read_function_typed_closure_bindings

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_typed_closure_bindings`；reader；events=8；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:binding->captureIndex; sizeof(TZrUInt32); line550; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `1:arguments; encoded_size(io_read_function_typed_type_ref(io,&binding->type)); line551; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `2:binding->symbolId; sizeof(TZrUInt32); line552; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `3:binding->typeId; sizeof(TZrUInt32); line553; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `4:binding->declarationStartLine; sizeof(TZrUInt32); line554; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `5:binding->declarationStartColumn; sizeof(TZrUInt32); line555; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `6:binding->declarationEndLine; sizeof(TZrUInt32); line556; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `7:binding->declarationEndColumn; sizeof(TZrUInt32); line557; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L071 io_read_function_typed_export_generic_parameters

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_typed_export_generic_parameters`；reader；events=12；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:serializedCount; sizeof(TZrUInt32); line570; unconditional`; `1:parameter->name; encoded_size(io_read_string_with_length(io)); line592; for:TZrUInt32 index = 0; !io->hasReadError && (index < serializedCount); index++`; `2:parameter->genericKind; sizeof(TZrUInt8); line593; for:TZrUInt32 index = 0; !io->hasReadError && (index < serializedCount); index++`; `3:parameter->variance; sizeof(TZrUInt8); line594; for:TZrUInt32 index = 0; !io->hasReadError && (index < serializedCount); index++`; `4:parameter->requiresClass; sizeof(TZrUInt8); line595; for:TZrUInt32 index = 0; !io->hasReadError && (index < serializedCount); index++`; `5:parameter->requiresStruct; sizeof(TZrUInt8); line596; for:TZrUInt32 index = 0; !io->hasReadError && (index < serializedCount); index++`; `6:parameter->requiresNew; sizeof(TZrUInt8); line597; for:TZrUInt32 index = 0; !io->hasReadError && (index < serializedCount); index++`; `7:parameter->requiresOwner; sizeof(TZrUInt8); line598; for:TZrUInt32 index = 0; !io->hasReadError && (index < serializedCount); index++`; `8:parameter->reserved0; sizeof(TZrUInt16); line599; for:TZrUInt32 index = 0; !io->hasReadError && (index < serializedCount); index++`; `9:parameter->requiredOwnershipQualifier; sizeof(TZrUInt32); line600; for:TZrUInt32 index = 0; !io->hasReadError && (index < serializedCount); index++`; `10:serializedConstraintCount; sizeof(TZrUInt32); line601; for:TZrUInt32 index = 0; !io->hasReadError && (index < serializedCount); index++`; `11:SZrString *constraintName; encoded_size(io_read_string_with_length(io)); line615; for:TZrUInt32 index = 0; !io->hasReadError && (index < serializedCount); index++ & for:TZrUInt32 constraintIndex = 0; !io->hasReadError && (constraintIndex < serializedConstraintCount); constraintIndex++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L072 io_read_function_typed_export_symbols

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_typed_export_symbols`；reader；events=21；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:symbol->name; encoded_size(io_read_string_with_length(io)); line633; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `1:symbol->stackSlot; sizeof(TZrUInt32); line634; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `2:symbol->accessModifier; sizeof(TZrUInt8); line635; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `3:symbol->symbolKind; sizeof(TZrUInt8); line636; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `4:symbol->exportKind; sizeof(TZrUInt8); line637; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `5:symbol->readiness; sizeof(TZrUInt8); line638; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `6:symbol->reserved0; sizeof(TZrUInt16); line639; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `7:symbol->callableChildIndex; sizeof(TZrUInt32); line640; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `8:arguments; encoded_size(io_read_function_typed_type_ref(io,&symbol->valueType)); line641; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `9:symbol->parameterCount; sizeof(TZrSize); line642; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `10:arguments; encoded_size(io_read_function_typed_type_ref(io,&symbol->parameterTypes[paramIndex])); line648; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:symbol->parameterCount > 0 & if:symbol->parameterTypes != ZR_NULL & for:TZrSize paramIndex = 0; !io->hasReadError && (paramIndex < symbol->parameterCount); paramIndex++`; `11:arguments; encoded_size(io_read_function_typed_export_generic_parameters(io,symbol)); line656; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_TYPED_EXPORT_GENERIC_PARAMETERS`; `12:symbol->lineInSourceStart; sizeof(TZrUInt32); line660; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_TYPED_EXPORT_DECLARATION_SPANS`; `13:symbol->columnInSourceStart; sizeof(TZrUInt32); line661; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_TYPED_EXPORT_DECLARATION_SPANS`; `14:symbol->lineInSourceEnd; sizeof(TZrUInt32); line662; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_TYPED_EXPORT_DECLARATION_SPANS`; `15:symbol->columnInSourceEnd; sizeof(TZrUInt32); line663; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_TYPED_EXPORT_DECLARATION_SPANS`; `16:symbol->metadataToken; sizeof(TZrMetadataToken); line672; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_TOKEN_MODEL`; `17:symbol->signatureToken; sizeof(TZrMetadataToken); line673; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_TOKEN_MODEL`; `18:symbol->signatureBlobOffset; sizeof(TZrUInt32); line674; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_TOKEN_MODEL`; `19:symbol->signatureBlobLength; sizeof(TZrUInt32); line675; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_TOKEN_MODEL`; `20:symbol->signatureHash; sizeof(TZrUInt64); line677; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_TOKEN_MODEL & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_TOKEN_SIGNATURE_HASH`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L073 io_read_metadata_token_record_list

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_metadata_token_record_list`；reader；events=18；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:*outRecordLength; sizeof(TZrSize); line706; unconditional`; `1:record->token; sizeof(TZrMetadataToken); line713; if:*outRecordLength > 0 & for:TZrSize index = 0; !io->hasReadError && (index < *outRecordLength); index++`; `2:record->relatedToken; sizeof(TZrMetadataToken); line714; if:*outRecordLength > 0 & for:TZrSize index = 0; !io->hasReadError && (index < *outRecordLength); index++`; `3:record->ownerToken; sizeof(TZrMetadataToken); line716; if:*outRecordLength > 0 & for:TZrSize index = 0; !io->hasReadError && (index < *outRecordLength); index++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_TOKEN_OWNER_CHAIN`; `4:record->ownerIndex; sizeof(TZrUInt32); line720; if:*outRecordLength > 0 & for:TZrSize index = 0; !io->hasReadError && (index < *outRecordLength); index++`; `5:record->signatureBlobOffset; sizeof(TZrUInt32); line721; if:*outRecordLength > 0 & for:TZrSize index = 0; !io->hasReadError && (index < *outRecordLength); index++`; `6:record->signatureBlobLength; sizeof(TZrUInt32); line722; if:*outRecordLength > 0 & for:TZrSize index = 0; !io->hasReadError && (index < *outRecordLength); index++`; `7:record->signatureHash; sizeof(TZrUInt64); line724; if:*outRecordLength > 0 & for:TZrSize index = 0; !io->hasReadError && (index < *outRecordLength); index++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_TOKEN_SIGNATURE_HASH`; `8:record->layoutVersion; sizeof(TZrUInt32); line729; if:*outRecordLength > 0 & for:TZrSize index = 0; !io->hasReadError && (index < *outRecordLength); index++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_TOKEN_LAYOUT_IDENTITY`; `9:record->reserved0; sizeof(TZrUInt32); line730; if:*outRecordLength > 0 & for:TZrSize index = 0; !io->hasReadError && (index < *outRecordLength); index++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_TOKEN_LAYOUT_IDENTITY`; `10:record->layoutHash; sizeof(TZrUInt64); line731; if:*outRecordLength > 0 & for:TZrSize index = 0; !io->hasReadError && (index < *outRecordLength); index++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_TOKEN_LAYOUT_IDENTITY`; `11:record->targetMetadataToken; sizeof(TZrMetadataToken); line738; if:*outRecordLength > 0 & for:TZrSize index = 0; !io->hasReadError && (index < *outRecordLength); index++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_TARGET_SIGNATURE_TOKENS`; `12:record->targetSignatureToken; sizeof(TZrMetadataToken); line739; if:*outRecordLength > 0 & for:TZrSize index = 0; !io->hasReadError && (index < *outRecordLength); index++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_TARGET_SIGNATURE_TOKENS`; `13:record->targetSignatureHash; sizeof(TZrUInt64); line745; if:*outRecordLength > 0 & for:TZrSize index = 0; !io->hasReadError && (index < *outRecordLength); index++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_TARGET_SIGNATURE_HASH`; `14:record->targetModuleSignatureHash; sizeof(TZrUInt64); line750; if:*outRecordLength > 0 & for:TZrSize index = 0; !io->hasReadError && (index < *outRecordLength); index++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_MODULE_EFFECT_TARGET_MODULE_HASH`; `15:record->requestedModuleVersion; encoded_size(io_read_string_with_length(io)); line755; if:*outRecordLength > 0 & for:TZrSize index = 0; !io->hasReadError && (index < *outRecordLength); index++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_ASSEMBLY_REF_VERSION_RANGE`; `16:record->minModuleVersionInclusive; encoded_size(io_read_string_with_length(io)); line756; if:*outRecordLength > 0 & for:TZrSize index = 0; !io->hasReadError && (index < *outRecordLength); index++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_ASSEMBLY_REF_VERSION_RANGE`; `17:record->maxModuleVersionExclusive; encoded_size(io_read_string_with_length(io)); line757; if:*outRecordLength > 0 & for:TZrSize index = 0; !io->hasReadError && (index < *outRecordLength); index++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_ASSEMBLY_REF_VERSION_RANGE`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L074 io_read_module_metadata_binding_list

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_module_metadata_binding_list`；reader；events=18；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:*outBindingLength; sizeof(TZrSize); line782; unconditional`; `1:binding->refToken; sizeof(TZrMetadataToken); line791; if:*outBindingLength > 0 & for:TZrSize index = 0; !io->hasReadError && (index < *outBindingLength); index++`; `2:binding->refSignatureToken; sizeof(TZrMetadataToken); line792; if:*outBindingLength > 0 & for:TZrSize index = 0; !io->hasReadError && (index < *outBindingLength); index++`; `3:binding->refSignatureHash; sizeof(TZrUInt64); line793; if:*outBindingLength > 0 & for:TZrSize index = 0; !io->hasReadError && (index < *outBindingLength); index++`; `4:binding->expectedMetadataToken; sizeof(TZrMetadataToken); line794; if:*outBindingLength > 0 & for:TZrSize index = 0; !io->hasReadError && (index < *outBindingLength); index++`; `5:binding->expectedSignatureToken; sizeof(TZrMetadataToken); line795; if:*outBindingLength > 0 & for:TZrSize index = 0; !io->hasReadError && (index < *outBindingLength); index++`; `6:binding->expectedSignatureHash; sizeof(TZrUInt64); line796; if:*outBindingLength > 0 & for:TZrSize index = 0; !io->hasReadError && (index < *outBindingLength); index++`; `7:binding->expectedModuleSignatureHash; sizeof(TZrUInt64); line797; if:*outBindingLength > 0 & for:TZrSize index = 0; !io->hasReadError && (index < *outBindingLength); index++`; `8:binding->expectedLayoutVersion; sizeof(TZrUInt32); line799; if:*outBindingLength > 0 & for:TZrSize index = 0; !io->hasReadError && (index < *outBindingLength); index++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_BINDING_LAYOUT_IDENTITY`; `9:binding->reserved0; sizeof(TZrUInt32); line800; if:*outBindingLength > 0 & for:TZrSize index = 0; !io->hasReadError && (index < *outBindingLength); index++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_BINDING_LAYOUT_IDENTITY`; `10:binding->expectedLayoutHash; sizeof(TZrUInt64); line801; if:*outBindingLength > 0 & for:TZrSize index = 0; !io->hasReadError && (index < *outBindingLength); index++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_BINDING_LAYOUT_IDENTITY`; `11:binding->resolvedMetadataToken; sizeof(TZrMetadataToken); line807; if:*outBindingLength > 0 & for:TZrSize index = 0; !io->hasReadError && (index < *outBindingLength); index++`; `12:binding->resolvedSignatureToken; sizeof(TZrMetadataToken); line808; if:*outBindingLength > 0 & for:TZrSize index = 0; !io->hasReadError && (index < *outBindingLength); index++`; `13:binding->resolvedSignatureHash; sizeof(TZrUInt64); line809; if:*outBindingLength > 0 & for:TZrSize index = 0; !io->hasReadError && (index < *outBindingLength); index++`; `14:binding->resolvedModuleSignatureHash; sizeof(TZrUInt64); line810; if:*outBindingLength > 0 & for:TZrSize index = 0; !io->hasReadError && (index < *outBindingLength); index++`; `15:binding->resolvedLayoutVersion; sizeof(TZrUInt32); line812; if:*outBindingLength > 0 & for:TZrSize index = 0; !io->hasReadError && (index < *outBindingLength); index++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_BINDING_LAYOUT_IDENTITY`; `16:binding->reserved1; sizeof(TZrUInt32); line813; if:*outBindingLength > 0 & for:TZrSize index = 0; !io->hasReadError && (index < *outBindingLength); index++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_BINDING_LAYOUT_IDENTITY`; `17:binding->resolvedLayoutHash; sizeof(TZrUInt64); line814; if:*outBindingLength > 0 & for:TZrSize index = 0; !io->hasReadError && (index < *outBindingLength); index++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_BINDING_LAYOUT_IDENTITY`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L075 io_read_metadata_string_heap

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_metadata_string_heap`；reader；events=3；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:*outStringHeapLength; sizeof(TZrSize); line835; unconditional`; `1:entry->stringIndex; sizeof(TZrUInt32); line848; for:TZrSize index = 0; !io->hasReadError && (index < *outStringHeapLength); index++`; `2:entry->value; encoded_size(io_read_string_with_length(io)); line849; for:TZrSize index = 0; !io->hasReadError && (index < *outStringHeapLength); index++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L076 io_read_function_metadata_token_model

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_metadata_token_model`；reader；events=9；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:arguments; encoded_size(io_read_metadata_token_record_list(io,&function->metadataTokenRecordLength,&function->metadataTokenRecords)); line877; unconditional`; `1:function->moduleVersion; encoded_size(io_read_string_with_length(io)); line881; if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_ASSEMBLY_REF_VERSION_RANGE`; `2:function->signatureBlobHeapLength; sizeof(TZrSize); line886; unconditional`; `3:function->signatureBlobHeap; function->signatureBlobHeapLength; line891; if:function->signatureBlobHeapLength > 0 & if:function->signatureBlobHeap != ZR_NULL`; `4:&discardByte; sizeof(discardByte); line895; if:function->signatureBlobHeapLength > 0 & else:NOT(function->signatureBlobHeap != ZR_NULL) & for:TZrSize index = 0; !io->hasReadError && (index < function->signatureBlobHeapLength); index++`; `5:arguments; encoded_size(io_read_metadata_string_heap(io,&function->metadataStringHeapLength,&function->metadataStringHeap)); line900; if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_STRING_HEAP`; `6:function->moduleSignatureHash; sizeof(TZrUInt64); line905; if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_MODULE_SIGNATURE_HASH`; `7:arguments; encoded_size(io_read_metadata_token_record_list(io,&function->moduleMetadataTokenRecordLength,&function->moduleMetadataTokenRecords)); line909; if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_MODULE_METADATA_REF_TABLE`; `8:arguments; encoded_size(io_read_module_metadata_binding_list(io,&function->moduleMetadataBindingLength,&function->moduleMetadataBindings)); line914; if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_MODULE_METADATA_BINDINGS`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L077 io_read_function_module_effects

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_module_effects`；reader；events=18；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:effect->kind; sizeof(TZrUInt8); line928; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `1:effect->exportKind; sizeof(TZrUInt8); line929; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `2:effect->readiness; sizeof(TZrUInt8); line930; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `3:effect->reserved0; sizeof(TZrUInt8); line931; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `4:effect->moduleName; encoded_size(io_read_string_with_length(io)); line932; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `5:effect->assemblyName; encoded_size(io_read_string_with_length(io)); line934; for:TZrSize index = 0; !io->hasReadError && (index < count); index++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_MODULE_EFFECT_ASSEMBLY_NAME`; `6:} effect->symbolName; encoded_size(io_read_string_with_length(io)); line938; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `7:effect->lineInSourceStart; sizeof(TZrUInt32); line939; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `8:effect->columnInSourceStart; sizeof(TZrUInt32); line940; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `9:effect->lineInSourceEnd; sizeof(TZrUInt32); line941; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `10:effect->columnInSourceEnd; sizeof(TZrUInt32); line942; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `11:effect->targetMetadataToken; sizeof(TZrMetadataToken); line944; for:TZrSize index = 0; !io->hasReadError && (index < count); index++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_TARGET_SIGNATURE_HASH`; `12:effect->targetSignatureToken; sizeof(TZrMetadataToken); line945; for:TZrSize index = 0; !io->hasReadError && (index < count); index++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_TARGET_SIGNATURE_HASH`; `13:effect->targetSignatureHash; sizeof(TZrUInt64); line946; for:TZrSize index = 0; !io->hasReadError && (index < count); index++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_TARGET_SIGNATURE_HASH`; `14:effect->targetModuleSignatureHash; sizeof(TZrUInt64); line948; for:TZrSize index = 0; !io->hasReadError && (index < count); index++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_TARGET_SIGNATURE_HASH & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_MODULE_EFFECT_TARGET_MODULE_HASH`; `15:effect->requestedModuleVersion; encoded_size(io_read_string_with_length(io)); line953; for:TZrSize index = 0; !io->hasReadError && (index < count); index++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_TARGET_SIGNATURE_HASH & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_ASSEMBLY_REF_VERSION_RANGE`; `16:effect->minModuleVersionInclusive; encoded_size(io_read_string_with_length(io)); line954; for:TZrSize index = 0; !io->hasReadError && (index < count); index++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_TARGET_SIGNATURE_HASH & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_ASSEMBLY_REF_VERSION_RANGE`; `17:effect->maxModuleVersionExclusive; encoded_size(io_read_string_with_length(io)); line955; for:TZrSize index = 0; !io->hasReadError && (index < count); index++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_METADATA_TARGET_SIGNATURE_HASH & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_ASSEMBLY_REF_VERSION_RANGE`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L078 io_read_function_callable_summaries

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_callable_summaries`；reader；events=4；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:summary->name; encoded_size(io_read_string_with_length(io)); line981; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `1:summary->callableChildIndex; sizeof(TZrUInt32); line982; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `2:summary->effectCount; sizeof(TZrSize); line983; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `3:arguments; encoded_size(io_read_function_module_effects(io,summary->effects,summary->effectCount)); line988; for:TZrSize index = 0; !io->hasReadError && (index < count); index++ & if:summary->effectCount > 0 & if:summary->effects != ZR_NULL`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L079 io_read_function_top_level_callable_bindings

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_top_level_callable_bindings`；reader；events=7；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:binding->name; encoded_size(io_read_string_with_length(io)); line1002; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `1:binding->stackSlot; sizeof(TZrUInt32); line1003; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `2:binding->callableChildIndex; sizeof(TZrUInt32); line1004; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `3:binding->accessModifier; sizeof(TZrUInt8); line1005; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `4:binding->exportKind; sizeof(TZrUInt8); line1006; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `5:binding->readiness; sizeof(TZrUInt8); line1007; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `6:binding->reserved0; sizeof(TZrUInt8); line1008; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L080 io_read_function_metadata_parameters

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_metadata_parameters`；reader；events=9；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:count; sizeof(TZrSize); line1022; unconditional`; `1:(*outParameters)[index].name; encoded_size(io_read_string_with_length(io)); line1036; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `2:arguments; encoded_size(io_read_function_typed_type_ref(io,&(*outParameters)[index].type)); line1037; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `3:(*outParameters)[index].hasDefaultValue; sizeof(TZrUInt8); line1039; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `4:arguments; encoded_size(io_read_function_constant_variables(io,&(*outParameters)[index].defaultValue,1)); line1041; for:TZrSize index = 0; !io->hasReadError && (index < count); index++ & if:(*outParameters)[index].hasDefaultValue`; `5:(*outParameters)[index].hasDecoratorMetadata; sizeof(TZrUInt8); line1052; for:TZrSize index = 0; !io->hasReadError && (index < count); index++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_PARAMETER_METADATA`; `6:arguments; encoded_size(io_read_function_constant_variables(io,&(*outParameters)[index].decoratorMetadataValue,1)); line1054; for:TZrSize index = 0; !io->hasReadError && (index < count); index++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_PARAMETER_METADATA & if:(*outParameters)[index].hasDecoratorMetadata`; `7:(*outParameters)[index].decoratorNamesLength; sizeof(TZrSize); line1057; for:TZrSize index = 0; !io->hasReadError && (index < count); index++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_PARAMETER_METADATA`; `8:(*outParameters)[index].decoratorNames[decoratorIndex]; encoded_size(io_read_string_with_length(io)); line1065; for:TZrSize index = 0; !io->hasReadError && (index < count); index++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_PARAMETER_METADATA & if:(*outParameters)[index].decoratorNamesLength > 0 & if:(*outParameters)[index].decoratorNames != ZR_NULL & for:TZrSize decoratorIndex = 0; !io->hasReadError && (decoratorIndex < (*outParameters)[index].decoratorNamesLength); decoratorIndex++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L081 io_read_function_compile_time_variable_infos

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_compile_time_variable_infos`；reader；events=8；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:infos[index].name; encoded_size(io_read_string_with_length(io)); line1080; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `1:arguments; encoded_size(io_read_function_typed_type_ref(io,&infos[index].type)); line1081; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `2:infos[index].lineInSourceStart; sizeof(TZrUInt32); line1082; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `3:infos[index].lineInSourceEnd; sizeof(TZrUInt32); line1083; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `4:infos[index].pathBindingsLength; sizeof(TZrSize); line1087; for:TZrSize index = 0; !io->hasReadError && (index < count); index++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_COMPILE_TIME_PATH_BINDINGS`; `5:SZrString *path; encoded_size(io_read_string_with_length(io)); line1094; for:TZrSize index = 0; !io->hasReadError && (index < count); index++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_COMPILE_TIME_PATH_BINDINGS & if:infos[index].pathBindingsLength > 0 & for:TZrSize bindingIndex = 0; !io->hasReadError && (bindingIndex < infos[index].pathBindingsLength); bindingIndex++`; `6:targetKind; sizeof(TZrUInt8); line1098; for:TZrSize index = 0; !io->hasReadError && (index < count); index++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_COMPILE_TIME_PATH_BINDINGS & if:infos[index].pathBindingsLength > 0 & for:TZrSize bindingIndex = 0; !io->hasReadError && (bindingIndex < infos[index].pathBindingsLength); bindingIndex++`; `7:targetName; encoded_size(io_read_string_with_length(io)); line1099; for:TZrSize index = 0; !io->hasReadError && (index < count); index++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_COMPILE_TIME_PATH_BINDINGS & if:infos[index].pathBindingsLength > 0 & for:TZrSize bindingIndex = 0; !io->hasReadError && (bindingIndex < infos[index].pathBindingsLength); bindingIndex++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L082 io_read_function_compile_time_function_infos

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_compile_time_function_infos`；reader；events=5；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:infos[index].name; encoded_size(io_read_string_with_length(io)); line1115; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `1:arguments; encoded_size(io_read_function_typed_type_ref(io,&infos[index].returnType)); line1116; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `2:arguments; encoded_size(io_read_function_metadata_parameters(io,&infos[index].parameters,&infos[index].parameterCount)); line1117; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `3:infos[index].lineInSourceStart; sizeof(TZrUInt32); line1118; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `4:infos[index].lineInSourceEnd; sizeof(TZrUInt32); line1119; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L083 io_read_function_escape_bindings

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_escape_bindings`；reader；events=7；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:binding->name; encoded_size(io_read_string_with_length(io)); line1130; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `1:binding->slotOrIndex; sizeof(TZrUInt32); line1131; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `2:binding->scopeDepth; sizeof(TZrUInt32); line1132; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `3:binding->escapeFlags; sizeof(TZrUInt32); line1133; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `4:binding->bindingKind; sizeof(TZrUInt8); line1134; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `5:binding->reserved0; sizeof(TZrUInt8); line1135; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `6:binding->reserved1; sizeof(TZrUInt16); line1136; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L084 io_read_function_decorator_metadata

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_decorator_metadata`；reader；events=4；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:function->hasDecoratorMetadata; sizeof(TZrUInt8); line1157; unconditional`; `1:arguments; encoded_size(io_read_function_constant_variables(io,&function->decoratorMetadataValue,1)); line1159; if:function->hasDecoratorMetadata`; `2:function->decoratorNamesLength; sizeof(TZrSize); line1162; unconditional`; `3:function->decoratorNames[index]; encoded_size(io_read_string_with_length(io)); line1169; if:function->decoratorNamesLength > 0 & if:function->decoratorNames != ZR_NULL & for:TZrSize index = 0; !io->hasReadError && (index < function->decoratorNamesLength); index++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L085 io_read_function_member_entries

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_member_entries`；reader；events=6；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:entries[index].symbol; encoded_size(io_read_string_with_length(io)); line1179; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `1:entries[index].entryKind; sizeof(TZrUInt8); line1180; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `2:entries[index].reserved0; sizeof(TZrUInt8); line1181; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `3:entries[index].reserved1; sizeof(TZrUInt16); line1182; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `4:entries[index].prototypeIndex; sizeof(TZrUInt32); line1183; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `5:entries[index].descriptorIndex; sizeof(TZrUInt32); line1184; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L086 io_read_function_semir_type_table

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_semir_type_table`；reader；events=1；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:arguments; encoded_size(io_read_function_typed_type_ref(io,&typeTable[index])); line1192; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L087 io_read_function_semir_ownership_table

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_semir_ownership_table`；reader；events=1；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:ownershipTable[index].state; sizeof(TZrUInt32); line1200; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L088 io_read_function_semir_effect_table

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_semir_effect_table`；reader；events=4；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:effectTable[index].kind; sizeof(TZrUInt32); line1208; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `1:effectTable[index].instructionIndex; sizeof(TZrUInt32); line1209; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `2:effectTable[index].ownershipInputIndex; sizeof(TZrUInt32); line1210; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `3:effectTable[index].ownershipOutputIndex; sizeof(TZrUInt32); line1211; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L089 io_read_function_semir_block_table

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_semir_block_table`；reader；events=3；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:blockTable[index].blockId; sizeof(TZrUInt32); line1219; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `1:blockTable[index].firstInstructionIndex; sizeof(TZrUInt32); line1220; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `2:blockTable[index].instructionCount; sizeof(TZrUInt32); line1221; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L090 io_read_function_semir_instruction_table

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_semir_instruction_table`；reader；events=8；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:instructionTable[index].opcode; sizeof(TZrUInt32); line1229; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `1:instructionTable[index].execInstructionIndex; sizeof(TZrUInt32); line1230; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `2:instructionTable[index].typeTableIndex; sizeof(TZrUInt32); line1231; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `3:instructionTable[index].effectTableIndex; sizeof(TZrUInt32); line1232; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `4:instructionTable[index].destinationSlot; sizeof(TZrUInt32); line1233; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `5:instructionTable[index].operand0; sizeof(TZrUInt32); line1234; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `6:instructionTable[index].operand1; sizeof(TZrUInt32); line1235; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `7:instructionTable[index].deoptId; sizeof(TZrUInt32); line1236; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L091 io_read_function_semir_deopt_table

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_semir_deopt_table`；reader；events=2；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:deoptTable[index].deoptId; sizeof(TZrUInt32); line1244; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `1:deoptTable[index].execInstructionIndex; sizeof(TZrUInt32); line1245; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L092 io_read_function_callsite_cache_table

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_callsite_cache_table`；reader；events=5；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:cacheTable[index].kind; sizeof(TZrUInt32); line1253; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `1:cacheTable[index].instructionIndex; sizeof(TZrUInt32); line1254; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `2:cacheTable[index].memberEntryIndex; sizeof(TZrUInt32); line1255; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `3:cacheTable[index].deoptId; sizeof(TZrUInt32); line1256; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`; `4:cacheTable[index].argumentCount; sizeof(TZrUInt32); line1257; for:TZrSize index = 0; !io->hasReadError && (index < count); index++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L093 io_read_function_semir_metadata

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_semir_metadata`；reader；events=12；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:function->semIrTypeTableLength; sizeof(TZrSize); line1268; unconditional`; `1:arguments; encoded_size(io_read_function_semir_type_table(io,function->semIrTypeTable,function->semIrTypeTableLength)); line1273; if:function->semIrTypeTableLength > 0 & if:function->semIrTypeTable != ZR_NULL`; `2:function->semIrOwnershipTableLength; sizeof(TZrSize); line1277; unconditional`; `3:arguments; encoded_size(io_read_function_semir_ownership_table(io,function->semIrOwnershipTable,function->semIrOwnershipTableLength)); line1282; if:function->semIrOwnershipTableLength > 0 & if:function->semIrOwnershipTable != ZR_NULL`; `4:function->semIrEffectTableLength; sizeof(TZrSize); line1288; unconditional`; `5:arguments; encoded_size(io_read_function_semir_effect_table(io,function->semIrEffectTable,function->semIrEffectTableLength)); line1293; if:function->semIrEffectTableLength > 0 & if:function->semIrEffectTable != ZR_NULL`; `6:function->semIrBlockTableLength; sizeof(TZrSize); line1297; unconditional`; `7:arguments; encoded_size(io_read_function_semir_block_table(io,function->semIrBlockTable,function->semIrBlockTableLength)); line1302; if:function->semIrBlockTableLength > 0 & if:function->semIrBlockTable != ZR_NULL`; `8:function->semIrInstructionLength; sizeof(TZrSize); line1306; unconditional`; `9:arguments; encoded_size(io_read_function_semir_instruction_table(io,function->semIrInstructions,function->semIrInstructionLength)); line1311; if:function->semIrInstructionLength > 0 & if:function->semIrInstructions != ZR_NULL`; `10:function->semIrDeoptTableLength; sizeof(TZrSize); line1317; unconditional`; `11:arguments; encoded_size(io_read_function_semir_deopt_table(io,function->semIrDeoptTable,function->semIrDeoptTableLength)); line1322; if:function->semIrDeoptTableLength > 0 & if:function->semIrDeoptTable != ZR_NULL`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L094 io_read_function_callsite_cache_metadata

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_callsite_cache_metadata`；reader；events=2；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:function->callSiteCacheLength; sizeof(TZrSize); line1334; unconditional`; `1:arguments; encoded_size(io_read_function_callsite_cache_table(io,function->callSiteCaches,function->callSiteCacheLength)); line1341; if:function->callSiteCacheLength > 0 & if:function->callSiteCaches != ZR_NULL`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L095 io_read_ffi_type_contract

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_ffi_type_contract`；reader；events=8；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:typeKind; sizeof(TZrUInt32); line1351; unconditional`; `1:type->size; sizeof(TZrUInt32); line1353; unconditional`; `2:type->alignment; sizeof(TZrUInt32); line1354; unconditional`; `3:type->canonicalTypeHash; sizeof(TZrUInt64); line1355; unconditional`; `4:type->layoutHash; sizeof(TZrUInt64); line1356; unconditional`; `5:type->flags; sizeof(TZrUInt32); line1357; unconditional`; `6:type->aggregateFieldStart; sizeof(TZrUInt32); line1358; unconditional`; `7:type->aggregateFieldCount; sizeof(TZrUInt32); line1359; unconditional`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L096 io_read_ffi_callable_contract

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_ffi_callable_contract`；reader；events=13；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:callable->parameterCount; sizeof(TZrUInt32); line1369; unconditional`; `1:parameter->canonicalTypeHash; sizeof(TZrUInt64); line1384; for:TZrUInt32 index = 0u; !io->hasReadError && (index < callable->parameterCount); index++`; `2:passingForm; sizeof(TZrUInt32); line1385; for:TZrUInt32 index = 0u; !io->hasReadError && (index < callable->parameterCount); index++`; `3:escapeUpperBound; sizeof(TZrUInt32); line1387; for:TZrUInt32 index = 0u; !io->hasReadError && (index < callable->parameterCount); index++`; `4:entryInitialization; sizeof(TZrUInt32); line1390; for:TZrUInt32 index = 0u; !io->hasReadError && (index < callable->parameterCount); index++`; `5:exitInitialization; sizeof(TZrUInt32); line1393; for:TZrUInt32 index = 0u; !io->hasReadError && (index < callable->parameterCount); index++`; `6:acceptsTemporary; sizeof(TZrUInt8); line1396; for:TZrUInt32 index = 0u; !io->hasReadError && (index < callable->parameterCount); index++`; `7:callSiteMarker; sizeof(TZrUInt32); line1398; for:TZrUInt32 index = 0u; !io->hasReadError && (index < callable->parameterCount); index++`; `8:callable->returnTypeHash; sizeof(TZrUInt64); line1402; unconditional`; `9:receiverEffect; sizeof(TZrUInt32); line1403; unconditional`; `10:callable->effectFlags; sizeof(TZrUInt32); line1405; unconditional`; `11:isVariadic; sizeof(TZrUInt8); line1406; unconditional`; `12:callable->contractHash; sizeof(TZrUInt64); line1408; unconditional`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L097 io_read_function_native_import_contracts

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_native_import_contracts`；reader；events=43；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:serializedContractCount; sizeof(TZrUInt64); line1417; unconditional`; `1:contract->schemaVersion; sizeof(TZrUInt32); line1452; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++`; `2:(TZrBytePtr)contract->libraryLocator; sizeof(contract->libraryLocator); line1453; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++`; `3:(TZrBytePtr)contract->entryPoint; sizeof(contract->entryPoint); line1454; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++`; `4:contract->symbolId; sizeof(TZrUInt64); line1455; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++`; `5:contract->declaringModuleId; sizeof(TZrUInt64); line1456; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++`; `6:arguments; encoded_size(io_read_ffi_callable_contract(io,&contract->callable)); line1457; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++`; `7:contract->availability; sizeof(TZrUInt32); line1461; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++`; `8:contract->requiredCapabilities; sizeof(TZrUInt64); line1462; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++`; `9:(TZrBytePtr)contract->sourceMapping.document; sizeof(contract->sourceMapping.document); line1463; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++`; `10:contract->sourceMapping.startOffset; sizeof(TZrUInt64); line1467; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++`; `11:contract->sourceMapping.endOffset; sizeof(TZrUInt64); line1469; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++`; `12:contract->sourceMapping.startLine; sizeof(TZrInt32); line1471; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++`; `13:contract->sourceMapping.startColumn; sizeof(TZrInt32); line1473; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++`; `14:contract->sourceMapping.endLine; sizeof(TZrInt32); line1475; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++`; `15:contract->sourceMapping.endColumn; sizeof(TZrInt32); line1477; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++`; `16:abi; sizeof(TZrUInt32); line1479; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++`; `17:contract->signature.targetPointerSize; sizeof(TZrUInt32); line1481; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++`; `18:targetEndianness; sizeof(TZrUInt32); line1482; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++`; `19:(TZrBytePtr)contract->signature.targetTriple; sizeof(contract->signature.targetTriple); line1485; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++`; `20:contract->signature.targetAbiHash; sizeof(TZrUInt64); line1489; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++`; `21:charset; sizeof(TZrUInt32); line1490; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++`; `22:errorPolicy; sizeof(TZrUInt32); line1492; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++`; `23:cleanupPolicy; sizeof(TZrUInt32); line1494; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++`; `24:callbackLifetime; sizeof(TZrUInt32); line1496; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++`; `25:callbackThreadPolicy; sizeof(TZrUInt32); line1499; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++`; `26:callbackExceptionPolicy; sizeof(TZrUInt32); line1502; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++`; `27:isVariadic; sizeof(TZrUInt8); line1505; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++`; `28:contract->signature.parameterCount; sizeof(TZrUInt32); line1507; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++`; `29:arguments; encoded_size(io_read_ffi_type_contract(io,&contract->signature.returnType)); line1513; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++`; `30:arguments; encoded_size(io_read_ffi_type_contract(io,&parameter->type)); line1526; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++ & for:TZrUInt32 parameterIndex = 0u; !io->hasReadError && (parameterIndex < contract->signature.parameterCount); parameterIndex++`; `31:direction; sizeof(TZrUInt32); line1527; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++ & for:TZrUInt32 parameterIndex = 0u; !io->hasReadError && (parameterIndex < contract->signature.parameterCount); parameterIndex++`; `32:marshalling; sizeof(TZrUInt32); line1529; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++ & for:TZrUInt32 parameterIndex = 0u; !io->hasReadError && (parameterIndex < contract->signature.parameterCount); parameterIndex++`; `33:ownership; sizeof(TZrUInt32); line1531; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++ & for:TZrUInt32 parameterIndex = 0u; !io->hasReadError && (parameterIndex < contract->signature.parameterCount); parameterIndex++`; `34:isNullable; sizeof(TZrUInt8); line1533; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++ & for:TZrUInt32 parameterIndex = 0u; !io->hasReadError && (parameterIndex < contract->signature.parameterCount); parameterIndex++`; `35:parameter->flags; sizeof(TZrUInt32); line1535; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++ & for:TZrUInt32 parameterIndex = 0u; !io->hasReadError && (parameterIndex < contract->signature.parameterCount); parameterIndex++`; `36:contract->signature.aggregateFieldCount; sizeof(TZrUInt32); line1537; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++`; `37:(TZrBytePtr)field->name; sizeof(field->name); line1550; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++ & for:TZrUInt32 fieldIndex = 0u; !io->hasReadError && (fieldIndex < contract->signature.aggregateFieldCount); fieldIndex++`; `38:fieldTypeKind; sizeof(TZrUInt32); line1552; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++ & for:TZrUInt32 fieldIndex = 0u; !io->hasReadError && (fieldIndex < contract->signature.aggregateFieldCount); fieldIndex++`; `39:field->size; sizeof(TZrUInt32); line1554; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++ & for:TZrUInt32 fieldIndex = 0u; !io->hasReadError && (fieldIndex < contract->signature.aggregateFieldCount); fieldIndex++`; `40:field->alignment; sizeof(TZrUInt32); line1555; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++ & for:TZrUInt32 fieldIndex = 0u; !io->hasReadError && (fieldIndex < contract->signature.aggregateFieldCount); fieldIndex++`; `41:field->offset; sizeof(TZrUInt32); line1556; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++ & for:TZrUInt32 fieldIndex = 0u; !io->hasReadError && (fieldIndex < contract->signature.aggregateFieldCount); fieldIndex++`; `42:contract->signature.signatureHash; sizeof(TZrUInt64); line1558; for:TZrSize contractIndex = 0u; !io->hasReadError && (contractIndex < function->nativeImportContractLength); contractIndex++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L098 io_read_function_closures

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_closures`；reader；events=1；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:arguments; encoded_size(io_read_functions(io,closure->subFunction,1)); line1571; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L099 io_read_function_debug_infos

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_debug_infos`；reader；events=10；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:debugInfo->sourceFile; encoded_size(io_read_string_with_length(io)); line1584; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_SOURCE_IDENTITY`; `1:debugInfo->sourceHash; encoded_size(io_read_string_with_length(io)); line1585; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_SOURCE_IDENTITY`; `2:debugInfo->instructionsLength; sizeof(TZrSize); line1587; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `3:(TZrBytePtr) debugInfo->instructionsLine; sizeof(TZrUInt64) * debugInfo->instructionsLength; line1594; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:debugInfo->instructionsLength > 0 & if:debugInfo->instructionsLine != ZR_NULL`; `4:ignoredLine; sizeof(TZrUInt64); line1600; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:debugInfo->instructionsLength > 0 & else:NOT(debugInfo->instructionsLine != ZR_NULL) & for:TZrSize index = 0; !io->hasReadError && (index < debugInfo->instructionsLength); index++`; `5:(TZrBytePtr) debugInfo->instructionRanges; sizeof(SZrIoInstructionSourceRange) * debugInfo->instructionsLength; line1610; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_SOURCE_RANGES && debugInfo->instructionsLength > 0 & if:debugInfo->instructionRanges != ZR_NULL`; `6:ignoredRange.startLine; sizeof(TZrUInt32); line1616; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_SOURCE_RANGES && debugInfo->instructionsLength > 0 & else:NOT(debugInfo->instructionRanges != ZR_NULL) & for:TZrSize index = 0; !io->hasReadError && (index < debugInfo->instructionsLength); index++`; `7:ignoredRange.startColumn; sizeof(TZrUInt32); line1617; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_SOURCE_RANGES && debugInfo->instructionsLength > 0 & else:NOT(debugInfo->instructionRanges != ZR_NULL) & for:TZrSize index = 0; !io->hasReadError && (index < debugInfo->instructionsLength); index++`; `8:ignoredRange.endLine; sizeof(TZrUInt32); line1618; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_SOURCE_RANGES && debugInfo->instructionsLength > 0 & else:NOT(debugInfo->instructionRanges != ZR_NULL) & for:TZrSize index = 0; !io->hasReadError && (index < debugInfo->instructionsLength); index++`; `9:ignoredRange.endColumn; sizeof(TZrUInt32); line1619; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_SOURCE_RANGES && debugInfo->instructionsLength > 0 & else:NOT(debugInfo->instructionRanges != ZR_NULL) & for:TZrSize index = 0; !io->hasReadError && (index < debugInfo->instructionsLength); index++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L100 io_read_function_frame_layout

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_frame_layout`；reader；events=19；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:function->frameByteSize; sizeof(TZrUInt32); line1633; unconditional`; `1:function->frameByteAlign; sizeof(TZrUInt32); line1634; unconditional`; `2:function->frameSlotLayoutsLength; sizeof(TZrSize); line1635; unconditional`; `3:ignoredLayout.stackSlot; sizeof(TZrUInt32); line1647; if:function->frameSlotLayouts == ZR_NULL & for:TZrSize index = 0; !io->hasReadError && (index < function->frameSlotLayoutsLength); index++`; `4:ignoredLayout.byteOffset; sizeof(TZrUInt32); line1648; if:function->frameSlotLayouts == ZR_NULL & for:TZrSize index = 0; !io->hasReadError && (index < function->frameSlotLayoutsLength); index++`; `5:ignoredLayout.byteSize; sizeof(TZrUInt32); line1649; if:function->frameSlotLayouts == ZR_NULL & for:TZrSize index = 0; !io->hasReadError && (index < function->frameSlotLayoutsLength); index++`; `6:ignoredLayout.byteAlign; sizeof(TZrUInt32); line1650; if:function->frameSlotLayouts == ZR_NULL & for:TZrSize index = 0; !io->hasReadError && (index < function->frameSlotLayoutsLength); index++`; `7:ignoredLayout.typeLayoutId; sizeof(TZrUInt32); line1651; if:function->frameSlotLayouts == ZR_NULL & for:TZrSize index = 0; !io->hasReadError && (index < function->frameSlotLayoutsLength); index++`; `8:ignoredLayout.slotKind; sizeof(TZrUInt8); line1652; if:function->frameSlotLayouts == ZR_NULL & for:TZrSize index = 0; !io->hasReadError && (index < function->frameSlotLayoutsLength); index++`; `9:ignoredLayout.isParameter; sizeof(TZrUInt8); line1653; if:function->frameSlotLayouts == ZR_NULL & for:TZrSize index = 0; !io->hasReadError && (index < function->frameSlotLayoutsLength); index++`; `10:ignoredLayout.reserved0; sizeof(TZrUInt16); line1654; if:function->frameSlotLayouts == ZR_NULL & for:TZrSize index = 0; !io->hasReadError && (index < function->frameSlotLayoutsLength); index++`; `11:layout->stackSlot; sizeof(TZrUInt32); line1662; for:TZrSize index = 0; !io->hasReadError && (index < function->frameSlotLayoutsLength); index++`; `12:layout->byteOffset; sizeof(TZrUInt32); line1663; for:TZrSize index = 0; !io->hasReadError && (index < function->frameSlotLayoutsLength); index++`; `13:layout->byteSize; sizeof(TZrUInt32); line1664; for:TZrSize index = 0; !io->hasReadError && (index < function->frameSlotLayoutsLength); index++`; `14:layout->byteAlign; sizeof(TZrUInt32); line1665; for:TZrSize index = 0; !io->hasReadError && (index < function->frameSlotLayoutsLength); index++`; `15:layout->typeLayoutId; sizeof(TZrUInt32); line1666; for:TZrSize index = 0; !io->hasReadError && (index < function->frameSlotLayoutsLength); index++`; `16:layout->slotKind; sizeof(TZrUInt8); line1667; for:TZrSize index = 0; !io->hasReadError && (index < function->frameSlotLayoutsLength); index++`; `17:layout->isParameter; sizeof(TZrUInt8); line1668; for:TZrSize index = 0; !io->hasReadError && (index < function->frameSlotLayoutsLength); index++`; `18:layout->reserved0; sizeof(TZrUInt16); line1669; for:TZrSize index = 0; !io->hasReadError && (index < function->frameSlotLayoutsLength); index++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L101 io_read_functions

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_functions`；reader；events=69；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:function->name; encoded_size(io_read_string_with_length(io)); line1682; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `1:function->startLine; sizeof(TZrUInt64); line1683; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `2:function->endLine; sizeof(TZrUInt64); line1684; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `3:function->parametersLength; sizeof(TZrSize); line1685; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `4:function->hasVarArgs; sizeof(TZrUInt64); line1686; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `5:function->stackSize; sizeof(TZrUInt32); line1687; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `6:function->vmEntryClearStackSizePlusOne; sizeof(TZrUInt32); line1691; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_VM_ENTRY_CLEAR_STACK_SIZE`; `7:arguments; encoded_size(io_read_function_frame_layout(io,function)); line1700; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_FRAME_LAYOUT`; `8:function->instructionsLength; sizeof(TZrSize); line1702; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `9:(TZrBytePtr) function->instructions; sizeof(TZrInstruction) * function->instructionsLength; line1712; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:function->instructionsLength > 0`; `10:function->localVariablesLength; sizeof(TZrSize); line1716; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `11:arguments; encoded_size(io_read_function_local_variables(io,function->localVariables,function->localVariablesLength)); line1720; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:function->localVariablesLength > 0`; `12:function->closureVariablesLength; sizeof(TZrSize); line1724; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `13:arguments; encoded_size(io_read_function_closure_variables(io,function->closureVariables,function->closureVariablesLength)); line1729; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:function->closureVariablesLength > 0 & if:function->closureVariables != ZR_NULL`; `14:function->catchClauseCount; sizeof(TZrSize); line1734; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `15:arguments; encoded_size(io_read_function_catch_clauses(io,function->catchClauses,function->catchClauseCount)); line1739; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:function->catchClauseCount > 0 & if:function->catchClauses != ZR_NULL`; `16:function->exceptionHandlerCount; sizeof(TZrSize); line1744; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `17:arguments; encoded_size(io_read_function_exception_handlers(io,function->exceptionHandlers,function->exceptionHandlerCount)); line1749; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:function->exceptionHandlerCount > 0 & if:function->exceptionHandlers != ZR_NULL`; `18:function->constantVariablesLength; sizeof(TZrSize); line1756; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `19:arguments; encoded_size(io_read_function_constant_variables(io,function->constantVariables,function->constantVariablesLength)); line1760; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:function->constantVariablesLength > 0`; `20:function->exportedVariablesLength; sizeof(TZrSize); line1764; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `21:arguments; encoded_size(io_read_function_exported_variables(io,function->exportedVariables,function->exportedVariablesLength)); line1768; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:function->exportedVariablesLength > 0`; `22:function->typedLocalBindingsLength; sizeof(TZrSize); line1772; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `23:arguments; encoded_size(io_read_function_typed_local_bindings(io,function->typedLocalBindings,function->typedLocalBindingsLength)); line1776; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:function->typedLocalBindingsLength > 0`; `24:function->typedClosureBindingsLength; sizeof(TZrSize); line1784; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_TYPED_CLOSURE_CAPTURE_BINDINGS`; `25:arguments; encoded_size(io_read_function_typed_closure_bindings(io,function->typedClosureBindings,function->typedClosureBindingsLength)); line1788; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_TYPED_CLOSURE_CAPTURE_BINDINGS & if:function->typedClosureBindingsLength > 0`; `26:function->typedExportedSymbolsLength; sizeof(TZrSize); line1798; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `27:arguments; encoded_size(io_read_function_typed_export_symbols(io,function->typedExportedSymbols,function->typedExportedSymbolsLength)); line1802; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:function->typedExportedSymbolsLength > 0`; `28:arguments; encoded_size(io_read_function_metadata_token_model(io,function)); line1808; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `29:function->staticImportsLength; sizeof(TZrSize); line1809; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `30:function->staticImports[index]; encoded_size(io_read_string_with_length(io)); line1815; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:function->staticImportsLength > 0 & if:function->staticImports != ZR_NULL & for:TZrSize index = 0; !io->hasReadError && (index < function->staticImportsLength); index++`; `31:function->moduleEntryEffectsLength; sizeof(TZrSize); line1821; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `32:arguments; encoded_size(io_read_function_module_effects(io,function->moduleEntryEffects,function->moduleEntryEffectsLength)); line1826; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:function->moduleEntryEffectsLength > 0 & if:function->moduleEntryEffects != ZR_NULL`; `33:function->exportedCallableSummariesLength; sizeof(TZrSize); line1833; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `34:arguments; encoded_size(io_read_function_callable_summaries(io,function->exportedCallableSummaries,function->exportedCallableSummariesLength)); line1838; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:function->exportedCallableSummariesLength > 0 & if:function->exportedCallableSummaries != ZR_NULL`; `35:function->topLevelCallableBindingsLength; sizeof(TZrSize); line1845; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `36:arguments; encoded_size(io_read_function_top_level_callable_bindings(io,function->topLevelCallableBindings,function->topLevelCallableBindingsLength)); line1850; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:function->topLevelCallableBindingsLength > 0 & if:function->topLevelCallableBindings != ZR_NULL`; `37:arguments; encoded_size(io_read_function_metadata_parameters(io,&function->parameterMetadata,&function->parameterMetadataLength)); line1901; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_PARAMETER_METADATA`; `38:function->hasCallableReturnType; sizeof(TZrUInt8); line1904; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_CALLABLE_RETURN_TYPE`; `39:arguments; encoded_size(io_read_function_typed_type_ref(io,&function->callableReturnType)); line1906; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_CALLABLE_RETURN_TYPE & if:function->hasCallableReturnType`; `40:function->compileTimeVariableInfosLength; sizeof(TZrSize); line1910; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_COMPILE_TIME_METADATA`; `41:arguments; encoded_size(io_read_function_compile_time_variable_infos(io,function->compileTimeVariableInfos,function->compileTimeVariableInfosLength)); line1916; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_COMPILE_TIME_METADATA & if:function->compileTimeVariableInfosLength > 0 & if:function->compileTimeVariableInfos != ZR_NULL`; `42:function->compileTimeFunctionInfosLength; sizeof(TZrSize); line1921; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_COMPILE_TIME_METADATA`; `43:arguments; encoded_size(io_read_function_compile_time_function_infos(io,function->compileTimeFunctionInfos,function->compileTimeFunctionInfosLength)); line1927; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_COMPILE_TIME_METADATA & if:function->compileTimeFunctionInfosLength > 0 & if:function->compileTimeFunctionInfos != ZR_NULL`; `44:function->escapeBindingLength; sizeof(TZrSize); line1933; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_COMPILE_TIME_METADATA & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_ESCAPE_METADATA`; `45:arguments; encoded_size(io_read_function_escape_bindings(io,function->escapeBindings,function->escapeBindingLength)); line1939; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_COMPILE_TIME_METADATA & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_ESCAPE_METADATA & if:function->escapeBindingLength > 0 & if:function->escapeBindings != ZR_NULL`; `46:function->returnEscapeSlotCount; sizeof(TZrSize); line1944; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_COMPILE_TIME_METADATA & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_ESCAPE_METADATA`; `47:(TZrBytePtr)function->returnEscapeSlots; sizeof(TZrUInt32) * function->returnEscapeSlotCount; line1950; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_COMPILE_TIME_METADATA & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_ESCAPE_METADATA & if:function->returnEscapeSlotCount > 0 & if:function->returnEscapeSlots != ZR_NULL`; `48:serializedLength; sizeof(TZrUInt64); line1959; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_COMPILE_TIME_METADATA & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_TEST_MANIFEST`; `49:function->testManifestData; function->testManifestDataLength; line1971; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_COMPILE_TIME_METADATA & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_TEST_MANIFEST & if:serializedLength > 0U & else:NOT(function->testManifestData == ZR_NULL)`; `50:removedLegacyTestInfoCount; sizeof(TZrUInt64); line1979; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_COMPILE_TIME_METADATA & else:NOT(io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_TEST_MANIFEST)`; `51:arguments; encoded_size(io_read_function_decorator_metadata(io,function)); line1987; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_DECORATOR_METADATA`; `52:function->memberEntriesLength; sizeof(TZrSize); line1990; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_MEMBER_ENTRIES`; `53:arguments; encoded_size(io_read_function_member_entries(io,function->memberEntries,function->memberEntriesLength)); line1996; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_MEMBER_ENTRIES & if:function->memberEntriesLength > 0 & if:function->memberEntries != ZR_NULL`; `54:arguments; encoded_size(io_read_function_semir_metadata(io,function)); line2001; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_SEMIR_METADATA`; `55:arguments; encoded_size(io_read_function_callsite_cache_metadata(io,function)); line2004; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_CALLSITE_CACHE`; `56:} if (io->sourceVersionPatch >; encoded_size(ZrCore_Io_ReadCallBindings(io,function)); line2007; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `57:arguments; encoded_size(io_read_function_native_import_contracts(io,function)); line2012; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_NATIVE_IMPORT_CONTRACTS`; `58:function->prototypesLength; sizeof(TZrSize); line2015; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `59:classCount; sizeof(TZrSize); line2025; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `60:arguments; encoded_size(io_read_classes(io,function->classes,classCount)); line2032; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:classCount > 0 & if:function->classes != ZR_NULL`; `61:structCount; sizeof(TZrSize); line2036; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `62:arguments; encoded_size(io_read_structs(io,function->structs,structCount)); line2042; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:structCount > 0 & if:function->structs != ZR_NULL`; `63:function->prototypeDataLength; sizeof(TZrSize); line2046; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_PROTOTYPE_BLOB`; `64:function->prototypeData; function->prototypeDataLength; line2051; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_PROTOTYPE_BLOB & if:function->prototypeDataLength > 0 & if:function->prototypeData != ZR_NULL`; `65:function->closuresLength; sizeof(TZrSize); line2055; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `66:arguments; encoded_size(io_read_function_closures(io,function->closures,function->closuresLength)); line2059; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:function->closuresLength > 0`; `67:function->debugInfosLength; sizeof(TZrSize); line2063; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `68:arguments; encoded_size(io_read_function_debug_infos(io,function->debugInfos,function->debugInfosLength)); line2068; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & if:function->debugInfosLength > 0`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L102 io_read_method

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_method`；reader；events=3；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:method->name; encoded_size(io_read_string_with_length(io)); line2080; unconditional`; `1:method->functionsLength; sizeof(TZrSize); line2081; unconditional`; `2:arguments; encoded_size(io_read_functions(io,method->functions,method->functionsLength)); line2084; unconditional`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L103 io_read_property

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_property`；reader；events=4；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:property->name; encoded_size(io_read_string_with_length(io)); line2089; unconditional`; `1:property->propertyType; sizeof(TZrUInt32); line2091; unconditional`; `2:arguments; encoded_size(io_read_functions(io,property->getter,1)); line2094; unconditional`; `3:arguments; encoded_size(io_read_functions(io,property->setter,1)); line2097; unconditional`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L104 io_read_meta

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_meta`；reader；events=3；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:meta->metaType; sizeof(TZrUInt32); line2102; unconditional`; `1:meta->functionsLength; sizeof(TZrSize); line2103; unconditional`; `2:arguments; encoded_size(io_read_functions(io,meta->functions,meta->functionsLength)); line2106; unconditional`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L105 io_read_enum_fields

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_enum_fields`；reader；events=2；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:field->name; encoded_size(io_read_string_with_length(io)); line2113; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `1:arguments; encoded_size(io_read_value(io,valueType,&field->value)); line2114; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L106 io_read_member_declares

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_member_declares`；reader；events=6；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:declare->type; sizeof(EZrIoMemberDeclareType); line2122; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `1:arguments; encoded_size(io_read_method(io,declare->method)); line2128; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & switch:declare->type`; `2:arguments; encoded_size(io_read_meta(io,declare->meta)); line2133; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & switch:declare->type`; `3:arguments; encoded_size(io_read_property(io,declare->property)); line2138; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & switch:declare->type`; `4:arguments; encoded_size(io_read_enum_fields(io,declare->enumField,1,ZR_VALUE_TYPE_UINT64)); line2143; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & switch:declare->type`; `5:arguments; encoded_size(io_read_field(io,declare->field)); line2148; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & switch:declare->type`。

switch/case labels：`[{"label":"case ZR_IO_MEMBER_DECLARE_TYPE_METHOD","line":2124},{"label":"case ZR_IO_MEMBER_DECLARE_TYPE_META","line":2130},{"label":"case ZR_IO_MEMBER_DECLARE_TYPE_PROPERTY","line":2135},{"label":"case ZR_IO_MEMBER_DECLARE_TYPE_ENUM","line":2140},{"label":"case ZR_IO_MEMBER_DECLARE_TYPE_FIELD","line":2145},{"label":"default","line":2150}]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L107 io_read_classes

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_classes`；reader；events=6；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:class->name; encoded_size(io_read_string_with_length(io)); line2161; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `1:class->superClassLength; sizeof(TZrSize); line2162; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `2:arguments; encoded_size(io_read_references(io,class->superClasses,class->superClassLength)); line2165; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `3:class->genericParametersLength; sizeof(TZrSize); line2166; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `4:class->declaresLength; sizeof(TZrSize); line2167; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `5:arguments; encoded_size(io_read_member_declares(io,class->declares,class->declaresLength)); line2170; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L108 io_read_structs

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_structs`；reader；events=6；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:struct_->name; encoded_size(io_read_string_with_length(io)); line2177; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `1:struct_->superStructLength; sizeof(TZrSize); line2178; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `2:arguments; encoded_size(io_read_references(io,struct_->superStructs,struct_->superStructLength)); line2181; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `3:struct_->genericParametersLength; sizeof(TZrSize); line2182; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `4:struct_->declaresLength; sizeof(TZrSize); line2183; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `5:arguments; encoded_size(io_read_member_declares(io,struct_->declares,struct_->declaresLength)); line2186; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L109 io_read_interfaces

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_interfaces`；reader；events=6；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:interface->name; encoded_size(io_read_string_with_length(io)); line2193; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `1:interface->superInterfaceLength; sizeof(TZrSize); line2194; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `2:arguments; encoded_size(io_read_references(io,interface->superInterfaces,interface->superInterfaceLength)); line2198; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `3:interface->genericParametersLength; sizeof(TZrSize); line2199; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `4:interface->declaresLength; sizeof(TZrSize); line2200; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `5:arguments; encoded_size(io_read_member_declares(io,interface->declares,interface->declaresLength)); line2203; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L110 io_read_enums

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_enums`；reader；events=4；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:enum_->name; encoded_size(io_read_string_with_length(io)); line2210; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `1:enum_->valueType; sizeof(EZrValueType); line2211; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `2:enum_->fieldsLength; sizeof(TZrSize); line2212; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `3:arguments; encoded_size(io_read_enum_fields(io,enum_->fields,enum_->fieldsLength,enum_->valueType)); line2215; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L111 io_read_module_declares

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_module_declares`；reader；events=7；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:declare->type; sizeof(EZrIoMemberDeclareType); line2222; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `1:arguments; encoded_size(io_read_classes(io,declare->class_,1)); line2228; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & switch:declare->type`; `2:arguments; encoded_size(io_read_functions(io,declare->function,1)); line2233; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & switch:declare->type`; `3:arguments; encoded_size(io_read_structs(io,declare->struct_,1)); line2238; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & switch:declare->type`; `4:arguments; encoded_size(io_read_interfaces(io,declare->interface_,1)); line2243; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & switch:declare->type`; `5:arguments; encoded_size(io_read_enums(io,declare->enum_,1)); line2248; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & switch:declare->type`; `6:arguments; encoded_size(io_read_field(io,declare->field)); line2253; for:TZrSize i = 0; !io->hasReadError && (i < count); i++ & switch:declare->type`。

switch/case labels：`[{"label":"case ZR_IO_MODULE_DECLARE_TYPE_CLASS","line":2225},{"label":"case ZR_IO_MODULE_DECLARE_TYPE_FUNCTION","line":2230},{"label":"case ZR_IO_MODULE_DECLARE_TYPE_STRUCT","line":2235},{"label":"case ZR_IO_MODULE_DECLARE_TYPE_INTERFACE","line":2240},{"label":"case ZR_IO_MODULE_DECLARE_TYPE_ENUM","line":2245},{"label":"case ZR_IO_MODULE_DECLARE_TYPE_FIELD","line":2250},{"label":"default","line":2255}]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L112 io_read_modules

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_modules`；reader；events=7；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:module->name; encoded_size(io_read_string_with_length(io)); line2266; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `1:module->md5; encoded_size(io_read_string_with_length(io)); line2267; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `2:module->importsLength; sizeof(TZrSize); line2268; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `3:arguments; encoded_size(io_read_imports(io,module->imports,module->importsLength)); line2271; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `4:module->declaresLength; sizeof(TZrSize); line2272; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `5:arguments; encoded_size(io_read_module_declares(io,module->declares,module->declaresLength)); line2275; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`; `6:arguments; encoded_size(io_read_functions(io,module->entryFunction,1)); line2278; for:TZrSize i = 0; !io->hasReadError && (i < count); i++`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L113 io_read_source

Key: `zr_vm_core/src/zr_vm_core/io/io.c::io_read_source`；reader；events=13；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:(TZrBytePtr)source->signature; sizeof(source->signature); line2344; unconditional`; `1:source->versionMajor; sizeof(TZrUInt32); line2345; unconditional`; `2:source->versionMinor; sizeof(TZrUInt32); line2346; unconditional`; `3:source->versionPatch; sizeof(TZrUInt32); line2347; unconditional`; `4:source->format; sizeof(TZrUInt64); line2348; unconditional`; `5:source->nativeIntSize; sizeof(TZrUInt8); line2349; unconditional`; `6:source->typeSizeSize; sizeof(TZrUInt8); line2350; unconditional`; `7:source->typeInstructionSize; sizeof(TZrUInt8); line2351; unconditional`; `8:source->isBigEndian; sizeof(TZrBool); line2352; unconditional`; `9:source->isDebug; sizeof(TZrBool); line2353; unconditional`; `10:(TZrBytePtr)source->optional; sizeof(source->optional); line2354; unconditional`; `11:source->modulesLength; sizeof(TZrSize); line2372; unconditional`; `12:arguments; encoded_size(io_read_modules(io,source->modules,source->modulesLength)); line2375; unconditional`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L114 ZrCore_Io_ReadSourceNew

Key: `zr_vm_core/src/zr_vm_core/io/io.c::ZrCore_Io_ReadSourceNew`；reader；events=1；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:arguments; encoded_size(io_read_source(&bounded,source)); line2423; if:!io->hasReadError && length > 0 & if:source != ZR_NULL`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L115 write_u32

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer_call_binding.c::write_u32`；writer；events=1；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:bytes; (1u) * (sizeof(bytes)); line9; unconditional`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L116 ZrParser_Writer_WriteCallBindings

Key: `zr_vm_parser/src/zr_vm_parser/writer/writer_call_binding.c::ZrParser_Writer_WriteCallBindings`；writer；events=10；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:arguments; encoded_size(write_u32(file,ZR_CALL_BINDING_SECTION_MAGIC)); line21; unconditional`; `1:arguments; encoded_size(write_u32(file,ZR_CALL_BINDING_SCHEMA_VERSION)); line22; unconditional`; `2:arguments; encoded_size(write_u32(file,ZR_CALL_BINDING_SECTION_ROW_SIZE)); line23; unconditional`; `3:arguments; encoded_size(write_u32(file,count)); line23; unconditional`; `4:arguments; encoded_size(write_u32(file,index)); line29; for:TZrUInt32 index = 0u; index < function->callSiteCacheLength; ++index`; `5:contract; (1u) * (sizeof(contract)); line30; for:TZrUInt32 index = 0u; index < function->callSiteCacheLength; ++index`; `6:arguments; encoded_size(write_u32(file,entry->bindingLocation.kind)); line31; for:TZrUInt32 index = 0u; index < function->callSiteCacheLength; ++index`; `7:arguments; encoded_size(write_u32(file,entry->bindingLocation.targetIndex)); line32; for:TZrUInt32 index = 0u; index < function->callSiteCacheLength; ++index`; `8:arguments; encoded_size(write_u32(file,entry->bindingLocation.ownerDepth)); line33; for:TZrUInt32 index = 0u; index < function->callSiteCacheLength; ++index`; `9:arguments; encoded_size(write_u32(file,entry->bindingLocation.flags)); line34; for:TZrUInt32 index = 0u; index < function->callSiteCacheLength; ++index`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### L117 ZrCore_Io_ReadCallBindings

Key: `zr_vm_core/src/zr_vm_core/io/io_call_binding.c::ZrCore_Io_ReadCallBindings`；reader；events=2；encoding owner=Core legacy IO；producer=Parser writer / reader=Core IO（按本行 direction）。

按 cursor 顺序：`0:header; sizeof(header); line12; unconditional`; `1:row; sizeof(row); line28; for:TZrUInt32 index = 0u; index < count; ++index`。

switch/case labels：`[]`；hash=H0（supplied hash 字段查 H 根；不额外计字段）。

### Reader/writer 配对

下表是语义配对，不是 event 数相等断言；reader-only/split 与 writer-only/split 函数仍在 L 全索引中。

| Writer | Reader | disposition |
| --- | --- | --- |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_string_with_length` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_string_with_length` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_native_string_with_length` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_string_with_length` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c::writer_binary_write_native_string_with_length` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_string_with_length` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_io_reference` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_references` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_prototype_class` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_classes` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_prototype_struct` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_structs` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_debug_infos` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_debug_infos` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_name` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_functions` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_local_variables` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_local_variables` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_closure_variables` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_closure_variables` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_exception_metadata` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_functions` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_frame_layout` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_frame_layout` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_value_payload` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_value` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_embedded_value` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_typed_value` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_constant` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_functions` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_typed_type_ref` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_typed_type_ref` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_typed_local_bindings` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_typed_local_bindings` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_typed_closure_bindings` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_typed_closure_bindings` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_typed_export_generic_parameter` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_typed_export_generic_parameters` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_typed_export_symbols` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_typed_export_symbols` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_metadata_token_records` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_metadata_token_record_list` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_module_metadata_bindings` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_module_metadata_binding_list` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_metadata_string_heap` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_metadata_string_heap` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_metadata_token_model` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_metadata_token_model` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_module_metadata_ref_table` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_functions` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_module_metadata_bindings` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_functions` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_module_effect` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_module_effects` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_static_imports` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_functions` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_module_entry_effects` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_functions` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_exported_callable_summaries` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_callable_summaries` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_top_level_callable_bindings` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_top_level_callable_bindings` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_metadata_parameters` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_metadata_parameters` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_parameter_metadata` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_functions` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_callable_return_metadata` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_functions` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_compile_time_metadata` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_functions` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_escape_metadata` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_escape_bindings` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_test_manifest` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_functions` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_decorator_metadata` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_decorator_metadata` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_member_entries` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_member_entries` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_semir_metadata` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_semir_metadata` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_callsite_cache_metadata` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_callsite_cache_metadata` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_ffi_type_contract` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_ffi_type_contract` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_ffi_callable_contract` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_ffi_callable_contract` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_native_import_contracts` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_native_import_contracts` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_function_prototypes` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_functions` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer.c::write_io_function_internal` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_functions` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c::ZrParser_Writer_WriteBinaryFileWithOptions` | `zr_vm_core/src/zr_vm_core/io/io.c::io_read_source` | semantic_pair_not_flat_leaf_equality |
| `zr_vm_parser/src/zr_vm_parser/writer/writer_call_binding.c::ZrParser_Writer_WriteCallBindings` | `zr_vm_core/src/zr_vm_core/io/io_call_binding.c::ZrCore_Io_ReadCallBindings` | semantic_pair_not_flat_leaf_equality |
### Patch admission 与全部42宏

Writer patch45，reader闭区间16–45；major/minor、signature、native sizes、host endian、debug≤1、三optional字节0与全输入消费按 io.c 实现。unknown patch 不自适应。下列是 grammar gates，不是42种新格式或capability。

| 宏 | 值 | 真实reader位置 |
| --- | --- | --- |
| `ZR_IO_SOURCE_PATCH_HAS_COMPILE_TIME_METADATA` | `2U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_functions:1909 |
| `ZR_IO_SOURCE_PATCH_HAS_PROTOTYPE_BLOB` | `3U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_functions:2045 |
| `ZR_IO_SOURCE_PATCH_HAS_SEMIR_METADATA` | `4U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_functions:2000 |
| `ZR_IO_SOURCE_PATCH_HAS_MEMBER_ENTRIES` | `5U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_functions:1989 |
| `ZR_IO_SOURCE_PATCH_HAS_CALLSITE_CACHE` | `7U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_functions:2003 |
| `ZR_IO_SOURCE_PATCH_HAS_FUNCTION_DECORATOR_METADATA` | `10U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_functions:1986 |
| `ZR_IO_SOURCE_PATCH_HAS_FUNCTION_PARAMETER_METADATA` | `11U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_metadata_parameters:1051; zr_vm_core/src/zr_vm_core/io/io.c::io_read_functions:1900 |
| `ZR_IO_SOURCE_PATCH_HAS_TYPED_EXPORT_DECLARATION_SPANS` | `12U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_typed_export_symbols:659 |
| `ZR_IO_SOURCE_PATCH_HAS_FUNCTION_SOURCE_IDENTITY` | `13U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_debug_infos:1583 |
| `ZR_IO_SOURCE_PATCH_HAS_COMPILE_TIME_PATH_BINDINGS` | `14U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_compile_time_variable_infos:1086 |
| `ZR_IO_SOURCE_PATCH_HAS_FUNCTION_SOURCE_RANGES` | `15U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_debug_infos:1605 |
| `ZR_IO_SOURCE_PATCH_HAS_MODULE_INIT_METADATA` | `16U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_source:2360 |
| `ZR_IO_SOURCE_PATCH_HAS_FUNCTION_ESCAPE_METADATA` | `17U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_local_variables:393; zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_closure_variables:410; zr_vm_core/src/zr_vm_core/io/io.c::io_read_functions:1932 |
| `ZR_IO_SOURCE_PATCH_HAS_FUNCTION_CALLABLE_RETURN_TYPE` | `18U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_functions:1903 |
| `ZR_IO_SOURCE_PATCH_HAS_FUNCTION_FRAME_LAYOUT` | `19U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_functions:1699 |
| `ZR_IO_SOURCE_PATCH_HAS_METADATA_TOKEN_MODEL` | `20U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_typed_export_symbols:671; zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_metadata_token_model:873 |
| `ZR_IO_SOURCE_PATCH_HAS_METADATA_TOKEN_SIGNATURE_HASH` | `21U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_typed_export_symbols:676; zr_vm_core/src/zr_vm_core/io/io.c::io_read_metadata_token_record_list:723 |
| `ZR_IO_SOURCE_PATCH_HAS_METADATA_TOKEN_OWNER_CHAIN` | `22U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_metadata_token_record_list:715 |
| `ZR_IO_SOURCE_PATCH_HAS_METADATA_TARGET_SIGNATURE_HASH` | `23U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_metadata_token_record_list:744; zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_module_effects:943 |
| `ZR_IO_SOURCE_PATCH_HAS_METADATA_TARGET_SIGNATURE_TOKENS` | `24U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_metadata_token_record_list:737 |
| `ZR_IO_SOURCE_PATCH_HAS_MODULE_METADATA_REF_TABLE` | `25U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_metadata_token_model:908 |
| `ZR_IO_SOURCE_PATCH_HAS_MODULE_SIGNATURE_HASH` | `26U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_metadata_token_model:904 |
| `ZR_IO_SOURCE_PATCH_HAS_MODULE_EFFECT_TARGET_MODULE_HASH` | `27U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_metadata_token_record_list:749; zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_module_effects:947 |
| `ZR_IO_SOURCE_PATCH_HAS_MODULE_METADATA_BINDINGS` | `28U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_metadata_token_model:913 |
| `ZR_IO_SOURCE_PATCH_HAS_ASSEMBLY_REF_VERSION_RANGE` | `29U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_metadata_token_record_list:754; zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_metadata_token_model:880; zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_module_effects:952 |
| `ZR_IO_SOURCE_PATCH_HAS_METADATA_STRING_HEAP` | `30U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_metadata_token_model:899 |
| `ZR_IO_SOURCE_PATCH_HAS_METADATA_TOKEN_LAYOUT_IDENTITY` | `31U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_metadata_token_record_list:728 |
| `ZR_IO_SOURCE_PATCH_HAS_METADATA_BINDING_LAYOUT_IDENTITY` | `32U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_module_metadata_binding_list:798; zr_vm_core/src/zr_vm_core/io/io.c::io_read_module_metadata_binding_list:811 |
| `ZR_IO_SOURCE_PATCH_HAS_MODULE_EFFECT_ASSEMBLY_NAME` | `33U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_module_effects:933 |
| `ZR_IO_SOURCE_PATCH_HAS_SEMIR_STATIC_C_TYPES` | `34U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_typed_type_ref:503 |
| `ZR_IO_SOURCE_PATCH_HAS_BORROWED_FRAME_ALIAS` | `35U` |  |
| `ZR_IO_SOURCE_PATCH_HAS_VM_ENTRY_CLEAR_STACK_SIZE` | `36U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_functions:1690 |
| `ZR_IO_SOURCE_PATCH_HAS_TYPED_LOCAL_CANONICAL_BINDINGS` | `37U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_typed_local_bindings:528 |
| `ZR_IO_SOURCE_PATCH_HAS_TYPED_LOCAL_ROLE_FLAGS` | `38U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_typed_local_bindings:537 |
| `ZR_IO_SOURCE_PATCH_HAS_NATIVE_IMPORT_CONTRACTS` | `39U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_functions:2011 |
| `ZR_IO_SOURCE_PATCH_HAS_TEST_MANIFEST` | `40U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_functions:1956 |
| `ZR_IO_SOURCE_PATCH_HAS_TYPED_CLOSURE_CAPTURE_BINDINGS` | `41U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_functions:1783 |
| `ZR_IO_SOURCE_PATCH_HAS_TYPED_EXPORT_GENERIC_PARAMETERS` | `42U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_function_typed_export_symbols:655 |
| `ZR_IO_SOURCE_PATCH_HAS_CALL_BINDING` | `43U` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_functions:2006 |
| `ZR_IO_SOURCE_PATCH_HAS_MARK_CLOSE_PROXY` | `44U` |  |
| `ZR_IO_SOURCE_PATCH_HAS_ARRAY_INITIAL_LENGTH` | `45U` |  |
| `ZR_IO_SOURCE_PATCH_CURRENT` | `ZR_IO_SOURCE_PATCH_HAS_ARRAY_INITIAL_LENGTH` | zr_vm_core/src/zr_vm_core/io/io.c::io_read_source:2361 |
### Packed prototype 45固定字段

现有 pack1 及 u32 宽度 assertions 已证明 header60B/member124B，不能笼统称 padding 未知。prototype[i] base =4+Σ(j<i)(60+4*inheritsCount[j]+4*decoratorsCount[j]+124*membersCount[j])；inherits base=+60；decorators base=+60+4*inheritsCount；members base=+60+4*(inheritsCount+decoratorsCount)；member[k] 再+124*k。count 前缀u32/4B。该 slice 不证明整个旧IO跨host可移植，native endian仍需匹配。

| record / fixed size | 全字段@相对offset:width |
| --- | --- |
| `SZrCompiledPrototypeInfo` / 60 | `nameStringIndex@0:4`; `type@4:4`; `accessModifier@8:4`; `inheritsCount@12:4`; `membersCount@16:4`; `protocolMask@20:8`; `hasDecoratorMetadata@28:4`; `decoratorMetadataConstantIndex@32:4`; `decoratorsCount@36:4`; `modifierFlags@40:4`; `nextVirtualSlotIndex@44:4`; `nextPropertyIdentity@48:4`; `layoutByteSize@52:4`; `layoutByteAlign@56:4` |
| `SZrCompiledMemberInfo` / 124 | `memberType@0:4`; `nameStringIndex@4:4`; `accessModifier@8:4`; `isStatic@12:4`; `isConst@16:4`; `fieldTypeNameStringIndex@20:4`; `fieldOffset@24:4`; `fieldSize@28:4`; `isMetaMethod@32:4`; `metaType@36:4`; `functionConstantIndex@40:4`; `parameterCount@44:4`; `returnTypeNameStringIndex@48:4`; `reservedRemovedUsingManaged@52:4`; `ownershipQualifier@56:4`; `callsClose@60:4`; `callsDestructor@64:4`; `declarationOrder@68:4`; `contractRole@72:4`; `hasDecoratorMetadata@76:4`; `decoratorMetadataConstantIndex@80:4`; `hasDecoratorNames@84:4`; `decoratorNamesConstantIndex@88:4`; `modifierFlags@92:4`; `ownerTypeNameStringIndex@96:4`; `baseDefinitionOwnerTypeNameStringIndex@100:4`; `baseDefinitionNameStringIndex@104:4`; `virtualSlotIndex@108:4`; `interfaceContractSlot@112:4`; `propertyIdentity@116:4`; `accessorRole@120:4` |

## C 声明字段路径组：不是新增 wire 记录

下两组保留完整字段名/类型，wire width/offset 只由上面的 F/L grammar决定。固定整数 typedef 按声明域；指针、TZrSize、enum/native struct 的宽度使用本host sizeof，不猜 portable 字节数，不自动 serialize。嵌套 runtime owner/borrow/clone/free 见运行期页；其他逻辑记录只按值复制标量或借用 caller-owned arrays，不凭 C struct 名称推导 deep clone。

旧527 declaration anchor纠正：默认524字段 line+1，三个例外：SZrCompiledMemberInfo.fieldTypeNameStringIndex 54→57、isMetaMethod59→62；SZrFfiSignatureContract.aggregateFields257→259。packed45默认43项+1，前两个同例外。四个双指针原parser未支持的声明仍明确记录：io.h constraint180/decorator286、function.h constraint224/decorator403；其count/string wire events已在L中覆盖。它们不是漏掉四种wire数据或ABI padding。

### Base 60 declarations

| record / authoritative header / classification | 全字段 : C type |
| --- | --- |
| `SZrArtifactPublicIdentity` / [artifact_schema.h](../../zr_vm_core/include/zr_vm_core/artifact_schema.h) / wire_semantics | `canonicalTypeId : TZrUInt32`; `typeRefToken : TZrMetadataToken`; `typeSpecToken : TZrMetadataToken`; `signatureToken : TZrMetadataToken`; `typeRefHash : TZrUInt64`; `typeSpecHash : TZrUInt64`; `signatureHash : TZrUInt64`; `layoutVersion : TZrUInt32`; `reserved0 : TZrUInt32`; `layoutHash : TZrUInt64`; `callableContractHash : TZrUInt64`; `moduleHash : TZrUInt64` |
| `SZrArtifactTypeDefRow` / [artifact_schema.h](../../zr_vm_core/include/zr_vm_core/artifact_schema.h) / wire_semantics | `token : TZrMetadataToken`; `flags : TZrUInt32`; `canonicalTypeId : TZrUInt32`; `constructorToken : TZrMetadataToken`; `constructorSignatureToken : TZrMetadataToken`; `reserved0 : TZrUInt32`; `typeSignatureHash : TZrUInt64`; `constructorContractHash : TZrUInt64`; `reserved1 : TZrUInt64` |
| `SZrArtifactTypeIdentityRow` / [artifact_schema.h](../../zr_vm_core/include/zr_vm_core/artifact_schema.h) / wire_semantics | `token : TZrMetadataToken`; `signatureToken : TZrMetadataToken`; `canonicalTypeId : TZrUInt32`; `flags : TZrUInt32`; `signatureOffset : TZrUInt32`; `signatureLength : TZrUInt32`; `signatureHash : TZrUInt64`; `layoutVersion : TZrUInt32`; `reserved0 : TZrUInt32`; `layoutHash : TZrUInt64` |
| `SZrArtifactMemberDefRow` / [artifact_schema.h](../../zr_vm_core/include/zr_vm_core/artifact_schema.h) / wire_semantics | `token : TZrMetadataToken`; `ownerTypeToken : TZrMetadataToken`; `signatureToken : TZrMetadataToken`; `flags : TZrUInt32`; `nameStringOffset : TZrUInt32`; `reserved0 : TZrUInt32`; `signatureHash : TZrUInt64`; `contractHash : TZrUInt64` |
| `SZrArtifactPropertyDefRow` / [artifact_schema.h](../../zr_vm_core/include/zr_vm_core/artifact_schema.h) / wire_semantics | `token : TZrMetadataToken`; `ownerTypeToken : TZrMetadataToken`; `getterToken : TZrMetadataToken`; `setterToken : TZrMetadataToken`; `signatureToken : TZrMetadataToken`; `flags : TZrUInt32`; `signatureHash : TZrUInt64`; `contractHash : TZrUInt64`; `initializerToken : TZrMetadataToken`; `nameStringOffset : TZrUInt32` |
| `SZrArtifactContractRow` / [artifact_schema.h](../../zr_vm_core/include/zr_vm_core/artifact_schema.h) / wire_semantics | `memberToken : TZrMetadataToken`; `signatureToken : TZrMetadataToken`; `parameterCount : TZrUInt32`; `flags : TZrUInt32`; `receiverEffect : TZrUInt32`; `refExportEffect : TZrUInt32`; `escapeFlags : TZrUInt32`; `abiLoweringKind : EZrArtifactAbiLoweringKind`; `contractHash : TZrUInt64` |
| `SZrArtifactLayoutRow` / [artifact_schema.h](../../zr_vm_core/include/zr_vm_core/artifact_schema.h) / wire_semantics | `typeToken : TZrMetadataToken`; `version : TZrUInt32`; `byteSize : TZrUInt32`; `byteAlignment : TZrUInt32`; `gcScanKind : TZrUInt32`; `capabilityFlags : TZrUInt32`; `ownershipMapOffset : TZrUInt32`; `ownershipMapLength : TZrUInt32`; `layoutHash : TZrUInt64`; `stableSlotContractHash : TZrUInt64` |
| `SZrArtifactMetadataStateRow` / [artifact_schema.h](../../zr_vm_core/include/zr_vm_core/artifact_schema.h) / wire_semantics | `typeToken : TZrMetadataToken`; `preservationState : EZrArtifactMetadataPreservationState`; `category : EZrArtifactReflectionCategory`; `metadataGeneration : TZrUInt32`; `retainedMemberCount : TZrUInt32`; `retainedPropertyCount : TZrUInt32`; `retainedMetaRecordCount : TZrUInt32`; `flags : TZrUInt32`; `typeSignatureHash : TZrUInt64`; `layoutHash : TZrUInt64`; `callableContractHash : TZrUInt64`; `metadataHash : TZrUInt64` |
| `SZrArtifactMetadataRecordRow` / [artifact_schema.h](../../zr_vm_core/include/zr_vm_core/artifact_schema.h) / wire_semantics | `ownerToken : TZrMetadataToken`; `kind : EZrArtifactMetadataRecordKind`; `retention : EZrArtifactMetadataRetention`; `flags : TZrUInt32`; `payloadOffset : TZrUInt32`; `payloadLength : TZrUInt32`; `metadataGeneration : TZrUInt32`; `reserved0 : TZrUInt32`; `recordHash : TZrUInt64` |
| `SZrArtifactDomainTransferRow` / [artifact_schema.h](../../zr_vm_core/include/zr_vm_core/artifact_schema.h) / wire_semantics | `typeToken : TZrMetadataToken`; `kind : EZrArtifactDomainTransferKind`; `schemaVersion : TZrUInt32`; `flags : TZrUInt32`; `schemaOffset : TZrUInt32`; `schemaLength : TZrUInt32`; `providerToken : TZrMetadataToken`; `reserved0 : TZrUInt32`; `schemaHash : TZrUInt64`; `providerContractHash : TZrUInt64` |
| `SZrArtifactSchedulerContractRow` / [artifact_schema.h](../../zr_vm_core/include/zr_vm_core/artifact_schema.h) / wire_semantics | `schedulerTypeToken : TZrMetadataToken`; `taskTypeToken : TZrMetadataToken`; `jobTypeToken : TZrMetadataToken`; `abiVersion : TZrUInt32`; `policyMask : TZrUInt32`; `attachedRequirementFlags : TZrUInt32`; `isolatedRequirementFlags : TZrUInt32`; `reserved0 : TZrUInt32`; `transportContractHash : TZrUInt64`; `schedulerContractHash : TZrUInt64` |
| `SZrArtifactRelocationRow` / [artifact_schema.h](../../zr_vm_core/include/zr_vm_core/artifact_schema.h) / wire_semantics | `codeOffset : TZrUInt32`; `kind : TZrUInt32`; `targetToken : TZrMetadataToken`; `targetSignatureToken : TZrMetadataToken`; `expectedSignatureHash : TZrUInt64`; `expectedContractHash : TZrUInt64`; `expectedModuleHash : TZrUInt64` |
| `SZrArtifactCallBindingRow` / [artifact_schema.h](../../zr_vm_core/include/zr_vm_core/artifact_schema.h) / wire_semantics | `schemaVersion : TZrUInt32`; `functionIndex : TZrUInt32`; `cacheIndex : TZrUInt32`; `instructionIndex : TZrUInt32`; `contract : SZrCallBindingContract`; `location : SZrCallBindingLocation` |
| `SZrCallBindingContract` / [call_binding.h](../../zr_vm_core/include/zr_vm_core/call_binding.h) / wire_semantics | `bindingKind : TZrUInt32`; `targetMetadataToken : TZrMetadataToken`; `signatureToken : TZrMetadataToken`; `ownerTypeToken : TZrMetadataToken`; `signatureHash : TZrUInt64`; `moduleSignatureHash : TZrUInt64`; `layoutVersion : TZrUInt32`; `dispatchSlot : TZrUInt32`; `layoutHash : TZrUInt64`; `operation : TZrUInt32`; `reserved0 : TZrUInt32`; `reserved1 : TZrUInt64` |
| `SZrCallBindingLocation` / [call_binding.h](../../zr_vm_core/include/zr_vm_core/call_binding.h) / wire_semantics | `kind : TZrUInt32`; `targetIndex : TZrUInt32`; `ownerDepth : TZrUInt32`; `flags : TZrUInt32` |
| `SZrExecutionContract` / [execution_contract.h](../../zr_vm_core/include/zr_vm_core/execution_contract.h) / shared_scalar_semantics_multiple_consumers | `schemaVersion : TZrUInt32`; `abiVersion : TZrUInt32`; `logicalVersion : TZrUInt32`; `reserved0 : TZrUInt32`; `generation : TZrUInt64`; `targetToken : TZrMetadataToken`; `reserved1 : TZrUInt32`; `signatureHash : TZrUInt64`; `layoutHash : TZrUInt64`; `moduleHash : TZrUInt64`; `requiredCapabilities : TZrUInt32`; `declaredEffects : TZrUInt32` |
| `SZrExecIrProfileKey` / [exec_ir_profile.h](../../zr_vm_parser/include/zr_vm_parser/exec_ir_profile.h) / logical_profile_no_disk_codec_identified | `moduleIdentity : TZrMetadataToken`; `moduleHash : TZrUInt64`; `irHash : TZrUInt64`; `signatureHash : TZrUInt64`; `layoutHash : TZrUInt64`; `compilerAbi : TZrUInt32` |
| `SZrExecIrProfileSite` / [exec_ir_profile.h](../../zr_vm_parser/include/zr_vm_parser/exec_ir_profile.h) / logical_profile_no_disk_codec_identified | `siteKey : TZrUInt64`; `functionToken : TZrMetadataToken`; `instructionId : TZrExecIrInstructionId`; `sourceId : TZrExecIrSourceId`; `kind : EZrExecIrProfileSiteKind`; `targetToken : TZrMetadataToken`; `receiverTypeToken : TZrExecIrTypeToken`; `numericTypeToken : TZrExecIrTypeToken`; `branchTaken : TZrUInt64`; `branchNotTaken : TZrUInt64`; `hitCount : TZrUInt64`; `missCount : TZrUInt64`; `allocationCount : TZrUInt64`; `allocationBytes : TZrUInt64`; `boxingCount : TZrUInt64`; `cacheHitCount : TZrUInt64`; `cacheMissCount : TZrUInt64`; `deoptCount : TZrUInt64`; `gcCount : TZrUInt64`; `contentionCount : TZrUInt64`; `distinctTargets : TZrUInt32`; `megamorphic : TZrBool` |
| `SZrExecIrSpecializationPolicy` / [exec_ir_profile.h](../../zr_vm_parser/include/zr_vm_parser/exec_ir_profile.h) / logical_profile_no_disk_codec_identified | `maxVersionsPerSite : TZrUInt32`; `maxVersionsPerFunction : TZrUInt32`; `maxVersionsPerModule : TZrUInt32`; `maxCodeBytesPerSite : TZrUInt32`; `maxCodeBytesPerFunction : TZrUInt32`; `maxCodeBytesPerModule : TZrUInt32`; `minSamples : TZrUInt64`; `minHitRatePermille : TZrUInt32`; `maxMissRatePermille : TZrUInt32`; `maxConsecutiveDeopts : TZrUInt32`; `cooldownSamples : TZrUInt32`; `maxPolymorphicTargets : TZrUInt32` |
| `SZrExecIrProfile` / [exec_ir_profile.h](../../zr_vm_parser/include/zr_vm_parser/exec_ir_profile.h) / logical_profile_no_disk_codec_identified | `magic : TZrUInt32`; `schemaVersion : TZrUInt32`; `key : SZrExecIrProfileKey`; `sites : SZrExecIrProfileSite *`; `siteCount : TZrUInt32`; `siteCapacity : TZrUInt32`; `policy : SZrExecIrSpecializationPolicy` |
| `SZrCompileIrCacheInputs` / [compile_ir_cache.h](../../zr_vm_parser/include/zr_vm_parser/compile_ir_cache.h) / in_memory_cache_not_disk_format | `sourceBytes : const TZrByte *`; `sourceSize : TZrSize`; `dependencyBytes : const TZrByte *`; `dependencySize : TZrSize`; `contractHash : TZrUInt64`; `compilerAbi : TZrUInt64`; `passPipelineHash : TZrUInt64`; `targetHash : TZrUInt64`; `numericPolicyHash : TZrUInt64`; `layoutHash : TZrUInt64`; `importedProfileHash : TZrUInt64` |
| `SZrCompileIrCacheKey` / [compile_ir_cache.h](../../zr_vm_parser/include/zr_vm_parser/compile_ir_cache.h) / in_memory_cache_not_disk_format | `schemaVersion : TZrUInt32`; `profileHash : TZrUInt64`; `contractHash : TZrUInt64`; `compilerAbi : TZrUInt64`; `passPipelineHash : TZrUInt64`; `targetHash : TZrUInt64`; `numericPolicyHash : TZrUInt64`; `layoutHash : TZrUInt64`; `importedProfileHash : TZrUInt64`; `digest : TZrByte[ZR_COMPILE_IR_CACHE_DIGEST_SIZE]`; `valid : TZrBool` |
| `SZrCompileIrCacheHit` / [compile_ir_cache.h](../../zr_vm_parser/include/zr_vm_parser/compile_ir_cache.h) / in_memory_cache_not_disk_format | `payload : const TZrByte *`; `payloadSize : TZrSize`; `accepted : TZrBool` |
| `SZrCompileIrCache` / [compile_ir_cache.h](../../zr_vm_parser/include/zr_vm_parser/compile_ir_cache.h) / in_memory_cache_not_disk_format | `entries : SZrCompileIrCacheEntry *`; `count : TZrSize`; `capacity : TZrSize`; `nextWriterToken : TZrUInt64`; `activeWriters : TZrUInt32` |
| `SZrCompileIrCacheTransaction` / [compile_ir_cache.h](../../zr_vm_parser/include/zr_vm_parser/compile_ir_cache.h) / in_memory_cache_not_disk_format | `cache : SZrCompileIrCache *`; `key : SZrCompileIrCacheKey`; `writerToken : TZrUInt64`; `active : TZrBool` |
| `SZrCompileOptimizationProfile` / [compile_optimization_profile.h](../../zr_vm_parser/include/zr_vm_parser/compile_optimization_profile.h) / logical_policy | `schemaVersion : TZrUInt32`; `preset : EZrCompileOptimizationPreset`; `buildMode : EZrCompileBuildMode`; `numericPermission : EZrCompileNumericPermission`; `target : EZrCompileTarget`; `backend : EZrCompileBackend`; `frameBudgetBytes : TZrUInt32`; `passMask : TZrUInt32`; `targetHash : TZrUInt64`; `hostAbiHash : TZrUInt64`; `frameSafe : TZrBool`; `requestFastMath : TZrBool`; `enableLto : TZrBool`; `enablePgo : TZrBool` |
| `SZrCompileEffectivePolicy` / [compile_optimization_profile.h](../../zr_vm_parser/include/zr_vm_parser/compile_optimization_profile.h) / logical_policy | `schemaVersion : TZrUInt32`; `preset : EZrCompileOptimizationPreset`; `buildMode : EZrCompileBuildMode`; `numericPermission : EZrCompileNumericPermission`; `target : EZrCompileTarget`; `backend : EZrCompileBackend`; `frameBudgetBytes : TZrUInt32`; `passMask : TZrUInt32`; `targetHash : TZrUInt64`; `hostAbiHash : TZrUInt64`; `profileHash : TZrUInt64`; `frameSafe : TZrBool`; `fastMathEnabled : TZrBool`; `ltoEnabled : TZrBool`; `pgoEnabled : TZrBool` |
| `SZrHotPatchCapabilityRequirement` / [capability_manifest.h](../../zr_vm_core/include/zr_vm_core/capability_manifest.h) / runtime_host_input_not_wire | `token : TZrUInt32`; `requiredBits : TZrUInt64`; `sourceOffset : TZrUInt32`; `reserved : TZrUInt32` |
| `SZrHotPatchCapabilityManifest` / [capability_manifest.h](../../zr_vm_core/include/zr_vm_core/capability_manifest.h) / runtime_host_input_not_wire | `schemaVersion : TZrUInt32`; `flags : TZrUInt32`; `patchId : TZrUInt64`; `contentHash : TZrUInt64`; `baseModuleHash : TZrUInt64`; `publicContractHash : TZrUInt64`; `requiredCapabilities : TZrUInt64`; `targetAbiVersion : TZrUInt32`; `targetProfile : TZrUInt32`; `requirementCount : TZrUInt32`; `requirements : const SZrHotPatchCapabilityRequirement *` |
| `SZrHotPatchValidationInput` / [capability_manifest.h](../../zr_vm_core/include/zr_vm_core/capability_manifest.h) / runtime_host_input_not_wire | `artifact : const SZrArtifactExecIrView *`; `manifest : const SZrHotPatchCapabilityManifest *`; `loadedBaseModuleHash : TZrUInt64`; `loadedPublicContractHash : TZrUInt64`; `hostAbiVersion : TZrUInt32`; `hostProfile : TZrUInt32`; `hostAllowedCapabilities : TZrUInt64`; `expectedPatchId : TZrUInt64`; `expectedContentHash : TZrUInt64`; `signature : const TZrByte *`; `signatureLength : TZrUInt32` |
| `SZrHotPatchZrafValidationInput` / [capability_manifest.h](../../zr_vm_core/include/zr_vm_core/capability_manifest.h) / runtime_host_input_not_wire | `outerBytes : const TZrByte *`; `outerLength : TZrSize`; `expectedPublicIdentity : const SZrArtifactPublicIdentity *`; `manifest : const SZrHotPatchCapabilityManifest *`; `entryFunctionToken : TZrMetadataToken`; `entrySignatureHash : TZrUInt64`; `loadedBaseModuleHash : TZrUInt64`; `loadedPublicContractHash : TZrUInt64`; `hostAbiVersion : TZrUInt32`; `hostProfile : TZrUInt32`; `hostAllowedCapabilities : TZrUInt64`; `expectedPatchId : TZrUInt64`; `expectedContentHash : TZrUInt64`; `signature : const TZrByte *`; `signatureLength : TZrUInt32` |
| `SZrValidatedHotPatch` / [capability_manifest.h](../../zr_vm_core/include/zr_vm_core/capability_manifest.h) / runtime_host_input_not_wire | `artifact : const SZrArtifactExecIrView *`; `manifest : const SZrHotPatchCapabilityManifest *`; `contentHash : TZrUInt64`; `contentBytes : const TZrByte *`; `contentLength : TZrUInt32`; `patchId : TZrUInt64`; `publicContractHash : TZrUInt64`; `requiredCapabilities : TZrUInt64`; `validationPolicyHash : TZrUInt64`; `targetProfile : TZrUInt32`; `signatureVerified : TZrBool`; `immutableContent : TZrBool` |
| `SZrValidatedHotPatchZraf` / [capability_manifest.h](../../zr_vm_core/include/zr_vm_core/capability_manifest.h) / runtime_host_input_not_wire | `outerBytes : const TZrByte *`; `outerLength : TZrSize`; `outerContentHash : TZrUInt64`; `publicIdentity : SZrArtifactPublicIdentity`; `entryFunctionToken : TZrMetadataToken`; `entrySignatureHash : TZrUInt64`; `patchId : TZrUInt64`; `publicContractHash : TZrUInt64`; `requiredCapabilities : TZrUInt64`; `validationPolicyHash : TZrUInt64`; `targetAbiVersion : TZrUInt32`; `targetProfile : TZrUInt32`; `signatureVerified : TZrBool`; `canonicalExecIrVerified : TZrBool` |
| `SZrExecutionGenerationKey` / [execution_backend.h](../../zr_vm_core/include/zr_vm_core/execution_backend.h) / runtime_witness_not_wire | `domainIdentity : TZrUInt64`; `moduleIdentity : TZrUInt64`; `generation : TZrUInt64`; `backendRegistrationIdentity : TZrUInt64` |
| `SZrExecutionBackendTarget` / [execution_backend.h](../../zr_vm_core/include/zr_vm_core/execution_backend.h) / runtime_witness_not_wire | `schemaVersion : TZrUInt32`; `kind : EZrExecutionBackendTargetKind`; `abiVersion : TZrUInt32`; `reserved : TZrUInt32`; `targetTripleHash : TZrUInt64`; `layoutHash : TZrUInt64`; `capabilityHash : TZrUInt64` |
| `SZrExecutionCompileRequest` / [execution_backend.h](../../zr_vm_core/include/zr_vm_core/execution_backend.h) / runtime_witness_not_wire | `magic : TZrUInt32`; `schemaVersion : TZrUInt32`; `flags : TZrUInt32`; `requestedTarget : EZrExecutionBackendTargetKind`; `generationKey : SZrExecutionGenerationKey`; `contract : SZrExecutionContract`; `immutableIrHash : TZrUInt64`; `compileInputHash : TZrUInt64`; `sourceId : TZrUInt32`; `instructionId : TZrUInt32`; `requiredOperations : TZrUInt32`; `requiredMapFlags : TZrUInt32` |
| `SZrExecutionCompileTicket` / [execution_backend.h](../../zr_vm_core/include/zr_vm_core/execution_backend.h) / runtime_witness_not_wire | `magic : TZrUInt32`; `schemaVersion : TZrUInt32`; `serviceIdentity : TZrUInt64`; `ticketId : TZrUInt64`; `generationKey : SZrExecutionGenerationKey`; `target : EZrExecutionBackendTargetKind`; `fallback : EZrExecutionBackendFallback`; `state : EZrExecutionBackendJobState` |
| `SZrExecutionBackendCodeInfo` / [execution_backend.h](../../zr_vm_core/include/zr_vm_core/execution_backend.h) / runtime_witness_not_wire | `magic : TZrUInt32`; `schemaVersion : TZrUInt32`; `generationKey : SZrExecutionGenerationKey`; `contract : SZrExecutionContract`; `immutableIrHash : TZrUInt64`; `compileInputHash : TZrUInt64`; `codeIdentity : TZrUInt64`; `codeSize : TZrUInt32`; `mapRegistrationFlags : TZrUInt32`; `rootMapHash : TZrUInt64`; `ehMapHash : TZrUInt64`; `debugMapHash : TZrUInt64`; `deoptMapHash : TZrUInt64`; `runtimeImportsHash : TZrUInt64` |
| `SZrExecutionCodeHandle` / [execution_backend.h](../../zr_vm_core/include/zr_vm_core/execution_backend.h) / runtime_witness_not_wire | `magic : TZrUInt32`; `slotIndex : TZrUInt32`; `serviceIdentity : TZrUInt64`; `codeIdentity : TZrUInt64`; `generationKey : SZrExecutionGenerationKey`; `leased : TZrBool`; `dependencyLeased : TZrBool`; `reserved : TZrUInt16` |
| `SZrHotPatchVersionRecord` / [hotpatch_generation.h](../../zr_vm_core/include/zr_vm_core/hotpatch_generation.h) / runtime_generation_not_wire | `generation : TZrUInt64`; `moduleHash : TZrUInt64`; `contentHash : TZrUInt64`; `publicContractHash : TZrUInt64`; `targetProfile : TZrUInt32`; `state : TZrUInt32`; `leaseCount : atomic_uint_fast32_t` |
| `SZrHotPatchGenerationManager` / [hotpatch_generation.h](../../zr_vm_core/include/zr_vm_core/hotpatch_generation.h) / runtime_generation_not_wire | `lock : atomic_flag`; `active : _Atomic(SZrHotPatchVersionRecord *)`; `nextGeneration : atomic_uint_fast64_t`; `records : SZrHotPatchVersionRecord *`; `capacity : TZrUInt32`; `count : TZrUInt32` |
| `SZrHotPatchGenerationHandle` / [hotpatch_generation.h](../../zr_vm_core/include/zr_vm_core/hotpatch_generation.h) / runtime_generation_not_wire | `record : SZrHotPatchVersionRecord *`; `generation : TZrUInt64`; `leased : TZrBool` |
| `SZrHotPatchVersionView` / [hotpatch_generation.h](../../zr_vm_core/include/zr_vm_core/hotpatch_generation.h) / runtime_generation_not_wire | `generation : TZrUInt64`; `moduleHash : TZrUInt64`; `contentHash : TZrUInt64`; `publicContractHash : TZrUInt64`; `targetProfile : TZrUInt32`; `state : EZrHotPatchVersionState`; `leaseCount : TZrUInt32` |
| `SZrMetadataTokenRecord` / [metadata_token.h](../../zr_vm_core/include/zr_vm_core/metadata_token.h) / metadata_record_or_runtime_binding_codec_bridge_required | `token : TZrMetadataToken`; `relatedToken : TZrMetadataToken`; `ownerToken : TZrMetadataToken`; `ownerIndex : TZrUInt32`; `signatureBlobOffset : TZrUInt32`; `signatureBlobLength : TZrUInt32`; `signatureHash : TZrUInt64`; `layoutVersion : TZrUInt32`; `reserved0 : TZrUInt32`; `layoutHash : TZrUInt64`; `targetMetadataToken : TZrMetadataToken`; `targetSignatureToken : TZrMetadataToken`; `targetSignatureHash : TZrUInt64`; `targetModuleSignatureHash : TZrUInt64`; `requestedModuleVersion : struct SZrString *`; `minModuleVersionInclusive : struct SZrString *`; `maxModuleVersionExclusive : struct SZrString *` |
| `SZrMetadataStringHeapEntry` / [metadata_token.h](../../zr_vm_core/include/zr_vm_core/metadata_token.h) / metadata_record_or_runtime_binding_codec_bridge_required | `stringIndex : TZrUInt32`; `value : struct SZrString *` |
| `SZrMetadataTokenBinding` / [metadata_token.h](../../zr_vm_core/include/zr_vm_core/metadata_token.h) / metadata_record_or_runtime_binding_codec_bridge_required | `refToken : TZrMetadataToken`; `refSignatureToken : TZrMetadataToken`; `refSignatureHash : TZrUInt64`; `expectedMetadataToken : TZrMetadataToken`; `expectedSignatureToken : TZrMetadataToken`; `expectedSignatureHash : TZrUInt64`; `expectedModuleSignatureHash : TZrUInt64`; `expectedLayoutVersion : TZrUInt32`; `reserved0 : TZrUInt32`; `expectedLayoutHash : TZrUInt64`; `resolvedMetadataToken : TZrMetadataToken`; `resolvedSignatureToken : TZrMetadataToken`; `resolvedSignatureHash : TZrUInt64`; `resolvedModuleSignatureHash : TZrUInt64`; `resolvedLayoutVersion : TZrUInt32`; `reserved1 : TZrUInt32`; `resolvedLayoutHash : TZrUInt64` |
| `SZrAotGenericSlot` / [zr_aot_abi.h](../../zr_vm_common/include/zr_vm_common/zr_aot_abi.h) / process_local_aot_registration_or_generated_C_metadata_not_wire | `kind : TZrUInt32`; `typeLayoutId : TZrUInt32`; `metadataToken : TZrUInt32`; `methodIndex : TZrUInt32`; `flags : TZrUInt32`; `debugName : const TZrChar *`; `staticTypeLayout : const struct SZrTypeLayout *`; `staticMethod : FZrAotEntryThunk` |
| `SZrAotGenericResolvedSlot` / [zr_aot_abi.h](../../zr_vm_common/include/zr_vm_common/zr_aot_abi.h) / process_local_aot_registration_or_generated_C_metadata_not_wire | `kind : TZrUInt32`; `isResolved : TZrUInt8`; `reserved0 : TZrUInt8`; `reserved1 : TZrUInt8`; `reserved2 : TZrUInt8`; `value : SZrAotGenericResolvedValue` |
| `SZrAotGenericDictionary` / [zr_aot_abi.h](../../zr_vm_common/include/zr_vm_common/zr_aot_abi.h) / process_local_aot_registration_or_generated_C_metadata_not_wire | `slotCount : TZrUInt32`; `slots : const SZrAotGenericSlot *`; `resolvedSlots : SZrAotGenericResolvedSlot *` |
| `SZrAotSignatureType` / [zr_aot_abi.h](../../zr_vm_common/include/zr_vm_common/zr_aot_abi.h) / process_local_aot_registration_or_generated_C_metadata_not_wire | `baseType : TZrUInt16`; `staticCType : TZrUInt16`; `staticCTypeId : TZrUInt32`; `ownershipQualifier : TZrUInt32`; `elementBaseType : TZrUInt16`; `isNullable : TZrUInt8`; `isArray : TZrUInt8`; `passingMode : TZrUInt8`; `reserved0 : TZrUInt8`; `reserved1 : TZrUInt16` |
| `SZrAotSignature` / [zr_aot_abi.h](../../zr_vm_common/include/zr_vm_common/zr_aot_abi.h) / process_local_aot_registration_or_generated_C_metadata_not_wire | `parameterCount : TZrUInt32`; `returnType : const SZrAotSignatureType *`; `parameterTypes : const SZrAotSignatureType *`; `hasReturnValue : TZrUInt8`; `hasVarArgs : TZrUInt8` |
| `SZrAotGcRootSlot` / [zr_aot_abi.h](../../zr_vm_common/include/zr_vm_common/zr_aot_abi.h) / process_local_aot_registration_or_generated_C_metadata_not_wire | `stackSlot : TZrUInt32`; `frameByteOffset : TZrUInt32`; `typeLayoutId : TZrUInt32`; `fieldByteOffset : TZrUInt32`; `locationKind : TZrUInt8`; `reserved0 : TZrUInt8`; `reserved1 : TZrUInt16` |
| `SZrAotGcRootMap` / [zr_aot_abi.h](../../zr_vm_common/include/zr_vm_common/zr_aot_abi.h) / process_local_aot_registration_or_generated_C_metadata_not_wire | `rootCount : TZrUInt32`; `roots : const SZrAotGcRootSlot *` |
| `SZrAotMethodInfo` / [zr_aot_abi.h](../../zr_vm_common/include/zr_vm_common/zr_aot_abi.h) / process_local_aot_registration_or_generated_C_metadata_not_wire | `functionIndex : TZrUInt32`; `metadataFunction : const struct SZrFunction *`; `registerFrameBytes : TZrUInt32`; `gcRootMap : const struct SZrAotGcRootMap *`; `signature : const SZrAotSignature *`; `genericDictionary : const SZrAotGenericDictionary *`; `invoker : FZrAotReflectionInvoker`; `observationPolicy : TZrUInt8`; `reflectionMetadataLevel : TZrUInt8`; `reserved0 : TZrUInt8`; `reserved1 : TZrUInt8` |
| `SZrAotGcDescriptor` / [zr_aot_abi.h](../../zr_vm_common/include/zr_vm_common/zr_aot_abi.h) / process_local_aot_registration_or_generated_C_metadata_not_wire | `typeLayoutId : TZrUInt32`; `gcFieldCount : TZrUInt32`; `gcFieldOffsets : const TZrUInt32 *` |
| `SZrAotMemberTokenRemap` / [zr_aot_abi.h](../../zr_vm_common/include/zr_vm_common/zr_aot_abi.h) / process_local_aot_registration_or_generated_C_metadata_not_wire | `sourceToken : TZrUInt32`; `targetToken : TZrUInt32` |
| `SZrAotManifestExportEntry` / [zr_aot_abi.h](../../zr_vm_common/include/zr_vm_common/zr_aot_abi.h) / process_local_aot_registration_or_generated_C_metadata_not_wire | `kind : TZrUInt32`; `flags : TZrUInt32`; `target : const TZrChar *`; `typeToken : TZrUInt32`; `memberToken : TZrUInt32` |
| `SZrAotNativeImportRange` / [zr_aot_abi.h](../../zr_vm_common/include/zr_vm_common/zr_aot_abi.h) / process_local_aot_registration_or_generated_C_metadata_not_wire | `contractStart : TZrUInt32`; `contractCount : TZrUInt32` |
| `SZrAotCodeRegistration` / [zr_aot_abi.h](../../zr_vm_common/include/zr_vm_common/zr_aot_abi.h) / process_local_aot_registration_or_generated_C_metadata_not_wire | `functionCount : TZrUInt32`; `functionPointers : const FZrAotEntryThunk *`; `methodInfos : const SZrAotMethodInfo *const *`; `methodInfoCount : TZrUInt32`; `methodTokens : const TZrUInt32 *`; `methodTokenCount : TZrUInt32`; `memberTokenRemaps : const SZrAotMemberTokenRemap *`; `memberTokenRemapCount : TZrUInt32`; `manifestExports : const SZrAotManifestExportEntry *`; `manifestExportCount : TZrUInt32`; `invokers : const FZrAotReflectionInvoker *`; `invokerCount : TZrUInt32`; `typeLayouts : const struct SZrTypeLayout *const *`; `typeLayoutCount : TZrUInt32`; `typeLayoutTokens : const TZrUInt32 *`; `typeLayoutTokenCount : TZrUInt32`; `gcDescriptors : const SZrAotGcDescriptor *const *`; `gcDescriptorCount : TZrUInt32`; `nativeImportContracts : const struct SZrNativeImportContract *`; `nativeImportContractCount : TZrUInt32`; `nativeImportRanges : const SZrAotNativeImportRange *`; `nativeImportRangeCount : TZrUInt32`; `callBindingRows : const TZrByte *`; `callBindingRowCount : TZrUInt32`; `callBindingRowSize : TZrUInt32`; `callBindingTargetFunctionIndices : const TZrUInt32 *` |
| `ZrAotCompiledModule` / [zr_aot_abi.h](../../zr_vm_common/include/zr_vm_common/zr_aot_abi.h) / process_local_aot_registration_or_generated_C_metadata_not_wire | `abiVersion : TZrUInt32`; `backendKind : TZrUInt32`; `moduleName : const TZrChar *`; `inputKind : TZrUInt32`; `inputHash : const TZrChar *`; `runtimeContracts : const TZrChar *const *`; `embeddedModuleBlob : const TZrByte *`; `embeddedModuleBlobLength : TZrSize`; `functionThunks : const FZrAotEntryThunk *`; `functionThunkCount : TZrUInt32`; `entryThunk : FZrAotEntryThunk`; `methodInfos : const SZrAotMethodInfo *const *`; `methodInfoCount : TZrUInt32`; `methodTokens : const TZrUInt32 *`; `methodTokenCount : TZrUInt32`; `memberTokenRemaps : const SZrAotMemberTokenRemap *`; `memberTokenRemapCount : TZrUInt32`; `manifestExports : const SZrAotManifestExportEntry *`; `manifestExportCount : TZrUInt32`; `typeLayouts : const struct SZrTypeLayout *const *`; `typeLayoutCount : TZrUInt32`; `typeLayoutTokens : const TZrUInt32 *`; `typeLayoutTokenCount : TZrUInt32`; `gcDescriptors : const SZrAotGcDescriptor *const *`; `gcDescriptorCount : TZrUInt32`; `nativeImportContracts : const struct SZrNativeImportContract *`; `nativeImportContractCount : TZrUInt32`; `nativeImportRanges : const SZrAotNativeImportRange *`; `nativeImportRangeCount : TZrUInt32`; `codeRegistration : const SZrAotCodeRegistration *`; `callBindingRows : const TZrByte *`; `callBindingRowCount : TZrUInt32`; `callBindingRowSize : TZrUInt32`; `callBindingTargetFunctionIndices : const TZrUInt32 *` |
### Base60 /563 当前声明坐标桥

本表只纠正 base 声明的源码行号，不改变字段集合、类型语义或 wire 坐标。稳定 ID 为 `base_decl/<record>/<field>`：record 来自第一列、field 来自第三列 `field@L行号`，sourcepath 来自第二列，行号为当前声明的一基坐标。原 `persistent-field-audit.json` 的 base anchors 保留为历史值；本次精确修正分布为 +1×457、+2×69、+3×1、+4×36，不能统一按 +1 解释。runtime115 的当前精确坐标和 legacy527 的既有独立纠正保持各自分母，不并入本表。

| record / stable-ID 前缀 | 当前 sourcepath | field@currentL |
| --- | --- | --- |
| `SZrArtifactPublicIdentity` | [zr_vm_core/include/zr_vm_core/artifact_schema.h](../../zr_vm_core/include/zr_vm_core/artifact_schema.h) | `canonicalTypeId@L338`; `typeRefToken@L339`; `typeSpecToken@L340`; `signatureToken@L341`; `typeRefHash@L342`; `typeSpecHash@L343`; `signatureHash@L344`; `layoutVersion@L345`; `reserved0@L346`; `layoutHash@L347`; `callableContractHash@L348`; `moduleHash@L349` |
| `SZrArtifactTypeDefRow` | [zr_vm_core/include/zr_vm_core/artifact_schema.h](../../zr_vm_core/include/zr_vm_core/artifact_schema.h) | `token@L354`; `flags@L355`; `canonicalTypeId@L356`; `constructorToken@L357`; `constructorSignatureToken@L358`; `reserved0@L359`; `typeSignatureHash@L360`; `constructorContractHash@L361`; `reserved1@L362` |
| `SZrArtifactTypeIdentityRow` | [zr_vm_core/include/zr_vm_core/artifact_schema.h](../../zr_vm_core/include/zr_vm_core/artifact_schema.h) | `token@L367`; `signatureToken@L368`; `canonicalTypeId@L369`; `flags@L370`; `signatureOffset@L371`; `signatureLength@L372`; `signatureHash@L373`; `layoutVersion@L374`; `reserved0@L375`; `layoutHash@L376` |
| `SZrArtifactMemberDefRow` | [zr_vm_core/include/zr_vm_core/artifact_schema.h](../../zr_vm_core/include/zr_vm_core/artifact_schema.h) | `token@L381`; `ownerTypeToken@L382`; `signatureToken@L383`; `flags@L384`; `nameStringOffset@L385`; `reserved0@L386`; `signatureHash@L387`; `contractHash@L388` |
| `SZrArtifactPropertyDefRow` | [zr_vm_core/include/zr_vm_core/artifact_schema.h](../../zr_vm_core/include/zr_vm_core/artifact_schema.h) | `token@L393`; `ownerTypeToken@L394`; `getterToken@L395`; `setterToken@L396`; `signatureToken@L397`; `flags@L398`; `signatureHash@L399`; `contractHash@L400`; `initializerToken@L401`; `nameStringOffset@L402` |
| `SZrArtifactContractRow` | [zr_vm_core/include/zr_vm_core/artifact_schema.h](../../zr_vm_core/include/zr_vm_core/artifact_schema.h) | `memberToken@L407`; `signatureToken@L408`; `parameterCount@L409`; `flags@L410`; `receiverEffect@L411`; `refExportEffect@L412`; `escapeFlags@L413`; `abiLoweringKind@L414`; `contractHash@L415` |
| `SZrArtifactLayoutRow` | [zr_vm_core/include/zr_vm_core/artifact_schema.h](../../zr_vm_core/include/zr_vm_core/artifact_schema.h) | `typeToken@L420`; `version@L421`; `byteSize@L422`; `byteAlignment@L423`; `gcScanKind@L424`; `capabilityFlags@L425`; `ownershipMapOffset@L426`; `ownershipMapLength@L427`; `layoutHash@L428`; `stableSlotContractHash@L429` |
| `SZrArtifactMetadataStateRow` | [zr_vm_core/include/zr_vm_core/artifact_schema.h](../../zr_vm_core/include/zr_vm_core/artifact_schema.h) | `typeToken@L434`; `preservationState@L435`; `category@L436`; `metadataGeneration@L437`; `retainedMemberCount@L438`; `retainedPropertyCount@L439`; `retainedMetaRecordCount@L440`; `flags@L441`; `typeSignatureHash@L442`; `layoutHash@L443`; `callableContractHash@L444`; `metadataHash@L445` |
| `SZrArtifactMetadataRecordRow` | [zr_vm_core/include/zr_vm_core/artifact_schema.h](../../zr_vm_core/include/zr_vm_core/artifact_schema.h) | `ownerToken@L450`; `kind@L451`; `retention@L452`; `flags@L453`; `payloadOffset@L454`; `payloadLength@L455`; `metadataGeneration@L456`; `reserved0@L457`; `recordHash@L458` |
| `SZrArtifactDomainTransferRow` | [zr_vm_core/include/zr_vm_core/artifact_schema.h](../../zr_vm_core/include/zr_vm_core/artifact_schema.h) | `typeToken@L464`; `kind@L465`; `schemaVersion@L466`; `flags@L467`; `schemaOffset@L468`; `schemaLength@L469`; `providerToken@L470`; `reserved0@L471`; `schemaHash@L472`; `providerContractHash@L473` |
| `SZrArtifactSchedulerContractRow` | [zr_vm_core/include/zr_vm_core/artifact_schema.h](../../zr_vm_core/include/zr_vm_core/artifact_schema.h) | `schedulerTypeToken@L478`; `taskTypeToken@L479`; `jobTypeToken@L480`; `abiVersion@L481`; `policyMask@L482`; `attachedRequirementFlags@L483`; `isolatedRequirementFlags@L484`; `reserved0@L485`; `transportContractHash@L486`; `schedulerContractHash@L487` |
| `SZrArtifactRelocationRow` | [zr_vm_core/include/zr_vm_core/artifact_schema.h](../../zr_vm_core/include/zr_vm_core/artifact_schema.h) | `codeOffset@L492`; `kind@L493`; `targetToken@L494`; `targetSignatureToken@L495`; `expectedSignatureHash@L496`; `expectedContractHash@L497`; `expectedModuleHash@L498` |
| `SZrArtifactCallBindingRow` | [zr_vm_core/include/zr_vm_core/artifact_schema.h](../../zr_vm_core/include/zr_vm_core/artifact_schema.h) | `schemaVersion@L503`; `functionIndex@L504`; `cacheIndex@L505`; `instructionIndex@L506`; `contract@L507`; `location@L508` |
| `SZrCallBindingContract` | [zr_vm_core/include/zr_vm_core/call_binding.h](../../zr_vm_core/include/zr_vm_core/call_binding.h) | `bindingKind@L65`; `targetMetadataToken@L66`; `signatureToken@L67`; `ownerTypeToken@L68`; `signatureHash@L69`; `moduleSignatureHash@L70`; `layoutVersion@L71`; `dispatchSlot@L72`; `layoutHash@L73`; `operation@L74`; `reserved0@L75`; `reserved1@L76` |
| `SZrCallBindingLocation` | [zr_vm_core/include/zr_vm_core/call_binding.h](../../zr_vm_core/include/zr_vm_core/call_binding.h) | `kind@L34`; `targetIndex@L36`; `ownerDepth@L37`; `flags@L38` |
| `SZrExecutionContract` | [zr_vm_core/include/zr_vm_core/execution_contract.h](../../zr_vm_core/include/zr_vm_core/execution_contract.h) | `schemaVersion@L125`; `abiVersion@L126`; `logicalVersion@L127`; `reserved0@L128`; `generation@L129`; `targetToken@L130`; `reserved1@L131`; `signatureHash@L132`; `layoutHash@L133`; `moduleHash@L134`; `requiredCapabilities@L135`; `declaredEffects@L136` |
| `SZrExecIrProfileKey` | [zr_vm_parser/include/zr_vm_parser/exec_ir_profile.h](../../zr_vm_parser/include/zr_vm_parser/exec_ir_profile.h) | `moduleIdentity@L60`; `moduleHash@L61`; `irHash@L62`; `signatureHash@L63`; `layoutHash@L64`; `compilerAbi@L65` |
| `SZrExecIrProfileSite` | [zr_vm_parser/include/zr_vm_parser/exec_ir_profile.h](../../zr_vm_parser/include/zr_vm_parser/exec_ir_profile.h) | `siteKey@L69`; `functionToken@L70`; `instructionId@L71`; `sourceId@L72`; `kind@L73`; `targetToken@L74`; `receiverTypeToken@L75`; `numericTypeToken@L76`; `branchTaken@L77`; `branchNotTaken@L78`; `hitCount@L79`; `missCount@L80`; `allocationCount@L81`; `allocationBytes@L82`; `boxingCount@L83`; `cacheHitCount@L84`; `cacheMissCount@L85`; `deoptCount@L86`; `gcCount@L87`; `contentionCount@L88`; `distinctTargets@L89`; `megamorphic@L90` |
| `SZrExecIrSpecializationPolicy` | [zr_vm_parser/include/zr_vm_parser/exec_ir_profile.h](../../zr_vm_parser/include/zr_vm_parser/exec_ir_profile.h) | `maxVersionsPerSite@L94`; `maxVersionsPerFunction@L95`; `maxVersionsPerModule@L96`; `maxCodeBytesPerSite@L97`; `maxCodeBytesPerFunction@L98`; `maxCodeBytesPerModule@L99`; `minSamples@L100`; `minHitRatePermille@L101`; `maxMissRatePermille@L102`; `maxConsecutiveDeopts@L103`; `cooldownSamples@L104`; `maxPolymorphicTargets@L105` |
| `SZrExecIrProfile` | [zr_vm_parser/include/zr_vm_parser/exec_ir_profile.h](../../zr_vm_parser/include/zr_vm_parser/exec_ir_profile.h) | `magic@L111`; `schemaVersion@L112`; `key@L113`; `sites@L114`; `siteCount@L115`; `siteCapacity@L116`; `policy@L117` |
| `SZrCompileIrCacheInputs` | [zr_vm_parser/include/zr_vm_parser/compile_ir_cache.h](../../zr_vm_parser/include/zr_vm_parser/compile_ir_cache.h) | `sourceBytes@L12`; `sourceSize@L13`; `dependencyBytes@L14`; `dependencySize@L15`; `contractHash@L16`; `compilerAbi@L17`; `passPipelineHash@L18`; `targetHash@L19`; `numericPolicyHash@L20`; `layoutHash@L21`; `importedProfileHash@L22` |
| `SZrCompileIrCacheKey` | [zr_vm_parser/include/zr_vm_parser/compile_ir_cache.h](../../zr_vm_parser/include/zr_vm_parser/compile_ir_cache.h) | `schemaVersion@L26`; `profileHash@L27`; `contractHash@L28`; `compilerAbi@L29`; `passPipelineHash@L30`; `targetHash@L31`; `numericPolicyHash@L32`; `layoutHash@L33`; `importedProfileHash@L34`; `digest@L35`; `valid@L36` |
| `SZrCompileIrCacheHit` | [zr_vm_parser/include/zr_vm_parser/compile_ir_cache.h](../../zr_vm_parser/include/zr_vm_parser/compile_ir_cache.h) | `payload@L58`; `payloadSize@L59`; `accepted@L60` |
| `SZrCompileIrCache` | [zr_vm_parser/include/zr_vm_parser/compile_ir_cache.h](../../zr_vm_parser/include/zr_vm_parser/compile_ir_cache.h) | `entries@L66`; `count@L67`; `capacity@L68`; `nextWriterToken@L69`; `activeWriters@L70` |
| `SZrCompileIrCacheTransaction` | [zr_vm_parser/include/zr_vm_parser/compile_ir_cache.h](../../zr_vm_parser/include/zr_vm_parser/compile_ir_cache.h) | `cache@L74`; `key@L75`; `writerToken@L76`; `active@L77` |
| `SZrCompileOptimizationProfile` | [zr_vm_parser/include/zr_vm_parser/compile_optimization_profile.h](../../zr_vm_parser/include/zr_vm_parser/compile_optimization_profile.h) | `schemaVersion@L73`; `preset@L74`; `buildMode@L75`; `numericPermission@L76`; `target@L77`; `backend@L78`; `frameBudgetBytes@L79`; `passMask@L80`; `targetHash@L81`; `hostAbiHash@L82`; `frameSafe@L83`; `requestFastMath@L84`; `enableLto@L85`; `enablePgo@L86` |
| `SZrCompileEffectivePolicy` | [zr_vm_parser/include/zr_vm_parser/compile_optimization_profile.h](../../zr_vm_parser/include/zr_vm_parser/compile_optimization_profile.h) | `schemaVersion@L90`; `preset@L91`; `buildMode@L92`; `numericPermission@L93`; `target@L94`; `backend@L95`; `frameBudgetBytes@L96`; `passMask@L97`; `targetHash@L98`; `hostAbiHash@L99`; `profileHash@L100`; `frameSafe@L101`; `fastMathEnabled@L102`; `ltoEnabled@L103`; `pgoEnabled@L104` |
| `SZrHotPatchCapabilityRequirement` | [zr_vm_core/include/zr_vm_core/capability_manifest.h](../../zr_vm_core/include/zr_vm_core/capability_manifest.h) | `token@L31`; `requiredBits@L33`; `sourceOffset@L35`; `reserved@L37` |
| `SZrHotPatchCapabilityManifest` | [zr_vm_core/include/zr_vm_core/capability_manifest.h](../../zr_vm_core/include/zr_vm_core/capability_manifest.h) | `schemaVersion@L45`; `flags@L47`; `patchId@L49`; `contentHash@L51`; `baseModuleHash@L53`; `publicContractHash@L55`; `requiredCapabilities@L57`; `targetAbiVersion@L59`; `targetProfile@L61`; `requirementCount@L63`; `requirements@L65` |
| `SZrHotPatchValidationInput` | [zr_vm_core/include/zr_vm_core/capability_manifest.h](../../zr_vm_core/include/zr_vm_core/capability_manifest.h) | `artifact@L82`; `manifest@L84`; `loadedBaseModuleHash@L86`; `loadedPublicContractHash@L88`; `hostAbiVersion@L90`; `hostProfile@L92`; `hostAllowedCapabilities@L94`; `expectedPatchId@L96`; `expectedContentHash@L98`; `signature@L100`; `signatureLength@L102` |
| `SZrHotPatchZrafValidationInput` | [zr_vm_core/include/zr_vm_core/capability_manifest.h](../../zr_vm_core/include/zr_vm_core/capability_manifest.h) | `outerBytes@L122`; `outerLength@L124`; `expectedPublicIdentity@L126`; `manifest@L128`; `entryFunctionToken@L130`; `entrySignatureHash@L132`; `loadedBaseModuleHash@L134`; `loadedPublicContractHash@L136`; `hostAbiVersion@L138`; `hostProfile@L140`; `hostAllowedCapabilities@L142`; `expectedPatchId@L144`; `expectedContentHash@L146`; `signature@L148`; `signatureLength@L150` |
| `SZrValidatedHotPatch` | [zr_vm_core/include/zr_vm_core/capability_manifest.h](../../zr_vm_core/include/zr_vm_core/capability_manifest.h) | `artifact@L158`; `manifest@L160`; `contentHash@L162`; `contentBytes@L164`; `contentLength@L166`; `patchId@L168`; `publicContractHash@L170`; `requiredCapabilities@L172`; `validationPolicyHash@L174`; `targetProfile@L176`; `signatureVerified@L178`; `immutableContent@L180` |
| `SZrValidatedHotPatchZraf` | [zr_vm_core/include/zr_vm_core/capability_manifest.h](../../zr_vm_core/include/zr_vm_core/capability_manifest.h) | `outerBytes@L200`; `outerLength@L202`; `outerContentHash@L204`; `publicIdentity@L206`; `entryFunctionToken@L208`; `entrySignatureHash@L210`; `patchId@L212`; `publicContractHash@L214`; `requiredCapabilities@L216`; `validationPolicyHash@L218`; `targetAbiVersion@L220`; `targetProfile@L222`; `signatureVerified@L224`; `canonicalExecIrVerified@L226` |
| `SZrExecutionGenerationKey` | [zr_vm_core/include/zr_vm_core/execution_backend.h](../../zr_vm_core/include/zr_vm_core/execution_backend.h) | `domainIdentity@L171`; `moduleIdentity@L172`; `generation@L173`; `backendRegistrationIdentity@L174` |
| `SZrExecutionBackendTarget` | [zr_vm_core/include/zr_vm_core/execution_backend.h](../../zr_vm_core/include/zr_vm_core/execution_backend.h) | `schemaVersion@L183`; `kind@L184`; `abiVersion@L185`; `reserved@L186`; `targetTripleHash@L187`; `layoutHash@L188`; `capabilityHash@L189` |
| `SZrExecutionCompileRequest` | [zr_vm_core/include/zr_vm_core/execution_backend.h](../../zr_vm_core/include/zr_vm_core/execution_backend.h) | `magic@L213`; `schemaVersion@L214`; `flags@L215`; `requestedTarget@L216`; `generationKey@L217`; `contract@L218`; `immutableIrHash@L219`; `compileInputHash@L220`; `sourceId@L221`; `instructionId@L222`; `requiredOperations@L223`; `requiredMapFlags@L224` |
| `SZrExecutionCompileTicket` | [zr_vm_core/include/zr_vm_core/execution_backend.h](../../zr_vm_core/include/zr_vm_core/execution_backend.h) | `magic@L230`; `schemaVersion@L231`; `serviceIdentity@L232`; `ticketId@L233`; `generationKey@L234`; `target@L235`; `fallback@L236`; `state@L237` |
| `SZrExecutionBackendCodeInfo` | [zr_vm_core/include/zr_vm_core/execution_backend.h](../../zr_vm_core/include/zr_vm_core/execution_backend.h) | `magic@L251`; `schemaVersion@L252`; `generationKey@L253`; `contract@L254`; `immutableIrHash@L255`; `compileInputHash@L256`; `codeIdentity@L257`; `codeSize@L258`; `mapRegistrationFlags@L259`; `rootMapHash@L260`; `ehMapHash@L261`; `debugMapHash@L262`; `deoptMapHash@L263`; `runtimeImportsHash@L264` |
| `SZrExecutionCodeHandle` | [zr_vm_core/include/zr_vm_core/execution_backend.h](../../zr_vm_core/include/zr_vm_core/execution_backend.h) | `magic@L296`; `slotIndex@L297`; `serviceIdentity@L298`; `codeIdentity@L299`; `generationKey@L300`; `leased@L301`; `dependencyLeased@L302`; `reserved@L303` |
| `SZrHotPatchVersionRecord` | [zr_vm_core/include/zr_vm_core/hotpatch_generation.h](../../zr_vm_core/include/zr_vm_core/hotpatch_generation.h) | `generation@L29`; `moduleHash@L30`; `contentHash@L31`; `publicContractHash@L32`; `targetProfile@L33`; `state@L34`; `leaseCount@L35` |
| `SZrHotPatchGenerationManager` | [zr_vm_core/include/zr_vm_core/hotpatch_generation.h](../../zr_vm_core/include/zr_vm_core/hotpatch_generation.h) | `lock@L41`; `active@L42`; `nextGeneration@L43`; `records@L44`; `capacity@L45`; `count@L46` |
| `SZrHotPatchGenerationHandle` | [zr_vm_core/include/zr_vm_core/hotpatch_generation.h](../../zr_vm_core/include/zr_vm_core/hotpatch_generation.h) | `record@L52`; `generation@L53`; `leased@L54` |
| `SZrHotPatchVersionView` | [zr_vm_core/include/zr_vm_core/hotpatch_generation.h](../../zr_vm_core/include/zr_vm_core/hotpatch_generation.h) | `generation@L59`; `moduleHash@L60`; `contentHash@L61`; `publicContractHash@L62`; `targetProfile@L63`; `state@L64`; `leaseCount@L65` |
| `SZrMetadataTokenRecord` | [zr_vm_core/include/zr_vm_core/metadata_token.h](../../zr_vm_core/include/zr_vm_core/metadata_token.h) | `token@L289`; `relatedToken@L293`; `ownerToken@L297`; `ownerIndex@L301`; `signatureBlobOffset@L305`; `signatureBlobLength@L309`; `signatureHash@L313`; `layoutVersion@L317`; `reserved0@L321`; `layoutHash@L325`; `targetMetadataToken@L329`; `targetSignatureToken@L333`; `targetSignatureHash@L337`; `targetModuleSignatureHash@L341`; `requestedModuleVersion@L345`; `minModuleVersionInclusive@L349`; `maxModuleVersionExclusive@L353` |
| `SZrMetadataStringHeapEntry` | [zr_vm_core/include/zr_vm_core/metadata_token.h](../../zr_vm_core/include/zr_vm_core/metadata_token.h) | `stringIndex@L470`; `value@L474` |
| `SZrMetadataTokenBinding` | [zr_vm_core/include/zr_vm_core/metadata_token.h](../../zr_vm_core/include/zr_vm_core/metadata_token.h) | `refToken@L516`; `refSignatureToken@L520`; `refSignatureHash@L524`; `expectedMetadataToken@L528`; `expectedSignatureToken@L532`; `expectedSignatureHash@L536`; `expectedModuleSignatureHash@L540`; `expectedLayoutVersion@L544`; `reserved0@L548`; `expectedLayoutHash@L552`; `resolvedMetadataToken@L556`; `resolvedSignatureToken@L560`; `resolvedSignatureHash@L564`; `resolvedModuleSignatureHash@L568`; `resolvedLayoutVersion@L572`; `reserved1@L576`; `resolvedLayoutHash@L580` |
| `SZrAotGenericSlot` | [zr_vm_common/include/zr_vm_common/zr_aot_abi.h](../../zr_vm_common/include/zr_vm_common/zr_aot_abi.h) | `kind@L78`; `typeLayoutId@L79`; `metadataToken@L80`; `methodIndex@L81`; `flags@L82`; `debugName@L83`; `staticTypeLayout@L84`; `staticMethod@L85` |
| `SZrAotGenericResolvedSlot` | [zr_vm_common/include/zr_vm_common/zr_aot_abi.h](../../zr_vm_common/include/zr_vm_common/zr_aot_abi.h) | `kind@L90`; `isResolved@L91`; `reserved0@L92`; `reserved1@L93`; `reserved2@L94`; `value@L95` |
| `SZrAotGenericDictionary` | [zr_vm_common/include/zr_vm_common/zr_aot_abi.h](../../zr_vm_common/include/zr_vm_common/zr_aot_abi.h) | `slotCount@L104`; `slots@L105`; `resolvedSlots@L106` |
| `SZrAotSignatureType` | [zr_vm_common/include/zr_vm_common/zr_aot_abi.h](../../zr_vm_common/include/zr_vm_common/zr_aot_abi.h) | `baseType@L126`; `staticCType@L127`; `staticCTypeId@L128`; `ownershipQualifier@L129`; `elementBaseType@L130`; `isNullable@L131`; `isArray@L132`; `passingMode@L133`; `reserved0@L134`; `reserved1@L135` |
| `SZrAotSignature` | [zr_vm_common/include/zr_vm_common/zr_aot_abi.h](../../zr_vm_common/include/zr_vm_common/zr_aot_abi.h) | `parameterCount@L143`; `returnType@L144`; `parameterTypes@L145`; `hasReturnValue@L146`; `hasVarArgs@L147` |
| `SZrAotGcRootSlot` | [zr_vm_common/include/zr_vm_common/zr_aot_abi.h](../../zr_vm_common/include/zr_vm_common/zr_aot_abi.h) | `stackSlot@L162`; `frameByteOffset@L163`; `typeLayoutId@L164`; `fieldByteOffset@L165`; `locationKind@L166`; `reserved0@L167`; `reserved1@L168` |
| `SZrAotGcRootMap` | [zr_vm_common/include/zr_vm_common/zr_aot_abi.h](../../zr_vm_common/include/zr_vm_common/zr_aot_abi.h) | `rootCount@L173`; `roots@L174` |
| `SZrAotMethodInfo` | [zr_vm_common/include/zr_vm_common/zr_aot_abi.h](../../zr_vm_common/include/zr_vm_common/zr_aot_abi.h) | `functionIndex@L182`; `metadataFunction@L183`; `registerFrameBytes@L184`; `gcRootMap@L185`; `signature@L186`; `genericDictionary@L187`; `invoker@L188`; `observationPolicy@L191`; `reflectionMetadataLevel@L192`; `reserved0@L193`; `reserved1@L194` |
| `SZrAotGcDescriptor` | [zr_vm_common/include/zr_vm_common/zr_aot_abi.h](../../zr_vm_common/include/zr_vm_common/zr_aot_abi.h) | `typeLayoutId@L199`; `gcFieldCount@L200`; `gcFieldOffsets@L201` |
| `SZrAotMemberTokenRemap` | [zr_vm_common/include/zr_vm_common/zr_aot_abi.h](../../zr_vm_common/include/zr_vm_common/zr_aot_abi.h) | `sourceToken@L209`; `targetToken@L210` |
| `SZrAotManifestExportEntry` | [zr_vm_common/include/zr_vm_common/zr_aot_abi.h](../../zr_vm_common/include/zr_vm_common/zr_aot_abi.h) | `kind@L225`; `flags@L226`; `target@L227`; `typeToken@L228`; `memberToken@L229` |
| `SZrAotNativeImportRange` | [zr_vm_common/include/zr_vm_common/zr_aot_abi.h](../../zr_vm_common/include/zr_vm_common/zr_aot_abi.h) | `contractStart@L237`; `contractCount@L238` |
| `SZrAotCodeRegistration` | [zr_vm_common/include/zr_vm_common/zr_aot_abi.h](../../zr_vm_common/include/zr_vm_common/zr_aot_abi.h) | `functionCount@L253`; `functionPointers@L254`; `methodInfos@L255`; `methodInfoCount@L256`; `methodTokens@L257`; `methodTokenCount@L258`; `memberTokenRemaps@L259`; `memberTokenRemapCount@L260`; `manifestExports@L261`; `manifestExportCount@L262`; `invokers@L263`; `invokerCount@L264`; `typeLayouts@L265`; `typeLayoutCount@L266`; `typeLayoutTokens@L267`; `typeLayoutTokenCount@L268`; `gcDescriptors@L269`; `gcDescriptorCount@L270`; `nativeImportContracts@L271`; `nativeImportContractCount@L272`; `nativeImportRanges@L273`; `nativeImportRangeCount@L274`; `callBindingRows@L275`; `callBindingRowCount@L276`; `callBindingRowSize@L277`; `callBindingTargetFunctionIndices@L278` |
| `ZrAotCompiledModule` | [zr_vm_common/include/zr_vm_common/zr_aot_abi.h](../../zr_vm_common/include/zr_vm_common/zr_aot_abi.h) | `abiVersion@L289`; `backendKind@L290`; `moduleName@L291`; `inputKind@L292`; `inputHash@L293`; `runtimeContracts@L294`; `embeddedModuleBlob@L295`; `embeddedModuleBlobLength@L296`; `functionThunks@L297`; `functionThunkCount@L298`; `entryThunk@L299`; `methodInfos@L300`; `methodInfoCount@L301`; `methodTokens@L302`; `methodTokenCount@L303`; `memberTokenRemaps@L304`; `memberTokenRemapCount@L305`; `manifestExports@L306`; `manifestExportCount@L307`; `typeLayouts@L308`; `typeLayoutCount@L309`; `typeLayoutTokens@L310`; `typeLayoutTokenCount@L311`; `gcDescriptors@L312`; `gcDescriptorCount@L313`; `nativeImportContracts@L314`; `nativeImportContractCount@L315`; `nativeImportRanges@L316`; `nativeImportRangeCount@L317`; `codeRegistration@L318`; `callBindingRows@L319`; `callBindingRowCount@L320`; `callBindingRowSize@L321`; `callBindingTargetFunctionIndices@L322` |

### Legacy65 logical declarations

唯一 declaration owner 为该 header 所属 Core/common 模块；encoder/reader 仍由 L 的明确 field event 定位。未解析的声明保留 literal，不伪造类型。

| record / header | 全字段 : declaration type |
| --- | --- |
| `SZrIoFunctionCatchClause` / [io.h](../../zr_vm_core/include/zr_vm_core/io.h) | `*typeName : struct SZrString`; `targetInstructionOffset : TZrUInt64` |
| `SZrIoFunctionExceptionHandler` / [io.h](../../zr_vm_core/include/zr_vm_core/io.h) | `protectedStartInstructionOffset : TZrUInt64`; `finallyTargetInstructionOffset : TZrUInt64`; `afterFinallyInstructionOffset : TZrUInt64`; `catchClauseStartIndex : TZrUInt32`; `catchClauseCount : TZrUInt32`; `hasFinally : TZrUInt8` |
| `SZrIoFunctionTypedTypeRef` / [io.h](../../zr_vm_core/include/zr_vm_core/io.h) | `baseType : EZrValueType`; `isNullable : TZrBool`; `ownershipQualifier : TZrUInt32`; `isArray : TZrBool`; `*typeName : struct SZrString`; `elementBaseType : EZrValueType`; `*elementTypeName : struct SZrString`; `staticCType : EZrStaticCType`; `staticCTypeId : TZrUInt32` |
| `SZrIoFunctionTypedGenericParameter` / [io.h](../../zr_vm_core/include/zr_vm_core/io.h) | `*name : struct SZrString`; `genericKind : TZrUInt8`; `variance : TZrUInt8`; `requiresClass : TZrUInt8`; `requiresStruct : TZrUInt8`; `requiresNew : TZrUInt8`; `requiresOwner : TZrUInt8`; `reserved0 : TZrUInt16`; `requiredOwnershipQualifier : TZrUInt32`; `constraintTypeCount : TZrUInt32`; `<unparsed declaration> : None; unparsed=struct SZrString **constraintTypeNames` |
| `SZrIoFunctionTypedLocalBinding` / [io.h](../../zr_vm_core/include/zr_vm_core/io.h) | `*name : struct SZrString`; `stackSlot : TZrUInt32`; `type : SZrIoFunctionTypedTypeRef`; `symbolId : TZrUInt32`; `typeId : TZrUInt32`; `placeId : TZrUInt32`; `declarationStartLine : TZrUInt32`; `declarationStartColumn : TZrUInt32`; `declarationEndLine : TZrUInt32`; `declarationEndColumn : TZrUInt32`; `roleFlags : TZrUInt32` |
| `SZrIoFunctionTypedClosureBinding` / [io.h](../../zr_vm_core/include/zr_vm_core/io.h) | `captureIndex : TZrUInt32`; `type : SZrIoFunctionTypedTypeRef`; `symbolId : TZrUInt32`; `typeId : TZrUInt32`; `declarationStartLine : TZrUInt32`; `declarationStartColumn : TZrUInt32`; `declarationEndLine : TZrUInt32`; `declarationEndColumn : TZrUInt32` |
| `SZrIoFunctionTypedExportSymbol` / [io.h](../../zr_vm_core/include/zr_vm_core/io.h) | `*name : struct SZrString`; `stackSlot : TZrUInt32`; `accessModifier : TZrUInt8`; `symbolKind : TZrUInt8`; `exportKind : TZrUInt8`; `readiness : TZrUInt8`; `reserved0 : TZrUInt16`; `callableChildIndex : TZrUInt32`; `valueType : SZrIoFunctionTypedTypeRef`; `parameterCount : TZrSize`; `*parameterTypes : SZrIoFunctionTypedTypeRef`; `genericParameterCount : TZrSize`; `*genericParameters : SZrIoFunctionTypedGenericParameter`; `lineInSourceStart : TZrUInt32`; `columnInSourceStart : TZrUInt32`; `lineInSourceEnd : TZrUInt32`; `columnInSourceEnd : TZrUInt32`; `metadataToken : TZrMetadataToken`; `signatureToken : TZrMetadataToken`; `signatureBlobOffset : TZrUInt32`; `signatureBlobLength : TZrUInt32`; `signatureHash : TZrUInt64` |
| `SZrIoFunctionModuleEffect` / [io.h](../../zr_vm_core/include/zr_vm_core/io.h) | `kind : TZrUInt8`; `exportKind : TZrUInt8`; `readiness : TZrUInt8`; `reserved0 : TZrUInt8`; `*moduleName : struct SZrString`; `*assemblyName : struct SZrString`; `*symbolName : struct SZrString`; `lineInSourceStart : TZrUInt32`; `columnInSourceStart : TZrUInt32`; `lineInSourceEnd : TZrUInt32`; `columnInSourceEnd : TZrUInt32`; `targetMetadataToken : TZrMetadataToken`; `targetSignatureToken : TZrMetadataToken`; `targetSignatureHash : TZrUInt64`; `targetModuleSignatureHash : TZrUInt64`; `*requestedModuleVersion : struct SZrString`; `*minModuleVersionInclusive : struct SZrString`; `*maxModuleVersionExclusive : struct SZrString` |
| `SZrIoFunctionCallableSummary` / [io.h](../../zr_vm_core/include/zr_vm_core/io.h) | `*name : struct SZrString`; `callableChildIndex : TZrUInt32`; `effectCount : TZrSize`; `*effects : SZrIoFunctionModuleEffect` |
| `SZrIoFunctionTopLevelCallableBinding` / [io.h](../../zr_vm_core/include/zr_vm_core/io.h) | `*name : struct SZrString`; `stackSlot : TZrUInt32`; `callableChildIndex : TZrUInt32`; `accessModifier : TZrUInt8`; `exportKind : TZrUInt8`; `readiness : TZrUInt8`; `reserved0 : TZrUInt8` |
| `SZrIoFunctionMetadataParameter` / [io.h](../../zr_vm_core/include/zr_vm_core/io.h) | `*name : struct SZrString`; `type : SZrIoFunctionTypedTypeRef`; `hasDefaultValue : TZrBool`; `defaultValue : SZrIoFunctionConstantVariable`; `hasDecoratorMetadata : TZrUInt8`; `decoratorMetadataValue : SZrIoFunctionConstantVariable`; `decoratorNamesLength : TZrSize`; `<unparsed declaration> : None; unparsed=struct SZrString **decoratorNames` |
| `SZrIoFunctionCompileTimePathBinding` / [io.h](../../zr_vm_core/include/zr_vm_core/io.h) | `*path : struct SZrString`; `targetKind : TZrUInt8`; `*targetName : struct SZrString` |
| `SZrIoFunctionCompileTimeVariableInfo` / [io.h](../../zr_vm_core/include/zr_vm_core/io.h) | `*name : struct SZrString`; `type : SZrIoFunctionTypedTypeRef`; `lineInSourceStart : TZrUInt32`; `lineInSourceEnd : TZrUInt32`; `pathBindingsLength : TZrSize`; `*pathBindings : SZrIoFunctionCompileTimePathBinding` |
| `SZrIoFunctionCompileTimeFunctionInfo` / [io.h](../../zr_vm_core/include/zr_vm_core/io.h) | `*name : struct SZrString`; `returnType : SZrIoFunctionTypedTypeRef`; `parameterCount : TZrSize`; `*parameters : SZrIoFunctionMetadataParameter`; `lineInSourceStart : TZrUInt32`; `lineInSourceEnd : TZrUInt32` |
| `SZrIoFunctionEscapeBinding` / [io.h](../../zr_vm_core/include/zr_vm_core/io.h) | `*name : struct SZrString`; `slotOrIndex : TZrUInt32`; `scopeDepth : TZrUInt32`; `escapeFlags : TZrUInt32`; `bindingKind : TZrUInt8`; `reserved0 : TZrUInt8`; `reserved1 : TZrUInt16` |
| `SZrIoFunctionMemberEntry` / [io.h](../../zr_vm_core/include/zr_vm_core/io.h) | `*symbol : struct SZrString`; `entryKind : TZrUInt8`; `reserved0 : TZrUInt8`; `reserved1 : TZrUInt16`; `prototypeIndex : TZrUInt32`; `descriptorIndex : TZrUInt32` |
| `SZrIoFunctionFrameSlotLayout` / [io.h](../../zr_vm_core/include/zr_vm_core/io.h) | `stackSlot : TZrUInt32`; `byteOffset : TZrUInt32`; `byteSize : TZrUInt32`; `byteAlign : TZrUInt32`; `typeLayoutId : TZrUInt32`; `slotKind : TZrUInt8`; `isParameter : TZrUInt8`; `reserved0 : TZrUInt16` |
| `SZrIoSemIrOwnershipEntry` / [io.h](../../zr_vm_core/include/zr_vm_core/io.h) | `state : TZrUInt32` |
| `SZrIoSemIrEffectEntry` / [io.h](../../zr_vm_core/include/zr_vm_core/io.h) | `kind : TZrUInt32`; `instructionIndex : TZrUInt32`; `ownershipInputIndex : TZrUInt32`; `ownershipOutputIndex : TZrUInt32` |
| `SZrIoSemIrBlockEntry` / [io.h](../../zr_vm_core/include/zr_vm_core/io.h) | `blockId : TZrUInt32`; `firstInstructionIndex : TZrUInt32`; `instructionCount : TZrUInt32` |
| `SZrIoSemIrInstruction` / [io.h](../../zr_vm_core/include/zr_vm_core/io.h) | `opcode : TZrUInt32`; `execInstructionIndex : TZrUInt32`; `typeTableIndex : TZrUInt32`; `effectTableIndex : TZrUInt32`; `destinationSlot : TZrUInt32`; `operand0 : TZrUInt32`; `operand1 : TZrUInt32`; `deoptId : TZrUInt32` |
| `SZrIoSemIrDeoptEntry` / [io.h](../../zr_vm_core/include/zr_vm_core/io.h) | `deoptId : TZrUInt32`; `execInstructionIndex : TZrUInt32` |
| `SZrIoFunctionCallSiteCacheEntry` / [io.h](../../zr_vm_core/include/zr_vm_core/io.h) | `kind : TZrUInt32`; `instructionIndex : TZrUInt32`; `memberEntryIndex : TZrUInt32`; `deoptId : TZrUInt32`; `argumentCount : TZrUInt32`; `bindingContract : SZrCallBindingContract`; `bindingLocation : SZrCallBindingLocation` |
| `SZrIoInstructionSourceRange` / [io.h](../../zr_vm_core/include/zr_vm_core/io.h) | `startLine : TZrUInt32`; `startColumn : TZrUInt32`; `endLine : TZrUInt32`; `endColumn : TZrUInt32` |
| `SZrCompiledPrototypeInfo` / [constant_reference.h](../../zr_vm_core/include/zr_vm_core/constant_reference.h) | `nameStringIndex : TZrUInt32`; `type : TZrUInt32`; `accessModifier : TZrUInt32`; `inheritsCount : TZrUInt32`; `membersCount : TZrUInt32`; `protocolMask : TZrUInt64`; `hasDecoratorMetadata : TZrUInt32`; `decoratorMetadataConstantIndex : TZrUInt32`; `decoratorsCount : TZrUInt32`; `modifierFlags : TZrUInt32`; `nextVirtualSlotIndex : TZrUInt32`; `nextPropertyIdentity : TZrUInt32`; `layoutByteSize : TZrUInt32`; `layoutByteAlign : TZrUInt32` |
| `SZrCompiledMemberInfo` / [constant_reference.h](../../zr_vm_core/include/zr_vm_core/constant_reference.h) | `memberType : TZrUInt32`; `nameStringIndex : TZrUInt32`; `accessModifier : TZrUInt32`; `isStatic : TZrUInt32`; `isConst : TZrUInt32`; `fieldTypeNameStringIndex : TZrUInt32`; `fieldOffset : TZrUInt32`; `fieldSize : TZrUInt32`; `isMetaMethod : TZrUInt32`; `metaType : TZrUInt32`; `functionConstantIndex : TZrUInt32`; `parameterCount : TZrUInt32`; `returnTypeNameStringIndex : TZrUInt32`; `reservedRemovedUsingManaged : TZrUInt32`; `ownershipQualifier : TZrUInt32`; `callsClose : TZrUInt32`; `callsDestructor : TZrUInt32`; `declarationOrder : TZrUInt32`; `contractRole : TZrUInt32`; `hasDecoratorMetadata : TZrUInt32`; `decoratorMetadataConstantIndex : TZrUInt32`; `hasDecoratorNames : TZrUInt32`; `decoratorNamesConstantIndex : TZrUInt32`; `modifierFlags : TZrUInt32`; `ownerTypeNameStringIndex : TZrUInt32`; `baseDefinitionOwnerTypeNameStringIndex : TZrUInt32`; `baseDefinitionNameStringIndex : TZrUInt32`; `virtualSlotIndex : TZrUInt32`; `interfaceContractSlot : TZrUInt32`; `propertyIdentity : TZrUInt32`; `accessorRole : TZrUInt32` |
| `SZrConstantReferencePath` / [constant_reference.h](../../zr_vm_core/include/zr_vm_core/constant_reference.h) | `depth : TZrUInt32`; `*steps : TZrUInt32`; `type : EZrValueType` |
| `SZrFunctionMemberEntry` / [function.h](../../zr_vm_core/include/zr_vm_core/function.h) | `*symbol : struct SZrString`; `entryKind : TZrUInt8`; `reserved0 : TZrUInt8`; `reserved1 : TZrUInt16`; `prototypeIndex : TZrUInt32`; `descriptorIndex : TZrUInt32` |
| `SZrFunctionFrameIndirectAliasBinding` / [function.h](../../zr_vm_core/include/zr_vm_core/function.h) | `ownerStackSlot : TZrUInt32`; `ownerByteOffset : TZrUInt32` |
| `SZrFunctionFrameBorrowedAliasBinding` / [function.h](../../zr_vm_core/include/zr_vm_core/function.h) | `*sourceCallInfo : struct SZrCallInfo`; `sourceFrameBase : SZrFunctionStackAnchor`; `sourceStackSlot : TZrUInt32`; `reserved0 : TZrUInt32` |
| `SZrFunctionFrameSlotLayout` / [function.h](../../zr_vm_core/include/zr_vm_core/function.h) | `stackSlot : TZrUInt32`; `byteOffset : TZrUInt32`; `byteSize : TZrUInt32`; `byteAlign : TZrUInt32`; `typeLayoutId : TZrUInt32`; `slotKind : TZrUInt8`; `isParameter : TZrUInt8`; `reserved0 : TZrUInt16` |
| `SZrFunctionFrameFieldLayout` / [function.h](../../zr_vm_core/include/zr_vm_core/function.h) | `byteOffset : TZrUInt32`; `byteSize : TZrUInt32`; `typeLayoutId : TZrUInt32`; `valueType : EZrValueType`; `isPrimitivePod : TZrUInt8`; `isValueSlot : TZrUInt8`; `reserved0 : TZrUInt16` |
| `SZrFunctionExportedVariable` / [function.h](../../zr_vm_core/include/zr_vm_core/function.h) | `*name : struct SZrString`; `stackSlot : TZrUInt32`; `accessModifier : TZrUInt8`; `exportKind : TZrUInt8`; `readiness : TZrUInt8`; `reserved0 : TZrUInt8`; `callableChildIndex : TZrUInt32` |
| `SZrFunctionTypedTypeRef` / [function.h](../../zr_vm_core/include/zr_vm_core/function.h) | `baseType : EZrValueType`; `isNullable : TZrBool`; `ownershipQualifier : TZrUInt32`; `isArray : TZrBool`; `*typeName : struct SZrString`; `elementBaseType : EZrValueType`; `*elementTypeName : struct SZrString`; `staticCType : EZrStaticCType`; `staticCTypeId : TZrUInt32` |
| `SZrFunctionTypedGenericParameter` / [function.h](../../zr_vm_core/include/zr_vm_core/function.h) | `*name : struct SZrString`; `genericKind : TZrUInt8`; `variance : TZrUInt8`; `requiresClass : TZrUInt8`; `requiresStruct : TZrUInt8`; `requiresNew : TZrUInt8`; `requiresOwner : TZrUInt8`; `reserved0 : TZrUInt16`; `requiredOwnershipQualifier : TZrUInt32`; `constraintTypeCount : TZrUInt32`; `<unparsed declaration> : None; unparsed=struct SZrString **constraintTypeNames` |
| `SZrFunctionTypedLocalBinding` / [function.h](../../zr_vm_core/include/zr_vm_core/function.h) | `*name : struct SZrString`; `stackSlot : TZrUInt32`; `type : SZrFunctionTypedTypeRef`; `symbolId : TZrUInt32`; `typeId : TZrUInt32`; `placeId : TZrUInt32`; `declarationStartLine : TZrUInt32`; `declarationStartColumn : TZrUInt32`; `declarationEndLine : TZrUInt32`; `declarationEndColumn : TZrUInt32`; `roleFlags : TZrUInt32` |
| `SZrFunctionTypedClosureBinding` / [function.h](../../zr_vm_core/include/zr_vm_core/function.h) | `captureIndex : TZrUInt32`; `type : SZrFunctionTypedTypeRef`; `symbolId : TZrUInt32`; `typeId : TZrUInt32`; `declarationStartLine : TZrUInt32`; `declarationStartColumn : TZrUInt32`; `declarationEndLine : TZrUInt32`; `declarationEndColumn : TZrUInt32` |
| `SZrFunctionSourceRange` / [function.h](../../zr_vm_core/include/zr_vm_core/function.h) | `startLine : TZrUInt32`; `startColumn : TZrUInt32`; `endLine : TZrUInt32`; `endColumn : TZrUInt32` |
| `SZrFunctionSourceCallableIdentity` / [function.h](../../zr_vm_core/include/zr_vm_core/function.h) | `schemaVersion : TZrUInt32`; `symbolId : TZrUInt32`; `typeId : TZrUInt32`; `canonicalSignatureHash : TZrUInt64`; `returnPrimitive : TZrUInt32`; `parameterCount : TZrUInt32`; `receiverFlags : TZrUInt32`; `effectFlags : TZrUInt32`; `declarationRange : SZrFunctionSourceRange`; `hasExplicitNoArgsI64 : TZrBool` |
| `SZrFunctionArtifactSourceTypeIdentity` / [function.h](../../zr_vm_core/include/zr_vm_core/function.h) | `metadataToken : TZrMetadataToken`; `signatureToken : TZrMetadataToken`; `signatureHash : TZrUInt64`; `layoutVersion : TZrUInt32`; `reserved0 : TZrUInt32`; `layoutHash : TZrUInt64`; `moduleSignatureHash : TZrUInt64` |
| `SZrFunctionSchedulerSourceFact` / [function.h](../../zr_vm_core/include/zr_vm_core/function.h) | `schedulerTypeId : TZrUInt32`; `taskTypeId : TZrUInt32`; `jobTypeId : TZrUInt32`; `schedulerAbiVersion : TZrUInt32`; `scheduleMemberToken : TZrMetadataToken`; `scheduleSignatureToken : TZrMetadataToken`; `scheduleSignatureHash : TZrUInt64`; `schedulerProtocolMask : TZrUInt64`; `contractRole : TZrUInt32`; `schedulerPolicyMask : TZrUInt32`; `attachedRequirementFlags : TZrUInt32`; `isolatedRequirementFlags : TZrUInt32`; `reserved0 : TZrUInt32`; `transportContractHash : TZrUInt64`; `schedulerContractHash : TZrUInt64`; `schedulerProvider : SZrFunctionArtifactSourceTypeIdentity`; `taskProvider : SZrFunctionArtifactSourceTypeIdentity`; `jobProvider : SZrFunctionArtifactSourceTypeIdentity` |
| `SZrFunctionTypedExportSymbol` / [function.h](../../zr_vm_core/include/zr_vm_core/function.h) | `*name : struct SZrString`; `stackSlot : TZrUInt32`; `accessModifier : TZrUInt8`; `symbolKind : TZrUInt8`; `exportKind : TZrUInt8`; `readiness : TZrUInt8`; `reserved0 : TZrUInt16`; `callableChildIndex : TZrUInt32`; `valueType : SZrFunctionTypedTypeRef`; `parameterCount : TZrUInt32`; `*parameterTypes : SZrFunctionTypedTypeRef`; `genericParameterCount : TZrUInt32`; `*genericParameters : SZrFunctionTypedGenericParameter`; `lineInSourceStart : TZrUInt32`; `columnInSourceStart : TZrUInt32`; `lineInSourceEnd : TZrUInt32`; `columnInSourceEnd : TZrUInt32`; `metadataToken : TZrMetadataToken`; `signatureToken : TZrMetadataToken`; `signatureBlobOffset : TZrUInt32`; `signatureBlobLength : TZrUInt32`; `signatureHash : TZrUInt64` |
| `SZrFunctionModuleEffect` / [function.h](../../zr_vm_core/include/zr_vm_core/function.h) | `kind : TZrUInt8`; `exportKind : TZrUInt8`; `readiness : TZrUInt8`; `reserved0 : TZrUInt8`; `*moduleName : struct SZrString`; `*assemblyName : struct SZrString`; `*symbolName : struct SZrString`; `lineInSourceStart : TZrUInt32`; `columnInSourceStart : TZrUInt32`; `lineInSourceEnd : TZrUInt32`; `columnInSourceEnd : TZrUInt32`; `targetMetadataToken : TZrMetadataToken`; `targetSignatureToken : TZrMetadataToken`; `targetSignatureHash : TZrUInt64`; `targetModuleSignatureHash : TZrUInt64`; `*requestedModuleVersion : struct SZrString`; `*minModuleVersionInclusive : struct SZrString`; `*maxModuleVersionExclusive : struct SZrString` |
| `SZrFunctionCallableSummary` / [function.h](../../zr_vm_core/include/zr_vm_core/function.h) | `*name : struct SZrString`; `callableChildIndex : TZrUInt32`; `effectCount : TZrUInt32`; `*effects : SZrFunctionModuleEffect` |
| `SZrFunctionTopLevelCallableBinding` / [function.h](../../zr_vm_core/include/zr_vm_core/function.h) | `*name : struct SZrString`; `stackSlot : TZrUInt32`; `callableChildIndex : TZrUInt32`; `accessModifier : TZrUInt8`; `exportKind : TZrUInt8`; `readiness : TZrUInt8`; `reserved0 : TZrUInt8` |
| `SZrFunctionMetadataParameter` / [function.h](../../zr_vm_core/include/zr_vm_core/function.h) | `*name : struct SZrString`; `type : SZrFunctionTypedTypeRef`; `hasDefaultValue : TZrBool`; `defaultValue : struct SZrTypeValue`; `hasDecoratorMetadata : TZrBool`; `decoratorMetadataValue : struct SZrTypeValue`; `<unparsed declaration> : None; unparsed=struct SZrString **decoratorNames`; `decoratorCount : TZrUInt32` |
| `SZrFunctionCompileTimePathBinding` / [function.h](../../zr_vm_core/include/zr_vm_core/function.h) | `*path : struct SZrString`; `targetKind : TZrUInt8`; `*targetName : struct SZrString` |
| `SZrFunctionCompileTimeVariableInfo` / [function.h](../../zr_vm_core/include/zr_vm_core/function.h) | `*name : struct SZrString`; `type : SZrFunctionTypedTypeRef`; `lineInSourceStart : TZrUInt32`; `lineInSourceEnd : TZrUInt32`; `*pathBindings : SZrFunctionCompileTimePathBinding`; `pathBindingCount : TZrUInt32` |
| `SZrFunctionCompileTimeFunctionInfo` / [function.h](../../zr_vm_core/include/zr_vm_core/function.h) | `*name : struct SZrString`; `returnType : SZrFunctionTypedTypeRef`; `parameterCount : TZrUInt32`; `*parameters : SZrFunctionMetadataParameter`; `lineInSourceStart : TZrUInt32`; `lineInSourceEnd : TZrUInt32` |
| `SZrFunctionEscapeBinding` / [function.h](../../zr_vm_core/include/zr_vm_core/function.h) | `*name : struct SZrString`; `slotOrIndex : TZrUInt32`; `scopeDepth : TZrUInt32`; `escapeFlags : TZrUInt32`; `bindingKind : TZrUInt8`; `reserved0 : TZrUInt8`; `reserved1 : TZrUInt16` |
| `SZrSemIrOwnershipEntry` / [function.h](../../zr_vm_core/include/zr_vm_core/function.h) | `state : TZrUInt32` |
| `SZrSemIrEffectEntry` / [function.h](../../zr_vm_core/include/zr_vm_core/function.h) | `kind : TZrUInt32`; `instructionIndex : TZrUInt32`; `ownershipInputIndex : TZrUInt32`; `ownershipOutputIndex : TZrUInt32` |
| `SZrSemIrBlockEntry` / [function.h](../../zr_vm_core/include/zr_vm_core/function.h) | `blockId : TZrUInt32`; `firstInstructionIndex : TZrUInt32`; `instructionCount : TZrUInt32` |
| `SZrSemIrInstruction` / [function.h](../../zr_vm_core/include/zr_vm_core/function.h) | `opcode : TZrUInt32`; `execInstructionIndex : TZrUInt32`; `typeTableIndex : TZrUInt32`; `effectTableIndex : TZrUInt32`; `destinationSlot : TZrUInt32`; `operand0 : TZrUInt32`; `operand1 : TZrUInt32`; `deoptId : TZrUInt32` |
| `SZrSemIrDeoptEntry` / [function.h](../../zr_vm_core/include/zr_vm_core/function.h) | `deoptId : TZrUInt32`; `execInstructionIndex : TZrUInt32` |
| `SZrFunctionCallSitePicSlot` / [function.h](../../zr_vm_core/include/zr_vm_core/function.h) | `*cachedReceiverPrototype : struct SZrObjectPrototype`; `*cachedOwnerPrototype : struct SZrObjectPrototype`; `*cachedReceiverObject : struct SZrObject`; `*cachedReceiverPair : struct SZrHashKeyValuePair`; `*cachedFunction : struct SZrFunction`; `*cachedMemberName : struct SZrString`; `cachedReceiverVersion : TZrUInt32`; `cachedOwnerVersion : TZrUInt32`; `cachedReceiverShapeId : TZrUInt64`; `cachedOwnerShapeId : TZrUInt64`; `cachedReceiverShapeGeneration : TZrUInt64`; `cachedOwnerShapeGeneration : TZrUInt64`; `cachedDescriptorIndex : TZrUInt32`; `cachedIsStatic : TZrUInt8`; `cachedAccessKind : TZrUInt8`; `cachedHotFieldKind : TZrUInt16` |
| `SZrFunctionCallSiteCacheEntry` / [function.h](../../zr_vm_core/include/zr_vm_core/function.h) | `kind : TZrUInt32`; `instructionIndex : TZrUInt32`; `memberEntryIndex : TZrUInt32`; `deoptId : TZrUInt32`; `argumentCount : TZrUInt32`; `picSlotCount : TZrUInt32`; `picNextInsertIndex : TZrUInt32`; `reserved2 : TZrUInt32`; `picSlots : SZrFunctionCallSitePicSlot[ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY]`; `runtimeHitCount : TZrUInt32`; `runtimeMissCount : TZrUInt32`; `binding : SZrCallBinding`; `bindingLocation : SZrCallBindingLocation` |
| `SZrFfiAggregateFieldContract` / [zr_ffi_contract.h](../../zr_vm_common/include/zr_vm_common/zr_ffi_contract.h) | `name : TZrChar[ZR_FFI_CONTRACT_FIELD_NAME_CAPACITY]`; `typeKind : EZrFfiTypeKind`; `size : TZrUInt32`; `alignment : TZrUInt32`; `offset : TZrUInt32` |
| `SZrFfiTypeContract` / [zr_ffi_contract.h](../../zr_vm_common/include/zr_vm_common/zr_ffi_contract.h) | `typeKind : EZrFfiTypeKind`; `size : TZrUInt32`; `alignment : TZrUInt32`; `canonicalTypeHash : TZrUInt64`; `layoutHash : TZrUInt64`; `flags : TZrUInt32`; `aggregateFieldStart : TZrUInt32`; `aggregateFieldCount : TZrUInt32` |
| `SZrFfiParameterContract` / [zr_ffi_contract.h](../../zr_vm_common/include/zr_vm_common/zr_ffi_contract.h) | `type : SZrFfiTypeContract`; `direction : EZrFfiDirection`; `marshalling : EZrFfiMarshallingKind`; `ownership : EZrFfiParameterOwnership`; `isNullable : TZrBool`; `flags : TZrUInt32` |
| `SZrFfiSignatureContract` / [zr_ffi_contract.h](../../zr_vm_common/include/zr_vm_common/zr_ffi_contract.h) | `abi : EZrFfiAbi`; `targetPointerSize : TZrUInt32`; `targetEndianness : EZrFfiTargetEndianness`; `targetTriple : TZrChar[ZR_FFI_CONTRACT_TARGET_TRIPLE_CAPACITY]`; `targetAbiHash : TZrUInt64`; `charset : EZrFfiCharset`; `errorPolicy : EZrFfiErrorPolicy`; `cleanupPolicy : EZrFfiCleanupPolicy`; `callbackLifetime : EZrFfiCallbackLifetime`; `callbackThreadPolicy : EZrFfiCallbackThreadPolicy`; `callbackExceptionPolicy : EZrFfiCallbackExceptionPolicy`; `isVariadic : TZrBool`; `parameterCount : TZrUInt32`; `returnType : SZrFfiTypeContract`; `parameters : SZrFfiParameterContract[ZR_FFI_CONTRACT_MAX_PARAMETERS]`; `aggregateFieldCount : TZrUInt32`; `aggregateFields : SZrFfiAggregateFieldContract[ZR_FFI_CONTRACT_MAX_AGGREGATE_FIELDS]`; `signatureHash : TZrUInt64` |
| `SZrFfiCallableParameterContract` / [zr_ffi_contract.h](../../zr_vm_common/include/zr_vm_common/zr_ffi_contract.h) | `canonicalTypeHash : TZrUInt64`; `passingForm : EZrFfiCallablePassingForm`; `escapeUpperBound : EZrFfiCallableEscapeUpperBound`; `entryInitialization : EZrFfiCallableEntryInitialization`; `exitInitialization : EZrFfiCallableExitInitialization`; `acceptsTemporary : TZrBool`; `callSiteMarker : EZrFfiCallableCallSiteMarker` |
| `SZrFfiCallableContract` / [zr_ffi_contract.h](../../zr_vm_common/include/zr_vm_common/zr_ffi_contract.h) | `parameterCount : TZrUInt32`; `parameters : SZrFfiCallableParameterContract[ZR_FFI_CONTRACT_MAX_PARAMETERS]`; `returnTypeHash : TZrUInt64`; `receiverEffect : EZrFfiCallableReceiverEffect`; `effectFlags : TZrUInt32`; `isVariadic : TZrBool`; `contractHash : TZrUInt64` |
| `SZrFfiSourceMapping` / [zr_ffi_contract.h](../../zr_vm_common/include/zr_vm_common/zr_ffi_contract.h) | `document : TZrChar[ZR_FFI_CONTRACT_SOURCE_DOCUMENT_CAPACITY]`; `startOffset : TZrUInt64`; `endOffset : TZrUInt64`; `startLine : TZrInt32`; `startColumn : TZrInt32`; `endLine : TZrInt32`; `endColumn : TZrInt32` |
| `SZrNativeImportContract` / [zr_ffi_contract.h](../../zr_vm_common/include/zr_vm_common/zr_ffi_contract.h) | `schemaVersion : TZrUInt32`; `libraryLocator : TZrChar[ZR_FFI_CONTRACT_LIBRARY_CAPACITY]`; `entryPoint : TZrChar[ZR_FFI_CONTRACT_ENTRY_CAPACITY]`; `symbolId : TZrUInt64`; `declaringModuleId : TZrUInt64`; `callable : SZrFfiCallableContract`; `availability : TZrUInt32`; `requiredCapabilities : TZrUInt64`; `sourceMapping : SZrFfiSourceMapping`; `signature : SZrFfiSignatureContract` |

## Opaque 与仍需下游完成的格式

CODE_TABLE/DEBUG_MAP/SYNTAX_TREE/SEMANTIC_IR/原始heap等 raw_payload 项冻结为 bounded byte-window 合同，writer/reader只证明已列外层范围、kind/flags/count/length规则。已经有 L 语法的旧IO载荷按 L 解码；不能把同名不同格式自动视为相同。没有可识别内层grammar的 opaque 格式仍归其明确 owner 的 planned 版本接口，08.01/07.01/相应 Parser leaf 实现后须补 codec 和准入，不得用 raw byte count 伪造完整内层字段验收。

本页完成已审计清单的 Git 可读 owner/坐标/producer/reader/hash 覆盖；未执行 ABI/编译/codec/loader 验证，不单独勾选完整00.02或47叶。

## Sealed 证据坐标

报告根为 `E:\cargo-targets\zr_vm\reports\ssa-contract-freeze-01a0fe2b`。Git 内保留规则和完整族/record 索引；精确分支、调用参数与源 pin 由下列不可变 JSON 复核。

| 文件 | 字节 | SHA256 |
| --- | ---: | --- |
| `persistent-field-audit.json` | 720108 | `FCD3A6A7A0AC2F7A57D34108AEC4A57E2BC50E68D5C32BCE3873557C98ED0784` |
| `audit-coordinate-corrections.json` | 41112 | `1EE1DF88A21024DE2098C3858BA25F814D1BEAF7FA8C12724420B43BBFD7D1D2` |
| `legacy-io-field-audit.json` | 1115640 | `D50CDBB8AC5E02EA426EBC090B1A8A72D379898A0BB7BE44BB829F293CEFCA8E` |
| `dictionary-delta-review.json` | 15688 | `5B6BDCF47B9805623AE459538F092E66441DA343C072530E031B1FC53EE6DD3D` |
| `contract-inventory-delta.json` | 119852 | `FC771882967BABE650C97412E24691A2AF13EF1536AD0ABAAA048CA51A61FA61` |
| `contract-inventory-delta-review.json` | 18076 | `BD7404366C0A8A4ADF6D67C4609ACD26112C733C59905CD58D2128A0453FA16E` |
| `interface-family-audit.json` | 104659 | `1F7F35D4C229F4F41BD7068B5441689FB535136ED3E0E55C3B37478B240F26E6` |
| `backend-production-review.json` | 24837 | `F3546F3D070B8FC17B4C11E56E10D1F42B827F0960A454A06F55C4E76CCA1679` |
| `backend-green-review.json` | 22707 | `221A4D88BF309C2F11566A992B054E98B849D5F7229D651AC5E6ABDBE93E483D` |
| `msvc-syntax-review.json` | 15138 | `394EB90F40205BDE63D56C9AC1061B71317B7E6D36D12B65A5EEE7D2E8D0C00D` |
