#include "zr_vm_language_server_stdio_internal.h"
#include "stdio_handler_result.h"

#include <errno.h>
#include <stdint.h>

#define ZR_LSP_CODE_ACTION_STALE_REASON \
    "Document changed since this code action was computed"

static TZrBool parse_code_action_snapshot_size(
        const cJSON *json,
        TZrSize *outValue) {
    TZrSize value;

    if (!cJSON_IsNumber((cJSON *)json) || json->valuedouble < 0.0 ||
        json->valuedouble > ZR_LSP_JSON_SAFE_INTEGER_MAX ||
        json->valuedouble > (double)SIZE_MAX ||
        outValue == ZR_NULL) {
        return ZR_FALSE;
    }
    value = (TZrSize)json->valuedouble;
    if ((double)value != json->valuedouble) {
        return ZR_FALSE;
    }
    *outValue = value;
    return ZR_TRUE;
}

static TZrBool parse_code_action_snapshot_hash(
        const cJSON *json,
        TZrUInt64 *outValue) {
    char *end;
    unsigned long long value;

    if (!cJSON_IsString((cJSON *)json) || json->valuestring == NULL ||
        strlen(json->valuestring) != 16U || outValue == ZR_NULL) {
        return ZR_FALSE;
    }
    errno = 0;
    end = ZR_NULL;
    value = strtoull(json->valuestring, &end, 16);
    if (errno != 0 || end == json->valuestring || *end != '\0') {
        return ZR_FALSE;
    }
    *outValue = (TZrUInt64)value;
    return ZR_TRUE;
}

static TZrBool parse_code_action_semantic_identity(
        const cJSON *json,
        SZrLspSemanticSnapshotIdentity *outIdentity) {
    if (!cJSON_IsObject((cJSON *)json) || outIdentity == ZR_NULL) {
        return ZR_FALSE;
    }
    return parse_code_action_snapshot_hash(
                   get_object_item(json, ZR_LSP_FIELD_DOCUMENT_GENERATION),
                   &outIdentity->documentGeneration) &&
           parse_code_action_snapshot_hash(
                   get_object_item(json, ZR_LSP_FIELD_PROJECT_GENERATION),
                   &outIdentity->projectGeneration) &&
           parse_code_action_snapshot_hash(
                   get_object_item(json, ZR_LSP_FIELD_PROVIDER_GENERATION),
                   &outIdentity->providerGeneration) &&
           parse_code_action_snapshot_hash(
                   get_object_item(json, ZR_LSP_FIELD_SEMANTIC_GENERATION),
                   &outIdentity->semanticGeneration) &&
           parse_code_action_snapshot_hash(
                   get_object_item(json, ZR_LSP_FIELD_DEPENDENCY_FINGERPRINT),
                   &outIdentity->dependencyFingerprint);
}

static TZrBool parse_code_action_document_snapshot(
        SZrStdioServer *server,
        const cJSON *params,
        SZrLspWorkspaceEditDocumentSnapshot *outSnapshot) {
    const cJSON *data;
    const cJSON *uriJson;
    const cJSON *snapshotJson;
    const cJSON *isOpenDocumentJson;
    const cJSON *semanticIdentityJson;

    if (server == ZR_NULL || outSnapshot == ZR_NULL) {
        return ZR_FALSE;
    }
    data = get_object_item(params, ZR_LSP_FIELD_DATA);
    uriJson = get_object_item(data, ZR_LSP_FIELD_URI);
    snapshotJson = get_object_item(data, ZR_LSP_FIELD_SNAPSHOT);
    isOpenDocumentJson =
            get_object_item(snapshotJson, ZR_LSP_FIELD_IS_OPEN_DOCUMENT);
    if (!cJSON_IsString((cJSON *)uriJson) || uriJson->valuestring == NULL ||
        !cJSON_IsObject((cJSON *)snapshotJson) ||
        !cJSON_IsBool((cJSON *)isOpenDocumentJson)) {
        return ZR_FALSE;
    }

    memset(outSnapshot, 0, sizeof(*outSnapshot));
    outSnapshot->uri = server_get_cached_uri(server, uriJson->valuestring);
    outSnapshot->isOpenDocument = cJSON_IsTrue((cJSON *)isOpenDocumentJson)
                                      ? ZR_TRUE
                                      : ZR_FALSE;
    if (outSnapshot->uri == ZR_NULL ||
        !parse_code_action_snapshot_hash(
                get_object_item(snapshotJson, ZR_LSP_FIELD_CONTENT_HASH),
                &outSnapshot->contentHash) ||
        !parse_code_action_snapshot_size(
                get_object_item(snapshotJson, ZR_LSP_FIELD_CONTENT_LENGTH),
                &outSnapshot->contentLength) ||
        !parse_code_action_snapshot_size(
                get_object_item(snapshotJson, ZR_LSP_FIELD_VERSION),
                &outSnapshot->version) ||
        !parse_code_action_snapshot_size(
                get_object_item(snapshotJson, ZR_LSP_FIELD_CONTENT_GENERATION),
                &outSnapshot->contentGeneration)) {
        return ZR_FALSE;
    }

    semanticIdentityJson = get_object_item(snapshotJson, ZR_LSP_FIELD_SEMANTIC_IDENTITY);
    if (semanticIdentityJson == ZR_NULL) {
        return ZR_TRUE;
    }
    if (!parse_code_action_semantic_identity(
                semanticIdentityJson, &outSnapshot->semanticIdentity)) {
        return ZR_FALSE;
    }
    outSnapshot->hasSemanticIdentity = ZR_TRUE;
    return ZR_TRUE;
}

