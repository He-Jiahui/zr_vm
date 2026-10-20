zr_vm_add_unity_test_target(
        zr_vm_comptime_control_flow_test
        ${CMAKE_SOURCE_DIR}/tests/compileTime/test_comptime_control_flow.c
)
target_include_directories(zr_vm_comptime_control_flow_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/compiler
)
zr_vm_link_parser_core_plus_library(zr_vm_comptime_control_flow_test)
add_test(NAME comptime_control_flow COMMAND $<TARGET_FILE:zr_vm_comptime_control_flow_test>)
set_tests_properties(comptime_control_flow PROPERTIES
        TIMEOUT 30
        ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1"
)

zr_vm_add_unity_test_target(
        zr_vm_syntax_expansion_contract_test
        ${CMAKE_SOURCE_DIR}/tests/compileTime/test_syntax_expansion_contract.c
        ${CMAKE_SOURCE_DIR}/tests/unity_lifecycle_noop.c
)
zr_vm_link_parser_core_plus_library(zr_vm_syntax_expansion_contract_test)
add_test(NAME syntax_expansion_contract COMMAND $<TARGET_FILE:zr_vm_syntax_expansion_contract_test>)
set_tests_properties(syntax_expansion_contract PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_comptime_budget_interleaving_test
        ${CMAKE_SOURCE_DIR}/tests/compileTime/test_comptime_budget_interleaving.c
        ${CMAKE_SOURCE_DIR}/tests/unity_lifecycle_noop.c
)
zr_vm_link_parser_core_plus_library(zr_vm_comptime_budget_interleaving_test)
add_test(NAME comptime_budget_interleaving COMMAND $<TARGET_FILE:zr_vm_comptime_budget_interleaving_test>)
set_tests_properties(comptime_budget_interleaving PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_comptime_aggregate_budget_test
        ${CMAKE_SOURCE_DIR}/tests/compileTime/test_comptime_aggregate_budget.c
)
target_include_directories(zr_vm_comptime_aggregate_budget_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/compiler
)
zr_vm_link_parser_core_plus_library(zr_vm_comptime_aggregate_budget_test)
add_test(NAME comptime_aggregate_budget COMMAND $<TARGET_FILE:zr_vm_comptime_aggregate_budget_test>)
set_tests_properties(comptime_aggregate_budget PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_comptime_definition_environment_test
        ${CMAKE_SOURCE_DIR}/tests/compileTime/test_comptime_definition_environment.c
)
target_include_directories(zr_vm_comptime_definition_environment_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
)
zr_vm_link_parser_core_plus_library(zr_vm_comptime_definition_environment_test)
add_test(NAME macro_comptime_definition_environment
        COMMAND $<TARGET_FILE:zr_vm_comptime_definition_environment_test>)
