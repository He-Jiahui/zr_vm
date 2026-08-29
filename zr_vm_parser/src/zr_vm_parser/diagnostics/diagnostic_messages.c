#include "zr_vm_parser/diagnostic_messages.h"

#include <string.h>

#define ZR_DIAGNOSTIC_MESSAGE_PAIR(codeValue, titleValue, messageValue) \
    {"diagnostic." codeValue ".title", (titleValue), ZR_NULL}, \
    {"diagnostic." codeValue ".message", (messageValue), ZR_NULL}

static const SZrDiagnosticMessage g_diagnostic_messages[] = {
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "missing_expression_after_assignment",
            "Missing assignment expression",
            "Missing expression after '='"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "missing_right_operand",
            "Missing right operand",
            "Missing expression after '%s'"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "missing_condition",
            "Missing condition",
            "Missing condition inside '%s'"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "missing_declaration_body_open",
            "Missing declaration body opener",
            "Missing '{' to start %s body"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "missing_declaration_body_close",
            "Missing declaration body closer",
            "Missing '}' to close %s body"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "missing_statement_body_open",
            "Missing statement body opener",
            "Missing '{' to start %s body"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "missing_block_close",
            "Missing block closer",
            "Missing '}' to close block"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "missing_catch_pattern_close",
            "Missing catch pattern closer",
            "Missing ')' to close catch pattern"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "missing_using_resource_close",
            "Missing using resource closer",
            "Missing ')' to close using resource"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "missing_for_header_close",
            "Missing for header closer",
            "Missing ')' to close for header"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "missing_for_header_separator",
            "Missing for header separator",
            "Missing ';' in for header"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "missing_foreach_header_close",
            "Missing foreach header closer",
            "Missing ')' to close foreach header"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "missing_foreach_in_keyword",
            "Missing foreach in keyword",
            "Missing 'in' in foreach header"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "missing_switch_case_header_close",
            "Missing switch case header closer",
            "Missing ':' after switch case"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "missing_switch_body_close",
            "Missing switch body closer",
            "Missing '}' to close switch body"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "missing_extern_spec_close",
            "Missing extern specification closer",
            "Missing ')' to close extern specification"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "missing_test_name_close",
            "Missing test name closer",
            "Missing ')' to close test name"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "missing_condition_close",
            "Missing condition closer",
            "Missing ')' to close %s condition"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "missing_member_name",
            "Missing member name",
            "Missing member name after '.'"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "missing_index_close",
            "Missing index expression closer",
            "Missing ']' to close index expression"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "missing_call_close",
            "Missing call closer",
            "Missing ')' to close call"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "missing_parameter_list_close",
            "Missing parameter list closer",
            "Missing ')' to close parameter list"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "missing_group_close",
            "Missing group closer",
            "Missing ')' to close grouped expression"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "array_element_assignment",
            "Invalid array element assignment",
            "Array elements cannot be assigned in this expression"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "missing_array_close",
            "Missing array closer",
            "Missing ']' to close array literal"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "missing_array_element_separator",
            "Missing array element separator",
            "Missing ',' between array elements"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "missing_object_close",
            "Missing object closer",
            "Missing '}' to close object literal"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "missing_object_computed_key_close",
            "Missing computed key closer",
            "Missing ']' to close computed property key"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "missing_object_property_colon",
            "Missing object property colon",
            "Missing ':' after object property name"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "missing_object_property_separator",
            "Missing object property separator",
            "Missing ',' between object properties"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "missing_conditional_consequent",
            "Missing conditional consequent",
            "Missing expression after '?'"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "missing_conditional_colon",
            "Missing conditional colon",
            "Missing ':' in conditional expression"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "missing_conditional_alternate",
            "Missing conditional alternate",
            "Missing expression after ':'"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "missing_statement_semicolon",
            "Missing statement semicolon",
            "Missing ';' after %s statement"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "using_binder_invalid",
            "Invalid using binder",
            "Using resource must bind a value"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "import_path_not_constant",
            "Import path is not constant",
            "%s path must be a compile-time string constant"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "pattern_shape_mismatch",
            "Pattern shape mismatch",
            "Pattern shape does not match the value"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "pattern_unknown_field",
            "Unknown pattern field",
            "Unknown field '%s' in pattern"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "pattern_arity_mismatch",
            "Pattern arity mismatch",
            "Pattern expects %zu fields but found %zu"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "pattern_variant_mismatch",
            "Pattern variant mismatch",
            "Variant '%s' belongs to '%s', not '%s'"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "legacy_ownership_type_syntax",
            "Legacy ownership type syntax",
            "Legacy ownership qualifier '%s' is deprecated"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "ownership_mismatch",
            "Ownership mismatch",
            "Expected ownership type '%s' but found '%s'"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "array_index_type_mismatch",
            "Array index type mismatch",
            "Array index must be an integer"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "duplicate_type",
            "Duplicate type definition",
            "Type '%s' is already defined"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "type_mismatch",
            "Type mismatch",
            "Expected '%s' but found '%s'"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "const_assignment",
            "Immutable assignment",
            "Cannot assign to an immutable target in this context"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "invalid_variance",
            "Invalid variance",
            "Generic parameter is used in an incompatible variance position"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "const_interface_mismatch",
            "Interface const field mismatch",
            "Interface const field must remain const in the implementing class"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "unresolved_reference",
            "Unresolved reference",
            "Reference could not be resolved to a declaration"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "member_not_found",
            "Member not found",
            "Member could not be resolved on the receiver type"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "initializer_requires_annotation",
            "Initializer requires annotation",
            "Variable declaration requires a type annotation or initializer"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "return_type_not_provable",
            "Return type not provable",
            "Callable return expressions do not establish one exact common type"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "invalid_decorator",
            "Invalid decorator",
            "Decorator is not valid for this declaration"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "cannot_infer_exact_type",
            "Cannot infer exact type",
            "Compiler could not prove one exact type"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "unreachable_code",
            "Unreachable code",
            "Unreachable code"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "uninitialized_read",
            "Uninitialized read",
            "Variable is read before it is initialized"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "possibly_uninitialized_read",
            "Possibly uninitialized read",
            "Variable may be uninitialized on some control-flow paths"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "numeric_overflow",
            "Numeric overflow",
            "Numeric expression may overflow"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "array_index_out_of_bounds",
            "Array index out of bounds",
            "Array index is out of bounds"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "array_index_may_be_out_of_bounds",
            "Array index may be out of bounds",
            "Array index may be out of bounds"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "use_after_move",
            "Use after move",
            "Value is used after it was moved"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "borrow_escape",
            "Borrow escape",
            "Borrowed value escapes its valid region"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "loan_escape",
            "Loan escape",
            "Loaned value escapes its valid region"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "weak_value_requires_wake",
            "Weak value requires wake",
            "Weak value must be woken before use"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "owner_to_plain_escape",
            "Owner to plain escape",
            "Owner value cannot escape into a plain reference"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "resource_shared_strong_cycle",
            "Resource shared strong cycle",
            "Shared resource fields form a strong ownership cycle"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "removed_ownership_member_syntax",
            "Removed ownership member syntax",
            "Ownership operations use reserved intrinsic calls"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "reserved_ownership_intrinsic_name",
            "Reserved ownership intrinsic name",
            "Ownership intrinsic name is reserved in lexical namespaces"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "ownership_intrinsic_call_required",
            "Ownership intrinsic call required",
            "Ownership intrinsic must be called with one positional argument"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "ownership_intrinsic_arity_mismatch",
            "Ownership intrinsic arity mismatch",
            "Ownership intrinsic requires exactly one positional argument"),
    ZR_DIAGNOSTIC_MESSAGE_PAIR(
            "nullable_ownership_intrinsic_operand",
            "Nullable ownership intrinsic operand",
            "Ownership transition requires a live owner"),
};

