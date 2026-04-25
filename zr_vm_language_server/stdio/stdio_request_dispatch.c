#include "zr_vm_language_server_stdio_internal.h"

int dispatch_request_method(SZrStdioServer *server,
                            const char *method,
                            const cJSON *params,
                            cJSON **outResult) {
    if (server == ZR_NULL || method == NULL || outResult == NULL) {
        return 0;
    }

    *outResult = NULL;
    if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_COMPLETION) == 0) {
        *outResult = handle_completion_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_COMPLETION_ITEM_RESOLVE) == 0) {
        *outResult = handle_completion_item_resolve_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_HOVER) == 0) {
        *outResult = handle_hover_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_ZR_RICH_HOVER) == 0) {
        *outResult = handle_rich_hover_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_SIGNATURE_HELP) == 0) {
        *outResult = handle_signature_help_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_INLAY_HINT) == 0) {
        *outResult = handle_inlay_hint_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_INLAY_HINT_RESOLVE) == 0) {
        *outResult = handle_inlay_hint_resolve_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_DEFINITION) == 0) {
        *outResult = handle_definition_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_DECLARATION) == 0) {
        *outResult = handle_declaration_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_TYPE_DEFINITION) == 0) {
        *outResult = handle_type_definition_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_IMPLEMENTATION) == 0) {
        *outResult = handle_implementation_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_REFERENCES) == 0) {
        *outResult = handle_references_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_FORMATTING) == 0) {
        *outResult = handle_formatting_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_RANGE_FORMATTING) == 0) {
        *outResult = handle_range_formatting_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_RANGES_FORMATTING) == 0) {
        *outResult = handle_ranges_formatting_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_ON_TYPE_FORMATTING) == 0) {
        *outResult = handle_on_type_formatting_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_WILL_SAVE_WAIT_UNTIL) == 0) {
        *outResult = handle_formatting_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_CODE_ACTION) == 0) {
        *outResult = handle_code_action_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_CODE_ACTION_RESOLVE) == 0) {
        *outResult = handle_code_action_resolve_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_FOLDING_RANGE) == 0) {
        *outResult = handle_folding_range_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_SELECTION_RANGE) == 0) {
        *outResult = handle_selection_range_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_LINKED_EDITING_RANGE) == 0) {
        *outResult = handle_linked_editing_range_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_MONIKER) == 0) {
        *outResult = handle_moniker_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_INLINE_VALUE) == 0) {
        *outResult = handle_inline_value_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_INLINE_COMPLETION) == 0) {
        *outResult = handle_inline_completion_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_DOCUMENT_COLOR) == 0) {
        *outResult = handle_document_color_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_COLOR_PRESENTATION) == 0) {
        *outResult = handle_color_presentation_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_DOCUMENT_LINK) == 0) {
        *outResult = handle_document_link_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_DOCUMENT_LINK_RESOLVE) == 0) {
        *outResult = handle_document_link_resolve_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_CODE_LENS) == 0) {
        *outResult = handle_code_lens_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_CODE_LENS_RESOLVE) == 0) {
        *outResult = handle_code_lens_resolve_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_PREPARE_CALL_HIERARCHY) == 0) {
        *outResult = handle_prepare_call_hierarchy_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_CALL_HIERARCHY_INCOMING_CALLS) == 0) {
        *outResult = handle_call_hierarchy_incoming_calls_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_CALL_HIERARCHY_OUTGOING_CALLS) == 0) {
        *outResult = handle_call_hierarchy_outgoing_calls_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_PREPARE_TYPE_HIERARCHY) == 0) {
        *outResult = handle_prepare_type_hierarchy_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TYPE_HIERARCHY_SUPERTYPES) == 0) {
        *outResult = handle_type_hierarchy_supertypes_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TYPE_HIERARCHY_SUBTYPES) == 0) {
        *outResult = handle_type_hierarchy_subtypes_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_DIAGNOSTIC) == 0) {
        *outResult = handle_text_document_diagnostic_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_WORKSPACE_DIAGNOSTIC) == 0) {
        *outResult = handle_workspace_diagnostic_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_DOCUMENT_SYMBOL) == 0) {
        *outResult = handle_document_symbols_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_WORKSPACE_SYMBOL) == 0) {
        *outResult = handle_workspace_symbols_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_WORKSPACE_SYMBOL_RESOLVE) == 0) {
        *outResult = handle_workspace_symbol_resolve_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_WORKSPACE_EXECUTE_COMMAND) == 0) {
        *outResult = handle_execute_command_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_WORKSPACE_WILL_CREATE_FILES) == 0) {
        *outResult = handle_will_create_files_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_WORKSPACE_WILL_RENAME_FILES) == 0) {
        *outResult = handle_will_rename_files_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_WORKSPACE_WILL_DELETE_FILES) == 0) {
        *outResult = handle_will_delete_files_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_DOCUMENT_HIGHLIGHT) == 0) {
        *outResult = handle_document_highlights_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_SEMANTIC_TOKENS_FULL) == 0) {
        *outResult = handle_semantic_tokens_full_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_SEMANTIC_TOKENS_FULL_DELTA) == 0) {
        *outResult = handle_semantic_tokens_full_delta_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_SEMANTIC_TOKENS_RANGE) == 0) {
        *outResult = handle_semantic_tokens_range_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_PREPARE_RENAME) == 0) {
        *outResult = handle_prepare_rename_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_RENAME) == 0) {
        *outResult = handle_rename_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_ZR_NATIVE_DECLARATION_DOCUMENT) == 0) {
        *outResult = handle_native_declaration_document_request(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_ZR_PROJECT_MODULES) == 0) {
        *outResult = handle_project_modules_request(server, params);
    } else {
        return 0;
    }

    return 1;
}
