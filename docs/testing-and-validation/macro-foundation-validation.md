---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time/compile_time_aggregate.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time/compile_time_aggregate.h
  - tests/compileTime/test_comptime_frame_transaction_cases.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time/compile_time_frame.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time/compile_time_executor.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time/compile_time_executor_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time/compile_time_executor_internal.h
  - tests/compileTime/test_comptime_frame_budget_cases.h
  - tests/parser/test_type_inference.c
  - tests/parser/test_type_inference_extern_visibility_cases.h
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_type_prototypes.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/analyzer/semantic_analyzer_symbols.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_generic_prototype.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_member_facts.c
  - tests/language_server/test_semantic_analyzer_prototype_identity_cases.h
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_class_callables.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_struct.c
  - tests/parser/test_token_source_meta_name_range_cases.h
  - tests/language_server/test_lsp_super_utf16_ranges.c
  - zr_vm_core/src/zr_vm_core/type/type_layout_initialization.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_generic_substitution.c
  - zr_vm_parser/src/zr_vm_parser/compiler/metadata/type_storage_layout.c
  - tests/parser/test_reference_receiver_layout_contract_support.h
  - tests/parser/test_compiler_storage_alignment_contract_cases.h
  - tests/parser/test_compiler_features.c
  - tests/parser/test_union.c
  - tests/core/test_type_layout_inline_copy.c
  - tests/CMakeLists.txt
  - tests/cmake/macro-foundation-tests.cmake
  - tests/cmake/macro-foundation-regressions.cmake
  - tests/cmake/run_executable_suite.cmake
  - tests/parser/test_token_source.c
  - tests/parser/test_token_source_mixed_postfix_cases.h
  - tests/parser/test_token_source_projection_cases.h
  - tests/parser/test_token_source_composite_range_cases.h
  - tests/parser/test_token_source_range_evidence_cases.h
  - tests/parser/test_token_source_diagnostic_evidence_cases.h
  - tests/parser/test_token_source_semantic_range_cases.h
  - tests/parser/test_token_source_construction_range_cases.h
  - tests/parser/test_semantic_loop_range_evidence_cases.h
  - tests/parser/test_semantic_query_symbols.c
  - tests/parser/test_aot_c_guardrail_contracts.c
  - tests/parser/test_parser_array_budget.c
  - tests/parser/test_parser_token_ownership.c
  - tests/parser/test_parser_recovery_ownership.c
  - tests/parser/test_parser_nesting_limit.c
  - tests/parser/test_flat_expression_compile.c
  - tests/parser/test_reference_fact_emission.c
  - tests/parser/test_reference_function_identity_fact_cases.h
  - tests/language_server/test_lsp_local_semantic_query.c
  - tests/language_server/test_lsp_local_semantic_scope_cases.h
  - tests/language_server/test_lsp_inlay_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_source_projection.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_source_projection.h
  - zr_vm_parser/include/zr_vm_parser/location.h
  - zr_vm_parser/src/zr_vm_parser/syntax/location.c
  - zr_vm_parser/src/zr_vm_parser/syntax/source_identity.c
  - zr_vm_parser/src/zr_vm_parser/syntax/source_identity.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_range_evidence.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_range_evidence.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_ownership.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_ownership.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_ownership_internal.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_ownership_strings.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_ownership_strings.h
  - zr_vm_parser/include/zr_vm_parser/comptime_contract.h
  - zr_vm_parser/src/zr_vm_parser/contract/comptime_contract.c
  - zr_vm_core/include/zr_vm_core/gc_domain.h
  - zr_vm_core/src/zr_vm_core/gc/gc_domain.c
  - zr_vm_parser/src/zr_vm_parser/syntax/token_source.c
  - zr_vm_parser/src/zr_vm_parser/syntax/token_source.h
  - zr_vm_parser/src/zr_vm_parser/syntax/lexer_cursor.c
  - tests/parser/test_token_source_edges.c
  - tests/parser/test_token_stream_limits.c
  - zr_vm_parser/src/zr_vm_parser/syntax/token_stream_clone.c
  - tests/compileTime/test_syntax_expansion_contract.c
  - tests/compileTime/test_comptime_contract.c
  - tests/compileTime/test_comptime_runtime_contract.c
  - tests/compileTime/test_compile_time_execution.c
  - tests/compileTime/test_comptime_frame_lifetime.c
  - tests/compileTime/test_comptime_aggregate_budget.c
  - tests/compileTime/test_comptime_definition_environment.c
  - tests/compileTime/test_comptime_import_alias_environment.c
  - tests/compileTime/test_comptime_import_alias_owner_cases.h
  - tests/exceptions/test_exceptions.c
  - tests/exceptions/test_named_finally_completion_cases.h
  - tests/parser/test_ssa_source_cleanup_cfg.c
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_pre_semantic_ir_source_cfg.inc
  - tests/parser/test_ownership_receiver_guard_contract_cases.h
  - tests/parser/test_cfg_finally_abrupt.c
  - tests/parser/test_resource_shared_weak.c
  - tests/parser/test_lambda_semantic_ir_isolation.c
  - tests/parser/test_lambda_local_range_cases.h
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_semantic_analyzer_generic_signature_range_cases.h
  - tests/language_server/test_semantic_analyzer_lsp_native_range_consumer_cases.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/features/lsp_signature_help.c
  - zr_vm_language_server/src/zr_vm_language_server/features/lsp_super_navigation.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_diagnostic_evidence.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_diagnostic_evidence.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_type_display_alias.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_display.c
  - zr_vm_parser/include/zr_vm_parser/semantic_display.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_loops.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_query_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/analyzer/semantic_analyzer_support.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_type_use.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_prototype_lookup.c
  - tests/language_server/test_semantic_analyzer_hygiene_binding_cases.h
  - tests/language_server/test_semantic_declaration_budget.c
  - tests/language_server/test_semantic_environment_budget_cases.h
  - tests/cmake/zr_vm_lsp_declaration_budget_tests.cmake
  - tests/cmake/lsp-component-targets.cmake
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_declaration_binding.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_context_lookup.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_variable_lookup.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_bindings.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_callable_lookup.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_callable_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_call_binding.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/analyzer/semantic_analyzer_declaration_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_typed_metadata.c
  - tests/parser/test_compiler_regressions.c
  - tests/parser/test_project_failure_diagnostic.inc
  - tests/parser/test_typed_call_binding.c
  - tests/parser/test_typed_call_binding_imported_returns.inc
  - tests/parser/test_typed_call_binding_cyclic_returns.inc
  - tests/parser/test_typed_call_binding_member_semantics.inc
  - tests/library/test_call_binding_module.c
  - tests/library/test_call_binding_module_imported_meta.inc
  - tests/library/test_call_binding_module_imported_instance.inc
  - tests/iterator/test_yield_semantic.c
  - tests/iterator/yield_public_compile_graph_cases.inc
  - tests/parser/test_escape_metadata_pipeline.c
  - tests/parser/test_escape_metadata_pipeline_main.c
  - tests/parser/test_escape_native_member_cases.inc
  - tests/parser/test_reference_receiver_call_boundary.c
  - tests/parser/test_reference_receiver_frame_contract_cases.h
  - tests/parser/test_semantic_query.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_messages.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time/compile_time_callable.c
  - tests/parser/test_parser_metadata_budget.c
  - tests/parser/test_parser_primary_budget.c
  - tests/parser/test_syntax_hygiene_aliases.c
  - tests/parser/test_syntax_hygiene_import_aliases.c
  - tests/parser/test_syntax_hygiene_alias_scope.c
  - tests/parser/test_syntax_hygiene_nominal_types.c
  - tests/parser/test_syntax_hygiene_nominal_prototypes.c
  - tests/parser/test_syntax_hygiene_generic_prototypes.c
  - tests/parser/test_syntax_hygiene_generic_definitions.c
  - tests/parser/test_syntax_hygiene_compiler_prototype_lookup.c
  - tests/parser/test_syntax_hygiene_receiver_members.c
  - tests/parser/test_syntax_hygiene_inherited_members.c
  - tests/parser/test_syntax_hygiene_field_results.c
  - tests/parser/test_function_type_conversion_lifetime.c
  - tests/parser/test_unpublished_type_metadata_lifetime.c
  - tests/parser/test_open_array_annotation.c
  - tests/parser/test_context_identity_queries.c
  - tests/parser/test_nominal_declaration_publication.c
  - zr_vm_parser/src/zr_vm_parser/syntax/syntax_context_internal.h
  - zr_vm_parser/src/zr_vm_parser/syntax/syntax_context_hygiene.c
  - zr_vm_parser/src/zr_vm_parser/syntax/syntax_context_import.c
  - zr_vm_parser/src/zr_vm_parser/syntax/syntax_context_identity.c
  - tests/parser/test_callable_return_inference.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_bindings.c
  - zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_callable_return_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_value_alias.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_value_alias.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_import_type_alias.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_import_type_alias.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_scope.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_declaration.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_declaration.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_top_level_duplicate.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_types.c
  - zr_vm_parser/src/zr_vm_parser/semantic/type_declaration_context.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_type_publication.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_type_publication.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_context_ownership.h
  - zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_union.c
  - zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_union_canonical.c
  - zr_vm_parser/src/zr_vm_parser/compiler/extern/compiler_extern_declaration.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_state.c
  - zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_struct.c
  - zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_prototype_lifecycle.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_prototype_lifecycle.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_field.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_field.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_function_type.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_function_type.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_internal.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_type_visibility.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_nominal_identity.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_nominal_prototype.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_nominal_prototype.h
  - zr_vm_parser/src/zr_vm_parser/types/canonical_type_adapter.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic.c
  - zr_vm_parser/src/zr_vm_parser/types/type_system.c
  - zr_vm_parser/include/zr_vm_parser/type_system.h
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_cfg_finally.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_cfg_try.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_primary_members.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_construction.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_blocks.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_types.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_control_flow_expression.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_control_flow_expression.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_identifier_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_semantic_facts.h
  - zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression.c
  - zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression_binary.c
  - zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression_binary.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_switch.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_intermediate.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_intermediate_generated_source_map.c
  - zr_vm_cli/src/zr_vm_cli/command/command.c
  - zr_vm_language_server/include/zr_vm_language_server/lsp_interface.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time/compile_time_control_flow.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time/compile_time_control_flow.h
  - zr_vm_parser/src/zr_vm_parser/compiler/statement/compile_statement_flow.c
  - tests/compileTime/test_comptime_control_flow.c
  - tests/compileTime/test_comptime_control_flow_edges.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time/compile_time_aggregate.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time/compile_time_aggregate.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time/compile_time_frame.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time/compile_time_executor_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time/compile_time_executor_internal.h
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_type_prototypes.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/analyzer/semantic_analyzer_symbols.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_generic_prototype.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_member_facts.c
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_class_callables.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_struct.c
  - zr_vm_core/src/zr_vm_core/type/type_layout_initialization.c
  - zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_struct.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_generic_substitution.c
  - zr_vm_parser/src/zr_vm_parser/compiler/metadata/type_storage_layout.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/features/lsp_signature_help.c
  - zr_vm_language_server/src/zr_vm_language_server/features/lsp_super_navigation.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_construction.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_blocks.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_types.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_control_flow_expression.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_control_flow_expression.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_identifier_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_semantic_facts.h
  - zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression.c
  - zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression_binary.c
  - zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression_binary.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_diagnostic_evidence.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_diagnostic_evidence.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_type_display_alias.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_display.c
  - zr_vm_parser/include/zr_vm_parser/semantic_display.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_loops.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_query_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_state.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/analyzer/semantic_analyzer_support.c
  - zr_vm_parser/include/zr_vm_parser/comptime_contract.h
  - zr_vm_parser/src/zr_vm_parser/contract/comptime_contract.c
  - zr_vm_core/include/zr_vm_core/gc_domain.h
  - zr_vm_core/src/zr_vm_core/gc/gc_domain.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_type_use.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_prototype_lookup.c
  - zr_vm_parser/include/zr_vm_parser/location.h
  - zr_vm_parser/src/zr_vm_parser/syntax/location.c
  - zr_vm_parser/src/zr_vm_parser/syntax/source_identity.c
  - zr_vm_parser/src/zr_vm_parser/syntax/source_identity.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_range_evidence.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_range_evidence.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_source_projection.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_source_projection.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_ownership.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_ownership_internal.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_ownership_strings.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_ownership_strings.h
  - zr_vm_parser/src/zr_vm_parser/syntax/token_source.c
  - zr_vm_parser/src/zr_vm_parser/syntax/token_source.h
  - zr_vm_parser/src/zr_vm_parser/syntax/lexer_cursor.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_context_lookup.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_variable_lookup.c
  - zr_vm_parser/src/zr_vm_parser/syntax/token_stream_clone.c
  - zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_child_function_relocation.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_child_function_relocation.h
  - zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_function_assembly.c
  - zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/tool/compile_tool_execution_scope.c
  - zr_vm_parser/src/zr_vm_parser/compiler/tool/compile_tool_execution_scope.h
  - zr_vm_parser/src/zr_vm_parser/compiler/tool/compile_tool_evaluator.c
  - zr_vm_parser/src/zr_vm_parser/compiler/statement/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/statement/compile_statement_flow.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_cfg.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_cfg_finally.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_prototype_lifecycle.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_prototype_lifecycle.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_function_type.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_function_type.h
  - zr_vm_parser/src/zr_vm_parser/semantic/type_declaration_context.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_types.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_type_publication.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_type_publication.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_context_ownership.h
  - zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_union.c
  - zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_union_canonical.c
  - zr_vm_parser/src/zr_vm_parser/compiler/extern/compiler_extern_declaration.c
  - zr_vm_parser/src/zr_vm_parser/syntax/syntax_context_identity.c
  - zr_vm_parser/src/zr_vm_parser/syntax/syntax_context_hygiene.c
  - zr_vm_parser/src/zr_vm_parser/syntax/syntax_context_import.c
  - zr_vm_parser/src/zr_vm_parser/syntax/syntax_context_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_declaration.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_top_level_duplicate.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_nominal_prototype.c
  - zr_vm_parser/src/zr_vm_parser/types/canonical_type_adapter.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic.c
  - zr_vm_parser/src/zr_vm_parser/types/type_system.c
  - zr_vm_parser/include/zr_vm_parser/type_system.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_type_visibility.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_nominal_identity.c
  - zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_callable_return_inference.c
  - zr_vm_parser/src/zr_vm_parser/syntax/lexer.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time/compile_time_executor.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time/compile_time_control_flow.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time/compile_time_control_flow.h
  - zr_vm_language_server/src/zr_vm_language_server/incremental/incremental_parser.c
plan_sources:
  - docs/plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md
  - .codex/plans/20261008-macro-tokenstream-structured-generation.md
tests:
  - tests/compileTime/test_comptime_aggregate_budget.c
  - tests/compileTime/test_comptime_frame_transaction_cases.h
  - tests/compileTime/test_comptime_frame_budget_cases.h
  - tests/parser/test_type_inference.c
  - tests/parser/test_type_inference_extern_visibility_cases.h
  - tests/language_server/test_semantic_analyzer_prototype_identity_cases.h
  - tests/parser/test_token_source_meta_name_range_cases.h
  - tests/language_server/test_lsp_super_utf16_ranges.c
  - tests/parser/test_compiler_storage_alignment_contract_cases.h
  - tests/parser/test_reference_receiver_layout_contract_support.h
  - tests/parser/test_union.c
  - tests/core/test_type_layout_inline_copy.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_semantic_analyzer_generic_signature_range_cases.h
  - tests/language_server/test_semantic_analyzer_lsp_native_range_consumer_cases.h
  - tests/language_server/test_semantic_analyzer_declaration_context_cases.h
  - tests/language_server/test_semantic_declaration_budget.c
  - tests/language_server/test_semantic_environment_budget_cases.h
  - tests/parser/test_lambda_semantic_ir_isolation.c
  - tests/parser/test_pre_semantic_ir_source_cfg.inc
  - tests/parser/test_ownership_receiver_guard_contract_cases.h
  - tests/compileTime/test_comptime_import_alias_owner_cases.h
  - tests/exceptions/test_exceptions.c
  - tests/exceptions/test_named_finally_completion_cases.h
  - tests/parser/test_ssa_source_cleanup_cfg.c
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_cfg_finally_abrupt.c
  - tests/parser/test_resource_shared_weak.c
  - tests/compileTime/test_comptime_import_alias_environment.c
  - tests/parser/test_syntax_hygiene_base_publication.c
  - tests/parser/test_compiler_features.c
  - tests/module/test_module_system.c
  - tests/parser/test_open_array_annotation.c
  - tests/parser/test_unpublished_type_metadata_lifetime.c
  - tests/parser/test_function_type_conversion_lifetime.c
  - tests/parser/test_syntax_hygiene_compiler_prototype_lookup.c
  - tests/parser/test_syntax_hygiene_receiver_members.c
  - tests/parser/test_syntax_hygiene_inherited_members.c
  - tests/parser/test_syntax_hygiene_field_results.c
  - tests/parser/test_syntax_hygiene_generic_prototypes.c
  - tests/parser/test_syntax_hygiene_generic_definitions.c
  - tests/parser/test_nominal_declaration_publication.c
  - tests/parser/test_context_identity_queries.c
  - tests/parser/test_syntax_hygiene_nominal_prototypes.c
  - tests/parser/test_callable_return_inference.c
  - tests/parser/test_token_source.c
  - tests/parser/test_token_source_mixed_postfix_cases.h
  - tests/parser/test_token_source_projection_cases.h
  - tests/parser/test_token_source_composite_range_cases.h
  - tests/parser/test_token_source_range_evidence_cases.h
  - tests/parser/test_token_source_diagnostic_evidence_cases.h
  - tests/parser/test_token_source_semantic_range_cases.h
  - tests/parser/test_token_source_construction_range_cases.h
  - tests/parser/test_semantic_loop_range_evidence_cases.h
  - tests/parser/test_semantic_query_symbols.c
  - tests/parser/test_aot_c_guardrail_contracts.c
  - tests/parser/test_parser_array_budget.c
  - tests/parser/test_parser_token_ownership.c
  - tests/parser/test_parser_recovery_ownership.c
  - tests/parser/test_parser_nesting_limit.c
  - tests/parser/test_flat_expression_compile.c
  - tests/parser/test_reference_fact_emission.c
  - tests/parser/test_reference_function_identity_fact_cases.h
  - tests/language_server/test_lsp_local_semantic_query.c
  - tests/language_server/test_lsp_local_semantic_scope_cases.h
  - tests/language_server/test_lsp_inlay_semantic_facts.c
  - tests/parser/test_typed_call_binding.c
  - tests/parser/test_typed_call_binding_imported_returns.inc
  - tests/parser/test_typed_call_binding_cyclic_returns.inc
  - tests/library/test_call_binding_module.c
  - tests/library/test_call_binding_module_imported_meta.inc
  - tests/library/test_call_binding_module_imported_instance.inc
  - tests/iterator/test_yield_semantic.c
  - tests/iterator/yield_public_compile_graph_cases.inc
  - tests/parser/test_escape_metadata_pipeline.c
  - tests/parser/test_escape_native_member_cases.inc
  - tests/parser/test_reference_receiver_call_boundary.c
  - tests/parser/test_reference_receiver_frame_contract_cases.h
  - tests/parser/test_token_source_edges.c
  - tests/compileTime/test_syntax_expansion_contract.c
  - tests/compileTime/test_comptime_control_flow.c
  - tests/compileTime/test_comptime_control_flow_edges.c
  - tests/compileTime/test_comptime_runtime_contract.c
  - tests/compileTime/test_comptime_frame_lifetime.c
  - tests/parser/test_parser_metadata_budget.c
  - tests/parser/test_parser_primary_budget.c
  - tests/parser/test_syntax_hygiene_aliases.c
  - tests/parser/test_syntax_hygiene_import_aliases.c
  - tests/parser/test_syntax_hygiene_alias_scope.c
  - tests/parser/test_syntax_hygiene_nominal_types.c
doc_type: testing-guide
---

# Macro Foundation Validation

## Comptime Frame Budget Gate

The lifetime executable preserves its original seven cases and appends four
contracts from `test_comptime_frame_budget_cases.h`:

| Added contract | Required oracle |
| --- | --- |
| Public scalar evaluation | A real parsed integer literal succeeds at zero heap allowance without frame-array requests, heap/aggregate charges or retained roots. |
| Temporary-root growth | Four rooted strings remain intact when the complete nine-slot candidate exceeds the heap limit by one byte; no allocator request, usage change or lost handle is permitted. |
| Binding growth | Four real bindings and their name roots remain intact under the corresponding full-candidate refusal, with exact resource/request evidence. |
| Rejected value update | A real foreign-domain string is rejected without changing the integer value, mutability, budget usage or either domain's roots. |

Allocator forwarding is restored before the target assertions. Local frames
and the foreign state are released before the exact behavior-failure messages;
normal fixture teardown releases compiler, AST and VM ownership. These checks
do not establish absence of every native allocation leak.

Closed lower RED r3 receipt
`96422BCFE3879C642189816B11E9882E77DBD58AD9DC0CF444F460A260F1E23B`
records old7 PASS and exactly four new behavior FAIL, Unity11/4/0, native4,
six naturally terminal children and fixed-input closure. Controller exit0
recognizes expected RED, not suite success. R1 remains FAIL for a Unity
separator mismatch; r2 remains FAIL for a foreign HEAD change before build.

Post-repair GREEN attempt r1 failed before build after HEAD changed. Attempt r2
actually built successfully and returned native0 with eleven ordered PASS
rows, footer11/0/0 and OK, but its source closure detected concurrent edits to
`test_canonical_consumers.c` and `test_reference_callable_consumers.c`.
Receipt `58C209CE353E7F1765DE78860EE74CC20B94D6E3E67596D3A2D6CBD7201A24FB`
therefore remains FAIL and is not adopted as accepted lower evidence.

`test_comptime_frame_transaction_cases.h` appends ten independent controls:
binding and temporary-root variants for candidate OOM, real candidate-allocation
GC, exact four-to-nine capacity growth/reuse, and live heap consumption that
either rejects or permits the final transaction commit. They preserve all
eleven earlier registrations. OOM and commit refusal require unchanged live
rows/handles and no candidate charge; the unrelated one-byte live consumption
must survive refusal. Positive interleave must retain that byte as well as the
complete candidate fee. Each test restores the allocator and frees its frame
before asserting complete usage, root-depth and root-count observations.

The aggregate fixture retains its twelve ordered cases and appends two private
constructor controls. Its four public-success cases include frame backing
while separately preserving the old exact header-fee oracle. Eight rejection
or effect cases remain unchanged. FFI wrapper inference now has three real
Parse/Compile/public-Infer visibility controls: default-private and explicit
`pri` must reject, while `pub` retains `Ptr<NativePoint>`, `NativePoint` and
`i32` types. Only the original positive fixture's `y` field changes to `pub`;
`x` stays private. Static reviews accept these fixture changes, but current
native acceptance remains pending.

Actual GREEN r3 receipt
`B6535EB4B3476D4867AF0D43C32DFE2CC518830E5C4E232E5B4904130E6B53B2`
records all 21 ordered PASS, footer21/0/0, native0 and six naturally terminal
exit0 children. Source4917/ED17EA0D and HEAD7c006421 remain fixed; support,
environment, registration, cache and binary closures pass. This accepts only
the frame lower contracts. The candidate GC probe does not exercise allocator
reentry during shared Core root-table growth. Aggregate14, FFI controls, fresh
26-document validation and complete Windows/Linux acceptance remain required.
M1 is OPEN; retain historical failures.

## Current Full Matrix And Support Repairs

The current fixed inventories remain52 foundation CTests,68 full selected CTests,
282 build targets and15 direct suites452 ordered cases plus3 numeric cases.
Additional Unity cases in existing CTest binaries do not remove any old case or
change the aggregate executable inventories. No timeout, compiler flag, tool,
PATH or failure expectation has been weakened.

| Actual run | Receipt SHA-256 | Result and boundary |
| --- | --- | --- |
| GCC full V4 R10 | `95C5A2BA1CA2D0C98B70AF69C084EBD8F8780911D47FC8C5EFFD23D816CC53E7` | Foundation52 and direct452 plus numeric3 PASS; full282 build naturally0. Full CTest66/68, exit8: AOT guardrail sixth case and three original LSP interface cases fail. CLI not reached. All30 main and three outer children terminal. Five foreign source changes independently invalidate fixed-input credit. Overall FAIL. |
| Windows full V4 R3 | `BF74386724768F9D3B61ABE3246886B7318E73352CF029A62D7CE1E9D5F6105F` | Direct455 PASS; full CTest66/68 with the same AOT/LSP failures; CLI not reached. All reached children terminal. Foreign source drift independently invalidates credit. Overall FAIL. |
| Clang full V4 R11 | `054BF737852F1FFBB897FE728274AF0DB95533E4EA31275931DEFD9AAB4B1346` | Registered source differs from the actual source before build; no configure/build/native execution. Overall FAIL, no platform credit. |
| Extern bootstrap metadata lower RED93 | `33813892BBAF04604840EF44B8F5016BEB30FD09BD8028844FAA84F0328277E4` | Exact93 ordered cases,91 PASS/two metadata FAIL; six children naturally terminal and fixed-input closures pass. Earlier delegate/enum provenance and same-declaration shell identity controls PASS. Valid focused RED only. |
| Completion contracts lower RED96 | `ACA4328E3597A70849C4A557AE7692F6436A301911E3E483402F6306B28C1E67` | Exact96 cases,91 PASS/five behavior FAIL. Extern enum metadata, struct layout, normal struct field AST, failed-completion preservation and ordinary enum completion reach their actual assertions. Source4908/5155AB3D and HEAD4f53caa remain fixed; six children naturally terminal, all input closures pass. |
| Completion contracts lower GREEN96 | `62CD3602AA6B9F307FA606F3AEB7AFAA7756DCD52F38BB88B5DF17CC69092E44` | Exact96 unique ordered cases all PASS, footer once/native0, stderr empty. Normal shared-header rebuild completes320 actions. All six children naturally0; source4908/CF3B3177, HEAD4f53caa and support/configuration/registration/environment/binary closures pass. Independently audited focused GREEN only. |
| GCC full V4 R9 | `2EA69A1E567C6892995FB2429EBF1936479EE54246953254052463CD66FD750A` | Foundation52 PASS; full282 build0; direct452 plus numeric3 PASS. Full selected CTest66/68 PASS, exit8: genuine original weak mismatch and super UTF-16 navigation failures. All30 reached direct children naturally terminal; CLI not reached. Source4890 to4895 and HEAD68d68 to48c36 independently invalidate fixed-input credit. Overall FAIL. |
| Windows full V4 R2 | `F08FC926EF1D5F81D5C22C5D82148D64346F0BC7E8BC92B43911F181E869827B` | READY4895/98AE differs actual pre/post4895/3E9B. Four bookkeeping children terminal; no configure/build/test. Outer receipt `E261248D3ED344C5888A4BAF66C99EC7E0BA0B9DC6FE9F89DD502E9F1B6A5C8E` also FAIL. No platform credit or stale-input adoption. |
| GDB producer RED R2 | `F66EBE24C1532B25E2586B17AD2A0D70FCC450663B0BAC4F49702990C40950BA` | Two debugger children0, original inferiors1; finite inputs/scripts/environment closed. Weak fallback accepts requiresWake0 and later adds owner-release evidence; printed diagnostic remains2008, not observed4004. Super declaration is whole-body range, not the expected name starting at UTF-16 character16. This is diagnostic evidence, not suite GREEN. |
| Weak classifier RED R2 | `04CF41E51E231815F7269CF65737F3E856E7BC9EAAA24CE259B30764F94139F5` | Exact85 rows,82 PASS/3 FAIL/0 IGNORE; native3. The original mismatch and two new direct classifier controls fail. Six children terminal; seven foreign source changes cause strict FAIL. |
| Weak classifier GREEN attempt R2 | `06EBDF099525A9F90DDC4FFFDAF6C6E604D7A7389235F2D2F8BFFCA741B816CC` | After only the requiresWake guard, exact85/0/0+OK, native0 and six children terminal. Three foreign SSA source paths change4904/C47F to4906/EB3D; strict receipt remains FAIL. Actual behavioral success is not closed GREEN or platform acceptance. |
| Meta name GREEN attempt R1 | `7F0C23F2A7A8BDD1C459FB20DB6EDB3BC77C21771BA2E6606708A0653358D0C7` | Exact35/0/0+OK, native0 and six children naturally terminal. Two foreign SSA source paths changeAFA0 to4990; strict receipt remains FAIL, without closed GREEN credit. |
| Weak classifier GREEN attempt R3 | `15171CA53DED65D3FBA04338B1C1ADEBCC152584237C63F99A186217B6FB70D0` | Actual85/0/0 native0 and fixed source4907/521603AA, but HEADf03219 to0307da9 changes. Six children naturally terminal; strict FAIL independently of successful behavior. |
| Weak classifier GREEN R4 | `3C70657DFAE5191D6AC2D5DAF5C41598DBF042BE6583966A88D72130A8B791F5` | Exact85 ordered cases,0 failures/0 ignored,+OK/native0; six children naturally terminal. Source4907/521603AA, HEAD0307da9 and all support/configuration/registration/environment/binary closure pass. Focused PASS only. |
| Meta name GREEN R3 | `28D6CB3DC5A2ED4FCD0B6D61D177001552EDEF51B287269BA4D428BC28464C35` | Exact35 ordered cases,0 failures/0 ignored,+OK/native0; six children naturally terminal. Source4907/521603AA, HEAD0307da9 and all support/configuration/registration/environment/binary closure pass. Focused PASS only. R2 was never launched after its registered HEAD became stale. |
| Original super UTF-16 GREEN R2 | `5BAC77E69436191A596B0BE0E9844E2E9EDFCB9604EBBDE26B8B2B61A1F537A9` | Unchanged original test: one case and three real LSP queries, exact stdout, empty stderr/native0. Six children naturally terminal; source4907/521603AA, HEADf03219 and all support/configuration/registration/environment/binary closure pass. Focused PASS only, not full-platform or M1 acceptance. |

The latest96-case focused replay preserves the old88 names and assertions and
appends real delegate/enum provenance, shell identity and completion contracts.
The repair keeps preview identity separate from completion: normal producers
retain real field ASTs and replace a same-declaration open shell only after
successful metadata/decorator/binding construction. Failure leaves its previous
metadata and the real missing-type diagnostic intact. Closed generic copies
cannot be selected for open-shell completion. The [ownership contract](../parser-and-semantics/type-prototype-metadata-ownership.md)
and [LSP contract](../parser-and-semantics/lsp-typecheck-canonical-binding.md)
describe these shared paths. Original AOT7 and LSP119 upward regressions, fresh
GCC/Clang/MSVC full matrices, fresh25-document validation and the separate UNIX
AOT integer gate remain pending; none is supplied by focused96 GREEN. Historical
full-matrix and GDB failures retain their original outcomes.

