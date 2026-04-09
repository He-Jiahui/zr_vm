#include "zr_vm_language_server_stdio_internal.h"

#include <ctype.h>

#define ZR_LSP_STDIO_HEX_DIGIT_INVALID ((TZrInt32)-1)

SZrFileVersion *get_file_version_for_uri(SZrStdioServer *server, SZrString *uri) {
    if (server == ZR_NULL || server->context == ZR_NULL || server->context->parser == ZR_NULL || uri == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrLanguageServer_IncrementalParser_GetFileVersion(server->context->parser, uri);
}

static void stdio_get_string_view(SZrString *value, TZrNativeString *text, TZrSize *length) {
    if (text == ZR_NULL || length == ZR_NULL) {
        return;
    }

    *text = ZR_NULL;
    *length = 0;
    if (value == ZR_NULL) {
        return;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        *text = ZrCore_String_GetNativeStringShort(value);
        *length = value->shortStringLength;
    } else {
        *text = ZrCore_String_GetNativeString(value);
        *length = value->longStringLength;
    }
}

static TZrBool stdio_is_hex_digit_char(TZrChar value) {
    return (value >= '0' && value <= '9') ||
           (value >= 'a' && value <= 'f') ||
           (value >= 'A' && value <= 'F');
}

static TZrInt32 stdio_hex_digit_to_value(TZrChar value) {
    if (value >= '0' && value <= '9') {
        return value - '0';
    }

    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }

    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }

    return ZR_LSP_STDIO_HEX_DIGIT_INVALID;
}

static TZrBool stdio_uri_to_native_path(SZrString *uri, TZrChar *buffer, TZrSize bufferSize) {
    TZrNativeString uriText;
    TZrSize uriLength;
    TZrSize readIndex = 0;
    TZrSize writeIndex = 0;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    buffer[0] = '\0';
    stdio_get_string_view(uri, &uriText, &uriLength);
    if (uriText == ZR_NULL) {
        return ZR_FALSE;
    }

    if (uriLength >= 7 && memcmp(uriText, "file://", 7) == 0) {
        readIndex = 7;
    }

#ifdef ZR_VM_PLATFORM_IS_WIN
    if (readIndex < uriLength &&
        uriText[readIndex] == '/' &&
        readIndex + 2 < uriLength &&
        isalpha((unsigned char)uriText[readIndex + 1]) &&
        uriText[readIndex + 2] == ':') {
        readIndex++;
    }
#endif

    while (readIndex < uriLength && writeIndex + 1 < bufferSize) {
        TZrChar current = uriText[readIndex];
        if (current == '%' &&
            readIndex + 2 < uriLength &&
            stdio_is_hex_digit_char(uriText[readIndex + 1]) &&
            stdio_is_hex_digit_char(uriText[readIndex + 2])) {
            buffer[writeIndex++] = (TZrChar)((stdio_hex_digit_to_value(uriText[readIndex + 1]) << 4) |
                                             stdio_hex_digit_to_value(uriText[readIndex + 2]));
            readIndex += 3;
            continue;
        }

#ifdef ZR_VM_PLATFORM_IS_WIN
        buffer[writeIndex++] = current == '/' ? '\\' : current;
#else
        buffer[writeIndex++] = current;
#endif
        readIndex++;
    }

    buffer[writeIndex] = '\0';
    return writeIndex > 0;
}

