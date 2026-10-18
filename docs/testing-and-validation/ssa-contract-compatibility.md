---
related_code:
  - lua/jdk/src/hotspot/share/classfile/classLoaderData.hpp
  - lua/mono/mono/metadata/image-internals.h
  - lua/mono/mono/metadata/image.h
  - lua/rust/compiler/rustc_data_structures/src/stable_hasher.rs
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
  - tests/acceptance/2026-06-05-aot-public-abi-header.md
  - tests/acceptance/2026-06-21-aot-m1-5-fail-macro-function-index-local.md
  - tests/acceptance/2026-06-21-aot-m1-5-method-info-skeleton.md
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
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
  - tests/tooling/test_layout_moves.py
doc_type: testing-guide
---


# SSA 契约兼容与拒绝政策

## 导航与冻结范围

[运行期 owner](../core-runtime/execution-contract-ownership.md) · [持久字段](../module-system/persistent-contract-fields.md) · [接口矩阵](../testing-and-validation/ssa-interface-contract-matrix.md) · [兼容政策](../testing-and-validation/ssa-contract-compatibility.md)。四页共同构成 00.02 的文字字典；[47 叶依赖图](../plans/ssa/index.md)与各叶运行验收保持独立。文档冻结不授予实现、加载、安全测试或运行权限；planned API 不据此成为已实现。


## 当前版本域与边界矩阵

本页政策不改变现有接受集合；现有reader没有检查的字节仍记“当前忽略”，不能凭文档变成拒绝。若下游要收紧，必须作为明确源码/版本变更、独立RED/GREEN与兼容验收提交。

| 域 | 当前owner / 准入 | 兼容与错误政策 |
| --- | --- | --- |
| execution schema/ABI/logical | Core declaration当前6/17/1；generic checker纯等值 | checker允许双方相等任意logical，如11；NULL→schema/ABI→logical→target→generation→signature→layout→module→capability→effects，首错返回 |
| backend support | Core backend owner gate | 当前支持schema6/logical1，descriptor/target/request/code reserved0；ABI匹配selectedregistration，合法23不能因canonical17被拒绝 |
| CallBinding static | Core Check/Compare与fixed LE codec，schema1 | token形状/field组合、reserved独立验证；Compare10 meaningful fields，不凭hash同值证明实体存在；runtime target/generation另验 |
| ZRAF outer | Core artifact，schema6、kind/section规则 | schema不匹配拒绝；mandatory/optional按实际reader。公开identity/row若当前reader忽略reserved，保留既有ignore，不能宣称普遍strictzero |
| ERI envelope | Core ArtifactExecIr，schema1 | magic/schema/dir/range/hash验证；header reserved检查与flags按实现；view成功不等于完整IR验证 |
| EIS1–6 profiles | Core scalar codec，各profile有限grammar | profile版本/长度/pool/contract/state限定按实际分支；版本相同不表示任意IR可编码；generation1是有限profile约束 |
| legacy native IO | Core reader、Parser writer | writer patch45；reader16–45且major/minor/native sizes/endian/signature/debug/optional匹配；不是跨hostportable格式 |
| source/cache/profile | Parser profile/cache owner | source/dependency/profile/compilerABI/pipeline/target/layout/policy等全key匹配才复用；无现有diskcodec的Crecord保持logical，不猜wire |
| domain transfer | Core ownership/host policy | 合法形状不证明providerToken↔providerHash实体对应；生产者/宿主在06.04补解析权威，不能靠字符串或u64相等放行 |
| hotpatch | Core validator+host authority | base/publicABI/profile/capability/signature/content各自拒绝；machinecode/nativeimport禁止，不自动降级 |
| runtime handles | Core loader/code/root owners | 不落盘、不跨process、不从diskgeneration恢复；D4 epoch政策与leases并行约束 |

### 当前 reserved 的精确含义

ZRAF writer 会零出 PublicIdentity 和若干 row reserved，但一些 reader不解码/不检查它们；header偏移28和tail104也不由当前 Read enforce。当前政策选择 **保留该schema reader的ignore行为**，writer继续zero，不能把未检字段当语义/授权/identity输入，也不能在同schema赋予新语义。需要语义使用或strict rejection时升级相应format/schema或提交明确兼容变更；保留失败/旧文件证据。

ERI reserved、CallBinding Check reserved、EIS profile predicate与backendowner strictzero各有独立规则；禁止从ZRAFignore推导backendignore，也禁止把backendstrictzero强加genericchecker。所有record-specific writer/reader/admission在字段页F索引可查。

### 错误/重编译/回退

静态invalidIR、link失败、capability拒绝必须保留本类诊断；schema/ABI不匹配可要求重新编译，不把损坏数据当可执行fallback。合法unsupported只在caller显式允许的backend策略中回退；REQUIRE_MACHINE_CODE禁止ExecBC/AOTfallback。PENDING是正常排队/异步结果；取消、stalegeneration/ticket、OOM与compilefailed不被“pending”掩盖。

speculation失配只能在有合法state-map与runtime provider时恢复基线；不得对任意invalidIR或loader拒绝调用deopt。当前Resume只是callbackdispatch，真实provider planned；metadata测试不提供运行时恢复信用。

## D5：historical ABI2 隔离政策（明确冻结）

canonical唯一生产owner是顶层 common 的 zr_aot_abi.h（17）；nested zr_vm_aot/...同名头（2）保留为历史参考，归该参考树维护，不同步、不删除、不参与ambient生产include搜索。两者都已知存在，禁止写“全仓宏唯一定义”。

所有当前安装/生产/生成C消费者必须解析顶层17；source-text oracle引用历史文件仅作为文字输入，不能把其include目录透传当前target。显式reference-only编译若未来需要，必须使用单独命名target、显式历史source/include闭包，禁止与当前Core/Parser17对象混链；这不是本轮已执行的平台验收。

Root CMake不add_subdirectory历史根；CommonMacros给当前模块/common根；Parser backend目录已在当前Parser树。历史tests/scripts的CMake虽写rootcommon include，但根测试未进入且含陈旧入口TODO，不能把它算活target。nested aot_runtime.h同样拼写common include，未来单独编译的owner必须明确选择历史2并隔离，不能依赖shell环境顺序。

下面逐项保留base inventory的全部68个已知文字路线；kind仅表示include/宏/生成字符串/源码oracle事实。文件当前行内容在本次输入pin内复核。另列新增可见route，不丢历史表项。仓外/手工build无法由仓内盘点证实，明确不在这份闭包；其发布者必须提供完整argv/header resolver并按同政策验收，不能以未知使用为理由改变当前canonical owner。

