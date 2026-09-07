#include "zr_vm_language_server_stdio_internal.h"
#include "zr_vm_language_server/lsp_capability_registry.h"

static cJSON *inventoryComparisons;
static int inventoryComparisonAllocationFailed;
static SZrStdioServer *inventoryExpectedServer;
static const cJSON *inventoryExpectedParams;
static size_t inventoryHandlerCalls;
static int inventoryHandlerArgumentsFailed;

static int inventory_error(const char *message, const char *subject) {
    fprintf(stderr, "lsp capability inventory: %s%s%s\n", message,
            subject != NULL ? ": " : "", subject != NULL ? subject : "");
    return 0;
}

static int inventory_append_owned(cJSON *array, cJSON *item) {
    if (item == NULL || !cJSON_AddItemToArray(array, item)) {
        cJSON_Delete(item);
        return 0;
    }
    return 1;
}

static int inventory_method_compare(const char *method, const char *candidate) {
    const int comparison = strcmp(method, candidate);

    if (inventoryComparisons != NULL) {
        cJSON *entry = cJSON_CreateObject();
        if (entry == NULL ||
            cJSON_AddStringToObject(entry, "method", candidate) == NULL ||
            cJSON_AddNumberToObject(entry, "comparison", comparison) == NULL) {
            cJSON_Delete(entry);
            inventoryComparisonAllocationFailed = 1;
        } else if (!inventory_append_owned(inventoryComparisons, entry)) {
            inventoryComparisonAllocationFailed = 1;
        }
    }
    return comparison;
}

static cJSON *inventory_handler_result(const char *handler,
                                       SZrStdioServer *server,
                                       const cJSON *params) {
    inventoryHandlerCalls++;
    if (server != inventoryExpectedServer || params != inventoryExpectedParams) {
        inventoryHandlerArgumentsFailed = 1;
    }
    return cJSON_CreateString(handler);
}

