---
related_code:
  - zr_vm_cli/src/zr_vm_cli/commands/explain_optimize_command.c
  - zr_vm_core/include/zr_vm_core/artifact_exec_ir.h
  - zr_vm_core/include/zr_vm_core/capability_manifest.h
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/include/zr_vm_core/execution_backend.h
  - zr_vm_core/include/zr_vm_core/gc.h
  - zr_vm_core/include/zr_vm_core/gc_domain.h
  - zr_vm_core/include/zr_vm_core/gc_domain_clone.h
  - zr_vm_core/include/zr_vm_core/hotpatch_rollback.h
  - zr_vm_core/include/zr_vm_core/optimization_remark.h
  - zr_vm_core/src/zr_vm_core/artifact/artifact_exec_ir.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_compile.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_facade.c
  - zr_vm_core/src/zr_vm_core/gc/gc_budget_runtime.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_clone.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_share.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_rollback.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_validate.c
  - zr_vm_core/src/zr_vm_core/module/module_exec_ir_artifact.c
  - zr_vm_core/src/zr_vm_core/ownership/ownership_transfer.c
  - zr_vm_core/src/zr_vm_core/ownership/ownership_transfer_cross_domain.c
  - zr_vm_core/src/zr_vm_core/ownership/ownership_transfer_cross_domain_internal.h
  - zr_vm_core/src/zr_vm_core/ownership/ownership_transfer_value_copy.c
  - zr_vm_core/src/zr_vm_core/runtime/optimization_remark.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_optimization_remarks.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_isolated_domain.c
  - zr_vm_parser/include/zr_vm_parser/aot_ir_lowering.h
  - zr_vm_parser/include/zr_vm_parser/exec_ir_builder.h
  - zr_vm_parser/include/zr_vm_parser/exec_ir_pass_manager.h
  - zr_vm_parser/src/zr_vm_parser/artifact/artifact_exec_ir.h
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_adapter.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_source_retention.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_branch_facts.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_call_graph.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_escape.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_lowering.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_cleanup_drops.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_dead_source_places.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effects.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_aot.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projections.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa_promotion.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/execbc/exec_ir_lower_execbc.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_allocation.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_licm.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_ownership_elision.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_frame.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_frame_validation.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c
implementation_files:
  - zr_vm_cli/src/zr_vm_cli/commands/explain_optimize_command.c
  - zr_vm_core/include/zr_vm_core/artifact_exec_ir.h
  - zr_vm_core/include/zr_vm_core/capability_manifest.h
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/include/zr_vm_core/execution_backend.h
  - zr_vm_core/include/zr_vm_core/gc.h
  - zr_vm_core/include/zr_vm_core/gc_domain.h
  - zr_vm_core/include/zr_vm_core/gc_domain_clone.h
  - zr_vm_core/include/zr_vm_core/hotpatch_rollback.h
  - zr_vm_core/include/zr_vm_core/optimization_remark.h
  - zr_vm_core/src/zr_vm_core/artifact/artifact_exec_ir.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_compile.c
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_facade.c
  - zr_vm_core/src/zr_vm_core/gc/gc_budget_runtime.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_clone.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_share.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_rollback.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_validate.c
  - zr_vm_core/src/zr_vm_core/module/module_exec_ir_artifact.c
  - zr_vm_core/src/zr_vm_core/ownership/ownership_transfer.c
  - zr_vm_core/src/zr_vm_core/ownership/ownership_transfer_cross_domain.c
  - zr_vm_core/src/zr_vm_core/ownership/ownership_transfer_cross_domain_internal.h
  - zr_vm_core/src/zr_vm_core/ownership/ownership_transfer_value_copy.c
  - zr_vm_core/src/zr_vm_core/runtime/optimization_remark.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_optimization_remarks.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_isolated_domain.c
  - zr_vm_parser/include/zr_vm_parser/aot_ir_lowering.h
  - zr_vm_parser/include/zr_vm_parser/exec_ir_builder.h
  - zr_vm_parser/include/zr_vm_parser/exec_ir_pass_manager.h
  - zr_vm_parser/src/zr_vm_parser/artifact/artifact_exec_ir.h
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_adapter.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_source_retention.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_branch_facts.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_call_graph.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_escape.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_lowering.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_cleanup_drops.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_dead_source_places.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effects.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_aot.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projections.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa_promotion.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/execbc/exec_ir_lower_execbc.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_allocation.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_licm.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_ownership_elision.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_frame.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_frame_validation.c
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
doc_type: testing-guide
---

# SSA 接口契约矩阵：19族、20操作

## 导航与冻结范围

[运行期 owner](../core-runtime/execution-contract-ownership.md) · [持久字段](../module-system/persistent-contract-fields.md) · [接口矩阵](../testing-and-validation/ssa-interface-contract-matrix.md) · [兼容政策](../testing-and-validation/ssa-contract-compatibility.md)。四页共同构成 00.02 的文字字典；[47 叶依赖图](../plans/ssa/index.md)与各叶运行验收保持独立。文档冻结不授予实现、加载、安全测试或运行权限；planned API 不据此成为已实现。


## 分母与 owner

原 I19 全部按原顺序保留；ReadWrite 包含 Read/Write 两操作，合计20。variant、wrapper、helper、service/facade 不增加族。public export decoration 不等于 installed header；声明、定义、静态 caller 与实际运行分别计。

D6 明确选择：本页作为 Git 内 current I 权威索引；保留 sealed历史证据，应用已接受offset/IO纠正与有限 H/runtime overlay；下游缺失 client 归所属叶 planned integration，不再在00.02重复设计接口名字，也不据声明计 consumer。完整F字段、hash与兼容边界见另外三页。

