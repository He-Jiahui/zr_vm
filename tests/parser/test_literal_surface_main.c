#include <stdio.h>

#include "unity.h"
#include "zr_test_log_macros.h"

extern void test_struct_prototype_compilation(void);
extern void test_class_prototype_compilation(void);
extern void test_prototype_creation_functions(void);
extern void test_prototype_inheritance(void);
extern void test_prototype_module_export(void);
extern void test_struct_field_offsets(void);
extern void test_prototype_inheritance_loading(void);
extern void test_struct_value_copy_clones_nested_storage(void);
extern void test_const_local_variable_declaration(void);
extern void test_const_local_variable_reassignment_error(void);
extern void test_const_class_field_in_constructor(void);
extern void test_const_class_field_in_method_error(void);
extern void test_const_struct_field_in_constructor(void);
extern void test_const_struct_field_in_method_error(void);
extern void test_const_interface_field_declaration(void);
extern void test_const_interface_implementation_match(void);
extern void test_const_function_parameter(void);
extern void test_const_parameter_modification_error(void);
extern void test_const_static_field_declaration(void);
extern void test_const_static_field_reassignment_error(void);
extern void test_const_interface_implementation_mismatch_error(void);
extern void test_const_field_multiple_assignment_in_constructor(void);
extern void test_const_local_variable_uninitialized_error(void);
extern void test_const_field_declared_after_constructor_initializes_successfully(void);
extern void test_const_field_missing_constructor_initialization_error(void);
extern void test_const_field_compound_assignment_in_constructor_error(void);
extern void test_const_field_initialized_once_per_branch_success(void);
extern void test_const_field_missing_branch_initialization_error(void);
extern void test_const_field_early_return_before_initialization_error(void);
extern void test_const_struct_field_initialized_once_per_branch_success(void);
extern void test_const_field_initialized_once_across_else_if_chain_success(void);
extern void test_const_field_switch_paths_initialized_once_success(void);
extern void test_const_field_switch_missing_default_initialization_error(void);
extern void test_const_field_ternary_branches_initialized_once_success(void);
extern void test_const_field_ternary_missing_branch_initialization_error(void);
extern void test_char_literals_parsing(void);
extern void test_char_literals_compilation(void);
extern void test_type_cast_basic_parsing(void);
extern void test_type_cast_basic_compilation(void);
extern void test_type_cast_struct_parsing(void);
extern void test_type_cast_class_parsing(void);
extern void test_relational_less_than_does_not_emit_speculative_type_cast_diagnostic(void);
extern void test_reference_core_semantics_manifest_inventory(void);
extern void test_reference_full_stack_manifest_inventory(void);
extern void test_reference_full_stack_priority_cases_are_present(void);
extern void test_reference_full_stack_master_matrix_document_exists(void);
extern void test_reference_core_semantics_matrix_document_exists(void);
extern void test_execution_order_doc_mentions_current_suites_and_tiers(void);
extern void test_reference_unclosed_string_fixture_surfaces_invalid_literal_node(void);
extern void test_reference_invalid_hex_escape_fixture_surfaces_invalid_literal_node(void);
extern void test_reference_multiline_literal_fixture_surfaces_invalid_literal_node(void);
extern void test_reference_invalid_char_width_fixture_surfaces_invalid_char_literal_node(void);
extern void test_reference_const_reassign_fixture_is_rejected(void);
extern void test_reference_constructor_const_control_flow_fixture_matrix(void);
extern void test_reference_expressions_fixture_matrix(void);
extern void test_reference_types_casts_const_fixture_matrix(void);
extern void test_reference_construct_target_misuse_fixture_matrix(void);

