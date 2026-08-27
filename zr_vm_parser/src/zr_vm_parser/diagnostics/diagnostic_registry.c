#include "zr_vm_parser/diagnostic_registry.h"

#include <string.h>

#define ZR_DIAGNOSTIC_HELP_URI \
    "https://github.com/He-Jiahui/zr_vm/blob/main/docs/plans/lsp/02-diagnostics-and-errors.md"

#define ZR_DIAGNOSTIC_DESCRIPTOR(idValue, codeValue, severityValue, categoryValue) \
    { \
        (idValue), \
        (codeValue), \
        "diagnostic." codeValue ".title", \
        "diagnostic." codeValue ".message", \
        (severityValue), \
        ZR_DIAGNOSTIC_HELP_URI, \
        (categoryValue) \
    }

static const SZrDiagnosticDescriptor g_diagnostic_descriptors[] = {
    ZR_DIAGNOSTIC_DESCRIPTOR(1001, "missing_expression_after_assignment",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SYNTAX),
    ZR_DIAGNOSTIC_DESCRIPTOR(1002, "missing_right_operand",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SYNTAX),
    ZR_DIAGNOSTIC_DESCRIPTOR(1003, "missing_condition",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SYNTAX),
    ZR_DIAGNOSTIC_DESCRIPTOR(1004, "missing_declaration_body_open",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SYNTAX),
    ZR_DIAGNOSTIC_DESCRIPTOR(1005, "missing_declaration_body_close",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SYNTAX),
    ZR_DIAGNOSTIC_DESCRIPTOR(1006, "missing_statement_body_open",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SYNTAX),
    ZR_DIAGNOSTIC_DESCRIPTOR(1007, "missing_block_close",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SYNTAX),
    ZR_DIAGNOSTIC_DESCRIPTOR(1008, "missing_catch_pattern_close",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SYNTAX),
    ZR_DIAGNOSTIC_DESCRIPTOR(1009, "missing_using_resource_close",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SYNTAX),
    ZR_DIAGNOSTIC_DESCRIPTOR(1010, "missing_for_header_close",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SYNTAX),
    ZR_DIAGNOSTIC_DESCRIPTOR(1011, "missing_for_header_separator",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SYNTAX),
    ZR_DIAGNOSTIC_DESCRIPTOR(1012, "missing_foreach_header_close",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SYNTAX),
    ZR_DIAGNOSTIC_DESCRIPTOR(1013, "missing_foreach_in_keyword",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SYNTAX),
    ZR_DIAGNOSTIC_DESCRIPTOR(1014, "missing_switch_case_header_close",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SYNTAX),
    ZR_DIAGNOSTIC_DESCRIPTOR(1015, "missing_switch_body_close",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SYNTAX),
    ZR_DIAGNOSTIC_DESCRIPTOR(1016, "missing_extern_spec_close",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SYNTAX),
    ZR_DIAGNOSTIC_DESCRIPTOR(1017, "missing_test_name_close",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SYNTAX),
    ZR_DIAGNOSTIC_DESCRIPTOR(1018, "missing_condition_close",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SYNTAX),
    ZR_DIAGNOSTIC_DESCRIPTOR(1019, "missing_member_name",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SYNTAX),
    ZR_DIAGNOSTIC_DESCRIPTOR(1020, "missing_index_close",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SYNTAX),
    ZR_DIAGNOSTIC_DESCRIPTOR(1021, "missing_call_close",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SYNTAX),
    ZR_DIAGNOSTIC_DESCRIPTOR(1022, "missing_parameter_list_close",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SYNTAX),
    ZR_DIAGNOSTIC_DESCRIPTOR(1023, "missing_group_close",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SYNTAX),
    ZR_DIAGNOSTIC_DESCRIPTOR(1024, "array_element_assignment",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SYNTAX),
    ZR_DIAGNOSTIC_DESCRIPTOR(1025, "missing_array_close",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SYNTAX),
    ZR_DIAGNOSTIC_DESCRIPTOR(1026, "missing_array_element_separator",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SYNTAX),
    ZR_DIAGNOSTIC_DESCRIPTOR(1027, "missing_object_close",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SYNTAX),
    ZR_DIAGNOSTIC_DESCRIPTOR(1028, "missing_object_computed_key_close",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SYNTAX),
    ZR_DIAGNOSTIC_DESCRIPTOR(1029, "missing_object_property_colon",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SYNTAX),
    ZR_DIAGNOSTIC_DESCRIPTOR(1030, "missing_object_property_separator",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SYNTAX),
    ZR_DIAGNOSTIC_DESCRIPTOR(1031, "missing_conditional_consequent",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SYNTAX),
    ZR_DIAGNOSTIC_DESCRIPTOR(1032, "missing_conditional_colon",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SYNTAX),
    ZR_DIAGNOSTIC_DESCRIPTOR(1033, "missing_conditional_alternate",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SYNTAX),
    ZR_DIAGNOSTIC_DESCRIPTOR(1034, "missing_statement_semicolon",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SYNTAX),

    ZR_DIAGNOSTIC_DESCRIPTOR(2001, "using_binder_invalid",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SEMANTIC),
    ZR_DIAGNOSTIC_DESCRIPTOR(2002, "import_path_not_constant",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SEMANTIC),
    ZR_DIAGNOSTIC_DESCRIPTOR(2003, "pattern_shape_mismatch",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_TYPE),
    ZR_DIAGNOSTIC_DESCRIPTOR(2004, "pattern_unknown_field",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_TYPE),
    ZR_DIAGNOSTIC_DESCRIPTOR(2005, "pattern_arity_mismatch",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_TYPE),
    ZR_DIAGNOSTIC_DESCRIPTOR(2006, "pattern_variant_mismatch",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_TYPE),
    ZR_DIAGNOSTIC_DESCRIPTOR(2007, "legacy_ownership_type_syntax",
                             ZR_STRUCTURED_DIAGNOSTIC_WARNING, ZR_LINT_CATEGORY_STYLE),
    ZR_DIAGNOSTIC_DESCRIPTOR(2008, "ownership_mismatch",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_TYPE),
    ZR_DIAGNOSTIC_DESCRIPTOR(2009, "array_index_type_mismatch",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_TYPE),
    ZR_DIAGNOSTIC_DESCRIPTOR(2010, "duplicate_type",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SEMANTIC),
    ZR_DIAGNOSTIC_DESCRIPTOR(2011, "type_mismatch",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_TYPE),
    ZR_DIAGNOSTIC_DESCRIPTOR(2012, "const_assignment",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SEMANTIC),
    ZR_DIAGNOSTIC_DESCRIPTOR(2013, "invalid_variance",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_TYPE),
    ZR_DIAGNOSTIC_DESCRIPTOR(2014, "const_interface_mismatch",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_TYPE),
    ZR_DIAGNOSTIC_DESCRIPTOR(2015, "unresolved_reference",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SEMANTIC),
    ZR_DIAGNOSTIC_DESCRIPTOR(2016, "member_not_found",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_TYPE),
    ZR_DIAGNOSTIC_DESCRIPTOR(2017, "initializer_requires_annotation",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_TYPE),
    ZR_DIAGNOSTIC_DESCRIPTOR(2018, "return_type_not_provable",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_TYPE),
    ZR_DIAGNOSTIC_DESCRIPTOR(2019, "invalid_decorator",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_SEMANTIC),

    ZR_DIAGNOSTIC_DESCRIPTOR(3001, "unreachable_code",
                             ZR_STRUCTURED_DIAGNOSTIC_WARNING, ZR_LINT_CATEGORY_FLOW),
    ZR_DIAGNOSTIC_DESCRIPTOR(3002, "uninitialized_read",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_FLOW),
    ZR_DIAGNOSTIC_DESCRIPTOR(3003, "possibly_uninitialized_read",
                             ZR_STRUCTURED_DIAGNOSTIC_WARNING, ZR_LINT_CATEGORY_FLOW),
    ZR_DIAGNOSTIC_DESCRIPTOR(3004, "numeric_overflow",
                             ZR_STRUCTURED_DIAGNOSTIC_WARNING, ZR_LINT_CATEGORY_FLOW),
    ZR_DIAGNOSTIC_DESCRIPTOR(3005, "array_index_out_of_bounds",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_FLOW),
    ZR_DIAGNOSTIC_DESCRIPTOR(3006, "array_index_may_be_out_of_bounds",
                             ZR_STRUCTURED_DIAGNOSTIC_WARNING, ZR_LINT_CATEGORY_FLOW),

    ZR_DIAGNOSTIC_DESCRIPTOR(4001, "use_after_move",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_OWNERSHIP),
    ZR_DIAGNOSTIC_DESCRIPTOR(4002, "borrow_escape",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_OWNERSHIP),
    ZR_DIAGNOSTIC_DESCRIPTOR(4003, "loan_escape",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_OWNERSHIP),
    ZR_DIAGNOSTIC_DESCRIPTOR(4004, "weak_value_requires_wake",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_OWNERSHIP),
    ZR_DIAGNOSTIC_DESCRIPTOR(4005, "owner_to_plain_escape",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_OWNERSHIP),
    ZR_DIAGNOSTIC_DESCRIPTOR(4006, "resource_shared_strong_cycle",
                             ZR_STRUCTURED_DIAGNOSTIC_WARNING, ZR_LINT_CATEGORY_OWNERSHIP),
    ZR_DIAGNOSTIC_DESCRIPTOR(4007, "removed_ownership_member_syntax",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_OWNERSHIP),
    ZR_DIAGNOSTIC_DESCRIPTOR(4008, "reserved_ownership_intrinsic_name",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_OWNERSHIP),
    ZR_DIAGNOSTIC_DESCRIPTOR(4009, "ownership_intrinsic_call_required",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_OWNERSHIP),
    ZR_DIAGNOSTIC_DESCRIPTOR(4010, "ownership_intrinsic_arity_mismatch",
                             ZR_STRUCTURED_DIAGNOSTIC_ERROR, ZR_LINT_CATEGORY_OWNERSHIP),
};

