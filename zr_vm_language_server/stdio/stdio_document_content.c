#include "zr_vm_language_server_stdio_internal.h"

SZrFileVersion *get_file_version_for_uri(SZrStdioServer *server, SZrString *uri) {
    if (server == ZR_NULL || server->context == ZR_NULL || server->context->parser == ZR_NULL || uri == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrLanguageServer_IncrementalParser_GetFileVersion(server->context->parser, uri);
}

static char *apply_single_change(SZrString *uri,
                                 const char *original,
                                 size_t originalLength,
                                 const cJSON *change,
                                 size_t *outLength) {
    const cJSON *textJson;
    const cJSON *rangeJson;
    const char *replacement;
    size_t replacementLength;
    char *updated;
    size_t prefixLength;
    size_t suffixLength;
    size_t startOffset = 0;
    size_t endOffset = originalLength;

    if (uri == ZR_NULL || original == NULL || change == NULL || outLength == NULL) {
        return NULL;
    }

    textJson = get_object_item(change, ZR_LSP_FIELD_TEXT);
    if (!cJSON_IsString(textJson)) {
        return NULL;
    }

    replacement = cJSON_GetStringValue(textJson);
    replacementLength = replacement != NULL ? strlen(replacement) : 0;
    rangeJson = get_object_item(change, ZR_LSP_FIELD_RANGE);

    if (rangeJson != NULL && !cJSON_IsNull(rangeJson)) {
        SZrLspRange lspRange;
        SZrFileRange fileRange;
        if (!parse_range(rangeJson, &lspRange)) {
            return NULL;
        }

        fileRange = ZrLanguageServer_LspRange_ToFileRangeWithContent(
            lspRange,
            uri,
            original,
            (TZrSize)originalLength
        );
        startOffset = (size_t)fileRange.start.offset;
        endOffset = (size_t)fileRange.end.offset;
        if (startOffset > originalLength) {
            startOffset = originalLength;
        }
        if (endOffset > originalLength) {
            endOffset = originalLength;
        }
        if (endOffset < startOffset) {
            endOffset = startOffset;
        }
    }

    prefixLength = startOffset;
    suffixLength = originalLength - endOffset;
    *outLength = prefixLength + replacementLength + suffixLength;

    updated = (char *)malloc(*outLength + 1);
    if (updated == NULL) {
        return NULL;
    }

    if (prefixLength > 0) {
        memcpy(updated, original, prefixLength);
    }
    if (replacementLength > 0) {
        memcpy(updated + prefixLength, replacement, replacementLength);
    }
    if (suffixLength > 0) {
        memcpy(updated + prefixLength + replacementLength, original + endOffset, suffixLength);
    }
    updated[*outLength] = '\0';
    return updated;
}

char *apply_content_changes(SZrString *uri,
                            const char *original,
                            size_t originalLength,
                            const cJSON *changes,
                            size_t *outLength) {
    char *current;
    size_t currentLength;
    int index;

    if (uri == ZR_NULL || original == NULL || changes == NULL || outLength == NULL) {
        return NULL;
    }

    current = duplicate_string_range(original, originalLength);
    if (current == NULL) {
        return NULL;
    }
    currentLength = originalLength;

    for (index = 0; index < cJSON_GetArraySize((cJSON *)changes); index++) {
        const cJSON *change = cJSON_GetArrayItem((cJSON *)changes, index);
        char *updated;
        size_t updatedLength;

        updated = apply_single_change(uri, current, currentLength, change, &updatedLength);
        if (updated == NULL) {
            free(current);
            return NULL;
        }

        free(current);
        current = updated;
        currentLength = updatedLength;
    }

    *outLength = currentLength;
    return current;
}

int update_document_contents(SZrStdioServer *server,
                             SZrString *uri,
                             const char *content,
                             size_t contentLength,
                             TZrSize version) {
    int updateOk;

    if (server == ZR_NULL || uri == ZR_NULL || content == NULL) {
        return 0;
    }

    updateOk = ZrLanguageServer_Lsp_UpdateDocument(
                   server->state,
                   server->context,
                   uri,
                   content,
                   (TZrSize)contentLength,
                   version)
                   ? 1
                   : 0;
    publish_diagnostics(server, uri);
    return updateOk;
}

int update_document_contents_from_disk(SZrStdioServer *server, SZrString *uri) {
    char *sourceCode;
    size_t sourceLength;
    SZrFileVersion *fileVersion;
    TZrSize version;
    int success;

    if (server == ZR_NULL || uri == ZR_NULL || server->state == ZR_NULL || server->state->global == ZR_NULL) {
        return 0;
    }

    fileVersion = get_file_version_for_uri(server, uri);
    version = fileVersion != ZR_NULL ? fileVersion->version + 1 : 0;
    if (server->context != ZR_NULL) {
        SZrTypeValue key;
        SZrHashKeyValuePair *pair;

        ZrCore_Value_InitAsRawObject(server->state, &key, &uri->super);
        pair = ZrCore_HashSet_Find(server->state, &server->context->uriToAnalyzerMap, &key);
        if (pair != ZR_NULL && pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
            SZrSemanticAnalyzer *analyzer = (SZrSemanticAnalyzer *)pair->value.value.nativeObject.nativePointer;
            if (analyzer != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_Free(server->state, analyzer);
            }
        }
        ZrCore_HashSet_Remove(server->state, &server->context->uriToAnalyzerMap, &key);

        if (server->context->parser != ZR_NULL) {
            ZrLanguageServer_IncrementalParser_RemoveFile(server->state, server->context->parser, uri);
        }
    }

    sourceCode = read_document_text_from_uri(uri, &sourceLength);
    if (sourceCode == ZR_NULL) {
        return 0;
    }

    success = update_document_contents(server, uri, sourceCode, sourceLength, version);
    free(sourceCode);
    return success;
}
