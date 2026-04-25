#include "zr_vm_language_server_stdio_internal.h"

#define ZR_LSP_SEMANTIC_TOKEN_TUPLE_SIZE 5

cJSON *create_semantic_token_legend_json(void) {
    cJSON *legend = cJSON_CreateObject();
    cJSON *types = cJSON_CreateArray();
    cJSON *modifiers = cJSON_CreateArray();

    if (legend == NULL || types == NULL || modifiers == NULL) {
        cJSON_Delete(legend);
        cJSON_Delete(types);
        cJSON_Delete(modifiers);
        return NULL;
    }

    for (TZrSize index = 0; index < ZrLanguageServer_Lsp_SemanticTokenTypeCount(); index++) {
        const TZrChar *typeName = ZrLanguageServer_Lsp_SemanticTokenTypeName(index);
        if (typeName != ZR_NULL) {
            cJSON_AddItemToArray(types, cJSON_CreateString(typeName));
        }
    }

    cJSON_AddItemToObject(legend, ZR_LSP_FIELD_TOKEN_TYPES, types);
    cJSON_AddItemToObject(legend, ZR_LSP_FIELD_TOKEN_MODIFIERS, modifiers);
    return legend;
}

static cJSON *serialize_semantic_tokens_result(SZrArray *tokens) {
    cJSON *result = cJSON_CreateObject();
    cJSON *data = cJSON_CreateArray();
    char resultId[64];

    if (result == NULL || data == NULL) {
        cJSON_Delete(result);
        cJSON_Delete(data);
        return NULL;
    }

    for (TZrSize index = 0; tokens != ZR_NULL && index < tokens->length; index++) {
        TZrUInt32 *valuePtr = (TZrUInt32 *)ZrCore_Array_Get(tokens, index);
        if (valuePtr != ZR_NULL) {
            cJSON_AddItemToArray(data, cJSON_CreateNumber((double)(*valuePtr)));
        }
    }

    snprintf(resultId, sizeof(resultId), "zr-semantic:%u", (unsigned int)(tokens != ZR_NULL ? tokens->length : 0));
    cJSON_AddStringToObject(result, ZR_LSP_FIELD_RESULT_ID, resultId);
    cJSON_AddItemToObject(result, ZR_LSP_FIELD_DATA, data);
    return result;
}

static TZrSize semantic_tokens_previous_result_length(const cJSON *params) {
    const cJSON *previousResultId = get_object_item(params, ZR_LSP_FIELD_PREVIOUS_RESULT_ID);
    const char *text;
    const char *prefix = "zr-semantic:";
    size_t prefixLength = strlen(prefix);

    if (!cJSON_IsString((cJSON *)previousResultId)) {
        return 0;
    }

    text = cJSON_GetStringValue((cJSON *)previousResultId);
    if (text == NULL || strncmp(text, prefix, prefixLength) != 0) {
        return 0;
    }

    return (TZrSize)strtoul(text + prefixLength, NULL, 10);
}

static cJSON *serialize_semantic_tokens_delta_result(SZrArray *tokens, TZrSize previousLength) {
    cJSON *result = cJSON_CreateObject();
    cJSON *edits = cJSON_CreateArray();
    cJSON *edit = cJSON_CreateObject();
    cJSON *data = cJSON_CreateArray();
    char resultId[64];

    if (result == NULL || edits == NULL || edit == NULL || data == NULL) {
        cJSON_Delete(result);
        cJSON_Delete(edits);
        cJSON_Delete(edit);
        cJSON_Delete(data);
        return NULL;
    }

    for (TZrSize index = 0; tokens != ZR_NULL && index < tokens->length; index++) {
        TZrUInt32 *valuePtr = (TZrUInt32 *)ZrCore_Array_Get(tokens, index);
        if (valuePtr != ZR_NULL) {
            cJSON_AddItemToArray(data, cJSON_CreateNumber((double)(*valuePtr)));
        }
    }

    snprintf(resultId, sizeof(resultId), "zr-semantic:%u", (unsigned int)(tokens != ZR_NULL ? tokens->length : 0));
    cJSON_AddStringToObject(result, ZR_LSP_FIELD_RESULT_ID, resultId);
    cJSON_AddNumberToObject(edit, ZR_LSP_FIELD_START, 0);
    cJSON_AddNumberToObject(edit, ZR_LSP_FIELD_DELETE_COUNT, (double)previousLength);
    cJSON_AddItemToObject(edit, ZR_LSP_FIELD_DATA, data);
    cJSON_AddItemToArray(edits, edit);
    cJSON_AddItemToObject(result, ZR_LSP_FIELD_EDITS, edits);
    return result;
}

static int is_semantic_token_in_range(TZrUInt32 line, TZrUInt32 character, SZrLspRange range) {
    TZrUInt32 startLine;
    TZrUInt32 startCharacter;
    TZrUInt32 endLine;
    TZrUInt32 endCharacter;

    if (range.start.line < 0 || range.start.character < 0 || range.end.line < 0 || range.end.character < 0) {
        return 0;
    }

    startLine = (TZrUInt32)range.start.line;
    startCharacter = (TZrUInt32)range.start.character;
    endLine = (TZrUInt32)range.end.line;
    endCharacter = (TZrUInt32)range.end.character;

    return (line > startLine || (line == startLine && character >= startCharacter)) &&
           (line < endLine || (line == endLine && character < endCharacter));
}