| ID / draft族 | 状态与唯一 owner | 实际实现名/层次 | 落地与consumer边界 |
| --- | --- | --- | --- |
| I01 / `Build` | implemented / Parser / ExecIR builder | `ZrParser_ExecIr_Build` | 01.01；BuildModule -> Build at exec_ir_build.c:906; compiler_source_retention.c:248 -> BuildModule; not a claim that every parser compilation uses this path |
| I02 / `Verify` | implemented_Core_with_Parser_facades / Core / ExecIR verification | `ZrCore_ExecIr_VerifyFunction, ZrParser_ExecIr_Verify` | 01.01；Core seal/module verification; Parser Build, passes, source retention, state-map validation; exact sites indexed |
| I03 / `Optimize` | implemented_public_entry_without_external_production_caller / Parser / ExecIR pass manager | `ZrParser_ExecIr_Optimize` | 02.01/02.02（当前两scalar）；Only test callers of exact public Optimize in scanned current first-party C/H; internal verifier calls are not clients |
| I04 / `LowerExecBc` | implemented_private_source_header_entry / Parser / ExecBC projection | `ZrParser_ExecIr_LowerExecBc` | 01.05；No external production direct caller of exact LowerExecBc; static test callers indexed |
| I05 / `LowerAot` | implemented_private_source_header_entry_plus_distinct_shared_backend_layer / Parser / AOT projection | `ZrParser_ExecIr_LowerAot, ZrParser_AotIr_LowerShared` | 07.01；Exact LowerAot has tests only; compiler_source_retention.c:330 -> LowerAotWithCanonicalCallable; backend_aot_ir_adapter.c:240 -> LowerShared; exec_ir_aot_lowering.c:183 internal wrapper |
| I06 / `ReadWrite` | implemented_exact_draft_wrappers_private_source_header / Parser / ExecIR artifact adapter; Core owns codec | `ZrParser_ExecIr_WriteCanonicalZroFile, ZrCore_ArtifactExecIr_Read, ZrCore_ArtifactExecIr_Write, ZrParser_ExecIr_Read, ZrParser_ExecIr_Write` | 08.01；Exact Parser Read/Write only tests/library/test_ssa_schema_relocation.c; no production caller; Core Read <- module_exec_ir_artifact.c:166 and writer_exec_ir.c:9; Core Write <- writer_exec_ir.c:21 and writer_exec_ir_artifact.c:169; canonical ZRO file entry is separate, tests only |
| I07 / `ExecutionBackend_Register` | implemented_Service_and_default_bool_facade / Core / execution backend service | `ZrCore_ExecutionBackendService_Register, ZrCore_ExecutionBackend_Register` | 10.01；Service production direct caller only default facade; no current external production caller of facade or SetDefaultService |
| I08 / `ExecutionBackend_CompileAsync` | implemented_Service_and_default_bool_facade / Core / execution backend compile service | `ZrCore_ExecutionBackendService_CompileAsync, ZrCore_ExecutionBackend_CompileAsync` | 10.01；Production direct Service caller only facade; no external production facade caller; tests exercise metadata mock |
| I09 / `ExecutionBackend_InvalidateGeneration` | implemented_Service_and_default_bool_facade / Core / execution backend lifetime | `ZrCore_ExecutionBackendService_InvalidateGeneration, ZrCore_ExecutionBackend_InvalidateGeneration` | 08.03/10.01；Production direct Service caller only facade; no external production facade caller |
| I10 / `ExecutionBackend_ResumeInterpreter` | implemented_callback_dispatch_without_runtime_provider_consumer / Core / execution backend resume adapter | `ZrCore_ExecutionBackendService_ResumeInterpreter, ZrCore_ExecutionBackend_ResumeInterpreter` | 01.04/10.01；Default facade delegates Service; sole exact Service consumer outside facade is metadata test callback, source77 |
| I11 / `OptimizationRemarks_Query` | implemented_with_real_CLI_and_LSP_consumers / Core / optimization remark storage/query | `ZrCore_OptimizationRemarks_Query` | 11.02；CLI explain_optimize_command.c:521; LSP lsp_optimization_remarks.c:279/289 |
| I12 / `HotPatch_Validate` | implemented_validators_without_production_host_consumer / Core / hotpatch admission | `ZrCore_HotPatch_Validate, ZrCore_HotPatch_ValidateZraf` | 08.02；No production direct caller of either validator in finite current source scan; tests call Validate; not evidence of live loader hookup |
| I13 / `HotPatch_Apply` | implemented_renamed_draft_mapping_not_C_alias / Core / hotpatch version entry publication | `ZrCore_HotPatch_ApplyValidated, ZrCore_HotPatch_Apply` | 08.03；Exact draft ZrCore_HotPatch_Apply absent; actual ApplyValidated used by tests only |
| I14 / `HotPatch_Rollback` | implemented_metadata_rollback_without_production_host_consumer / Core / hotpatch generation manager | `ZrCore_HotPatch_Rollback` | 08.04；Only test direct callers in finite current source scan |
| I15 / `Domain_GetContract` | planned_exact_entry_identity_query_is_partial_existing_support / Core / GC-domain contract (proposed accountable owner) | `ZrCore_GcDomain_GetIdentity, ZrCore_Domain_GetContract` | 06.03；Actual GetIdentity production consumers indexed; none may be relabelled GetContract consumer |
| I16 / `Domain_ShareValue` | implemented_object_macro_alias_without_consumer / Core / GC-domain roots/sharing | `ZrCore_Domain_ShareValue, ZrCore_GcDomain_ShareValue` | 06.03；gc_domain.h:242 object-like alias ZrCore_Domain_ShareValue -> ZrCore_GcDomain_ShareValue; no real call sites of either in finite scan |
| I17 / `Domain_TransferValue` | planned_generic_entry_with_existing_distinct_clone_and_ownership_graph_protocols / Core / cross-domain ownership transfer (proposed accountable owner) | `ZrCore_GcDomainClone_Prepare, ZrCore_DomainTransferGraph_Prepare, ZrCore_GcDomainClone_Execute, ZrCore_Domain_TransferValue` | 06.04；Existing graph and clone call sites indexed separately, not aliases and not additive semantic transfer coverage |
| I18 / `Gc_SetBudget` | implemented_budget_wrapper_without_production_client / Core / garbage collector budget | `ZrCore_Gc_SetBudget` | 06.02；Exact wrapper only tests; underlying collector API not equivalent to a host wrapper client |
| I19 / `Gc_GetStats` | implemented_budget_stats_alias_not_general_heap_snapshot / Core / garbage collector budget telemetry | `ZrCore_Gc_GetStats` | 06.02；Only tests/core/test_ssa_major_budget_runtime.c exact wrapper consumer; general GetStatsSnapshot separate API |

## 全族可调用合同