set_tests_properties(macro_comptime_definition_environment PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_token_stream_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_token_stream.c
)
zr_vm_link_parser_core_plus_library(zr_vm_token_stream_test)
add_test(NAME macro_token_stream COMMAND $<TARGET_FILE:zr_vm_token_stream_test>)
set_tests_properties(macro_token_stream PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_syntax_hygiene_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_syntax_hygiene.c
)
zr_vm_link_parser_core_plus_library(zr_vm_syntax_hygiene_test)
add_test(NAME macro_syntax_hygiene COMMAND $<TARGET_FILE:zr_vm_syntax_hygiene_test>)
set_tests_properties(macro_syntax_hygiene PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_token_source_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_token_source.c
)
zr_vm_link_parser_core_plus_library(zr_vm_token_source_test)
target_include_directories(zr_vm_token_source_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser)
add_test(NAME macro_token_source COMMAND $<TARGET_FILE:zr_vm_token_source_test>)
set_tests_properties(macro_token_source PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_token_stream_limits_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_token_stream_limits.c
)
zr_vm_link_parser_core_plus_library(zr_vm_token_stream_limits_test)
add_test(NAME macro_token_stream_limits COMMAND $<TARGET_FILE:zr_vm_token_stream_limits_test>)
set_tests_properties(macro_token_stream_limits PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_token_span_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_token_span.c
        ${CMAKE_SOURCE_DIR}/tests/unity_lifecycle_noop.c
)
zr_vm_link_parser_core_plus_library(zr_vm_token_span_test)
add_test(NAME macro_token_span COMMAND $<TARGET_FILE:zr_vm_token_span_test>)
set_tests_properties(macro_token_span PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_comptime_frame_lifetime_test
        ${CMAKE_SOURCE_DIR}/tests/compileTime/test_comptime_frame_lifetime.c
)
target_include_directories(zr_vm_comptime_frame_lifetime_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/compiler
)
zr_vm_link_parser_core_plus_library(zr_vm_comptime_frame_lifetime_test)
add_test(NAME comptime_frame_lifetime COMMAND $<TARGET_FILE:zr_vm_comptime_frame_lifetime_test>)
set_tests_properties(comptime_frame_lifetime PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_token_hygiene_lifetime_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_token_hygiene_lifetime.c
)
zr_vm_link_parser_core_plus_library(zr_vm_token_hygiene_lifetime_test)
add_test(NAME macro_token_hygiene_lifetime COMMAND $<TARGET_FILE:zr_vm_token_hygiene_lifetime_test>)
set_tests_properties(macro_token_hygiene_lifetime PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_token_stream_boundaries_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_token_stream_boundaries.c
)
zr_vm_link_parser_core_plus_library(zr_vm_token_stream_boundaries_test)
add_test(NAME macro_token_stream_boundaries COMMAND $<TARGET_FILE:zr_vm_token_stream_boundaries_test>)
set_tests_properties(macro_token_stream_boundaries PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_comptime_control_flow_edges_test
        ${CMAKE_SOURCE_DIR}/tests/compileTime/test_comptime_control_flow_edges.c
)
target_include_directories(zr_vm_comptime_control_flow_edges_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/compiler
)
zr_vm_link_parser_core_plus_library(zr_vm_comptime_control_flow_edges_test)
add_test(NAME comptime_control_flow_edges COMMAND $<TARGET_FILE:zr_vm_comptime_control_flow_edges_test>)
set_tests_properties(comptime_control_flow_edges PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_parser_token_ownership_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_parser_token_ownership.c
)
zr_vm_link_parser_core_plus_library(zr_vm_parser_token_ownership_test)
add_test(NAME macro_parser_token_ownership COMMAND $<TARGET_FILE:zr_vm_parser_token_ownership_test>)
set_tests_properties(macro_parser_token_ownership PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_context_budget_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_context_budget.c
)
target_include_directories(zr_vm_context_budget_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
)
zr_vm_link_parser_core_plus_library(zr_vm_context_budget_test)
add_test(NAME macro_context_budget COMMAND $<TARGET_FILE:zr_vm_context_budget_test>)
set_tests_properties(macro_context_budget PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_context_identity_queries_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_context_identity_queries.c
)
target_include_directories(zr_vm_context_identity_queries_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
)
zr_vm_link_parser_core_plus_library(zr_vm_context_identity_queries_test)
add_test(NAME macro_context_identity_queries COMMAND $<TARGET_FILE:zr_vm_context_identity_queries_test>)
set_tests_properties(macro_context_identity_queries PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_compile_tool_phase_gate_test
        ${CMAKE_SOURCE_DIR}/tests/compileTime/test_compile_tool_phase_gate.c
)
target_include_directories(zr_vm_compile_tool_phase_gate_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser/compiler
)
zr_vm_link_parser_core_plus_library(zr_vm_compile_tool_phase_gate_test)
add_test(NAME compile_tool_phase_gate COMMAND $<TARGET_FILE:zr_vm_compile_tool_phase_gate_test>)
set_tests_properties(compile_tool_phase_gate PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_token_stream_provenance_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_token_stream_provenance.c
)
zr_vm_link_parser_core_plus_library(zr_vm_token_stream_provenance_test)
add_test(NAME macro_token_stream_provenance COMMAND $<TARGET_FILE:zr_vm_token_stream_provenance_test>)
set_tests_properties(macro_token_stream_provenance PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_token_source_edges_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_token_source_edges.c
)
target_include_directories(zr_vm_token_source_edges_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
)
zr_vm_link_parser_core_plus_library(zr_vm_token_source_edges_test)
add_test(NAME macro_token_source_edges COMMAND $<TARGET_FILE:zr_vm_token_source_edges_test>)
set_tests_properties(macro_token_source_edges PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_token_budget_peak_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_token_budget_peak.c
)
target_include_directories(zr_vm_token_budget_peak_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
)
zr_vm_link_parser_core_plus_library(zr_vm_token_budget_peak_test)
add_test(NAME macro_token_budget_peak COMMAND $<TARGET_FILE:zr_vm_token_budget_peak_test>)
set_tests_properties(macro_token_budget_peak PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_ast_array_budget_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_ast_array_budget.c
)
zr_vm_link_parser_core_plus_library(zr_vm_ast_array_budget_test)
add_test(NAME macro_ast_array_budget COMMAND $<TARGET_FILE:zr_vm_ast_array_budget_test>)
set_tests_properties(macro_ast_array_budget PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_parser_array_budget_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_parser_array_budget.c
)
target_include_directories(zr_vm_parser_array_budget_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
)
zr_vm_link_parser_core_plus_library(zr_vm_parser_array_budget_test)
add_test(NAME macro_parser_array_budget COMMAND $<TARGET_FILE:zr_vm_parser_array_budget_test>)
set_tests_properties(macro_parser_array_budget PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_token_context_ledger_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_token_context_ledger.c
)
target_include_directories(zr_vm_token_context_ledger_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
)
zr_vm_link_parser_core_plus_library(zr_vm_token_context_ledger_test)
add_test(NAME macro_token_context_ledger COMMAND $<TARGET_FILE:zr_vm_token_context_ledger_test>)
set_tests_properties(macro_token_context_ledger PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_parser_collection_budget_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_parser_collection_budget.c
)
target_include_directories(zr_vm_parser_collection_budget_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
)
zr_vm_link_parser_core_plus_library(zr_vm_parser_collection_budget_test)
add_test(NAME macro_parser_collection_budget COMMAND $<TARGET_FILE:zr_vm_parser_collection_budget_test>)
set_tests_properties(macro_parser_collection_budget PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_parser_metadata_budget_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_parser_metadata_budget.c
)
target_include_directories(zr_vm_parser_metadata_budget_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
)
zr_vm_link_parser_core_plus_library(zr_vm_parser_metadata_budget_test)
add_test(NAME macro_parser_metadata_budget COMMAND $<TARGET_FILE:zr_vm_parser_metadata_budget_test>)
set_tests_properties(macro_parser_metadata_budget PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_parser_primary_budget_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_parser_primary_budget.c
)
target_include_directories(zr_vm_parser_primary_budget_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
        ${CMAKE_SOURCE_DIR}/tests
)
zr_vm_link_parser_core_plus_library(zr_vm_parser_primary_budget_test)
add_test(NAME macro_parser_primary_budget COMMAND $<TARGET_FILE:zr_vm_parser_primary_budget_test>)
set_tests_properties(macro_parser_primary_budget PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_syntax_hygiene_aliases_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_syntax_hygiene_aliases.c
)
target_include_directories(zr_vm_syntax_hygiene_aliases_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
        ${CMAKE_SOURCE_DIR}/tests
)
zr_vm_link_parser_core_plus_library(zr_vm_syntax_hygiene_aliases_test)
add_test(NAME macro_syntax_hygiene_aliases COMMAND $<TARGET_FILE:zr_vm_syntax_hygiene_aliases_test>)
set_tests_properties(macro_syntax_hygiene_aliases PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_syntax_hygiene_import_aliases_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_syntax_hygiene_import_aliases.c
)
target_include_directories(zr_vm_syntax_hygiene_import_aliases_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
        ${CMAKE_SOURCE_DIR}/tests
)
zr_vm_link_parser_core_plus_library(zr_vm_syntax_hygiene_import_aliases_test)
add_test(NAME macro_syntax_hygiene_import_aliases
        COMMAND $<TARGET_FILE:zr_vm_syntax_hygiene_import_aliases_test>)