int main(void) {
    printf("\n");
    ZR_TEST_MODULE_DIVIDER();
    printf("Prototype + const + literal/cast surface tests\n");
    ZR_TEST_MODULE_DIVIDER();
    printf("\n");

    UNITY_BEGIN();

    ZR_TEST_MODULE_DIVIDER();
    RUN_TEST(test_struct_prototype_compilation);
    ZR_TEST_DIVIDER();

    RUN_TEST(test_class_prototype_compilation);
    ZR_TEST_DIVIDER();

    RUN_TEST(test_prototype_creation_functions);
    ZR_TEST_DIVIDER();

    RUN_TEST(test_prototype_inheritance);
    ZR_TEST_DIVIDER();

    RUN_TEST(test_prototype_module_export);
    ZR_TEST_DIVIDER();

    RUN_TEST(test_struct_field_offsets);
    ZR_TEST_DIVIDER();

    RUN_TEST(test_prototype_inheritance_loading);
    ZR_TEST_DIVIDER();

    RUN_TEST(test_struct_value_copy_clones_nested_storage);
    ZR_TEST_MODULE_DIVIDER();

    printf("Const keyword tests\n");
    ZR_TEST_MODULE_DIVIDER();

    RUN_TEST(test_const_local_variable_declaration);
    ZR_TEST_DIVIDER();

    RUN_TEST(test_const_local_variable_reassignment_error);
    ZR_TEST_DIVIDER();

    RUN_TEST(test_const_class_field_in_constructor);
    ZR_TEST_DIVIDER();

    RUN_TEST(test_const_class_field_in_method_error);
    ZR_TEST_DIVIDER();

    RUN_TEST(test_const_struct_field_in_constructor);
    ZR_TEST_DIVIDER();

    RUN_TEST(test_const_struct_field_in_method_error);
    ZR_TEST_DIVIDER();

    RUN_TEST(test_const_interface_field_declaration);
    ZR_TEST_DIVIDER();

    RUN_TEST(test_const_interface_implementation_match);
    ZR_TEST_DIVIDER();

    RUN_TEST(test_const_function_parameter);
    ZR_TEST_DIVIDER();

    RUN_TEST(test_const_parameter_modification_error);
    ZR_TEST_DIVIDER();

    RUN_TEST(test_const_static_field_declaration);
    ZR_TEST_DIVIDER();

    RUN_TEST(test_const_static_field_reassignment_error);
    ZR_TEST_DIVIDER();

    RUN_TEST(test_const_interface_implementation_mismatch_error);
    ZR_TEST_DIVIDER();

    RUN_TEST(test_const_field_multiple_assignment_in_constructor);
    ZR_TEST_DIVIDER();

    RUN_TEST(test_const_local_variable_uninitialized_error);
    ZR_TEST_DIVIDER();

    RUN_TEST(test_const_field_declared_after_constructor_initializes_successfully);
    ZR_TEST_DIVIDER();

    RUN_TEST(test_const_field_missing_constructor_initialization_error);
    ZR_TEST_DIVIDER();

    RUN_TEST(test_const_field_compound_assignment_in_constructor_error);
    ZR_TEST_DIVIDER();

    RUN_TEST(test_const_field_initialized_once_per_branch_success);
    ZR_TEST_DIVIDER();

    RUN_TEST(test_const_field_missing_branch_initialization_error);
    ZR_TEST_DIVIDER();

    RUN_TEST(test_const_field_early_return_before_initialization_error);
    ZR_TEST_DIVIDER();

    RUN_TEST(test_const_struct_field_initialized_once_per_branch_success);
    ZR_TEST_DIVIDER();

    RUN_TEST(test_const_field_initialized_once_across_else_if_chain_success);
    ZR_TEST_DIVIDER();

    RUN_TEST(test_const_field_switch_paths_initialized_once_success);
    ZR_TEST_DIVIDER();

    RUN_TEST(test_const_field_switch_missing_default_initialization_error);
    ZR_TEST_DIVIDER();

    RUN_TEST(test_const_field_ternary_branches_initialized_once_success);
    ZR_TEST_DIVIDER();

    RUN_TEST(test_const_field_ternary_missing_branch_initialization_error);
    ZR_TEST_MODULE_DIVIDER();

    printf("Char literals and type cast tests\n");
    ZR_TEST_MODULE_DIVIDER();

    RUN_TEST(test_char_literals_parsing);
    RUN_TEST(test_char_literals_compilation);

    ZR_TEST_MODULE_DIVIDER();

    RUN_TEST(test_type_cast_basic_parsing);
    RUN_TEST(test_type_cast_basic_compilation);
    RUN_TEST(test_type_cast_struct_parsing);
    RUN_TEST(test_type_cast_class_parsing);
    RUN_TEST(test_relational_less_than_does_not_emit_speculative_type_cast_diagnostic);
    RUN_TEST(test_reference_core_semantics_manifest_inventory);
    RUN_TEST(test_reference_full_stack_manifest_inventory);
    RUN_TEST(test_reference_full_stack_priority_cases_are_present);
    RUN_TEST(test_reference_full_stack_master_matrix_document_exists);
    RUN_TEST(test_reference_core_semantics_matrix_document_exists);
    RUN_TEST(test_execution_order_doc_mentions_current_suites_and_tiers);
    RUN_TEST(test_reference_unclosed_string_fixture_surfaces_invalid_literal_node);
    RUN_TEST(test_reference_invalid_hex_escape_fixture_surfaces_invalid_literal_node);
    RUN_TEST(test_reference_multiline_literal_fixture_surfaces_invalid_literal_node);
    RUN_TEST(test_reference_invalid_char_width_fixture_surfaces_invalid_char_literal_node);
    RUN_TEST(test_reference_const_reassign_fixture_is_rejected);
    RUN_TEST(test_reference_constructor_const_control_flow_fixture_matrix);
    RUN_TEST(test_reference_expressions_fixture_matrix);
    RUN_TEST(test_reference_types_casts_const_fixture_matrix);
    RUN_TEST(test_reference_construct_target_misuse_fixture_matrix);

    ZR_TEST_MODULE_DIVIDER();

    return UNITY_END();
}