每族状态引用下方同名 status domain；domain 的 error/pending/invalidIR/link/capability/deopt/unsupported 七项都适用，未提供某类状态时明确不创造新枚举。下列输入输出和生存期是实际源码合同；D1/D2 的 planned 行按运行期页的决定执行，不能误读成已有声明。

### I01 Build

- Owner：Parser / ExecIR builder；状态域：`IR`。
- 输入域：Borrowed SemanticIR function + build options, or BuildInput(token, signature, semantic function). Parser owns AST/canonical semantic facts.
- 输出域：Core SZrExecIrFunction / module-owned function; no AST pointers are transferred to Core.
- Lifetime/rollback：Build prepares temporary; failure frees temporary and preserves initialized output; success frees old output and transfers pools. BuildModule prepares before allocating slot, copies slot contract, sets state-map identity.
- 实际定义：`ZrParser_ExecIr_Build @ zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c:878`。
- 实际声明：`ZrParser_ExecIr_Build @ zr_vm_parser/include/zr_vm_parser/exec_ir_builder.h:24`。
- Production reverse refs：`ZrParser_ExecIr_Build @ zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c:906`。
- 接入限制：No whole compiler migration credit; bounded BuildModule caller is real, draft Build exact call is its wrapper.

### I02 Verify

- Owner：Core / ExecIR verification；状态域：`IR`。
- 输入域：Borrowed Core function + explicit verify level, or module; no AST requirement.
- 输出域：Boolean + SZrExecIrDiagnostic; no execution, code or runtime witness.
- Lifetime/rollback：Read-only graph validation with verifier temporary allocations. Parser function/module entry delegates Core; module verifies ALL per function plus owned constants. Exact Core definition is current line719, not old706.
- 实际定义：`ZrCore_ExecIr_VerifyFunction @ zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:719`; `ZrParser_ExecIr_Verify @ zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effects.c:629`。
- 实际声明：`ZrCore_ExecIr_VerifyFunction @ zr_vm_core/include/zr_vm_core/exec_ir.h:584`; `ZrParser_ExecIr_Verify @ zr_vm_parser/include/zr_vm_parser/exec_ir_builder.h:54`。
- Production reverse refs：`ZrCore_ExecIr_VerifyFunction @ zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:995`; `ZrCore_ExecIr_VerifyFunction @ zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:706`; `ZrCore_ExecIr_VerifyFunction @ zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_source_retention.c:281,318`; `ZrCore_ExecIr_VerifyFunction @ zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_branch_facts.c:119`; `ZrCore_ExecIr_VerifyFunction @ zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_call_graph.c:1618,1793`; `ZrCore_ExecIr_VerifyFunction @ zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_escape.c:413`; `ZrCore_ExecIr_VerifyFunction @ zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c:469`; `ZrCore_ExecIr_VerifyFunction @ zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_cleanup_drops.c:34,53`; `ZrCore_ExecIr_VerifyFunction @ zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_dead_source_places.c:637,664,674`; `ZrCore_ExecIr_VerifyFunction @ zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effects.c:626`; `ZrCore_ExecIr_VerifyFunction @ zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa_promotion.c:117`; `ZrCore_ExecIr_VerifyFunction @ zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:376`; `ZrCore_ExecIr_VerifyFunction @ zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_allocation.c:382,541`; `ZrCore_ExecIr_VerifyFunction @ zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_licm.c:586,635,666,696,729,746`; `ZrCore_ExecIr_VerifyFunction @ zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_ownership_elision.c:475,571,606`; `ZrCore_ExecIr_VerifyFunction @ zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_frame.c:31,59`; `ZrCore_ExecIr_VerifyFunction @ zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_frame_validation.c:371`; `ZrParser_ExecIr_Verify @ zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c:737,787`。
- 接入限制：A selected VerifyFunction level is not ALL; successful shape/codec validation is not full VerifyModule.

### I03 Optimize

- Owner：Parser / ExecIR pass manager；状态域：`OPTIMIZE`。
- 输入域：Owned mutable valid Core module, options/disabled masks/budget/remarks/failure context.
- 输出域：Transactional replacement module + result counters/budgetExhausted + diagnostic.
- Lifetime/rollback：Verify original; clone candidate; selected SCCP and DCE only; per-function manager; Verify complete candidate before swap/free old owner. FALSE rolls back original and remark count, zeroes result; budget exhaustion can successfully publish verified partial progress.
- 实际定义：`ZrParser_ExecIr_Optimize @ zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c:719`。
- 实际声明：`ZrParser_ExecIr_Optimize @ zr_vm_parser/include/zr_vm_parser/exec_ir_pass_manager.h:171`。
- Production reverse refs：`未找到精确direct syntactic caller`。
- 接入限制：Do not infer full optimizer or all compiler callers integrated; direct pass/cache contracts remain distinct from public transaction.

### I04 LowerExecBc

- Owner：Parser / ExecBC projection；状态域：`LOWER`。
- 输入域：Borrowed Core function.
- 输出域：Owned SZrExecBcProjection metadata; not an executed bytecode program.
- Lifetime/rollback：BuildProjection prepares candidate. FALSE leaves initialized output; success Free old projection then transfer. Caller frees projection; unsupported operation differs from malformed graph.
- 实际定义：`ZrParser_ExecIr_LowerExecBc @ zr_vm_parser/src/zr_vm_parser/exec_ir/execbc/exec_ir_lower_execbc.c:3`。
- 实际声明：`ZrParser_ExecIr_LowerExecBc @ zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projections.h:226`。
- Production reverse refs：`未找到精确direct syntactic caller`。
- 接入限制：End-to-end baseline interpreter integration not proven by projection construction or exported decoration.

### I05 LowerAot