set_tests_properties(macro_syntax_hygiene_import_aliases PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_syntax_hygiene_alias_scope_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_syntax_hygiene_alias_scope.c
)
target_include_directories(zr_vm_syntax_hygiene_alias_scope_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
        ${CMAKE_SOURCE_DIR}/tests
)
zr_vm_link_parser_core_plus_library(zr_vm_syntax_hygiene_alias_scope_test)
add_test(NAME macro_syntax_hygiene_alias_scope
        COMMAND $<TARGET_FILE:zr_vm_syntax_hygiene_alias_scope_test>)
set_tests_properties(macro_syntax_hygiene_alias_scope PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_syntax_hygiene_nominal_types_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_syntax_hygiene_nominal_types.c
)
target_include_directories(zr_vm_syntax_hygiene_nominal_types_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
        ${CMAKE_SOURCE_DIR}/tests
)
zr_vm_link_parser_core_plus_library(zr_vm_syntax_hygiene_nominal_types_test)
add_test(NAME macro_syntax_hygiene_nominal_types
        COMMAND $<TARGET_FILE:zr_vm_syntax_hygiene_nominal_types_test>)
set_tests_properties(macro_syntax_hygiene_nominal_types PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_syntax_hygiene_nominal_prototypes_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_syntax_hygiene_nominal_prototypes.c
)
target_include_directories(zr_vm_syntax_hygiene_nominal_prototypes_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
        ${CMAKE_SOURCE_DIR}/tests
)
zr_vm_link_parser_core_plus_library(zr_vm_syntax_hygiene_nominal_prototypes_test)
add_test(NAME macro_syntax_hygiene_nominal_prototypes
        COMMAND $<TARGET_FILE:zr_vm_syntax_hygiene_nominal_prototypes_test>)