static cJSON *disable_stale_code_action(const cJSON *params) {
    cJSON *result;
    cJSON *disabled;

    result = params != NULL
                 ? cJSON_Duplicate((cJSON *)params, 1)
                 : cJSON_CreateObject();
    if (result == NULL || !cJSON_IsObject(result)) {
        cJSON_Delete(result);
        return NULL;
    }

    cJSON_DeleteItemFromObjectCaseSensitive(result, ZR_LSP_FIELD_EDIT);
    cJSON_DeleteItemFromObjectCaseSensitive(result, ZR_LSP_FIELD_DISABLED);
    disabled = cJSON_CreateObject();
    if (disabled == NULL ||
        cJSON_AddStringToObject(disabled, ZR_LSP_FIELD_REASON, ZR_LSP_CODE_ACTION_STALE_REASON) == NULL ||
        !cJSON_AddItemToObject(result, ZR_LSP_FIELD_DISABLED, disabled)) {
        cJSON_Delete(disabled);
        cJSON_Delete(result);
        return NULL;
    }
    return result;
}

SZrLspHandlerResult handle_formatting_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray edits = {0};
    const char *uriText;
    SZrString *uri;
    cJSON *result;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    ZrCore_Array_Init(server->state, &edits, sizeof(SZrLspTextEdit *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetFormatting(server->state, server->context, uri, &edits)) {
        ZrLanguageServer_Lsp_FreeTextEdits(server->state, &edits);
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }

    ZR_UNUSED_PARAMETER(uriText);
    result = serialize_text_edits_array(&edits);
    ZrLanguageServer_Lsp_FreeTextEdits(server->state, &edits);
    return stdio_handler_result_from_json(server->context, result);
}