- Owner：Parser / AOT projection；状态域：`LOWER`。
- 输入域：Borrowed Core function; explicit variants add constant/layout pools and canonical callable context/typeID.
- 输出域：Owned SZrAotIrProjection; distinct SZrAotIrSharedModule for LowerShared; no machine-code execution implied.
- Lifetime/rollback：Transactional candidate in lowering variants; initialized old output released on success. Canonical callable variant checks matching context-local structuralHash/effects and limited noargs-i64 callable ABI.
- 实际定义：`ZrParser_ExecIr_LowerAot @ zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_aot.c:262`; `ZrParser_AotIr_LowerShared @ zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_lowering.c:62`。
- 实际声明：`ZrParser_ExecIr_LowerAot @ zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projections.h:244`; `ZrParser_AotIr_LowerShared @ zr_vm_parser/include/zr_vm_parser/aot_ir_lowering.h:64`。
- Production reverse refs：`ZrParser_AotIr_LowerShared @ zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_adapter.c:240`; `ZrParser_AotIr_LowerShared @ zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_lowering.c:183`。
- 接入限制：Do not alias LowerShared to LowerAot: different input/output level and consumers; context-local type hash may reach ExecIR contracts (H overlay).

### I06 ReadWrite

- Owner：Parser / ExecIR artifact adapter; Core owns codec；状态域：`CODEC`。
- 输入域：Read borrowed byte reader; Write borrowed artifact document/section bytes + caller output buffer.
- 输出域：Read borrowed SZrArtifactExecIrView; Write buffer bytes + writer.written; wrappers return bool, Core returns codec enum.
- Lifetime/rollback：Read view section.data borrows input buffer lifetime. Core Read zeroes nonnull out on failure, wrapper null-reader/null-module early return does not guarantee clearing. Core Write validates sizes/hashes before writes; parser failure after codec sets written0; null module/writer early return does not. Input/output buffers must be independent pending overlap policy.
- 实际定义：`ZrParser_ExecIr_WriteCanonicalZroFile @ zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:92`; `ZrCore_ArtifactExecIr_Read @ zr_vm_core/src/zr_vm_core/artifact/artifact_exec_ir.c:95`; `ZrCore_ArtifactExecIr_Write @ zr_vm_core/src/zr_vm_core/artifact/artifact_exec_ir.c:68`; `ZrParser_ExecIr_Read @ zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir.c:3`; `ZrParser_ExecIr_Write @ zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir.c:14`。
- 实际声明：`ZrParser_ExecIr_WriteCanonicalZroFile @ zr_vm_parser/src/zr_vm_parser/artifact/artifact_exec_ir.h:30`; `ZrCore_ArtifactExecIr_Read @ zr_vm_core/include/zr_vm_core/artifact_exec_ir.h:136`; `ZrCore_ArtifactExecIr_Write @ zr_vm_core/include/zr_vm_core/artifact_exec_ir.h:131`; `ZrParser_ExecIr_Read @ zr_vm_parser/src/zr_vm_parser/artifact/artifact_exec_ir.h:20`; `ZrParser_ExecIr_Write @ zr_vm_parser/src/zr_vm_parser/artifact/artifact_exec_ir.h:23`。
- Production reverse refs：`ZrCore_ArtifactExecIr_Read @ zr_vm_core/src/zr_vm_core/module/module_exec_ir_artifact.c:166`; `ZrCore_ArtifactExecIr_Read @ zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir.c:9`; `ZrCore_ArtifactExecIr_Write @ zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir.c:21`; `ZrCore_ArtifactExecIr_Write @ zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c:169`。
- 接入限制：Correct original I[5] classification; no generic new alias required. Private src header is not installed API. ERI1 envelope view is not reconstructed/verified ExecIR module.

### I07 ExecutionBackend_Register

- Owner：Core / execution backend service；状态域：`BACKEND`。
- 输入域：Host descriptor/vtable/userData and caller-owned service registration/job/code arrays.
- 输出域：Service typed status/registration/diagnostic; default facade bool discards registration and diagnostic.
- Lifetime/rollback：Descriptor copied by value; callbacks/userData remain borrowed. Service allocates monotone registration identity; default-service pointer lock protects pointer only, host preserves service lifetime. No worker spawned.
- 实际定义：`ZrCore_ExecutionBackendService_Register @ zr_vm_core/src/zr_vm_core/execution/execution_backend.c:210`; `ZrCore_ExecutionBackend_Register @ zr_vm_core/src/zr_vm_core/execution/execution_backend_facade.c:48`。
- 实际声明：`ZrCore_ExecutionBackendService_Register @ zr_vm_core/include/zr_vm_core/execution_backend.h:525`; `ZrCore_ExecutionBackend_Register @ zr_vm_core/include/zr_vm_core/execution_backend.h:681`。
- Production reverse refs：`ZrCore_ExecutionBackendService_Register @ zr_vm_core/src/zr_vm_core/execution/execution_backend_facade.c:54`。
- 接入限制：Real provider registration/host init wiring pending; no metadata mock can prove machine code provider support.

### I08 ExecutionBackend_CompileAsync

- Owner：Core / execution backend compile service；状态域：`BACKEND`。
- 输入域：Immutable borrowed ExecIR/AOTIR request payload and scalar contract/generation/target witness; request flags must promise immutable input.
- 输出域：Service PENDING ticket or explicit fallback/failure; facade TRUE for PENDING and fallback, not completed executable code.
- Lifetime/rollback：Copies request scalars/pointers to job; caller maintains pointed input until job/callback completion. Host drives ProcessNext, callback may return PENDING then Complete/Publish. Query callback outside lock, in-flight counts guard shutdown; existing QUEUED-before-query-return race TODO retained.
- 实际定义：`ZrCore_ExecutionBackendService_CompileAsync @ zr_vm_core/src/zr_vm_core/execution/execution_backend_compile.c:226`; `ZrCore_ExecutionBackend_CompileAsync @ zr_vm_core/src/zr_vm_core/execution/execution_backend_facade.c:59`。
- 实际声明：`ZrCore_ExecutionBackendService_CompileAsync @ zr_vm_core/include/zr_vm_core/execution_backend.h:541`; `ZrCore_ExecutionBackend_CompileAsync @ zr_vm_core/include/zr_vm_core/execution_backend.h:685`。
- Production reverse refs：`ZrCore_ExecutionBackendService_CompileAsync @ zr_vm_core/src/zr_vm_core/execution/execution_backend_facade.c:65`。
- 接入限制：Schema6/logical1/reserved0 owner gate; ABI matches selected registration (23 may be valid), not global17. Accepted queue does not imply compilation/publication/execution.

### I09 ExecutionBackend_InvalidateGeneration