TZrSize ZrParser_DiagnosticRegistry_Count(void) {
    return sizeof(g_diagnostic_descriptors) / sizeof(g_diagnostic_descriptors[0]);
}

const SZrDiagnosticDescriptor *ZrParser_DiagnosticRegistry_DescriptorAt(TZrSize index) {
    return index < ZrParser_DiagnosticRegistry_Count()
                   ? &g_diagnostic_descriptors[index]
                   : ZR_NULL;
}

const SZrDiagnosticDescriptor *ZrParser_DiagnosticRegistry_FindByCode(const TZrChar *code) {
    TZrSize index;

    if (code == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0; index < ZrParser_DiagnosticRegistry_Count(); index++) {
        const SZrDiagnosticDescriptor *descriptor = &g_diagnostic_descriptors[index];
        if (strcmp(descriptor->code, code) == 0) {
            return descriptor;
        }
    }
    return ZR_NULL;
}

const SZrDiagnosticDescriptor *ZrParser_DiagnosticRegistry_FindById(TZrUInt32 id) {
    TZrSize index;

    if (id == 0) {
        return ZR_NULL;
    }
    for (index = 0; index < ZrParser_DiagnosticRegistry_Count(); index++) {
        const SZrDiagnosticDescriptor *descriptor = &g_diagnostic_descriptors[index];
        if (descriptor->id == id) {
            return descriptor;
        }
    }
    return ZR_NULL;
}

TZrUInt32 ZrParser_DiagnosticRegistry_DescriptorIdForCode(const TZrChar *code) {
    const SZrDiagnosticDescriptor *descriptor =
            ZrParser_DiagnosticRegistry_FindByCode(code);
    return descriptor != ZR_NULL ? descriptor->id : 0;
}
