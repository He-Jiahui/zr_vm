#include "zr_vm_language_server_stdio_internal.h"

static cJSON *create_semantic_tokens_response(SZrStdioServer *server,
                                              const cJSON *params,
                                              const SZrLspRange *range) {
    const char *uriText;
    SZrString *uri;
    SZrArray tokens = {0};
    cJSON *result;
    char resultId[64];

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return NULL;
    }

    ZrCore_Array_Init(server->state, &tokens, sizeof(TZrUInt32), ZR_LSP_SEMANTIC_TOKEN_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetSemanticTokens(server->state, server->context, uri, &tokens)) {
        ZrCore_Array_Free(server->state, &tokens);
        return cJSON_CreateNull();
    }

    format_semantic_tokens_result_id(&tokens, resultId, sizeof(resultId));
    result = range != ZR_NULL ? serialize_semantic_tokens_range_result(&tokens, *range)
                              : serialize_semantic_tokens_result(&tokens, resultId);
    if (range == ZR_NULL && result != NULL) {
        upsert_semantic_token_snapshot(server, uriText, resultId, &tokens);
    }
    ZrCore_Array_Free(server->state, &tokens);
    return result != NULL ? result : cJSON_CreateNull();
}

cJSON *handle_semantic_tokens_full_request(SZrStdioServer *server, const cJSON *params) {
    return create_semantic_tokens_response(server, params, ZR_NULL);
}

cJSON *handle_semantic_tokens_full_delta_request(SZrStdioServer *server, const cJSON *params) {
    const char *uriText;
    const cJSON *previousResultIdJson;
    const char *previousResultIdText = NULL;
    SZrString *uri;
    SZrArray tokens = {0};
    TZrSize previousLength;
    SZrSemanticTokenSnapshot *previousSnapshot;
    cJSON *result;
    char resultId[64];

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return NULL;
    }

    previousResultIdJson = get_object_item(params, ZR_LSP_FIELD_PREVIOUS_RESULT_ID);
    if (cJSON_IsString((cJSON *)previousResultIdJson)) {
        previousResultIdText = cJSON_GetStringValue((cJSON *)previousResultIdJson);
    }
    previousLength = semantic_tokens_previous_result_length(params);
    ZrCore_Array_Init(server->state, &tokens, sizeof(TZrUInt32), ZR_LSP_SEMANTIC_TOKEN_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetSemanticTokens(server->state, server->context, uri, &tokens)) {
        ZrCore_Array_Free(server->state, &tokens);
        return cJSON_CreateNull();
    }

    format_semantic_tokens_result_id(&tokens, resultId, sizeof(resultId));
    previousSnapshot = find_semantic_token_snapshot(server, uriText);
    result =
        serialize_semantic_tokens_delta_result(&tokens, previousLength, previousResultIdText, previousSnapshot, resultId);
    if (result != NULL) {
        upsert_semantic_token_snapshot(server, uriText, resultId, &tokens);
    }
    ZrCore_Array_Free(server->state, &tokens);
    return result != NULL ? result : cJSON_CreateNull();
}

cJSON *handle_semantic_tokens_range_request(SZrStdioServer *server, const cJSON *params) {
    SZrLspRange range;

    if (!parse_range(get_object_item(params, ZR_LSP_FIELD_RANGE), &range)) {
        return cJSON_CreateNull();
    }

    return create_semantic_tokens_response(server, params, &range);
}