- Owner：Core / execution backend lifetime；状态域：`BACKEND`。
- 输入域：Host runtime generation namespace: domainIdentity,moduleIdentity,generation,backendRegistrationIdentity all required.
- 输出域：Typed status / bool plus invalidated keys, cancellation/retirement of matching jobs/code.
- Lifetime/rollback：Invalidation namespace includes selected registration; marks/retire paths and active leases delay reclamation. Callback lifetimes remain host/provider responsibilities, not a disk record edit.
- 实际定义：`ZrCore_ExecutionBackendService_InvalidateGeneration @ zr_vm_core/src/zr_vm_core/execution/execution_backend.c:457`; `ZrCore_ExecutionBackend_InvalidateGeneration @ zr_vm_core/src/zr_vm_core/execution/execution_backend_facade.c:72`。
- 实际声明：`ZrCore_ExecutionBackendService_InvalidateGeneration @ zr_vm_core/include/zr_vm_core/execution_backend.h:585`; `ZrCore_ExecutionBackend_InvalidateGeneration @ zr_vm_core/include/zr_vm_core/execution_backend.h:690`。
- Production reverse refs：`ZrCore_ExecutionBackendService_InvalidateGeneration @ zr_vm_core/src/zr_vm_core/execution/execution_backend_facade.c:76`。
- 接入限制：No automatic HotPatch generation manager or module metadata generation -> backend invalidation bridge.

### I10 ExecutionBackend_ResumeInterpreter

- Owner：Core / execution backend resume adapter；状态域：`RESUME`。
- 输入域：Borrowed Core resume request and caller-owned diagnostic.
- 输出域：Bool / resume diagnostic, delegated to registered host resume callback.
- Lifetime/rollback：Snapshots callback/userData under service lock, increments callback in-flight, calls outside lock, releases counter on success/failure. Default pointer host-lifetime restriction applies; service shutdown check TODO still explicit.
- 实际定义：`ZrCore_ExecutionBackendService_ResumeInterpreter @ zr_vm_core/src/zr_vm_core/execution/execution_backend.c:770`; `ZrCore_ExecutionBackend_ResumeInterpreter @ zr_vm_core/src/zr_vm_core/execution/execution_backend_facade.c:81`。
- 实际声明：`ZrCore_ExecutionBackendService_ResumeInterpreter @ zr_vm_core/include/zr_vm_core/execution_backend.h:666`; `ZrCore_ExecutionBackend_ResumeInterpreter @ zr_vm_core/include/zr_vm_core/execution_backend.h:694`。
- Production reverse refs：`ZrCore_ExecutionBackendService_ResumeInterpreter @ zr_vm_core/src/zr_vm_core/execution/execution_backend_facade.c:85`。
- 接入限制：No CoreVM/MaterializeState execution or real interpreter resume provider credit; unsupported missing callback is not an invalid-IR finding.

### I11 OptimizationRemarks_Query

- Owner：Core / optimization remark storage/query；状态域：`REMARK`。
- 输入域：Borrowed validated public POD store and filter/page request; producer passes append scalar remark copies.
- 输出域：Allocated copied SZrOptimizationRemarkPage with deterministic sorted matches, total/dropped/truncated metadata.
- Lifetime/rollback：Validates masks/ranges/strings and every store row. Caller PageFree before reusing page; Query zeroes first-use page and does not free prior unknown pointer. Store and result ownership separate; no compilation at query time.
- 实际定义：`ZrCore_OptimizationRemarks_Query @ zr_vm_core/src/zr_vm_core/runtime/optimization_remark.c:435`。
- 实际声明：`ZrCore_OptimizationRemarks_Query @ zr_vm_core/include/zr_vm_core/optimization_remark.h:336`。
- Production reverse refs：`ZrCore_OptimizationRemarks_Query @ zr_vm_cli/src/zr_vm_cli/commands/explain_optimize_command.c:521`; `ZrCore_OptimizationRemarks_Query @ zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_optimization_remarks.c:279,289`。
- 接入限制：A query result is diagnostic evidence, not optimization success or runtime execution; Source41 tests credited only by separate actual reports.

### I12 HotPatch_Validate

- Owner：Core / hotpatch admission；状态域：`HOTPATCH`。
- 输入域：Borrowed artifact view or bounded ZRAF bytes, host manifest/requirements, base identity and signature verifier callback.
- 输出域：Scalar validation result plus borrowed artifact/manifest references; structured EZrHotPatchCapabilityStatus.
- Lifetime/rollback：Validates bounded format/content hash, ABI/profile/public identity, host capability subset, bans machine code/native imports, invokes host signature policy. Borrowed bytes/policy must remain valid and stable through validation; full ZRAF variant snapshots scalar policies.
- 实际定义：`ZrCore_HotPatch_Validate @ zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_validate.c:74`; `ZrCore_HotPatch_ValidateZraf @ zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_validate.c:133`。
- 实际声明：`ZrCore_HotPatch_Validate @ zr_vm_core/include/zr_vm_core/capability_manifest.h:235`; `ZrCore_HotPatch_ValidateZraf @ zr_vm_core/include/zr_vm_core/capability_manifest.h:248`。
- Production reverse refs：`未找到精确direct syntactic caller`。
- 接入限制：Admission relies on host-supplied policy/signature callback; effect bits are not security authority. Exact host owner integration pending.

### I13 HotPatch_Apply

- Owner：Core / hotpatch version entry publication；状态域：`HOTPATCH`。
- 输入域：Validated scalar witness, stable captured artifact bytes, host registry and generation manager.
- 输出域：Apply handle/status for metadata generation publication; no machine-code patching implied.
- Lifetime/rollback：Rehash captured content, require immutable verified artifact, registry collision/capacity check, prepare/publish generation and registry entry. Caller serialization still required for registry; result borrows host-owned registry/manager storage.
- 实际定义：`ZrCore_HotPatch_ApplyValidated @ zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_rollback.c:14`。
- 实际声明：`ZrCore_HotPatch_ApplyValidated @ zr_vm_core/include/zr_vm_core/hotpatch_rollback.h:50`。
- Production reverse refs：`未找到精确direct syntactic caller`。
- 接入限制：Freeze documentation mapping Apply -> ApplyValidated, not invented alias or automatic loader. Validation output public POD is not cryptographic unforgeable token.