SZrLspHandlerResult handle_range_formatting_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray edits = {0};
    SZrLspRange range;
    const char *uriText;
    SZrString *uri;
    cJSON *result;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }
    if (!parse_range_for_uri(server, uri, get_object_item(params, ZR_LSP_FIELD_RANGE), &range)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    ZrCore_Array_Init(server->state, &edits, sizeof(SZrLspTextEdit *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetRangeFormatting(server->state, server->context, uri, range, &edits)) {
        ZrLanguageServer_Lsp_FreeTextEdits(server->state, &edits);
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }

    ZR_UNUSED_PARAMETER(uriText);
    result = serialize_text_edits_array(&edits);
    ZrLanguageServer_Lsp_FreeTextEdits(server->state, &edits);
    return stdio_handler_result_from_json(server->context, result);
}

SZrLspHandlerResult handle_ranges_formatting_request(SZrStdioServer *server, const cJSON *params) {
    const cJSON *rangesJson;
    const cJSON *rangeJson;
    const char *uriText;
    SZrString *uri;
    cJSON *result;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }
    rangesJson = get_object_item(params, ZR_LSP_FIELD_RANGES);
    if (!cJSON_IsArray(rangesJson)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    result = cJSON_CreateArray();
    if (result == NULL) {
        return stdio_handler_result_from_json(server->context, ZR_NULL);
    }

    cJSON_ArrayForEach(rangeJson, rangesJson) {
        SZrArray edits = {0};
        SZrLspRange range;

        if (!parse_range_for_uri(server, uri, rangeJson, &range)) {
            cJSON_Delete(result);
            return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
        }

        ZrCore_Array_Init(server->state, &edits, sizeof(SZrLspTextEdit *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
        if (ZrLanguageServer_Lsp_GetRangeFormatting(server->state, server->context, uri, range, &edits)) {
            for (TZrSize index = 0; index < edits.length; index++) {
                SZrLspTextEdit **editPtr = (SZrLspTextEdit **)ZrCore_Array_Get(&edits, index);
                if (editPtr != ZR_NULL && *editPtr != ZR_NULL) {
                    cJSON_AddItemToArray(result, serialize_text_edit(*editPtr));
                }
            }
        }
        ZrLanguageServer_Lsp_FreeTextEdits(server->state, &edits);
    }

    ZR_UNUSED_PARAMETER(uriText);
    return stdio_handler_result_from_json(server->context, result);
}

SZrLspHandlerResult handle_on_type_formatting_request(SZrStdioServer *server, const cJSON *params) {
    const cJSON *chJson;
    SZrArray edits = {0};
    const char *uriText;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspRange range;
    cJSON *result;

    chJson = get_object_item(params, ZR_LSP_FIELD_CH);
    if (!cJSON_IsString(chJson) ||
        (strcmp(chJson->valuestring, "}") != 0 && strcmp(chJson->valuestring, ";") != 0) ||
        !get_uri_from_text_document(server, params, &uriText, &uri) ||
        !parse_position_for_uri(server, uri, get_object_item(params, ZR_LSP_FIELD_POSITION), &position)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    ZR_UNUSED_PARAMETER(uriText);
    range.start = position;
    range.start.character = 0;
    range.end = position;

    ZrCore_Array_Init(server->state, &edits, sizeof(SZrLspTextEdit *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetRangeFormatting(server->state, server->context, uri, range, &edits)) {
        ZrLanguageServer_Lsp_FreeTextEdits(server->state, &edits);
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }

    result = serialize_text_edits_array(&edits);
    ZrLanguageServer_Lsp_FreeTextEdits(server->state, &edits);
    return stdio_handler_result_from_json(server->context, result);
}

SZrLspHandlerResult handle_code_action_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray actions = {0};
    SZrLspRange range = {{0, 0}, {0, 0}};
    SZrLspWorkspaceEditDocumentSnapshot documentSnapshot = {0};
    const cJSON *contextJson;
    const cJSON *diagnosticsJson;
    const cJSON *onlyJson;
    cJSON *itemJson;
    const char *uriText;
    SZrString *uri;
    cJSON *result;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }
    contextJson = get_object_item(params, ZR_LSP_FIELD_CONTEXT);
    if (!cJSON_IsObject((cJSON *)contextJson)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }
    diagnosticsJson = get_object_item(contextJson, ZR_LSP_FIELD_DIAGNOSTICS);
    if (!cJSON_IsArray((cJSON *)diagnosticsJson)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }
    cJSON_ArrayForEach(itemJson, (cJSON *)diagnosticsJson) {
        if (!cJSON_IsObject(itemJson)) {
            return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
        }
    }
    onlyJson = get_object_item(contextJson, ZR_LSP_FIELD_ONLY);
    if (onlyJson != ZR_NULL) {
        if (!cJSON_IsArray((cJSON *)onlyJson)) {
            return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
        }
        cJSON_ArrayForEach(itemJson, (cJSON *)onlyJson) {
            if (!cJSON_IsString(itemJson) || itemJson->valuestring == ZR_NULL) {
                return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
            }
        }
    }
    if (!ZrLanguageServer_LspWorkspaceEdit_CaptureDocumentSnapshot(
                server->state,
                server->context,
                uri,
                &documentSnapshot)) {
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }
    if (!parse_range_for_uri(server, uri, get_object_item(params, ZR_LSP_FIELD_RANGE), &range)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    ZrCore_Array_Init(server->state, &actions, sizeof(SZrLspCodeAction *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetCodeActions(server->state, server->context, uri, range, &actions)) {
        ZrLanguageServer_Lsp_FreeCodeActions(server->state, &actions);
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }

    if (!ZrLanguageServer_LspWorkspaceEdit_ValidateDocumentSnapshot(
                server->state,
                server->context,
                &documentSnapshot)) {
        ZrLanguageServer_Lsp_FreeCodeActions(server->state, &actions);
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }
    result = serialize_code_actions_array(
            uriText, &documentSnapshot, &actions, params);
    ZrLanguageServer_Lsp_FreeCodeActions(server->state, &actions);
    return stdio_handler_result_from_json(server->context, result);
}

SZrLspHandlerResult handle_code_action_resolve_request(SZrStdioServer *server, const cJSON *params) {
    SZrLspWorkspaceEditDocumentSnapshot documentSnapshot = {0};

    if (server == ZR_NULL || !cJSON_IsObject((cJSON *)params) ||
        !parse_code_action_document_snapshot(
                server, params, &documentSnapshot)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }
    if (!ZrLanguageServer_LspWorkspaceEdit_ValidateDocumentSnapshot(
                server->state,
                server->context,
                &documentSnapshot)) {
        return stdio_handler_result_from_json(server->context, disable_stale_code_action(params));
    }
    return stdio_handler_result_from_json(server->context, cJSON_Duplicate((cJSON *)params, 1));
}