| 已知位置 | 路线分类 | 冻结解析/隔离责任 |
| --- | --- | --- |
| `zr_vm_common/include/zr_vm_common/zr_aot_abi.h:18` | macro_definition | canonical17定义；Core/common ABI唯一生产owner |
| `zr_vm_core/src/zr_vm_core/artifact/zrp_metadata.c:4` | include_or_route_reference | 当前生产/生成C入口；owner构建路线只准canonical17 |
| `zr_vm_core/src/zr_vm_core/closure/closure_close_meta_guard.c:3` | include_or_route_reference | 当前生产/生成C入口；owner构建路线只准canonical17 |
| `zr_vm_core/src/zr_vm_core/exception/exception_roots_internal.h:8` | include_or_route_reference | 当前生产/生成C入口；owner构建路线只准canonical17 |
| `zr_vm_core/src/zr_vm_core/execution/execution_control.c:10` | include_or_route_reference | 当前生产/生成C入口；owner构建路线只准canonical17 |
| `zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:8` | include_or_route_reference | 当前生产/生成C入口；owner构建路线只准canonical17 |
| `zr_vm_core/src/zr_vm_core/gc/gc.c:9` | include_or_route_reference | 当前生产/生成C入口；owner构建路线只准canonical17 |
| `zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:12` | include_or_route_reference | 当前生产/生成C入口；owner构建路线只准canonical17 |
| `zr_vm_core/src/zr_vm_core/gc/gc_mark.c:9` | include_or_route_reference | 当前生产/生成C入口；owner构建路线只准canonical17 |
| `zr_vm_core/src/zr_vm_core/module/module_exec_ir_artifact.c:3` | include_or_route_reference | 当前生产/生成C入口；owner构建路线只准canonical17 |
| `zr_vm_core/src/zr_vm_core/ownership/ownership_resource.c:3` | include_or_route_reference | 当前生产/生成C入口；owner构建路线只准canonical17 |
| `zr_vm_core/include/zr_vm_core/call_binding.h:6` | include_or_route_reference | 当前生产/生成C入口；owner构建路线只准canonical17 |
| `zr_vm_core/include/zr_vm_core/execution_control.h:6` | include_or_route_reference | 当前生产/生成C入口；owner构建路线只准canonical17 |
| `zr_vm_core/include/zr_vm_core/metadata_runtime.h:4` | include_or_route_reference | 当前生产/生成C入口；owner构建路线只准canonical17 |
| `zr_vm_core/include/zr_vm_core/reflection.h:8` | include_or_route_reference | 当前生产/生成C入口；owner构建路线只准canonical17 |
| `zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c:27` | include_or_route_reference | 当前生产/生成C入口；owner构建路线只准canonical17 |
| `zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c:1387` | include_or_route_reference | 当前生产/生成C入口；owner构建路线只准canonical17 |
| `zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_method_metadata.c:8` | include_or_route_reference | 当前生产/生成C入口；owner构建路线只准canonical17 |
| `zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_manifest_export.c:8` | include_or_route_reference | 当前生产/生成C入口；owner构建路线只准canonical17 |
| `zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_prune.h:5` | include_or_route_reference | 当前生产/生成C入口；owner构建路线只准canonical17 |
| `zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_module_artifacts.c:8` | include_or_route_reference | 当前生产/生成C入口；owner构建路线只准canonical17 |
| `zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:4` | include_or_route_reference | 当前生产/生成C入口；owner构建路线只准canonical17 |
| `zr_vm_parser/include/zr_vm_parser/writer.h:10` | include_or_route_reference | 当前生产/生成C入口；owner构建路线只准canonical17 |
| `zr_vm_aot/README.md:10` | include_or_route_reference | 历史树reference消费者；未来显式隔离target，不能与17混链 |
| `zr_vm_aot/zr_vm_library/include/zr_vm_library/aot_runtime.h:4` | include_or_route_reference | 历史树reference消费者；未来显式隔离target，不能与17混链 |
| `zr_vm_aot/zr_vm_common/include/zr_vm_common/zr_aot_abi.h:13` | macro_definition | 历史2定义；reference-only，禁止ambient生产include |
| `zr_vm_aot/tests/scripts/test_utils.c:10` | include_or_route_reference | 历史树reference消费者；未来显式隔离target，不能与17混链 |
| `zr_vm_aot/tests/fixtures/projects/aot_module_graph_pipeline/bin/aot_c/src/graph_binary_stage.c:7` | include_or_route_reference | 历史树reference消费者；未来显式隔离target，不能与17混链 |
| `zr_vm_aot/tests/fixtures/projects/aot_module_graph_pipeline/bin/aot_c/src/graph_stage_a.c:7` | include_or_route_reference | 历史树reference消费者；未来显式隔离target，不能与17混链 |
| `zr_vm_aot/tests/fixtures/projects/aot_module_graph_pipeline/bin/aot_c/src/graph_stage_b.c:7` | include_or_route_reference | 历史树reference消费者；未来显式隔离target，不能与17混链 |
| `zr_vm_aot/tests/fixtures/projects/aot_module_graph_pipeline/bin/aot_c/src/main.c:7` | include_or_route_reference | 历史树reference消费者；未来显式隔离target，不能与17混链 |
| `zr_vm_aot/tests/fixtures/projects/aot_eh_tail_gc_stress/bin/aot_c/src/main.c:177` | include_or_route_reference | 历史树reference消费者；未来显式隔离target，不能与17混链 |
| `zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/aot_c/src/main.c:185` | include_or_route_reference | 历史树reference消费者；未来显式隔离target，不能与17混链 |
| `tests/acceptance/2026-06-05-aot-public-abi-header.md:15` | include_or_route_reference | 当前测试/生成C/文本oracle；编译canonical17，历史路径只能作source-text输入 |
| `tests/acceptance/2026-06-21-aot-m1-5-fail-macro-function-index-local.md:44` | include_or_route_reference | 当前测试/生成C/文本oracle；编译canonical17，历史路径只能作source-text输入 |
| `tests/acceptance/2026-06-21-aot-m1-5-method-info-skeleton.md:47` | include_or_route_reference | 当前测试/生成C/文本oracle；编译canonical17，历史路径只能作source-text输入 |
| `tests/core/ssa_runtime_objects_concurrency.c:7` | include_or_route_reference | 当前测试/生成C/文本oracle；编译canonical17，历史路径只能作source-text输入 |
| `tests/core/test_aot_gc_root_frame.c:11` | include_or_route_reference | 当前测试/生成C/文本oracle；编译canonical17，历史路径只能作source-text输入 |
| `tests/core/test_close_proxy.c:6` | include_or_route_reference | 当前测试/生成C/文本oracle；编译canonical17，历史路径只能作source-text输入 |
| `tests/core/test_gc_domain_multimutator.c:8` | include_or_route_reference | 当前测试/生成C/文本oracle；编译canonical17，历史路径只能作source-text输入 |
| `tests/core/test_session_checkpoint.c:15` | include_or_route_reference | 当前测试/生成C/文本oracle；编译canonical17，历史路径只能作source-text输入 |
| `tests/core/test_ssa_runtime_objects.c:7` | include_or_route_reference | 当前测试/生成C/文本oracle；编译canonical17，历史路径只能作source-text输入 |
| `tests/ffi/test_native_extern_contract.c:32` | include_or_route_reference | 当前测试/生成C/文本oracle；编译canonical17，历史路径只能作source-text输入 |
| `tests/library/test_ssa_schema_relocation.c:2` | include_or_route_reference | 当前测试/生成C/文本oracle；编译canonical17，历史路径只能作source-text输入 |
| `tests/module/test_metadata_runtime_manifest_exports.c:8` | include_or_route_reference | 当前测试/生成C/文本oracle；编译canonical17，历史路径只能作source-text输入 |
| `tests/module/test_zrp_metadata_format.c:11` | include_or_route_reference | 当前测试/生成C/文本oracle；编译canonical17，历史路径只能作source-text输入 |
| `tests/parser/test_aot_c_code_stripping.c:9` | include_or_route_reference | 当前测试/生成C/文本oracle；编译canonical17，历史路径只能作source-text输入 |
| `tests/parser/test_aot_c_descriptor_diagnostics.c:112` | include_or_route_reference | 当前测试/生成C/文本oracle；编译canonical17，历史路径只能作source-text输入 |
| `tests/parser/test_aot_c_descriptor_diagnostics.c:153` | include_or_route_reference | 当前测试/生成C/文本oracle；编译canonical17，历史路径只能作source-text输入 |
| `tests/parser/test_aot_c_descriptor_diagnostics.c:203` | include_or_route_reference | 当前测试/生成C/文本oracle；编译canonical17，历史路径只能作source-text输入 |
| `tests/parser/test_aot_c_generic_bool_equality_local_smoke.c:15` | include_or_route_reference | 当前测试/生成C/文本oracle；编译canonical17，历史路径只能作source-text输入 |
| `tests/parser/test_aot_c_generic_call_result_stack_copy_equality_local_smoke.c:11` | include_or_route_reference | 当前测试/生成C/文本oracle；编译canonical17，历史路径只能作source-text输入 |
| `tests/parser/test_aot_c_generic_equality_stack_copy_local_smoke.c:11` | include_or_route_reference | 当前测试/生成C/文本oracle；编译canonical17，历史路径只能作source-text输入 |
| `tests/parser/test_aot_c_generic_jump_if_bool_local_smoke.c:15` | include_or_route_reference | 当前测试/生成C/文本oracle；编译canonical17，历史路径只能作source-text输入 |
| `tests/parser/test_aot_c_generic_logical_not_numeric_local_smoke.c:15` | include_or_route_reference | 当前测试/生成C/文本oracle；编译canonical17，历史路径只能作source-text输入 |
| `tests/parser/test_aot_c_generic_not_equal_stack_copy_jump_if_local_smoke.c:11` | include_or_route_reference | 当前测试/生成C/文本oracle；编译canonical17，历史路径只能作source-text输入 |
| `tests/parser/test_aot_c_golden_scalar_smoke.c:10` | include_or_route_reference | 当前测试/生成C/文本oracle；编译canonical17，历史路径只能作source-text输入 |
| `tests/parser/test_aot_c_load_const_scalar.c:11` | include_or_route_reference | 当前测试/生成C/文本oracle；编译canonical17，历史路径只能作source-text输入 |
| `tests/parser/test_aot_c_logical_shared_library_smoke.c:15` | include_or_route_reference | 当前测试/生成C/文本oracle；编译canonical17，历史路径只能作source-text输入 |
| `tests/parser/test_aot_c_method_info_signature.c:9` | include_or_route_reference | 当前测试/生成C/文本oracle；编译canonical17，历史路径只能作source-text输入 |
| `tests/parser/test_aot_c_shared_library_smoke.c:15` | include_or_route_reference | 当前测试/生成C/文本oracle；编译canonical17，历史路径只能作source-text输入 |
| `tests/parser/test_aot_c_source_contracts.c:3073` | include_or_route_reference | 当前测试/生成C/文本oracle；编译canonical17，历史路径只能作source-text输入 |
| `tests/parser/test_aot_c_source_contracts.c:3164` | include_or_route_reference | 当前测试/生成C/文本oracle；编译canonical17，历史路径只能作source-text输入 |
| `tests/parser/test_aot_c_typed_scalar.c:11` | include_or_route_reference | 当前测试/生成C/文本oracle；编译canonical17，历史路径只能作source-text输入 |
| `tests/parser/test_aot_c_value_type_shared_library_smoke.c:16` | include_or_route_reference | 当前测试/生成C/文本oracle；编译canonical17，历史路径只能作source-text输入 |
| `tests/parser/test_aot_c_zrp_metadata_methodspec_pruning.c:7` | include_or_route_reference | 当前测试/生成C/文本oracle；编译canonical17，历史路径只能作source-text输入 |
| `tests/parser/test_aot_c_zrp_metadata_pruning.c:7` | include_or_route_reference | 当前测试/生成C/文本oracle；编译canonical17，历史路径只能作source-text输入 |
| `tests/parser/test_ssa_contract_freeze.c:4` | include_or_route_reference | 当前测试/生成C/文本oracle；编译canonical17，历史路径只能作source-text输入 |