### I14 HotPatch_Rollback

- Owner：Core / hotpatch generation manager；状态域：`HOTPATCH`。
- 输入域：Existing retained generation ID and caller-owned manager/record slots.
- 输出域：New generation identity selecting previous scalar artifact contract; structured rollback status.
- Lifetime/rollback：Rollback prepares fresh monotone generation copying retained target identity, then publishes it; old leases remain bound to old records. It does not rewind counter, mutate held pointers or automatically invalidate backend entries.
- 实际定义：`ZrCore_HotPatch_Rollback @ zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_rollback.c:72`。
- 实际声明：`ZrCore_HotPatch_Rollback @ zr_vm_core/include/zr_vm_core/hotpatch_rollback.h:61`。
- Production reverse refs：`未找到精确direct syntactic caller`。
- 接入限制：Version entry cell / module dispatch / backend invalidation integration remains an explicit downstream task.

### I15 Domain_GetContract

- Owner：Core / GC-domain contract (proposed accountable owner)；状态域：`DOMAIN`。
- 输入域：Planned: borrowed state/domain identity; actual GetIdentity(state).
- 输出域：Actual SZrGcDomainIdentity{id,generation}, zero pair when missing/inactive; NOT full domain capability/allocator/clone contract.
- Lifetime/rollback：Identity is copied scalar process witness; state/domain lifetime remains host-owned and identity-current check required. No new public GetContract declaration found.
- 实际定义：`ZrCore_GcDomain_GetIdentity @ zr_vm_core/src/zr_vm_core/gc/gc_domain.c:274`。
- 实际声明：`ZrCore_GcDomain_GetIdentity @ zr_vm_core/include/zr_vm_core/gc_domain.h:95`。
- Production reverse refs：`ZrCore_GcDomain_GetIdentity @ zr_vm_core/src/zr_vm_core/gc/gc_domain_clone.c:92,93,161,162`; `ZrCore_GcDomain_GetIdentity @ zr_vm_core/src/zr_vm_core/gc/gc_domain_share.c:18,19,79`; `ZrCore_GcDomain_GetIdentity @ zr_vm_core/src/zr_vm_core/ownership/ownership_transfer.c:293,326,897`; `ZrCore_GcDomain_GetIdentity @ zr_vm_core/src/zr_vm_core/ownership/ownership_transfer_value_copy.c:46`; `ZrCore_GcDomain_GetIdentity @ zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:627`; `ZrCore_GcDomain_GetIdentity @ zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_isolated_domain.c:369,822`。
- 接入限制：D1: freeze planned full contract shape, borrowed identity fields vs policy capabilities, error representation and snapshot consistency; retain exact draft as planned until API exists.

### I16 Domain_ShareValue

- Owner：Core / GC-domain roots/sharing；状态域：`DOMAIN`。
- 输入域：Producer and consumer states, object, sharedReference and trusted proof flags, empty output root handle.
- 输出域：Retained consumer GC root handle or structured share diagnostic.
- Lifetime/rollback：Only same active gcDomain pointer; object must belong to source; sharedReference requires syncProof, else sendProof; dropping/dropped rejected. Does not transfer ownership. Caller Release handle; resolve pointer borrowed and may move with GC. Output cleared so preexisting owned handle forbidden.
- 实际定义：`ZrCore_GcDomain_ShareValue @ zr_vm_core/src/zr_vm_core/gc/gc_domain_share.c:24`。
- 实际声明：`ZrCore_GcDomain_ShareValue @ zr_vm_core/include/zr_vm_core/gc_domain.h:236`。
- Production reverse refs：`未找到精确direct syntactic caller`。
- 接入限制：Proof flags are caller assertion, not generated/validated proof authority; host producer/caller remains missing.

### I17 Domain_TransferValue

- Owner：Core / cross-domain ownership transfer (proposed accountable owner)；状态域：`DOMAIN`。
- 输入域：Planned generic transfer: source/target active distinct domains, object graph + quotas/schema + transaction/worker identity.
- 输出域：Existing clone transaction / cloned target value; separate ownership graph protocol; no generic Domain_TransferValue entry.
- Lifetime/rollback：Clone Prepare encodes source graph; Publish/Claim/Commit; Abort/Free on failure; sourceState must outlive envelope free. Distinct target graph, no GC/control pointer sharing. Graph API is used by ownership_transfer.c:394; clone Execute/Prepare external clients presently tests.
- 实际定义：`ZrCore_GcDomainClone_Prepare @ zr_vm_core/src/zr_vm_core/gc/gc_domain_clone.c:111`; `ZrCore_DomainTransferGraph_Prepare @ zr_vm_core/src/zr_vm_core/ownership/ownership_transfer_cross_domain.c:352`; `ZrCore_GcDomainClone_Execute @ zr_vm_core/src/zr_vm_core/gc/gc_domain_clone.c:447`。
- 实际声明：`ZrCore_GcDomainClone_Prepare @ zr_vm_core/include/zr_vm_core/gc_domain_clone.h:33`; `ZrCore_DomainTransferGraph_Prepare @ zr_vm_core/src/zr_vm_core/ownership/ownership_transfer_cross_domain_internal.h:13`; `ZrCore_GcDomainClone_Execute @ zr_vm_core/include/zr_vm_core/gc_domain_clone.h:95`。
- Production reverse refs：`ZrCore_GcDomainClone_Prepare @ zr_vm_core/src/zr_vm_core/gc/gc_domain_clone.c:460`; `ZrCore_DomainTransferGraph_Prepare @ zr_vm_core/src/zr_vm_core/ownership/ownership_transfer.c:394`。
- 接入限制：D2: select clone vs unique/resource move semantics explicitly; bind future draft owner to protocol, quotas, failure/Throw cleanup and worker claim epoch. Existing clone source-materialization/OOM/Free-conflict/schema-fingerprint TODOs forbid blanket atomic-clone claim.

### I18 Gc_SetBudget