/* Only handler bodies are replaced; method selection remains production code. */
#define INVENTORY_HANDLER(name) \
    cJSON *name(SZrStdioServer *server, const cJSON *params) { \
        return inventory_handler_result(#name, server, params); \
    }
#define INVENTORY_STATUS_HANDLER(name) \
    SZrLspHandlerResult name(SZrStdioServer *server, const cJSON *params) { \
        SZrLspHandlerResult response = {ZR_LSP_HANDLER_OK, inventory_handler_result(#name, server, params)}; \
        return response; \
    }

INVENTORY_HANDLER(handle_completion_request)
INVENTORY_HANDLER(handle_completion_item_resolve_request)
INVENTORY_STATUS_HANDLER(handle_hover_request)
INVENTORY_STATUS_HANDLER(handle_rich_hover_request)
INVENTORY_STATUS_HANDLER(handle_signature_help_request)
INVENTORY_STATUS_HANDLER(handle_inlay_hint_request)
INVENTORY_STATUS_HANDLER(handle_definition_request)
INVENTORY_HANDLER(handle_implementation_request)
INVENTORY_STATUS_HANDLER(handle_references_request)
INVENTORY_HANDLER(handle_formatting_request)
INVENTORY_HANDLER(handle_range_formatting_request)
INVENTORY_HANDLER(handle_ranges_formatting_request)
INVENTORY_HANDLER(handle_on_type_formatting_request)
INVENTORY_HANDLER(handle_code_action_request)
INVENTORY_HANDLER(handle_code_action_resolve_request)
INVENTORY_HANDLER(handle_folding_range_request)
INVENTORY_HANDLER(handle_selection_range_request)
INVENTORY_HANDLER(handle_linked_editing_range_request)
INVENTORY_HANDLER(handle_moniker_request)
INVENTORY_HANDLER(handle_inline_value_request)
INVENTORY_HANDLER(handle_inline_completion_request)
INVENTORY_HANDLER(handle_document_link_request)
INVENTORY_HANDLER(handle_code_lens_request)
INVENTORY_HANDLER(handle_prepare_call_hierarchy_request)
INVENTORY_HANDLER(handle_call_hierarchy_incoming_calls_request)
INVENTORY_HANDLER(handle_call_hierarchy_outgoing_calls_request)
INVENTORY_HANDLER(handle_prepare_type_hierarchy_request)
INVENTORY_HANDLER(handle_type_hierarchy_supertypes_request)
INVENTORY_HANDLER(handle_type_hierarchy_subtypes_request)
INVENTORY_HANDLER(handle_text_document_diagnostic_request)
INVENTORY_HANDLER(handle_workspace_diagnostic_request)
INVENTORY_STATUS_HANDLER(handle_document_symbols_request)
INVENTORY_STATUS_HANDLER(handle_workspace_symbols_request)
INVENTORY_HANDLER(handle_will_rename_files_request)
INVENTORY_STATUS_HANDLER(handle_document_highlights_request)
INVENTORY_HANDLER(handle_semantic_tokens_full_request)
INVENTORY_HANDLER(handle_semantic_tokens_full_delta_request)
INVENTORY_HANDLER(handle_semantic_tokens_range_request)
INVENTORY_HANDLER(handle_prepare_rename_request)
INVENTORY_HANDLER(handle_rename_request)
INVENTORY_STATUS_HANDLER(handle_native_declaration_document_request)
INVENTORY_HANDLER(handle_project_modules_request)

#undef INVENTORY_HANDLER
#undef INVENTORY_STATUS_HANDLER

/* The guarded internal header is already loaded before strcmp is intercepted. */
#define strcmp inventory_method_compare
#include "../../zr_vm_language_server/stdio/stdio_request_dispatch.c"
#undef strcmp

static int inventory_probe_dispatch(SZrStdioServer *server,
                                     const char *method,
                                     const cJSON *params,
                                     const char *expectedHandler,
                                     cJSON **outResult) {
    cJSON initialResult = {0};
    cJSON *result = &initialResult;
    EZrLspHandlerStatus status = ZR_LSP_HANDLER_INVALID_PARAMS;
    int handled;
    int valid;

    if (outResult != NULL) {
        *outResult = NULL;
    }
    inventoryExpectedServer = server;
    inventoryExpectedParams = params;
    inventoryHandlerCalls = 0;
    inventoryHandlerArgumentsFailed = 0;
    handled = dispatch_request_method(server, method, params, &result, &status);
    valid = status == ZR_LSP_HANDLER_OK && !inventoryHandlerArgumentsFailed;
    if (handled == 0) {
        valid = valid && result == NULL && inventoryHandlerCalls == 0;
    } else if (handled == 1) {
        valid = valid && inventoryHandlerCalls == 1 &&
                cJSON_IsString(result) && result->valuestring != NULL;
        if (valid && expectedHandler != NULL) {
            valid = strcmp(result->valuestring, expectedHandler) == 0;
        }
    } else {
        valid = 0;
    }
    if (!valid) {
        if (result != &initialResult) {
            cJSON_Delete(result);
        }
        inventory_error("dispatcher result, status, or handler contract failed", method);
        return -1;
    }
    if (outResult != NULL) {
        *outResult = result;
    } else {
        cJSON_Delete(result);
    }
    return handled;
}

static int inventory_validate_comparisons(const cJSON *comparisons) {
    const cJSON *entry;

    if (cJSON_GetArraySize(comparisons) == 0) {
        return inventory_error("dispatcher exposed no method comparisons", NULL);
    }
    cJSON_ArrayForEach(entry, comparisons) {
        const cJSON *method = cJSON_GetObjectItemCaseSensitive(entry, "method");
        const cJSON *comparison = cJSON_GetObjectItemCaseSensitive(entry, "comparison");
        const cJSON *previous;

        if (!cJSON_IsString(method) || method->valuestring == NULL ||
            method->valuestring[0] == '\0' || !cJSON_IsNumber(comparison) ||
            comparison->valueint == 0) {
            return inventory_error("invalid unknown-method comparison", NULL);
        }
        for (previous = comparisons->child; previous != entry; previous = previous->next) {
            const cJSON *previousMethod =
                    cJSON_GetObjectItemCaseSensitive(previous, "method");
            if (strcmp(previousMethod->valuestring, method->valuestring) == 0) {
                return inventory_error("duplicate dispatcher method", method->valuestring);
            }
        }
    }
    return 1;
}

static int inventory_add_route(cJSON *routes,
                               SZrStdioServer *server,
                               const cJSON *params,
                               const char *method) {
    const int expectsInlineGate =
            strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_INLINE_COMPLETION) == 0;
    const int expectsRangesGate =
            strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_RANGES_FORMATTING) == 0;
    int observed[2][2] = {{0}};
    cJSON *handler = NULL;
    cJSON *route = NULL;
    int inlineEnabled;
    int rangesEnabled;
    int succeeded = 0;

    server->supportsInlineCompletion = ZR_TRUE;
    server->supportsRangesFormatting = ZR_TRUE;
    if (inventory_probe_dispatch(server, method, params, NULL, &handler) != 1) {
        inventory_error("discovered method did not dispatch with both flags enabled", method);
        goto cleanup;
    }
    observed[1][1] = 1;
    for (inlineEnabled = 0; inlineEnabled <= 1; inlineEnabled++) {
        for (rangesEnabled = 0; rangesEnabled <= 1; rangesEnabled++) {
            const int expected = (!expectsInlineGate || inlineEnabled) &&
                                 (!expectsRangesGate || rangesEnabled);
            if (inlineEnabled == 1 && rangesEnabled == 1) {
                continue;
            }
            server->supportsInlineCompletion = inlineEnabled ? ZR_TRUE : ZR_FALSE;
            server->supportsRangesFormatting = rangesEnabled ? ZR_TRUE : ZR_FALSE;
            observed[inlineEnabled][rangesEnabled] =
                    inventory_probe_dispatch(server, method, params, handler->valuestring, NULL);
            if (observed[inlineEnabled][rangesEnabled] != expected) {
                inventory_error("optional capability dispatch gating failed", method);
                goto cleanup;
            }
        }
    }
    route = cJSON_CreateObject();
    if (route == NULL ||
        cJSON_AddStringToObject(route, "method", method) == NULL ||
        cJSON_AddStringToObject(route, "handler", handler->valuestring) == NULL ||
        cJSON_AddBoolToObject(route, "requiresInlineCompletion", !observed[0][1]) == NULL ||
        cJSON_AddBoolToObject(route, "requiresRangesFormatting", !observed[1][0]) == NULL) {
        inventory_error("could not allocate dispatcher route JSON", method);
        goto cleanup;
    }
    if (!cJSON_AddItemToArray(routes, route)) {
        inventory_error("could not append dispatcher route JSON", method);
        goto cleanup;
    }
    route = NULL;
    succeeded = 1;

cleanup:
    cJSON_Delete(route);
    cJSON_Delete(handler);
    return succeeded;
}

static int inventory_add_native_routes(cJSON *routes) {
    static const char unknownMethod[] = "$/zrCapabilityInventoryUnknownMethod";
    SZrStdioServer server = {0};
    cJSON *params = cJSON_CreateObject();
    cJSON *comparisons = cJSON_CreateArray();
    const cJSON *entry;
    int inlineEnabled;
    int rangesEnabled;
    int handled;
    int succeeded = 0;

    if (params == NULL || comparisons == NULL) {
        inventory_error("could not allocate dispatcher fixtures", NULL);
        goto cleanup;
    }
    server.supportsInlineCompletion = ZR_TRUE;
    server.supportsRangesFormatting = ZR_TRUE;
    inventoryComparisonAllocationFailed = 0;
    inventoryComparisons = comparisons;
    handled = inventory_probe_dispatch(&server, unknownMethod, params, NULL, NULL);
    inventoryComparisons = NULL;
    if (handled != 0 || inventoryComparisonAllocationFailed) {
        inventory_error("unknown-method discovery failed", NULL);
        goto cleanup;
    }
    if (!inventory_validate_comparisons(comparisons)) {
        goto cleanup;
    }
    for (inlineEnabled = 0; inlineEnabled <= 1; inlineEnabled++) {
        for (rangesEnabled = 0; rangesEnabled <= 1; rangesEnabled++) {
            server.supportsInlineCompletion = inlineEnabled ? ZR_TRUE : ZR_FALSE;
            server.supportsRangesFormatting = rangesEnabled ? ZR_TRUE : ZR_FALSE;
            if (inventory_probe_dispatch(&server, unknownMethod, params, NULL, NULL) != 0) {
                inventory_error("unknown method was accepted", NULL);
                goto cleanup;
            }
        }
    }
    cJSON_ArrayForEach(entry, comparisons) {
        const cJSON *method = cJSON_GetObjectItemCaseSensitive(entry, "method");
        if (!inventory_add_route(routes, &server, params, method->valuestring)) {
            goto cleanup;
        }
    }
    succeeded = 1;

cleanup:
    inventoryComparisons = NULL;
    inventoryExpectedServer = NULL;
    inventoryExpectedParams = NULL;
    cJSON_Delete(params);
    cJSON_Delete(comparisons);
    return succeeded;
}

static cJSON *inventory_add_nullable_string(cJSON *object,
                                             const char *key,
                                             const char *value) {
    return value != NULL ? cJSON_AddStringToObject(object, key, value)
                         : cJSON_AddNullToObject(object, key);
}

static int inventory_add_capabilities(cJSON *capabilities) {
    const TZrSize count = ZrLanguageServer_LspCapabilityRegistry_Count();
    TZrSize index;

    if (count == 0 || ZrLanguageServer_LspCapabilityRegistry_At(count) != ZR_NULL) {
        return inventory_error("invalid capability registry bounds", NULL);
    }
    for (index = 0; index < count; index++) {
        const SZrLspCapabilityDescriptor *descriptor =
                ZrLanguageServer_LspCapabilityRegistry_At(index);
        cJSON *entry;

        if (descriptor == ZR_NULL ||
            !ZrLanguageServer_LspCapabilityRegistry_IsDescriptorPublishable(descriptor) ||
            ZrLanguageServer_LspCapabilityRegistry_At(index) != descriptor) {
            return inventory_error("invalid capability descriptor lifetime", NULL);
        }
        entry = cJSON_CreateObject();
        if (entry == NULL ||
            inventory_add_nullable_string(entry, "capabilityKey", descriptor->capabilityKey) == NULL ||
            inventory_add_nullable_string(entry, "method", descriptor->method) == NULL ||
            inventory_add_nullable_string(entry, "clientCapabilityPath", descriptor->clientCapabilityPath) == NULL ||
            inventory_add_nullable_string(entry, "coreEntryPoint", descriptor->coreEntryPoint) == NULL ||
            inventory_add_nullable_string(entry, "nativeAdapter", descriptor->nativeAdapter) == NULL ||
            inventory_add_nullable_string(entry, "wasmExport", descriptor->wasmExport) == NULL ||
            inventory_add_nullable_string(entry, "testId", descriptor->testId) == NULL ||
            cJSON_AddNumberToObject(entry, "runtimeMask", descriptor->runtimeMask) == NULL ||
            cJSON_AddNumberToObject(entry, "minimumMajor", descriptor->minimumMajor) == NULL ||
            cJSON_AddNumberToObject(entry, "minimumMinor", descriptor->minimumMinor) == NULL ||
            cJSON_AddBoolToObject(entry, "hasResolve", descriptor->hasResolve != ZR_FALSE) == NULL ||
            cJSON_AddBoolToObject(entry, "isExperimental", descriptor->isExperimental != ZR_FALSE) == NULL ||
            cJSON_AddNumberToObject(entry, "resolveBehavior", descriptor->resolveBehavior) == NULL ||
            cJSON_AddNumberToObject(entry, "resolveRuntimeMask", descriptor->resolveRuntimeMask) == NULL ||
            cJSON_AddNumberToObject(entry, "implementationLayer", descriptor->implementationLayer) == NULL) {
            cJSON_Delete(entry);
            return inventory_error("could not allocate capability JSON", descriptor->capabilityKey);
        }
        if (!inventory_append_owned(capabilities, entry)) {
            return inventory_error("could not append capability JSON", descriptor->capabilityKey);
        }
    }
    return 1;
}

static int inventory_add_semantic_token_types(cJSON *types) {
    const TZrSize count = ZrLanguageServer_Lsp_SemanticTokenTypeCount();
    TZrSize index;

    if (count == 0 || ZrLanguageServer_Lsp_SemanticTokenTypeName(count) != ZR_NULL) {
        return inventory_error("invalid semantic token type bounds", NULL);
    }
    for (index = 0; index < count; index++) {
        const TZrChar *name = ZrLanguageServer_Lsp_SemanticTokenTypeName(index);
        if (name == ZR_NULL || name[0] == '\0') {
            return inventory_error("missing semantic token type name", NULL);
        }
        if (!inventory_append_owned(types, cJSON_CreateString(name))) {
            return inventory_error("could not append semantic token type JSON", name);
        }
    }
    return 1;
}

int main(void) {
    cJSON *inventory = cJSON_CreateObject();
    cJSON *capabilities;
    cJSON *semanticTokenTypes;
    cJSON *nativeFeatureRoutes;
    char *serialized = NULL;
    int exitCode = EXIT_FAILURE;

    if (inventory == NULL || cJSON_AddNumberToObject(inventory, "schemaVersion", 1) == NULL) {
        inventory_error("could not allocate inventory JSON", NULL);
        goto cleanup;
    }
    capabilities = cJSON_AddArrayToObject(inventory, "capabilities");
    semanticTokenTypes = cJSON_AddArrayToObject(inventory, "semanticTokenTypes");
    nativeFeatureRoutes = cJSON_AddArrayToObject(inventory, "nativeFeatureRoutes");
    if (capabilities == NULL || semanticTokenTypes == NULL || nativeFeatureRoutes == NULL) {
        inventory_error("could not allocate inventory arrays", NULL);
        goto cleanup;
    }
    if (!inventory_add_capabilities(capabilities) ||
        !inventory_add_semantic_token_types(semanticTokenTypes) ||
        !inventory_add_native_routes(nativeFeatureRoutes)) {
        goto cleanup;
    }
    serialized = cJSON_PrintUnformatted(inventory);
    if (serialized == NULL) {
        inventory_error("could not serialize inventory JSON", NULL);
        goto cleanup;
    }
    if (puts(serialized) == EOF || fflush(stdout) == EOF) {
        inventory_error("could not write inventory JSON", NULL);
        goto cleanup;
    }
    exitCode = EXIT_SUCCESS;

cleanup:
    cJSON_free(serialized);
    cJSON_Delete(inventory);
    return exitCode;
}
