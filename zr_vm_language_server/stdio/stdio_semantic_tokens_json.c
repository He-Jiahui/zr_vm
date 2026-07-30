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

cJSON *serialize_semantic_tokens_result(SZrArray *tokens, const char *resultId) {
    cJSON *result = cJSON_CreateObject();
    cJSON *data = cJSON_CreateArray();

    if (result == NULL || data == NULL) {
        cJSON_Delete(result);
        cJSON_Delete(data);
        return NULL;
    }

    for (TZrSize index = 0; tokens != ZR_NULL && index < tokens->length; index++) {
        cJSON_AddItemToArray(data, cJSON_CreateNumber((double)semantic_tokens_value_at(tokens, index)));
    }

    cJSON_AddStringToObject(result, ZR_LSP_FIELD_RESULT_ID, resultId);
    cJSON_AddItemToObject(result, ZR_LSP_FIELD_DATA, data);
    return result;
}

TZrSize semantic_tokens_previous_result_length(const cJSON *params) {
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

cJSON *serialize_semantic_tokens_delta_result(SZrArray *tokens,
                                              TZrSize previousLength,
                                              const char *previousResultId,
                                              const SZrSemanticTokenSnapshot *previousSnapshot,
                                              const char *resultId) {
    cJSON *result = cJSON_CreateObject();
    cJSON *edits = cJSON_CreateArray();
    cJSON *edit = cJSON_CreateObject();
    cJSON *data = cJSON_CreateArray();
    TZrSize newLength = tokens != ZR_NULL ? tokens->length : 0;
    TZrSize start = 0;
    TZrSize deleteCount;
    TZrSize insertEnd;
    const TZrUInt32 *oldData = NULL;
    TZrSize oldLength = previousLength;

    if (result == NULL || edits == NULL || edit == NULL || data == NULL) {
        cJSON_Delete(result);
        cJSON_Delete(edits);
        cJSON_Delete(edit);
        cJSON_Delete(data);
        return NULL;
    }

    cJSON_AddStringToObject(result, ZR_LSP_FIELD_RESULT_ID, resultId);
    if (previousResultId != NULL && strcmp(previousResultId, resultId) == 0) {
        cJSON_Delete(edit);
        cJSON_Delete(data);
        cJSON_AddItemToObject(result, ZR_LSP_FIELD_EDITS, edits);
        return result;
    }

    if (previousResultId != NULL && previousSnapshot != NULL &&
        strcmp(previousSnapshot->resultId, previousResultId) == 0) {
        TZrSize suffix = 0;

        oldData = previousSnapshot->data;
        oldLength = previousSnapshot->length;
        while (start < oldLength && start < newLength && oldData[start] == semantic_tokens_value_at(tokens, start)) {
            start++;
        }
        while (suffix + start < oldLength && suffix + start < newLength &&
               oldData[oldLength - suffix - 1] == semantic_tokens_value_at(tokens, newLength - suffix - 1)) {
            suffix++;
        }
        deleteCount = oldLength - start - suffix;
        insertEnd = newLength - suffix;
    } else {
        deleteCount = previousLength;
        insertEnd = newLength;
    }

    for (TZrSize index = start; index < insertEnd; index++) {
        cJSON_AddItemToArray(data, cJSON_CreateNumber((double)semantic_tokens_value_at(tokens, index)));
    }

    if (oldData != NULL && deleteCount == 0 && start == insertEnd) {
        cJSON_Delete(edit);
        cJSON_Delete(data);
    } else {
        cJSON_AddNumberToObject(edit, ZR_LSP_FIELD_START, (double)start);
        cJSON_AddNumberToObject(edit, ZR_LSP_FIELD_DELETE_COUNT, (double)deleteCount);
        if (start < insertEnd) {
            cJSON_AddItemToObject(edit, ZR_LSP_FIELD_DATA, data);
            data = NULL;
        }
        cJSON_AddItemToArray(edits, edit);
        edit = NULL;
    }

    cJSON_Delete(edit);
    cJSON_Delete(data);
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

cJSON *serialize_semantic_tokens_range_result(SZrArray *tokens, SZrLspRange range) {
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
        TZrUInt32 deltaLine = semantic_tokens_value_at(tokens, index);
        TZrUInt32 deltaStart = semantic_tokens_value_at(tokens, index + 1);
        TZrUInt32 length = semantic_tokens_value_at(tokens, index + 2);
        TZrUInt32 tokenType = semantic_tokens_value_at(tokens, index + 3);
        TZrUInt32 tokenModifiers = semantic_tokens_value_at(tokens, index + 4);

        if (deltaLine == 0) {
            absoluteCharacter += deltaStart;
        } else {
            absoluteLine += deltaLine;
            absoluteCharacter = deltaStart;
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
                                     length,
                                     tokenType,
                                     tokenModifiers);
            previousIncludedLine = absoluteLine;
            previousIncludedCharacter = absoluteCharacter;
            hasPreviousIncluded = ZR_TRUE;
        }
    }

    cJSON_AddItemToObject(result, ZR_LSP_FIELD_DATA, data);
    return result;
}