- Owner：Core / garbage collector budget；状态域：`GC`。
- 输入域：Borrowed state/global collector and validated SZrGcBudget.
- 输出域：Bool; collector configuration and reset budget telemetry.
- Lifetime/rollback：Validate before domain mutation lock, copies config under lock, no allocation. Domain-null lock no-op; host serializes unsupported concurrent paths.
- 实际定义：`ZrCore_Gc_SetBudget @ zr_vm_core/src/zr_vm_core/gc/gc_budget_runtime.c:179`。
- 实际声明：`ZrCore_Gc_SetBudget @ zr_vm_core/include/zr_vm_core/gc.h:409`。
- Production reverse refs：`未找到精确direct syntactic caller`。
- 接入限制：Budget caps/cooperative accounting do not promise hard realtime pauses. Preserve actual cursor OVER_BUDGET compatibility TODO.

### I19 Gc_GetStats

- Owner：Core / garbage collector budget telemetry；状态域：`GC`。
- 输入域：Borrowed state + caller output SZrGcBudgetStats (typedef SZrGcBudgetStepResult).
- 输出域：Bool + cumulative budget result copy, sampled debt/pressure.
- Lifetime/rollback：Caller serializes with collector mutation/evaluation. Requires configured budget. maxAtomicPauseUs is configured threshold, bytesDone/objectsDone are0, nextCursor is stored accepted cursor; not measured max pause/general heap census.
- 实际定义：`ZrCore_Gc_GetStats @ zr_vm_core/src/zr_vm_core/gc/gc_budget_runtime.c:186`。
- 实际声明：`ZrCore_Gc_GetStats @ zr_vm_core/include/zr_vm_core/gc.h:412`。
- Production reverse refs：`未找到精确direct syntactic caller`。
- 接入限制：D3: freeze this actual budget-specific mapping in dictionary; if draft intended general heap stats, plan separate shape/API instead of changing meaning silently.


### 本次名字/层次纠正

ReadWrite 原 I[5] 的 name_mapping_open 已被真实源码解决：Parser 私有 artifact_exec_ir.h:20/23 声明，writer_exec_ir.c:3/14 定义 Read/Write，委托 Core ERI1 codec；不新增API、不新增操作。CanonicalZroFile 是独立文件写出层，不替换内存wrapper。Core Read 的 module reader consumer 在 module_exec_ir_artifact.c:166；精确 Parser Read/Write 只有 schema_relocation 测试 caller。

HotPatch_Apply 是草案映射到真实 ApplyValidated，不存在同名 C alias；ShareValue 有真实 object-like macro alias；GetContract、通用 TransferValue 仍是 planned，不把 GetIdentity 或 graph helper 改名冒充。

BuildModule 在 compiler_source_retention.c:248 有实际client，canonical LowerAot variant在同文件:330；LowerShared 在 backend_aot_ir_adapter.c:240 有真实client，不能与精确 LowerAot 层合并。四默认 backend facade 当前只下派 service，缺少外部production client/default service安装；service测试只是metadata mock。

## 七类结果含义

“invalid IR”是静态结构/SSA/effect等不合法；“link error”是解析/绑定失败；capability拒绝是host准入；deopt是有效运行状态的恢复；unsupported是合法但当前实现不支持。不得把这些混成 bool FALSE 或把失效/取消当 speculation fallback。具体API已有bool时由其现有diagnostic区分，不改publicABI来满足文档。

### IR

| 类别 | 冻结含义 |
| --- | --- |
| `error` | FALSE + exact diagnostic; invalid arguments/OOM/capacity are not invalid IR semantics. |
| `pending` | No async pending return. |
| `invalidIR` | Verifier/build structural, SSA, effects, exception/state-map rejection; never silently deopt malformed IR. |
| `link` | Token/name/binding or static-link facts belong Parser/module resolution; Verify success alone does not link. No universal LINK_ERROR enum invented. |
| `capability` | Declared effects/capabilities are semantic metadata, not host privilege grant. |
| `deopt` | State-map validation/construction is metadata; no running-frame materialization here. |
| `unsupported` | Represent unsupported semantic form distinctly from malformed graph when diagnostic provides it; no automatic dynamic fallback implied. |

### OPTIMIZE

| 类别 | 冻结含义 |
| --- | --- |
| `error` | FALSE transactional public failure; result reset and original module retained. |
| `pending` | No async status; budgetExhausted can accompany TRUE verified partial publication. |
| `invalidIR` | Input or complete candidate Verify failure => FALSE, not speculation miss. |
| `link` | No resolution of arbitrary external names/imports; preserved contract is not link proof. |
| `capability` | No privilege escalation permitted by pass result. |
| `deopt` | Metadata preservation/verification only; no runtime deopt execution. |
| `unsupported` | Disabled/unselected or conservative pass does not mean invalid IR; only selected two scalar passes credited. |

### LOWER

| 类别 | 冻结含义 |
| --- | --- |
| `error` | FALSE + exact projection diagnostic; output owner follows specific transactional function. |
| `pending` | No async pending. |
| `invalidIR` | Projection rejection and invalid function must be distinguished from valid unsupported opcode/ABI. |
| `link` | Address fixups/import resolution are downstream; projection and LowerShared are not completed link. |
| `capability` | Callable effect/target contract matching is metadata equality, not host authorization. |
| `deopt` | Preserved recovery maps are metadata; machine code/entrypoints not produced or executed by this audit. |
| `unsupported` | Valid unsupported operation/callable ABI returns unsupported/invalid projection as actual branch specifies; fallback requires host caller policy. |

### CODEC

| 类别 | 冻结含义 |
| --- | --- |
| `error` | Wrapper bool loses Core codec enum; Core has magic/version/truncation/directory/limit/hash diagnostics. |
| `pending` | No pending status. |
| `invalidIR` | Bounded ERI1 view validation != semantic VerifyModule; module scalar admission is separate. |
| `link` | Bytes and directory tokens are not resolved runtime addresses. |
| `capability` | Codec hash/version checks do not grant capability or verify a host signature. |
| `deopt` | Wire recovery metadata is not live runtime state; no materialization. |
| `unsupported` | Unsupported wire version is rejection, not backend-target fallback. |

### BACKEND