static void add_semantic_token_tuple(cJSON *data,
                                     TZrUInt32 deltaLine,
                                     TZrUInt32 deltaStart,
                                     TZrUInt32 length,
                                     TZrUInt32 tokenType,
                                     TZrUInt32 tokenModifiers) {
    cJSON_AddItemToArray(data, cJSON_CreateNumber((double)deltaLine));
    cJSON_AddItemToArray(data, cJSON_CreateNumber((double)deltaStart));
    cJSON_AddItemToArray(data, cJSON_CreateNumber((double)length));
    cJSON_AddItemToArray(data, cJSON_CreateNumber((double)tokenType));
    cJSON_AddItemToArray(data, cJSON_CreateNumber((double)tokenModifiers));
}

static cJSON *serialize_semantic_tokens_range_result(SZrArray *tokens, SZrLspRange range) {
    cJSON *result = cJSON_CreateObject();
    cJSON *data = cJSON_CreateArray();
    TZrUInt32 absoluteLine = 0;
    TZrUInt32 absoluteCharacter = 0;
    TZrUInt32 previousIncludedLine = 0;
    TZrUInt32 previousIncludedCharacter = 0;
    TZrBool hasPreviousIncluded = ZR_FALSE;

    if (result == NULL || data == NULL) {
        cJSON_Delete(result);
        cJSON_Delete(data);
        return NULL;
    }

    for (TZrSize index = 0;
         tokens != ZR_NULL && index + ZR_LSP_SEMANTIC_TOKEN_TUPLE_SIZE - 1 < tokens->length;
         index += ZR_LSP_SEMANTIC_TOKEN_TUPLE_SIZE) {
        TZrUInt32 *deltaLinePtr = (TZrUInt32 *)ZrCore_Array_Get(tokens, index);
        TZrUInt32 *deltaStartPtr = (TZrUInt32 *)ZrCore_Array_Get(tokens, index + 1);
        TZrUInt32 *lengthPtr = (TZrUInt32 *)ZrCore_Array_Get(tokens, index + 2);
        TZrUInt32 *tokenTypePtr = (TZrUInt32 *)ZrCore_Array_Get(tokens, index + 3);
        TZrUInt32 *tokenModifiersPtr = (TZrUInt32 *)ZrCore_Array_Get(tokens, index + 4);

        if (deltaLinePtr == ZR_NULL || deltaStartPtr == ZR_NULL || lengthPtr == ZR_NULL ||
            tokenTypePtr == ZR_NULL || tokenModifiersPtr == ZR_NULL) {
            continue;
        }

        if (*deltaLinePtr == 0) {
            absoluteCharacter += *deltaStartPtr;
        } else {
            absoluteLine += *deltaLinePtr;
            absoluteCharacter = *deltaStartPtr;
        }

        if (is_semantic_token_in_range(absoluteLine, absoluteCharacter, range)) {
            TZrUInt32 encodedDeltaLine = hasPreviousIncluded ? absoluteLine - previousIncludedLine : absoluteLine;
            TZrUInt32 encodedDeltaStart =
                (hasPreviousIncluded && encodedDeltaLine == 0)
                    ? absoluteCharacter - previousIncludedCharacter
                    : absoluteCharacter;

            add_semantic_token_tuple(data,
                                     encodedDeltaLine,
                                     encodedDeltaStart,
                                     *lengthPtr,
                                     *tokenTypePtr,
                                     *tokenModifiersPtr);
            previousIncludedLine = absoluteLine;
            previousIncludedCharacter = absoluteCharacter;
            hasPreviousIncluded = ZR_TRUE;
        }
    }

    cJSON_AddItemToObject(result, ZR_LSP_FIELD_DATA, data);
    return result;
}

static cJSON *create_semantic_tokens_response(SZrStdioServer *server,
                                              const cJSON *params,
                                              const SZrLspRange *range) {
    const char *uriText;
    SZrString *uri;
    SZrArray tokens = {0};
    cJSON *result;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return NULL;
    }
    ZR_UNUSED_PARAMETER(uriText);

    ZrCore_Array_Init(server->state, &tokens, sizeof(TZrUInt32), ZR_LSP_SEMANTIC_TOKEN_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetSemanticTokens(server->state, server->context, uri, &tokens)) {
        ZrCore_Array_Free(server->state, &tokens);
        return cJSON_CreateNull();
    }

    result = range != ZR_NULL ? serialize_semantic_tokens_range_result(&tokens, *range)
                              : serialize_semantic_tokens_result(&tokens);
    ZrCore_Array_Free(server->state, &tokens);
    return result != NULL ? result : cJSON_CreateNull();
}

cJSON *handle_semantic_tokens_full_request(SZrStdioServer *server, const cJSON *params) {
    return create_semantic_tokens_response(server, params, ZR_NULL);
}

cJSON *handle_semantic_tokens_full_delta_request(SZrStdioServer *server, const cJSON *params) {
    const char *uriText;
    SZrString *uri;
    SZrArray tokens = {0};
    TZrSize previousLength;
    cJSON *result;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return NULL;
    }
    ZR_UNUSED_PARAMETER(uriText);

    previousLength = semantic_tokens_previous_result_length(params);
    ZrCore_Array_Init(server->state, &tokens, sizeof(TZrUInt32), ZR_LSP_SEMANTIC_TOKEN_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetSemanticTokens(server->state, server->context, uri, &tokens)) {
        ZrCore_Array_Free(server->state, &tokens);
        return cJSON_CreateNull();
    }

    result = serialize_semantic_tokens_delta_result(&tokens, previousLength);
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