### 本次新增实际include-site差集

本表仅增加已找到的direct include sites，不机械增加H/C语义分母；source pin在本任务implementation report。

| 当前位置 | owner政策 |
| --- | --- |
| `zr_vm_library/include/zr_vm_library/aot_runtime.h:4` | 当前production/test target必须canonical17 |
| `zr_vm_library/include/zr_vm_library/project.h:8` | 当前production/test target必须canonical17 |

## Hash 域：31项有限 union

H identity以source-file+symbol去重；原28保留，InternFunction独立FUNCTION根+两个CallBinding signature helpers新增3；原parameter helper已在28，不重计。下面列全部31项，ordered input与algorithm是兼容域的一部分。输入hash相等不证明authoritative producer、实体解析或安全授权；XXH3/FNV-like都不是signature。

| H | 唯一hash owner / symbol | algorithm | 输入与排除/限制 |
| --- | --- | --- | --- |
| H01 | `zr_vm_core/src/zr_vm_core/artifact/artifact_identity.c::ZrCore_Artifact_HashBytes` | 64-bit xor-byte then multiply1099511628211 modulo2^64; seed1469598103934665603 | exact byte span；Not XXH3; no cryptographic guarantee |
| H02 | `zr_vm_core/src/zr_vm_core/artifact/artifact_identity.c::ZrCore_Artifact_ComputeMetadataStateHash` | 64-bit xor-byte then multiply1099511628211 modulo2^64; seed1469598103934665603 | u32 typeToken,preservationState,category,metadataGeneration,retainedMemberCount,retainedPropertyCount,retainedMetaRecordCount,flags then u64 typeSignatureHash,layoutHash,callableContractHash, all LE；excludes metadataHash |
| H03 | `zr_vm_core/src/zr_vm_core/artifact/artifact_identity.c::ZrCore_Artifact_ComputeMetadataRecordHash` | 64-bit xor-byte then multiply1099511628211 modulo2^64; seed1469598103934665603 | u32 ownerToken,kind,retention,flags,payloadOffset,payloadLength,metadataGeneration,reserved0 then exact payload bytes；excludes recordHash; payloadLength must match |
| H04 | `zr_vm_core/src/zr_vm_core/artifact/artifact_exec_ir.c::ZrCore_ArtifactExecIr_HashBytes` | 64-bit xor-byte then multiply1099511628211 modulo2^64; seed1469598103934665603 | exact bounded section/container byte span chosen by caller；ExecIR and ExecBC hashes can be zero and then are not recomputed; no full-container identity implied |
| H05 | `zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_metadata_signature.c::metadata_signature_hash_v1` | XXH3_64 prefix streaming | zr.md.sig.v1 plus NUL then exact nonempty signature blob；zero conflates allocation/update failure with hash value |
| H06 | `zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_metadata_module_hash.c::metadata_token_compute_module_signature_hash` | XXH3_64 prefixed canonical buffer | zr.md.mod.v2 plus NUL; module ABI buffer via sorted export indices and sorted TYPE_DEF/TYPE_SPEC identities；helper sort/write functions are authoritative; do not substitute byte sorting or include arbitrary record fields |
| H07 | `zr_vm_core/src/zr_vm_core/call/call_binding_signature.c::ZrCore_CallBinding_FunctionSignatureHash` | XXH3_64 domain-separated recursive mixing | typed function flag/count/return+parameter type hashes/roleFlags/index per actual fixed LE buffers；not metadata_signature_hash_v1; zero propagation TODO exists |
| H08 | `zr_vm_core/src/zr_vm_core/call/call_binding_layout.c::ZrCore_CallBinding_PrototypeLayoutHash` | XXH3_64 exact prototype slice | compiled prototype header plus inheritance/decorator/member rows using sizeof native compiled structs；host-layout dependency unresolved for cross-host persistent stability |
| H09 | `zr_vm_core/src/zr_vm_core/type/type_layout.c::ZrCore_TypeLayout_ComputeHash` | 64-bit xor-byte then multiply1099511628211 modulo2^64; seed1469598103934665603 | schema,width/alignment/kind/copy/drop/GC,counts/tag/blittable/transfer fields,ordered field descriptors and GC/ownership/ref maps; LEu32/u64；layoutHash excluded; domainTransferSchemaHash participates; not prototype hash |
| H10 | `zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c::ZrCore_ExecIr_FunctionBindingRowsHash` | FNV-1a64 with file-defined seed/prime | schema,count then ordered row/contract/location/source fields LEu32/u64；legacy returns0; invalid returns0; valid computed0 mapped1 |
| H11 | `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_profile.c::zr_profile_function_hash` | 64-bit xor-byte then multiply1099511628211 modulo2^64; seed1469598103934665603 | actual function graph fields in implementation, typed binding hash when present；profile key identity; not canonical metadata signature |
| H12 | `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_profile.c::ZrParser_ExecIr_ProfileModuleHash` | 64-bit xor-byte then multiply1099511628211 modulo2^64; seed1469598103934665603 | module identity/contract fields and function scalar summaries in actual order；moduleHash name collision must remain distinguished |
| H13 | `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_profile.c::zr_profile_module_ir_hash` | 64-bit xor-byte then multiply1099511628211 modulo2^64; seed1469598103934665603 | function hash accumulation；not wire-container hash |
| H14 | `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_profile.c::ZrParser_ExecIr_ProfileHash` | 64-bit xor-byte then multiply1099511628211 modulo2^64; seed1469598103934665603 | magic/schema,key,site count,ordered site observations and specialization policy；logical in-memory profile; no disk encoding found |
| H15 | `zr_vm_parser/src/zr_vm_parser/compiler/compile_optimization_profile.c::ZrParser_CompileOptimizationProfile_Hash` | file-defined FNV-like LEu32/u64 | schema,buildMode,numericPermission,target,backend,frameBudgetBytes,passMask,targetHash,hostAbiHash,frameSafe,fastMath,lto,pgo；excludes origin/preset label and existing profileHash |
| H16 | `zr_vm_parser/src/zr_vm_parser/compiler/compile_ir_cache.c::ZrParser_CompileIrCache_BuildKey` | SHA256 | domain zr.compile.ir-cache/v1 without NUL; u64LE length +source, length+dependencies; profileHash,contractHash,compilerAbi,pipeline,target,numericPolicy,layout,importedProfile as u64LE；runtime key no wire codec; valid flag outside digest |
| H17 | `zr_vm_parser/src/zr_vm_parser/compiler/compile_ir_cache.c::cache_payload_digest` | SHA256 | exact cached payload span；integrity at Publish/Lookup; no disk persistence established |
| H18 | `zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_validate.c::ZrCore_HotPatch_ComputePolicyHash` | 64-bit xor-byte then multiply1099511628211 modulo2^64; seed1469598103934665603 | manifest schema,flags,targetABI,targetProfile,hostABI,hostProfile,hostAllowedCapabilities,loadedBaseModuleHash,loadedPublicContractHash all castu64LE；host policy identity not authority; requirements not included in this helper |
| H19 | `zr_vm_core/src/zr_vm_core/aot/aot_ir.c::ZrCore_AotIr_HashModule` | file-local FNV-like typed encoding | ValidateModule before hashing; scalar target/module/contracts/functions/state maps in actual helper order；different AOTIR domain, not AOT17 registration layout |
| H20 | `zr_vm_parser/src/zr_vm_parser/types/canonical_type.c::canonical_primitive_hash` | canonical_type_hash_word/string file-defined mixing | kind-specific canonical constructor inputs; ordered words/string helper calls；May include context-local TypeId; stable metadata token mapping must be explicit before persistence |
| H21 | `zr_vm_parser/src/zr_vm_parser/types/canonical_type.c::canonical_nominal_hash` | canonical_type_hash_word/string file-defined mixing | kind-specific canonical constructor inputs; ordered words/string helper calls；May include context-local TypeId; stable metadata token mapping must be explicit before persistence |
| H22 | `zr_vm_parser/src/zr_vm_parser/types/canonical_type.c::canonical_generic_instance_hash` | canonical_type_hash_word/string file-defined mixing | kind-specific canonical constructor inputs; ordered words/string helper calls；May include context-local TypeId; stable metadata token mapping must be explicit before persistence |
| H23 | `zr_vm_parser/src/zr_vm_parser/types/canonical_type.c::canonical_generic_parameter_hash` | canonical_type_hash_word/string file-defined mixing | kind-specific canonical constructor inputs; ordered words/string helper calls；May include context-local TypeId; stable metadata token mapping must be explicit before persistence |
| H24 | `zr_vm_parser/src/zr_vm_parser/types/canonical_type.c::canonical_array_hash` | canonical_type_hash_word/string file-defined mixing | kind-specific canonical constructor inputs; ordered words/string helper calls；May include context-local TypeId; stable metadata token mapping must be explicit before persistence |
| H25 | `zr_vm_parser/src/zr_vm_parser/types/canonical_type.c::canonical_type_list_hash` | canonical_type_hash_word/string file-defined mixing | kind-specific canonical constructor inputs; ordered words/string helper calls；May include context-local TypeId; stable metadata token mapping must be explicit before persistence |
| H26 | `zr_vm_parser/src/zr_vm_parser/types/canonical_type.c::canonical_qualified_target_hash` | canonical_type_hash_word/string file-defined mixing | kind-specific canonical constructor inputs; ordered words/string helper calls；May include context-local TypeId; stable metadata token mapping must be explicit before persistence |
| H27 | `zr_vm_parser/src/zr_vm_parser/types/canonical_type.c::canonical_parameter_contract_hash` | canonical_type_hash_word/string file-defined mixing | kind-specific canonical constructor inputs; ordered words/string helper calls；May include context-local TypeId; stable metadata token mapping must be explicit before persistence |
| H28 | `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::artifact_projection_domain_hash` | 64-bit signatureHash XOR domain; multiply1099511628211; XOR token; multiply1099511628211 | type-ref/type-spec domain plus signatureHash and token；Not bytewise FNV; not XXH3; callers BuildPublicIdentity. |
| H29 | `zr_vm_parser/src/zr_vm_parser/types/canonical_type.c::ZrParser_CanonicalType_InternFunction` | FNV-like xor/multiply modulo 2^64, seed1469598103934665603 (not standard FNV offset), prime1099511628211; each word processed least significant byte first for 8 bytes | FUNCTION kind; parameterCount; for each parameter: typeId,passingForm,escapeUpperBound,entryInitialization,exitInitialization,acceptsTemporary,callSiteMarker; returnTypeId; receiverEffect; effectFlags; 40+56*N bytes; context-local TypeIds |
| H30 | `zr_vm_core/src/zr_vm_core/call/call_binding_signature.c::signature_string_hash` | unsalted XXH3_64bits | null->0; native string bytes/byteLength; no pointer hash |
| H31 | `zr_vm_core/src/zr_vm_core/call/call_binding_signature.c::signature_type_hash` | unsalted XXH3_64bits | {"bytes":48,"little_endian_lanes":{"0":"baseType u32","4":"nullable boolean u32","8":"ownershipQualifier u32","12":"array boolean u32","16":"typeName stable hash u64","24":"elementBaseType u32","28":"staticCType u32","32":"staticCTypeId only STRUCT else NONE u32","36":"zero padding u32","40":"elementTypeName stable hash u64"},"algorithm":"unsalted XXH3_64bits of all 48 bytes"} |

