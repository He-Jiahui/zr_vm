#include "zr_vm_language_server_stdio_internal.h"

void add_advanced_editor_capabilities(cJSON *capabilities) {
    cJSON *codeActionProvider;
    cJSON *codeActionKinds;
    cJSON *onTypeFormattingProvider;
    cJSON *onTypeMoreTriggers;
    cJSON *documentLinkProvider;
    cJSON *codeLensProvider;
    cJSON *diagnosticProvider;

    if (capabilities == NULL) {
        return;
    }

    codeActionProvider = cJSON_CreateObject();
    codeActionKinds = cJSON_CreateArray();
    if (codeActionProvider != NULL && codeActionKinds != NULL) {
        cJSON_AddItemToArray(codeActionKinds, cJSON_CreateString(ZR_LSP_CODE_ACTION_KIND_QUICK_FIX));
        cJSON_AddItemToArray(codeActionKinds, cJSON_CreateString(ZR_LSP_CODE_ACTION_KIND_SOURCE_ORGANIZE_IMPORTS));
        cJSON_AddItemToArray(codeActionKinds, cJSON_CreateString(ZR_LSP_CODE_ACTION_KIND_SOURCE_REMOVE_UNUSED));
        cJSON_AddItemToObject(codeActionProvider, ZR_LSP_FIELD_CODE_ACTION_KINDS, codeActionKinds);
        cJSON_AddBoolToObject(codeActionProvider, ZR_LSP_FIELD_RESOLVE_PROVIDER, 1);
        cJSON_AddItemToObject(capabilities, ZR_LSP_FIELD_CODE_ACTION_PROVIDER, codeActionProvider);
    } else {
        cJSON_Delete(codeActionProvider);
        cJSON_Delete(codeActionKinds);
    }

    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_DOCUMENT_FORMATTING_PROVIDER, 1);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_DOCUMENT_RANGE_FORMATTING_PROVIDER, 1);
    onTypeFormattingProvider = cJSON_CreateObject();
    onTypeMoreTriggers = cJSON_CreateArray();
    if (onTypeFormattingProvider != NULL && onTypeMoreTriggers != NULL) {
        cJSON_AddStringToObject(onTypeFormattingProvider, ZR_LSP_FIELD_FIRST_TRIGGER_CHARACTER, "}");
        cJSON_AddItemToArray(onTypeMoreTriggers, cJSON_CreateString(";"));
        cJSON_AddItemToObject(onTypeFormattingProvider,
                              ZR_LSP_FIELD_MORE_TRIGGER_CHARACTER,
                              onTypeMoreTriggers);
        cJSON_AddItemToObject(capabilities,
                              ZR_LSP_FIELD_DOCUMENT_ON_TYPE_FORMATTING_PROVIDER,
                              onTypeFormattingProvider);
    } else {
        cJSON_Delete(onTypeFormattingProvider);
        cJSON_Delete(onTypeMoreTriggers);
    }

    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_FOLDING_RANGE_PROVIDER, 1);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_SELECTION_RANGE_PROVIDER, 1);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_LINKED_EDITING_RANGE_PROVIDER, 1);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_MONIKER_PROVIDER, 1);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_INLINE_VALUE_PROVIDER, 1);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_INLINE_COMPLETION_PROVIDER, 1);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_COLOR_PROVIDER, 1);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_DECLARATION_PROVIDER, 1);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_TYPE_DEFINITION_PROVIDER, 1);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_IMPLEMENTATION_PROVIDER, 1);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_CALL_HIERARCHY_PROVIDER, 1);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_TYPE_HIERARCHY_PROVIDER, 1);

    documentLinkProvider = cJSON_CreateObject();
    if (documentLinkProvider != NULL) {
        cJSON_AddBoolToObject(documentLinkProvider, ZR_LSP_FIELD_RESOLVE_PROVIDER, 1);
        cJSON_AddItemToObject(capabilities, ZR_LSP_FIELD_DOCUMENT_LINK_PROVIDER, documentLinkProvider);
    }

    codeLensProvider = cJSON_CreateObject();
    if (codeLensProvider != NULL) {
        cJSON_AddBoolToObject(codeLensProvider, ZR_LSP_FIELD_RESOLVE_PROVIDER, 1);
        cJSON_AddItemToObject(capabilities, ZR_LSP_FIELD_CODE_LENS_PROVIDER, codeLensProvider);
    }

    diagnosticProvider = cJSON_CreateObject();
    if (diagnosticProvider != NULL) {
        cJSON_AddBoolToObject(diagnosticProvider, ZR_LSP_FIELD_INTER_FILE_DEPENDENCIES, 1);
        cJSON_AddBoolToObject(diagnosticProvider, ZR_LSP_FIELD_WORKSPACE_DIAGNOSTICS, 1);
        cJSON_AddItemToObject(capabilities, ZR_LSP_FIELD_DIAGNOSTIC_PROVIDER, diagnosticProvider);
    }
}
