#include "zr_vm_language_server_stdio_internal.h"
#include "zr_vm_language_server/lsp_capability_registry.h"

static const char ZR_STDIO_FIELD_RANGES_SUPPORT[] = "rangesSupport";

static TZrBool optional_editor_capability_is_valid(const cJSON *capability) {
    const cJSON *dynamicRegistration = get_object_item(capability, "dynamicRegistration");

    return cJSON_IsObject(capability) &&
                   (dynamicRegistration == ZR_NULL || cJSON_IsBool(dynamicRegistration))
                   ? ZR_TRUE : ZR_FALSE;
}

static void negotiate_optional_editor_capabilities(SZrStdioServer *server,
                                                    const cJSON *params) {
    const cJSON *clientCapabilities = get_object_item(params, ZR_LSP_FIELD_CAPABILITIES);
    const cJSON *textDocument = get_object_item(clientCapabilities, ZR_LSP_FIELD_TEXT_DOCUMENT);
    const cJSON *inlineCompletion = get_object_item(textDocument, "inlineCompletion");
    const cJSON *rangeFormatting = get_object_item(textDocument, "rangeFormatting");

    server->supportsInlineCompletion = optional_editor_capability_is_valid(inlineCompletion);
    server->supportsRangesFormatting =
            optional_editor_capability_is_valid(rangeFormatting) &&
            cJSON_IsTrue(get_object_item(rangeFormatting, ZR_STDIO_FIELD_RANGES_SUPPORT))
                    ? ZR_TRUE : ZR_FALSE;
}

void add_advanced_editor_capabilities(SZrStdioServer *server,
                                      const cJSON *params,
                                      cJSON *capabilities) {
    cJSON *codeActionProvider;
    cJSON *codeActionKinds;
    cJSON *onTypeFormattingProvider;
    cJSON *onTypeMoreTriggers;
    cJSON *documentLinkProvider;
    cJSON *codeLensProvider;
    cJSON *diagnosticProvider;

    if (server == ZR_NULL || capabilities == NULL) {
        return;
    }
    negotiate_optional_editor_capabilities(server, params);

    codeActionProvider = cJSON_CreateObject();
    codeActionKinds = cJSON_CreateArray();
    if (codeActionProvider != NULL && codeActionKinds != NULL) {
        cJSON_AddItemToArray(codeActionKinds, cJSON_CreateString(ZR_LSP_CODE_ACTION_KIND_QUICK_FIX));
        cJSON_AddItemToArray(codeActionKinds, cJSON_CreateString(ZR_LSP_CODE_ACTION_KIND_SOURCE_ORGANIZE_IMPORTS));
        cJSON_AddItemToArray(codeActionKinds, cJSON_CreateString(ZR_LSP_CODE_ACTION_KIND_SOURCE_REMOVE_UNUSED));
        cJSON_AddItemToObject(codeActionProvider, ZR_LSP_FIELD_CODE_ACTION_KINDS, codeActionKinds);
        cJSON_AddBoolToObject(codeActionProvider, ZR_LSP_FIELD_RESOLVE_PROVIDER,
                              ZrLanguageServer_LspCapabilityRegistry_HasResolveForRuntime(
                                      ZR_LSP_FIELD_CODE_ACTION_PROVIDER, ZR_LSP_RUNTIME_NATIVE));
        cJSON_AddItemToObject(capabilities, ZR_LSP_FIELD_CODE_ACTION_PROVIDER, codeActionProvider);
    } else {
        cJSON_Delete(codeActionProvider);
        cJSON_Delete(codeActionKinds);
    }

    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_DOCUMENT_FORMATTING_PROVIDER, 1);
    if (server->supportsRangesFormatting) {
        cJSON *rangeFormattingProvider = cJSON_CreateObject();
        if (rangeFormattingProvider == NULL ||
            cJSON_AddBoolToObject(rangeFormattingProvider, ZR_STDIO_FIELD_RANGES_SUPPORT, 1) == NULL ||
            !cJSON_AddItemToObject(capabilities,
                                   ZR_LSP_FIELD_DOCUMENT_RANGE_FORMATTING_PROVIDER,
                                   rangeFormattingProvider)) {
            cJSON_Delete(rangeFormattingProvider);
            server->supportsRangesFormatting = ZR_FALSE;
        }
    }
    if (!server->supportsRangesFormatting) {
        cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_DOCUMENT_RANGE_FORMATTING_PROVIDER, 1);
    }
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
    if (server->supportsInlineCompletion &&
        cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_INLINE_COMPLETION_PROVIDER, 1) == NULL) {
        server->supportsInlineCompletion = ZR_FALSE;
    }
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_COLOR_PROVIDER, 1);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_IMPLEMENTATION_PROVIDER, 1);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_CALL_HIERARCHY_PROVIDER, 1);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_TYPE_HIERARCHY_PROVIDER, 1);

    documentLinkProvider = cJSON_CreateObject();
    if (documentLinkProvider != NULL) {
        cJSON_AddBoolToObject(documentLinkProvider, ZR_LSP_FIELD_RESOLVE_PROVIDER,
                              ZrLanguageServer_LspCapabilityRegistry_HasResolveForRuntime(
                                      ZR_LSP_FIELD_DOCUMENT_LINK_PROVIDER, ZR_LSP_RUNTIME_NATIVE));
        cJSON_AddItemToObject(capabilities, ZR_LSP_FIELD_DOCUMENT_LINK_PROVIDER, documentLinkProvider);
    }

    codeLensProvider = cJSON_CreateObject();
    if (codeLensProvider != NULL) {
        cJSON_AddBoolToObject(codeLensProvider, ZR_LSP_FIELD_RESOLVE_PROVIDER,
                              ZrLanguageServer_LspCapabilityRegistry_HasResolveForRuntime(
                                      ZR_LSP_FIELD_CODE_LENS_PROVIDER, ZR_LSP_RUNTIME_NATIVE));
        cJSON_AddItemToObject(capabilities, ZR_LSP_FIELD_CODE_LENS_PROVIDER, codeLensProvider);
    }

    diagnosticProvider = cJSON_CreateObject();
    if (diagnosticProvider != NULL) {
        cJSON_AddBoolToObject(diagnosticProvider, ZR_LSP_FIELD_INTER_FILE_DEPENDENCIES, 1);
        cJSON_AddBoolToObject(diagnosticProvider, ZR_LSP_FIELD_WORKSPACE_DIAGNOSTICS, 1);
        cJSON_AddItemToObject(capabilities, ZR_LSP_FIELD_DIAGNOSTIC_PROVIDER, diagnosticProvider);
    }
}