TZrSize ZrParser_DiagnosticMessages_Count(void) {
    return sizeof(g_diagnostic_messages) / sizeof(g_diagnostic_messages[0]);
}

const SZrDiagnosticMessage *ZrParser_DiagnosticMessages_MessageAt(TZrSize index) {
    return index < ZrParser_DiagnosticMessages_Count()
                   ? &g_diagnostic_messages[index]
                   : ZR_NULL;
}

const SZrDiagnosticMessage *ZrParser_DiagnosticMessages_Find(const TZrChar *key) {
    TZrSize index;

    if (key == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0; index < ZrParser_DiagnosticMessages_Count(); index++) {
        const SZrDiagnosticMessage *message = &g_diagnostic_messages[index];
        if (strcmp(message->key, key) == 0) {
            return message;
        }
    }
    return ZR_NULL;
}

const TZrChar *ZrParser_DiagnosticMessages_Resolve(EZrDiagnosticLocale locale,
                                                   const TZrChar *key) {
    const SZrDiagnosticMessage *message = ZrParser_DiagnosticMessages_Find(key);

    if (message == ZR_NULL) {
        return ZR_NULL;
    }
    if (locale == ZR_DIAGNOSTIC_LOCALE_CHINESE_SIMPLIFIED &&
        message->chineseSimplified != ZR_NULL &&
        message->chineseSimplified[0] != '\0') {
        return message->chineseSimplified;
    }
    return message->english;
}