set_tests_properties(macro_syntax_hygiene_nominal_prototypes PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_syntax_hygiene_generic_prototypes_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_syntax_hygiene_generic_prototypes.c
)
target_include_directories(zr_vm_syntax_hygiene_generic_prototypes_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
        ${CMAKE_SOURCE_DIR}/tests
)
zr_vm_link_parser_core_plus_library(zr_vm_syntax_hygiene_generic_prototypes_test)
add_test(NAME macro_syntax_hygiene_generic_prototypes
        COMMAND $<TARGET_FILE:zr_vm_syntax_hygiene_generic_prototypes_test>)
set_tests_properties(macro_syntax_hygiene_generic_prototypes PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_syntax_hygiene_generic_definitions_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_syntax_hygiene_generic_definitions.c
)
target_include_directories(zr_vm_syntax_hygiene_generic_definitions_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
        ${CMAKE_SOURCE_DIR}/tests
)
zr_vm_link_parser_core_plus_library(zr_vm_syntax_hygiene_generic_definitions_test)
add_test(NAME macro_syntax_hygiene_generic_definitions
        COMMAND $<TARGET_FILE:zr_vm_syntax_hygiene_generic_definitions_test>)
set_tests_properties(macro_syntax_hygiene_generic_definitions PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_syntax_hygiene_compiler_prototype_lookup_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_syntax_hygiene_compiler_prototype_lookup.c
)
target_include_directories(zr_vm_syntax_hygiene_compiler_prototype_lookup_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
        ${CMAKE_SOURCE_DIR}/tests
)
zr_vm_link_parser_core_plus_library(zr_vm_syntax_hygiene_compiler_prototype_lookup_test)
add_test(NAME macro_syntax_hygiene_compiler_prototype_lookup
        COMMAND $<TARGET_FILE:zr_vm_syntax_hygiene_compiler_prototype_lookup_test>)
set_tests_properties(macro_syntax_hygiene_compiler_prototype_lookup PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_syntax_hygiene_receiver_members_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_syntax_hygiene_receiver_members.c
)
target_include_directories(zr_vm_syntax_hygiene_receiver_members_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
        ${CMAKE_SOURCE_DIR}/tests
)
zr_vm_link_parser_core_plus_library(zr_vm_syntax_hygiene_receiver_members_test)
add_test(NAME macro_syntax_hygiene_receiver_members
        COMMAND $<TARGET_FILE:zr_vm_syntax_hygiene_receiver_members_test>)
set_tests_properties(macro_syntax_hygiene_receiver_members PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_syntax_hygiene_inherited_members_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_syntax_hygiene_inherited_members.c
)
target_include_directories(zr_vm_syntax_hygiene_inherited_members_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
        ${CMAKE_SOURCE_DIR}/tests
)
zr_vm_link_parser_core_plus_library(zr_vm_syntax_hygiene_inherited_members_test)
add_test(NAME macro_syntax_hygiene_inherited_members
        COMMAND $<TARGET_FILE:zr_vm_syntax_hygiene_inherited_members_test>)
set_tests_properties(macro_syntax_hygiene_inherited_members PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_syntax_hygiene_base_publication_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_syntax_hygiene_base_publication.c
)
target_include_directories(zr_vm_syntax_hygiene_base_publication_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
        ${CMAKE_SOURCE_DIR}/tests
)
zr_vm_link_parser_core_plus_library(zr_vm_syntax_hygiene_base_publication_test)
add_test(NAME macro_syntax_hygiene_base_publication
        COMMAND $<TARGET_FILE:zr_vm_syntax_hygiene_base_publication_test>)