### Hash provenance 与复用决定

FUNCTION root 在同context内先比kind/return/receiver/effects/完整parameter contracts再复用；其40+56*N有序u64LE输入含context-local TypeId。**选择：跨context/session不能仅以此hash放行persistent cache或ABI兼容。** Parser/compiler owner必须通过公开type identity重映射并重算，或将复用限制在同一生产context/已验证compiler artifact身份内；无法证明则拒绝复用、重新编译。归11.01缓存与01.01 canonical/08.01artifactconsumer，不虚构已实现跨context算法。

FUNCTION hash可由source/module/callable/frame桥进入ExecIR contract，不能写“context-local值永不wire”。CallBinding的ZRCALL+version1、32B root/fold及48B type helper为另一域，不能替换canonical FUNCTIONhash。PrototypeLayoutHash覆盖native compiled struct bytes，不能宣称跨hoststable；Rust HashStable先例只支持本政策，不授实现信用。

CreateStable64WithPrefix分配状态失败返回0，现有CallBinding root可把中间0再fold成非零；非零也不是“整个hash过程成功”的证明。**选择：权威producer应独立报告hash construction失败；未提供该通道的现有consumer不得以非零补出成功证明。** 修正需另有correctness任务与OOM验收，本次不改源码/错误合同、不隐去限制。外部supplied hash按其producer pin/域身份使用，decoder只搬运不自动升格authoritative hash。