The new meta-name lower regression has35 ordered cases: unchanged31 plus four
text/mixed class/struct controls. RED R3 actually compiled the zero-initialized
field scaffolding with unchanged producers, then passed original31 and failed
exactly four final assertions with name ranges0..0:35/4/0, native4. All four
registered behavior messages match after valid Parse/EOS, member/leaf/body
evidence and cleanup; no fixture prerequisite failed. Six direct children
terminate naturally. Six foreign SSA source paths change14AF to4F8D while all
other closure flags stay true, so receipt
`DFFF692AC9BA17ABCEFB73D2A098C46FC404C6C8250C031DCF0B840CB6D79EDE`
remains FAIL, not closed RED credit. The checked merge producers and three LSP
consumers are implemented. Their targeted behavior and unchanged upward UTF-16
results, including the later closed lower reruns, are recorded above. They do not
relabel the source/HEAD-drift attempts. The [name-range contract](../parser-and-semantics/macro-token-foundation.md#meta-name-range-checkpoint-2026-10-18)
records the three-stream fixture and original-source boundaries.

Clang full, fresh full GCC/MSVC, the separate UNIX AOT integer-boundaries gate,
revised22-document validation, M1 acceptance and commit remain OPEN. Earlier
document receipts freeze their older revisions and do not cover these edits.

## Receiver Storage And Dispatch Gates

The unchanged31-case receiver vector now contains a deterministic storage
oracle in its existing twelfth case, not an additional case. Real source
compilation must publish i64 field/prototype/runtime/payload size and alignment
consistent with `TZrInt64`. The validated runtime STRUCT has one POD field;
the enclosing frame can require greater alignment. The public layout resolver
can allocate its lazy function-owned cache. Original no-copy/runtime assertions
and strict negative guards remain required. Historical Windows31 results do
not validate the current source or Linux binaries.

All receipt names below are under `E:/cargo-targets/zr_vm/reports/`.

| Actual run | Receipt and SHA-256 | Result and boundary |
| --- | --- | --- |
| Storage RED R2 | `m1-wsl-gcc-receiver-storage-layout-red-r2-receipt.json`, `3BD3B99D27AC509903B27A8111D9BE58509C0ABCC79B7A12CE37CA8AFBBD568F` | Exact31 rows:27 PASS/four FAIL/0 IGNORE; native exit4, unique31/4/0 footer and terminalFAIL. Case12 expected8/actual32. All11 direct children terminate naturally; source4882/HEAD/support/tools/environment/cache/binary closure PASS. Root208d02 reconstructs the complete source delta. This is TDD RED, not acceptance. |
| Alignment-only attempt | `m1-wsl-gcc-receiver-storage-layout-green-r1-receipt.json`, `3494BFB4BC19979E7C6C7F142DC5755C3E01444DC2BBD346FB5DD7DA45C44671` | OverallFAIL: exact31 rows28 PASS/three FAIL/0 IGNORE; native exit3 and31/3/0+FAIL. Case12 and its original runtime/no-copy assertions PASS. The last3 cases still fail before execution because the required direct member opcode is absent. All11 direct children naturally terminate and full4883-input/HEAD/support/tools/environment/cache/binary closure PASS; Rootca78f9 independently verifies it. No all31 GREEN credit. |
| Storage and dispatch GREEN R2 | `m1-wsl-gcc-receiver-storage-layout-green-r2-receipt.json`, `89D4D643675509C73B9F00A007D6119D5F0F91DD5D400820BC1070A31E392F4D` | Exact31 rows31 PASS/0 FAIL/0 IGNORE; native exit0, unique31/0/0 footer and terminalOK without trailing output. All11 direct children naturally terminate0. Complete4883-source/HEAD/support/tools/environment/cache/binary closure PASS; Root5f0451 reconstructs both full source inventories and verifies raw output. Focused receiver repair GREEN only. |

The deterministic oracle message is
`int-only struct alignment must follow its TZrInt64 storage`. The independent
dispatch defect came from unconditionally clearing direct eligibility for
compiled inline receivers. A narrow source-reviewed repair preserves callable
staging for ownership/mutable/meta/spread and only restores eligible ordinary
readonly direct calls. Fresh GCC all31 GREEN is verified at this checkpoint;
other platforms and the full matrix remain separate required gates.

Natural alignment changes can affect total struct/frame sizes and implicit
extern padding. Explicit positive alignments/offsets, size mappings, ordering,
padding algorithms and runtime address/layout/cache rejection checks are not
weakened. Adjacent Core, union, generic and extern gates remain separate from
the original325+127 direct-case matrix. A key/dedup-only generic test is not a
generic field-layout oracle, and explicit pack/offset tests do not prove
implicit i8/i64 extern padding. A reviewed helper now augments the existing
compiler127 layout case with normal source compilation of closed
`AlignmentPair<int>` and implicit extern `NaturalPad`. Exact serialized
prototype/instance-field scalar snapshots are compared with unpacked C mirrors
after function/VM cleanup. The unchanged127 vector and seven adjacent suites193
cases form the focused320 gate below. This coverage
augmentation is not fabricated pre-repair RED evidence.

| Adjacent run | Receipt SHA-256 | Actual result |
| --- | --- | --- |
| R5 precision RED | `DB147784DA01C9E9CEACE5EAA025FB927D24BD11D194B142E4452CC1B124B7A8` | Exact320 rows319 PASS/one FAIL/0 IGNORE; only independent signed-i64 positive/negative2^53+1 oracle FAIL. FFI native exit1; other seven native exits0. All18 direct children naturally terminal. HEAD33c3f43e and4890/318A9714 source unchanged; support/environment/cache/ELF closure PASS. Valid RED, not acceptance. |
| R6 repaired GREEN | `511E8B48C1186187E73AAE413D7F52032EFB9D2CE49499331F4B711213F6F168` | Exact320 rows320 PASS/0 FAIL/0 IGNORE; all eight native exits0 and all18 direct children naturally terminal. HEAD33c3f43e and4890/E36006EF source unchanged; strict support/environment/cache/ELF closure PASS. Only production change from R5 is the reviewed13-line tagged64 argument repair. Focused gate only. |

The [tagged64 contract](ffi-tagged64-argument-precision.md) records the real
signed-only oracle, unchanged original30 FFI case vector and remaining numeric/
result/callback boundaries. Canonical union metadata recovery resolves the
nominal definition without reading a UNION payload as nominal, while checking
semantic-record identity against the original outer canonical ID. The full
union69 vector and native-extern30 vector pass in both R5 and R6. R3 source/HEAD
FAIL and R4 preflight HEAD rejection remain immutable failures, not waived runs.

The first20-document storage checkpoint passed20/1473 metadata/221 links/nav7,
receipt `m1-docs20-storage-r1-receipt.json`, SHA-256
`0987DF4A31FE7D048E8ECA972975D1CAF3FB56B8F8F30896BA5054036DB81BB7`.
That receipt freezes the prior document bytes; subsequent edits require fresh
registration and validation. The revised documentation selection is22
documents: prior20 plus the union contract and new tagged64 page. Old19/R34 and
prior20 validation do not cover these revisions. Full MSVC/GCC/Clang, current documentation
validation, M1 acceptance and commit remain OPEN.

## Parser Statement Recovery Checkpoint

These parser-statement support labels are independent of earlier carrier and
full-matrix rounds; actual R43 RED ran before R42 GREEN. Receipts below are under
`E:/cargo-targets/zr_vm/reports/` and retain focused-only credit.
`test_parser_recovery_ownership.c` registers 20 cases: the original 15, three
valid following direct/nested/optional calls, and two fresh-invalid controls.
The three valid cases check retained statement/call AST shape, visible original
missing-semicolon diagnostics, sticky `hasError`/message, original source ranges
and shared AST ownership; the direct case additionally checks exact call offsets.
The two invalid controls require fresh primary-expression errors and reject a
published partial `calc` function for invalid-first/partial argument lists.
Every fixture tears down AST, parser and VM in that order, then requires zero
tracked allocator blocks. This is per-fixture cleanup evidence, not a global
native-memory-leak claim. The unchanged 15-case inlay-semantic-facts runner is
the upward LSP gate, not 15 additional ownership fixtures.

| Actual run | Receipt and SHA-256 | Result and boundary |
| --- | --- | --- |
| Statement R43 RED | `m1-msvc-parser-statement-recovery-red-r43-receipt.json`, `58D847966F2CC804405154AE46B310992C5E157798D516041F535ADFB6AF5B8A` | Ownership 17 PASS/three FAIL, including all three valid-following-call failures; both fresh-invalid controls PASS. Original LSP 14 PASS/one FAIL. Strict closure PASS, all 7 children terminal. This genuine RED is not rewritten by R42. |
| Statement R42 GREEN | `m1-msvc-parser-statement-recovery-green-r42-receipt.json`, `80C6AFBCBB203C05E2F1C4F1153701AF5AD0AD44B4B6989692385C3305A4B11F` | Ownership 20 PASS/0 FAIL/0 IGNORE, Unity 20/0/0+OK; original LSP 15 PASS/0 FAIL, unchanged normal protocol. Exact ordered rows, native exits0, strict closure PASS, all 7 children terminal. |

R42 pins source 4881/identity `A7615FF0`, HEAD `8761bf7b`, support/cache/binaries
unchanged, and normal PE stack reserve1048576/commit4096. `parser_blocks.c`
isolates only prior recoverable statement errors; fresh/fatal/nesting/source-
evidence/budget failures, callbacks, postfix/argument guards, ownership and
cleanup remain enforced. No getter/cursor/recursion, header/API, CMake, stack,
limit or timeout change is credited.
Only this focused recovery regression is closed. Full MSVC, fresh Linux
GCC/Clang, fresh selected-docs19 validation and M1 acceptance/commit remain OPEN.
Historical R31 docs19 PASS covers its frozen inputs, not this documentation revision.

## Full Windows Focused R27-R29 Checkpoint

These support-run labels are separate from the dated 15-suite carrier R27/R28.
Receipts below are under `E:/cargo-targets/zr_vm/reports/`; only the stated
focused contracts receive execution credit.

| Actual run | Receipt and SHA-256 | Result and boundary |
| --- | --- | --- |
| Reference R27 | `m1-msvc-reference-identity-red-r27-receipt.json`, `2A148A233BA66CF03A2BC5152F4E668E2E5B2CF4B3E612706864AFC78A17EA09` | Genuine behavioral RED: 19 reference cases give 14 PASS/five FAIL; 33 local LSP cases give 32 PASS/one FAIL. No fixture-prerequisite errors; closure PASS, all 7 children terminal. These FAIL rows are not rewritten by R28. |
| Reference R28 | `m1-msvc-reference-identity-green-r28-receipt.json`, `620934251C04F03E94B0126E33956217AEC3B86F1AFBBECDB0C3FFBA5ECC71E2` | Reference 19/19 and local LSP 33/33 PASS, exact ordered normal protocols, Unity 19/0/0+OK, native exits0; source 4865/identity 442FF26A, HEAD 4064a1f4, 24 support pins/cache/binaries stable, all 7 children terminal. Focused canonical function/READ dependency GREEN only. |
| Cold-frame R29 | `m1-msvc-stack-cold-frame-r29-receipt.json`, `F027FC6BA658DA65E4F75D3588E210250AF3FA939936680CC8A2DE502A88F485` | Unchanged nesting 6/6 and flat-expression 3/3 PASS, exact ordered Unity footers 6/0/0 and 3/0/0+OK, native exits0; source 4867/identity D725B778, HEAD 9306cc82, 24 support pins/cache/binaries stable, all 7 children terminal. Normal PE stack reserve1048576/commit4096; no limit/flag/timeout change. |

R28's 19th case uses real TokenStream/AST ownership and public `AppendReference`
to clone signature/provider text and both native arrays, frees caller arrays,
then checks canonical identity/range/context, two direct identifier inferences,
one READ and unchanged context-owned payload addresses/content. The baseline
is a borrowed view, not a second owner or evidence that VM-string GC roots are fixed.
Completion still checks `canonicalLocal`, VARIABLE kind, two requests, the same
current canonical snapshot and unchanged fact counts; normal detail starts
with `int\nSemantic facts: `, preserving normal fact enrichment.
R29's PE CodeView GUID/age matches both actual PDBs. Frames in bytes are
`parse_generic_type`1016->344, generic finish744 outside argument recursion,
`ZrParser_Expression_Compile`472->96, binary compile1448->976 and post-child emit344.
The generic `wholeRange`/`GENERIC_TYPE.location` distinction is retained; the
private binary path keeps three inferred types across child compilation and
borrows their addresses in emission. This does not establish constant stack use.
R17/R18 and stack R25/R26 FAIL remain historical FAIL. Original CTest
`parser_nesting_limit`/`flat_expression_compile` with 30/20-second timeouts,
the full Windows 65-CTest/280-target matrix and aggregates, aux7/snapshot37/graph2, fresh docs19 and
complete repaired Linux GCC/Clang remain OPEN; M1 is unaccepted/uncommitted.

## Full Windows R17-R19 Diagnostic Checkpoint

This historical Full Windows namespace is separate from the dated carrier R17-R19.
Receipts/logs below are under `E:/cargo-targets/zr_vm/reports/`; no repair is credited.

| Actual run | Receipt and SHA-256 | Result and boundary |
| --- | --- | --- |
| Full R17 | `m1-msvc-current-r17-receipt.json`, `EC3B7A40F2C0C1119E7F544403BB843FC359E7873680B77FD5013E8727856551` | Foundation52 PASS and full280-target build exit0; lower323+original127=450 native PASS/zero FAIL/ignored are focused only. Full65 CTest:61 PASS/four FAIL, exit8; source4853-to4854 and HEAD drift independently FAIL. Controller exits1, all29 children terminal; CLI not reached. |
| Diagnostic R18 | `m1-msvc-full-failures-diagnostic-r18-receipt.json`, `90D84D488BCEE44C7D1C7B1B882865F4C75DB7FEB40BDBA6B35F43157D028214` | Same four default binaries, normal1MiB stack: nesting/flat exit `0xC00000FD` before Unity rows; reference-fact suite14 PASS/one extern FAIL/zero ignored; local LSP three FAIL. Behavioral FAIL despite closure PASS, stable source4854/HEAD/support/binaries and all8 children terminal. |

R17's failed CTest names are `language_pipeline`, `language_server`, `parser_nesting_limit` and `flat_expression_compile`.
R18 retains the extern identity failure and the LSP float-range, resolved-function-value dependency and completion-cache failures; double/completion fixture prerequisites still need separate checking.
R19's unchanged-image GDB/PDB probe observes nesting `recursionDepth=187 < 256`,
`hasNestingError=false`, a4496B `parse_statement` frame and the repeated block/statement path.
Flat's actual fault PC resolves to `ZrParser_BinaryExpressionType_Infer`; the `ZrParser_ExpressionType_Infer` dispatcher frame is2856B.
The old GDB mapping does not establish a complete flat callchain; only matching PDB/actual-PC mapping is claimed.
The two frozen logs are `m1-msvc-stack-crash-r19-flat-gdb.log` SHA-256 `64AB29C5B4BB348071C663C1D2AA66B1245351C9FCA4C6022E6231F575A3E894`
and `m1-msvc-stack-crash-r19-nesting-gdb.log` SHA-256 `0484D11E5124085587F95E949A81939CE4B98A47E3B3F24E2FDBE1216FA61D12`.
Both inferiors naturally terminate with `0xC00000FD` before GDB quit/exit0; no kill,
image/stack/timeout change or inferior memory patch is used. LLDB is unavailable
with missing-DLL `0xC0000135`; `lldb-instr` is instrumentation, not a debugger.
At this diagnostic endpoint, cold-frame, extern-identity and READ-fact candidates
were unapplied; the focused checkpoint above records later repairs. Source Span
and AST-owner contracts are retained; these failures are not all attributed to AST ownership.
GCC R6(controller36537) and Clang R7(controller14545) were both live at that endpoint, without acceptance.
R6's foreign source drift is not waived; only Clang R7 adopts fresh4854-input identity
`4783ACB41F0F36E29EE4485B04CD0B8B564F805D87B0F7CCE126A4AC4A32AC14` before launch. GCC R7 had not run at that endpoint. Fresh repaired Linux compiler/test acceptance,
full Windows, remaining CLI/aux/graph, fresh selected-docs19, M1 and commit remain OPEN; M2-M6 have not started.

## Full Windows Ownership Fixture Checkpoint

The Full Windows R13-R16b labels below use a separate run namespace from the dated
carrier R13-R28 sections. R16b is focused fixture GREEN, not a full Windows
or platform pass. Receipts below are under `E:/cargo-targets/zr_vm/reports/`;
their FAIL, Timeout and prerequisite observations are not retrospectively
reclassified.

| Full Windows run | Actual receipt and SHA-256 | Observation and boundary |
| --- | --- | --- |
| R13 | `m1-msvc-current-r13-receipt.json`, `C609B79830AA5B1B3A9B82CD094815CDB8B516771100AC884CBE466FF93B63A7` | Foundation52:50 PASS, ownership `Expected Non-NULL` FAIL and nominal Timeout at39.21s under the original30s property; `fs_common.c` also changes during the run. Strict FAIL, no full-matrix credit. |
| Repro R14 | `m1-msvc-foundation-repro-r14-receipt.json`, `A105ED0FBE857B04F820CAD227DC712F4CF9CE700F12DE808AAD88F50D2ED250` | Unchanged-binary nominal47 PASS, including its original30s CTest replay; ownership4 PASS/one FAIL and CTest exit8. The Timeout does not reproduce; its root cause and performance implications remain unproven. Diagnostic evidence only. |
| Fixture R15 | `m1-msvc-budget-fixture-prerequisites-r15-receipt.json`, `7237FF424639DDE7459123860377E76A27278DE0950B1ADD0C2E2FE059FF0363` | Prebuild closure FAIL:4849 inputs versus4848 registered, with new foreign `tests/parser/support/ssa_sccp_pooled_int64_fixture.h`. No build, CTest or native suite executes. |
| Fixture R15b | `m1-msvc-budget-fixture-prerequisites-r15b-receipt.json`, `14CF19289BB4F4422CD79E8A87139A0E9ABF26D8F14E798DBCACBC78D1D6AF39` | Ownership3 PASS/two exact healthy-Init prerequisite FAIL, nominal47 PASS; ownership exit2 and CTest exit8. The expected-prerequisite controller exits0, not ordinary GREEN or production behavior RED. |
| Fixture R16 | `m1-msvc-budget-fixture-green-r16-receipt.json`, `71F8462EDEFB7FC096D1F51ED63351074ECD62F95BDF6F3B13ECACFC9E8B2F78` | Prebuild closure FAIL:4850 inputs versus4849 registered, with new foreign `tests/parser/support/ssa_sccp_pooled_int64_fixture.c`. No build, CTest or native suite executes. |
| Fixture R16b | `m1-msvc-budget-fixture-green-r16b-receipt.json`, `D73FFE399F9D47255561A5BDCA36014E2F3E2D07B4C502F23C3C6B0742131D1A` | Ownership5 PASS and nominal47 PASS, zero FAIL/ignored; native exits0 and both original CTest entries PASS/exit0. Focused fixture GREEN only. |

The two original CTest names are `macro_parser_token_ownership` and
`macro_syntax_hygiene_nominal_types`; their existing30s properties remain unchanged.

R15b's two final reported messages are
`Shared-budget parser must initialize before AST parsing` and
`AST-node rejection fixture must first initialize successfully`. Neither
establishes the later shared-charge or NODE_COUNT cleanup contract. The only
subsequent fixture correction adds default FUEL to both existing budget cases:
removing those two exact lines in RAM restores the strengthened zero-FUEL
fixture's 8456B/SHA-256
`2023810FB5868AC1E0104233726C5F6EAB956A5390803E369ABE9490FBAB02CC`.
The corrected file is 8558B/184LF, SHA-256
`DD14AB512B0B4E836988482808FAD214DDAAD4002763F7AF92D567EC147C1FEF`.
No production charging, final oracle, case registration/order or timeout changes.

Root's complete R16b raw audit `b90b53` verifies the normal protocols and the
strengthened [budget contracts](../parser-and-semantics/macro-token-foundation.md#budget-and-transactions),
including healthy Init, NODE_COUNT usage3/limit3/request4, no source-evidence
error and the post-cleanup GC-root baseline. All8 owned children naturally
terminate/drain. Source4850 identity
`C8F28116B61537C4A25DD3638E7ACB45AF116C2F99286F2883AEC42F91C86D81`,
HEAD `ff34a71d99492c73bd6af41966813e4c43fb087f`, support and binary pins remain
stable. The independent15-input Core guard review is static evidence only;
the GC-root assertion bounds this fixture's AST-owner/string-root retention,
not all native memory leaks.

The later bounded RAM inverse of `fs_common.c` proves comment replacements,
appended comment blocks and EOF whitespace only; it does not waive R13's
in-run source drift. The foreign header/source additions above are adopted
only before their subsequent runs. Complete current-input Windows65/280,
pipeline126/CLI1/LSP83 aggregates, separate CLI/auxiliary/graph gates, a fresh
docs19 pass for these edits, Linux compiler/test evidence, M1 acceptance and
milestone commit remain OPEN. M2-M6 have not started; released bytes remain0
and lifecycle remains OPEN.

## Native Range R25-R28 Checkpoint (2026-10-17)

The dated 15-suite carrier result is R28:381 PASS/zero FAIL/zero ignored in the
same15 normal/default suites, including all378 prior cases and the three
genuine R27b final-behavior RED cases. Root's complete raw audit561905 checks
exact case names/order, native footers/exits and logs; native35083 naturally
exits0 and drains, with all20 owned children terminal. R27's invalid fixture
FAIL and all earlier RED/FAIL receipts remain immutable. This is not current
full Windows, complete constructor/LSP, Linux, M1 or commit acceptance.

The Windows280-target/65-selected-CTest inventory and pipeline126/CLI1/LSP83
aggregate inventories remain static; no completed current-input matrix result
is credited by R28. The separate Full Windows fixture checkpoint above records
the later focused follow-up, not a replacement carrier inventory. Complete
Windows, two graph checks, standalone CLI, auxiliary checks, fresh docs19,
Linux GCC/Clang, M1 acceptance and milestone commit remain OPEN. Linux is
UNPASSED and M2-M6 have not started. The old docs19 R24b PASS applies to its
old pins only; it does not validate these edits. Older dated
current/running/latest statements below describe historical endpoints, not
current full Windows acceptance. Released bytes remain0 and lifecycle remains OPEN.

### Constructor R25 RED And R26 GREEN

The three cases appended through `test_token_source_construction_range_cases.h`
to `test_token_source.c` retain real FromText/Concat input, original spans,
contexts, AST ownership, argument/ownership shape, native ledger, the second
return and EOS prerequisites. They are
`test_new_construct_keeps_mixed_call_close_evidence`,
`test_own_construct_keeps_mixed_call_close_evidence` and
`test_new_construct_without_args_stops_at_target_evidence`. R25's sole final
failures require the first two constructions to end at the real foreign close
with interval `[1,5]`, and the third to stop at its target `[1,2]` without
absorbing a foreign semicolon. All375 old cases pass; no fixture failure is
accepted as RED.

| Actual receipt under `E:/cargo-targets/zr_vm/reports/` | SHA-256 | Normal result |
| --- | --- | --- |
| `m1-msvc-r11-construction-range-red-r25-receipt.json` | `758BB831D1C7135F1962FC8FD83FA7EA85CC67610BCAE19EB954CAF5A64E730A` | 378 total,375 PASS/three final-behavior FAIL/zero ignored |
| `m1-msvc-r11-construction-range-green-r26-receipt.json` | `4E1431432C3801B9FBC1BCD2B714F53C5B2E21399494EA9A3A3BDACE4317A183` | Same378 total,378 PASS/zero FAIL/zero ignored |

R26 finishes `2026-10-17T23:12:51.118699+08:00`; Root raw auditf817b4 verifies
all15 normal/default exact names/order/footers/native exits and the unchanged
three tests. The one-file endpoint repair in `parser_construction.c` captures
the real closing token before consumption and uses the target location without
arguments, preserving checked-merge cleanup and child ownership. Grouped-target
runtime and every constructor/prototype-call category are not established.
Source4847 identity
`82CD5E14EFE86CA435E944B9E65FF71CA3EEBC59616963E6A33C0EC16BA836F2`,
raw pre/post `029CA662092FE91DB10E217CEF5BCF98DDF94E3F8F2822E9B569B198181B4D9B`,
HEAD `1873e51a413168af79abb72a5568f5f88a1bf259`, support7 and binary15 stay
stable; all20 children terminate. Build exits0 in14490ms with one D9025, not
W0; its log SHA-256 is
`144C5C87F81497CB621821D3F8F2E37E34C9A6EF37E1CBE417F822D986BF38FB`.

### R27 Fixture FAIL And Genuine R27b RED

R27 adds one generic target case and two public native LSP cases. Its receipt
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-native-range-consumer-red-r27-receipt.json`,
SHA-256 `D3BDFE3657F29C6C0F200000B407AA0401B64763528F3BD3776C94A1582D4F81`,
is FAIL, not a three-case genuine RED:381 total includes378 old PASS, two final
behavior RED and one fixture prerequisite FAIL. The mixed LSP fixture wrongly
requires Concat to preserve the original ContextTable pointer and fails before
the final `super` contract. Production stays frozen at that endpoint.

The two-line correction in
`tests/language_server/test_semantic_analyzer_lsp_native_range_consumer_cases.h`
requires a non-null table on the sealed Concat stream and compares the script's
owned table with `TokenStream_ContextTable(fixture->input)`. Concat's table is independent;
imported root contexts use semantic equality, not table-pointer identity.
The corrected header SHA-256 is
`0ED9F67CCE5E78825595479112084E7DDEEBC6321C4EBB9ACC183C6065853FBC`.
This is explicit fixture correction, not retrospective credit for R27.

R27b receipt
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-native-range-consumer-red-r27b-receipt.json`,
SHA-256 `14CE4099D0506E87E52A6A52F8D6A6B0FFFF12BF6C6A6320B6A4EBA8E6F5088F`,
has state `NATIVE_RANGE_CONSUMERS_RED_VERIFIED_NO_PLATFORM_CREDIT`. It finishes
`2026-10-18T00:02:39.471735+08:00` with378 PASS/three genuine final-behavior
FAIL/zero ignored; each target reason occurs once and fixture failures are
absent. Source4848 identity
`511134E11DD7ED360F512FBA52CBE15908643B56F05758B20092CC0214128853`,
raw pre/post `E7C330C93BC96AEE12D24D25483A1DE0846B4F284D0D173B6CF99022C6868932`,
HEAD `7c7335502be339afd0c875861c548200d29b19e4`, support7 and binary15 are
stable; all20 children terminate. Root raw audit80d33a accepts this genuine RED
before production changes.

### Three-File Repair And Actual R28 GREEN

The [bounded implementation contract](../parser-and-semantics/macro-token-foundation.md#constructor-and-native-lsp-r25-r28)
is limited to `parse_generic_construct_target` using existing
`genericType.wholeRange` for its TYPE wrapper, `signature_call_context_range`
preserving the complete FUNCTION_CALL location, and the two `super` range
helpers retaining parser `superCallRange` with an exact-scope same-source
keyword scan only. It does not repair every GENERIC_TYPE location, other
signature call kinds, mixed physical-position queries or all M6 behavior.
Cross-source `super` fallback preserves the real source and first parser
anchor; the regression does not constrain its returned end.

| Frozen new case | R27b RED to R28 PASS contract |
| --- | --- |
| `test_new_generic_construct_without_args_stops_at_closing_angle_evidence` in `test_token_source_construction_range_cases.h` | Target `[2,5]` and construction `[1,5]` exclude the foreign semicolon; both keep flags0 and validated native proof. |
| `LSP Signature Help Preserves Native Inner Call Close` in `test_semantic_analyzer_lsp_native_range_consumer_cases.h` | Public request at the real inner close keeps activeParameter1 after correct canonical inner label/two-parameter prerequisites. |
| `LSP Super References Keep Foreign Native Anchor` in that LSP header | Public references keep foreign URI_B and start line0/character41; the ordinary same-source keyword control also passes. No returned-end requirement is added. |

Receipt
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-native-range-consumer-green-r28-receipt.json`,
SHA-256 `9328752C698B8ECBFE70B204E29AFF111AE9ED188FA13C54F7AACA7E21C33306`,
has state `FOCUSED_381_GREEN_NO_PLATFORM_CREDIT`. It starts
`2026-10-18T00:20:36.450079+08:00` and finishes
`2026-10-18T00:22:32.032860+08:00`. All381 exact registrations PASS with zero
FAIL/ignored and every native exit0. The unchanged suite order/counts are
query77, ownership25, aot18, lsp87, guardrail7, facts20, semir4, token31,
edges9, array-budget4, collection-budget5, metadata-budget10, primary-budget37,
token-peak3 and scope-symbols44. All prior semantic/loop/domain/FUEL/guardrail
and constructor regressions remain, and the three final RED messages are absent.
The fixture's unconditional healthy observation is
`target=2..5 construct=1..5 flags=0/0 proof=1/1`, not a failure line.

Whole4848 source identity is
`2010FA374686BC221181A7AD40A9F31D8AD2F5C145D64458976F311EC9603023`;
source-pre/post raw identity is
`6C391E823DCA52484B0EB3F0ABCCD06327AE35729F618369E31BBBB827D141E7`.
HEAD `5842d412a074bcc73400ae2d29b737104e70ff7e`, support7 and binary15 stay
stable with no in-run drift waiver. This HEAD is adopted before the run after
review of already-present closure inputs, not an after-the-fact substitution.
The actual three production hashes are
`B95FC290A1EFCB7C610F75E644BAFF00C3A3F7CE3C03FCBD22FC79AB8EF0DCDA`,
`B7376E93167966D062446E33D6D816B5512356AEBEB9953A3932BDB6F295B5B0`
and `E2A9E5685FA567260378888B8F2978C66B74420B4D99F9E7393C998141B3461A`
for `parser_construction.c`, `lsp_signature_help.c` and
`lsp_super_navigation.c`, respectively. All tests remain unchanged after R27b.

The normal build reuses `E:/cargo-targets/zr_vm/build/m1-msvc-verified`, exits0
in52790ms and retains three D9025 warnings, not W0. Its log
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-native-range-consumer-green-r28-build.log`
has SHA-256 `920AC9E970960498BFA12BA7C7E2CB0C78A764B2574BF869A17A07927F2C6255`.
Native35083 naturally exits0/drains and all20 owned children are terminal.
Exact build/native commands remain in the receipt. No full matrix, platform,
M1, M2-M6 implementation or milestone-commit credit follows from this focused
GREEN; live source/AST ownership and alias GC-root limits remain unchanged.

## Semantic Carrier R22/R23/R24 Checkpoint (2026-10-17)

This section preserves the historical R22-R24 checkpoint. The latest focused
result is R28 above; the following inventories and open gates describe this
earlier endpoint and do not override that checkpoint.

The R24 executed carrier result is focused GREEN: the same 15 normal
suites/375 cases all PASS, zero failures/ignored, exact names/order/footer
protocols and every native exit0. The five genuine R23 consumer RED cases now
pass after the reviewed six-file repair. R22 and R23 retain their original
results below. Current full Windows inventory is statically 280 build targets
and 65 selected CTest entries; that inventory is not execution credit.
Constructor and LSP are the next focused gates. Their acceptance, complete
Windows, graph, CLI, auxiliary checks, fresh 19-document validation, Linux
GCC/Clang and M1 remain OPEN at that endpoint. The 19-document inventory is
explicitly decided below; its validation had not yet executed at that R24
native endpoint. Later historical R24b document evidence is recorded below.
Linux is UNPASSED, M1 is
uncommitted and M2-M6 have not started. Released bytes remain
0 and lifecycle remains OPEN. Earlier dated current/running paragraphs below
describe their own endpoints and do not supersede this checkpoint.

### Immutable R22 FAIL And Guardrail Fixture Correction

Receipt
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-semantic-carrier-red-r22-receipt.json`,
SHA-256 `17DF610ACF455DB7B22FB00ECE145A854BD1F4E2845A36541D1A1DF4048F01CE`,
remains raw FAIL: 374 cases, 367 PASS/seven FAIL/zero ignored. Five are the new
consumer behavior failures below. The two extra old guardrail failures assume
that signed i64 `+` is state-free, while the concurrent checked-integer ABI
requires state. They cannot be waived or converted into five-failure R22 credit.

All old named registrations and strict assertions remain. The shared state-free
i64 fixture deliberately changes to real `combine_i64` bitwise `&`, and one real
checked-i64-add emission case is appended. The resulting
`tests/parser/test_aot_c_guardrail_contracts.c` has SHA-256
`F51828105A9DDD968F5CB2B74085A1E6D81634BFC87046239F8B24D392286C3F`.
This fixture correction is explicit, not unchanged-fixture credit for R22.
The new case follows SourceCompile through binary emission into generated C;
the resulting C has 95,411 bytes and SHA-256
`380CE45B4E85FF2D500E5F91B32C21C2916D7871CB33EC0BA25A3D233F3CCE6A`.
It establishes the checked-add emitted-C contract, not execution of runtime
overflow behavior or a clean platform result.

### Actual R23 Focused RED

Root's raw-log review confirms exact ordered suite registrations, native exits,
footers, zero ignored and each of the five final failure reasons exactly once,
with no fixture prerequisite failure. Receipt
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-semantic-carrier-red-r23-receipt.json`,
SHA-256 `CBB7AF2F5244A1E55DE2BF594BCD998FAB221E6A2CB17C72B7C1D8F95EADC08B`,
has state `SEMANTIC_CARRIERS_RED_VERIFIED_NO_PLATFORM_CREDIT` and controller
exit0. It records 15 normal suites/375 cases =370 PASS/five consumer FAIL/zero
ignored. All seven guardrails pass, including the appended checked-add case.

| Suite | Total | PASS | FAIL | Native exit |
| --- | --- | --- | --- | --- |
| query | 77 | 77 | 0 | 0 |
| ownership | 25 | 25 | 0 | 0 |
| aot | 18 | 18 | 0 | 0 |
| lsp | 85 | 85 | 0 | 0 |
| guardrail | 7 | 7 | 0 | 0 |
| facts | 20 | 20 | 0 | 0 |
| semir | 4 | 4 | 0 | 0 |
| token | 27 | 24 | 3 | 3 |
| edges | 9 | 9 | 0 | 0 |
| array-budget | 4 | 4 | 0 | 0 |
| collection-budget | 5 | 5 | 0 | 0 |
| metadata-budget | 10 | 10 | 0 | 0 |
| primary-budget | 37 | 37 | 0 | 0 |
| token-peak | 3 | 3 | 0 | 0 |
| scope-symbols | 44 | 42 | 2 | 2 |

The three token failures are
`test_qualified_alias_preserves_mixed_type_use_evidence`,
`test_generic_alias_preserves_long_source_evidence` and
`test_primary_facts_preserve_mixed_call_evidence`, appended through
`test_token_source_semantic_range_cases.h` to `test_token_source.c`.
The two scope failures are
`test_for_scope_preserves_complete_mixed_source_body_interval` and
`test_foreach_scope_preserves_complete_mixed_source_body_interval`, appended
through `test_semantic_loop_range_evidence_cases.h` to
`test_semantic_query_symbols.c`. Their exact final reasons are:

- `Qualified type display aliases must preserve the complete native interval and first real anchor`
- `Type display aliases must retain valid complete evidence for long source names`
- `PRIMARY expression facts must retain the complete parsed range without rewriting foreign endpoints`
- `For scope evidence must end at its real body close without absorbing the following declaration`
- `Foreach scope evidence must end at its real body close without absorbing the following declaration`

The run starts `2026-10-17T14:16:47.993768+00:00` and finishes
`2026-10-17T14:21:05.089239+00:00`; all20 owned children naturally terminate.
Build42932 exits0 after125 planned actions in186008ms. Raw build log
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-semantic-carrier-red-r23-build.log`
has SHA-256 `C574B847D5E7A5A31CF3655D9948035DD4A1A05FBB6CB074A9A0E178C3BA5716`.
Token34400 exits3 in545ms; its raw token log has SHA-256
`5DBFF0C3BEF412B902FD05B760573A355CFE112BF22015F52B5ABE51ADDFD2CD`.
Scope-symbols51788 exits2 in7138ms; its raw log has SHA-256
`71AE7254F2EBB8F578F6DAC4FC982737688A88A22C9E70383780523342934F5F`.
Guardrail31824 exits0 in2461ms; its raw log has SHA-256
`A9415A206DE309A0A09758DD1C9BBC9E05D660649DEDB9470446EFFD5BCBBF1E`.
These three logs use the same report prefix with `-token.log`,
`-scope-symbols.log` and `-guardrail.log` respectively; stderr is empty.

Whole4846 source identity is
`95F8DB29648A403D8A9B1577B49CD5B7F8BA98054EBBBB8B991053D93B51EAF6`;
source-pre/post raw identity is
`C15FCB70D4CA3865F84F01335E537D4CC3D51A2862A54B26DDCD2A722D5761F8`.
HEAD `1873e51a413168af79abb72a5568f5f88a1bf259`, support7 and binary15 are
stable. No drift waiver applies. The actual build reuses
`E:/cargo-targets/zr_vm/build/m1-msvc-verified`; exact build targets and native
commands are retained in receipt `runs[].command`. This is focused behavioral
RED verification only, with no full-build, W0, Linux, M1 or milestone credit.

### Six-File Repair And R24 Focused GREEN

The [consumer repair contract](../parser-and-semantics/macro-token-foundation.md#semantic-consumer-carriers-r22r23r24)
covers checked qualified-alias merging under one AST owner, exact borrowed alias
source/range publication, original parsed PRIMARY facts, body-close loop ranges
and direct loop-node scope publication. Its scope is
`type_inference_type_display_alias.c`, `type_inference_semantic_facts.c`,
`semantic_display.c`, `semantic_display.h`, `parser_loops.c` and
`semantic_scope_facts.c`. Root confirms application and accepted post-apply
static review. The subsequent normal R24 run is complete, not merely a static
acceptance or build-only result.

Receipt
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-semantic-carrier-green-r24-receipt.json`,
SHA-256 `EA1ACDE2CA5216E87704FC91A2C89C1D8A242FF8E57CFDEA71C762BE6B958291`,
has state `FOCUSED_375_GREEN_NO_PLATFORM_CREDIT`. It starts
`2026-10-17T14:32:13.716775+00:00` and finishes
`2026-10-17T14:34:00.307492+00:00`. The same15 normal suites/375 registrations
now give375 PASS/zero FAIL/zero ignored. Exact names/order, footer protocols and
all native exits0 match the registered GREEN contract. In particular, token27
and scope-symbols44 are fully PASS, including all five genuine R23 consumer
RED cases, with no final RED reasons. The corrected shared bitwise fixture and
separate checked-add emission case remain unchanged; guardrail7 passes.

Build35932 exits0 after30 planned actions in36826ms. Its raw log
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-semantic-carrier-green-r24-build.log`
has SHA-256 `108C10CF9AC2453E4800937116EB2F47C64D85276590592018304AA5572621EF`.
The log retains D9025 and C4701 for `externalOriginRange` in
`zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c:385/465`;
no W0 or full-clean-build claim follows. Token52964 exits0 in993ms, raw log
SHA-256 `7509CF54EB13564777DF41ABA1335241AFCEECBD8878732DD41621E5D1FBA8BE`.
Scope-symbols42884 exits0 in6262ms, raw log SHA-256
`A238BB833EA556827409323AA91338A887AA8B25D43CA7C1E1770B72AB37ADD2`.
Guardrail42500 exits0 in3389ms, raw log SHA-256
`A9415A206DE309A0A09758DD1C9BBC9E05D660649DEDB9470446EFFD5BCBBF1E`.
These native logs use the R24 report prefix with `-token.log`,
`-scope-symbols.log` and `-guardrail.log`; their stderr logs are empty.

Whole4846 source identity is
`4BD6C7D43B66691EFA45077E7BFB37D3D4F428CC08FE2E9F5CA2D260C83FFFF8`;
source-pre/post raw identity is
`6AECBF7A9589FD624096E06DD97F46FE76CCAA962E8F1BCDF0E02494DB3CED7C`.
HEAD `1873e51a413168af79abb72a5568f5f88a1bf259`, support7 and binary15 are
stable, with no source drift or waiver; all20 owned children terminate. The
same physical E-root build is reused. This closes the five focused consumer
regressions under their documented live-owner contract, with no constructor,
complete LSP, full Windows280/65, Linux, M1 or milestone-commit credit.

Canonical TypeId remains derived from the inferred type. An optional alias miss
does not establish side-effect-free failure: checked merge budget work and its
sticky evidence failure remain. Borrowed source strings and native AST owners
must stay live. Existing alias/source GC-root TODOs and FullGC/query/Reset/Free
coverage remain open. Same-named alias physical-key queries and mixed-source
position selection remain M6; these tests do not establish runtime overflow,
constructor, complete LSP or platform acceptance.

### Historical Document Inventory And Current Refresh

Root explicitly confirms the fresh inventory as the old18 expanded document
registration plus
[diagnostic-message-catalog.md](../parser-and-semantics/diagnostic-message-catalog.md),
whose frontmatter is valid. Those19 documents plus two indices and three
support files define24 input pins. The first actual R24 docs attempt is FAIL,
receipt SHA-256
`95077E412DFBD37939687B670CBC86AC1AF385A13820AF1717D4F53C42AA4C50`,
because two catalog `related_code` paths moved. After exactly those two
metadata references are corrected, the historical R24b receipt
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-docs-r24b-receipt.json`, SHA-256
`34F9A0058BE428943E0D4320918DB928EF957ABC5DC2B1076BF80179693B1D24`,
records selected19 metadata PASS:1272 references,209 local links, seven exact
navigation links and all24 input pins unchanged. This applies only to that
old frozen document revision, not the subsequent R25-R28 documentation edits.
Fresh docs19 validation is OPEN and has not executed for the current revision;
Root will freeze/preregister it after document writers stop. The old18
registration and its historical evidence keep their original count, with no
retrospective19-document credit or whole-wiki/platform/M1 acceptance.

## Diagnostic Evidence R21 Focused GREEN (2026-10-17)

This section preserves the dated R21 endpoint, including its original inventory
and then-open gates. The R28 checkpoint above is the current carrier state.

R21 unified57600 naturally exits0/drained with state
`DIAGNOSTIC_EVIDENCE_ALL_327_GREEN_NO_PLATFORM_CREDIT`. Receipt
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-diagnostic-evidence-green-r21-receipt.json`,
SHA-256 `33CCC6F59BCE100C2BF0D57854DE01F9DFDBEB5B824744398ECDB2D082628EB1`,
records the same14/default327 =327 PASS/zero FAIL/zero ignored. Exact ordered
case names, footers and raw log identities are retained; no cases are reduced.
Token24/query77/ownership25/LSP85 and every other normal suite, including the68
extra support cases, pass. All three diagnostic fragment/interval/GC/atomic
cases and the real foreign-context-domain refusal pass with no fixture failures.

The successful duplicate now commits comparison FUEL: actual
`appended=1 facts=1 roots=1 pins=1 input=1 otherUsage=1 fuel=1 before=107 after=114`.
Fact/header/related payload, caller input, roots, pins and non-FUEL usage remain
unchanged. This verifies only the narrow R20 RED-to-R21 GREEN transaction change;
failure keeps its existing abort path. The
[publication contract](../parser-and-semantics/macro-token-foundation.md#diagnostic-evidence-publication)
still requires live owner/context and immutable live inputs, and does not supply
global host-OOM, GC-metadata rollback, concurrent-owner or AST-free guarantees.
The known foreign-import AST lifetime boundary and seven cross-AST carriers are
not repaired or accepted by this result.

Started `2026-10-17T11:16:37.170380Z`, finished
`2026-10-17T11:17:16.269101Z`; all19 owned children naturally terminate with exit0.
Build45384 exits0/15863ms with17 planned incremental steps; raw
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-diagnostic-evidence-green-r21-build.log`
has SHA-256 `E2473F3915CBDE698530F21C14081737F3DE8BD254646FE71A5645B5EE38EC46`.
It retains one D9025, not W0 or full-clean-build credit. Token32340 exits0/131ms
with24/0/0 footer; raw
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-diagnostic-evidence-green-r21-token.log`
has SHA-256 `1837BAB09EF33ECD8D3FF7FF505EA5C2E1E1EAC73793408CB9849DF363F4C4D8`.
Whole4826 identity is
`9BFECA7AE2C6CA5DD469FFB42BD8463FD3E8B9DC05473439BDB5FF2F1438F107`;
source-pre/post share `27CA290E8717E40749028A064634FFB5D7A8F305DF7C409B8413498027C2D078`.
HEAD `957a391b705f5052b8a92751e5cef64c571243ca`, support6 and binary14 are stable.

R19 strict49 FAIL and R20's genuine duplicate FUEL RED below remain historical
facts. This focused Windows GREEN does not accept full273/55 CTest, CLI, aux3+4,
graph tests, docs validator, Linux GCC/Clang, M1, M2-M6 or milestone commit.
Linux remains UNPASSED, released bytes0 and lifecycle OPEN.

## Diagnostic Domain R19 And FUEL R20 (2026-10-17)

The pending-state statements in this section describe the R20 endpoint; the
later focused R21 result above does not rewrite either run's raw history.

R19 is strict FAIL, not a successful expected-RED gate. Receipt
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-diagnostic-context-domain-red-r19-receipt.json`,
SHA-256 `3015005AE9F92282029C9DD90861F0336778DC2489D6C430730CF14BC2DF8294`,
records unified64726 naturally exit1/drained and all19 owned children terminal.
The normal14/default326 give 277 PASS/49 FAIL/zero ignored: query77 has12 FAIL,
ownership25 has4, LSP85 has32, and Token23 has the new foreign-domain failure.
AOT18/guard6/facts20/SemIR4 and all68 extra support cases pass. The 48 old
regressions are prohibited regressions, not revised expected failures.

All old Token22 pass. Physical variants0/1/2 each report complete1; native inner/
outer/equivalent intervals leave two complete facts; HEAP3/FUEL1 rejection,
category, unchanged fact/root/pin/usage and retry predicates are all1. The formerly
conditional external-input release/two-full-GC branch is reached for the first
time and passes its completion-count check. These are bounded lifetime results,
not an AST-free, concurrent-owner or global-OOM guarantee. The real two-runtime
negative reports `rejected=0 facts=0 usage=0 rootsA=0 rootsB=1 pins=0 input=1`;
all hard prerequisites pass, so this fourth diagnostic case is genuine RED.

R19 build49368 naturally exits0/139922ms with66 planned actions; raw
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-diagnostic-context-domain-red-r19-build.log`
has SHA-256 `1D185D73C0F80222D485F89CD7664DC09B0A68EF569579DC8FF5C2E412794A87`.
It retains48 D9025, two old C4701, new owner flexible-array C4200 and13 CMake path
warnings. Token49124 naturally exits1/1889ms with23/1/0 footer; its raw
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-diagnostic-context-domain-red-r19-token.log`
has SHA-256 `B93B63EE983ABAA96BC80D8E5CFA5C44FE9C29444041BD6538BDF62718010E19`.
Whole4826 identity is
`99FC9BD951D60694C5677F237A207AF519229EE0B9EC793766B3673F6D643955`;
source-pre/post share `83C16C8D426108FB280BF51A35433E17836B8D9CEC8A51511302F2A3047B39DE`.
HEAD `a8c29b8d7c3bc5c730190d0bfa7fb843be6edc4c`, support6 and binary14 are stable.

The subsequent narrow publisher fallback uses a live script AST only when
`currentAst` is NULL; it preserves the original diagnostic location/Span and
strict owner validation. Context-state string inventory validation independently
rejects foreign-domain inputs before owner-pin publication. These implementations
are covered in the [publication contract](../parser-and-semantics/macro-token-foundation.md#diagnostic-evidence-publication),
including immutable/live input preconditions, staged arrays and pins, atomic
budget commit, owner release and the still-unfixed foreign-import AST lifetime
boundary. They are not a blanket fallback or global GC rollback guarantee.

The fifth diagnostic test appends to the unchanged old23 Token cases: header
`3C5D69AB3207B56ABF1E3611EBDA7D81E8BA8076EC6BEDA17CDCA4B9565598AB`,
runner `8CD43685383744F73E8F44B95817EC6F178905BE23E664BD1C7A4A384EA9F6C1`.
R20 unified61390 naturally exits0/drained with state
`DIAGNOSTIC_DUPLICATE_FUEL_RED_VERIFIED_WITH_EXISTING_326_GREEN_NO_PLATFORM_CREDIT`.
Receipt
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-diagnostic-duplicate-fuel-red-r20-receipt.json`,
SHA-256 `B573E7C5DAE288890EA67A2C54B5340392842E8164E690F4D29CF6E457F6EB9F`,
records normal14/default327 =326 PASS/one new FUEL FAIL/zero ignored. Query77,
ownership25 and LSP85 recover all old48 regressions; Token24 retains old23 PASS,
including the domain case with rejected/facts/usage/rootsA/rootsB/pins/input all1.

The single new duplicate call returns TRUE and preserves the complete fact/header/
related payload, caller input, roots, pins and non-FUEL usage. Actual FUEL107 to107
makes `fuel=0`, with all other printed predicates1. Its unique final reason is
`Successful diagnostic deduplication must charge fuel without changing publication or ownership`.
It occurs once after cleanup with no prerequisite failure. This is the successful
duplicate Abort branch's genuine behavior RED, not failure to publish an initial
complete fact. At the R20 endpoint the narrow CommitTransaction candidate is
applied but R21 all327 GREEN is pending; failure must keep the existing abort behavior.

R20 build31320 naturally exits0/9671ms with19 planned actions; raw
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-diagnostic-duplicate-fuel-red-r20-build.log`
has SHA-256 `41CBFED9CE09B6D0418B565407BB1A6D00C128C82F5BCD74035AB5F866E12114`.
Its three incremental D9025 do not erase R19 warnings or establish W0. Token52920
naturally exits1/124ms with24/1/0 footer; raw
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-diagnostic-duplicate-fuel-red-r20-token.log`
has SHA-256 `B19DDF2837C53EF702EEF9DB66BE8650C120E6359E36B9EA09E799C67A8BC109`.
Whole4826 identity is
`1689839A1AB66AF115B6079A5415753940952705B3236F643FD4CEB0EEE2721D`;
source-pre/post share `7321BD61F73D9F5935F216E9C9E8D5C805D9948B09371ABCE6C09553799C9319`.
HEAD `957a391b705f5052b8a92751e5cef64c571243ca`, support6 and binary14 are stable,
and all19 owned children naturally terminate. Controller0 is only registered RED
verification; all327 GREEN, complete related diagnostics, seven cross-AST carriers,
full Windows55/CLI/aux3+4/graph, validator, Linux GCC/Clang, M1/M2-M6 and milestone
commit remain unaccepted. Linux is UNPASSED, released bytes0 and lifecycle OPEN.

## Diagnostic Evidence R18 RED (2026-10-17)

This section retains the R18 snapshot; its then-current header and in-progress
production statements do not supersede the dated R19/R20 results above.

R18 unified35069 naturally exits0/drained with state
`DIAGNOSTIC_EVIDENCE_RED_VERIFIED_WITH_EXISTING_322_GREEN_NO_PLATFORM_CREDIT`.
Receipt
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-diagnostic-evidence-red-r18-receipt.json`,
SHA-256 `4944D10E7E9C45D3775D9AE6C87F69A4AF8D724E4DCBEB42E01C8B3CA7C4611F`,
records normal build0 and the same 14 targets/default325: 322 PASS/three exact
new diagnostic behavior FAIL/zero ignored. Token22 retains all original19 PASS
in order and appends only these three failures; LSP85 and every other suite pass.
All real parse, source/AST-owner/native-interval and ordinary-publication hard
prerequisites pass. These are final behavior REDs, not compile or fixture errors.

The new
[`test_token_source_diagnostic_evidence_cases.h`](../../tests/parser/test_token_source_diagnostic_evidence_cases.h)
covers actual composite fragments, interval-based deduplication and independent
HEAP/FUEL publication transactions. Its
[module contract](../parser-and-semantics/macro-token-foundation.md#diagnostic-evidence-r18-red)
separates the intended behavior from this executed result:

| New case | Actual R18 observation and exact final reason |
|---|---|
| `test_composite_diagnostic_preserves_all_physical_fragments` | Variants0/1/2 each append one fact but report `complete=0`: `Composite diagnostics must retain the first real anchor and every unique physical fragment`. |
| `test_diagnostic_dedup_uses_native_interval_identity` | Inner/outer/equivalent calls all append, but collapse to one fact: `Diagnostic deduplication must distinguish native intervals and ignore equivalent handles`. |
| `test_diagnostic_evidence_budget_failure_is_atomic` | Both HEAP3/FUEL1 report rejected/category/fact-state-unchanged false, roots/pins/usage unchanged true, complete retry false: `Diagnostic evidence budget failure must leave publication and ownership state unchanged and permit retry`. |

Each final reason appears exactly once in the immutable Token raw log. Its
`facts=0` budget field is the failed unchanged-state predicate, not zero stored
facts. The composite case's external-input release/full-GC branch is conditional
on complete publication and is not reached in R18; no new diagnostic lifetime
credit is inferred. The legacy node-NULL/unknown-range dedup control passes.

Build46368 naturally exits0 in 27490ms with three planned incremental actions.
Raw
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-diagnostic-evidence-red-r18-build.log`
has SHA-256 `B9DC8E820FB28A09004AEAE62EB122E47179DF97381F6CE5E4957821C16F6E9B`.
It retains one D9025 and new test-only C4090 at header64, not W0. Token native37268
exits3/583ms with footer22/3/0; raw
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-diagnostic-evidence-red-r18-token.log`
has SHA-256 `64449F7FFC64F0C7E2C7C3C5BFAAC94610290A64D0F1E5BB7666EEE974B875E0`.

The executed header SHA-256 is
`87408AB8E5B77E37B5AED566B6F4674D29FB8E1096EAEF9C88149E25C1D2464D`.
Root subsequently authorizes only the 17-byte explicit `TZrNativeString` cast;
current header SHA-256 is
`5079ED9D7BD553DB20BB4997EC7CD050C25E7A7852A4BCBB75DD41188945377E`.
Exact RAM removal of that one cast restores the R18 bytes/hash, so no test
assertions change. No later native result is credited to this adjustment, and the
R18 raw warning remains unchanged.

Whole4821 source input identity is
`EBCAB3616296477A0813172D456E9C5D795E461F1D69C1F0386070EFDA424D37`;
source-pre/post JSON share SHA-256
`797BB7403FECFA19B4FE2EB12754076931BCB68544C711239B1A61EB7769D258`.
HEAD `f67957d6eac42b4ed2132ab1fb5f10976fbbd23e`, support6 and binary14 are unchanged;
all 19 owned children naturally terminate, without a source/HEAD gate waiver.

Controller0 verifies only this registered genuine RED with the existing322 GREEN.
Private diagnostic production work is still in progress, with no frozen-delivery
or native GREEN claim. R17 and all earlier strict FAIL histories remain immutable.
No related-diagnostic completion, seven cross-AST carrier acceptance, full
Windows55, CLI, aux3+4, graph, validator, Linux GCC/Clang, M1/M2-M6 or milestone
commit credit follows. Linux remains UNPASSED, M1 unaccepted, M2-M6 pending,
released bytes 0 and lifecycle OPEN.

## AST Type Facts R16/R17 (2026-10-17)

R16 unified20293 naturally exits0/drained with state
`GENERIC_SIGNATURE_RANGE_RED_WITH_EVIDENCE_BOUNDARIES_VERIFIED_NO_PLATFORM_CREDIT`.
Receipt
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-generic-signature-range-red-r16-receipt.json`,
SHA-256 `3124C33A11482CE436454877728D58DB008293DA401CE926D12153D6472C54C4`,
records normal build0 and14 default suites/322 cases:319 PASS/three exact LSP
behavior FAIL/zero ignored. LSP85 retains the original84 calls in order and
appends `Semantic Analyzer Generic Signature Query Uses Exact Type Reference
Range`. Its real parse/shared snapshot/TYPE-node/resolved canonical identity/
format/exact basename source and coordinates/position-query prerequisites pass
before the sole final target reason: `Generic signatures must consume the
resolved TYPE fact for their exact source reference`. The two ownership/generic
signature failures from R15 remain explicit, giving82 PASS/three FAIL. R16 LSP
raw SHA-256 is `421F5B7900F634936C717D464EC6BFA55D79B5A33528DD6997A4F35D69D2E93F`.
Token19 and all other suites pass, including all eight empty-coordinate
mutations, sticky refusal and fresh-init retry. Controller0 validates the
registered bounded RED observation, not platform GREEN.

R16's full4820 input identity is
`24F093AC6E834BE80A04DAE1C7B09CE3EDF36660EB2702675AF1F0943495F3EC`;
source-pre/post JSON share SHA-256
`D7B2F5560F4B3BC975D00711F5D130AF994CE09A3C6C8638E99D84A084284559`.
HEAD `ba1b8df3b61430ad6030f09a4f490cf42cda04dd`, support6 and binary14 are
unchanged, and all19 children naturally terminate. Its19-step incremental build
retains three D9025, one C4267 and two C4701 warnings, with no W0 claim.

After that genuine RED, the one-path LSP formatter repair replaces obsolete
generic coordinate back-calculation with same-snapshot AST-node+TYPE lookup,
then resolved/canonical formatting checks. Its inference fallback clears the
borrowed fact pointer and freshly queries after temporary inferred-type cleanup.
It does not replace canonical facts with AST spelling or change position-query
navigation semantics. Normal type-use/prototype publishers enforce their own
identity/conflict checks; generic `AppendReference` gains no global conflict
guarantee. The
[module contract](../parser-and-semantics/macro-token-foundation.md#ast-identity-type-facts)
details these boundaries. The actual implementation is
`zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c`,
SHA-256 `21BCAD3C7A145441AD94D4F298F21ED39B32CC8394140B67430838B910393948`;
the new test header is
`A1E7ACB8A35BF5D9EDF4C7175029778A72D413A5D8A238C0F6DFD05840AF0B75`.

R17 unified72479 naturally exits0/drained. Receipt
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-ast-type-facts-green-r17-receipt.json`,
SHA-256 `98EBF3AB9A18A0B3F117CB7AF2949918B1303616041BCC44BE23E6A4A0792327`,
has state `AST_TYPE_FACTS_AND_RANGE_EVIDENCE_VERIFIED_NARROWLY_NO_PLATFORM_CREDIT`.
It runs from 09:57:26.453241Z to 09:59:07.767751Z on 2026-10-17. The same14 default
suites execute322 PASS/zero FAIL/zero ignored with unchanged ordered registrations:
query77, ownership25, AOT18, LSP85, guardrail6, facts20, SemIR4, Token19,
edges9, array4, collection5, metadata10, primary37 and peak3. All three R16 LSP
REDs now pass, including the appended exact-fact case and the original ownership/
generic-type cases. Token19 passes all old and new controls, including every
empty-coordinate variant, sticky refusal and fresh retry; no fixture failure is
reclassified or ignored.

Build10032 exits0 in 27894ms; its five planned incremental actions retain one
D9025, not W0 or a claim that historical warnings were fixed. Raw build
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-ast-type-facts-green-r17-build.log`
has SHA-256 `60FA318270B5D8873F02CAC1C724CF5C2FCB7DDBE01A28F9096B9B527913D30E`.
LSP native40036 exits0; its85-PASS raw log SHA-256 is
`41D78F5A8CE9A2231D38CF2D0BDECADF063DFC594B7E8ADB067CA1EC884F3701`.
Token native37400 exits0/19-0-0; raw SHA-256 is
`9220435A01F416871162DFE98381F04D245F3FDA4B8D3E8D03B7C635271216D3`.

R17 full4820 input identity is
`D3F236775160B624391D445042035890C3401AB6704023C445FF95F8F58BD1D7`;
source-pre/post JSON share SHA-256
`7EE39D94BD629B80ABC9B9288000ACDFA442E7C6213BAE25CEBB10CD7AB153E3`.
HEAD `ba1b8df3b61430ad6030f09a4f490cf42cda04dd`, support6 and binary14 remain
unchanged; all19 owned children naturally terminate. No source/HEAD gate is
waived. R14's compile FAIL and R15's unexpected-LSP strict FAIL stay immutable.
R17 grants only focused14/322 GREEN: related diagnostics/dedup, seven cross-AST
carriers, full Windows55, standalone CLI, outer aux3+4, graph tests, Linux
GCC/Clang, M1 acceptance, M2-M6 and milestone commit remain open. It does not
replace the earlier complete Windows55 result of53 PASS/two FAIL or its unrun
CLI/aux/source-final tail.

## Range Boundary R14/R15 FAIL (2026-10-17)

R14 unified44498 naturally exits1/drained with no native tests. Receipt
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-range-evidence-controls-red-r14-receipt.json`,
SHA-256 `423DA9DFB91603D2F6106A09F73D711D1BE0762A2006C2CD634F128931366760`,
retains build19380 exit1/C2198 at `test_parser_array_budget.c:128`: its old
`parse_embedded_expression` call lacks the newly required origin argument.
Only receipt/source-pre/source-post/build log are created, four of32 registered
outputs; the other28 never exist. There is no native behavior RED from R14.
Its build warnings remain641 D9025,22 C4701 and ten other warnings, not W0.

The bounded test-only migration passes the real parent CurrentLocation, keeps
the original four registrations/assertions and checks that exhausted outer heap,
not an evidence error, rejects the embedded parse. Its SHA-256 is
`F9D44FDDE46C04F35B582335C1FC5576DC0A463A25EE74C06E897F79221EFD65`;
Root's RAM inverse restores `1067F918F6F809E4A48E53E84A03483318D504C9B75A8CB0ED05536C8399BBF7`,
and independent static review accepts that limited change. R15's actual
array-budget native result is four PASS/zero FAIL/zero ignored. This does not
prove that the deeper nested-origin validation ran after the heap refusal.

R15 unified72421 naturally exits1/drained. Receipt
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-range-evidence-controls-red-r15-receipt.json`,
SHA-256 `DD1A82585B15BCC952A29B5182EED5394C18D08F74349630560B0D4D8345E687`,
is strictly FAIL. Normal build34656 exits0 in 28062ms; raw build log
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-range-evidence-controls-red-r15-build.log`
has SHA-256 `0C0F35800F334DFAD3F778D16D81EF31509631697B2417AB75704E3CE508B188`.
That successful build is not a W0 claim. All14 default suites run321 cases:
315 PASS/six FAIL/zero ignored, rather than the registered317 PASS/four expected
RED contract.

| Raw suite/group | Actual R15 result |
| --- | --- |
| Query77 | 77 PASS |
| Ownership25 | 25 PASS |
| AOT18 | 18 PASS |
| LSP84 | 82 PASS/two unexpected FAIL |
| Guardrail6, facts20, SemIR4 | All30 PASS |
| Token19 | 15 PASS/four expected final behavior FAIL |
| Edges9, array4, collection5, metadata10, primary37, peak3 | All68 PASS |

Token native46308 exits4. Raw
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-range-evidence-controls-red-r15-token.log`,
SHA-256 `0E5DF03D2A4C5395AF9C073F3826F7CDE9F39C1E2AADB71759F788427ADB076D`,
retains all original14 PASS, including the four old R13 parent REDs. Forward
multiline coordinates also pass. The new four fail at exactly:

- `Line-backtracking evidence must preserve the first anchor as composite`
- `Column-backtracking evidence must preserve the first anchor as composite`
- `Interior operator coordinate backtracking must remain composite`
- `Noncanonical empty-stream coordinates must fail closed`

Their real builder/Span/raw/context/AST/leaves/EOS prerequisites pass; the empty
case first passes its canonical zero-offset, line1/column1 merge. Unity stops
that case at the first invalid-coordinate assertion, so its complete eight-variant
coverage, cleared-output/sticky-failure/refused-parse/fresh-retry assertions are
not all executed or credited. The
[boundary contract](../parser-and-semantics/macro-token-foundation.md#evidence-boundary-controls)
describes those intended checks. Header
[`test_token_source_range_evidence_cases.h`](../../tests/parser/test_token_source_range_evidence_cases.h),
runner and CMake registration are pinned at
`1A35EF9C930A273CC0D71E1B7874B12BC77BE7097E3B5824465BDB74EB55A7FD`,
`AE4327CDFC9966DD9577C709A755F6D128832EAF4A197664F23CC02DE526CCA1` and
`B447B235D1DA164F3AB3F014E70600123989D463638F97493B80F3A174B7E316`.

LSP native10752 exits1. Raw
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-range-evidence-controls-red-r15-lsp.log`,
SHA-256 `A822CEE280259D17B7FF2EC44866CB820A2A969F9254430D7BA709B5BA049587`,
contains84 starts,82 PASS and two unexpected FAIL: `Semantic Analyzer Function
Signatures Preserve Ownership Qualifiers` shows `take(seed: cannot infer exact
type): cannot infer exact type`; `Semantic Analyzer Generic Type Symbols Surface
Signature Detail` shows an unresolved `Derived` base type. The controller rejects
these unexpected failures before publishing later structured suite checks.
The raw Token observations therefore do not make R15's registered RED contract
successful. No cause or additional production repair is established here.

Full4819 registered input identity is
`009AE7E62B361BAFAB7EF7354EF576133E8D2DBA978036CF07F32E8A6112FBDA`.
Source-pre and source-post JSON both have SHA-256
`02D16420D8AADE40556E757BBDC9247751922E5F5CC51F63CAC68A99FBB78DEE`;
HEAD `fc4b2fc4c8ab286052096bf417ca743cfef4a609`, support6 and binary14 are
unchanged. All19 owned children naturally terminate. R14/R15 do not rewrite
R10/R11/R12/R13 history or grant full Windows, Linux, M1, M2 or commit credit.
Linux remains UNPASSED, M1 unaccepted, M2-M6 pending. Related diagnostics/dedup,
seven cross-AST carriers and complete matrices remain open. At R15 the unexpected
LSP failures require lower-layer diagnosis; the later focused R16/R17 evidence
above does not upgrade that strict FAIL receipt.

## Composite Range RED R13 (2026-10-17)

Unified5874 naturally ends0/drained. Receipt
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-composite-range-red-r13-receipt.json`,
SHA-256 `E079DECFD1D31AD0FC3B57C1F3C022D59DD00254A638ACF69DB118107828AF2A`,
has state `COMPOSITE_PARENT_RANGES_RED_WITH_ORIGINAL_SPANS_VERIFIED_NO_PLATFORM_CREDIT`.
Normal build48936 exits0;14 default suites run316 cases,312 PASS/four precise
parent-range behavior FAIL/zero ignored. Query77, ownership25, AOT18, LSP84,
guardrail6, facts20, SemIR4 and the extra68 all pass. Token14 is10 PASS/four
FAIL: original mixed postfix plus operator-only foreign source, equal-name
distinct sources and reordered single-source tokens. The new real embedded
template case passes its whole-origin `7..15` assertion.

All new hard prerequisites pass: original token kind/raw/native payload,
Span/context/sourceIds and order before/after parsing, actual AST shape/operator/
values, leaf/CALL/following anchors and complete EOS consumption. Token native27004
exits4; raw
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-composite-range-red-r13-token.log`,
SHA-256 `5625FA5FC87E9BCD70B1A64AF2F701535C480F83E8958A7521EBD8737C890BE0`,
retains parent envelopes mixed `0..70`, operator-only `0..3`, same-name `0..70`
and reverse-spliced `0..4`, rather than their real first-token anchors.
Source4814 and HEAD `193f46439fa2025bc9eff4a50ef264bb30a2ea5f` are unchanged,
as are support6 and binary14; all19 children naturally terminate. This incremental
build has one D9025 warning, not W0. It does not replace R10's44 D9025, two C4701
and13 CMake warning history.

The new544-line header
[`test_token_source_composite_range_cases.h`](../../tests/parser/test_token_source_composite_range_cases.h)
is frozen at SHA-256
`AD58556B9824D55A833FB75E4C387DB58F85BDAA13F4AB0091DFB3DB6C69D6C5`;
runner `7CE5884B47E2502BDF5A8E17E8A4CE968530B583007CDC87A198BE3AB32F1B9E`
keeps the original ten in order and appends four. The two old headers are
unchanged. Root's input-map audit finds only the three new-header enum-name
corrections relative to actual R11 inputs; the other SSA edits already belonged
to that R11 input state, not an inferred new R13 repair.

Earlier attempts remain failures, not parent-range RED credit. R11 receipt
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-composite-range-red-r11-receipt.json`,
SHA-256 `5A6430F689D4FACA02C8250F9BD9AE39D7632B60E06833A162F185CF988390CB`,
fails compilation on three invalid enum uses (`ZR_AST_PRIMARY` and two
`ZR_AST_IDENTIFIER`), with zero native tests. R12 receipt
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-composite-range-red-r12-receipt.json`,
SHA-256 `51BFDDA8A12050C8F89E6C1734DC58C3FA3AF7EB50260CE3330C82AA0661CB9C`,
rejects the changed HEAD193f4643 during strict preflight: two HEAD children,
no build or tests. R13's new preregistered input does not rewrite either FAIL.

### Applied Implementation And Remaining Limits

This section records the R13-R17 implementation endpoint. The later R22/R23/R24
consumer checkpoint above supersedes its then-open carrier status.

After R13, the 16-file identity/evidence core is applied. R17's focused GREEN
above does not establish overall platform acceptance. The 64-byte `FileRange` ABI
contains `originKey`, `ownerKey`,
`evidenceIndex` and `flags`. Owner-held flat atoms and immutable prefix summaries
back immutable `(first,last)` interval records, not a merge DAG. The reviewed
checked-merge migration has 97 calls across 29 parser files, consolidating
98 former direct expressions. Nineteen old constructors initialize the new state.

The bounded same-anchor follow-up adds 14 `WithoutEvidence` resets across 13
files without changing their physical coordinate writes or source. These clear
owner/evidence handles and add PARTIAL, retaining origin and coordinates. Seven
cross-AST derived-range carriers across five other files remain unchanged and
pending separate policy/behavioral tests: handle invalidation alone does not
validate a cross-origin endpoint envelope. The
[module contract](../parser-and-semantics/macro-token-foundation.md#range-implementation-and-boundary-failures)
describes the implementation and this limit.

Related diagnostics/deduplication remain pending. The earlier all-316-PASS RAM
draft was syntax-checked only and never launched; it is not the registered
boundary-RED R14/R15 contract. The added coordinate/empty controls now have the
four narrow actual R15 behavior failures above, and unexpected LSP failures
keep that entire receipt FAIL. R16/R17 subsequently pass those Token controls;
R17 also passes the85-case TYPE-fact consumer suite. R13's312 PASS/four parent
RED and R10/R11/R12 history are not rewritten. No completed provenance, Windows, Linux or M1 PASS
follows from applied code or the narrow observations. Linux is UNPASSED,
M1 unaccepted, M2-M6 pending; no milestone commit credit.

## Token Projection Evidence (2026-10-17)

At R8/R10, `zr_vm_token_source_test` / `macro_token_source` has ten ordered cases:
the original seven unchanged, followed by the three cases in
[`test_token_source_projection_cases.h`](../../tests/parser/test_token_source_projection_cases.h).
Focused R8, unified handle32473, has naturally ended0/drained. Its normal build
exits0; token native18164 exits4 with six PASS/four exact final behavior FAIL/
zero ignored. Mixed postfix and all three appended cases fail, with no fixture
prerequisite failure. Full-GC count increase and both 8 KiB healthy short-name
controls, including the fresh retry, pass before their final assertions.

| Appended case | Unchanged contract: R8 behavior RED, R10 focused PASS |
| --- | --- |
| `test_token_source_projection_is_pure_across_lookahead` | Entry name stays unchanged; repeated lookahead preserves the current original Span and caches the next source's Span. Current locations select the real source name and coordinates before lookahead, after lookahead and after consumption. |
| `test_token_source_projection_names_survive_ast_owner_and_gc` | After parser and external stream/context/name-lease release, the AST alone retains ownership. Two full-GC requests must increase `fullCollectionCount` before target/argument/literal names and coordinates are checked. A failed GC prerequisite is not behavior RED. |
| `test_token_source_projection_name_budget_fails_closed_and_retries` | A retained 64 KiB name under an 8 KiB parser heap budget fails during Init with parser/lexer error, `HEAP_BYTES`, limit8192, requested usage above8192, committed usage at most8192 and no AST. Init state is snapshotted before Parse; healthy short-name controls must parse before and after the attempt. |

The name-budget fixture constructs native streams without that parser budget;
it does not establish a global allocation-failure or OOM guarantee. Fixture
cleanup precedes VM teardown, including Unity assertion-jump cleanup. The
[module contract](../parser-and-semantics/macro-token-foundation.md) keeps these
dated focused results separate from full provenance and milestone acceptance.

Actual receipt
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-projection-name-red-r8-receipt.json`,
SHA-256 `E7821B4A615AB80A344B663EF5C9C66FD67BB66371A833D95F6CA12D4973665F`,
has state `LOWER_DA_AOT_VERIFIED_PROJECTION_NAME_CONTROLS_RED_NO_PLATFORM_CREDIT`.
Query77, ownership25, AOT18, LSP84, guardrail6, facts20 and SemIR4 all pass on this
dated input. Whole-source4811 and HEAD
`83e88fad7c70876c63b1fca86457259ba300d647` are unchanged, as are support6 and
binary8; all13 owned children naturally terminate. The build retains two D9025
`/W3` to `/W4` override warnings, so there is no W0 claim. Controller exit0
confirms the controlled expected-RED protocol, not all-suite behavior PASS.

Historical R7 remains strict FAIL. Receipt
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-da-aot-repair-r7-receipt.json`, SHA-256
`985AF370C14738D6AD637272505A4DC3348F8A7E9A1898E1063C3B1CE9229B80`,
records token7 as six PASS/one mixed-postfix behavior FAIL: original Span and
parse prerequisites pass, but all five anchor checks are false and the parent
range is the fabricated `0..70`. Its separate explicit-OBJECT fixture failure
also remains in that receipt; no later correction changes the historical FAIL.
The approved original-Span/AST-identity/cross-source-anchor policy is still
unverified. No Windows, Linux, full M1 or commit credit follows from the new
registrations or controlled R8 result.

### Cache Implementation And R10

The six-file implementation is STOPPED; normal R10 compilation/native results
below follow its static review, without reopening the broader provenance scope.
The private `parser_source_projection.c/.h` owns the stream-local sourceId/name
table and parser projection APIs. `token_source.c` prewarms item/EOS names before
cursor publication; table lookups do not allocate or mutate the lexer entry
`sourceName` during lookahead. Budgeted VM names are rooted/pinned by the shared
AST owner, while cursor teardown releases the native table. Empty sourced EOS,
source-less empty EOS and a valid zero-length source name remain distinct
boundaries. The detailed
[cache/lifetime contract](../parser-and-semantics/macro-token-foundation.md)
describes these paths and their limits. Root's exact RAM inverse of the four
existing files checks preservation of their original bytes only. R9's launcher
fails with SyntaxError before CMake/native execution; it supplies no native
test result and remains separately recorded in the artifact ledger.

Actual R10 unified14251 naturally ends0/drained. Receipt
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-projection-cache-r10-receipt.json`,
SHA-256 `72EBF180A0E7E9BACF8DCE9D10465A22997CF4B65D24F683D859484C24A572A5`,
has state `PROJECTION_SOURCE_NAMES_VERIFIED_MIXED_PARENT_RANGE_RED_NO_PLATFORM_CREDIT`.
Normal build35472 exits0. All14 default suites run312 cases:311 PASS, only the
old mixed-postfix parent behavior FAIL, zero ignored. The original seven
neighboring suites pass all234 cases; token10 has nine PASS/one FAIL, including
PASS for the three new projection contracts. Extra68 also all pass: edges9,
array4, collection5, metadata10, primary37 and peak3. GC-count, owner-only and
short-name/64 KiB-name/8 KiB-Init/fresh-retry prerequisites remain intact.

Token native40500 exits1. Its raw log
`E:/cargo-targets/zr_vm/reports/m1-msvc-r11-projection-cache-r10-token.log`,
SHA-256 `1A9E312CD22B55778953139C66A408B153EBDC8CFDDC47B13652042F7A4EB230`,
prints target=1, call=1, argument=1, parent=0, following=1 and
`parentRange=0..70`. Source4813 and HEAD
`83e88fad7c70876c63b1fca86457259ba300d647` are unchanged, as are support6 and
binary14; all19 children naturally terminate. The build retains44 D9025,
two C4701 and CMake warnings, so there is no W0 claim. Controller exit0 accepts
the controlled expected-RED protocol, not all-suite behavior PASS.

R8's four behavior failures and R7's strict FAIL remain historical facts. R10
closes only the focused source-name/lifetime/budget assertions; it does not
prove every empty-name combination or full provenance. Merge/FileRange ABI and
composite-origin work are outside this cache implementation; cross-source
parent-range, full Windows, Linux, M1 and commit credit remain unavailable.

## Latest Windows R10 Result (2026-10-17)

This is the retained dated full-matrix R10 result. Later focused checkpoints do
not reclassify it or accept the current static 280-target/65-CTest inventory.

The current-input r10 attempt has naturally ended, not passed. Foundation52,
lower323/original127, budget11 and the original LSP semantic-analyzer84 cases
passed on that dated input. Full CTest37444 exited8:52 PASS/3 FAIL of55.
Failures were language_pipeline (AOT source26/5), language_server (ownership25/13)
and cli_project_incremental (12/1). Standalone CLI, auxiliary3+4 and source-final
were not reached. Receipt24E06251 also rejects one concurrent source change,
`tests/cmake/ssa-gc-map-metadata-consumers.cmake`; HEAD-post was not reached.
Outer6AD77864 confirms all owned children terminal and no fixed/extra leftovers.

The four incidental AOT self-filename checks and obsolete unconditional
begin-instruction ban were corrected without changing production AOT code.
[Focused AOT source verification](aot-c-source-contracts.md) passed the original
26 cases with stable source/HEAD/tool/binary pins; this does not reclassify the
failed full matrix. LSP ownership and CLI output failures are under focused
diagnosis. Linux remains unpassed, M1 unaccepted/uncommitted, M2-M6 pending.
Released0B; the reused build and necessary evidence remain retained.

## Current Platform Gate (2026-10-16)

This section preserves the historical R8-R10 gate inventory and execution order.
The R28 checkpoint above states the current inventory and open gates.

The user explicitly selected independent Windows validation first after WSL
startup failures. Linux GCC and Clang acceptance remains unpassed; Windows
evidence does not replace either gate. Parallel foreign edits are permitted,
but source, HEAD, support and binary gates remain strict.

Windows r8 foundation CTest passed all 52 entries with their original timeouts.
The subsequent full build naturally failed on two C1083 generated-file errors;
the raw receipt `m1-msvc-current-r8-receipt.json` (SHA-256
`FA26E2CCF17C02EDCAEBCFBD307D538C6C288FB77BEC3F4CDB8ABE1BE95D1737`)
also preserves foreign source-post drift. Lower323, original127, full55,
declaration-budget11, original semantic-analyzer84 and CLI were not executed.
HEAD/support post were not reached; no missing verification is credited.

The full selected inventory is original51 plus three aggregates and the separate
declaration-budget CTest, or 55 entries. New standalone build-graph checks do
not replace or reduce those entries. The
[Windows object-path follow-up](windows-test-build-paths.md) records the actual
six-path RED and bounded internal target renaming. The
[bundled libffi contract](../library-and-builtins/bundled-libffi-build.md) records
actual dependency RED-to-GREEN, canonical preprocessing and standalone CTest1/1.
Neither graph check establishes full-build, cache, platform or M1 acceptance.

### Retained Windows r9 Evidence

R9 foundation CTest passes all 52 entries. Normal full-build PID 33132 naturally
exits 0 with 956 planned actions and no `FAILED:` rows. The typed-call native
target passes, but the next yield target exits `3221225477` (`0xC0000005`).
Source-post detects 12 changed inputs; HEAD/support post are not reached.
The immutable receipt `m1-msvc-current-r9-receipt.json`, SHA-256
`9D06E96E01D9CFCA76C48B84A9D11699AA85A7F06F0B10E76E548DB308BB8B1B`,
therefore remains raw FAIL. Remaining lower targets, original127, full55,
declaration-budget11, original semantic-analyzer84 and CLI were not executed in
R9. The earlier R8 failure and all Linux failures remain retained history.

The bounded typed-cache audit is separately verified: receipt
`E:/cargo-targets/zr_vm/reports/m1-msvc-cache-post-r9.json`, SHA-256
`D4F8443A9E0C672F59D1C81F9D8BD41E0A11568F7F5B34921FADAAC8A75398E5`.
All selected 119 C plus one MASM objects have unique actual successful r9
compile edges: foundation106 C + one MASM, full13 C. The C records are 118
positive `VALID` header closures and the exact 84-byte tinydir placeholder's
`VALID 0`; the MASM evidence uses its separate nine-input graph closure.
All 627 C dependency pins, 6 metadata, 23 support, 127 source and 120 object
records match pre/post. Five inspection children naturally exit 0; the three
symbol checks each show one `UNDEF` reference and no external definition.
This is selected-object cache evidence, not a full-tree cache or platform PASS.

Normal standalone graph CTest also passes exactly 2/2: object-path validation
audits 6,828 paths with maximum 258 characters, and bundled FFI validation finds
all nine inputs. All 20 recorded inputs remain unchanged. Receipt
`E:/cargo-targets/zr_vm/reports/m1-msvc-build-graph-ctest-r2-receipt.json` has
SHA-256 `D563475EF1AD7F794D8D9CE41620E467E48B30B616561DDB59ECA5B1383AAB79`.
These two graph checks are not members of, or substitutes for, selected full55.

Debug r3 reproduces the same R9 access violation: debugger10912 and
symbolizer22456 naturally exit 0; inferior14856 naturally exits `0xC0000005`.
Actual module/PDB mapping locates the fault at `ZrCore_GlobalState_Free`,
`zr_vm_core/src/zr_vm_core/runtime/global.c:734`. Receipt
`E:/cargo-targets/zr_vm/reports/m1-msvc-yield-debug-r3-receipt.json` has SHA-256
`4ED01C2DBB1D06ADDF455220FF870360F4AB29A88E4251D6DAE102F895FBFD9A`;
its raw log has SHA-256
`CE696047C102B34EAD9581CAF68E14A4A5D14B9F67BA985D7F771ED6638E01A1`.
`set stop-on-solib-events` did not produce the planned early stop, so those
early breakpoints and lifecycle observations were not obtained. Neither this
mapping nor the raw-stack candidates establish a GC or lifecycle root cause.

Separate read-only PDB inspection finds incompatible layout pairs:
`State.global`184/208, `RawObject`184/208 and string payload200/224. Actual
`DestroyState` machine code reads state+`0xB8` (184); its fixture object has
Ninja `#deps 0 (VALID)` and is outside the audited 120 objects. Mixed ABI and
that missing header closure were observed at this checkpoint; the linked
stale-COMDAT cause is established by the later evidence below. The selected
120-object cache PASS cannot be extrapolated to the full build tree.

### Current Zero-Dependency Repair And r10

Actual machine-code/PDB inspection establishes a linked mixed-ABI cause, not a
GC source-level lifecycle conclusion from debug r3's missing early stops.
Current `State_Init` writes global+`0xD0` (208), while stale fixture
`DestroyState` reads+`0xB8` (184). `current_identifier_equals` calls the old
linked string getter, which reads length+`0xC0`/payload+`0xC8`; the actual current
string creator writes length+`0xD8`/payload+`0xE0`. PDB module1 attributes that
linked COMDAT to old `container_test_common.c.obj`, while the parser's own
getter already uses the new layout. These are concrete incompatible linked
accesses; the prior selected120 audit did not cover this stale fixture.

The first seven-object repair attempt r1 naturally builds successfully
(PID6756/exit0; seven fresh selected rows), but its saved Ninja graph still has
all seven as `VALID 0`. It stops before native tests and remains FAIL:
`m1-msvc-zero-deps-r1-receipt.json`, SHA-256
`0D4D43F9AEC2826E6055AF5C99BE7D61BD1423716B0CA49F90860880B15CACF8`.
Its source4788 inventories and HEAD match; successful compilation did not prove
correct header tracking. Native UTF-8 Chinese include-prefix output did not
match the configured prefix, and later language initialization could overwrite
the prefix setting.

The narrow local repair leaves the actual compiler command, arguments and
native exit intact. It normalizes only line-start showIncludes prefixes to
ASCII `Note: including file: ` and retains paths/indentation/unrelated output.
The local `CMAKE_PROJECT_INCLUDE` hook sets matching prefixes immediately and
through a root-directory deferred macro. Helpers
`m1-msvc-utf8-launcher.ps1` SHA-256
`F76CEF63A808397E3FDEF66982074BBA0A4E6E3B52A6F4FAAA802FAFF14D4EC3`
and `m1-msvc-localized-deps.cmake` SHA-256
`4668B9386A4F99AD0C33365CCA09051622240E2F3A718825A05EF39D2A83CD19`
have narrow independent core review acceptance and five passing actual RAM AST
prefix controls. Those controls alone carry no compiler or platform credit.

Normal same-tree r2 then produces exactly seven fresh successful selected
compile rows and seven positive `VALID` closures, with dependency counts
197/223/193/241/240/237/232. Its saved full existing-C graph has2,474 positive
`VALID` records and only the exact84B headerless tinydir placeholder at zero;
4,350 absent C objects gain no execution credit. The four original native runs
complete in source registration order: yield7, binary metadata3, import
ownership4 and compiler integration127, all PASS/zero failures/zero ignored,
each with its unique Unity footer and `OK`. Selection/invalidation pins match,
the operation preserves bytes and deletes nothing, eight support pins remain
stable, and all12 children naturally terminate with exit0.

The immutable receipt
`E:/cargo-targets/zr_vm/reports/m1-msvc-zero-deps-r2-receipt.json`, SHA-256
`558DFD252894A57C84E4ED689BFDE7B72AEB7C75EA3D9BA1F4E2E31FB8C53674`,
still has only `StrictInputGate` failure: source4788 becomes4789 with six changed
foreign ExecIR paths, and HEAD changes from
`e7c7cde99c69f6057e43187c386c18df4d15023c` to
`0f889998976cca908404c0cd3e20ecc8250a7362`. The 141 passes are dated functional
evidence, not current-input platform/M1 acceptance; R9 and r1 failures remain
immutable.

At this historical checkpoint r10-v2 was running (unified68201, driver22008), using unchanged
driver8AC8E769 with only its preregistered two-literal RAM AST mapping. Source-pre
PID34448 exits0 with4,789 inputs; tools14988 and foundation-configure37060 exit0.
Foundation-build28240 is in progress with693 planned actions, without completion
credit. The original foundation52/full55/lower14-and323/original127/budget11/
semantic-analyzer84/CLI gates remain intact. After the original driver passes,
normal auxiliary two-target build/native3+4 and source-final must compare against
the original r10 source-pre plus the same HEAD, with helper/binary identities
also strict. No full matrix or r10 result is yet credited; user-authorized
parallel editing does not weaken any gate.

M1 remains open and uncommitted; M2-M6 remain pending. Sections below are dated
history and must not override this current execution order or actual gate state.

## Historical Lower-Contract Gate (2026-10-15)

Receiver-setup production has Root/core SOURCE PASS at
`4C5B070D1ACF444D8E75DCF5EAC271F8F3805C532D3F4C64C68F0A460978B6F2`
(519729 bytes, 11076 LF, zero CR). Nine finite edits add 1085 bytes/21 LF,
extending the existing helper from 6034 to 7119 bytes. Complete RAM inverse and
finite-forward byte equality pass. Adjacent LOAD+STAGE with an optional reload
is accepted without changing the existing liveness, named-slot alias, native
provider or burst eligibility guards.

The controlled prepatch public 22-case RED-r1 completes 22/1/0: build PID 53024
exits 0, native 40448 exits 1 and wrapper 36652 exits 1, with all 100 source pins
and HEAD stable. Only the four-array FILL assertion fails. The real live native
null-return case reports PASS and FINE/integer 14. Its ADD at instruction 14 has
operandExtra 2; instruction 15 has opcode 121 and operandExtra 2. This records
the observed case without authorizing a new null-write transformation.

Fresh lower-r7 completes fourteen targets and 322 cases: 321 PASS, one failure,
zero ignored. Build PID 30324 exits 0; wrapper 52372 exits 1. All 100 source pins
and HEAD are stable. The thirteen non-W2 targets pass 300 cases, including escape
metadata PID 47384 at 14/0. W2 PID 48160 reports 22/1; the live-null-result case
still reports PASS/integer 14. The final build log SHA256 is
`A69FE33836F806CFC9F1619F96FC10DA486806F4AC5CE656DC42DD7FD2B9FA4A`.
The W2 binary and log SHA256 are
`62B6383E6B1B392052C06BB5C24FE95AE387C5292E668AF1AED8D9BB18CD2560` and
`6493FA514C0AEB881384B97C616616509E9C8E0220D085AAA02233EB0626527E`.

All four ADDs now discard their result with USE_RET 65535, but gaps remain and
FILL count is zero. The new diagnostic accesses operand1[2]/[3] beyond the real
two-element array. Those out-of-bounds values cannot establish the gap opcode
or alias facts. Root will correct the diagnostic and obtain a fresh 22-case
receipt; the actual opcode remains under Root's LLDB investigation. No final
W2 GREEN or later null-write/FILL production fix is credited. Any additional
null write must independently satisfy dead-temp and named-slot alias checks.

Original127-r2 and original51-r5 have not started. The earlier original127
120/7, lower-r5 286/0 and lower-r6 320/1 remain retained history, not post-4C
whole-project acceptance. Original51's 50/1 and 34/17-timeout receipts remain
open, as does WSL after the real E_ACCESSDENIED probe. M1 is unaccepted;
M2-M6 are pending and no Root milestone commit has occurred.

### Retained Lower-r6 Checkpoint

Fresh lower-r6 (2026-10-15) completes 14 native targets and 321 cases:
320 PASS, one failure and zero ignored. Build PID 30104 exits 0; wrapper 48876
(session 38138) exits 1. All 100 source pins and HEAD remain stable, registration
names/order/unique rows match, and every binary is stable. The prior twelve
lower targets retain 286/0/0. Escape metadata PID 44552 exits 0 with 14/0;
W2 PID 46832 exits 1 with 21/1, preserving its old 20 passes.
The final build log SHA256 is
`5FB55E56F9472B199DD8A6F3ADA24F750023D5FE85839267F38040D24A34993F`.
The escape log and binary SHA256 are
`D551341C47A2311C3C4B152BFEDDFFA859367E5C3DC0C005ADD8BCCEDDB95DF8` and
`BDC95060D0E590FD4B73C333F20A19FD65224832E002D5E1D662322428B3B978`.
The W2 log and binary SHA256 are
`497BE1C486331169ED2E286F6C40234CDD4EDF1C9753AFC8A2002B68A475D1BA` and
`68BAB5197DA8DCA91AAACD31D0EE8540CAD44A82BD227508B705EAA67335841E`.

The source-reviewed nominal14 and W2-21 candidates first produce RED-r2 35/5/0:
escape PID 47624 is 14/4 and W2 PID 47416 is 21/1. Build PID 7008 exits 0;
wrapper 27708 exits 1. Exact rows, all 100 source pins and HEAD remain stable.
The final build log SHA256 is
`73BAFB4FB5F218D554890E3AF1EF77F1EB1C0F4E2BDBF2119919D2B9578EC3B6`.
The new nominal child first passes its identity/parent-DOUBLE prerequisites,
then expects OBJECT 18 and receives DOUBLE 11. The W2 source reaches FINE/int0
before failing its final FILL assertion.

Production 475D8D8CFD3E6EF26414849DB27DE42DBB4BA43C0152F2B5CACD76B4C82AF914
(115687 bytes, 2747 LF, zero CR) adds 1712 ASCII bytes/36 LF to 37A70BA7.
Root/peer whole-RAM inverse and finite-forward byte equality pass. The 30-line
helper accepts only a bare NOMINAL/module-NULL node and the same context's exact
TypeId/name/definition-token inferred OBJECT record, with no generic elements,
ownership, GC bridge, reference access, readonly or nullable qualifier. The
six-line hook uses the existing converter after primitive projection and before
name lookup, without allocation or interning. Unmatched shapes retain fallback.
Escape14 now confirms child OBJECT 18/TypeId 23/name CallbackHandle and parent
DOUBLE 11/TypeId 18; seed/capture flags are 258 and the native flag is 256.

The sole lower-r6 failure is
`test_w2_public_four_array_zero_fill_loop_fuses`: real execution is FINE/int0,
but FILL is absent. SUPER_ARRAY_ADD_INT sites 42/44/46/48 have operandExtra 12;
the burst guard expects USE_RET 65535. This observed boundary remains under
diagnosis; no source fix or native-contract relaxation is credited.

Historical lower-r5 (2026-10-15) completes driver exit 0 and build exit 0. All twelve native
targets exit naturally 0 and pass 286 tests/zero failures/zero ignored, with
exact unique rows and expected footer counts. This includes lambda 10/0,
pre-semantic IR 121/0 and alias hygiene 15/0. Every binary, all 76 source pins
and HEAD `af5f4a7bc7de7d22ba545e91dffd982c2d3454bf` remain stable. The final
build log SHA256 is
`9819DB7BFEE856C4986E0C7E9D0E3D1F16E5B25C83C557AC14D209BD5B24872B`.
The lambda log is
`0A1A79D4832B2551E8F78EF3986427B13165145A21FA1541D728A8B78B10B92C`
and its binary is
`AFE16F89DAB1E06D8259C8BD31A4502F34A50D2F03E9C347960B909BC519F2A2`.

Lower-r4 had already passed all 286 native cases, but its driver exited 1
because `matrix4x4.h` and `matrix4x4_registry.c` changed during that run.
The original 69 contract pins and metadata/common/canonical sources remained
stable; those facts did not establish all 76 source-byte identities. R5 is a
separate rebuilt validation, rather than a relabeling of the r4 driver result.
A separate stdin raw-lexer comparison of four current Math files against HEAD
passes, with all eight invocations exiting 0 and stable hashes. Its proof SHA256
is `006F7172432A5CAFA26461CC3070A98A4AAD329D056BDC6DBD78F5A48E11FB54`.

The historical twelve-target lower-r3 replay completes 285 tests, two failures
and zero ignored. All original nine lower targets pass 140/140, alias hygiene
passes 15/15 and pre-semantic IR passes 121/121. Lambda isolation completes 9/2:
the actual child integer and double typed-local references fall back to OBJECT
after the block owner restores the parent environment. All 69 contract
source/launcher pins and HEAD remain stable in that replay, and every build and
native process exits naturally. Build log FD9CDF36 and lambda log 1F2EEADF retain
the complete rows and expected-count footers.

The appended same-name regression normally compiles a parent DOUBLE `value`
and a child INT64 `value`. Historical RED-r3 completes 10 unique cases, three failures
and zero ignored, with native exit 3. The original two failures expect bases
5/11 and receive OBJECT 18. The new case first passes all actual public identity,
canonical type, declaration/place and parent IR/binding prerequisites, then
expects child INT64 5 and receives parent DOUBLE 11. Its binary is 9D90E6F1,
native log 6AE1AB6E and final build log 2228A5EC; full identities are recorded in
`E:/cargo-targets/zr_vm/reports/macro-tokenstream-20261008/artifacts.json`.

The 69 frozen contract pins remain stable during that RED. Three foreign Math
comment files change concurrently among the broader 75 recorded pins, so the
run does not establish that all 75 are unchanged. A foreign missing `kFields`
declaration is separately restored as a build dependency repair: independent
review verifies 45 ASCII bytes and one CR while every other byte is preserved.
Neither that repair nor the concurrent comments is credited to the M1 metadata
change.

The earlier primitive baseline has Root/peer approval at
`37A70BA70E00BD422355B063EA64C30220CA414B54AB6B598CEDB42FE9030F3C`
(113975 bytes, 2711 LF, zero CR). Its 16 LF/776 ASCII bytes prioritize the actual
semantic TypeId after injected receivers and before name lookup, projecting
only canonical PRIMITIVE NULL/BOOL/NUMBER/STRING with pure lookup and no
allocation or interning. Every other kind and fallback remains unchanged.
Complete whole-RAM inverse and finite-forward byte equality pass. The stable
lower-r5 replay establishes native GREEN for those twelve lower targets.
See [typed local scalar metadata](../parser-and-semantics/lambda-semantic-ir-isolation.md#typed-local-scalar-metadata-2026-10-15).

The two block hooks already have Root/peer SOURCE PASS and complete inverse
proofs. Typed 24/24 confirms normal/error restoration and strict arity rejection.
The historical original127 replay before 475D completes 127 unique registrations:
120 PASS/seven failures/zero ignored, native PID 22632 exit 7, with all 93 source
pins and HEAD stable. The complete log is
`E:/cargo-targets/zr_vm/reports/m1-msvc-original127-after-lower-r1-native.log`,
SHA256 `A560D84400F600CEADB298F663872816DEED4DFC69E4C137C6A01BFBA63573A1`.
All seven failures persist from the complete earlier baselines, with the same
assertion lines/messages and no new failure. Relative to the complete 12-failure
baseline, generator, binary frame layout, network loopback and both LSP cases
change to PASS; the two 13-failure baselines additionally resolve decorator.
Generator is counted once, using its standard current row or the unique custom
failure only when the earlier standard row is absent.

The historical support33 RED-r1 completes 30 PASS/three failures/zero ignored:
escape metadata 13/3 and W2 20/0. Its log prefix is
`m1-msvc-native-callback-arrayfill-support-red-r1`. Original127 has not run after
475D; its historical seven-failure record remains the latest whole-project receipt.
Original51's 34PASS/17TIMEOUT r4 and earlier 50PASS/1TIMEOUT records remain open.
The latest real WSL true probe PID 19852 exits -1 with E_ACCESSDENIED; no GCC or
Clang run receives acceptance credit. M1 is unaccepted; M2-M6 and the milestone
commit have not started.

## Historical Lower-Contract Follow-Up (2026-10-14)

The nine lower targets complete build0 and138 actual cases/2 failures/0ignored.
Eight whole targets pass116/116, including receiver31/31 and its new source/callee
layout rejection cases. Typed22/2 preserves old18PASS and two new public query
cases; only new shadow/arity fixture defects fail. All17 pins and HEAD614c5776
stay stable. The driver incorrectly rejected a trailing space in Unity's footer;
independent parsing of all complete raw logs establishes these actual counts.

Only the new semantic include has a finite fixture correction lease: semicolon,
actual block expression wrapper, and removal of a later statement that overwrites
the intended structured diagnostic. Production, assertions and cleanup remain
frozen. Root/peer source review and affected typed22 rerun precede original127;
the other eight unchanged targets retain their116 complete passes. M1, original51
and platform acceptance remain open.
The terminal four-file probe cleanup releases105867B; cumulative measured
release1223815B, with required diagnostic logs and reuse build preserved.
The older counts and pending-run descriptions below are historical evidence.

The fresh scalar result repair completes its focused receiver run: build0,
29 tests/one failure/zero ignored and natural exit1. It removes the previous
CRT assertion; the sole remaining case is borrowed readonly in/ref returning
pointer payload instead of14. The opt-in Z7 launcher works in the same build,
with linked PDB data; no installed toolchain files have changed.

Both next source repairs have author STOP plus Root and independent peer review:
borrowed receiver flags/storage checks in two files and canonical member-callable
variable refinement before IR publication in three files. Full RAM inverses
restore all five registered PRE hashes. Native validation of those repairs is
pending after a pre-build compiler-path identity check; that stopped attempt
created no new build/native log and executed no native test. The older counts
and pending-probe wording below remain historical observations.

M1 remains active and unaccepted. After the source-reviewed METHOD receiver
effect projection, eight complete native targets report103 tests/one failure,
seven whole targets GREEN. The receiver executable instead reaches a CRT integer
assertion and is explicitly stopped; its short log earns no29-case completion.
The cyclic typed18/1 failure remains. Source27 pins are unchanged within that
run while foreign HEAD advances from a9a321c4 to de7b26df; Root makes no Git writes.

Owned LLDB launch/source debugging now succeeds despite the separate historical
attach denial. Fresh observations locate two receiver producer errors: an integer
member result retains a nominal INLINE_STRUCT row and loses scalar return
delivery; borrowed alias storage flags19 become9 and the field reads binding
bytes. A one-condition fresh-result repair is source-reviewed; its initial
focused build fails MSVC C1902 before native. Tiny probes reproduce C1902 with
Zi, pass Z7 compilation, and pass DEBUG:FULL linking with a PDB. The task-local
launcher now has an explicit environment opt-in for Z7; installed VS files are
unchanged. Fresh focused receiver runtime validation is pending.

Cyclic source debugging proves helper guard/intern FUNCTION3 and initializer
fact FUNCTION3, then local callback binding PRIMITIVE/CLOSURE4. A reviewed
structural refinement before IR-local creation remains pending; zero target
tokens/hash are not waived. See the receiver and typed-call module documents
for the observable flow and the artifact manifest for full log hashes.

Original127/original51 and WSL GCC/Clang remain open; M2-M6 and the milestone
commit have not started. Cumulative measured cleanup remains1117948B. Required
failure evidence, reusable build, shared caches and foreign/unknown products
are retained; the task manifest remains active.

This document is the M1 validation ledger for the macro token/input,
structured-tree, hygiene, resource-accounting, and comptime-control-flow
foundation. The authoritative design is
`docs/plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md`.
M1 is active and has not passed its acceptance gate. A row marked proposed is a
test contract, not evidence that the implementation exists or passes.

## Native Lower-Layer Evidence (2026-10-14)

The imported-return regression builds successfully and reports 16 tests, one
failure and zero ignored on MSVC. Its original twelve cases and nominal,
closed-generic and `u8` controls pass. The primitive consumer requires signature
hash `16702598370128743575`; its live provider has `12842983158373519980`,
exactly the pair from the decorator integration failure. See
[Typed Function Value Call Binding](../core-runtime/typed-call-binding.md#imported-primitive-return-regression-2026-10-14)
for the actual provider, alias, signature-row and runtime checks. The reviewed
27-line primitive projection repair then passes all 61 cases in five focused
targets: typed calls 16, return inference 14, pipeline 18, artifact 8 and AOT
projection 5. It preserves display names, explicit widths and the existing
complex-type inference path. A subsequent typed suite passes all seventeen
cases, including an independent builder/fresh-consumer binary provider. Its
bytes-only loader must successfully load binary metadata on the natural import
compile-info stack during manual compilation; live canonical/signature checks
and runtime result 17 also pass. The generated fixture is absent after cleanup.
This adds specific IO import evidence. The natural cyclic fixture subsequently
reports 18 tests, one failure and zero ignored, with the original seventeen
cases passing. Its actual returned prescan function has zero CALL/typed caches;
runtime and consumer functions each have one of both and execute to 17. The
diagnostic log is `m1-msvc-imported-cyclic-return-diagnostic-r2-zr_vm_typed_call_binding_test.log`,
SHA256 `5D9E0E10E758E50EEB9DAA23B6380C265D2E8B5C1C60700AAE62152F19889E7F`,
2782 bytes. The source routing candidate preserves native provider priority and
phase checks while allowing an existing source prescan on an import backedge
to reach the original summary registration path. It has no native credit yet.

The pure provider diagnostic now accepts native-tagged GC strings as field
keys, while retaining all native-function exclusions. The fresh original
integration run reports 127 tests, thirteen failures and zero ignored; all
126 standard outcomes and messages match the preceding run, plus the custom
generator failure. All 31 cached module rows expose valid provider functions.
The network `lib`/`Record` and both LSP `oop_meta`/`SignalBox` witnesses have a
unique callable owner and a matching `call` in consistent meta slot 15, with
zero ordinary descriptor method matches. Cached provider rows do not establish
the failed consumer's module identity or select its target. A subsequent real
import-link fixture compiles a `Callable` with `@call(int)`, imports an instance
and verifies the exact consumer/provider module signature, callable signature,
unique owner, serialized layout and runtime meta slot. `LinkImportedModule`
then returns `TARGET_NOT_FOUND` with that same consumer's token/instruction;
the original five module cases pass (six tests, one failure, zero ignored).
This establishes the descriptor-only imported-owner failure on a genuine META
contract. Four real-provider negative groups then bring the suite to ten cases,
five failures and zero ignored; the original five still pass. The old linker
returns target-not-found for wrong meta type, two consistent META slots sharing
one function, a compiler-produced static CALL and a signature mismatch. A
missing CALL slot already reports target-not-found correctly. Later receiver
checks require successful linking and have not yet been reached. The minimal
production patch has independent whole-source/inverse reviews. Six focused
suites then pass all 54 cases with zero ignored: module 10, runtime 8, pipeline
18, relocation 5, artifact 8 and AOT projection 5. All real-provider negative
statuses, receiver rejection, generation rejection, restored preparation and
result 42 are reached. Two bounded enum casts remove the new comparison
warnings, followed by another ten-case module pass. See
[Imported META owner validation](../core-runtime/call-binding.md#imported-meta-owner-validation).

The complete logs in `E:/cargo-targets/zr_vm/reports` are
`m1-msvc-imported-return-red-r1-zr_vm_typed_call_binding_test.log`
(SHA256 `369E871145DD19BFC8BAB2999763E220E45A6B2BC7CBFDECDE08601E0AA6051D`,
2524 bytes) and `m1-msvc-project-provider-probe-r3-zr_vm_compiler_integration_test.log`
(SHA256 `951CEC36BC3D5E1542B5558B57DA3FB575D2BE4ED5C30F60B4D53FB9D430D956`,
144378 bytes). The explicit META RED is
`m1-msvc-imported-meta-red-r1-zr_vm_call_binding_module_test.log`
(SHA256 `949390FD9CCF937B24D89D95E95C0FE3A4D83B254C6E70DC9B38657E9DC8DD97`,
1185 bytes). These owned native processes exited naturally. The post-primitive
original127 replay builds successfully and exits naturally with twelve failures
and zero ignored. Only
`test_migrated_decorator_import_project_run_returns_expected_total` changes
from FAIL to PASS; the other 125 standard outcomes and messages and the custom
generator failure remain unchanged. Its complete log is
`m1-msvc-primitive-project-replay-r1-zr_vm_compiler_integration_test.log`
(SHA256 `C9712BDF7012E54050EEFD78CF3DBA844E9189774B474E72AD864B3F3DF19385`,
144027 bytes).

The post-META original integration replay builds successfully and passes all
ten module cases. Its first LSP case then hits the native-callable assertion in
`function.c:5110`; it has no 127-case footer. Root verified the native process's
exact path and start time, terminated only that owned process, and waited for
the wrapper's actual exit -1. The partial log is
`m1-msvc-meta-project-replay-r1-zr_vm_compiler_integration_test.log`, SHA256
`89FF12AEB8C58D8516DEDA8454758D6CC146E2C183DB04DB1BB838BBFF4258FB`,
96749 bytes. Of the 89 completed standard rows, 86 pass and three fail. Only
the network project changes from FAIL to PASS; the other 88 completed outcomes
and messages match the prior complete run. The custom generator failure is
separate, and the remaining 37 standard rows receive no credit. The actual LSP
function/instruction remains unobserved.

The lower ordinary-instance test reproduces a linked VM target emitted as a
native member call. The two-line classification repair then passes 34 focused
cases: module 11, pipeline 18 and AOT projection 5. The new case executes the
exact VM method to 42. See
[Call Binding Runtime](../core-runtime/call-binding.md#imported-meta-owner-validation)
for the complete module receipt and the remaining AOT instance boundary.
The copied generator child-pointer repair subsequently makes its public compile
graph case pass. The yield suite reports 7/3/0 with the original three semantic
failures unchanged; module 11 and pipeline 18 companions all pass. The yield
log is `m1-msvc-yield-graph-green-r1-zr_vm_yield_semantic_test.log`, SHA256
`6142C99C4F87BDFDDD24D810914DB7530E64F5874B82B0C18796230B476CE053`,
4427 bytes. This is graph preservation evidence, not iterator runtime acceptance.

The earlier escape diagnostic builds successfully and reports 13/5/0. Its actual
`FUNCTION_CALL` has a unique `KNOWN_CALL` cache with a complete native `MODULE`
provider contract; the typed `CallbackHandle` exists but capture native flags
are zero. The diagnostic log is
`m1-msvc-escape-knowncall-diagnostic-r2-zr_vm_escape_pipeline_test.log`, SHA256
`1B4AE21BF114A261A4DF7465F6895D859AA2EF34FAD88D2BBC002487D97841E5`,
8269 bytes. The native-provider classification repair subsequently passes all
13 source/binary escape cases, with pipeline 18 and artifact 8 companions also
passing: 39/0/0. Its escape log,
`m1-msvc-escape-provider-classification-r1-zr_vm_escape_pipeline_test.log`, has
SHA256 `16626D053785F614749E5C64AEEC57687229B9397B1E5E25D4E5726BC0F87AEE`,
8541 bytes. The helper requires a unique real CALL cache and the complete native
provider contract; it preserves the existing capture classification rules.

The first receiver probe reports 29/5/0: its new call's source row is VALUE
without a TypeLayout, so the inline-alias gate prevents execution. That log has SHA256
`F64B931092EB526717A32B6615A2FA9C90DA740201AEB004DE506F168ECBEAF7`,
14940 bytes. The placement repair now passes the new frame gate and all three
original readonly alias/zero-copy gates, but execution fails with
`layout-mismatch`. Its latest suite remains 29/5/0; log
`m1-msvc-receiver-placement-r1-zr_vm_reference_receiver_call_boundary_test.log`
has SHA256 `E57F803C04732C3EECAE01C9817C322FFA8D2F1E72F6F5BBC8B2AD118649A7AC`,
15942 bytes. Module 11 and pipeline 18 companions pass. A bounded test correction
checks top-level `const fn` rejection through its actual null-AST, first
diagnostic and fatal-state contract, with cleanup before assertions.

The cyclic source routing replay still reports 18/1/0. A fresh existing startup
trace proves successful summary registration followed by full function
compilation and quickening; the returned summary has no typed CALL cache. Trace
SHA256 is `10FCD600878346072C45FA469AEDC9F206FBD2F1C2B78522FCD98DFEB441B050`,
1519904 bytes. The zero-argument metadata contract remains under review.
The task manifest records finite source leases, exact inverses and actual
process exits. These focused results do not complete M1 acceptance.

The META module GREEN log has SHA256
`AB96A5F9D670232CA3BB805312BDBB4B5722376505F59403D89C17EBCB759599`,
3440 bytes. The task manifest records all six complete logs, source hashes and
process exits. The cycle test inspects a naturally existing summary and real
typed sites; those observations alone do not prove which summary-import
producer branch executes. M1 and the platform acceptance gates remain open.

The last original 51-target foundation replay remains 34 PASS and 17 TIMEOUT,
with unchanged test limits and source pins. WSL reports `E_ACCESSDENIED` and
the explicit LLDB attach reports `Access is denied`; no stack or cross-toolchain
acceptance is credited. M1 remains active and unaccepted.

## Nine-Target Lower-Contract Replay (2026-10-14)

Root session 4762 completed a bounded nine-native MSVC replay. The wrapper
exited naturally with status 1; the shared build completed with status 0. Its
build log is `m1-msvc-m1-lower-contracts-r1-build.log`, SHA256
`817B8073CABB9D1CCA25696196936BAF9A6DF21D8CB1F2E63D7EA54C249D085A`,
8313 bytes. The wrapper recorded all 26 source pins unchanged and HEAD
`a9a321c4f7ade9ec57ae2249a0f148fbfe687437` before and after.

The nine targets total 132 tests with five failures: typed calls 18/1/0, yield
7/0/0, readonly receiver 29/4/0, runtime 8/0/0, module 11/0/0, call-binding
pipeline 18/0/0, artifact 8/0/0, callable returns 14/0/0 and canonical type
graph 19/0/0. Seven whole targets pass. These scoped results are not the
original 127-case project/compiler acceptance replay.

The yield suite now passes all four scoped-body and public-graph contracts,
including copied yield facts/ranges, the zero-instruction child and the
ordinary outer return; see
[Iterator Yield Suspension](../parser-and-semantics/iterator-yield-suspension.md#scoped-body-and-public-graph-replay-2026-10-14).
The typed zero-argument guard passes, but its sole failure remains the cyclic
summary-cache mismatch: summary cache count zero versus one in both runtime and
consumer, which execute to 17. The receiver target now passes the parser's
top-level `const fn` rejection case, but retains three readonly alias failures
and a direct-call failure. That call reaches opcode 213 with inline frame rows
but fails with `layout-mismatch`; passing its metadata gate is not a runtime
success. See the corresponding typed-call and receiver module records for the
target logs and hashes.

This replay does not accept M1, waive prior failures or close the platform
matrix. WSL and LLDB gates remain open. No source snapshot or commit is implied.

## Current Owner And Cleanup Evidence (2026-10-13)

The current alias fixture preserves all twelve prior cases and adds one real
ordinary-source-loader control. After explicit rooting corrects that control's
runtime-function lifetime, complete RED-r9 is13/5/0. The private owner-view and
local-receiver candidate passes independent core review. Fresh GREEN-r1 exits
naturally0/build0/test0:13/0/0, including callback cache/fullGC, real ancestor
function invocation and ordinary-source alias/result11. Complete runtime log
SHA256 is `64944DAF14B1F65399DD804DBADB19EA74D01BFA33C3B95286F53CFAE751D774`.
Hash-guarded fixture cleanup removes24files/33198B/errors0; exact source/archive
paths are independently confirmed absent. See the
[alias contract](../parser-and-semantics/comptime-import-alias-environment.md).

Fifteen actual source/VM exception cases retain the prior eleven and append
four admitted literal-cleanup companions. Their RED naturally exits1/build0/
test3:12PASS/3FAIL/0ignored, exposing both-arm cleanup, throw and void-return
gaps. The bounded follow-up guards keep only active matching finally plans in
the current callable graph. Independent peers then identify that successful
cleanup entry retains the protected body's Terminated flag.

Four lower programs run to natural exit: source cleanup62/0 and abrupt7/0 PASS;
pre semantic IR117/2 and shared weak84/2 FAIL. The four failures have no retained
pre-candidate native evidence and remain baseline-unattributed; unchanged test
source and unrelated50/50 foundation results do not establish their baseline.
Actual lower log hashes are `1CEEDD5D` and `B6769823` for the failing programs.
Root retains these failures and assigns source-level diagnosis.

Only the two existing terminal return/throw source-cleanup fixtures are
strengthened. All62 cases, source inputs and original assertions remain.
Additional checks inspect cleanup source line6, the payload's defined LOAD and
real ExecIR construction, with new assertions after owned resources are freed.
Complete strengthened RED naturally exits1/build0/test2:60PASS/2FAIL/0ignored;
both first new cleanup-source assertions fail. Later LOAD/ExecIR assertions
remain uncredited on RED abort. Runtime log SHA256 is
`AFEF341195C90C627308C9F215BA0A244AAFE5A18AF90B613E7BA852DACC6A14`.
This real-source evidence authorizes a one-line successful-entry flag reset;
fresh62/15/7 verification naturally exits0/build0. All84 cases pass with zero
failures/ignores; both terminal cases reach all three new assertions. Source
log SHA256 is `BB22EBE0E85F4C436735F3533847F7C1E2B9863BE1014407CA313F42B12B8D52`,
runtime log SHA256 is `0A4DEB88CE5CF64DAC64826A2ADBFE31E1891CA12AEA4FD5E479B16F4C117A76`.
See the
[finally contract](../parser-and-semantics/named-finally-completion.md).

All current private references follow the actual functional-directory
migration. Original126/full121 replay, current foundation graph and WSL
GCC/Clang remain open; no milestone acceptance, commit or M2 advancement.

The two resource failures are traced to an old fixture that manually appends a
local variable without its parallel hygiene identity. Actual contextual lookup
rejects that incomplete record before the intended missing-guard assertion.
The bounded TEST-only repair uses the existing local allocator, retaining
original type inference, deliberate missing facts and diagnostic assertions.
Fresh84 GREEN session77410/chunk3b34cf exits naturally0/build0/test0, all84PASS
with zero ignores. Both intended missing-guard diagnostics now execute and
pass. Runtime log SHA256 is
`B1F0CF6A628FB484B1A3E648C73B3CB60E50E0EC37C89FB45D9B91ADFAE3A02C`.
This source diagnosis and focused repair do not invent a prior native baseline.

The pre-semantic IR failures have separate obsolete fixture assumptions.
Current int64 literal/load/transfer producers preserve their defined value IDs;
five temporary INITIALIZEs and two same-type CONVERTs no longer appear in the
old four-statement golden. The updated contract verifies exact23 operations,
stable CONSTANT/LOAD definitions and types, direct reuse by local INITIALIZE,
and assignment LOAD/STORE identity. String values still require the existing
memory-backed temporary initialization and conversion assertions.

Typed `value > 0` now emits canonical COMPARE with int64 operands and a boolean
result, so it legitimately keeps the nested source CFG. The fallback negative
uses `value == 0`, outside the current semantic comparison subset, while a
separate positive retains the complete original greater-than source and verifies
its actual comparison/branch/value/source/type contract. All117 original test
registrations remain, with one positive added. Fresh118 native session2386,
terminal68a5cf, exits naturally0/build0/test0:118PASS/0failures/0ignored.
The complete runtime log SHA256 is
`EB2726AC9473CFF2481EE82779CF9C4F5FCE34C80772D8BA131F79E3E01401CF`.
Root independently confirms the footer, all eight production and both test
pins unchanged, and an empty reused working directory. This proves the tested
pre-semantic source/value/type/branch contract; it does not add numerical,
runtime or downstream ExecIR comparison acceptance.

The actual language-pipeline manifest retains all126 original programs. Full
driver92140 completes both builds and executes all51 foundation entries:
23PASS/28TIMEOUT,1573.60s, with no other failure outcome. The complete raw CTest
log is retained at
`E:/cargo-targets/zr_vm/reports/m1-msvc-owner-finally-full-r1-foundation-lasttest.log`,
78791B/SHA256 `79593AB1D254F84ADB89EB60FB0993F7DBC9E290AC026CA29D23DDA503317D7A`.
Every timeout entry has zero captured output. Unity's linked static library
lacks the target's flush define, so this cannot locate execution before the
first test. No actual lock, GC wait, environment cause or candidate causality
has been established.

The aggregate reaches compiler integration127/13/0 and fails there, retaining
the actual STORE and subsequent import-metadata errors. Its first five original
programs have complete zero-failure footers. Full driver92140 exits naturally1:
builds0/0, foundation8, aggregate8, inventory1. The remaining121 original programs
complete with111PASS/10FAIL. The formal comparison exits1 with121 programs on
each side, zero missing footers or unaccounted failures, and four changed
programs. All eight production pins remain exact after exit. Neither focused
GREEN nor an unchanged historical failure substitutes for M1/platform gates.

The four changes include the actual exceptions improvement8/2/0 to15/0/0,
retaining all eight original cases. Eight new failing cases remain: two AOT
runtime source-read prerequisites and six ExecBC LLVM source include contracts.
The runtime tests read a nonexistent nested AOT path; only the root library was
moved into its `aot_runtime/` directory, while the AOT variant remains flat.
The two TEST paths now point to that actual flat variant and preserve every
original assertion. Complete raw inverses restore the original test files.

The six LLVM tests retain all195 positive/negative source-text assertions. Nine
include expectations gain the actual root header's `backend_aot/` prefix:
module prelude, call result and terminal, three occurrences each. The terminal
checks were previously masked by first failures. Independent core review
verifies the entire raw inverse,2077 unchanged assertion tokens and100 unchanged
registrations:98 default tests plus two environment-selected focused entries.
These are TEST source repairs only; their fresh native result is still pending.

The unchanged full-r1 TokenStream binary is subsequently run directly:2/0/0,
3.474s, then through its original unchanged CTest entry:PASS0.96s. This narrows
the timing evidence but does not establish a unique root cause or erase the
original28TIMEOUT outcomes. The unchanged original51-binary diagnostic replay
naturally exits8:20PASS/31TIMEOUT,1606.43s. Its complete51-entry raw log is
retained at `E:/cargo-targets/zr_vm/reports/m1-msvc-foundation-timeout-diagnostic-r2-lasttest.log`,
74698B/SHA371D3D49. Both foundation failure records remain open; neither
establishes a unique cause or passes acceptance. Unbuilt new STORE tests and
AOT TEST source repairs are not credited by the original-binary replay.

Four appended STORE cases have independent raw/inverse review. The public
captured-IIFE case preserves the enum equality condition and expected result8;
the public enum modifier is present and independently reviewed. Lower
tests capture the real declaration binding, source, context identity and empty
destination slot before compiling RHS42. They cover inactive untyped legacy
assignment, canonical INT64 STORE, and an actual BOOL/begin_if ACTIVE refusal.
The latter checks the lower API contract, not rejection of a legal ZR program.
Original8/118 cases remain and the reviewed candidates contain9/121. First
rebuilt native76963 naturally exits1/build0 with121/1 and9/1: only the inactive
untyped API assignment and new public enum source compilation fail. Canonical
slot rematerialization and ACTIVE refusal pass after all real prerequisites.

The minimal STORE repair reuses the existing LOAD policy before typed
materialization, additionally requiring initialized state and non-NONE RHS.
Only12LF/513B are inserted; independent whole raw inverses restore the original
98939-byte module. No helper, producer, binding, ownership, budget or typed
view-fact propagation changes. Source hash0B2AD1FD is independently pinned.
Fresh native39312 naturally exits1/build0:121IR,9lambda,62cleanup,84resource,
15exception,7abrupt and13comptime alias cases allPASS,311/0/0. The new source
compiles, executes and returns8. Integration remains127/13/0FAIL. The matched
126 standard rows have unchanged case status/failure details. The127th case is
`test_generator_mechanism`: its existing `ZR_TEST_FAIL` macro sets
`Unity.CurrentTestFailed` without emitting a standard row. Both raw logs contain
exactly one corresponding custom failure, so all127/13 cases are accounted;
none are waived. STORE8 and import-metadata6 error occurrences fall to0. Both
LSP outer-status assertions pass; `Project_Run` returns runtime-error2 at the
request-status assertions. Complete startup trace19712 naturally exits13 with
127/13/0 and unchanged original case statuses. Both LSP source loaders,
core/oop/async compilation and registration, main creation and entry-call setup
succeed. The inner execution TryRun returns2/resultNULL/callCompleted0/thread2;
the concrete return is execute_function2481 through Project_Run2566. The deeper
exception message/throw site remains unobserved. A TEST-only diagnostic include
reads borrowed native state, binding/module-load fields and an actual own
`message` string immediately after failed Project_Run. It is enabled only by
`ZR_VM_TEST_PROJECT_RUN_DIAGNOSTIC=1`; it neither executes user conversion nor
allocates VM values or invokes property/prototype lookup. The large regression
file gains only include/call lines, and the complete RAM inverse preserves all
original source, assertions, cleanup, registration and eight caller bodies.

LLDB CLI capability probe68053 naturally exits0/46c64d. Actual version9.0.0
accepts external symbol lookup off and the registered physical E module cache.
Its439-byte log SHA4BB95945 is retained; no target/attach/PDB or M1 acceptance
credit comes from this probe. One unchanged own-process control-flow diagnostic
and existing project startup trace are preregistered before creation, with
bounded own handles and separate records for any forced exit. The controlled
original nine-case diagnostics95181 and5695 both naturally exit0 with9/0/0 and
identical1053-byte stdout SHA30FC9BBB. Both exit before debugger attachment, so
no stack/PDB/attach credit is claimed. The first startup/identity interval is
27307ms; the second records preStart162/Start-return688/Handle693/identity798/
streams809/total3132ms. These sampled intervals include adjacent PowerShell and
identity-query overhead; neither establishes an OS, GC or VM cause. The original
control CTest44748 also passes0.25s with its unchanged30-second limit. Original
28/31 timeout records stay open. Fresh current-source original51 replay1009
naturally exits1:build0/discovery0/CTest8,50PASS/1TIMEOUT171.74s. The only timeout
is alias scope; its complete raw branch has zero output and reported37.22s
under the original30-second limit. No unique cause follows from that absence.
The complete51-entry raw log02D042FE/155433B is retained, and all15 frozen
source pre/post hashes match. An owned original15-case alias-scope stack
diagnostic is registered; neither a later direct pass nor this50PASS result
waives the failed51 acceptance record. Alias probe92514 naturally exits0 with
15/0/0 and complete1792-byte stdout SHA57F141E8. The debugger exits0 but its
actual output has five `invalid process` errors and no attached process or
frames. This is failed stack evidence, regardless of the helper's numeric gate;
explicit owned-PID attach and content checks must be repaired before retry.
The current probe inherits UBSAN absent; the earlier51 inherited value was
unrecorded, so matching environment cannot be inferred.

The pure project probe17606 naturally exits13/build0 with127/13/0. Its116700-byte
log SHAE7B913F9 contains126 standard rows with exactly unchanged names, status
and failure messages, plus the original custom generator failure. Four stored
exception messages now identify call binding: network token0x03000004/instruction32
and both LSP token0x03000007/instruction26 report `target-not-found`; decorator
token0/instruction1 reports `signature-mismatch`, expected16702598370128743575
and actual12842983158373519980. All four have actual exception status2 and no
module-load diagnostic. These observed payloads narrow the next imported-link
and signature-provenance investigation; no speculative guard bypass or unique
provider/row branch is accepted without actual table evidence.

The repaired owned alias diagnostic82715 correctly exits1: the original15
cases allPASS and both owned processes exit0, but explicit LLDB attach returns
`Access is denied`. Actual stderr is42B; no stopped process, stack or detach
evidence exists. The new content gate rejects debugger exit0. The unchanged
original alias CTest32981 subsequently passes5.79s under its original30s limit.
Fresh full51 replay97168 then builds/discovers successfully and exits1/CTest8:
34PASS/17TIMEOUT in1015.61s, including aliasScopePASS5.93s. Complete fresh raw
BDBE16F0/134552B and all15 matching pre/post source hashes are retained.
All51 names are accounted; no allPASS or unique OS/GC/VM timeout cause follows.
Further attach attempts are unavailable; no permission bypass or larger test
limit is used, and prior failed51 records remain visible.

Two bounded TEST candidates are in progress. The existing failure diagnostic
may inspect candidate provider tables rooted by the loaded-module registry;
it must not follow the post-longjmp frame or infer the failing consumer's
module identity from a token RID. A separate real source-loader fixture
exercises an imported zero-argument integer function held in a callback alias
and called by a named function. Nominal, closed-generic canonical arguments
and explicit-width integer metadata are preservation controls. Source reading
maps the observed signature pair to INT64 with spelling `int` versus no name;
this is a producer hypothesis until the lower fixture has native evidence.
Production remains frozen, and all original integration failures are retained.

The prior TEST-only AOT repairs have a complete native result:26/5,8/1 and
98/13, each0ignored. Exact comparison preserves all26/8/98 original names,
resolves only the eight full-r1 added failures and leaves every other status
and failure message unchanged. Original5/1/13 failures remain open. Complete
failure evidence and final foundation/full126/platform gates are retained.

## Base Replay Follow-Up (2026-10-12)

Frozen lookupB512AF86/relations54790C7C with fixture34 main3B2939F0/cases519C409A
passes native15856 naturally0/build0. Root verifies15 unique Unity footers,
435 tests/435 PASS rows/0 failures/0 ignored, including imported-first generic
NULL-target and forward typed interface controls. Eight unique RAM inverse
fragments restore both complete preimages exactly. No source copy or snapshot.

Actual public `Parse(..., sourceName=NULL)` is then exercised by a35th case.
It proves nonempty parsed positions and navigation TYPE publisher rejection
without appending a fact; actual class producer and exact BASE succeed before
alias truncation. Complete native67316 RED3 output is35/1/0, all previous34 PASS;
the first contracts replay fails because the optional fact is absent and the
opaque alias is already cleared. Exec handle missing after continuation means
the natural exit code remains unobserved, not assumed. Necessary logs are retained.

Lookup-only follow-up6F81AD3E persists compile identity without false navigation
ranges via existing structured facts; relations54790C7C, shared type-use API,
class and headers stay unchanged. Fixture35 main124E5CD5/cases6860FBDC adds
post-RED identity/range/idempotency assertions, with no separate RED credit.
GREEN contracts-r2 native94400 naturally exits0/build0. Root accounts for15
unique Unity footers,436 tests/436 PASS rows/0 failures/0 ignored and unchanged
pins. Five RAM inverse fragments restore the complete23923-byte prior lookup
source. Independent static review finds no new blocking issue; all-zero sentinel
position queries can match a range-free fact, unlike actual positive-line cursors.

Full native replay59266 naturally exits1 against the reused MSVC tree: original126
pipeline build0, foundation build0 and47/47 PASS, aggregate8 with the same
catalogue failures, complete remaining121 inventory108 PASS/13 FAIL. Comparator
36925 naturally exits0 with all121 pairs accounted: exit/footer/failure name and
non-pointer-message changes0, missing footers0, unaccounted failures0. The bounded
BASE repair introduces no original-program regressions. Original13 failures,
WSL GCC/Clang and remaining M1 contracts remain unwaived; no stage or commit.

Root adds the dedicated six-case `comptime_aggregate_budget` fixture after this
full replay. Actual parsed empty object/array expressions exercise the public
evaluator, not fabricated aggregate values. It distinguishes atomic rejection
of paired reservations from cumulative successful-work usage after scratch
root cleanup and GC. First native80224 exits1/build1 at a test-only nonexistent
value API, so it executes no tests and receives no behavioral RED credit.
Root corrects three calls to existing `ZrCore_Value_ResetAsNull`; RED-r2 native
99959 naturally exits1/build0/test2,6 tests/4 PASS/2 failures/0 ignored. Only
the final heap assertion fails in both aggregate-slot cases (expected7/actual463),
after exact diagnostics/null result/no object/root balance/fuel prerequisites.
Heap rejection and cumulative success/GC controls pass. Independent review
accepts fixture861E6C39/CMake96C7E457. Only constructor extraction into a private
module and paired reservation are authorized; GREEN/full replay stay pending.
Fresh GREEN native30938 naturally exits0/build0/test0:6 PASS/0 failures/0 ignored.
Independent static review accepts executor88964C20/newC6945A33D/newHD0EE6A2A;
Root full-byte RAM inverse matches prelease99ECACD7, with fourteen callers and
support643156D2 unchanged. No source copy. Full25248 naturally exits1: original126
build0, foundation build0/48 PASS, aggregate8, remaining121=108 PASS/13 FAIL.
Comparator3626 naturally0 proves zero exit/footer/failure changes versus the
BASE-final-r1 replay. The prior47 pass is not relabelled as a48-test run. Real
imported typed Diagnostic/Patch budget probes are the next separate gate; these
results do not cover them. Fresh Ubuntu execution
availability returns `Wsl/Service/E_ACCESSDENIED`, natural exit-1; no GCC/Clang
build or test credit. See [aggregate reservation](../parser-and-semantics/comptime-aggregate-reservation.md).
The separate real-import typed gate has clean RED383423:12 cases/10PASS/2FAIL,
only finalheap Diagnostic7/463 and first-Patch-array463/919 after all source/provider/
diagnostic/root/object/fuel prerequisites. Prior78398 build-only and45952 incorrect
ARRAY raw-tag control are retained without clean-RED credit. Corrected fixture
56410678/CMakeAC3604EB is independently accepted. Candidate936C56F3/F3C8E6F2/
DAB18EE4 shares the private location-aware transaction; Root and author reconstruct
all three full preimages in RAM exactly, no source copy. Peer accepts the candidate.
Focused82488 naturally0/build0/test0: all12PASS, footer and current pins verified.
Existing C4701/D9025 warnings are not waived. Full65127 naturally exits1: both
builds0, and the foundation stage records47 PASS/1 TIMEOUT, with
`macro_syntax_hygiene_nominal_types` timing out after40.89s at its existing
limit30s. The aggregate exits8 with the two existing catalogue assertions;
all121 remaining programs run108 PASS/13 FAIL. Comparator98749 naturally0
verifies zero exit/footer/failure/normalized-diagnostic changes, no missing or
unaccounted result. The separately preregistered identical-source/binary nominal
CTest directly exits0:47 Unity PASS/0ignored,0.63s at its original30s property.
The original timeout is retained and its cause unconfirmed. Whole48 current
unchanged-input replay24286 naturally exits0:48/48 PASS100.27s, nominal_types
1.46s, no rebuild or property change. This separate run does not relabel65127.
Original13/WSL/M1 remain open, no commit/M2. Module-doc validation of eleven
current documents passes537 metadata references and32 local links using actual
Python3.14.4/PyYAML6.0.3, exit0.

## Definition Environment Evidence (2026-10-12)

The standalone source-provider fixture is registered as
`macro_comptime_definition_environment`; the dynamic foundation graph now
includes it in addition to the previous48 programs. A synthetic provider source
and actual `.zrm` archive are loaded through BuildFacts using real content
hashes, a manifest and dependency lock. No manual compiler owner/binding values
are inserted. Cold-cache evaluation checks caller argument versus owner default
lookup, private/internal visibility, captured functions and locals, callback
values, lexical blocks, full GC and exact diagnostic ranges.

Native62008 naturally exits1/build0/test12: all cases stop at removed legacy
`comptime let` parsing. Fixture-only correction uses supported module-level
comptime blocks and their actual parsed initializer. Native17814 naturally
exits1/build0/test5: four intended failures plus one false diagnostic substring
for private external access. Peer/Root trace the existing exported-only lookup
to the exact alias rejection and correct only that expectation. Both original
logs remain immutable.

Clean RED63325 naturally exits1/build0/test4:12cases8PASS4FAIL0ignored. Only
provider function/local capture and supplied positional/named22 becoming11 fail.
Every provider identity, scope/root restoration, default, callback, lexical,
full-GC and private diagnostic control passes. Fixture0ED151EE/CMake85708630 and
raw test log90C33D8F are pinned. Each run removes24 test-created files, measured
30348 bytes, zero cleanup errors; source/archive paths are absent after RED3.
Four production pins were unchanged through the clean RED.

The authorized private call C/H extraction and bounded executor/support repair
passed whole-file Root RAM inverse review and independent delta review.
First GREEN42047 naturally0/build0/test0,12PASS/0FAIL/0ignored; raw logBECAD5E8.
Six additional companions passed49580 naturally0/build0/test0,18PASS/0FAIL/
0ignored, raw logF8B217C0/fixtureE1AE929A. They cover earlier-parameter defaults,
caller-variable shadowing, explicit/default scalar cache hits and GC replay,
and an actual full GC during second array-argument allocation. The latter
observes first-array contents before/after GC and restored root/scope marks,
without claiming isolated temporary-root or exact cumulative charge equality.
Companion cleanup removed36 test-owned files58788 measured bytes/errors0.
The unchanged production and test inputs completed separately registered full
native16417 naturally1: both builds0, foundation49/49PASS54.08s, aggregate8
at the same catalogue assertions, remaining121=108PASS/13FAIL. The structured
comparison against typed-aggregate-final-r1 exits0: zero exit/footer/failed-name/
message changes, zero missing footers and zero unaccounted failures. The decorator and
execution-scope modules remain outside the lease. This evidence does not accept
M1 or waive original13/WSL obligations. See the
[definition contract](../parser-and-semantics/comptime-definition-environment.md).

The later private-provider function-value clean RED-r2 has26 cases:21PASS and
five intended callability failures, native90343 naturally1/build0/test5,
logBDEF995D. Producer-record/held-GC identity, both effect rejection controls,
borrowed-parent prerequisites and the original18 cases pass. The two-path
private callable extraction passes independent full-file inverse review;
Root additionally compares all43651 prelease bytes in RAM. First GREEN88458
naturally0/build0/test0 has26PASS/0FAIL/0ignored, log86EC93D2. Fresh callable and
support compilation/linking is present; hashes remain frozen. Cleanup removes
52 test-owned files110448 measured bytes/errors0. Broader regression and
binary-provider function values remain open.

## Lambda Semantic Isolation Evidence (2026-10-12)

The new five-case parsed fixture registers
`macro_lambda_semantic_ir_isolation` with the unchanged30-second timeout.
It checks a named-function control, float and integer lambdas, siblings and
captured-lambda execution followed by a parent store. Actual seed binding,
canonical INT64, local place/value/source identities precede child compilation.
Complete parent semantic-state snapshots exclude legitimate outer closure
bytecode/constants/stack changes. Review corrected the named control's false
seed-slot-zero assumption before native testing; the integer collision case
still explicitly proves its real LOCAL slot zero.

Fixture71EB2A6C/22739B/496LF is frozen and independently reviewed. Its full RAM
inverse restores priorF56AAFD0/22377B with no filesystem source copy. First RED
native7350 naturally1/build0/test4 produces1PASS4FAIL0ignored, log37E7C18A.
All four failures precede lambda compilation at a false declaration-range
containment assumption; parsed declaration range covers the keyword, while
the actual local binding range covers its identifier. The runtime8 control
passes. Fresh r2 is preregistered after a fixture-only correction; no behavioral
RED or production GO is attributed to r1. Production4DD34B08 is unchanged.
Corrected fixture96EAAC6E then passes native94659 naturally0/build0/test0:
all5PASS0ignored, logF504FE6C. The initial direct-return collision prediction
omitted the actual separate return-expression semantic isolation. New real
child-initializer/error-restoration companions are test-only; production stays
unchanged and no behavioral RED is claimed for the passing five cases. The
eight-case STOP7A4B0B07/29982B/642LF adds three parsed companions. Root whole-file
RAM inversefa834b naturally0 restores96EAAC6E/22691B, and independent peer review
grounds child-local metadata and rejected-child diagnostics in actual producers.
Fresh r3 evidence paths are registered before native invocation. See the
[lambda contract](../parser-and-semantics/lambda-semantic-ir-isolation.md).

Actual r3 native21046 naturally1/build0/test3 has5PASS and3FAIL. Two failures
reach the parent snapshot; the float companion first stops on an incorrect
FLOAT-versus-DOUBLE prerequisite. Corrected fixture27FA953E changes only that
expected enum, with whole-byte RAM inverseffe2c9 to7A4B0B07. Clean r4 native34389
naturally1/build0/test3 produces5PASS/3FAIL/0ignored: all three new cases now pass
their identity or exact diagnostic prerequisites and fail parent places count
Expected3 Was5. Complete logC031B66B is independently reconciled. This supports
the registered sole-file lambda isolation repair; no GREEN or M1 acceptance is
claimed yet.

Candidate04EC2C2F/56518B/1082LF/549CR is independently reviewed and has a complete
Root RAM inverse9b0f95 to preimage4DD34B08/56092B. First GREEN native9999 naturally
exits0/build0/test0, with8PASS/0FAIL/0ignored, log224E0A24. Production/test pins
remain unchanged after exit. Complete child identities, parent snapshots and
real diagnostic cleanup now pass; captured-lambda execution returns8. Existing
D9025 is retained. Broader original126 build, foundation50 and remaining121
replay is running under the preregistered lambda-definition-final-r1 stem; this
is not M1 acceptance or fresh OOM/exception-state coverage.

Full native61000 naturally1/builds0: original126 outputs build successfully,
foundation50/50 PASS in33.84s, aggregate exit8, remaining121=110PASS/11FAIL.
Comparisona6212b returns1 by design for eight observed improvements in exactly
two programs: literal_surface57/1-to0 and lexer_parser_compiler_execution14/1-to0.
All other119 program pairs are unchanged, missing footers0 and unaccounted
failures0. There are no new failures. Complete evidence uses the registered
lambda-definition-final-r1 stem; own source/test pins remain unchanged after
exit. The diagnostic catalogue's two failures and original11 programs stay
unaccepted. Full fixture cleanup does not emit a measured byte total, so it is
not inferred from focused runs.

## Diagnostic Catalog Support Evidence (2026-10-12)

Focused RED exec2184db naturally1/build0/test2 has30tests/28PASS/2FAIL/0ignored:
exact registry count71versus94 and actual messages188versus142. All23 existing
optimization reason descriptors5001-5023 lack title/message text. Candidate
messagesF35FF276 supplies23pairs using the existing macro; testDAEB5FFC changes
only stale exactcount71to94. Registry26818FAB and all ID/uniqueness/per-entry/
locale assertions remain intact. Root whole-byte RAM inversesa6cc26/6b9b27 and
independent peer key/status/inverse review pass.

First GREEN15104 naturally0/build0/test0, all30PASS0ignored; logBBF03D58. Source,
test and registry pins are unchanged after exit. The table has94pairs188keys;
English fallback and informational remark semantics are preserved. No source
copies or filesystem fixtures are created, and broader pipeline/platform
acceptance remains open. See the
[catalog contract](../parser-and-semantics/diagnostic-message-catalog.md).

## Historical Import Alias RED (2026-10-12)

The twelve-case target uses an actual source archive, manifest V2 dependency,
dependency lock, canonicalized consumer AST and BuildFacts. Independent review
accepts the original nine prerequisites and three owner companions. The Root
view companion adds real `SHADOW`, full-GC and live-row byte restoration
coverage; it does not establish marked hygiene. The conditional companion uses
the branch already selected by BuildFacts. The ancestor companion validates an
actual parent function value and calls the existing private source-call
primitive; it does not claim public ancestor bare-name lookup.

Root appends one normal CMake target, retaining the 30-second timeout. Its
whole-byte inverse restores CMake F9DB7FC8; current CMake is 27CFF610. The
original nine cases receive only lifecycle guards and callback stage markers;
main050C2CB6 plus private companion header7F8F623F preserve all of those bytes.
Root's whole-main inverse restores C22365C4/38,405B with no source copies.

Actual RED-r3 session54677 exits naturally1: build0, test5, twelve tests,
seven PASS, five FAIL, zero ignores. Original four failures remain, and the
validated ancestor call fails only after its real parent/child prerequisites.
Root view/GC/saved-row and selected conditional import controls PASS. Both
callback stage markers PASS before its first cross-owner failure; later
callback cache/GC assertions remain unreached. Hash-guarded cleanup removes
24 files/33,198B with zero errors and exact source/archive absences. Test log
SHA256 is `19C2F87C6A51475AFAF3EFFEE591C87E1ACCC1C0283C86DC7F23C3BA0E070287`.
Fresh RED/GREEN logs and exact synthetic paths were registered before execution.
No production GREEN or full M1 acceptance is credited. See the
[alias contract](../parser-and-semantics/comptime-import-alias-environment.md).

## Unpublished Metadata Support Evidence (2026-10-11)

The seven-case full-lifetime ARRAY fixture has two genuine successful controls
and five source rejection/recovery cases. Class builtin metadata is established
by compiling a real Baseline declaration on the retained source AST before the
probe; its observed count replaces the original incorrect zero/one assumptions.
Child types, error cause, compiler context and actual recovery publication are
checked before final owned-buffer assertions.

RED-r1's fixture-only post-Global delegate use required Root to stop only its
identified own PID7788; terminal driver1/build0/test-1 has no behavioral credit.
RED-r2 naturally1/build0/test6, seven=1PASS6FAIL, gives only struct ownership RED;
class count failures are fixture errors. RED-r3 native5738 naturally1/build1
uses a nonexistent AST field and runs no test. Those logs are preserved.
Corrected RED-r4 naturally1/build0/test5, seven=2PASS5FAIL0ignored: both controls
pass, all failure prerequisites pass, and only final surviving ARRAY buffers
fail (eight in pending/recovery cases, five in abstract/final). Independent
fixture review finds no further issue. Logs use distinct
`m1-msvc-unpublished-type-metadata-red-r4-*` report paths.

Root grants only the six-path private lifecycle extraction and source
pre-publication cleanup lease. No field capture, production GREEN, semantic
binding rollback, M1 acceptance or matrix waiver is implied. See
[metadata ownership](../parser-and-semantics/type-prototype-metadata-ownership.md).

The source lease is now STOP/frozen (84CB83B6/4B133E77/0BE0A8F0/7DEE51EA/
7A6B938B/91FE3C84). Root independently proves complete read-only in-memory
inverses to the four prior raw hashes, including mixed endings and State's
actual transferred destructor. Shared headers remain unchanged. Independent
static review and GREEN-r1 native67316 are pending; source/tests/CMake remain
frozen until the actual native terminal result.

That replay now naturally ends0/build0: fifteen programs408PASS0FAIL0ignored.
Independent production review finds no actionable regression. Root adds seven
actual nested-array/method/body-rejection companions. GREEN-r2 native5625 ends
0/build0/all415PASS but emits a fixture-only C4090 qualifier warning; the narrow
Root correction is validated by GREEN-r3 native21375 naturally0/build0/
fifteenprograms415PASS0FAIL0ignored/no C4090. Production pins stay unchanged.
Independent fourteen-case fixture review continues; generic constraint cleanup
has no new dynamic claim. The standalone nine-case converter replay native48500
passes. Upward field native4010, RED-r3-after-prototype-lifecycle, naturally
ends1/build0/test11 with the same twenty=9PASS11FAIL result identity losses.
The lifecycle source lease is retired/frozen; field code remains read-only.

## Field Result Support Evidence (2026-10-11)

First field replay native5099 naturally closes driver1/build0: fifteen programs,
429 cases, 428 PASS, one FAIL, zero ignored. All twenty-eight field cases pass,
but eager capture rejects the existing Matrix const-generic producer at
test_type_inference.c:5100 with `Unknown compile-time identifier: N`.
The seven field files remain frozen, not accepted GREEN. Root independently
reconstructs all five full pre-lease raw hashes in RAM, preserving mixed EOLs.

The separate eighteen-case [open array annotation](../parser-and-semantics/open-array-annotation.md)
fixture reaches genuine RED-r3 native11165, natural1/build0/test5:
thirteen PASS, five FAIL, zero ignored. Three direct open converters and two
actual class producers fail after all real binding/AST/scalar identity and
thirteen diagnostic/concrete-value controls pass. RED-r1 build1 has no behavior
credit; RED-r2's mistaken zero-expression prerequisite has no zero RED credit.
No constructor bypass, symbolic-size identity or closed-size claim is accepted.
The exact three-source lower repair is now frozen at7D7A106B/E38CA8DE/DFC6529F.
Root's full in-memory inverse matches452DC6EE, and independent source review
finds no actionable defect. Native42187 GREEN-r1 naturally0/build0: fifteen
unique footers total419PASS0FAIL0ignored/all18cases. Existing Matrix acceptance
is restored through normal annotation conversion, not constructor logic.
Seven context companions extend the fixture to25cases, no prior RED claim.
GREEN-r2-contexts native10925 naturally1/build0: Related401PASS but all25fixture
cases stop at a statement-count prerequisite because source function name
shadow is reserved. Root renames only that name to inner_type. Standalone
GREEN-r3-contexts native32148 naturally0/build0/test0/all25PASS0FAIL0ignored;
this is not a new combined426-case result. Four nested generic field companions
(32 cases) and ten field-capture lifetime companions (24 cases) are peer-accepted.
Upward native42537 naturally ends0, all three builds/drivers0: field32+Related401
gives433PASS across fifteen unique footers; standalone metadata24 and function9
also pass, all zero failures/ignored. Root independently verifies all17 footers
and unchanged field/array source pins. Distinct fresh stems preserve all logs:
`m1-msvc-field-results-green-r2-nested-*`,
`m1-msvc-unpublished-type-metadata-green-r4-fields-*`, and
`m1-msvc-function-type-lifetime-after-fields-r1-*`.
Full native92840 naturally ends1 under `m1-msvc-field-array-final-r1-*`:
original126 build0, foundation46/46PASS, aggregate8 with the same catalogue
71/94 and188/142 assertions, remaining121=108PASS/13FAIL. Two compiler ownership
metadata cases and one module ownership metadata case newly fail. The old
comparator missed CRLF Unity rows and custom ZR_TEST_FAIL records; its corrected
LF/CRLF/custom/accounting self-test passes, and all121 log pairs have zero missing
footers and zero unaccounted failure records. Corrected comparison names exactly
those three new failures (compiler127/13->127/15, module78/2->78/3).
All three fixtures omit Resource, which is not an implicit builtin. The normal
ownership-inner annotation converter rejects it before array extent handling.
Root adds real public resource-class declarations and prerequisite metadata
checks, retaining all original ownership assertions. Counts become3 because
one real source prototype is added. Independent exact diff review accepts this
test-only correction. Targeted native28620 naturally ends1/all builds0:
compiler127/13 and module78/2, all three repaired cases explicitly PASS.
The programs remain failing due to their original cases. Full native18591
naturally ends1 under `m1-msvc-owned-field-prerequisites-final-r1-*`: original126
and foundation builds0, foundation46/46PASS, aggregate8/same catalogue failures,
remaining121=108PASS/13FAIL. Corrected comparison native24332 naturally ends0:
all121 exits/footers/failed names/non-pointer messages unchanged, missing0 and
unaccounted0. Root independently rechecks both modified test hashes and unchanged
field/array pins. All three new fixture failures are repaired without waivers.
BASE publication resumes as a separate unaccepted gate; whole M1 stays open.
Its [real producer fixture](../parser-and-semantics/macro-base-relation-publication.md)
has eleven authored cases: nine real annotation/prototype prerequisites followed
by exact published BASE identity checks, and two direct missing/invalid-context
failure cases. Independent peer review accepts after Root hardens the negative
query's valid IDs/exact AST/TYPE-symbol prerequisite. RED-r1 native97142 naturally
exits1/build0: eleven=4PASS7FAIL0ignored, four Root/mixed controls pass and all
intended opaque/missing/invalid publication assertions fail. Three companions
extend the fixture to14: Root-forward order, private opaque-alias registration
using the real Root value carrier, and parsed generic aliases with Root cache
first/marked argument identity. RED-r2 native29363 ends build1 from a test include
path error, with no behavioral credit. Root fixes it; RED-r3 native90616 builds0,
fourteen=6PASS8FAIL, but the plain alias stops at a nonexistent variable binding.
Root instead converts a real parsed Evidence:Base annotation. Corrected RED-r4
native48817 naturally1/build0, fourteen=6PASS8FAIL0ignored, all eight intended
BASE assertion failures after real prerequisites. Exact lookup.c/.h + class.c
producer-only GO is granted. The generic edge's open declaration endpoint
cannot prove closed-instance selection; direct typed-helper instance assertions
are added for alias and actual generic annotation (fifteenth companion), with no
new RED credit. A sixteenth Root generic-forward success companion protects
valid deferral without guessing prototype counts. Later contracts replay is open.
An imported-null key-priority control brings the fixture to17. GREEN-r1 native
16490 naturally0/build0,15 unique footers418PASS0FAIL0ignored,17 direct +401 related.
Peer review flags wrapped alias shape and mixed LOCAL-miss Root-alias fallback.
Five real companions bring it to22. RED-r5 native19247 naturally1/build0/test2:
20 controls PASS; only the two expected actual producer BASE assertions fail.
Root authorizes lookup.c only, complete alias spelling and an initial Root alias
policy disabled after checked LOCAL absence; public name wrapper/header/class
remain compatible. Root's RAM-only inverse verifies all six unique changes and
full pre-edit bytes/SHA. GREEN-r2 native52095 naturally0/build0:15 accounted
footers423PASS0FAIL0ignored,22 direct+401 related. Producer support only is accepted.
Eight real post-contract companions extend the fixture to30; contracts RED-r1
native34192 naturally1/build0/test3,27controlsPASS. Both same-name opaque targets
gain wrong Root BASE edges and mixed LOCAL miss captures a Root alias after replay.
Expired alias-storage cleanup and actual closed-source metadata controls pass.
Two producer-fact/multi-target companions bring it to32. Contracts RED-r2 native
84649 naturally1/build0/test4,28controlsPASS. The extra failure is a missing TYPE
fact after real alias-base publication; Known interface + forward class twice
publishes both exact edges and passes. The fixture now strengthens fact re-query
after cleanup/replay and explicit interface prerequisites; no additional RED
credit is inferred for these post-RED assertions. Production remains frozen
pending the bounded target-fact/replay design; whole M1/platform gates remain open.

The artifact manifest is at the configured external root:
`E:/cargo-targets/zr_vm/reports/macro-tokenstream-20261008/artifacts.json`.
Registration is explicitly retrospective for products predating the new user
instructions. Five configured output roots scan successfully without following
junctions; source permissions are excluded. One proven regenerable tmp ZRO is
removed by a SHA256/size/time guarded plan after all own native sessions exit:
105357bytes released, zero empty directories removed. The actual checkout,
current reusable build, necessary failure logs, shared caches, foreign products
and unknown candidates are preserved. Win32_Process access is denied; no actual
PID or whole-machine quiet claim is invented. Cleanup is scoped, not full legacy
ownership closure, and the task manifest remains active for pending M1 work.
Focused GREEN does not accept M1 or waive original failures/platform gates.
Symbolic/closed-size, normalization-budget injection and whole M1 remain open.

Upward RED-r3-after-prototype-lifecycle native4010 naturally1/build0/test11
preserves the twenty-case9PASS11FAIL identity losses after the lower lifecycle
gate passes. Root then adds eight real initializer companions with actual seed
annotation conversion, context-aware variable publication and initializer
expression inference before source Holder publication. RED-r4-initializers
native98555 naturally1/build0/test15,28=13PASS15FAIL0ignored. Four new Root
controls pass; the four marked companions fail only the same final token
assertion. Independent fixture review finds no issue. Logs use
`m1-msvc-field-results-red-r4-initializers-*`.

The lower lifecycle fourteen-case fixture and 401 related cases are GREEN,
and its source lease is retired. Root now grants only seven field source paths:
private annotation/move helpers, three producers, generic copying and field
projection. No constructor, property, computed/meta consumer, public ABI,
semantic rollback, matrix waiver or whole M1 acceptance is implied.

The lower function-type conversion RED-r1 native18479 is terminal1/build0/test4,
seven cases/threePASS/fourFAIL/zeroignored. Missing returns with zero/one/two
parameters each pass exact two-array allocation and capacity-byte prerequisites
but free zero arrays. The opaque-return companion stops at a fixture assumption
that neglected eight context-query scratch allocations, with no functional RED
credit. Root corrects only that ledger prerequisite; fresh RED-r2 is terminal1/
build0/test4, threePASS/fourFAIL/zeroignored. Root cases allocate2/free0 and the
marked return allocates10/frees8. All source/child/diagnostic prerequisites pass,
and independent fixture review finds no further issue. Production unchanged/no
repair lease at that checkpoint. Raw logs: `m1-msvc-function-type-lifetime-red-r1-*` and
`m1-msvc-function-type-lifetime-red-r2-*`.

The exact three-file signature/name-builder extraction and common failure
release passes independent static review. Root's complete in-memory inverse
matches prior raw source C41D173D, with no source snapshots or newline churn.
GREEN-r1 native66218 is terminal0/build0/all fifteen programs408PASS0FAIL/
0ignored. GREEN-r2 native89345 adds actual array-parameter recursive ownership
and parsed in/ref/out mode/display companions: fifteen programs410PASS0FAIL/
0ignored, including all nine focused cases. One disjoint foreign frame-roots
source rebuild is recorded, so the entire checkout is not claimed immutable.
Production pins452DC6EE/39F6015E/B11D42F8 and shared headers remain unchanged;
no new own compiler warning beyond established D9025. Lower lease retired.

Upward field `red-r2-after-converter` naturally terminates1/build0/test11,
twenty cases9PASS11FAIL0ignored, same result-token losses after prerequisites.
No field source lease yet. Full baseline and WSL matrix remain open; no M1
acceptance or commit is inferred from this lower focused GREEN.

The genuine twenty-case field fixture runs through extern struct registration,
ordinary struct/class compilation and actual closed generic metadata. RED-r1
native85821 naturally terminates1, build0/test11: nine PASS, eleven FAIL, zero
ignored. All declaration bindings, canonical IDs, annotation conversion,
selected member and receiver prerequisites pass. The eleven failures first
occur at the field result definition token check, expected opaque identity and
actual Root0. Independent fixture review finds no false RED or ownership flaw.
Logs are `m1-msvc-field-results-red-r1-*`; no field production lease or GREEN
credit exists. The next prerequisite is actual allocator RED for the shared
function-type return conversion failure cleanup, followed by unpublished field
publication ownership checks. No M1 acceptance, commit or matrix waiver.

## Generic Support Evidence (2026-10-11)

The ten-case prototype fixture first establishes actual 2 PASS / 8 FAIL after
build0. The identity-key repair then passes all 17 cache/annotation companions.
Separate native r1 exposes two marked constraint failures and seven source
struct-definition failures; Root/mixed-site prerequisites pass. R1 also records
five binary writer prerequisite failures caused by its unsuitable working
directory. Fresh r2 runs in external writable per-target directories and passes
all 125 inference cases along with nominal26/publication38/constraints1/
instantiation3/canonical19/consumers21 and struct definitions9, zero ignored.

The expanded generic fixture in r2 has 23 PASS / 2 FAIL. Both failures are nested
open const forwarding; direct declaration owner, ordinal, cache isolation and
child-scope visibility pass. Passing enclosing const bindings to recursive
canonical conversion repairs those cases in r3: 25 PASS / 0 FAIL / 0 ignored.
Two new source duplicate cleanup cases reproduce retained allocations of 15,000
bytes each, leaving struct definitions at 9 PASS / 2 FAIL. They correctly reject
the second declaration and preserve the original publication. The narrow early
identity guard and both error-recovery companions pass in r4. Three typed-target
handoff failures then arise after correct declaration/canonical/annotation
prerequisites. The `AST_TYPE` handoff uses the structured prototype finder to
repair them; its Root/two opaque/mixed/invalid-context companions run in r6.

R5 is a test-only compile failure at an undeclared private helper; no tests run.
Corrected r6 naturally exits0 with build0 and 278 PASS / 0 FAIL / 0 ignored across
nine programs: generic25, struct definitions/typed targets20, nominal26,
publication38, constraints1, instantiation3, inference125, canonical graph19 and
consumers21. Independent static review confirms the frozen support hashes and
finds no new actionable issue. Later constructor emission lookups remain open.

Immutable report stems are `m1-msvc-generic-prototypes-red-r1/r2` and
`m1-msvc-generic-support-r1/r2/r3/r4/r5/r6` under `E:/cargo-targets/zr_vm/reports`.
R1-r4/r6 builds return0; failing behavioral drivers retain natural exit1.
R5 retains build1 separately. This ledger
does not infer full M1 acceptance from focused support tests. Full foundation,
original pipeline and WSL matrix replays remain required. Full native replay
`m1-msvc-generic-support-final-r1` is terminal1: both builds0, foundation39/39PASS,
aggregate8 at the same catalogue assertions, inventory108PASS/13FAIL. All121
program exits and Unity footer tuples match nominal-publication-r5, zero missing
footers. A fresh WSL distro probe exits1
with `Wsl/EnumerateDistros/Service/E_ACCESSDENIED`; GCC/Clang cannot execute.

The next compiler prototype lookup fixture uses real union declarations and
parsed type-value aliases. Its first attempt omits a private function declaration
and marks the wrong generic AST arm; the owned probe is stopped and no behavioral
credit is assigned. Corrected red-r2 builds0 and naturally exits1 with19cases,
10PASS/9FAIL/0ignored. Strengthened red-r3 builds0 and naturally exits1 with
22cases,10PASS/12FAIL/0ignored. Six compiler Root query and six alias cases fail
after structured producer/materializer prerequisites pass. The ordinary/mixed
Root, imported-null metadata and compiler direct/alias first-registration
compatibility companions pass. Immutable stems are
`m1-msvc-compiler-prototype-lookup-red-r1/r2/r3`. The narrow lookup repair passes
fresh `m1-msvc-compiler-prototype-lookup-green-r1`: build0, twelve programs,
352PASS/0FAIL/0ignored. Independent read-only review finds no actionable issue.
Full replay `m1-msvc-compiler-prototype-lookup-final-r1` is terminal1 with both
builds0, foundation40/40PASS, aggregate8 and inventory108PASS/13FAIL. Structured
comparison of all121 original programs against generic-support-final-r1 checks
exit codes, every Unity footer and failed-test names/messages: zero changes,
zero missing footers. Only hexadecimal memory-pointer values are normalized;
the immutable comparison log ends in `-final-r1-compare.log`.
Receiver field/call selection and constructor consumers remain separate gates.
The existing catalogue and pipeline failures, artifact replay and unavailable
WSL GCC/Clang matrix still prevent complete M1 acceptance or a milestone commit.

Receiver selection's genuine parsed field/method fixture establishes actual
RED in `m1-msvc-receiver-members-red-r1`: build0, natural driver1/test10,
18cases8PASS/10FAIL/0ignored. All declaration, annotation, keyed prototype,
real field/method metadata, variable and identifier prerequisites pass; Root
and mixed-site controls pass. Independent fixture review has no actionable
finding. Five field and five method ordinary/public consumers drop identity.
The private repair's green-r1 builds0 and naturally exits1: receiver32PASS/2FAIL,
all twelve related programs352PASS/0ignored. The two failed growth fixtures
mistake a compile type descriptor's unknown member for a static field and earn
no growth credit. Root corrects only the fixture to an untyped source parameter
and direct type descriptor, preserving the capacity/length assertions and logs.
Corrected green-r2 naturally exits0 after build0, all13 programs386PASS/0ignored
including all34 receiver cases and two observed capacity-growth cases. Frozen
production hashes are unchanged. Read-only review's same-spelling class
inheritance candidate is separate and needs actual producer/hierarchy RED.
Full `m1-msvc-receiver-members-final-r1` is terminal1: both builds0,
foundation41PASS/aggregate8/remaining121 inventory108PASS13FAIL. Fresh structured
comparison to compiler-prototype-lookup-final-r1 has zero exit/footer/failure
message changes and zero missing footers. Neither focused support success nor
an unchanged failing baseline accepts M1. Details and
remaining result-carrier obligations are in
[receiver member identity](../parser-and-semantics/macro-receiver-member-identity.md).

Inherited member support has a separate genuine class/canonical hierarchy gate.
The immutable red-r1 builds0 and naturally exits1,14cases5PASS9FAIL/0ignored;
seven method cases stop at a wrong member/call AST count and earn no method RED
credit. Corrected red-r2 naturally exits1/build0/test4,14cases10PASS4FAIL/0ignored.
Root/mixed/distinct opaque controls pass; all exact source bindings, nominal
IDs, direct BASE_TYPE endpoints, base metadata, selected receiver and identifier
prerequisites pass. Four same-name inherited field/method direct/public consumers
get OBJECT instead of INT64. Core's new lease only addresses the recursive
self-cycle identity check, not base producers or field-result metadata. The
strengthened fifteen-case fixture adds actual member/call/argument AST checks
and a real const-generic open/two-closed/snapshot identity companion. Green-r1
naturally exits0/build0, fourteen programs401PASS/0FAIL/0ignored; two own fixture
snapshot variables trigger C4701. Root-only explicit initialization removes
those warnings. Fresh green-r2 again naturally exits0/build0 with401PASS and
unchanged production hashes. Independent static review has no actionable
regression; actual hierarchy cycles, AST-null cycles and generic class inheritance
remain unclaimed. Full inherited-members-final-r1 naturally exits1: both builds0,
foundation42PASS/aggregate8/remaining121 inventory108PASS13FAIL. Exact comparison
against receiver-members-final-r1 finds zero exit/footer/failed-message changes
and zero missing footers. Independent Root in-memory inverse checks match all
three pre-repair production hashes. M1 remains active and unaccepted.

## Latest Native Evidence (2026-10-10)

This section preserves the historical 2026-10-10 endpoint and its then-later
follow-ups. Its supersession of earlier failing snapshots is local to that
history; R28 above is the latest focused checkpoint. The source was the
then-current checkout, built at
`E:/cargo-targets/zr_vm/build/m1-msvc-verified` with MSVC 19.44.35228.0,
CMake 3.31.6-msvc6, VS Ninja, Debug, SDK 10.0.26100.0, and static libraries.
The original focused replay enabled `ZR_VM_TESTS_MACRO_FOUNDATION_ONLY`.
The subsequent broader configuration disables it to include language-pipeline
and artifact/provider targets.

Ninja records actual include dependencies: current inspections found 172
dependencies for token_stream_batch.c, 182 for compiler_closure.c, 186 for
compile_time_decorator_execution.c, and 176 for parser_expression_primary.c.
A temporary external compiler launcher converts localized `/showIncludes`
output to UTF-8. This avoids the old zero-dependency cache which mixed public
structure sizes. Two later incremental primary links reported LNK1236 while
other executables linked from the same archive; object headers were readable,
and the exact links passed on serial replay. Disabling incremental linking
with `/INCREMENTAL:NO` produced the final successful 30-target rebuild.

| Surface | Observed RED | Current native result |
| --- | --- | --- |
| Borrow/Loan closure hygiene | Def-site borrowed capture used an unrelated root integer type. | 8/8 hygiene cases pass after effective ownership accounts for reference access as well as qualifiers. |
| Module-alias predeclaration hygiene | Two of three new cases failed: a definition binding was missing and the later definition alias overwrote the root module type. | 3/3 cases pass after predeclaration preserves the pattern's context; existing hygiene remains 8/8. |
| Decorator/frame lifetime | A returned Patch could outlive its callee root. | 7/7 cases pass, including forced GC across struct and real parameter transform boundaries and parameter failure root restoration. |
| Metadata budget | Seven of eight initial cases returned uncharged storage; after storage repair two native decode quota cases still returned partial arguments. | 10/10 cases pass, including exact initial/growth name-metadata heap deltas. |
| Primary failure ownership | Three initial append cases returned partial base trees; later cases exposed switch cleanup, duplicate defaults, an unconsumed split closing angle and a fatal redundant declaration trial. | Latest primary replay: 37/37 cases pass, CTest exit 0; text/native inputs check full consumption, budget rejection, zero native storage and root restoration. |
| Original complete foundation configuration | Initial hygiene/metadata REDs were excluded only in a separately labelled 27/27 diagnostic run. | Prior full CTest: 30/30 PASS, exit 0, 21.90 seconds. After declaration-dispatch repair, the expanded configuration passes 24 M1 entries, 3 provider entries and the test-log runner contract: 28/28, exit 0, 6.41 seconds. |

The primary quota guards count discarded parser probes in the resource ledger;
they do not assume that budget node count equals the final tree size. An
unlimited parse verifies the corresponding complete shape before rejecting the
last wrapper/constructor allocation. A forwarding allocation observer prepares
reusable GC-domain root-table capacity before monitoring parser storage, so
long-lived domain infrastructure is not misreported as a parser leak.

The metadata cursor warmup prepares ownership capacity but does not cache native
token decoding. Quota cases therefore include replayed token string charges.
Separate identical-source parses with and without returned names assert the
exact metadata heap difference, including the full new backing allocation on
growth. Shared-ledger exhaustion is fatal at the argument-list boundary, even
when first reported by the lexer.

Exact logs are under `E:/cargo-targets/zr_vm/reports/`:
`m1-msvc-verified-build.log`, `m1-msvc-verified-focused-ctest.log`,
`m1-msvc-verified-other-ctest.log`,
`m1-msvc-verified-primary-segment-red.log`,
`m1-msvc-verified-primary-cleanup-build.log`, and
`m1-msvc-verified-all-ctest.log`. These are earlier checkpoint logs; the scoped
alias follow-up below records the latest native foundation replay.

Current WSL startup still returns `Wsl/EnumerateDistros/Service/E_ACCESSDENIED`.
No current GCC or Clang result is credited. Current native provider results
pass; the complete language-pipeline replay and its diagnostic inventory are
recorded below.
M1 remains unaccepted and uncommitted until the required matrix and broader
gates are complete.

### Broader Native Replay And Switch Follow-Up

The same build directory was expanded with
`ZR_VM_TESTS_MACRO_FOUNDATION_ONLY=OFF`. Artifact tests require
`BUILD_THREAD_LIB=ON` and compiler-integration tests require
`BUILD_NETWORK_LIB=ON`; disabled configurations failed to link
`ZrVmThread_Register` and `ZrVmLibNetwork_Register`, respectively. Both
dependencies were enabled through existing CMake options. The artifact replay
is 3/3 CTest PASS (`binary_metadata_source`, `compile_time_import_ownership`,
`artifact_schema`), exit 0, 1.31 seconds. The separate CompileTool project-import
executable passes all nine cases, including provider execution, transitive
failure rollback, and phase-cycle diagnostics.

All 41 `language_pipeline` smoke executables built. The actual smoke aggregate
returned exit 8 after 117.21 seconds: instruction execution passed 31/31,
then compiler integration ran 127 cases with 14 failures and stopped the
aggregate. These are current observed failures, not yet certified as baseline
differences. They include native callback escape flags, generator closure
graph serialization, imported-project lowering, quickening assertions, and
repeated generic-constructor compilation. Full language-pipeline acceptance
is still open.

Read-only parser review also found switch continuation cleanup gaps. Six
additional primary cases produced 26 cases / 4 failures: default-wrapper node
rejection retained six allocations, two later malformed case exits retained
twelve each, and duplicate defaults were accepted. Successful branch shape
and ordinary case-wrapper rejection companions passed. The repair moves
switch branch assembly into `parser_switch.c`, uses one deep-cleanup failure
exit, and rejects a duplicate default before replacing the previous branch.
The shared-dependency rebuild completed. All 26 switch/primary cases passed,
but a twenty-seventh nested generic constructor case failed at the parser with
`Expected primary expression`. Four precise follow-ups then recorded 30 cases
with four failures: text/native nested construction and text/native closing
angle consumption. Both closing-angle probes expected byte 1 after consuming
the first `>`, but observed byte 0. The lexer split contract deliberately leaves
the first piece current; the parser had failed to advance it. The parser now
advances once after a successful split. The resulting 30-case primary replay
passes, CTest exit 0, 0.23 seconds, with exact span/lookahead and native/root
cleanup checks. The full 126-target language-pipeline manifest and M1/provider
targets are being rebuilt for broader replay; no aggregate pass is inferred.

Exact follow-up logs: `m1-msvc-core-regression-ctest.log`,
`m1-msvc-compile-tool-import.log`,
`m1-msvc-language-pipeline-smoke-build-network.log`,
`m1-msvc-language-pipeline-smoke-ctest.log`, and
`m1-msvc-switch-red-ctest.log`, `m1-msvc-switch-green-ctest.log`,
`m1-msvc-generic-red-ctest.log`, `m1-msvc-generic-green-build.log`, and
`m1-msvc-generic-green-ctest.log` under the report directory above.

### Full Native Build And Path-Length Repair

The full manifest contains 126 executables. The first build invocation used
output names as target names and stopped before compilation on a shortened
target/output-name mismatch. Replaying the actual Ninja outputs reached a
current MSVC `C1083` on the existing zero-inclusive sum dataflow test; a serial
replay reproduced it. Its object path was 269 characters, and the longest
selected object path was 273 characters. This matches the earlier CMake
object-path warning, not a source-code diagnostic.

A checked junction at `E:/cargo-targets/zr_vm/build/m` points to the same
`m1-msvc-verified` build tree. The two longest tests then compiled and linked,
exit 0. Continuing the remaining 473 build steps through that short alias
completed, exit 0, without changing production/test names, assertions, compiler
flags, or source files. All 126 manifest outputs and M1/provider targets linked
from the then-current archive. The fresh 25-entry foundation/runner CTest replay passed, exit
0, 49.71 seconds. The five direct contracts also passed: 69 evaluator, two
comptime contract, 14 runtime contract, six declaration transform, and three
decorator cases. Fresh provider/artifact CTest passed 3/3 and the direct project
import suite passed 9/9. These focused results do not establish aggregate
language-pipeline acceptance.

Path/build logs under the same report directory:
`m1-msvc-full-pipeline-rebuild.log`,
`m1-msvc-full-pipeline-output-rebuild.log`,
`m1-msvc-long-path-serial-rebuild.log`,
`m1-msvc-long-path-short-alias-rebuild.log`, and
`m1-msvc-full-pipeline-short-alias-rebuild.log`.
`m1-wiki-source-validation.log` records source validation passing for 116
Markdown files, 115 manifest pages and 648 local links.

### Top-Level Call Dispatch Follow-Up

The full pipeline stopped at its first executable: parser tests ran 74 cases
with two failures, static-import member chains and explicit generic function
calls returning NULL. Four source/native complete-script companions reproduced
the failures. Top-level dispatch had already checked declaration shapes, then
redundantly tried every identifier followed by `(` or `<` as a function
declaration. The missing `fn` legacy diagnostic set fatal; the M1 rollback
contract correctly preserved it. Removing the redundant trial keeps actual
resource failures fatal and leaves calls on the ordinary expression path.

The existing parser suite now passes 74/74. The new storage observer initially
reported a 384-byte allocation after the longest combined generic script;
separate component cases all passed. This allocation matches GC-domain root
table growth, which persists until VM state destruction. Warming 64 simultaneous
root handles before observing parser storage removes this infrastructure from
the measurement without relaxing zero native parser blocks or root-restoration
assertions. All 37 primary cases pass, CTest exit 0, 0.95 seconds. The subsequent
full language-pipeline and foundation/provider relink completed in 156 steps,
exit 0. The CMake test-log driver is a command-based test, not a native output,
and was excluded from the native build target selection only.

Exact logs: `m1-msvc-full-language-pipeline-ctest.log`,
`m1-msvc-parser-test-direct.log`, `m1-msvc-parser-integration-red-ctest.log`,
`m1-msvc-parser-integration-green-build.log`, `m1-msvc-parser-test-green.log`,
`m1-msvc-generic-probe-storage-red-ctest.log`, and
`m1-msvc-parser-integration-final-ctest.log` in the report directory above.

### Complete Native Diagnostic Inventory

The fresh post-dispatch CTest run passes all 24 independent M1 entries, all
3 provider entries and the test-log runner contract, exit 0, 6.41 seconds. Existing standalone contracts also
pass: comptime budget 2/2, runtime contract 14/14, and declaration transform
6/6. The aggregate language pipeline still fails, CTest exit 8, 10.39 seconds.
Its first four executables pass (parser 74/74, instruction execution 31/31,
type inference 125/125, semantic facts 17/17); semantic query runs 30 cases
with two failures and stops the original runner.

The semantic-query failures are a committed catalogue inconsistency. Commit
`831f8081` added 23 SSA optimization remarks to the registry, giving 94
descriptors. The test still expects 71 and the message table still contains
142 entries rather than the required 188. The registry, message table and
test are unchanged by this macro work. This finding explains the aggregate's
first failure; it does not waive the aggregate gate.

Root then ran each of the remaining 121 manifest executables through the
unchanged `run_executable_suite.cmake`, continuing between separate diagnostic
invocations. This inventory is not a replacement aggregate and does not change
the accepted manifest or assertions. It completes with 108 passing executables
and 13 failing executables:

| Diagnostic executable | Cases / failures | Current observation |
| --- | --- | --- |
| `zr_vm_compiler_integration_test` | 127 / 13 | Native-handle escape flags, receiver layout, four project-import results and five quickening assertions fail. Repeated generic-constructor calls now pass. |
| `zr_vm_aot_c_source_contracts_test` | 26 / 5 | Required generated C text is absent, or forbidden frame-cleanup text is present. |
| `zr_vm_aot_c_guardrail_contracts_test` | 6 / 1 | The direct scalar-call output contains forbidden value handling. |
| `zr_vm_aot_c_value_semir_contracts_test` | 8 / 1 | Required frame-alias source text is absent. |
| `zr_vm_aot_c_logical_contracts_test` | 4 / 4 | Required logical/equality source text is absent. |
| `zr_vm_aot_c_global_contracts_test` | 9 / 1 | Required meta-value boundary source text is absent. |
| `zr_vm_aot_c_scope_contracts_test` | 1 / 1 | Required scope-lifecycle boundary source text is absent. |
| `zr_vm_literal_surface_test` | 57 / 1 | Basic cast fixture fails literal lowering into pre-execution Semantic IR. |
| `zr_vm_lexer_parser_compiler_execution_test` | 14 / 1 | The same cast fixture fails in the full parser/compiler path. |
| `zr_vm_exceptions_test` | 8 / 2 | Two finally paths fail entry into semantic cleanup. |
| `zr_vm_module_system_test` | 78 / 2 | Module-init cache summary and wrapper-box compilation fail. |
| `zr_vm_execbc_aot_pipeline_test` | 98 / 13 | Generated backend contracts, quickening, binary container values and property/meta paths fail. |
| `zr_vm_meta_call_pipeline_test` | 4 / 1 | Dynamic-call backend runtime-contract text is absent. |

These 13 failures have not been certified as baseline differences. The frame
roundtrip case fails at `test_compiler_features.c:2798`, before binary writing:
the source wrapper has no `INLINE_RECEIVER_ARGUMENT` layout. Both escape-shape
failures are the missing `NATIVE_HANDLE` bit at
`test_escape_metadata_pipeline.c:731`; ordinary closure-capture assertions
before that point pass. These precise lower-layer observations avoid treating
every roundtrip-named failure as a serialization defect.

Exact logs under the report directory are
`m1-msvc-post-dispatch-full-relink-native.log`,
`m1-msvc-post-dispatch-foundation-provider-ctest.log`,
`m1-msvc-post-dispatch-full-language-pipeline-ctest.log`,
`m1-msvc-post-dispatch-zr_vm_comptime_contract_test.log`,
`m1-msvc-post-dispatch-zr_vm_comptime_runtime_contract_test.log`,
`m1-msvc-post-dispatch-zr_vm_declaration_transform_contract_test.log`, and
`m1-msvc-post-dispatch-language-diagnostic-summary.log`. Individual inventory
logs use `m1-msvc-language-diagnostic-<executable>.log`.

After shared compiler/build-script updates, native output refresh completed
with exit 0 (142 native build steps after CMake regeneration; five path-length
warnings and two existing MSVC `/W3` to `/W4` override warnings are retained).
The fresh 24 M1 plus 3 provider replay passes 27/27, exit 0, 10.05 seconds;
the runner contract separately passes 1/1. Existing execution/budget/runtime/
transform/decorator/import contracts pass 69/2/14/6/3/9 cases. The fresh full
aggregate again fails at the same two catalogue assertions, exit 8, 22.15
seconds. These results are in `m1-msvc-post-inventory-build.log`,
`m1-msvc-post-inventory-foundation-provider-ctest.log`,
`m1-msvc-post-inventory-test-log-contract.log`,
`m1-msvc-post-inventory-full-language-pipeline-ctest.log` and
`m1-msvc-post-inventory-<contract>.log`.

The next source review found that module-alias predeclarations still registered
the root context. A new `macro_syntax_hygiene_aliases` target adds three focused
cases: definition/root aliases in both source orders, and a call-site/root
normalization companion. The first run records two real failures (missing
definition binding and overwritten root module type) and one passing companion.
This target is additional to the preceding 24-target snapshot. Its initial
compile warned about a missing private function declaration. After including
the actual private header, the unchanged assertions again record 3 cases / 2
failures, without the declaration warning. Production predeclaration now uses
the existing context-aware variable registration API and the pattern's context.
All three cases pass, as do the existing eight hygiene cases.

The final targeted rebuild completes with exit 0. Foundation/provider/runner
CTest passes 29/29, exit 0, 17.03 seconds: 25 M1 entries, 3 provider entries and
1 runner contract. Existing direct execution/budget/runtime/transform/decorator/
provider-import contracts pass 69/2/14/6/3/9 cases; parser/instruction/type-
inference/semantic-facts pass 74/31/125/17 cases. Module-system replay still has
the same two failures out of 78 cases at lines 2432 and 4789. The broader
121-program diagnostic inventory predates this narrow alias repair; it is not
represented as a second complete replay. WSL status retry still exits 1 with
`Wsl/EnumerateDistros/Service/E_ACCESSDENIED`.

Alias repair logs are `m1-msvc-module-alias-red-declared-build.log`,
`m1-msvc-module-alias-red-declared-ctest.log`,
`m1-msvc-module-alias-green-build.log`, `m1-msvc-module-alias-green-ctest.log`,
`m1-msvc-module-alias-regression-build.log`,
`m1-msvc-module-alias-regression-ctest.log` and
`m1-msvc-module-alias-<contract>.log`. No milestone is accepted or committed.

### Callable And Local Metadata Follow-Up

The alias target now contains 21 cases. Five callable lookup/shadowing cases
first recorded four REDs: root lookup selected a definition-site return type,
the root overload set included another context, equal signatures collapsed,
and an unrelated definition-site variable hid an outer root function. After
context-aware lookup and registration, all eight cases then present passed.
Three source-alias/identifier/call-inference cases subsequently recorded three
REDs and passed after the identifier context reached callable registration,
candidate lookup, inference, and overload selection.

Executable companions then exposed two distinct defects. Named calls selected
the first script declaration by spelling, reporting `Unknown argument name:
visible` for the root declaration. Marked function parameters executed but
published no typed call cache and accepted an incompatible string argument.
The corrected 18-case fixture recorded four actual failures, including an
annotated function-valued local. Named arguments now use the resolved
declaration, and typed parameter registration carries the parameter context.

Root and marked companions showed that annotated locals initialized by a call
returning a function value both lacked callable metadata. The 20-case snapshot
recorded three failures: two invalid calls compiled and a valid marked call
published no typed cache. Local function-type annotations now use the existing
structured signature registrar. Equivalent existing lambda metadata must be
reused: a shared type-inference replay exposed two duplicate-registration
regressions, repaired by comparing context, signature, and passing modes.
Five additional binary-fixture write failures in that first direct replay came
from using the nonwritable repository root as its working directory. The fresh
replay in `E:/cargo-targets/zr_vm/tests` passes all 125 type-inference cases.

The final new metadata case returned the correct integer at runtime while its
typed local row reported the same-name root variable's string type (expected 5,
observed 12). Typed metadata now queries the context in the compiler's parallel
local identity table. Runtime row layout is unchanged. The 21-case suite passes
without changing its assertions.

The final native build completed with exit 0. CTest passes 30/30, exit 0,
21.52 seconds: 25 M1 entries, 3 provider entries, 1 typed-call-binding entry and
the runner contract. Direct execution/budget/runtime/transform/decorator/
project-import contracts pass 69/2/14/6/3/9 cases. Parser, instruction,
semantic-facts, type-inference and named-argument suites pass 74/31/17/125/10.
Typed-call-binding also passes its 12 cases. Existing compiler warnings remain;
this is not a zero-warning build or M1 acceptance.

Reports under `E:/cargo-targets/zr_vm/reports` include
`m1-msvc-callable-hygiene-red.log`, `m1-msvc-callable-propagation-red.log`,
`m1-msvc-callable-execution-red.log`,
`m1-msvc-callable-result-contract-red.log`,
`m1-msvc-hygiene-local-metadata-red.log`,
`m1-msvc-hygiene-local-metadata-green.log`,
`m1-msvc-callable-final-regression-build.log`,
`m1-msvc-callable-final-foundation-provider-ctest.log` and the
`m1-msvc-callable-final-zr_vm_*.log` direct contract reports. The full aggregate
and the GCC/Clang matrix remain separate required gates.

The post-callable full pipeline build completed with exit 0 (220 steps, all
126 original manifest outputs). The unchanged `language_pipeline` CTest then
failed with exit 8, total 50.94 seconds. Parser/instruction/type-inference/
semantic-facts pass 74/31/125/17 cases before semantic query fails 30/2. The
same assertions expect 71 descriptors but observe 94, and expect 188 message
entries but observe 142. This is recorded in
`m1-msvc-callable-full-pipeline-build.log` and
`m1-msvc-callable-full-pipeline-ctest.log`. The earlier separate 121-program
inventory is historical, not a post-callable replay or acceptance waiver.

### Type-Value Alias Follow-Up

Three new cases expanded the alias suite from 21 to 24 and produced three
actual REDs: predeclaration expected two bindings but found one, a marked
`int[]` annotation resolved to `string[]` (element type expected 5, observed
12), and compilation failed. Shared context-aware type-value registration and
annotation lookup make the unchanged 24-case snapshot pass with exit 0.
Three additional companions check that definition-only aliases are invisible
to root/call-site lookup, that a later definition alias preserves the root
annotation, and that call-site aliases remain usable. The resulting 27-case
suite passes with zero failures/ignores and exit 0.

Root extracted repeated registration/lookup into `compiler_type_value_alias`
and routed name-only prototype/metadata fallbacks through root lookup. The
module retains existing inferred-type ownership and does not claim allocation
failure hardening. Imported/destructured and nominal type paths remain separate
audit work. Native reports include `m1-msvc-type-value-hygiene-red.log`,
`m1-msvc-type-value-hygiene-first-green.log`, and
`m1-msvc-type-value-hygiene-companion-green.log`. The first regression build
stopped at an incorrectly spelled project-import target. The corrected build
completed with exit 0, and fresh post-type-value CTest passes 30/30, exit 0,
37.26 seconds. Parser/instruction/semantic-facts/type-inference/named-argument
direct suites pass 74/31/17/125/10; decorator/project-import pass 3/9.

The first direct comptime replay hung after invalid-interface diagnostics.
Inspection found an old test manually constructing `SZrTypeBinding` without
initializing its newly consulted context or optional fields. Root stopped only
that identified replay process and preserved its log. The fixture now starts
from zero and explicitly sets root context; its original alias-duplicate
rejection assertion is unchanged. After rebuilding, execution/budget/runtime/
transform suites pass 69/2/14/6, metadata TypeRef passes 9 and semantic type
display passes 29 cases, all exit 0. Other manual type-value fixtures already
zero their binding rows. Reports are `m1-msvc-type-value-regression-build-r2.log`,
`m1-msvc-type-value-foundation-provider-ctest.log`,
`m1-msvc-type-value-fixture-regression-build.log`, and
`m1-msvc-type-value-r2-zr_vm_*.log`. A new WSL toolchain probe still refuses
startup with `Wsl/Service/E_ACCESSDENIED` (exit -1); no GCC/Clang process ran.
Wiki source validation passes 116 Markdown files, 115 pages and 648 links.
These focused results do not accept the full pipeline or M1.

### Imported Type-Value Alias Follow-Up

The new `macro_syntax_hygiene_import_aliases` target first checks real
`zr.math` registration and source-root alias publication. The first build
failed because root added an unnecessary nonexistent library target; removing
that line uses the existing link helper. Rebuilt seven-case tests then record
four functional REDs: definition alias missing, definition alias exposed to
root, and both declaration orders rejecting distinct-context aliases. Root,
call-site and same-context duplicate prerequisites pass. Report:
`m1-msvc-import-alias-context-red-r2.log` (exit 4).

Registration moves from the oversized statement module into private
`compiler_import_type_alias.c/.h`. It retains the destination identifier,
checks/records normalized local identity, and publishes only normalized root
aliases into the existing name-only registry. A broader build command first
stops on an obsolete comptime target name. The retry uses real output paths,
but CMake generation fails while a concurrent SSA registration references a
not-yet-written test file. Root preserves the foreign registration. Once that
file appears, the original build configuration succeeds (exit 0), and all
unchanged seven cases pass with zero failures/ignores, exit 0. These command
and configuration failures are not functional RED evidence. Reports:
`m1-msvc-import-alias-context-green-build.log`,
`m1-msvc-import-alias-context-green-build-r2.log`,
`m1-msvc-import-alias-context-green-build-r3.log`, and
`m1-msvc-import-alias-context-first-green.log`.

The first constructor companions use unsupported native alias-name construction
and an incorrect AST-kind prerequisite. Root replaces those invalid fixtures
with the existing qualified module-member syntax. A second attempt reaches a
`Vector2` constructor mismatch, which has no validated prerequisite and is not
credited as a hygiene RED. The final runtime companion uses the validated
`Vector3` constructor beside a root `int[]` alias with the same name. Annotation
inference keeps both identities, and native member access plus array indexing
returns 18. All ten cases pass, exit 0. Shorthand import, source-member context
independence, invalid context rejection and duplicate preservation expand the
suite to 13 passing cases. Its fresh independent CTest passes 1/1, exit 0,
15.42 seconds. Logs retain all attempts under
`m1-msvc-import-alias-context-companion*` and
`m1-msvc-import-alias-context-boundary*`.

Post-import full 126-output pipeline and foundation rebuilds complete with
exit 0. Fresh foundation/provider/typed-call/runner CTest passes 31/31, exit 0,
50.60 seconds. The unchanged aggregate fails the same two catalogue assertions
after parser/instruction/type-inference/semantic-facts pass 74/31/125/17 cases;
CTest exits 8, 36.00 seconds. Direct post-import regressions pass execution,
budget, runtime, transform, decorator, project import, TypeRef, semantic display,
symbol query and named arguments with 69/2/14/6/3/9/9/29/42/10 cases. Module
system retains the same two assertions out of 78: source-summary initialization
and wrapper helpers. The preceding complete 121-program inventory predates the
import repair and is not a post-import full replay. No failure is waived.

Reports use `m1-msvc-import-alias-final-*`. Wiki validation passes 116 Markdown
files, 115 pages and 648 links. A fresh WSL GCC probe exits -1 with
`Wsl/Service/E_ACCESSDENIED`; neither GCC nor Clang ran. Its raw report is
`m1-import-alias-wsl-toolchain-probe.log`. M1 remains unaccepted and uncommitted.

### Exception Flow Context Follow-Up

Two finally preflight cases expanded the suite to 29 and recorded two actual
REDs. A marked call incorrectly accepted a root function, while a marked call
to a definition-only function was rejected. The root-call prerequisite and
context-aware registration prerequisites passed before those assertions.
Finally now passes the target context into callable lookup. All 30 cases pass
after adding a runtime assignment/cleanup companion returning 8. Existing CFG
finally/try-catch/typed-catch/typed-loop suites pass 7/11/8/6 cases.

The first catch snapshot has six failures, but four are fixture prerequisite
failures: `let` makes an immutable alias, outside the existing mutable alias
route. Root changed only that fixture spelling to `var`. The rebuilt 36-case
snapshot then records six functional REDs: incorrect parameter read and return
identity, incorrect alias initializer and destination read, false shadowing by
an unrelated root variable, and failure to keep fallback for an existing
matching definition binding. Catch preflight now keeps full identifiers and
compares normalized local identities; variable and callable collision queries
carry those contexts. All unchanged 36 cases pass. Three positive/runtime
companions expand the suite to 39 PASS, zero failures/ignores, exit 0. The
runtime case returns caught payload 7 beside a root variable holding 99.

Reports include `m1-msvc-finally-call-context-red.log`,
`m1-msvc-finally-call-context-green-build.log`,
`m1-msvc-finally-context-zr_vm_*.log`, `m1-msvc-catch-context-red.log`,
`m1-msvc-catch-context-red-r2.log`, `m1-msvc-catch-context-green-build.log`,
`m1-msvc-catch-context-zr_vm_*.log`, and
`m1-msvc-catch-context-companion.log`.

The post-exception foundation and complete 126-output pipeline rebuilds both
completed with exit 0. Fresh foundation/provider/typed-call/runner CTest passes
30/30, exit 0, 22.78 seconds. The unchanged aggregate still stops at the same
two semantic-query catalogue assertions, exit 8, 55.67 seconds; its preceding
parser/instruction/type-inference/semantic-facts programs pass 74/31/125/17
cases. This aggregate remains a failed acceptance gate.

A diagnostic driver invokes the original runner separately for the remaining
121 manifest programs. It initially records 108 passing programs, 12 functional
failures and one compiler-integration timeout under its temporary 120-second
limit. A separate invocation of the original runner without that limit then
finishes naturally: compiler integration reports 127 tests / 13 failures and
exit 13 (runner exit 1). Its timeout record is preserved, rather than rewritten
as a completed result. Combined with this separate replay, all 121 programs
have terminal results: 108 pass and 13 fail. The failed program names and case
counts match the preceding inventory. Context-sensitive CFG and comptime
execution programs pass in this fresh inventory; the earlier catalogue and
other pipeline failures are not waived.

Reports are `m1-msvc-hygiene-final-foundation-build.log`,
`m1-msvc-hygiene-final-full-pipeline-build.log`,
`m1-msvc-hygiene-final-foundation-provider-ctest.log`,
`m1-msvc-hygiene-final-full-pipeline-ctest.log`,
`m1-msvc-hygiene-final-pipeline-inventory.log`, the per-program logs and original
`results.tsv` under `m1-msvc-hygiene-final-pipeline-inventory/`, and
`m1-msvc-hygiene-final-compiler-integration-natural.log`. Wiki validation passes
116 Markdown files, 115 pages and 648 links. WSL GCC/Clang remain unexecuted
after the recorded startup denial. No M1 acceptance or commit is inferred.

### Lexical Alias Scope Follow-Up

The dedicated `macro_syntax_hygiene_alias_scope` target has 15 cases. Its first
bare-brace fixtures parsed as object expressions and receive no functional
scope evidence credit. Corrected function/if block fixtures established one
outer-binding prerequisite PASS and seven actual REDs: overwritten parent
storage, missing restoration, context leakage, failed real block/function
execution, escaping function-local aliases and failed-block cleanup.

`SZrScope` now records its alias-array entry boundary. Registration updates
only the current scope, reverse lookup selects the nearest visible identity,
and exit frees/truncates new rows without allocation. The outer compiler scope
retains aliases for later metadata queries. Error exits still pop scopes and
release alias storage; instruction emission retains its error guard. Block
predeclaration failure exits its entered scope explicitly. All eight unchanged
cases passed after the shared-header rebuild.

Seven companions cover same-scope predeclaration, three nested scopes, root
retention, lambda execution and real imports. The independent root qualified
Vector3 constructor prerequisite passed. Two inner-import companions then
failed with duplicate type names against enclosing primitive/imported aliases.
Import duplicate checks now use the current alias scope; only normalized-root
aliases in the retained outer scope publish to the name-only type registry.
All 15 scope, 39 existing alias and 13 import cases pass unchanged after this
repair. This does not establish nominal declaration/canonical identity hygiene.

The full language-pipeline build completed in 203 steps, followed by a 41-step
foundation/provider rebuild, both exit 0. Fresh CTest passed all 32 selected
foundation/provider/typed-call/runner entries, exit 0, 180.62 seconds. The
unchanged full aggregate returned exit 8 after 89.39 seconds at the same two
diagnostic-catalogue assertions: expected 71 / actual 94, expected 188 / actual
142. Its first four programs passed 74/31/125/17 cases. The remaining original
121 manifest programs completed with the unchanged runner and no temporary
per-program timeout: 108 PASS / 13 FAIL. The twelve other failing programs have
the same case/failure/ignored counts as the earlier inventory. Compiler
integration now finishes naturally at 127 cases / 13 failures / 0 ignored;
the older TSV's temporary timeout and separate natural replay remain distinct.
These are post-alias-scope results, preceding the nominal lookup repair below.
No aggregate PASS or baseline waiver is inferred.

Raw logs under `E:/cargo-targets/zr_vm/reports/` include
`m1-msvc-alias-scope-red-r2.log`, `m1-msvc-alias-scope-first-green.log`,
`m1-msvc-alias-scope-companion.log`,
`m1-msvc-alias-scope-import-green-zr_vm_syntax_hygiene_alias_scope_test.log`,
`m1-msvc-alias-scope-final-full-pipeline-build.log`,
`m1-msvc-alias-scope-final-foundation-build.log`,
`m1-msvc-alias-scope-final-foundation-provider-ctest.log`, and
`m1-msvc-alias-scope-final-full-pipeline-ctest.log`. Earlier failed setup and
functional RED logs remain preserved separately. No M1 acceptance or commit is
claimed; current GCC/Clang and complete broader gates remain pending.

### Nominal Type Context Follow-Up

`macro_syntax_hygiene_nominal_types` is an independent target. Its initial five
cases built successfully and ran with one root registration prerequisite PASS
and four functional REDs, zero ignored, CTest exit 8. Actual parsed struct
declarations demonstrate lost symbol context, publication to the root type-name
table, absent second same-spelling declaration identity and root lookup choosing
an earlier opaque definition. Canonical distinctness remains behind the
missing-second-symbol assertion; that initial run does not independently prove
the canonical comparison branch.

Root extracted source declaration lookup into `compiler_type_declaration.c/.h`.
It compares spelling and normalized item context; name-only calls remain root
queries. The original query assertion is unchanged. Three companions check
definition-side lookup, rejection when only opaque declarations exist and
actual CallSite compatibility. Full native pipeline/foundation builds passed,
exit 0. The 33-entry CTest replay had 32 PASS / 1 nominal FAIL in 130.53 seconds;
nominal ran eight cases with five PASS / three registration FAIL / zero ignored.
The original query failure and all query companions passed. Unchanged aggregate
returned exit 8 in 62.84 seconds at the same catalogue assertions; all remaining
121 programs naturally completed 108 PASS / 13 FAIL. Every executable result
matches the post-alias-scope inventory.

Type-environment registration now stores spelling/item-context bindings with
declaration and semantic identity. Root registrations retain the legacy
canonical projection and name table; opaque registrations use their reserved
SymbolId as a distinct canonical nominal definition token and publish a
context-qualified semantic symbol. Environment New/Free initialize/release the
new backing array. The shared-ABI focused rebuild completed and the unchanged
eight-case nominal target passed, CTest exit 0, 0.77 seconds.

Six companions for reversed order, repeated registration, explicit context
lookup, mixed-site item compatibility, parent-environment shadowing and invalid
context preservation expand the target to 14 cases, all PASS / zero ignored.
The final full pipeline and foundation builds completed in 219 and 67 steps,
exit 0. Fresh foundation/provider/typed-call/runner CTest passed 33/33, exit 0,
55.36 seconds. The unchanged aggregate still fails the same catalogue count
assertions, exit 8, 31.68 seconds. The remaining original 121 programs naturally
complete 108 PASS / 13 FAIL. Each program result and the case/failure/ignored
counts of every failing program match the preceding lookup-only inventory.
This does not certify those failures as an acceptable baseline. Annotation
canonical conversion and
prototype/construction identity remain open. No new OOM rollback guarantee is
claimed for the existing semantic/array APIs.

A further 26-step consumer build completed, exit 0. Its first direct driver
used the repository root instead of the writable fixture directory: five
binary-import type-inference cases failed at fixture writer prerequisite
`test_type_inference.c:644`. That run is preserved and receives no functional
RED credit. The full direct set was replayed at `E:/cargo-targets/zr_vm/tests`:
canonical graph 19/19, type inference 125/125, project import 9/9, comptime
budget/runtime 2/2 and 14/14, declaration transform 6/6, decorator 3/3, metadata
TypeRef 9/9, display 29/29, semantic symbols 42/42 and nominal hygiene 14/14
pass with zero ignored cases. These correct-directory logs use
`m1-msvc-nominal-consumer-direct-valid-workdir/` under the report root.

Canonical consumers still report 21 cases / one failure / zero ignored in both
direct directories. `test_callable_value_call_publishes_canonical_contract` at
`test_canonical_consumers.c:990` expects `fn(int, int, int) -> int` but queries
`fn(int, int, int) -> object` for an alias of a function with an inferred return.
This is a real observed semantic failure, not a fixture or directory failure.
Its baseline provenance is not certified. The parameter-environment/alias
return-refinement path is the next support audit; its test assertion is unchanged
and this ledger grants no acceptance waiver.

The subsequent callable-return audit recorded six lower-layer REDs in nine
cases: typed parameters, same-spelling shadowing, definition-site parameters,
untyped-parameter capture, lambda parameters and identifier aliases. Two
literal/registered-parameter prerequisites and structured-error restoration
passed. The shared return query now establishes an owned parameter environment
with syntax contexts and always restores it; aliases reuse that query. Fourteen
focused cases pass with zero ignored cases, including generic names, methods,
meta-functions, repeated queries and existing compiler errors. Existing alias
cases pass 39/39 and the unchanged canonical consumer passes 21/21, including
`fn(int, int, int) -> int` at the original failing assertion. The companion
CTest replay is 2/2 PASS, exit 0, 2.63 seconds. Native reports use
`m1-msvc-callable-return-*`. Initial fixture compile errors and a temporary
foreign missing-SSA-source generation failure are preserved without functional
RED credit. The existing foundation-only option enabled the actual lower RED;
full configuration was restored once the foreign fixture appeared. This slice
does not complete nominal annotation/prototype identity or the full pipeline.

The initial raw logs are `m1-msvc-nominal-hygiene-red-build.log` and
`m1-msvc-nominal-hygiene-red-ctest.log` under
`E:/cargo-targets/zr_vm/reports/`. Lookup replay uses the
`m1-msvc-nominal-lookup-*` prefix; registration repair and its latest full replay
use `m1-msvc-nominal-registration-*`. A fresh Ubuntu-22.04 startup probe again
returns `Wsl/Service/E_ACCESSDENIED`; neither GCC nor Clang executes. M1 remains
unaccepted and uncommitted.

### Callable Return Full Replay

The post-return repair build completed all 126 original language-pipeline
outputs in 488 incremental steps, exit 0. A corrected foundation build
completed in 127 steps, exit 0, and its 34 original/current CTest entries all
passed in 66.93 seconds. The first driver attempted `bin/cmake.exe` while
extracting a wrapped test's probe; it failed before tests. An obsolete aggregate
regex subsequently found no tests. Neither result is credited as functional
RED or aggregate GREEN. The corrected run uses the actual `language_pipeline`
name and `--no-tests=error`.

That aggregate naturally returned CTest exit 8 in 68.78 seconds at the same
two diagnostic-catalogue assertions: expected 71/actual 94 and expected
188/actual 142. The preceding parser/instruction/type-inference/semantic-facts
programs passed 74/31/125/17 cases. The separate remaining-program inventory
naturally completed every one of the 121 original manifest entries, with
108 PASS and 13 FAIL executables and no per-program timeout. Comparing each
result and every Tests/Failures/Ignored footer against the pre-return replay
found no differences. This is diagnostic evidence, not a failure waiver or
full-pipeline acceptance.

The corrected raw logs are `m1-msvc-callable-return-final-r2-foundation-*`,
`m1-msvc-callable-return-final-r3-aggregate-ctest.log`, and
`m1-msvc-callable-return-final-r2-pipeline-inventory/`, under
`E:/cargo-targets/zr_vm/reports`. The failed wrapper and no-tests logs are
retained separately. Current GCC/Clang validation is still unexecuted.

### Nominal Annotation Visibility Follow-Up

The nominal fixture then expanded from 14 to 20 cases against the actual AST
annotation converter. Its initial run was 17 PASS / three FAIL / zero ignored:
root annotations saw definition-only declarations, definition annotations saw
root-only declarations, and definition annotations could not use a matching
environment binding without `scriptAst`. Root ordinary conversion, marked
forward declarations and call-site compatibility passed as prerequisites.

The new private `type_inference_type_visibility.c` extracts the shared
explicit-binding predicate and reuses the contextual declaration helper.
Identifier and generic-base conversion now pass their actual syntax contexts;
the duplicate context-blind declaration scanner is removed. Existing qualified
prototype, `zr` bootstrap and generic-parameter behavior is preserved, not
accepted as completely hygienic. Canonical inferred-type and prototype identity
are still separate M1 work.

The focused build and two CTest entries naturally completed with exit 0,
CTest 2/2 PASS in 1.07 seconds. Unchanged type inference passed 125/125 and
canonical consumers passed 21/21, both zero ignored. Adding mixed-site,
invalid-context and five generic-base companions produced 27/27 nominal
cases, zero failures and zero ignored, on the actual rebuilt binary.
These passing companions require no additional production change and are not
recorded as new functional REDs. The earlier driver named `generic-red`
unexpectedly passed; its name does not turn that result into RED evidence.

Raw logs use `m1-msvc-nominal-annotation-red-*`,
`m1-msvc-nominal-visibility-green-*`,
`m1-msvc-nominal-visibility-type-inference.log`,
`m1-msvc-nominal-visibility-canonical-consumers.log`, and
`m1-msvc-nominal-visibility-companions-direct.log` under the approved report
root. The full replay under `m1-msvc-nominal-visibility-final-*` naturally
completed: pipeline/foundation builds exit 0 in 108/32 incremental steps;
foundation has 32 PASS and two TIMEOUTs in 291.54 seconds. The timed-out entries
are `macro_context_budget` and `macro_syntax_hygiene_import_aliases` with their
unchanged 30-second limits. Direct context-budget replay passes 9/9. A separate
unchanged full CTest replay naturally passes 34/34, exit 0, in 20.84 seconds,
including both timed-out entries. Both runs remain recorded; no limit was
raised and the original timeouts are not erased.

The actual aggregate still fails the two catalogue assertions, CTest exit 8
in 37.34 seconds. All 121 remaining programs naturally complete with timeout
disabled: 108 PASS / 13 FAIL. Comparing every result and Tests/Failures/Ignored
footer against the post-return inventory finds zero changes. This remains
diagnostic evidence rather than aggregate acceptance. No M1 acceptance or
commit is implied.

### Inferred Nominal Identity Follow-Up

Eleven additional cases extend the same fixture to 38. Native RED naturally
completes with 28 PASS / ten FAIL / zero ignored, exit 10, after a successful
three-step build. The root canonical identity prerequisite passes. The actual
failures expose definition identity lost by annotation conversion, canonical
semantic records, equality/compatibility and nullable widening, generic base,
ref pointee, array element, forward binding publication and deep-copy
preservation. Raw evidence is `m1-msvc-nominal-identity-red-*`.

The repair carries the existing nominal definition token through inferred
types, semantic nominal records, canonical adaptation and context-qualified
annotation binding. The first build driver used a nonexistent semantic-symbol
target and stopped before
compilation; this setup error is retained, not counted as functional evidence.
The corrected six-target shared-ABI build uses
`m1-msvc-nominal-identity-green-r2-*`. It naturally completes, exit 0, after
374 incremental build steps. Nominal 38/38, canonical graph 19/19, type
inference 125/125, canonical consumers 21/21, semantic symbols 42/42 and
display 29/29 pass with zero ignored cases.

Nine boundary companions expand the nominal target to 47/47 PASS, zero ignored,
without additional production edits. They cover Init/Free, malformed token
placement, generic-binding capture and legacy compatibility, nullable and
dynamic-object compatibility, mixed-site identity, repeated publication,
nullable projection and nested generic arguments. The first companion build
fails at a mistaken nonexistent AST field before tests; its log is preserved
and receives no functional RED credit. The corrected companion build completes
in six incremental steps, exit 0, including two concurrently changed foreign
finalizer sources. One foreign source emits C4013; existing D9025 warnings also
remain. No warning-free or immutable whole-worktree claim is made. Corrected
raw logs are `m1-msvc-nominal-identity-boundary-r2-*`.

The fresh complete replay uses `m1-msvc-nominal-identity-final-*`. Its native
controller naturally exits 1 during the original 126-output build, before
foundation CTest, aggregate or the 121-program inventory can run. The shared
foreign `exec_ir_source_finalized_module_validation.c` then in active
implementation emits C2059 at its POOL invocations (lines 59 onward); the macro
passes an expression to `_Alignof`, which this MSVC C11 mode rejects. This
cross-session build blocker is not repaired by changing, reverting or excluding
the foreign source. No current full-suite result is inferred from older logs
or the six-target/47-case passes. Prototype/construction, serialization,
cross-snapshot migration and general OOM rollback remain unaccepted.

The foreign owner subsequently changed POOL to use an explicit type. A fresh
full replay, `m1-msvc-nominal-identity-final-r2-*`, naturally completes with
both original 126-output pipeline and foundation builds exiting 0. Its original
34-entry CTest run is 33 PASS / one TIMEOUT, exit 8, 206.73 seconds; the timeout
is `macro_token_hygiene_lifetime` (reported 43.17 seconds). The raw failed run is
retained. A separate, unchanged 34-entry replay at the original limits passes
34/34, exit 0, 47.42 seconds, under
`m1-msvc-nominal-identity-final-r2-foundation-rerun-ctest.log`. No timeout limit
or test assertion was changed.

The actual `language_pipeline` aggregate still exits 8 at the same two
diagnostic-catalogue assertions (expected 71 / actual 94; expected 188 / actual
142), after 37.83 seconds. All remaining 121 original manifest executables run
without a temporary per-program timeout: 108 PASS / thirteen FAIL. Structured
comparison against the post-visibility inventory finds zero exit-result
changes, zero test/failure/ignored-footer changes and zero missing footers.
Comparison evidence is
`m1-msvc-nominal-identity-final-r2-inventory-comparison.log`. This is evidence
of unchanged observed failures, not aggregate acceptance or a waiver.

### Nominal Prototype Support Follow-Up

Read-only core review identifies context-blind top-level duplicate rejection,
Root prototype lookup and protocol lookup that drops nominal identity. Import
ownership review forbids attaching temporary imported AST pointers to ordinary
prototypes. Generic definition/cache lookup and source union/extern prototype
publication remain separate open consumers.

The new independent prototype fixture and normal foundation registration add
one CTest entry. Its initial native run is 17 cases / thirteen failures, but
the Root prerequisite incorrectly expects a nonzero token, so that attempt is
not functional RED. After correcting only that fixture expectation, the native
build succeeds and the unchanged production runs 17 cases / five PASS / twelve
FAIL / zero ignored, exit 12. The Root prerequisite, same-context duplicate
rejection, same-node idempotence, mixed-site normalization and imported-stub
compatibility pass. Functional failures cover both declaration orders, Root
visibility, protocol isolation, the current prototype, imported-stub capture
and unresolved-token fallback. Evidence is
`m1-msvc-nominal-prototypes-red-r2-*`.

The repair extracts a private nominal-prototype module and reuses the existing
declaration-context support. The first GREEN build driver requests a nonexistent
canonical-consumer target and ends before compilation/tests; this setup failure
earns no test credit. The corrected four-program native driver uses the actual
`zr_vm_canonical_consumers_test` target and
`m1-msvc-nominal-prototypes-green-r2-*`; all four programs naturally exit 0:
17/17 prototype, 47/47 nominal, 21/21 canonical-consumer and 125/125
type-inference cases, zero ignored. No new public ABI field, imported AST
ownership or generic cache repair is claimed.

Independent core review then identifies a normalization-budget failure that
would be treated as differing contexts and allow publication. Three real
exhaustion cases extend the fixture to 20: all original 17 pass and all three
new assertions fail after a successful build (exit 3). Raw evidence is
`m1-msvc-nominal-prototypes-budget-red-*`. The extractor now validates the
normalized output; either incoming or previous-declaration failure emits a
compiler diagnostic and blocks publication.

Six distinct-Def, MixedSite, active-snapshot and recovery companions expand the
fixture to 26. `m1-msvc-nominal-prototypes-budget-green-*` naturally exits 0;
26 prototype, 47 nominal, 21 canonical-consumer and 125 type-inference cases
all pass, zero ignored. The recovery test is subsequently strengthened to
prove normalization succeeds after restoring the budget, not merely that
another error is reported. Its separate final build/direct replay,
`m1-msvc-nominal-prototypes-recovery-r2-*`, passes 26/26, exit 0. Independent
read-only core review finds no remaining issue in this scoped production
repair or the strengthened assertion delta. Cross-expansion Equal/StableHash
allocation remains a pending M1 budget audit, not a fixed behavior.

The complete post-prototype driver naturally exits 1 under
`m1-msvc-nominal-prototypes-final-*`. The structured current CTest graph has
35 entries and includes `macro_syntax_hygiene_nominal_prototypes`. Original
126-output and foundation builds both exit 0. Foundation CTest passes 35/35
at original limits (32.90s). The actual aggregate exits 8 (58.85s): its first
74/31/125/17-case programs pass, then the unchanged catalogue assertions report
expected 71 / actual 94 and expected 188 / actual 142 (30 cases / two failures).
All 121 remaining original programs run: 108 PASS / 13 FAIL. Structured
comparison against the identity-r2 inventory finds zero result changes, zero
test/failure/ignored-footer changes and zero missing footers; evidence is
`m1-msvc-nominal-prototypes-final-inventory-comparison.log`. These scoped
results do not accept M1 or waive the failed aggregate.
Fresh `m1-nominal-prototypes-wsl-probe.log` again records
`Wsl/Service/E_ACCESSDENIED` (native exit -1); GCC and Clang remain unexecuted.

### Exact Context Query Follow-Up

Static core review identifies unbudgeted StableHash scratch and hash-as-identity
comparisons in equality and import reuse. The separate fixture uses real context
registration and wraps the existing global allocator only after setup, restoring
it before assertions. Actual native 95439 builds successfully, then exits 9:
11 cases / two PASS / nine FAIL / zero ignored. Evidence is
`m1-msvc-context-identity-red-build.log` and
`m1-msvc-context-identity-red-test.log`.

Failures reproduce nested call/definition-site hash collisions, unequal sites
becoming equal under complete query allocation failure, equivalent sites becoming
unequal under a single failure, a known hash becoming zero, three uncharged query
ARRAY allocations at an exhausted heap budget, incorrect import origin reuse,
and duplicate publication when only the source allocator rejects scratch.
The two prerequisite cases pass. No fixture compiler failure is counted as RED.

Six companions bring the fixture to 17 cases: legacy numeric hash vectors,
multi-pending normalized marks with an equivalent earlier representative, 1024
nested call/definition contexts, distinct raw parent expansion IDs, batch growth
abort/retry, and import growth failure/retry. The new normal CTest entry is
`macro_context_identity_queries` with the existing 30-second limit; the new
structured graph is verified as 36 entries and includes that executable.

Core implementation adds only private mark representatives and rolling hash
prefixes, in four syntax-context files. Root independently reads the complete
identity module and publication/import changes. Actual native 41363 naturally
exits 0: six-target build 0, 17/17 direct cases / zero ignored, six related CTest
entries PASS (4.21s, original limits). Evidence is
`m1-msvc-context-identity-green-*`. Existing MSVC C4701 warnings in context/token
budget and builder paths, and D9025 flag warnings, remain recorded; this is not
a warning-free build claim.

Review identifies weak import retry assertions: identical wrapper hashes could
hide a shallower returned context. Record-count and 20-site origin tracing
assertions are added; actual native 59428 naturally exits 0, 17/17 PASS. Further
review adds destination validity/nonroot/owner guards before origin indexing
and makes the batch retry repeat the growth sequence and verify capacities.
Actual native 39410 builds 0 and passes 17/17 / zero ignored under the separate
`m1-msvc-context-identity-recovery-r3-*` evidence. Production is unchanged.

The complete post-context driver 18199 naturally exits 1 under the immutable
`m1-msvc-context-identity-final-*` stem. Original 126-output and foundation
builds return 0; all 36 foundation entries pass in 37.24s at the original
limits. The aggregate returns 8 in 27.36s: the first four programs pass
74/31/125/17 cases, followed by the same two diagnostic catalogue assertions
(expected 71 / actual 94 and expected 188 / actual 142). All 121 remaining
programs finish with 108 PASS / 13 FAIL. Structured comparison against the
post-prototype inventory finds zero result changes, zero test/failure/ignored
footer changes and zero missing footers; its evidence is
`m1-msvc-context-identity-final-inventory-comparison.log`.

Only after this full run ends, a defensive per-iteration validity guard is
added to the retry origin trace. Final native 90925 naturally exits 0: build
0, direct 17/17 PASS / zero ignored, and all 36 foundation entries PASS in
49.32s at unchanged limits. Evidence is
`m1-msvc-context-identity-recovery-r4-{build,test,foundation-ctest}.log`.
Production and registration are unchanged after the complete replay.
The separate `m1-context-identity-wsl-probe.log` records native exit -1 and
`Wsl/Service/E_ACCESSDENIED`; no GCC or Clang result is credited. These runs
are actual 2026-10-10 evidence, reconciled on 2026-10-11 against unchanged
production hashes. M1 remains unaccepted and uncommitted.

### Source Nominal Publication Follow-Up

The next independent fixture is `test_nominal_declaration_publication.c`,
registered as `macro_nominal_declaration_publication` at the original 30-second
limit. It first tests the shared canonical publisher and declaration-binding
adoption, then calls the real union and extern struct producers. Native 57560
builds successfully, but 11 publisher cases stop at an invalid empty-variant
union prerequisite. Those cases are fixture setup failures, not publisher RED;
six producer assertion failures and two passing producer prerequisites remain
separately identified. The attempt is retained in
`m1-msvc-nominal-publication-red-{configure,build,test}.log`.

The corrected constructor supplies a real primitive variant. Native 69408
naturally exits 15 after build 0: 19 cases / four PASS / 15 actual FAIL / zero
ignored. Root publisher, same ITEM duplicate refusal and both root producers
pass. Failures establish an uninitialized context owner, missing plain/generic
nominal tokens, spelling-only rejection, invalid/budget-failed contexts being
published, duplicate SymbolIds during binding adoption, missing context-table
retention, and collapsed/missing source producer bindings. Evidence is
`m1-msvc-nominal-publication-red-r2-{build,test}.log`. Production changes only
after this behavioral RED is verified.

Boundary companions bring the fixture to 38 cases, including raw mixed-site
context, budget restoration, repeated source declaration, same ITEM duplicate,
invalid and exhausted-context failure cleanup, source/extern enum parity,
actual marked annotation conversion, forward provisional union adoption and 20
distinct-site declaration growth. Native MSVC build/test replay
`m1-msvc-nominal-publication-r4-{build,test}.log` passes 38/38 with zero
ignored cases. The direct canonical graph replay passes 19/19 in
`m1-msvc-canonical-r1-test.log`, and the related
`macro_nominal_declaration_publication` CTest entry passes in
`m1-msvc-canonical-r2-related-ctest.log`.

The complete foundation driver then builds its current graph successfully and
passes all 37 focused M1 entries in
`m1-msvc-nominal-publication-r5-foundation-ctest.log`. Its separate aggregate
still exits 8 at the established semantic catalogue assertions (94 descriptors
versus the old 71 assertion, and 142 messages versus the old 188 assertion).
The remaining-program inventory is unchanged at 108 PASS / 13 FAIL. These are
reported as separate open gates, not folded into the focused publication GREEN.
Fresh 2026-10-11 WSL naturally exits -1 with `Wsl/Service/E_ACCESSDENIED`;
decoded output is in `m1-nominal-publication-wsl-probe.log`. No Linux or M1
acceptance is inferred.

## Validation Rules

Every implementation slice starts with a behavioral test that fails against the
pre-feature implementation (RED). The owner records the failing assertion and
exit status, then reruns the same test after the implementation. A green CTest
aggregate without the focused assertion is not acceptance. Tests that exercise
token ownership, rollback, or source lifetime must run as standalone CTest
entries; they must not be hidden only inside `language_pipeline`.

Build and test one toolchain at a time in a unique build directory. Root owns
CMake registration and build orchestration. Do not run two builds or CTest
suites against one binary/working directory because fixture projects and
incremental manifests intentionally write under the build tree.

The following are RED-first command examples only; they were not executed for
the current checkpoint. Replace `<registered-config>` with the task's registered
toolchain/configuration and reuse that physical E-root build tree:

```powershell
wsl.exe -- bash -lc "cmake --build /mnt/e/cargo-targets/zr_vm/build/<registered-config> --target zr_vm_token_source_test -j2"
wsl.exe -- bash -lc "ctest --test-dir /mnt/e/cargo-targets/zr_vm/build/<registered-config> -R '^macro_token_source$' --output-on-failure"
wsl.exe -- bash -lc "cmake --build /mnt/e/cargo-targets/zr_vm/build/<registered-config> --target zr_vm_token_source_edges_test -j2"
wsl.exe -- bash -lc "ctest --test-dir /mnt/e/cargo-targets/zr_vm/build/<registered-config> -R '^macro_token_source_edges$' --output-on-failure"
wsl.exe -- bash -lc "cmake --build /mnt/e/cargo-targets/zr_vm/build/<registered-config> --target zr_vm_token_stream_limits_test -j2"
wsl.exe -- bash -lc "ctest --test-dir /mnt/e/cargo-targets/zr_vm/build/<registered-config> -R '^macro_token_stream_limits$' --output-on-failure"
wsl.exe -- bash -lc "cmake --build /mnt/e/cargo-targets/zr_vm/build/<registered-config> --target zr_vm_token_stream_test -j2"
wsl.exe -- bash -lc "ctest --test-dir /mnt/e/cargo-targets/zr_vm/build/<registered-config> -R '^macro_token_stream$' --output-on-failure"
wsl.exe -- bash -lc "cmake --build /mnt/e/cargo-targets/zr_vm/build/<registered-config> --target zr_vm_comptime_control_flow_test -j2"
wsl.exe -- bash -lc "ctest --test-dir /mnt/e/cargo-targets/zr_vm/build/<registered-config> -R '^comptime_control_flow$' --output-on-failure"
wsl.exe -- bash -lc "cmake --build /mnt/e/cargo-targets/zr_vm/build/<registered-config> --target zr_vm_syntax_expansion_contract_test -j2"
wsl.exe -- bash -lc "ctest --test-dir /mnt/e/cargo-targets/zr_vm/build/<registered-config> -R '^syntax_expansion_contract$' --output-on-failure"
```

The first run is expected to be nonzero while the API is absent. It is not a
failure of the final milestone; the recorded RED output is part of the test
evidence. After implementation, both focused entries must be zero before the
aggregate replay. Use separate directories for WSL GCC, WSL Clang, and Windows
MSVC. The final order is GCC, Clang, then MSVC, with the exact build directory,
compiler versions, CTest count, failures, and baseline differences recorded.

## Toolchain And Existing Targets

The original read-only inventory found the following WSL tools and historical
caches. It is not a current toolchain probe or execution result:

- `/usr/bin/gcc` 11.4.0 (Ubuntu 22.04), `/usr/bin/clang` 14.0.0,
  `/usr/bin/cmake` 3.22.1, `/usr/bin/ninja` 1.10.1, and `/usr/bin/ctest`
  3.22.1.
- `build/codex-macro-wsl-gcc-debug` is a Ninja Debug cache with GCC, tests on,
  language-server extension off, and performance CTest registration off. Its
  generated CTest manifests are present, but no test binary had been built at
  audit time. Do not treat this cache as a completed run.
- Existing GCC and Clang Debug caches are available under other
  `build/codex-*` names. They are historical caches, not a shared build slot;
  current builds use the registered configuration under physical
  `E:/cargo-targets/zr_vm/build/`.
- Existing MSVC caches include a Visual Studio 17 2022 cache with tests off and
  Ninja caches using `cl.exe` 14.44.35207. A Windows CLI smoke is useful after
  Linux validation; the full MSVC test graph has a known baseline build block.

The top-level test registration in `tests/CMakeLists.txt` currently creates
these relevant executables:

| Existing target | Source | Current suite/CTest surface |
| --- | --- | --- |
| `zr_vm_compile_time_test` | `tests/compileTime/test_compile_time_execution.c` | `language_pipeline` aggregate |
| `zr_vm_comptime_contract_test` | `tests/compileTime/test_comptime_contract.c` | target exists; no focused direct CTest entry |
| `zr_vm_declaration_transform_contract_test` | `tests/compileTime/test_declaration_transform_contract.c` | target exists; no focused direct CTest entry |
| `zr_vm_comptime_runtime_contract_test` | `tests/compileTime/test_comptime_runtime_contract.c` | target exists; no focused direct CTest entry |
| `zr_vm_decorator_pipeline_test` | `tests/decorators/test_compile_time_member_decorators.c` | `language_pipeline` aggregate |
| `zr_vm_lexer_parser_compiler_execution_test` | `tests/parser/test_lexer_parser_compiler_execution.c` | `language_pipeline` aggregate |
| `zr_vm_compile_time_import_ownership_test` | `tests/parser/test_compile_time_import_ownership.c` | `compile_time_import_ownership` |
| `zr_vm_binary_metadata_source_test` | `tests/parser/test_binary_metadata_source.c` | `binary_metadata_source` |
| `zr_vm_artifact_schema_test` | parser artifact schema sources | `artifact_schema` |
| `zr_vm_cli_project_incremental_test` | `tests/cli/test_cli_project_incremental.c` plus CLI compiler sources | `cli_project_incremental` |
| `zr_vm_token_source_test` | `tests/parser/test_token_source.c` | `macro_token_source` |
| `zr_vm_token_source_edges_test` | `tests/parser/test_token_source_edges.c` | `macro_token_source_edges` |
| `zr_vm_token_stream_limits_test` | `tests/parser/test_token_stream_limits.c` | `macro_token_stream_limits` |
| `zr_vm_token_stream_test` | `tests/parser/test_token_stream.c` | `macro_token_stream` |
| `zr_vm_syntax_hygiene_test` | `tests/parser/test_syntax_hygiene.c` | `macro_syntax_hygiene` |
| `zr_vm_syntax_expansion_contract_test` | `tests/compileTime/test_syntax_expansion_contract.c` | `syntax_expansion_contract` |
| `zr_vm_comptime_control_flow_test` | `tests/compileTime/test_comptime_control_flow.c` | `comptime_control_flow` |
| `zr_vm_comptime_control_flow_edges_test` | `tests/compileTime/test_comptime_control_flow_edges.c` | `comptime_control_flow_edges` |
| `zr_vm_comptime_budget_interleaving_test` | `tests/compileTime/test_comptime_budget_interleaving.c` | `comptime_budget_interleaving` |

`zr_vm_add_unity_test_target` creates executables but does not itself register
CTest. New M1 contracts should therefore have explicit `add_test` entries with
stable names, then be added to the appropriate aggregate only after the
focused RED/GREEN cycle is understood. The existing aggregate commands are:

```bash
ctest --test-dir <build> -R '^language_pipeline$' --output-on-failure
ctest --test-dir <build> -R '^(binary_metadata_source|compile_time_import_ownership|artifact_schema)$' --output-on-failure
ctest --test-dir <build> -R '^cli_project_incremental$' --output-on-failure
ctest --test-dir <build> -R '^language_server$' --output-on-failure
```

Run `ctest --test-dir <build> -N -V` first to verify that the generated
manifest points at the intended toolchain. On the audited GCC cache the
selected names were `binary_metadata_source`,
`compile_time_import_ownership`, `artifact_schema`, `language_pipeline`,
`language_server`, and `cli_project_incremental`; no selected executable was
present yet.

## M1 Focused Matrix

The direct source boundary target is the registered
`zr_vm_token_source_test` from `tests/parser/test_token_source.c`, with CTest
name `macro_token_source`. The independent edge target from
`tests/parser/test_token_source_edges.c` is registered as
`zr_vm_token_source_edges_test` / `macro_token_source_edges`; this row is a
later RED/GREEN evidence hook and is not yet a passing result. It covers
cross-source joint punctuation and split provenance, checkpoint replay around
lookahead, transparent `None` groups, poisoned lexer initialization, empty
string semantic materialization, declaration-pattern hygiene, and AST
stream-owner retention through lexer free and full/moving GC. The owned-stream
limits target is
`zr_vm_token_stream_limits_test` from
`tests/parser/test_token_stream_limits.c`, with CTest name
`macro_token_stream_limits`; the adjacent immutable-tree target is
`zr_vm_token_stream_test` / `macro_token_stream`. The phase/control source is
`tests/compileTime/test_comptime_control_flow.c`, registered as
`zr_vm_comptime_control_flow_test` / `comptime_control_flow`. The
phase/resource target is `zr_vm_syntax_expansion_contract_test` /
`syntax_expansion_contract`. Keep these focused registrations rather than
hiding failures in the large compile-time executable.

| Contract | Focused case and required assertion | Negative/boundary companion | Proposed test label |
| --- | --- | --- | --- |
| Direct and owned input | Lexing the same source through `TokenSource` and an owned `TokenStream` yields equal tree shape, token kind, raw text, spacing, and source identity; compilation consumes the tree directly and does not display/relex. | Empty input, comments-only input, invalid character, and unbalanced delimiter return located errors. | `macro_token_source`, `macro_token_stream_limits` |
| Raw spelling and spans | Identifier escapes, numeric suffixes, string escapes, comments, and delimiter open/close spans preserve original byte ranges and source id. | Spans remain queryable after the caller releases its source buffer or AST. | `macro_token_stream`, `macro_token_stream_limits` |
| Punctuation | Joint punctuation stays joint; a compound operator such as `>>` can be split into two `>` tokens without losing each span or context. | `+ =`, `+ident`, and punctuation followed by a delimiter remain `Alone` where required. | `macro_token_source` |
| Groups and opaque fragments | Parenthesis, bracket, and brace groups retain nesting and open/close spans; opaque parsed fragments remain atomic while traversal can enter explicit groups. | Mismatched close, missing close, and nested depth at the configured limit fail with a located diagnostic. | `macro_token_stream`, `syntax_expansion_contract` |
| Lookahead and restoration | Mark/restore or cursor snapshots replay the same token sequence, including nested groups and end-of-stream. | Restore after EOS, repeated restore, and abandoned checkpoints leave no stale borrow or counter. | `macro_token_source` |
| Syntax context | Cloning, interpolation, and span replacement preserve `SyntaxContext`; call-site, definition-site, and mixed-site contexts resolve according to the locked contract. | Context mismatch does not silently resolve a generated identifier to an unrelated binding. | `macro_syntax_hygiene`, `macro_token_stream_limits` |
| Tree traversal and cleanup | Deep, wide, and repeated traversal accounts nodes and releases temporary trees on both success and error. | Deep-tree limit, allocation failure, and malformed opaque fragment leave no leaked detached tree and preserve the previous published tree. | `syntax_expansion_contract` |
| Expansion transaction | Expansion validates a detached tree before publication; a rejected replacement leaves the prior AST/artifact byte-identical and a retry deterministic. | Type/signature/category error after partial generation must roll back all generated nodes and source-map entries. | `syntax_expansion_contract` |
| Comptime loops | `while`, `for` init/condition/step, `foreach`, statement `switch` case/default, nested `break`/`continue`, function return through loops, and block shadowing execute in the comptime frame. A statement switch with no matching case and no default continues normally. | Invalid break/continue scope, non-iterable foreach, and loop variable scope leak are rejected. | `comptime_control_flow`, `comptime_control_flow_edges` |
| Resource accounting | Fuel, call depth, heap, aggregate values, generated declarations, diagnostics, token allocation, matching, and expansion budgets stop exactly at their limits. | Infinite loop, recursive expansion, 1025th diagnostic, and one-over-limit tree/match allocation fail atomically with the correct resource id. | `syntax_expansion_contract`, `comptime_budget_interleaving`, `macro_token_stream_limits` |
| Frame and cache determinism | Function/loop frames clean up on return, break, continue, and failure; repeated equivalent input produces stable output and cache identity. | Failure must not consume a later run's budget or retain stale locals, source pointers, or cache entries. | `syntax_expansion_contract`, `comptime_control_flow` |

For compile-time statement switches, when no case matches and there is no
default, `compile_time_control_flow.c:568-570` releases the selector root and
returns normal flow; the runtime statement compiler in
`compile_statement_flow.c:352-539` has the corresponding no-match behavior.
Compile-time switch expressions remain unsupported. The statement rule does
not define an unmatched-expression rejection contract.

The existing `test_comptime_contract.c` and
`test_comptime_runtime_contract.c` already cover typed effect policy, fuel and
call-depth limits, deterministic cache keys, provider contract/content hashes,
diagnostic limits, cache snapshot atomicity, and owned artifact identity. M1
should extend those assertions only where the new token/tree resources or
control-flow frames cross the existing contract; duplicate tests should remain
in the focused target.

## Rust Reference Ledger

The reference checkout is `lua/rust`. These files provide behavior evidence,
not an instruction to copy Rust's semantics where the ZR design explicitly
differs:

- `library/proc_macro/src/lib.rs:91-217` defines `TokenStream` as the input and
  output boundary, separates token-tree iteration from display text, and
  documents that string parsing assigns call-site spans.
- `library/proc_macro/src/lib.rs:644-757` defines the four `TokenTree` forms
  and span delegation. `:762-885` defines groups and delimiter spans.
- `library/proc_macro/src/lib.rs:908-991` defines `Punct` spacing (`Joint` /
  `Alone`), including why joint punctuation must be preserved during assembly.
- `library/proc_macro/src/lib.rs:483-626` defines call-site, definition-site,
  mixed-site, parent/source, byte-range, join, and source-text span queries.
- `compiler/rustc_ast/src/tokenstream.rs:26-80` stores a token or delimited
  subtree; `:542-617` stores spacing and an owned, cloneable stream; `:870-979`
  provides shallow lookahead and a nested `TokenCursor` that enters/exits
  delimited streams and returns EOF deterministically.
- `compiler/rustc_parse/src/parser/tokenstream/tests.rs:10-115` tests stream
  concatenation, empty streams, unspanned equality, and joint `...` assembly.
- `compiler/rustc_expand/src/proc_macro_server.rs:162-201` preserves jointness
  and splits multi-character operators into punctuation tokens with per-byte
  spans; `:680-801` maps source locations, parent/source spans, joins, and
  source snippets; `:803-838` documents saving/recovering spans through binary
  metadata.
- `compiler/rustc_expand/src/mbe/macro_parser.rs:614-701` shows token-by-token
  matcher traversal, nonterminal parsing, and ambiguity/error boundaries;
  `mbe/transcribe.rs:35-99` and `:163-289` show expansion marking and nested
  transcription.
- `compiler/rustc_span/src/hygiene.rs:48-80`, `:710-906`, and `:1298-1469`
  model syntax-context marks/transparency and recursive serialization/decoding.

The M1 acceptance ledger must cite the corresponding ZR API and test location
once each API is locked; Rust references alone do not establish acceptance.

## Existing Artifact, Incremental, and LSP Hooks (M6 Map)

M6 depends on existing artifact and editor seams. The following are verified
inspection points and proposed future focused labels; they are not M1 pass
criteria:

| M6 behavior | Existing seam | Proposed focused label and acceptance |
| --- | --- | --- |
| Macro manifest and executable compile-time representation | `.zro` module payloads, `.zri` textual intermediate writer, and `.zrm` manifest/provider APIs documented in `docs/module-system/zrm-assembly-container.md`; `tests/library/test_zrm_container.c`, `tests/library/test_project_import_resolver.c`, and `tests/artifact/test_manifest_roundtrip.c` | `macro_artifact_roundtrip`: deterministic write/read bytes, schema/version rejection, manifest identity/hash, source-map and syntax-context provenance, and no native pointers in serialized data |
| Source provider import | Existing project source provider fixtures under `tests/fixtures/projects/decorator_compile_time_provider_import*`; compile-time import ownership target and CLI compiler `.zri` reconciliation | `macro_source_provider`: source macro provider and consumer compile, cache invalidates on provider/source/build-input change, and source-only and `.zri` paths agree |
| Binary provider import | Existing `.zro`/`.zri` fixture trees, `.zrm` assembly fixture `tests/cli/test_cli_zrm_fixture.c`, and compile-time provider-phase checks | `macro_binary_provider`: consumer works with provider source removed, exact content/public-contract hash is checked, wrong Runtime/Test phase is rejected, and binary/source output is equivalent |
| Expanded-source CLI | `zr_vm_cli/src/zr_vm_cli/command/command.c:241-244,784-800,957-960` currently lists and parses `--intermediate`, `--emit-zrm`, `--emit-aot-c`, and `--incremental`; no `--emit-expanded` option was present in the audit | `cli_emit_expanded`: add/help/parse/compile-only validation, deterministic expanded text, source-map sidecar, and no output on failed expansion; reject the modifier without `--compile` |
| Incremental source maps and generated symbols | `zr_vm_language_server/include/zr_vm_language_server/incremental_parser.h`; `src/.../incremental_parser.c`, `incremental_token_equivalence.c`, and `incremental_syntax_reparse.c`; current generated source-map tests in `tests/compileTime/test_compile_time_decorator_shape_retention_cases.h` | `lsp_macro_source_map`: call-site/definition provenance, diagnostics and generated ranges survive source snapshots; edits invalidate only affected expansion and do not reuse stale fallback AST |
| LSP generated-symbol navigation | Public queries in `zr_vm_language_server/include/zr_vm_language_server/lsp_interface.h:326,386,396,415,421`; existing project feature and CompileTool virtual-document tests cover provider refresh, definition, references, document/workspace symbols, and virtual docs. No generic public AST visitor was found; project import code recursively visits AST nodes. | `lsp_macro_generated_symbols`: generated declarations appear in document/workspace symbols, definition/references navigate to macro call or provider source/virtual expanded URI, diagnostics map through provenance, semantic tokens use generated ranges, and incremental provider changes refresh results |

The explicit absence of `--emit-expanded` is a known M6 feature gap, not an M1
blocker. Existing `.zri` generated-source-map tests prove deterministic
intermediate output for generated fields but do not prove macro expansion
manifest/source-map serialization.

## Current Focused Evidence (2026-10-08 09:18 UTC)

The following is the exact prior-round WSL GCC snapshot from the registered
M1 CTest subset (the integrated 08:45 UTC run). It is evidence for the
RED/GREEN ledger only; it is not an M1 acceptance claim. Eleven targets passed
and seven targets failed out of the 18-target snapshot.

| CTest label | Prior-round status | Observed result |
| --- | --- | --- |
| `comptime_control_flow` | PASS | All registered cases passed. |
| `syntax_expansion_contract` | PASS | All registered cases passed. |
| `comptime_budget_interleaving` | PASS | All registered cases passed. |
| `macro_token_stream` | PASS | All registered cases passed. |
| `macro_syntax_hygiene` | RED | Binding-key lookup expected 1 and got 2; marked type/closure capture expected 5 and got 12. |
| `macro_token_source` | PASS | All registered cases passed. |
| `macro_token_stream_limits` | PASS | All seven registered cases passed. |
| `macro_token_span` | PASS | All registered cases passed. |
| `comptime_frame_lifetime` | PASS | All registered cases passed. |
| `macro_token_hygiene_lifetime` | RED | Mixed-site normalization for local/item expected true and got false. |
| `macro_token_stream_boundaries` | PASS | All registered cases passed. |
| `comptime_control_flow_edges` | RED | The block-shadow edge fixture reported a syntax error. |
| `macro_parser_token_ownership` | RED | Shared expansion-budget usage stayed at zero; AST allocation-budget failure returned non-null. |
| `macro_context_budget` | PASS | All five registered cases passed in this integrated snapshot. |
| `compile_tool_phase_gate` | PASS | All registered cases passed. |
| `macro_token_stream_provenance` | RED | All seven provenance cases failed: mixed-owner import, empty-group trivia, punctuation spacing, invalid punctuation, decoded text semantics, string/integer text semantics, and comment/concat trivia. |
| `macro_token_source_edges` | RED | Three cases failed: cross-source joint provenance, native cached `>>` split (expected IDENT 346, got EOS 347), and the equivalent `FromText` split. |
| `macro_token_budget_peak` | RED | The peak heap-overcommit case expected a null result and received a non-null result. |

The same snapshot also identifies three AST ownership/GC cases as green inside
`macro_parser_token_ownership` (`test_script_roots_materialized_strings_after_parser_and_stream_free`,
`test_detached_statement_retains_strings_and_context_table`, and
`test_source_asts_can_be_freed_out_of_order`). The target was RED in that dated
snapshot because its two budget cases failed. These are retained observations,
not a current production-cause diagnosis; the later
[Full Windows prerequisite replay](#full-windows-ownership-fixture-checkpoint)
separates zero-FUEL Init failures from the final budget and cleanup contracts.

Additional isolated evidence is tracked separately from that 18-target
snapshot:

| Isolated surface | Evidence | Interpretation |
| --- | --- | --- |
| Native token provenance | 7 cases, 7 RED | The integrated provenance failures are genuine contract gaps, not a missing CTest registration. |
| Token budget peak | 3 newly added cases pending integration | No result is credited until the three cases have a registered target and a fresh build. |
| Syntax-context budget | 7 cases, 6 PASS; capacity case RED at `tests/parser/test_context_budget.c:278` | The six passes are isolated-library evidence only; capacity accounting remains open. |
| Comptime control-flow edges | 4 cases, 2 PASS and 2 RED | The shadow invalid-reserved-token fixture has been corrected; the cached compile-time-value GC case remains an actual RED at `tests/compileTime/test_comptime_control_flow_edges.c:276`. |

The current focused WSL GCC baseline also contains actual evidence of 69
execution tests, 14 runtime-contract tests, and 2 budget tests passing. These
counts are retained as focused baseline evidence and must not be replaced by
the historical March broad-suite numbers. They do not waive the seven RED
targets or imply cross-toolchain acceptance.

At the prior ledger timestamp, root was running the shared GCC rebuild for 20
targets (session `55649`). That WSL run was later interrupted after 98/526:
the restart hung in `VerifyGlobs` with `D+` `p9_client_rpc` for more than 12
minutes, so root terminated Ubuntu. The current configure session `5176` is
waiting with suppress-regeneration enabled. No new GCC GREEN status is claimed.

### Windows MSVC checkpoint

Root configured `build/codex-macro-msvc-debug` with MSVC 19.44, Debug Ninja,
static-only settings, and the x64 `VsDevCmd` environment. All 20 registered
targets built successfully in 1,048 steps. The resulting CTest snapshot was 15
PASS and 5 not-PASS entries:

| MSVC surface | Result | Evidence |
| --- | --- | --- |
| General M1 targets | PASS | All targets not listed below passed, including heap/node ownership, the 7-case context target, the 5-case cached-GC target, and the 3-case peak-budget target. |
| Syntax hygiene | RED | The fourth runtime case failed because `x` was not found. |
| Phase gate | TIMEOUT | `compile_tool_phase_gate` did not complete. |
| Native provenance | 6/7 PASS, 1 RED | The comment-only/concat case expected 2 and got 3 at `tests/parser/test_token_stream_provenance.c:468`. |
| Token-source edges | 7/9 PASS, 2 RED | Expression-context assertion at `tests/parser/test_token_source_edges.c:417` and variable-pattern assertion at `tests/parser/test_token_source_edges.c:485` failed with genuinely non-root contexts. |
| Parser array budget | TIMEOUT | `parser_array_budget` did not complete. |

The MSVC build and partial CTest results are toolchain evidence only. The
interrupted WSL rebuild, the outstanding MSVC RED/TIMEOUT entries, and the
unreplayed Clang/artifact-provider gates keep M1 acceptance open.

## Known Baseline And Blockers

The repository's validation skill records the last full baseline (2026-03-26)
as WSL GCC and Clang configure/build plus `hello_world` smoke success, with
15 failures out of 23 in the broad CTest run. Windows MSVC tests-off configure,
build, and CLI smoke succeed. The MSVC tests-on graph fails before CTest in the
known baseline paths, including `tests/test_runner.c` including `unistd.h` and
parser test targets missing `setUp`/`tearDown`. These failures must be reported
as baseline differences, not silently folded into M1 results.

The R28 checkpoint retains the dated 15-suite carrier result; the Full Windows
fixture checkpoint above records the later bounded follow-up. Acceptance
remains pending until the current focused contracts have executable evidence
on WSL GCC and WSL Clang, followed by the aggregate language-pipeline and
artifact/provider replays across the required matrix.

## Array Null-State Late-Pass Checkpoint (2026-10-15)

The private `compiler_quickening_null_state.c/.h` analyzer supports the
Array<int>.add late cleanup. It proves a candidate plain-NULL `GET_CONSTANT`
redundant using `UNREACHED < NULL < UNKNOWN`, initializes entry to NULL only
for a validated isolated value frame, and removes the candidate's own write
from the graph. Invalid instruction or frame geometry, unknown opcodes,
overflow, and allocation failure reject the fold. The new parent pass only
considers a clear immediately followed by an in-range backward `JUMP`; the
clear must not be a block start, must be outside active local and typed-local
lifetime, must be CFG-dead, and must pass the independent null proof. NOPs are
compacted before the unchanged strict late FILL matcher. Existing tests and
registrations were not changed.

| Evidence | Result | Receipt |
| --- | --- | --- |
| R4 source proof | Root and peer whole-RAM inverse/forward proof passes. The fresh pin set has 119 files and adds `zr_vm_core/src/zr_vm_core/object/object_call.c` (SHA-256 `902CEBF13C910EF12D9C60737ECBD97046A4E1F1BFAEEBBC0E2892B70F8D7E49`); historical R3 had 118 and did not pin it. | `E:/cargo-targets/zr_vm/reports/m1-array-null-state-source-proof-r4.json`, SHA-256 `49BAABFFD2282E96FA86149239201A17795A5CFF60611B7BD51C870285AB508C` |
| MSVC R4 quickening | `zr_vm_compiler_w2_performance_quickening_test`: 23 cases, 0 failures, 0 ignored; native exit 0. | `E:/cargo-targets/zr_vm/reports/m1-msvc-array-null-state-r4-zr_vm_compiler_w2_performance_quickening_test.log`, SHA-256 `4A2D9A7A33499B161EDA92EFF9F5C03D69127138E2A2D3276935AD774CF4A81F` |
| Lower-contract MSVC R8 | 14 targets, 323 cases, 0 failures, 0 ignored; exact ordered registrations, stable binaries, stable source pins, and unchanged HEAD `63dd36d0c72db5aaf9bee15a2d81107f823a5cc6`. | `E:/cargo-targets/zr_vm/reports/m1-msvc-m1-lower-contracts-r8-build.log`, SHA-256 `39F39AAF696E621D35CC603EB9003C80BA657ECC3C2C491CB6BB59D1D8DC9ED6` |
| Original127 MSVC R2 | 127 cases, 0 failures, 0 ignored; exit 0, ordered registrations, zero custom failure rows, 127 stable source pins, stable binary, unchanged HEAD `63dd36d0c72db5aaf9bee15a2d81107f823a5cc6`. | Build `m1-msvc-original127-after-lower-r2-build.log`, SHA-256 `F205B65E867C597BAA7F376E14EB2C268DE747130FF3EE921E5B557F6028FDE9`; native `m1-msvc-original127-after-lower-r2-native.log`, SHA-256 `28CC63BC1341A81FE8847B516F02699937143815241B41DA4757F357CD826D19` |
| Original51 MSVC R5 | 51/51 CTest cases, exit 0 in 24.79 seconds; original `-j1` invocation, no timeout override. Build, discovery, and CTest exit 0; receipt passed with no errors. All 119 source pins are consistent; HEAD before and after is `63dd36d0c72db5aaf9bee15a2d81107f823a5cc6`. | Receipt `m1-msvc-foundation-current-r5-receipt.json`, SHA-256 `E8DFAE09D85E182DC3B864C87BC55D4BAC3173C68B3EE46CE6B5ACC317409443`; CTest `m1-msvc-foundation-current-r5-ctest.log`, SHA-256 `7B1BC51F1AD22E959D0CDCA4CD74D696562C7BA83B184454C1A23D5BB5B68F7E` |
| WSL availability | `Wsl/Service/CreateInstance/E_ACCESSDENIED`; no GCC or Clang tests ran. | `E:/cargo-targets/zr_vm/reports/m1-wsl-service-recheck-20261015-r1.json`, SHA-256 `0652654541403BB7766A266134D2B962CD106E9C712FBD7F9EC2BAA2AD1DFDD1` |

R3 native 23/1 and R7 W2 22/1 remain dated historical outcomes; the later R4
23/0/0 replay is separate evidence and does not erase them. The R4 source
receipt predates the native run and reports source proof only; native credit
comes from the later R4 and R8 logs. Fresh original127 and original51 runs are
both green. Earlier original127 120/7, original51 50/1, and original51
34/17-timeout records remain historical; the R5 run did not reproduce a
timeout, and no cause is inferred. After R5, another session's documentation
commit moved the checkout from the tested HEAD
`63dd36d0c72db5aaf9bee15a2d81107f823a5cc6` to
`7a3e20119a5afe51f4625182833caa4ab8511b1b`. Root reviewed the complete commit
diff: it adds only comments and whitespace in `math/module.h` and
`math/module.c`, without declaration or code changes. R5's 119-pin result
remains specific to `63dd36d...` and does not claim validation at the later
HEAD. All listed Windows slices are green on their recorded source revisions.
The WSL availability probe reports
`Wsl/Service/CreateInstance/E_ACCESSDENIED`, and neither compiler ran. WSL
GCC/Clang is now the only remaining unaccepted validation gate. M1 remains
open and unaccepted, M2-M6 remain pending, and no milestone commit is claimed.

## Comptime Control-Flow Negative Coverage Checkpoint (2026-10-15)

Independent review found five negative comptime-control-flow cases without
focused coverage. The existing edge source now registers five rejection
fixtures: `break` and `continue` outside loops, `foreach` over a GC object
instead of a built-in array, and `for`/`foreach` bindings referenced after
their loop scope. It also adds a success fixture where a statement switch has
a GC object selector, no matching case, and no default; execution continues to
the following `return 17`.

Both `test_comptime_control_flow.c` and
`test_comptime_control_flow_edges.c` parse fixtures with
`ZrParser_ParseWithState`, prepare facts with
`ZrParser_CompileTime_PrepareBuildFactsInCompilerState`, then run
`ZrParser_CompileTime_ExecuteLateChecksInCompilerState` in the late-check
phase. The edge helpers check expected success/failure and verify that freeing
the compiler restores the GC root count to its pre-run baseline. The success
helper performs the same root-count assertion for the statement-switch case.

The bounded MSVC R1 run is green at HEAD
`7a3e20119a5afe51f4625182833caa4ab8511b1b`. The build exits 0 in 8.431 seconds;
the receipt reports 163 identical source pins, `source_changed: []`,
`active_native_tests_post: []`, and unchanged pre/post HEAD. Both targets have
matching ordered registrations and stable binaries. The edge test source is
SHA-256 `7F3A9D2A3BCEF3958CFA14D189FC7C92AE0EBA3E40F47DD9A3412D3FAD56CBE4`.

| Target | Result | Native log |
| --- | --- | --- |
| `zr_vm_comptime_control_flow_test` | 9 tests, 0 failures, 0 ignored; native exit 0 in 461 ms. | `m1-msvc-comptime-control-negative-r1-control.log`, SHA-256 `33E989B88C35718F344EAA895DCE0F8B1A0EADDE7B73CB78D66EF31229F992D5` |
| `zr_vm_comptime_control_flow_edges_test` | 11 tests, 0 failures, 0 ignored; native exit 0 in 347 ms. | `m1-msvc-comptime-control-negative-r1-edges.log`, SHA-256 `8CE3D29D34945CB8DE7ECA9E6E86AFB269B5D565303FB8F284E10C5E157F64DB` |

Build log `m1-msvc-comptime-control-negative-r1-build.log` has SHA-256
`4805098765FAABDE70D6DFCCBDB3F68878197C1138CEC4634996756D3893E706`; receipt
`m1-msvc-comptime-control-negative-r1-receipt.json` has SHA-256
`5A0640C1D9524D6AAAC129D20588695B627E493C9C92C92A3887809A27F1774E`. An
initial bootstrap-only `Get-Command cl` failure occurred before the build and
produced no build output. After one PATH row was restored, this bounded run
completed; production and CMake files were unchanged. Earlier R4/R8/original127/
original51 results remain at HEAD `63dd36d...` and are not retroactively
credited to `7a3e201...`.

The retained WSL receipt reports
`Wsl/Service/CreateInstance/E_ACCESSDENIED`; neither GCC nor Clang ran. WSL
GCC/Clang is now the only remaining unaccepted validation gate. M2-M6 remain
pending, and no milestone commit is claimed.

## Concat Initial Heap-Budget Failure Checkpoint (2026-10-15)

`ZrParser_TokenStream_Concat` previously returned immediately when its temporary
context table allocation failed. The shadow budget contained the rejection,
but the caller's failure resource, limit and requested usage were not copied.
The narrow repair routes this case through the existing common failure path.
It introduces no public API, allocation size constant or CMake change.

The new eighth limits case uses a zero heap limit and checks failure details,
NULL output, unchanged committed usage, retained sealed input trees, and a
successful retry after resetting the budget. Removing only the new test,
comment and registration in RAM restores the original test file SHA-256
`18DF83C96E3DE0A95E2DFA9A4AE5D1E2290522CC680A7BB1ABB54E801BEED46B`.
The final test SHA-256 is
`A5E6840B3A6463D7647CE9E0DA7128644A3AB88CB0AA24E0640BA2F74CCA1E08`.
Independent core and Root whole-file inverse checks restore production PRE
`55D11DEF77E068424771F8B578642E5F1FC2906ECA64F8951249A73DB2A9A083` from POST
`D0D17B2F58439F3CFC9FDA757B1DC9DE90CA49D22E62526F327D1BD8708C4F83`.

Actual prepatch MSVC RED builds successfully and completes 8 tests with one
failure: only the new case reports `Expected 3 Was 0` for the missing heap
failure resource. Native exit is 1; the original seven cases pass. All 113
source pins and HEAD `936dc4ddda52ebf7b03b98c5c1cd0f8d1b9670ad` remain stable.
Log `m1-msvc-concat-budget-red-r1-limits.log` has SHA-256
`AA0D384A87FAF8C8084F3F9F6467F2500F46A59D81536164E72D66D7C1691BAA`.
The original driver receipt remains failed: PowerShell unwrapped the singleton
target hashtable, making its field count 3 instead of target count 1. This is
a separate driver summary error; it does not invalidate the raw behavioral
RED. Receipt SHA-256 is
`820ADECC86757D398AE9E379DC8F9356CD0838419352DC99936CA18A5FE41912`.
Only the count expression was corrected before GREEN, with an in-memory inverse
restoring the RED helper. No retroactive driver pass is claimed.

The actual postpatch MSVC GREEN build exits 0 in 22.666 seconds. Seven native
targets complete 35 tests with zero failures or ignored cases. The driver exits
0 with matching ordered registrations, stable binaries, 113 unchanged source
pins and unchanged HEAD `7fda6c16da05758d01fdf2be6a1ea2426ae3da63`.

| Target | Cases | Result |
| --- | --- | --- |
| `zr_vm_token_stream_limits_test` | 8 | PASS |
| `zr_vm_token_stream_boundaries_test` | 5 | PASS |
| `zr_vm_token_stream_provenance_test` | 7 | PASS |
| `zr_vm_token_context_ledger_test` | 4 | PASS |
| `zr_vm_token_budget_peak_test` | 3 | PASS |
| `zr_vm_token_source_test` | 6 | PASS |
| `zr_vm_token_stream_test` | 2 | PASS |

GREEN receipt `m1-msvc-concat-budget-green-r1-receipt.json` has SHA-256
`38D0284A36E5DA9F0E78A8A6BF2EF2C42C175C968C6087F29E0C18C41EEF166B`;
build log SHA-256 is
`7D625FFACB31779C304CEB5FA9A5B368B28A31DAFF53889A080115864EA2F45A`.
Logs and receipts are retained under `E:/cargo-targets/zr_vm/reports/` and
registered in the existing `macro-tokenstream-20261008/artifacts.json` manifest.
All owned build/native processes exit naturally; native timeouts remain 30
seconds without override. This focused run does not replace the full M1 matrix
or relabel previous runs at older source revisions.

The resumed WSL availability probe still exits -1 with
`Wsl/Service/CreateInstance/E_ACCESSDENIED`; neither GCC nor Clang starts.
Receipt `m1-wsl-service-recheck-20261015-r3.json` has SHA-256
`14AEE8990B94275774FF2F48464BD04ECD674045BC1B5FE198CE8C48CE2479B5`.
M1 remains unaccepted, M2-M6 remain pending and no milestone commit is made.

## Current WSL Recovery and Matrix Start (2026-10-16)

The WSL startup prerequisite is resolved in the current tool environment.
The real `/usr/bin/true` probe exits 0 naturally in 54142 ms, and its owned
process is absent afterward. Receipt
`E:/cargo-targets/zr_vm/reports/m1-wsl-service-recheck-20261016-r3.json`
has SHA-256
`DE9A8E22556B67D155466D490FCCF718AC8D55620F37DC3FDA0BE36D457F76B7`.
This grants availability evidence only; the earlier denied attempts remain
unchanged. The available Linux tools include GCC 11.4.0 and Clang 14.0.0.

The actual GCC foundation configure uses the current checkout and
`E:/cargo-targets/zr_vm/build/macro-wsl-gcc-debug`, with Ninja, Debug, static
libraries, tests, CLI and language server enabled. Rust binding, editor
extension, host JIT and performance CTest registration are disabled.
The native process starts at 2026-10-15T19:07:15.0720601Z and exits 0 naturally
at 19:59:39.6031517Z after compiler identification, include probes and Ninja
generation. Bootstrap receipt
`m1-wsl-gcc-current-r3-foundation-configure-receipt.json` has SHA-256
`C4159E372BE7E924EE2519EAED60817A53A4F2C61121BFFF1E364395D13440C3`.
Root independently verifies both output-log hashes. Explicit adoption in the
matrix driver also requires this receipt/hash, exact command, cache identities
and generated files; it records the real bootstrap process separately.

The driver captures 4655 current source-input hashes with native Windows
PowerShell, exit 0 in 67052 ms, against the same physical checkout. Foundation
compilation is now running with 1271 build steps; no current CTest result is
claimed. A bounded syscall trace explains the slow directory verification:
successive first-party file queries take 0.123508-1.350967 seconds through 9P.
The trace detaches naturally, and the original build proceeds to GCC object
compilation. Earlier helper inventory attempts were stopped before compiler
execution and retain explicit failed/aborted receipts.

The two-stage selection is foundation-only52, then the same build's relevant
full graph: lower14 with 323 native cases, the native original127 executable,
original51 plus language_pipeline/CLI incremental/language_server CTest entries,
and hello_world. Source/helper/binary identities are checked before and after;
CTest remains serial with the source-defined timeouts. Clang follows GCC, then
final native MSVC. MSVC preparation reuses the physical existing build directly,
enables its previously disabled CLI/language server, and verifies generated
Ninja output aliases. It has not started a compiler or test.

All current outputs are preregistered in
`E:/cargo-targets/zr_vm/reports/macro-tokenstream-20261008/artifacts.json`.
Current reusable builds and necessary evidence are retained; this continuation
has deliberately deleted 0 bytes. M1 is still unaccepted and uncommitted,
M2-M6 remain pending, and the full artifact lifecycle is active.

## GCC R3 Repair Stop and Current R4 (2026-10-16)

GCC r3 is terminal FAIL. Root deliberately sends SIGINT through a verified
pidfd only to owned Ninja PID 545 under CMake PID 544, after identifying an unused
M1 root-context CFG wrapper. Compilation stops at 628/1271; Ninja reports
interrupted by user and CMake exits 2. Driver 84581 then completes source-post
with exit 0 in 59659 ms and exits 1. No tests run. Receipt
`m1-wsl-gcc-current-r3-receipt.json` has SHA-256
`61AD3336F4AE0C0E639DF32246C61A861356B0D2F8510EFDCDF0DB32606056F6`.
Fresh ps confirms PIDs 406/544/545 absent. This is an implementation repair
checkpoint, not a compiler failure or an observation-timeout restart.

The final 4655 source pins contain eight foreign changed paths. The earlier
comment-equivalence proof covers five only; it does not establish equivalence
for changes to test_ssa_aotir_state_map.c, test_ssa_contract_freeze.c and
test_ssa_state_maps.c. The frozen driver's raw-source failure remains intact.
All foreign changes and their evidence remain in place.

Core deletes only the unused five-line wrapper and one following blank line
from compiler/semantic/compiler_semantic_cfg_try.c. Both callers retain the
context-aware query. Root independently restores the removed bytes in RAM,
matching the actual preceding source pin and whole-file pre-hash
`D2B88F079D3DB1CF396456A0911D4DC57673B536DB91C898F04AB2EC9DF123C1`.
The post-hash is
`3387DB198137B22E927F6C0C16B5FE13A38E222DDF63F709F0CF2872E9C65735`;
the exact-word obsolete-wrapper search finds no references. This private
dead-code removal uses the existing CFG/hygiene and full acceptance gates.

R4 output paths are preregistered before creation. Root starts the unchanged
Linux helper with gcc --attempt r4, the default configure path and the same
physical GCC build. Exec 7531 completes source-pre PID 526 with 4655 pins;
the CFG pin matches the reviewed post-hash. Tool discovery exits 0 and
foundation configure PID 538 starts. Current test results
remain pending; Clang r4 and final MSVC r6 follow the GCC gate. No source
copies or new build trees are created. Lifecycle remains active; 0 bytes freed.

## GCC R4 Terminal And Focused Repairs (2026-10-16)

The immutable r4 receipt is terminal FAIL, SHA-256
`743A3F11A320EF93685B26ECE095398B77922F4647A37D80321581A5690182E3`.
Foundation configure/build both exit0, followed by52/52 serial CTest PASS in
9.59seconds with all52 binary identities unchanged. Full configure exits0,
but full build exits1 on two LSP declaration lookups missing the new fifth
syntax-context argument. Lower14/original127/full54/CLI never execute.
Final source-post contains four foreign changed paths: SSA differential test,
SSA state-map test, public state-map header and core exec_ir_materialize.c.
Earlier two-path checks cannot replace this final inventory. No complete GCC,
Clang, final MSVC or M1 acceptance is claimed.

The M1-new lambda range witness compares signed AST positions against unsigned
metadata. The preregistered focused driver retains its original ten ordered
cases and real-source checks, adds zero/INT32_MAX controls and evaluates all
four negative-coordinate pairs before asserting. Actual old-mask0xF RED is
required before rejecting negatives and converting safely. The first launcher
fails before Python/compiler/test startup with WSL CreateVm0x800705b4 and no
phase output. Availability-only retry19412/PID11836 naturally exits0 in66368ms,
receipt hash7CED71D8EF934106464E3088861241D1F89D0BDF0F3DE4C46A7686BED9E387E2.
The first actual focused attempt records source-pre532/exit0 and reaches80/554
compilation steps before its retained log ends `interrupted by user`. On
continuation the old handles are missing, WSL has a new boot and those owned
processes are absent. Root sent no stop signal. No native log, source-post or
driver receipt exists; driver/build exit and the interruption's actor remain
unknown. The retained interruption observation has SHA-256
`99B7B7A31D8F820438CF79650510C6D05D77B6CE84315B9A7DA3443831739DE8`.
This is not behavioral RED.

Attempt r2 reuses the same build and existing objects. Source-pre503 exits0
with4664 pins; build505 naturally completes949/949/exit0 after6742025ms. Native
PID4397 exits1 in421ms with the original ordered ten cases, nine PASS, one
intended final boundary FAIL, exact negative mask0xF and zero ignored. Binary
and both lambda pre/post pins remain unchanged. The immutable raw receipt is
still FAIL, SHA7B2F91868161B87E4BCDE56392F08330004C85E24826E5B5CE13ED31052E2B09,
because66 global source inputs changed. Its head_post was not captured after
the failed source gate; do not invent one. Root/core separately accept only
the dated intended behavioral observation, not a stable platform/M1 gate.
The comparator now rejects signed negatives before UInt32 comparison; all
source assertions and10 registrations remain. The later GREEN attempts and
their raw source-gate results are recorded below.

LSP's two original hygiene cases remain frozen. A separate937-line companion
adds nine context/nominal/owner/invalid/Root-alias cases; main has84 unique calls.
Root independently checks the full header, final refinement, actual hashes and
exact ten-line main inverse. Same-AST/name/kind root decoys distinguish the real
nonroot lookup from a Root-only compile workaround. A separately frozen driver
892C9FAA checks exact84 registration, three test hashes, the original four hygiene
production phase C/H pins and the later symbols prerequisite pin, eleven expected
summary/reason failures, complete custom result rows,
native exit, helpers, source, HEAD and binary identities. Fixture failure cannot
count as behavioral RED. Native AST1867 and static review pass, not runtime tests.
The two Root-only production arguments were frozen through r3 as the explicit
RED mutant, not the final context-aware repair. Distinct fixture-prerequisite
failures are rejected by the driver. The actual r1/r2 failures, diagnostic
observation and dated r3 RED are recorded below; the current platform matrix
remains pending. A core review also traces nominal
type declarations through these lookups before approving their final context
selection. See
[LSP binding contract](../parser-and-semantics/lsp-typecheck-canonical-binding.md).
All outputs are E-root preregistered and reuse the existing GCC build; no source
copies or foreign resets are made. The cleanup and current lifecycle boundary
are recorded below.

## Focused Lambda GREEN Evidence (2026-10-16)

The normal same-tree lambda GREEN r2 build naturally terminates1 at837/838.
Its actual failure is `/usr/bin/ranlib: lib/libzr_vm_parser.a: No space left
on device`, not a native test result. BuildPID490 lasts4999779ms; no native
process starts. Source-post3922 exits0 with61 global changed inputs, while
both lambda source pins remain unchanged. Immutable receipt
`m1-gcc-range-witness-green-r2-receipt.json` has SHA-256
`FF5CFF5C0B536EDFB884EF7D7753E1AC7050F14342F6ABB6DAD12E67958C9C6C`;
build-log SHA-256 is
`164B0549BB53833EA71CC3C578C0D517DBCE5BBECCE5D01F4C3E6644837BFD68`.
The raw state remains FAIL and its absent head_post is not invented.

Unified86496 and all recorded build/source-post children are terminal. A later
read-only ps finds the listed old PIDs absent. E available space was72036352
bytes at06:57, so no new build started at that checkpoint. The user keeps other
source editing in parallel and accepts later revalidation; Root does not request an
external freeze, stop foreign work or create an isolated source copy.

The known failed Parser archive is100524694bytes, hash2E0DDB9D. It is the only
selected cleanup leaf: actual-config permission checks, immutable SHA/size/time
plan, successful preview and guarded Apply delete it at07:01:17Z. Apply receipt
EB00E9B5 records exactly100524694bytes released and zero errors. The necessary
raw failure receipt/build log are hash-protected and retained. No other current
build, shared cache, unknown file or source is deleted. All reusable objects
remain for normal regeneration; the lifecycle is active, not closed.

After capacity recovers, the normal same-tree lambda GREEN r3 build PID 539
completes 951/951 steps and exits 0. Native PID 4455 exits 0 with the original
ordered ten cases all PASS, zero failures and zero ignored. Both own lambda source
pre/post pins remain unchanged, as does binary SHA-256
`6630345AF183F6F4BDD9E45245A4EA182D43D29231A4524D3CBB9DD3E72913D9`,
41217544 bytes. Source-post PID 4456 exits 0 but records 62 global changed inputs.
The immutable receipt at
`E:/cargo-targets/zr_vm/reports/m1-gcc-range-witness-green-r3-receipt.json`
has SHA-256
`580B32C2C240F2C720C238BEF4644066AD6D40F126AA528A54357A3C61B0AECB`
and remains raw FAIL, with no head_post. Independent core review accepts only
dated functional GREEN, not a source-stable GCC or M1 pass.

## Focused LSP RED And Diagnostic Evidence (2026-10-16)

RED r1 session35897 is terminal: build PID509 exits1 on fixture API uses, and no
native process starts. Four nonexistent `ZR_VALUE_TYPE_INT` constants and the
nonexistent `ZrParser_SyntaxContext_Invalid` constructor prevent compilation.
The raw receipt at
`E:/cargo-targets/zr_vm/reports/m1-gcc-lsp-hygiene-red-r1-receipt.json`
has SHA-256
`89F08034BEAD515171D3547D1FC4C8EF51A783E35A95437BDC0AEA0DA240B8BD`.
It also records six foreign changed source inputs and no head_post; this is a
prerequisite failure, not behavioral RED. The exact five-use fixture correction
uses builtin-int `ZR_VALUE_TYPE_INT64` and a public table-owned invalid-mark
compound literal. The declaration-context header becomes937 lines; Root's full
RAM inverse restores the prior two header hashes. All84 main calls, behavior
assertions, eleven exact expected failures and four production RED pins remain.

RED r2 session32044 completes build PID485/exit0 in11 steps. Native PID559 exits1
with84 ordered unique starts/results:72 PASS and12 FAIL. The eleven registered
failures have their exact reasons, with no fixture prerequisite error. The sole
extra failure is the existing `Semantic Analyzer Unknown Parameter Preserves
Identity`, whose raw reason is `An unavailable parameter type must not expose an
outer binding or infer its return type`. The unchanged helper correctly rejects
twelve failures against eleven registered; it does not grant exact RED. Receipt
`E:/cargo-targets/zr_vm/reports/m1-gcc-lsp-hygiene-red-r2-receipt.json`
has SHA-256
`99902011C67FC9F50ED79FCBB819C03EDCE97450EE7E0044DA8EB593383C887A`
and remains raw FAIL. Source-pre/source-post each contain4772 identical inputs
and share SHA-256
`54F151FB3DBA94B0E933300B2713A5012F24D100DD45D9B85C8E539645FFF5E0`;
HEAD pre/post both equal `d85ce7a48311b3419714c9a240f4e36cfcbb3520`.
The phase-failure rejection occurs before the binary-post gate, so the receipt
has no binary-post result; a later observation cannot fill that missing field.

Read-only GDB r1 uses that actual RED executable without rebuilding. GDB PID527
exits0; the inferior PID549 exits1. Case0 passes with parameter declaration/read
symbol2/type0 and callable symbol3/type0. Case1, with an explicit `:int` return,
retains parameter declaration/read symbol3/type0 and the same owner, but the
callable is symbol2/type3. Hover stays NULL because the callable-type assertion
short-circuits before the hover query. These values isolate erroneous callable
publication, not parameter identity loss or an outer binding capture. Receipt
`E:/cargo-targets/zr_vm/reports/m1-gcc-lsp-unknown-parameter-diagnosis-r1-receipt.json`
has SHA-256
`3FD9CDD9650902A0C6785D3C0DE81E0F6F83446294E4C51CE93575FC50B8904F`.
It remains raw FAIL after three foreign ExecIR inputs change
(`exec_ir_verify_effects.c`, `exec_ir_verify_ssa.c`, `exec_ir_verify_ssa.h`);
binary-post and head-post gates are not reached. This is causal observation
only, with no source-stable or acceptance credit.

Source tracing finds that the `symbols.c` `!parameterFullyResolved` branch emits
a diagnostic but omits `hasUnresolvedParameter`. Core inserts only that one-line
prerequisite correction; Root's full RAM inverse restores B822 and pins current
symbols SHA11C6A53B as a fifth production phase input. The four main hygiene
production files stay RED for that run.

RED r3 session12432 is terminal and drained with driver exit1. Build PID509
completes7/7 actions, exits0 in3102251ms; native PID644 exits1 in1508ms with84
ordered unique starts/results:73 PASS and exactly the original eleven FAIL.
All eleven names, order and reasons match, with no fixture prerequisite error;
`Unknown Parameter Preserves Identity` passes. Binary SHA-256
`E4AEAB4065494BF5DEB851F57BE497EA4BF495D44A761D15E702A28BA8F9FA25`,
53963096 bytes, remains unchanged after execution. The immutable receipt at
`E:/cargo-targets/zr_vm/reports/m1-gcc-lsp-hygiene-red-r3-receipt.json`
has SHA-256
`19000590E066D32F478A244A96B526BF40D95F995251EFF55A2EC95800266A5D`.

Source-post PID645 exits0 in14464ms, but source-pre4772 becomes source-post4774.
The four changed foreign paths are:

- `tests/parser/support/ssa_execir_builder_storage_cases.inc`
- `tests/parser/test_ssa_source_integer_expression_proof.c`
- `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c`
- `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build_storage_internal.h`

The raw receipt remains FAIL at the full source gate; HEAD-post and
HEAD-unchanged fields are absent. Root's pre-lease review confirms the three
tests and five RED production inputs match source-pre, source-post and the
reviewed files. This supports dated functional TDD counterexamples only, not
accepted fixed-input RED. No later current-HEAD observation replaces the absent
receipt field, and neither source nor HEAD gates are relaxed. With the user's
parallel-edit/revalidation decision retained, Root authorizes the bounded
four-file repair. Source review now freezes that implementation with the
budget-boundary finding below, not GREEN. The original 84-case GREEN and the
complete GCC/Clang/final MSVC matrix remain pending. No platform or M1
acceptance, milestone commit or M2 advancement is credited.

## Declaration Budget Follow-Up (2026-10-16)

Independent source review finds that the shared exact-declaration lookup returns
`NULL` for both a missing owner and budgeted normalization failure. After a
successful entry precheck, a marked preowned callable may therefore publish a
replacement symbol/overload member. A fresh nominal may publish successfully
but then fail its post-publication lookup after scratch-budget exhaustion.
Variable registration also requires lookup failure before inferred-type
publication. The original seven-case r1 now supplies the dated native
counterexamples below, not fixed-input acceptance or a claim that production
is fixed.

Root and independent review froze the original 628-line, seven-case r1 regression
and dedicated target registration. Its historical frozen SHA-256 inputs are:

- `tests/language_server/test_semantic_declaration_budget.c`:
  `4859A78CB36A7F2A9BF9C5D7C5F8B01D278C7A613F24331D7DDEEA45FFD1204C`
- `tests/cmake/zr_vm_lsp_declaration_budget_tests.cmake`:
  `898AE2CA59569966087D7E867AF2D83CF043F53096E8163A7016C3A9238E7290`
- `tests/cmake/lsp-component-targets.cmake`:
  `AADB6534524B7B64EDE4130FD4647F5118F6863EAAA9FC46CCC9002B798F4C18`

The single dedicated include has a RAM inverse restoring pre-edit 38B1001C.
Target `zr_vm_language_server_semantic_declaration_budget_test` and CTest
`language_server_semantic_declaration_budget` are independent of the unchanged
original 84 analyzer cases and foundation 52 selection. The original seven cover
marked callable budget failure, fresh interface/delegate publication, wide-budget
full-identity reuse, Root reuse without scratch heap, ITEM-equivalent differing
raw contexts, a fresh nominal past an earlier Root decoy, and variable publication
ordering. Normalization cost is measured dynamically as a positive allowance;
the fixture does not assume fixed scratch-byte costs.

Normal WSL GCC `red --attempt r1`, unified session 75983, is terminal and drained
with driver exit 1. Build PID 534 completes 18/18 actions, exit 0; native PID 767
exits 5 with seven ordered unique cases, 2 PASS, exactly the five registered FAIL
names/reasons and 0 IGNORE. There is no fixture prerequisite failure; both wide
and Root controls pass. The immutable receipt
`E:/cargo-targets/zr_vm/reports/m1-gcc-declaration-budget-red-r1-receipt.json`
has SHA-256 `3888EF4CC44C8A5B55AC689DFE769D84E2176C6DBF326341E999FE45329FC0EE`.

The raw receipt stays FAIL: both inventories contain 4776 entries, but five
foreign source paths change:

- `tests/parser/test_ssa_compare_metadata_guards.c`
- `tests/parser/test_ssa_core_model.c`
- `tests/parser/test_ssa_value_validation.c`
- `zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c`
- `zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module_validation.c`

HEAD actually changes from `d3936df4dd6cab715d79c8aa2ee62866c8d8ba85` to
`063644809b74de0d8236054677b3feac295cb22c`, so `head_unchanged` is false.
Independent review authenticates all seventeen owned source-pre/post identities
and the stable 53748264-byte binary SHA-256
`4DD65559556255CBAED0BC9E6B3019615A1E9A6F21966235C48A8396BD013912`.
The later eleven-case test version does not rewrite this seven-case receipt.
This is dated functional TDD RED, not accepted fixed-input RED, native GREEN or
M1/platform evidence. Full source, HEAD, helper and binary gates remain strict;
it neither supersedes hygiene r3's raw FAIL nor fills its absent HEAD-post fields.

The frozen helper `E:/cargo-targets/zr_vm/tmp/m1-declaration-budget-validation.py`
has SHA-256 `CE277678027F284A3CC1BADB084DD35F32E12A713AFD084A4FA15686ED70CB24`.

Separate isolated helper regressions exercise final classification and checked
exception paths. RED r1 has 4 failures in 8 cases, followed by 8/8 GREEN; RED r2
has 1 failure in 10 cases, followed by 10/10 GREEN. The latter runs as PID 3400,
exit 0, with receipt
`E:/cargo-targets/zr_vm/reports/m1-budget-helper-final-state-green-r2-receipt.json`,
SHA-256 `5E59D482D70A2EB670967F0981CAB8383C924F45A6AF34E443A7F5B359713B1B`.
This establishes validator control flow only, not a real interruption signal,
child interruption, source enumeration, compilation or native test execution.
Budget production repair, original 84-case GREEN, complete GCC/Clang/final MSVC,
M1 acceptance and milestone commit remain pending; M2-M6 scope is unchanged.

### Checked Lookup Candidate And Actual RED r2

The prior binding/callable context comparators also treat budgeted Normalize
failure as a nonmatch. With a real public `ZrParser_TypeEnvironment_New(state)`
environment whose parent and semantic snapshot are NULL, exhausted scratch can
append a duplicate marked variable or callable. Multi-candidate
`LookupFunctionsWithContext` can skip a later failed comparison and return TRUE
with a partial overload set. Invalid contexts never compare equal. This is a
static P2 and reachable existing M1 context-budget prerequisite, not a native
observation or a regression attributed to a future checked exact-query patch.
An exact-query-only repair cannot silently exempt it from M1.

Root and independent review now freeze four authored public no-snapshot cases:
marked-variable reuse without a duplicate binding; marked-callable same-AST
duplicate rejection; Root/MixedSite ITEM-equivalent same-AST rejection; and a
two-signature lookup that fails closed rather than returning partial success.
The authors are stopped. Current frozen inputs are:

- 637-line `tests/language_server/test_semantic_declaration_budget.c`:
  `71CDCD5EEDE570611B8A8E9AB244776A5AECE23458AAD8A3E2C9E3EDDF4FF706`
- 426-line `tests/language_server/test_semantic_environment_budget_cases.h`:
  `216F2D59E0B91131642EC9836E033004C8CD47E2352FD81FECC4A94AB2017A39`

Root and independent full-main RAM inverses restore original r1 SHA4859A78C,
preserving all original seven functions/assertions. Original84 and both CMake
files were unchanged at RED r2. The normal GCC run in unified75735 is now
terminal/drained: build517 completes6/6 actions/exit0; native566 exits9 with
eleven unique ordered cases,2 PASS/nine exact FAIL/0 IGNORE and no prerequisite
failure. The immutable receipt
`E:/cargo-targets/zr_vm/reports/m1-gcc-declaration-budget-red-r2-receipt.json`
has SHA-256 `08934E6EFDF81E956B5D086B16CA74D8D97E221AA4C5FBCB4CA2964BB12944B9`.
Both complete4777-source inventories have identical SHA09A58B41; all18 owned
pins and binary DE7F0768/53774328 bytes are stable. Raw state remains FAIL solely
for HEAD06364480-to-badc069b. This supplies dated behavioral TDD evidence for
the original five and new four failures, not fixed Source+HEAD acceptance.

After the bounded GO, the ten-path candidate is implemented and its author
STOPPED. Root and independent core review report no new P1/P2. Exact lookup now
returns checked BOOL/out-record, scans all raw hits first, then lazily normalizes
the query once. The new private `type_environment_context_lookup.h` separates
comparison status from `outEqual`; `type_environment_variable_lookup.c` owns
current/full-chain finders and four extracted existing public wrappers. Variable
registration preflights before type publication. Callable/alias/typed annotation
paths propagate failure, and failed multi-lookup frees/reconstructs its result.
Detailed contracts are in [LSP canonical bindings](../parser-and-semantics/lsp-typecheck-canonical-binding.md).
Normal recursive-glob discovery of the new TU still needs an actual build.

Current-scope registration/callable shadow remains separate from full-chain
RuntimeRoot/Capture/public lookup. There is no public ABI expansion, complete
pointer-caller error/miss distinction or global allocator-OOM transaction claim.
The four no-snapshot tests directly exercise registration and multi-lookup,
not RuntimeRoot/Capture, typed-call or external-alias branches. Original84's
utility changes from8C04A6E7 to BA21308E solely for BOOL/out migration; Root and
independent nine-region RAM inverses restore all50775 original bytes, preserving
all assertions, reasons and84 main calls. Budget tests and CMake stay frozen.

GREEN r2 freezes4 test/CMake plus16 phase pins. Launcher7197 exits1/drained with
WSL CreateInstance/CreateVm/0x800705b4; the helper never starts and all5 outputs
remain absent. Minimal `/usr/bin/true` probe8920 also exits1 with the same error.
No compiler/native GREEN or helper receipt exists. Services run, but the cause
is unknown; no machine/service restart, configuration change or foreign process
control is performed. Original84 GREEN, GCC/Clang/final MSVC and M1 remain open.

Nine-doc r10, PID2272/exit0, checks9 documents/669 metadata references/34 links
with11 stable pins; receiptC4E44918 covers only its old hashes. Earlier r8/r9
evidence remains historical. None confers compiler/platform/M1 credit or validates
this changed revision; fresh r11 document validation remains Root-owned.

The separate budget target is now explicitly included in the full phase of both
unstarted platform helpers: selected CTest54 becomes55, with build, discovery,
execution and binary pre/post pins. Foundation52, lower14/323, original51/127,
aggregates and all source/HEAD gates are unchanged. Frozen helper SHA-256 values:

- WSL: `AEC3755EC333AFD3A63946A07E872EC4F8EA2799DC74F741C4307595110FD656`.
- MSVC: `254D22754C2A98755ADCC1F36465A6E395A129E9DB5B214D7FF5E698A892EC60`.

Root and independent core review each recover the prior E7A887BE/ABA4614F bytes
by pure RAM inverse. The unchanged isolated eight-case HEAD checker passes after
this expansion (PID19004, exit0, all four pins stable); receipt
`E:/cargo-targets/zr_vm/reports/m1-matrix-budget-head-gate-green-r2-receipt.json`,
SHA-256 `B4705C3596823B139207CDF0D7A7513DA8F24D61E38DADAEC0FB879C4B1C8EDE`.
This is validator evidence only. Normal matrix execution still waits for the
focused budget and original84 GREEN gates.

## Matrix HEAD Checker Evidence (2026-10-16)

The superseded WSL helper10A8D928 and MSVC helperD7CF5D7E recorded
`head_unchanged` but did not reject false. The actual isolated AST regression
executes each helper's final verification try/except with controlled same/changed
HEAD and initial PASS/FAIL states. Its RED receipt
`E:/cargo-targets/zr_vm/reports/m1-matrix-head-gate-red-r1-receipt.json`,
SHA-256 `D7BB45D2144E3A4C08C6420BFBF3D3422684B1C1FBA46DCC546B9A63FE1F586B`,
records8 cases,4 PASS and4 expected failures.

After focused r3 is terminal, Root adds only a two-line HEAD-false if/raise to
each helper; full RAM inverses recover the exact superseded hashes. The
unchanged regression helper0B791C06 then passes all8 cases, PID7884/exit0, with
all four input hashes stable during that run. Its GREEN receipt
`E:/cargo-targets/zr_vm/reports/m1-matrix-head-gate-green-r1-receipt.json`
has SHA-256
`3A6A5EC3E4B530D2201EBE926759F9272F25849C8F1713397D54607CAC57768F`.
Changed HEAD now forces FAIL and verification_error without overwriting the
prior native error. Source enumeration and compiler/native matrix execution
do not run in this checker regression; its credit is control flow only.

Unstarted r5/r6 registrations now pin the reviewed helper hashes while retaining
the superseded identities:

- WSL: `E7A887BE8C781320DEE4219A64804CA2568004382CF9BFA412EAFD64B973FA05`
- MSVC: `ABA4614F3EEB745F8E5BF6F2EC2A93D1EF82CC65697FE9202DBDB67B2885A11E`

No r5/r6 native or platform result follows from this checker GREEN. Full-source,
HEAD, helper and binary gates remain mandatory; M1 is not accepted and M2-M6
remain pending.