static char *stdio_read_all_text(const TZrChar *nativePath, size_t *outLength) {
    FILE *file;
    long rawSize;
    size_t readLength;
    char *buffer;

    if (nativePath == ZR_NULL || outLength == NULL) {
        return NULL;
    }

    *outLength = 0;
    file = fopen(nativePath, "rb");
    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    rawSize = ftell(file);
    if (rawSize < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    buffer = (char *)malloc((size_t)rawSize + 1);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    readLength = rawSize > 0 ? fread(buffer, 1, (size_t)rawSize, file) : 0;
    fclose(file);
    if (rawSize > 0 && readLength != (size_t)rawSize) {
        free(buffer);
        return NULL;
    }

    buffer[readLength] = '\0';
    *outLength = readLength;
    return buffer;
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


void publish_diagnostics(SZrStdioServer *server, SZrString *uri) {
    SZrArray diagnostics;
    cJSON *params;
    cJSON *diagnosticsJson;
    char *uriText;
    SZrFileVersion *fileVersion;

    if (server == ZR_NULL || uri == ZR_NULL) {
        return;
    }

    ZrCore_Array_Init(server->state,
                      &diagnostics,
                      sizeof(SZrLspDiagnostic *),
                      ZR_LSP_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(server->state, server->context, uri, &diagnostics)) {
        ZrCore_Array_Free(server->state, &diagnostics);
        return;
    }

    params = cJSON_CreateObject();
    diagnosticsJson = serialize_diagnostics_array(&diagnostics);
    uriText = zr_string_to_c_string(uri);
    fileVersion = get_file_version_for_uri(server, uri);

    if (params != NULL) {
        cJSON_AddStringToObject(params, ZR_LSP_FIELD_URI, uriText != NULL ? uriText : "");
        if (fileVersion != ZR_NULL) {
            cJSON_AddNumberToObject(params, ZR_LSP_FIELD_VERSION, (double)fileVersion->version);
        }
        cJSON_AddItemToObject(params, ZR_LSP_FIELD_DIAGNOSTICS, diagnosticsJson);
        send_notification(ZR_LSP_METHOD_TEXT_DOCUMENT_PUBLISH_DIAGNOSTICS, params);
    } else {
        cJSON_Delete(diagnosticsJson);
    }

    free(uriText);
    free_diagnostics_array(server->state, &diagnostics);
}

void publish_empty_diagnostics(SZrStdioServer *server, SZrString *uri) {
    cJSON *params;
    cJSON *diagnostics;
    char *uriText;

    if (server == ZR_NULL || uri == ZR_NULL) {
        return;
    }

    params = cJSON_CreateObject();
    diagnostics = cJSON_CreateArray();
    uriText = zr_string_to_c_string(uri);

    if (params != NULL && diagnostics != NULL) {
        cJSON_AddStringToObject(params, ZR_LSP_FIELD_URI, uriText != NULL ? uriText : "");
        cJSON_AddItemToObject(params, ZR_LSP_FIELD_DIAGNOSTICS, diagnostics);
        send_notification(ZR_LSP_METHOD_TEXT_DOCUMENT_PUBLISH_DIAGNOSTICS, params);
    } else {
        cJSON_Delete(params);
        cJSON_Delete(diagnostics);
    }

    free(uriText);
}

const cJSON *get_object_item(const cJSON *json, const char *key) {
    if (json == NULL || key == NULL) {
        return NULL;
    }
    return cJSON_GetObjectItemCaseSensitive((cJSON *)json, key);
}

TZrSize parse_size_value(const cJSON *json, TZrSize fallback) {
    if (!cJSON_IsNumber((cJSON *)json) || json->valuedouble < 0) {
        return fallback;
    }
    return (TZrSize)json->valuedouble;
}

int get_uri_from_text_document(SZrStdioServer *server,
                               const cJSON *params,
                               const char **outUriText,
                               SZrString **outUri) {
    const cJSON *textDocument;
    const cJSON *uriJson;
    const char *uriText;

    if (server == ZR_NULL || params == NULL || outUriText == NULL || outUri == NULL) {
        return 0;
    }

    textDocument = get_object_item(params, ZR_LSP_FIELD_TEXT_DOCUMENT);
    uriJson = get_object_item(textDocument, ZR_LSP_FIELD_URI);
    if (!cJSON_IsString((cJSON *)uriJson)) {
        return 0;
    }

    uriText = cJSON_GetStringValue((cJSON *)uriJson);
    if (uriText == NULL) {
        return 0;
    }

    *outUriText = uriText;
    *outUri = server_get_cached_uri(server, uriText);
    return *outUri != ZR_NULL;
}

int get_uri_and_position(SZrStdioServer *server,
                         const cJSON *params,
                         const char **outUriText,
                         SZrString **outUri,
                         SZrLspPosition *outPosition) {
    const cJSON *positionJson;

    if (!get_uri_from_text_document(server, params, outUriText, outUri) || outPosition == NULL) {
        return 0;
    }

    positionJson = get_object_item(params, ZR_LSP_FIELD_POSITION);
    return parse_position(positionJson, outPosition);
}

int update_document_contents(SZrStdioServer *server,
                             SZrString *uri,
                             const char *content,
                             size_t contentLength,
                             TZrSize version) {
    if (server == ZR_NULL || uri == ZR_NULL || content == NULL) {
        return 0;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocument(
            server->state,
            server->context,
            uri,
            content,
            (TZrSize)contentLength,
            version)) {
        return 0;
    }
    publish_diagnostics(server, uri);
    return 1;
}

int update_document_contents_from_disk(SZrStdioServer *server, SZrString *uri) {
    TZrChar nativePath[ZR_VM_PATH_LENGTH_MAX];
    char *sourceCode;
    size_t sourceLength;
    SZrFileVersion *fileVersion;
    TZrSize version;
    int success;

    if (server == ZR_NULL || uri == ZR_NULL || server->state == ZR_NULL || server->state->global == ZR_NULL ||
        !stdio_uri_to_native_path(uri, nativePath, sizeof(nativePath))) {
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

    sourceCode = stdio_read_all_text(nativePath, &sourceLength);
    if (sourceCode == ZR_NULL) {
        return 0;
    }

    success = update_document_contents(server, uri, sourceCode, sourceLength, version);
    free(sourceCode);
    return success;
}