set_tests_properties(macro_syntax_hygiene_base_publication PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_syntax_hygiene_field_results_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_syntax_hygiene_field_results.c
)
target_include_directories(zr_vm_syntax_hygiene_field_results_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
        ${CMAKE_SOURCE_DIR}/tests
)
zr_vm_link_parser_core_plus_library(zr_vm_syntax_hygiene_field_results_test)
add_test(NAME macro_syntax_hygiene_field_results
        COMMAND $<TARGET_FILE:zr_vm_syntax_hygiene_field_results_test>)
set_tests_properties(macro_syntax_hygiene_field_results PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_function_type_conversion_lifetime_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_function_type_conversion_lifetime.c
)

target_include_directories(zr_vm_function_type_conversion_lifetime_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
        ${CMAKE_SOURCE_DIR}/tests
)
zr_vm_link_parser_core_plus_library(zr_vm_function_type_conversion_lifetime_test)
add_test(NAME macro_function_type_conversion_lifetime
        COMMAND $<TARGET_FILE:zr_vm_function_type_conversion_lifetime_test>)
set_tests_properties(macro_function_type_conversion_lifetime PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_unpublished_type_metadata_lifetime_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_unpublished_type_metadata_lifetime.c
)

target_include_directories(zr_vm_unpublished_type_metadata_lifetime_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
        ${CMAKE_SOURCE_DIR}/tests
)
zr_vm_link_parser_core_plus_library(zr_vm_unpublished_type_metadata_lifetime_test)
add_test(NAME macro_unpublished_type_metadata_lifetime
        COMMAND $<TARGET_FILE:zr_vm_unpublished_type_metadata_lifetime_test>)
set_tests_properties(macro_unpublished_type_metadata_lifetime PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_nominal_declaration_publication_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_nominal_declaration_publication.c
)
target_include_directories(zr_vm_nominal_declaration_publication_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
        ${CMAKE_SOURCE_DIR}/tests
)
zr_vm_link_parser_core_plus_library(zr_vm_nominal_declaration_publication_test)
add_test(NAME macro_nominal_declaration_publication
        COMMAND $<TARGET_FILE:zr_vm_nominal_declaration_publication_test>)
set_tests_properties(macro_nominal_declaration_publication PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_open_array_annotation_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_open_array_annotation.c
)
target_include_directories(zr_vm_open_array_annotation_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
        ${CMAKE_SOURCE_DIR}/tests
)
zr_vm_link_parser_core_plus_library(zr_vm_open_array_annotation_test)
add_test(NAME macro_open_array_annotation
        COMMAND $<TARGET_FILE:zr_vm_open_array_annotation_test>)
set_tests_properties(macro_open_array_annotation PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_callable_return_inference_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_callable_return_inference.c
)
target_include_directories(zr_vm_callable_return_inference_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
        ${CMAKE_SOURCE_DIR}/tests
)
zr_vm_link_parser_core_plus_library(zr_vm_callable_return_inference_test)
add_test(NAME macro_callable_return_inference
        COMMAND $<TARGET_FILE:zr_vm_callable_return_inference_test>)
set_tests_properties(macro_callable_return_inference PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_lambda_semantic_ir_isolation_test
        ${CMAKE_SOURCE_DIR}/tests/parser/test_lambda_semantic_ir_isolation.c
)

target_include_directories(zr_vm_lambda_semantic_ir_isolation_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
        ${CMAKE_SOURCE_DIR}/tests
)
zr_vm_link_parser_core_plus_library(zr_vm_lambda_semantic_ir_isolation_test)
add_test(NAME macro_lambda_semantic_ir_isolation
        COMMAND $<TARGET_FILE:zr_vm_lambda_semantic_ir_isolation_test>)
set_tests_properties(macro_lambda_semantic_ir_isolation PROPERTIES TIMEOUT 30)

zr_vm_add_unity_test_target(
        zr_vm_comptime_import_alias_environment_test
        ${CMAKE_SOURCE_DIR}/tests/compileTime/test_comptime_import_alias_environment.c
)
target_include_directories(zr_vm_comptime_import_alias_environment_test PRIVATE
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/src/zr_vm_parser
        ${CMAKE_SOURCE_DIR}/tests
)
zr_vm_link_parser_core_plus_library(zr_vm_comptime_import_alias_environment_test)
add_test(NAME macro_comptime_import_alias_environment
        COMMAND $<TARGET_FILE:zr_vm_comptime_import_alias_environment_test>)
set_tests_properties(macro_comptime_import_alias_environment PROPERTIES TIMEOUT 30)