| 类别 | 冻结含义 |
| --- | --- |
| `error` | Service typed status with diagnostics; default bool facades intentionally discard most details. Preserve actual validation/flags/immutable and Complete generation-first ordering. |
| `pending` | PENDING means queued/backend-pending normal nonblocking result. ASYNC descriptor flag starts no thread; ProcessNext host-driven. Facade TRUE also includes FALLBACK, so TRUE != completed code. |
| `invalidIR` | Service immutable hash and contract admission do not perform full Core VerifyModule; caller owes valid IR. |
| `link` | Map registration hashes/flags and imports witness are metadata; provider owns actual linking/registration and executable memory. |
| `capability` | Required operation bits subset provider support; contract capabilityHash equality separate from security allowlist. |
| `deopt` | Resume callback and DEOPT map declaration are distinct; no automatic runtime state recovery. |
| `unsupported` | Target/operation/provider unavailable can produce explicit permitted ExecBC/AOT fallback; REQUIRE_MACHINE_CODE forbids fallback; compile-failed/stale/cancelled remain distinct. |

### RESUME

| 类别 | 冻结含义 |
| --- | --- |
| `error` | FALSE with INVALID_ARGUMENT, overflow, UNSUPPORTED when callback absent; callback failure without diagnostic becomes MATERIALIZATION_FAILED. |
| `pending` | No async result from Resume bool. |
| `invalidIR` | Not a replacement for Verify; caller supplies valid resume request. |
| `link` | No linker or native entrypoint resolution here. |
| `capability` | Registered host callback is trusted runtime integration, not new authority. |
| `deopt` | Actual callback semantics host-owned; test callback only diagnostic source77; no execution credit. |
| `unsupported` | Missing callback unsupported; do not classify as malformed IR or claim fallback occurred. |

### REMARK

| 类别 | 冻结含义 |
| --- | --- |
| `error` | FALSE + remark diagnostic on invalid store/query/OOM; no compiler execution. |
| `pending` | No pending; page is synchronous copied data. |
| `invalidIR` | Rows may describe invalid IR but query failure means bad query/store, not proof compiler IR invalid. |
| `link` | Rows are diagnostic metadata only. |
| `capability` | Filtering does not authorize effects or capabilities. |
| `deopt` | A deopt remark is evidence description only, not recovery. |
| `unsupported` | Remark reason/status/backend filters preserve distinct unsupported/missed/error causes; querying them does not execute fallback. |

### HOTPATCH

| 类别 | 冻结含义 |
| --- | --- |
| `error` | Typed validation/rollback errors; retain ABI/profile/base/public-contract/signature/content/limit categories rather than generic bool. |
| `pending` | No backend-style PENDING enum; generation prepare/publish states are metadata transitions. |
| `invalidIR` | ARTIFACT_INVALID may denote encoding/admission failure; full semantic IR verification is not automatically every validation entry. |
| `link` | New native imports and machine-code payloads are forbidden by current validator; Apply metadata publication is not dynamic code link. |
| `capability` | Host-supplied signature verifier and allowedBits enforce declared subset; unknown/required flags policy from actual branch; effect summaries are not authorization. |
| `deopt` | Old generation leases keep old records; rollback publishes new scalar identity, no automatic running-frame migration. |
| `unsupported` | Machine-code/import/public-layout changes explicitly rejected; not silently downgraded to runtime fallback. |

### DOMAIN

| 类别 | 冻结含义 |
| --- | --- |
| `error` | Actual identity query zero-pair or typed share/clone diagnostics; future GetContract/TransferValue return types not declared and must not be invented as existing ABI. |
| `pending` | Clone transaction has published/claimed/committing states; not backend PENDING; GetIdentity/share synchronous. |
| `invalidIR` | Domain/object/proof/quota checks are not ExecIR validation. |
| `link` | No native import/name linking. |
| `capability` | Same-domain share trusts caller proof flags; cross-domain requires copied graph/control separation; no evidence of generated proof authority. |
| `deopt` | Object graph clone/transfer is not optimized-frame materialization or hotupdate state migration. |
| `unsupported` | Unsupported graph/resource/type/schema behavior must retain actual transaction diagnostics and cleanup; generic future API remains planned. |

### GC

| 类别 | 冻结含义 |
| --- | --- |
| `error` | FALSE on missing state/collector/configuration or invalid budget; no typed wrapper diagnostic. |
| `pending` | No async pending in these wrappers; collector cooperative progress separate. |
| `invalidIR` | Not an IR API. |
| `link` | Not a linker API. |
| `capability` | Budget config conveys resource limits, not permission to execute effects. |
| `deopt` | Not a deopt API. |
| `unsupported` | No universal unsupported status; scalar budget-step rejected/over-budget distinct from wrapper FALSE; statistics alias has limited field meanings. |


## 必需下游接口与验收 owner

| planned接口/接入 | 唯一owner / 叶 | 验收责任 |
| --- | --- | --- |
| Domain_GetContract | Core GC-domain /06.03 | 一致只读快照、失效identity、证明类别、注销/并发配置；不签发proof |
| Domain_TransferValue facade | Core ownership /06.04 | 独立目标图、quota/schema、claim epoch、Throw/OOM、abort/commit、释放重试 |
| loadEpoch/version entry bridge | Core module loader /08.03 | 非零不复用、overflow拒绝、同内容重载、旧leases与晚ticket；10.01接收命名空间 |
| backend provider/默认service caller | Core backend host adapter /10.01 | 注册/销毁顺序、immutable input lifetime、callback计数、worker与shutdown竞争 |
| 真resume provider | Core runtime recovery /01.04，service adapter/10.01 | state-map验证、合法deopt路径、失败清理，不能由source77mock计完成 |
| HotPatch host admission/publication | Core loader host adapter /08.02–08.04 | signature/allowlist输入可信、content稳定、entry publish/rollback与旧lease |
| 一般堆stats若需要 | Core GC /06.02 | 使用现有GetStatsSnapshot或另计划明确API；不得改Gc_GetStats预算含义 |
| opaque内层codec | 所属Parser/artifact owner /08.01与对应IR叶 | 按新schema补producer/reader/admission，不靠byte-window声称完整 |

接口owner与数据/版本政策在本四页固定；这些planned实现验收不会被文字交付自动勾选。本次未运行loader、patch、capability、resume、VM或网络。当前后端33/33+service7/CTest2已由221A4D88独审接受，MSVC6C syntax由394EB90F接受；限定metadata mock与syntax，本矩阵不扩展其运行范围。

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
