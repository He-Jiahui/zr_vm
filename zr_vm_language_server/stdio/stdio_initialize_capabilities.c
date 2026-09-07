#include "zr_vm_language_server_stdio_internal.h"
#include "stdio_json_builder.h"
#include "zr_vm_language_server/lsp_capability_registry.h"

static const char ZR_STDIO_FIELD_RANGES_SUPPORT[] = "rangesSupport";

static TZrBool optional_editor_capability_is_valid(const cJSON *capability) {
    const cJSON *dynamicRegistration = get_object_item(capability, "dynamicRegistration");

    return cJSON_IsObject(capability) &&
                   (dynamicRegistration == ZR_NULL || cJSON_IsBool(dynamicRegistration))
                   ? ZR_TRUE : ZR_FALSE;
}

TZrBool add_advanced_editor_capabilities(SZrStdioServer *server,
                                         const cJSON *params,
                                         cJSON *capabilities) {
    const cJSON *clientCapabilities = get_object_item(params, ZR_LSP_FIELD_CAPABILITIES);
    const cJSON *textDocument = get_object_item(clientCapabilities, ZR_LSP_FIELD_TEXT_DOCUMENT);
    const cJSON *inlineCompletion = get_object_item(textDocument, "inlineCompletion");
    const cJSON *rangeFormatting = get_object_item(textDocument, "rangeFormatting");
    const TZrBool supportsInlineCompletion = optional_editor_capability_is_valid(inlineCompletion);
    const TZrBool supportsRangesFormatting =
            optional_editor_capability_is_valid(rangeFormatting) &&
            cJSON_IsTrue(get_object_item(rangeFormatting, ZR_STDIO_FIELD_RANGES_SUPPORT));
    const char *actionKinds[] = {
            ZR_LSP_CODE_ACTION_KIND_QUICK_FIX,
            ZR_LSP_CODE_ACTION_KIND_SOURCE_ORGANIZE_IMPORTS,
            ZR_LSP_CODE_ACTION_KIND_SOURCE_REMOVE_UNUSED
    };
    const char *onTypeTriggers[] = {";"};
    cJSON *codeActionProvider;
    cJSON *rangeFormattingProvider;
    cJSON *onTypeFormattingProvider;
    cJSON *documentLinkProvider;
    cJSON *codeLensProvider;
    cJSON *diagnosticProvider;

    if (server == ZR_NULL || capabilities == NULL) {
        return ZR_FALSE;
    }
    if ((codeActionProvider = cJSON_AddObjectToObject(capabilities, ZR_LSP_FIELD_CODE_ACTION_PROVIDER)) == NULL ||
        !stdio_json_add_owned_item(codeActionProvider, ZR_LSP_FIELD_CODE_ACTION_KINDS,
                                   cJSON_CreateStringArray(actionKinds,
                                           (int)(sizeof(actionKinds) / sizeof(actionKinds[0])))) ||
        cJSON_AddBoolToObject(codeActionProvider, ZR_LSP_FIELD_RESOLVE_PROVIDER,
                              ZrLanguageServer_LspCapabilityRegistry_HasResolveForRuntime(
                                      ZR_LSP_FIELD_CODE_ACTION_PROVIDER, ZR_LSP_RUNTIME_NATIVE)) == NULL ||
        cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_DOCUMENT_FORMATTING_PROVIDER, 1) == NULL) {
        return ZR_FALSE;
    }

    if (supportsRangesFormatting) {
        rangeFormattingProvider = cJSON_AddObjectToObject(capabilities, ZR_LSP_FIELD_DOCUMENT_RANGE_FORMATTING_PROVIDER);
        if (rangeFormattingProvider == NULL ||
            cJSON_AddBoolToObject(rangeFormattingProvider, ZR_STDIO_FIELD_RANGES_SUPPORT, 1) == NULL) {
            return ZR_FALSE;
        }
    } else if (cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_DOCUMENT_RANGE_FORMATTING_PROVIDER, 1) == NULL) {
        return ZR_FALSE;
    }
    if ((onTypeFormattingProvider = cJSON_AddObjectToObject(
                 capabilities, ZR_LSP_FIELD_DOCUMENT_ON_TYPE_FORMATTING_PROVIDER)) == NULL ||
        cJSON_AddStringToObject(onTypeFormattingProvider, ZR_LSP_FIELD_FIRST_TRIGGER_CHARACTER, "}") == NULL ||
        !stdio_json_add_owned_item(onTypeFormattingProvider, ZR_LSP_FIELD_MORE_TRIGGER_CHARACTER,
                                   cJSON_CreateStringArray(onTypeTriggers, 1)) ||
        cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_FOLDING_RANGE_PROVIDER, 1) == NULL ||
        cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_SELECTION_RANGE_PROVIDER, 1) == NULL ||
        cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_LINKED_EDITING_RANGE_PROVIDER, 1) == NULL ||
        cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_MONIKER_PROVIDER, 1) == NULL ||
        cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_INLINE_VALUE_PROVIDER, 1) == NULL ||
        (supportsInlineCompletion &&
         cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_INLINE_COMPLETION_PROVIDER, 1) == NULL) ||
        cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_IMPLEMENTATION_PROVIDER, 1) == NULL ||
        cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_CALL_HIERARCHY_PROVIDER, 1) == NULL ||
        cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_TYPE_HIERARCHY_PROVIDER, 1) == NULL) {
        return ZR_FALSE;
    }

    if ((documentLinkProvider = cJSON_AddObjectToObject(capabilities, ZR_LSP_FIELD_DOCUMENT_LINK_PROVIDER)) == NULL ||
        cJSON_AddBoolToObject(documentLinkProvider, ZR_LSP_FIELD_RESOLVE_PROVIDER,
                              ZrLanguageServer_LspCapabilityRegistry_HasResolveForRuntime(
                                      ZR_LSP_FIELD_DOCUMENT_LINK_PROVIDER, ZR_LSP_RUNTIME_NATIVE)) == NULL ||
        (codeLensProvider = cJSON_AddObjectToObject(capabilities, ZR_LSP_FIELD_CODE_LENS_PROVIDER)) == NULL ||
        cJSON_AddBoolToObject(codeLensProvider, ZR_LSP_FIELD_RESOLVE_PROVIDER,
                              ZrLanguageServer_LspCapabilityRegistry_HasResolveForRuntime(
                                      ZR_LSP_FIELD_CODE_LENS_PROVIDER, ZR_LSP_RUNTIME_NATIVE)) == NULL ||
        (diagnosticProvider = cJSON_AddObjectToObject(capabilities, ZR_LSP_FIELD_DIAGNOSTIC_PROVIDER)) == NULL ||
        cJSON_AddBoolToObject(diagnosticProvider, ZR_LSP_FIELD_INTER_FILE_DEPENDENCIES, 1) == NULL ||
        cJSON_AddBoolToObject(diagnosticProvider, ZR_LSP_FIELD_WORKSPACE_DIAGNOSTICS, 1) == NULL) {
        return ZR_FALSE;
    }

    server->supportsInlineCompletion = supportsInlineCompletion;
    server->supportsRangesFormatting = supportsRangesFormatting;
    return ZR_TRUE;
}