## C315 current-view反向索引

C计数以file+symbol有限union去重，role合并不加数。旧source main里迁出的symbol由下面current路径替代，40函数move-map不算新consumer。当前后续gate/诊断修正不从旧pin继承PASS。此索引是字典选定producer/reader/hash/admission/runtime consumer集合，不是全项目函数数；仅声明/API名不够加入consumer。

| Current file::symbol | 已审计roles |
| --- | --- |
| `zr_vm_core/src/zr_vm_core/aot/aot_ir.c::ZrCore_AotIr_HashModule` | hash_producer |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_call_binding.c::ZrCore_Artifact_ReadCallBindingRow` | implementation_inventory, record_reader |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_call_binding.c::ZrCore_Artifact_WriteCallBindingRow` | implementation_inventory, record_writer |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_call_binding.c::artifact_binding_validate_row` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_call_binding.c::zr_artifact_call_binding_validate_decoded` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_call_binding.c::zr_artifact_call_binding_validate_input` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_encoding.c::zr_artifact_read_u16` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_encoding.c::zr_artifact_read_u32` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_encoding.c::zr_artifact_read_u64` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_encoding.c::zr_artifact_write_u16` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_encoding.c::zr_artifact_write_u32` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_encoding.c::zr_artifact_write_u64` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_exec_ir.c::ZrCore_ArtifactExecIr_HashBytes` | hash_producer |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_exec_ir.c::ZrCore_ArtifactExecIr_Read` | reader_admission |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_exec_ir.c::ZrCore_ArtifactExecIr_ValidateRelocations` | relocation_admission |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_exec_ir.c::ZrCore_ArtifactExecIr_Write` | writer |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_identity.c::ZrCore_Artifact_ComputeMetadataRecordHash` | hash_producer |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_identity.c::ZrCore_Artifact_ComputeMetadataStateHash` | hash_producer |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_identity.c::ZrCore_Artifact_HashBytes` | hash_producer |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::ZrCore_Artifact_ReadContractRow` | implementation_inventory, record_reader |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::ZrCore_Artifact_ReadDomainTransferRow` | implementation_inventory, record_reader |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::ZrCore_Artifact_ReadLayoutRow` | implementation_inventory, record_reader |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::ZrCore_Artifact_ReadMemberDefRow` | implementation_inventory, record_reader |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::ZrCore_Artifact_ReadMetadataRecordRow` | implementation_inventory, record_reader |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::ZrCore_Artifact_ReadMetadataStateRow` | implementation_inventory, record_reader |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::ZrCore_Artifact_ReadPropertyDefRow` | implementation_inventory, record_reader |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::ZrCore_Artifact_ReadRelocationRow` | implementation_inventory, record_reader |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::ZrCore_Artifact_ReadSchedulerContractRow` | implementation_inventory, record_reader |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::ZrCore_Artifact_ReadTypeDefRow` | implementation_inventory, record_reader |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::ZrCore_Artifact_ReadTypeIdentityRow` | implementation_inventory, record_reader |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::artifact_write_contract_row` | implementation_inventory, record_writer |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::artifact_write_domain_transfer_row` | implementation_inventory, record_writer |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::artifact_write_layout_row` | implementation_inventory, record_writer |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::artifact_write_member_row` | implementation_inventory, record_writer |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::artifact_write_metadata_record_row` | implementation_inventory, record_writer |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::artifact_write_metadata_state_row` | implementation_inventory, record_writer |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::artifact_write_property_row` | implementation_inventory, record_writer |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::artifact_write_relocation_row` | implementation_inventory, record_writer |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::artifact_write_scheduler_contract_row` | implementation_inventory, record_writer |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::artifact_write_type_def_row` | implementation_inventory, record_writer |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::artifact_write_type_identity_row` | implementation_inventory, record_writer |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c::zr_artifact_write_section_payload` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Read` | reader_admission |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::ZrCore_Artifact_Write` | writer |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::artifact_read_identity` | record_reader |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::artifact_validate_document` | writer_admission |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c::artifact_write_identity` | record_writer |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_schema_metadata_graph.c::artifact_metadata_decoded_count_members` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_schema_metadata_graph.c::artifact_metadata_decoded_count_properties` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_schema_metadata_graph.c::artifact_metadata_decoded_count_records` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_schema_metadata_graph.c::artifact_metadata_decoded_find_layout` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_schema_metadata_graph.c::artifact_metadata_decoded_find_member_owner` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_schema_metadata_graph.c::artifact_metadata_decoded_find_type_def` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_schema_metadata_graph.c::artifact_metadata_decoded_has_contract_hash` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_schema_metadata_graph.c::artifact_metadata_decoded_has_type_token` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_schema_metadata_graph.c::artifact_metadata_decoded_member_has_owner` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_schema_metadata_graph.c::artifact_metadata_input_has_contract_hash` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_schema_metadata_graph.c::artifact_metadata_validate_decoded_links` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_schema_metadata_graph.c::artifact_metadata_validate_decoded_state` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_schema_metadata_graph.c::artifact_metadata_validate_input_links` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_schema_metadata_graph.c::artifact_metadata_validate_input_state` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_schema_metadata_graph.c::artifact_metadata_validate_layout_map` | implementation_inventory, layout_map_admission |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_schema_metadata_graph.c::zr_artifact_metadata_graph_validate_decoded` | cross_section_admission, implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_schema_metadata_graph.c::zr_artifact_metadata_graph_validate_input` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::ZrCore_Artifact_ReadCallableSignatureSummary` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::ZrCore_Artifact_ValidateSignature` | implementation_inventory, signature_grammar_admission |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::artifact_signature_read_u32` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::artifact_signature_read_u8` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::artifact_signature_validate_function` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::artifact_signature_validate_node` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/artifact_signature.c::artifact_signature_validate_nodes` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/canonical_consumer.c::ZrCore_CanonicalConsumer_ResolveDomainTransfer` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/canonical_consumer.c::ZrCore_CanonicalConsumer_ResolveLayout` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/canonical_consumer.c::ZrCore_CanonicalConsumer_ResolveSchedulerContract` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/canonical_consumer.c::ZrCore_CanonicalConsumer_ResolveTypeId` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/canonical_consumer.c::ZrCore_CanonicalConsumer_ResolveTypeToken` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/canonical_consumer.c::ZrCore_CanonicalConsumer_ValidatePublicRefLikeAbi` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/canonical_consumer.c::ZrCore_CanonicalConsumer_ValidateSchedulerContract` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/canonical_consumer.c::canonical_consumer_hash_fail` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/canonical_consumer.c::canonical_consumer_project_domain_transfer` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/canonical_consumer.c::canonical_consumer_project_identity_row` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/canonical_consumer.c::canonical_consumer_project_type_def` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/canonical_consumer.c::canonical_consumer_validate_callable_contract` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Read` | reader_admission |
| `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::ZrCore_ArtifactExecIrScalar_Write` | writer |
| `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar.c::scalar_select_wire_format` | profile_admission |
| `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c::ZrCore_ArtifactExecIrScalarEis3_GetEncodedSize` | profile_admission |
| `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c::ZrCore_ArtifactExecIrScalarEis3_Read` | reader_admission |
| `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis3.c::ZrCore_ArtifactExecIrScalarEis3_Write` | writer |
| `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c::ZrCore_ArtifactExecIrScalarEis4_GetEncodedSize` | profile_admission |
| `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c::ZrCore_ArtifactExecIrScalarEis4_Read` | reader_admission |
| `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis4.c::ZrCore_ArtifactExecIrScalarEis4_Write` | writer |
| `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5.c::ZrCore_ArtifactExecIrScalarEis5_ValidateModule` | profile_admission |
| `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5_read.c::ZrCore_ArtifactExecIrScalarEis5_Read` | reader_admission |
| `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis5_write.c::ZrCore_ArtifactExecIrScalarEis5_Write` | writer |
| `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6.c::ZrCore_ArtifactExecIrScalarEis6_ValidateModule` | profile_admission |
| `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6_read.c::ZrCore_ArtifactExecIrScalarEis6_Read` | reader_admission |
| `zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6_write.c::ZrCore_ArtifactExecIrScalarEis6_Write` | writer |
| `zr_vm_core/src/zr_vm_core/call/call_binding.c::ZrCore_CallBinding_CheckContract` | admission_compare_codec_or_hash_implementation, implementation_inventory |
| `zr_vm_core/src/zr_vm_core/call/call_binding.c::ZrCore_CallBinding_CompareContracts` | admission_compare_codec_or_hash_implementation |
| `zr_vm_core/src/zr_vm_core/call/call_binding.c::ZrCore_CallBinding_Invalidate` | admission_compare_codec_or_hash_implementation, implementation_inventory |
| `zr_vm_core/src/zr_vm_core/call/call_binding.c::ZrCore_CallBinding_Resolve` | admission_compare_codec_or_hash_implementation, implementation_inventory |
| `zr_vm_core/src/zr_vm_core/call/call_binding.c::ZrCore_CallBinding_Validate` | admission_compare_codec_or_hash_implementation, implementation_inventory |
| `zr_vm_core/src/zr_vm_core/call/call_binding.c::binding_validate_target` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/call/call_binding_contract_internal.h::zr_core_call_binding_check_contract` | implementation_inventory, runtime_owner_or_state_consumer, shared_contract_admission |
| `zr_vm_core/src/zr_vm_core/call/call_binding_contract_internal.h::zr_core_call_binding_contract_diagnostic_init` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/call/call_binding_contract_internal.h::zr_core_call_binding_contract_fail` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/call/call_binding_encoding.c::ZrCore_CallBinding_DecodeContract` | admission_compare_codec_or_hash_implementation |
| `zr_vm_core/src/zr_vm_core/call/call_binding_encoding.c::ZrCore_CallBinding_EncodeContract` | admission_compare_codec_or_hash_implementation, writer |
| `zr_vm_core/src/zr_vm_core/call/call_binding_encoding.c::zr_call_binding_decode_contract_status` | admission_compare_codec_or_hash_implementation, reader_admission |
| `zr_vm_core/src/zr_vm_core/call/call_binding_inline_frame.c::zr_call_binding_prepare_inline_frame_member` | hash_caller |
| `zr_vm_core/src/zr_vm_core/call/call_binding_layout.c::ZrCore_CallBinding_PrototypeLayoutHash` | hash_producer |
| `zr_vm_core/src/zr_vm_core/call/call_binding_link.c::link_function` | hash_caller |
| `zr_vm_core/src/zr_vm_core/call/call_binding_member.c::ZrCore_CallBinding_PrepareMember` | hash_caller |
| `zr_vm_core/src/zr_vm_core/call/call_binding_signature.c::ZrCore_CallBinding_FunctionSignatureHash` | admission_compare_codec_or_hash_implementation, hash_producer |
| `zr_vm_core/src/zr_vm_core/call/call_binding_signature.c::signature_string_hash` | admission_compare_codec_or_hash_implementation |
| `zr_vm_core/src/zr_vm_core/call/call_binding_signature.c::signature_type_hash` | admission_compare_codec_or_hash_implementation |
| `zr_vm_core/src/zr_vm_core/call/typed_call_binding.c::ZrCore_CallBinding_PrepareTypedCall` | hash_caller |
| `zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c::ZrCore_ExecIr_ModuleAddFunction` | producer |
| `zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c::ZrCore_ExecIr_FunctionBindingRowsHash` | hash_producer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend.c::ZrCore_ExecutionBackendService_Cancel` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend_compile.c::ZrCore_ExecutionBackendService_CompileAsync` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend_compile.c::ZrCore_ExecutionBackendService_Complete` | implementation_inventory, runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend.c::ZrCore_ExecutionBackendService_Deinit` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend.c::ZrCore_ExecutionBackendService_FinalizeShutdown` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend.c::ZrCore_ExecutionBackendService_Init` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend.c::ZrCore_ExecutionBackendService_InvalidateGeneration` | implementation_inventory, runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend_compile.c::ZrCore_ExecutionBackendService_ProcessNext` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend_compile.c::ZrCore_ExecutionBackendService_Publish` | implementation_inventory, runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend_compile.c::ZrCore_ExecutionBackendService_QueryTicket` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend.c::ZrCore_ExecutionBackendService_Register` | implementation_inventory, runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend.c::ZrCore_ExecutionBackendService_ResumeInterpreter` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend.c::ZrCore_ExecutionBackendService_Shutdown` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend.c::ZrCore_ExecutionBackendService_Unregister` | implementation_inventory, runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend_facade.c::ZrCore_ExecutionBackend_InvalidateGeneration` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend_facade.c::ZrCore_ExecutionBackend_Register` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend.c::zr_execution_backend_cancel_compiling_one` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend_compile.c::zr_execution_backend_choose_fallback` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend_compile.c::zr_execution_backend_code_identity_exists_locked` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend_compile.c::zr_execution_backend_code_mismatch_status` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend.c::zr_execution_backend_code_references_registration` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend_compile.c::zr_execution_backend_fill_fallback` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend_compile.c::zr_execution_backend_find_candidate_locked` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend_compile.c::zr_execution_backend_find_code_slot_locked` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend_compile.c::zr_execution_backend_find_job_slot_locked` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend.c::zr_execution_backend_job_references_registration` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend_compile.c::zr_execution_backend_mark_callback_failure` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend.c::zr_execution_backend_validate_descriptor` | implementation_inventory, runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend_compile.c::zr_execution_backend_validate_request` | implementation_inventory, runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend_internal.h::zr_execution_backend_code_map_hash` | implementation_inventory, runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend_internal.h::zr_execution_backend_code_shape_valid` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend_internal.h::zr_execution_backend_descriptor_shape_valid` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend_internal.h::zr_execution_backend_fail` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend_internal.h::zr_execution_backend_find_job_locked` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend_internal.h::zr_execution_backend_generation_equal` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend_internal.h::zr_execution_backend_generation_invalidated_locked` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend_internal.h::zr_execution_backend_key_valid` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend_internal.h::zr_execution_backend_lock` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend_internal.h::zr_execution_backend_mark_codes_retired_locked` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend_internal.h::zr_execution_backend_mark_jobs_cancelled_locked` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend_internal.h::zr_execution_backend_request_shape_valid` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend_internal.h::zr_execution_backend_service_shape_valid` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_backend_internal.h::zr_execution_backend_unlock` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c::ZrCore_ExecutionBackendService_AcquireCode` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c::ZrCore_ExecutionBackendService_AcquireDependencyLease` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c::ZrCore_ExecutionBackendService_CollectRetired` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c::ZrCore_ExecutionBackendService_LookupEntry` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c::ZrCore_ExecutionBackendService_QueryCode` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c::ZrCore_ExecutionBackendService_QueryMap` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c::ZrCore_ExecutionBackendService_ReleaseCode` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c::ZrCore_ExecutionBackendService_ReleaseDependencyLease` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c::zr_execution_backend_collect_code_slot` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c::zr_execution_backend_dispose_unpublished` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c::zr_execution_backend_find_code_locked_local` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c::zr_execution_backend_finish_callback` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c::zr_execution_backend_handle_ticket_id` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c::zr_execution_backend_validate_handle_locked` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/function/function.c::ZrCore_Function_Free` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/gc/gc_cycle.c::garbage_collector_function_callsite_caches_reference_live_young` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/gc/gc_cycle.c::garbage_collector_rewrite_function_graph` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/gc/gc_mark.c::garbage_collector_scan_object` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_capability.c::ZrCore_HotPatch_ValidateCapabilityClosure` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_validate.c::ZrCore_HotPatch_ComputePolicyHash` | hash_producer, implementation_inventory |
| `zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_validate.c::ZrCore_HotPatch_RecheckZrafContent` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_validate.c::ZrCore_HotPatch_Validate` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_validate.c::ZrCore_HotPatch_ValidateZraf` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/io/io_call_binding.c::ZrCore_Io_ReadCallBindings` | implementation_inventory, legacy_container_reader_admission, reader_bridge |
| `zr_vm_core/src/zr_vm_core/io/io_runtime.c::io_runtime_copy_callsite_cache_metadata` | runtime_owner_or_state_consumer |
| `zr_vm_core/src/zr_vm_core/module/module_call_binding.c::link_imported_function` | hash_caller |
| `zr_vm_core/src/zr_vm_core/runtime/execution_contract.c::ZrCore_ExecutionContract_Check` | equality_only_not_supported_version_admission |
| `zr_vm_core/src/zr_vm_core/type/type_layout.c::ZrCore_TypeLayout_ComputeHash` | hash_producer, implementation_inventory |
| `zr_vm_core/src/zr_vm_core/type/type_layout.c::ZrCore_TypeLayout_Validate` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/type/type_layout.c::type_layout_checked_add` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/type/type_layout.c::type_layout_hash_byte` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/type/type_layout.c::type_layout_hash_map` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/type/type_layout.c::type_layout_hash_u32` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/type/type_layout.c::type_layout_hash_u64` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/type/type_layout.c::type_layout_read_union_tag` | implementation_inventory |
| `zr_vm_core/src/zr_vm_core/type/type_layout.c::type_layout_validate_map` | implementation_inventory |
| `zr_vm_library/src/zr_vm_library/native_binding/native_binding_typed_call.c::native_registry_resolve_typed_call_binding` | hash_caller |
| `zr_vm_parser/src/zr_vm_parser/artifact/artifact_call_binding_projection.c::projection_copy_row` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/artifact/artifact_call_binding_projection.c::projection_fail` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/artifact/artifact_metadata_projection.c::artifact_metadata_projection_fail` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::ZrParser_ArtifactType_WriteSignature` | implementation_inventory, signature_projection_owner |
| `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::artifact_layout_projection_fail` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::artifact_projection_domain_hash` | hash_producer, implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::artifact_projection_fail` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::artifact_type_read_function` | hash_caller, implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::artifact_type_read_generic_arguments` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::artifact_type_read_node` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::artifact_type_read_type_list` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::artifact_type_read_u32` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::artifact_type_read_u64` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::artifact_type_read_u8` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::artifact_type_write_bytes` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::artifact_type_write_function` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::artifact_type_write_generic_arguments` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::artifact_type_write_node` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::artifact_type_write_type_id_array` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::artifact_type_write_u32` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::artifact_type_write_u64` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c::artifact_type_write_u8` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_call_bindings.c::backend_aot_c_project_call_binding` | hash_caller |
| `zr_vm_parser/src/zr_vm_parser/compiler/compile_ir_cache.c::ZrParser_CompileIrCache_BeginWrite` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/compiler/compile_ir_cache.c::ZrParser_CompileIrCache_BuildKey` | hash_producer, implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/compiler/compile_ir_cache.c::ZrParser_CompileIrCache_CancelWrite` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/compiler/compile_ir_cache.c::ZrParser_CompileIrCache_Lookup` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/compiler/compile_ir_cache.c::ZrParser_CompileIrCache_Publish` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/compiler/compile_ir_cache.c::cache_hash_bytes` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/compiler/compile_ir_cache.c::cache_hash_u64` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/compiler/compile_ir_cache.c::cache_payload_digest` | hash_producer |
| `zr_vm_parser/src/zr_vm_parser/compiler/compile_optimization_profile.c::ZrParser_CompileOptimizationProfile_Hash` | hash_producer |
| `zr_vm_parser/src/zr_vm_parser/compiler/compiler_call_binding.c::binding_finalize_function` | hash_caller |
| `zr_vm_parser/src/zr_vm_parser/compiler/compiler_call_binding.c::compiler_typed_call_signature_hash` | hash_caller |
| `zr_vm_parser/src/zr_vm_parser/compiler/compiler_module_call_binding.c::compiler_publish_module_call_bindings` | hash_caller |
| `zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_callable_identity.c::compiler_script_callable_identity_try_publish` | canonical_to_exec_ir_bridge, hash_caller |
| `zr_vm_parser/src/zr_vm_parser/compiler/compiler_source_callable_identity.c::source_identity_candidate` | canonical_to_exec_ir_bridge |
| `zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_callable_binding_refinement.c::compiler_refine_function_type_binding_return` | hash_caller |
| `zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_class.c::compiler_class_bind_interface_contract_slot` | hash_caller |
| `zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_metadata_module_hash.c::metadata_module_hash_buffer_size` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_metadata_module_hash.c::metadata_module_hash_count_identity_records` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_metadata_module_hash.c::metadata_module_hash_record_participates` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_metadata_module_hash.c::metadata_module_hash_sort_export_indices` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_metadata_module_hash.c::metadata_module_hash_sort_identity_indices` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_metadata_module_hash.c::metadata_module_hash_write_buffer` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_metadata_module_hash.c::metadata_module_hash_write_export_symbols` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_metadata_module_hash.c::metadata_module_hash_write_identity_records` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_metadata_module_hash.c::metadata_module_hash_write_u64` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_metadata_module_hash.c::metadata_token_compute_module_signature_hash` | hash_producer, implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_metadata_signature.c::metadata_signature_hash_v1` | hash_producer, implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_metadata_signature.c::metadata_token_try_resolve_union_signature_type` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_metadata_signature.c::metadata_token_write_field_signature` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_metadata_signature.c::metadata_token_write_generic_argument_signatures` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_metadata_signature.c::metadata_token_write_method_signature` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_metadata_signature.c::metadata_token_write_string_ref` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_metadata_signature.c::metadata_token_write_symbol_signature` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_metadata_signature.c::metadata_token_write_type_ref_signature` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_metadata_signature.c::metadata_token_write_u32` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_metadata_signature.c::metadata_token_write_u8` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_source_retention.c::compiler_retain_source_scalar_metadata` | canonical_to_exec_ir_bridge |
| `zr_vm_parser/src/zr_vm_parser/contract/syntax_contract.c::ZrParser_SyntaxCallable_Intern` | hash_caller |
| `zr_vm_parser/src/zr_vm_parser/contract/syntax_contract.c::ZrParser_SyntaxCallable_RefineFromDeclaration` | hash_caller |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_dead_source_places.c::dead_source_prove_semantic_literal` | canonical_to_exec_ir_bridge |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_host_aot_target.c::ZrParser_ExecIr_MakeHostNoArgsI64AotTarget` | canonical_to_exec_ir_bridge |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_aot.c::ZrParser_ExecIr_LowerAotWithCanonicalCallable` | canonical_to_exec_ir_bridge |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_aot.c::ZrParser_ExecIr_LowerAotWithConstantsAndLayouts` | canonical_to_exec_ir_bridge |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_profile.c::ZrParser_ExecIr_ImportProfile` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_profile.c::ZrParser_ExecIr_ProfileBuildKey` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_profile.c::ZrParser_ExecIr_ProfileHash` | hash_producer, implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_profile.c::ZrParser_ExecIr_ProfileImport` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_profile.c::ZrParser_ExecIr_ProfileModuleHash` | hash_producer, implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_profile.c::ZrParser_ExecIr_ProfileValidate` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_profile.c::zr_profile_function_hash` | hash_producer, implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_profile.c::zr_profile_hash_u32` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_profile.c::zr_profile_hash_u64` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_profile.c::zr_profile_module_ir_hash` | hash_producer, implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_profile.c::zr_profile_validate_sites` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_callable_query.c::callable_node` | canonical_to_exec_ir_bridge |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_callable_query.c::source_callable_query` | canonical_to_exec_ir_bridge |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module_validation.c::source_finalized_module_admit_source` | canonical_to_exec_ir_bridge |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module_validation.c::source_finalized_module_metadata` | canonical_to_exec_ir_bridge |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_integer_expression.c::exec_storage` | canonical_to_exec_ir_bridge |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_integer_expression.c::source_integer_check_ast` | canonical_to_exec_ir_bridge |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_integer_expression.c::source_integer_check_function` | canonical_to_exec_ir_bridge |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_integer_expression.c::source_integer_prove` | canonical_to_exec_ir_bridge |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_module_contract.c::ZrParser_ExecIr_BindSourceModuleContract` | canonical_to_exec_ir_bridge |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_module_contract.c::source_contract_canonical` | canonical_to_exec_ir_bridge |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_module_contract.c::source_contract_existing` | canonical_to_exec_ir_bridge |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_module_contract.c::source_contract_pair` | canonical_to_exec_ir_bridge |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_scalar_constant.c::scalar_callable` | canonical_to_exec_ir_bridge |
| `zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c::type_inference_record_construct_call_facts` | hash_caller |
| `zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_super_constructor_semantic_facts.c::type_inference_super_constructor_call_type_id` | hash_caller |
| `zr_vm_parser/src/zr_vm_parser/types/canonical_type.c::ZrParser_CanonicalType_InternFunction` | admission_compare_codec_or_hash_implementation |
| `zr_vm_parser/src/zr_vm_parser/types/canonical_type.c::ZrParser_CanonicalType_ValidateParameterContract` | admission_compare_codec_or_hash_implementation |
| `zr_vm_parser/src/zr_vm_parser/types/canonical_type.c::canonical_array_hash` | hash_producer |
| `zr_vm_parser/src/zr_vm_parser/types/canonical_type.c::canonical_generic_instance_hash` | hash_producer |
| `zr_vm_parser/src/zr_vm_parser/types/canonical_type.c::canonical_generic_parameter_hash` | hash_producer |
| `zr_vm_parser/src/zr_vm_parser/types/canonical_type.c::canonical_nominal_hash` | hash_producer |
| `zr_vm_parser/src/zr_vm_parser/types/canonical_type.c::canonical_parameter_contract_array_equal` | admission_compare_codec_or_hash_implementation |
| `zr_vm_parser/src/zr_vm_parser/types/canonical_type.c::canonical_parameter_contract_equal` | admission_compare_codec_or_hash_implementation |
| `zr_vm_parser/src/zr_vm_parser/types/canonical_type.c::canonical_parameter_contract_hash` | admission_compare_codec_or_hash_implementation, hash_producer |
| `zr_vm_parser/src/zr_vm_parser/types/canonical_type.c::canonical_primitive_hash` | hash_producer |
| `zr_vm_parser/src/zr_vm_parser/types/canonical_type.c::canonical_qualified_target_hash` | hash_producer |
| `zr_vm_parser/src/zr_vm_parser/types/canonical_type.c::canonical_type_list_hash` | hash_producer |
| `zr_vm_parser/src/zr_vm_parser/types/canonical_type_adapter.c::ZrParser_CanonicalType_FromFunctionSignatureWithGenericBindings` | hash_caller |
| `zr_vm_parser/src/zr_vm_parser/types/canonical_type_adapter.c::ZrParser_CanonicalType_RebindFunctionSignature` | hash_caller |
| `zr_vm_parser/src/zr_vm_parser/types/canonical_type_projection.c::canonical_type_substitute_projection` | hash_caller |
| `zr_vm_parser/src/zr_vm_parser/writer/writer_call_binding.c::ZrParser_Writer_WriteCallBindings` | implementation_inventory, legacy_container_writer, writer_bridge |
| `zr_vm_parser/src/zr_vm_parser/writer/writer_call_binding.c::write_u32` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir.c::ZrParser_ExecIr_Read` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir.c::ZrParser_ExecIr_Write` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c::ZrParser_ExecIrArtifact_TestInjectWriteFailure` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c::ZrParser_ExecIr_WriteCanonicalZroFile` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c::exec_ir_writer_fail` | implementation_inventory |
| `zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c::exec_ir_writer_metadata_is_supported` | implementation_inventory |

### I索引额外实现位置（不冒称原C315覆盖）

本次I族索引发现/补定位的实际定义中，有 19 个file+symbol不在上述selected C315中，单独记为C_I定位增量；这不是新语义接口或全项目分母。若未来合并，必须先按current relocation identity去重，再取315与此差集union，不能机械加role/source-sites。

- `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c::ZrParser_ExecIr_Build`
- `zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c::ZrCore_ExecIr_VerifyFunction`
- `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effects.c::ZrParser_ExecIr_Verify`
- `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c::ZrParser_ExecIr_Optimize`
- `zr_vm_parser/src/zr_vm_parser/exec_ir/execbc/exec_ir_lower_execbc.c::ZrParser_ExecIr_LowerExecBc`
- `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_aot.c::ZrParser_ExecIr_LowerAot`
- `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_lowering.c::ZrParser_AotIr_LowerShared`
- `zr_vm_core/src/zr_vm_core/execution/execution_backend_facade.c::ZrCore_ExecutionBackend_CompileAsync`
- `zr_vm_core/src/zr_vm_core/execution/execution_backend_facade.c::ZrCore_ExecutionBackend_ResumeInterpreter`
- `zr_vm_core/src/zr_vm_core/runtime/optimization_remark.c::ZrCore_OptimizationRemarks_Query`
- `zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_rollback.c::ZrCore_HotPatch_ApplyValidated`
- `zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_rollback.c::ZrCore_HotPatch_Rollback`
- `zr_vm_core/src/zr_vm_core/gc/gc_domain.c::ZrCore_GcDomain_GetIdentity`
- `zr_vm_core/src/zr_vm_core/gc/gc_domain_share.c::ZrCore_GcDomain_ShareValue`
- `zr_vm_core/src/zr_vm_core/gc/gc_domain_clone.c::ZrCore_GcDomainClone_Prepare`
- `zr_vm_core/src/zr_vm_core/ownership/ownership_transfer_cross_domain.c::ZrCore_DomainTransferGraph_Prepare`
- `zr_vm_core/src/zr_vm_core/gc/gc_domain_clone.c::ZrCore_GcDomainClone_Execute`
- `zr_vm_core/src/zr_vm_core/gc/gc_budget_runtime.c::ZrCore_Gc_SetBudget`
- `zr_vm_core/src/zr_vm_core/gc/gc_budget_runtime.c::ZrCore_Gc_GetStats`

## 全47依赖与当前可交付结论

本四页固定I/F/H/C的职责、可读索引、坐标与兼容选择；D1–D6不再留“以后再决定”。planned API与producer/caller由接口矩阵指定叶实现。policy文档可被独立review接受；实现/运行验收仍须对应owner提供证据。

原opaque内层grammar、跨context重映射、hash失败通道、provider token/hash实体对应、backend并发lifetime和真实resume等限制按上文保留，不因dictionary文字交付变成已实现。Root仍须对00.02全部要求和独立文档审查收口，不由本页宣布全47完成。未运行loader/patch/capability/VM/FFI/network。

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
