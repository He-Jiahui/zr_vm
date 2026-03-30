#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "cJSON/cJSON.h"

#include "zr_vm_language_server/lsp_interface.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/value.h"

typedef struct SZrCachedUri {
    char *text;
    SZrString *value;
} SZrCachedUri;

typedef struct SZrUriCache {
    SZrCachedUri *items;
    size_t count;
    size_t capacity;
} SZrUriCache;

typedef struct SZrStdioServer {
    SZrGlobalState *global;
    SZrState *state;
    SZrLspContext *context;
    SZrUriCache uriCache;
    TZrBool shutdownRequested;
} SZrStdioServer;

static TZrPtr stdio_allocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize, TZrInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(originalSize);
    ZR_UNUSED_PARAMETER(flag);

    if (newSize == 0) {
        free(pointer);
        return ZR_NULL;
    }

    if (pointer == ZR_NULL) {
        return malloc(newSize);
    }

    return realloc(pointer, newSize);
}

static int starts_with_case_insensitive(const char *text, const char *prefix) {
    size_t index;

    if (text == NULL || prefix == NULL) {
        return 0;
    }

    for (index = 0; prefix[index] != '\0'; index++) {
        char left = text[index];
        char right = prefix[index];

        if (left == '\0') {
            return 0;
        }

        if (left >= 'A' && left <= 'Z') {
            left = (char)(left - 'A' + 'a');
        }
        if (right >= 'A' && right <= 'Z') {
            right = (char)(right - 'A' + 'a');
        }
        if (left != right) {
            return 0;
        }
    }

    return 1;
}

static const char *skip_spaces(const char *text) {
    while (text != NULL && (*text == ' ' || *text == '\t')) {
        text++;
    }
    return text;
}

static char *duplicate_string_range(const char *text, size_t length) {
    char *result = (char *)malloc(length + 1);
    if (result == NULL) {
        return NULL;
    }

    if (length > 0) {
        memcpy(result, text, length);
    }
    result[length] = '\0';
    return result;
}

static char *duplicate_c_string(const char *text) {
    if (text == NULL) {
        return NULL;
    }
    return duplicate_string_range(text, strlen(text));
}

static char *zr_string_to_c_string(SZrString *value) {
    TZrNativeString nativeString;
    TZrSize length;

    if (value == ZR_NULL) {
        return NULL;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        nativeString = ZrCore_String_GetNativeStringShort(value);
        length = value->shortStringLength;
    } else {
        nativeString = ZrCore_String_GetNativeString(value);
        length = value->longStringLength;
    }

    return duplicate_string_range(nativeString, length);
}

static SZrString *server_get_cached_uri(SZrStdioServer *server, const char *uriText) {
    size_t index;

    if (server == ZR_NULL || uriText == NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < server->uriCache.count; index++) {
        if (strcmp(server->uriCache.items[index].text, uriText) == 0) {
            return server->uriCache.items[index].value;
        }
    }

    if (server->uriCache.count == server->uriCache.capacity) {
        size_t newCapacity = server->uriCache.capacity == 0 ? 8 : server->uriCache.capacity * 2;
        SZrCachedUri *newItems =
            (SZrCachedUri *)realloc(server->uriCache.items, newCapacity * sizeof(SZrCachedUri));
        if (newItems == NULL) {
            return ZR_NULL;
        }
        server->uriCache.items = newItems;
        server->uriCache.capacity = newCapacity;
    }

    server->uriCache.items[server->uriCache.count].text = duplicate_c_string(uriText);
    if (server->uriCache.items[server->uriCache.count].text == NULL) {
        return ZR_NULL;
    }

    server->uriCache.items[server->uriCache.count].value =
        ZrCore_String_Create(server->state, uriText, (TZrSize)strlen(uriText));
    if (server->uriCache.items[server->uriCache.count].value == ZR_NULL) {
        free(server->uriCache.items[server->uriCache.count].text);
        server->uriCache.items[server->uriCache.count].text = NULL;
        return ZR_NULL;
    }

    server->uriCache.count++;
    return server->uriCache.items[server->uriCache.count - 1].value;
}

static void free_uri_cache(SZrUriCache *cache) {
    size_t index;

    if (cache == NULL) {
        return;
    }

    for (index = 0; index < cache->count; index++) {
        free(cache->items[index].text);
        cache->items[index].text = NULL;
        cache->items[index].value = ZR_NULL;
    }

    free(cache->items);
    cache->items = NULL;
    cache->count = 0;
    cache->capacity = 0;
}

static cJSON *duplicate_id(const cJSON *id) {
    if (id == NULL) {
        return cJSON_CreateNull();
    }
    return cJSON_Duplicate(id, 1);
}

static void send_json_message(cJSON *message) {
    char *payload;
    size_t payloadLength;

    if (message == NULL) {
        return;
    }

    payload = cJSON_PrintUnformatted(message);
    if (payload == NULL) {
        cJSON_Delete(message);
        return;
    }

    payloadLength = strlen(payload);
    fprintf(stdout, "Content-Length: %zu\r\n\r\n", payloadLength);
    fwrite(payload, 1, payloadLength, stdout);
    fflush(stdout);

    free(payload);
    cJSON_Delete(message);
}

static void send_result_response(const cJSON *id, cJSON *result) {
    cJSON *message = cJSON_CreateObject();

    if (message == NULL) {
        cJSON_Delete(result);
        return;
    }

    cJSON_AddStringToObject(message, "jsonrpc", "2.0");
    cJSON_AddItemToObject(message, "id", duplicate_id(id));
    if (result == NULL) {
        cJSON_AddNullToObject(message, "result");
    } else {
        cJSON_AddItemToObject(message, "result", result);
    }

    send_json_message(message);
}

static void send_error_response(const cJSON *id, int code, const char *messageText) {
    cJSON *message = cJSON_CreateObject();
    cJSON *errorObject = cJSON_CreateObject();

    if (message == NULL || errorObject == NULL) {
        cJSON_Delete(message);
        cJSON_Delete(errorObject);
        return;
    }

    cJSON_AddStringToObject(message, "jsonrpc", "2.0");
    cJSON_AddItemToObject(message, "id", duplicate_id(id));
    cJSON_AddNumberToObject(errorObject, "code", code);
    cJSON_AddStringToObject(errorObject, "message", messageText != NULL ? messageText : "Unknown error");
    cJSON_AddItemToObject(message, "error", errorObject);

    send_json_message(message);
}

static void send_notification(const char *method, cJSON *params) {
    cJSON *message = cJSON_CreateObject();

    if (message == NULL) {
        cJSON_Delete(params);
        return;
    }

    cJSON_AddStringToObject(message, "jsonrpc", "2.0");
    cJSON_AddStringToObject(message, "method", method);
    if (params == NULL) {
        cJSON_AddNullToObject(message, "params");
    } else {
        cJSON_AddItemToObject(message, "params", params);
    }

    send_json_message(message);
}

static cJSON *serialize_position(SZrLspPosition position) {
    cJSON *json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    cJSON_AddNumberToObject(json, "line", position.line);
    cJSON_AddNumberToObject(json, "character", position.character);
    return json;
}

static cJSON *serialize_range(SZrLspRange range) {
    cJSON *json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    cJSON_AddItemToObject(json, "start", serialize_position(range.start));
    cJSON_AddItemToObject(json, "end", serialize_position(range.end));
    return json;
}

static cJSON *serialize_location(const SZrLspLocation *location) {
    cJSON *json;
    char *uriText;

    if (location == NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    uriText = zr_string_to_c_string(location->uri);
    if (uriText != NULL) {
        cJSON_AddStringToObject(json, "uri", uriText);
        free(uriText);
    } else {
        cJSON_AddNullToObject(json, "uri");
    }
    cJSON_AddItemToObject(json, "range", serialize_range(location->range));
    return json;
}

static cJSON *serialize_symbol_information(const SZrLspSymbolInformation *info) {
    cJSON *json;
    char *nameText;
    char *containerText;

    if (info == NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    nameText = zr_string_to_c_string(info->name);
    if (nameText != NULL) {
        cJSON_AddStringToObject(json, "name", nameText);
        free(nameText);
    }
    cJSON_AddNumberToObject(json, "kind", info->kind);
    cJSON_AddItemToObject(json, "location", serialize_location(&info->location));

    if (info->containerName != ZR_NULL) {
        containerText = zr_string_to_c_string(info->containerName);
        if (containerText != NULL) {
            cJSON_AddStringToObject(json, "containerName", containerText);
            free(containerText);
        }
    }

    return json;
}

static cJSON *serialize_diagnostic(const SZrLspDiagnostic *diagnostic) {
    cJSON *json;
    char *messageText;
    char *codeText;

    if (diagnostic == NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    cJSON_AddItemToObject(json, "range", serialize_range(diagnostic->range));
    cJSON_AddNumberToObject(json, "severity", diagnostic->severity);
    cJSON_AddStringToObject(json, "source", "zr");

    messageText = zr_string_to_c_string(diagnostic->message);
    if (messageText != NULL) {
        cJSON_AddStringToObject(json, "message", messageText);
        free(messageText);
    }

    if (diagnostic->code != ZR_NULL) {
        codeText = zr_string_to_c_string(diagnostic->code);
        if (codeText != NULL) {
            cJSON_AddStringToObject(json, "code", codeText);
            free(codeText);
        }
    }

    return json;
}

static cJSON *serialize_completion_item(const SZrLspCompletionItem *item) {
    cJSON *json;
    char *text;
    int insertTextFormat = 1;

    if (item == NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    text = zr_string_to_c_string(item->label);
    if (text != NULL) {
        cJSON_AddStringToObject(json, "label", text);
        free(text);
    }
    cJSON_AddNumberToObject(json, "kind", item->kind);

    if (item->detail != ZR_NULL) {
        text = zr_string_to_c_string(item->detail);
        if (text != NULL) {
            cJSON_AddStringToObject(json, "detail", text);
            free(text);
        }
    }

    if (item->documentation != ZR_NULL) {
        cJSON *documentation = cJSON_CreateObject();
        text = zr_string_to_c_string(item->documentation);
        if (documentation != NULL && text != NULL) {
            cJSON_AddStringToObject(documentation, "kind", "markdown");
            cJSON_AddStringToObject(documentation, "value", text);
            cJSON_AddItemToObject(json, "documentation", documentation);
        } else {
            cJSON_Delete(documentation);
        }
        free(text);
    }

    if (item->insertText != ZR_NULL) {
        text = zr_string_to_c_string(item->insertText);
        if (text != NULL) {
            cJSON_AddStringToObject(json, "insertText", text);
            free(text);
        }
    }

    if (item->insertTextFormat != ZR_NULL) {
        text = zr_string_to_c_string(item->insertTextFormat);
        if (text != NULL) {
            if (strcmp(text, "snippet") == 0) {
                insertTextFormat = 2;
            }
            free(text);
        }
    }
    cJSON_AddNumberToObject(json, "insertTextFormat", insertTextFormat);

    return json;
}

static cJSON *serialize_hover(const SZrLspHover *hover) {
    cJSON *json;
    cJSON *contents;
    char *text = NULL;

    if (hover == ZR_NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    contents = cJSON_CreateObject();
    if (contents != NULL) {
        if (hover->contents.length > 0) {
            SZrString **textPtr = (SZrString **)ZrCore_Array_Get((SZrArray *)&hover->contents, 0);
            if (textPtr != ZR_NULL && *textPtr != ZR_NULL) {
                text = zr_string_to_c_string(*textPtr);
            }
        }
        cJSON_AddStringToObject(contents, "kind", "markdown");
        cJSON_AddStringToObject(contents, "value", text != NULL ? text : "");
        cJSON_AddItemToObject(json, "contents", contents);
    }

    cJSON_AddItemToObject(json, "range", serialize_range(hover->range));
    free(text);
    return json;
}

static cJSON *serialize_document_highlight(const SZrLspDocumentHighlight *highlight) {
    cJSON *json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    cJSON_AddItemToObject(json, "range", serialize_range(highlight->range));
    cJSON_AddNumberToObject(json, "kind", highlight->kind);
    return json;
}

static cJSON *serialize_locations_array(SZrArray *locations) {
    cJSON *json = cJSON_CreateArray();
    TZrSize index;

    if (json == NULL || locations == ZR_NULL) {
        return json;
    }

    for (index = 0; index < locations->length; index++) {
        SZrLspLocation **locationPtr = (SZrLspLocation **)ZrCore_Array_Get(locations, index);
        if (locationPtr != ZR_NULL && *locationPtr != ZR_NULL) {
            cJSON_AddItemToArray(json, serialize_location(*locationPtr));
        }
    }
    return json;
}

static cJSON *serialize_symbols_array(SZrArray *symbols) {
    cJSON *json = cJSON_CreateArray();
    TZrSize index;

    if (json == NULL || symbols == ZR_NULL) {
        return json;
    }

    for (index = 0; index < symbols->length; index++) {
        SZrLspSymbolInformation **symbolPtr =
            (SZrLspSymbolInformation **)ZrCore_Array_Get(symbols, index);
        if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
            cJSON_AddItemToArray(json, serialize_symbol_information(*symbolPtr));
        }
    }
    return json;
}

static cJSON *serialize_diagnostics_array(SZrArray *diagnostics) {
    cJSON *json = cJSON_CreateArray();
    TZrSize index;

    if (json == NULL || diagnostics == ZR_NULL) {
        return json;
    }

    for (index = 0; index < diagnostics->length; index++) {
        SZrLspDiagnostic **diagnosticPtr = (SZrLspDiagnostic **)ZrCore_Array_Get(diagnostics, index);
        if (diagnosticPtr != ZR_NULL && *diagnosticPtr != ZR_NULL) {
            cJSON_AddItemToArray(json, serialize_diagnostic(*diagnosticPtr));
        }
    }
    return json;
}

static cJSON *serialize_completion_items_array(SZrArray *items) {
    cJSON *json = cJSON_CreateArray();
    TZrSize index;

    if (json == NULL || items == ZR_NULL) {
        return json;
    }

    for (index = 0; index < items->length; index++) {
        SZrLspCompletionItem **itemPtr = (SZrLspCompletionItem **)ZrCore_Array_Get(items, index);
        if (itemPtr != ZR_NULL && *itemPtr != ZR_NULL) {
            cJSON_AddItemToArray(json, serialize_completion_item(*itemPtr));
        }
    }
    return json;
}

static cJSON *serialize_highlights_array(SZrArray *highlights) {
    cJSON *json = cJSON_CreateArray();
    TZrSize index;

    if (json == NULL || highlights == ZR_NULL) {
        return json;
    }

    for (index = 0; index < highlights->length; index++) {
        SZrLspDocumentHighlight **highlightPtr =
            (SZrLspDocumentHighlight **)ZrCore_Array_Get(highlights, index);
        if (highlightPtr != ZR_NULL && *highlightPtr != ZR_NULL) {
            cJSON_AddItemToArray(json, serialize_document_highlight(*highlightPtr));
        }
    }
    return json;
}

static void free_locations_array(SZrState *state, SZrArray *locations) {
    TZrSize index;

    if (state == ZR_NULL || locations == ZR_NULL) {
        return;
    }

    for (index = 0; index < locations->length; index++) {
        SZrLspLocation **locationPtr = (SZrLspLocation **)ZrCore_Array_Get(locations, index);
        if (locationPtr != ZR_NULL && *locationPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *locationPtr, sizeof(SZrLspLocation));
        }
    }
    ZrCore_Array_Free(state, locations);
}

static void free_symbols_array(SZrState *state, SZrArray *symbols) {
    TZrSize index;

    if (state == ZR_NULL || symbols == ZR_NULL) {
        return;
    }

    for (index = 0; index < symbols->length; index++) {
        SZrLspSymbolInformation **symbolPtr =
            (SZrLspSymbolInformation **)ZrCore_Array_Get(symbols, index);
        if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *symbolPtr, sizeof(SZrLspSymbolInformation));
        }
    }
    ZrCore_Array_Free(state, symbols);
}

static void free_diagnostics_array(SZrState *state, SZrArray *diagnostics) {
    TZrSize index;

    if (state == ZR_NULL || diagnostics == ZR_NULL) {
        return;
    }

    for (index = 0; index < diagnostics->length; index++) {
        SZrLspDiagnostic **diagnosticPtr = (SZrLspDiagnostic **)ZrCore_Array_Get(diagnostics, index);
        if (diagnosticPtr != ZR_NULL && *diagnosticPtr != ZR_NULL) {
            ZrCore_Array_Free(state, &(*diagnosticPtr)->relatedInformation);
            ZrCore_Memory_RawFree(state->global, *diagnosticPtr, sizeof(SZrLspDiagnostic));
        }
    }
    ZrCore_Array_Free(state, diagnostics);
}

static void free_completion_items_array(SZrState *state, SZrArray *items) {
    TZrSize index;

    if (state == ZR_NULL || items == ZR_NULL) {
        return;
    }

    for (index = 0; index < items->length; index++) {
        SZrLspCompletionItem **itemPtr = (SZrLspCompletionItem **)ZrCore_Array_Get(items, index);
        if (itemPtr != ZR_NULL && *itemPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *itemPtr, sizeof(SZrLspCompletionItem));
        }
    }
    ZrCore_Array_Free(state, items);
}

static void free_highlights_array(SZrState *state, SZrArray *highlights) {
    TZrSize index;

    if (state == ZR_NULL || highlights == ZR_NULL) {
        return;
    }

    for (index = 0; index < highlights->length; index++) {
        SZrLspDocumentHighlight **highlightPtr =
            (SZrLspDocumentHighlight **)ZrCore_Array_Get(highlights, index);
        if (highlightPtr != ZR_NULL && *highlightPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *highlightPtr, sizeof(SZrLspDocumentHighlight));
        }
    }
    ZrCore_Array_Free(state, highlights);
}

static void free_hover(SZrState *state, SZrLspHover *hover) {
    if (state == ZR_NULL || hover == ZR_NULL) {
        return;
    }

    ZrCore_Array_Free(state, &hover->contents);
    ZrCore_Memory_RawFree(state->global, hover, sizeof(SZrLspHover));
}

static int parse_position(const cJSON *json, SZrLspPosition *outPosition) {
    const cJSON *line;
    const cJSON *character;

    if (json == NULL || outPosition == NULL) {
        return 0;
    }

    line = cJSON_GetObjectItemCaseSensitive((cJSON *)json, "line");
    character = cJSON_GetObjectItemCaseSensitive((cJSON *)json, "character");
    if (!cJSON_IsNumber(line) || !cJSON_IsNumber(character)) {
        return 0;
    }

    outPosition->line = (TZrInt32)line->valuedouble;
    outPosition->character = (TZrInt32)character->valuedouble;
    return 1;
}

static int parse_range(const cJSON *json, SZrLspRange *outRange) {
    const cJSON *start;
    const cJSON *end;

    if (json == NULL || outRange == NULL) {
        return 0;
    }

    start = cJSON_GetObjectItemCaseSensitive((cJSON *)json, "start");
    end = cJSON_GetObjectItemCaseSensitive((cJSON *)json, "end");
    if (!parse_position(start, &outRange->start) || !parse_position(end, &outRange->end)) {
        return 0;
    }

    return 1;
}

static SZrFileVersion *get_file_version_for_uri(SZrStdioServer *server, SZrString *uri) {
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

    textJson = cJSON_GetObjectItemCaseSensitive((cJSON *)change, "text");
    if (!cJSON_IsString(textJson)) {
        return NULL;
    }

    replacement = cJSON_GetStringValue(textJson);
    replacementLength = replacement != NULL ? strlen(replacement) : 0;
    rangeJson = cJSON_GetObjectItemCaseSensitive((cJSON *)change, "range");

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

static char *apply_content_changes(SZrString *uri,
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

static char *read_message_payload(size_t *outLength) {
    char headerLine[1024];
    size_t contentLength = 0;
    int sawHeader = 0;

    if (outLength == NULL) {
        return NULL;
    }
    *outLength = 0;

    while (fgets(headerLine, sizeof(headerLine), stdin) != NULL) {
        size_t lineLength = strlen(headerLine);
        sawHeader = 1;

        if (lineLength == 0 || strcmp(headerLine, "\n") == 0 || strcmp(headerLine, "\r\n") == 0) {
            break;
        }

        if (starts_with_case_insensitive(headerLine, "Content-Length:")) {
            const char *valueText = skip_spaces(headerLine + strlen("Content-Length:"));
            contentLength = (size_t)strtoul(valueText, NULL, 10);
        }
    }

    if (!sawHeader || contentLength == 0) {
        return NULL;
    }

    {
        char *payload = (char *)malloc(contentLength + 1);
        size_t totalRead = 0;

        if (payload == NULL) {
            return NULL;
        }

        while (totalRead < contentLength) {
            size_t readNow = fread(payload + totalRead, 1, contentLength - totalRead, stdin);
            if (readNow == 0) {
                free(payload);
                return NULL;
            }
            totalRead += readNow;
        }

        payload[contentLength] = '\0';
        *outLength = contentLength;
        return payload;
    }
}

static void remove_document_state(SZrStdioServer *server, SZrString *uri) {
    SZrTypeValue key;
    SZrHashKeyValuePair *pair;

    if (server == ZR_NULL || uri == ZR_NULL || server->context == ZR_NULL) {
        return;
    }

    if (server->context->parser != ZR_NULL) {
        ZrLanguageServer_IncrementalParser_RemoveFile(server->state, server->context->parser, uri);
    }

    ZrCore_Value_InitAsRawObject(server->state, &key, &uri->super);
    pair = ZrCore_HashSet_Find(server->state, &server->context->uriToAnalyzerMap, &key);
    if (pair != ZR_NULL && pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
        SZrSemanticAnalyzer *analyzer = (SZrSemanticAnalyzer *)pair->value.value.nativeObject.nativePointer;
        if (analyzer != ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(server->state, analyzer);
        }
    }
    ZrCore_HashSet_Remove(server->state, &server->context->uriToAnalyzerMap, &key);
}

static void publish_diagnostics(SZrStdioServer *server, SZrString *uri) {
    SZrArray diagnostics;
    cJSON *params;
    cJSON *diagnosticsJson;
    char *uriText;
    SZrFileVersion *fileVersion;

    if (server == ZR_NULL || uri == ZR_NULL) {
        return;
    }

    ZrCore_Array_Init(server->state, &diagnostics, sizeof(SZrLspDiagnostic *), 8);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(server->state, server->context, uri, &diagnostics)) {
        ZrCore_Array_Free(server->state, &diagnostics);
        return;
    }

    params = cJSON_CreateObject();
    diagnosticsJson = serialize_diagnostics_array(&diagnostics);
    uriText = zr_string_to_c_string(uri);
    fileVersion = get_file_version_for_uri(server, uri);

    if (params != NULL) {
        cJSON_AddStringToObject(params, "uri", uriText != NULL ? uriText : "");
        if (fileVersion != ZR_NULL) {
            cJSON_AddNumberToObject(params, "version", (double)fileVersion->version);
        }
        cJSON_AddItemToObject(params, "diagnostics", diagnosticsJson);
        send_notification("textDocument/publishDiagnostics", params);
    } else {
        cJSON_Delete(diagnosticsJson);
    }

    free(uriText);
    free_diagnostics_array(server->state, &diagnostics);
}

static cJSON *create_workspace_edit(SZrArray *locations, SZrString *newName) {
    cJSON *edit = cJSON_CreateObject();
    cJSON *changes = cJSON_CreateObject();
    TZrSize index;
    char *newNameText;

    if (edit == NULL || changes == NULL) {
        cJSON_Delete(edit);
        cJSON_Delete(changes);
        return NULL;
    }

    newNameText = zr_string_to_c_string(newName);
    if (newNameText == NULL) {
        newNameText = duplicate_c_string("");
    }

    for (index = 0; index < locations->length; index++) {
        SZrLspLocation **locationPtr = (SZrLspLocation **)ZrCore_Array_Get(locations, index);
        if (locationPtr != ZR_NULL && *locationPtr != ZR_NULL) {
            char *uriText = zr_string_to_c_string((*locationPtr)->uri);
            cJSON *uriEdits;
            cJSON *textEdit;

            if (uriText == NULL) {
                continue;
            }

            uriEdits = cJSON_GetObjectItemCaseSensitive(changes, uriText);
            if (uriEdits == NULL) {
                uriEdits = cJSON_CreateArray();
                if (uriEdits != NULL) {
                    cJSON_AddItemToObject(changes, uriText, uriEdits);
                }
            }

            textEdit = cJSON_CreateObject();
            if (uriEdits != NULL && textEdit != NULL) {
                cJSON_AddItemToObject(textEdit, "range", serialize_range((*locationPtr)->range));
                cJSON_AddStringToObject(textEdit, "newText", newNameText != NULL ? newNameText : "");
                cJSON_AddItemToArray(uriEdits, textEdit);
            } else {
                cJSON_Delete(textEdit);
            }

            free(uriText);
        }
    }

    cJSON_AddItemToObject(edit, "changes", changes);
    free(newNameText);
    return edit;
}

static void publish_empty_diagnostics(SZrStdioServer *server, SZrString *uri) {
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
        cJSON_AddStringToObject(params, "uri", uriText != NULL ? uriText : "");
        cJSON_AddItemToObject(params, "diagnostics", diagnostics);
        send_notification("textDocument/publishDiagnostics", params);
    } else {
        cJSON_Delete(params);
        cJSON_Delete(diagnostics);
    }

    free(uriText);
}

static const cJSON *get_object_item(const cJSON *json, const char *key) {
    if (json == NULL || key == NULL) {
        return NULL;
    }
    return cJSON_GetObjectItemCaseSensitive((cJSON *)json, key);
}

static TZrSize parse_size_value(const cJSON *json, TZrSize fallback) {
    if (!cJSON_IsNumber((cJSON *)json) || json->valuedouble < 0) {
        return fallback;
    }
    return (TZrSize)json->valuedouble;
}

static int get_uri_from_text_document(SZrStdioServer *server,
                                      const cJSON *params,
                                      const char **outUriText,
                                      SZrString **outUri) {
    const cJSON *textDocument;
    const cJSON *uriJson;
    const char *uriText;

    if (server == ZR_NULL || params == NULL || outUriText == NULL || outUri == NULL) {
        return 0;
    }

    textDocument = get_object_item(params, "textDocument");
    uriJson = get_object_item(textDocument, "uri");
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

static int get_uri_and_position(SZrStdioServer *server,
                                const cJSON *params,
                                const char **outUriText,
                                SZrString **outUri,
                                SZrLspPosition *outPosition) {
    const cJSON *positionJson;

    if (!get_uri_from_text_document(server, params, outUriText, outUri) || outPosition == NULL) {
        return 0;
    }

    positionJson = get_object_item(params, "position");
    return parse_position(positionJson, outPosition);
}

static int update_document_contents(SZrStdioServer *server,
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

static int handle_did_open(SZrStdioServer *server, const cJSON *params) {
    const cJSON *textDocument;
    const cJSON *textJson;
    const cJSON *versionJson;
    const char *uriText;
    SZrString *uri;
    const char *text;
    TZrSize version;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return 0;
    }

    textDocument = get_object_item(params, "textDocument");
    textJson = get_object_item(textDocument, "text");
    if (!cJSON_IsString((cJSON *)textJson)) {
        return 0;
    }

    versionJson = get_object_item(textDocument, "version");
    version = parse_size_value(versionJson, 0);
    text = cJSON_GetStringValue((cJSON *)textJson);
    if (text == NULL) {
        text = "";
    }

    return update_document_contents(server, uri, text, strlen(text), version);
}

static int handle_did_change(SZrStdioServer *server, const cJSON *params) {
    const cJSON *textDocument;
    const cJSON *versionJson;
    const cJSON *changes;
    const char *uriText;
    SZrString *uri;
    SZrFileVersion *fileVersion;
    const char *originalContent;
    size_t originalLength;
    char *updatedContent;
    size_t updatedLength = 0;
    TZrSize version;
    int success;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return 0;
    }

    textDocument = get_object_item(params, "textDocument");
    versionJson = get_object_item(textDocument, "version");
    changes = get_object_item(params, "contentChanges");
    if (!cJSON_IsArray((cJSON *)changes)) {
        return 0;
    }

    fileVersion = get_file_version_for_uri(server, uri);
    if (fileVersion != ZR_NULL && fileVersion->content != ZR_NULL) {
        originalContent = fileVersion->content;
        originalLength = (size_t)fileVersion->contentLength;
        version = parse_size_value(versionJson, fileVersion->version + 1);
    } else {
        originalContent = "";
        originalLength = 0;
        version = parse_size_value(versionJson, 0);
    }

    updatedContent = apply_content_changes(uri, originalContent, originalLength, changes, &updatedLength);
    if (updatedContent == NULL) {
        return 0;
    }

    success = update_document_contents(server, uri, updatedContent, updatedLength, version);
    free(updatedContent);
    return success;
}

static int handle_did_close(SZrStdioServer *server, const cJSON *params) {
    const char *uriText;
    SZrString *uri;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return 0;
    }

    publish_empty_diagnostics(server, uri);
    remove_document_state(server, uri);
    return 1;
}

static int handle_did_save(SZrStdioServer *server, const cJSON *params) {
    const cJSON *textJson;
    const char *uriText;
    SZrString *uri;
    SZrFileVersion *fileVersion;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return 0;
    }

    textJson = get_object_item(params, "text");
    if (cJSON_IsString((cJSON *)textJson)) {
        const char *text = cJSON_GetStringValue((cJSON *)textJson);
        fileVersion = get_file_version_for_uri(server, uri);
        return update_document_contents(
            server,
            uri,
            text != NULL ? text : "",
            text != NULL ? strlen(text) : 0,
            fileVersion != ZR_NULL ? fileVersion->version : 0
        );
    }

    publish_diagnostics(server, uri);
    return 1;
}

static cJSON *handle_completion_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray completions = {0};
    SZrLspPosition position;
    const char *uriText;
    SZrString *uri;
    cJSON *result;

    if (!get_uri_and_position(server, params, &uriText, &uri, &position)) {
        return NULL;
    }

    if (!ZrLanguageServer_Lsp_GetCompletion(server->state, server->context, uri, position, &completions)) {
        return cJSON_CreateArray();
    }

    result = serialize_completion_items_array(&completions);
    free_completion_items_array(server->state, &completions);
    return result;
}

static cJSON *handle_hover_request(SZrStdioServer *server, const cJSON *params) {
    SZrLspPosition position;
    const char *uriText;
    SZrString *uri;
    SZrLspHover *hover = ZR_NULL;
    cJSON *result;

    if (!get_uri_and_position(server, params, &uriText, &uri, &position)) {
        return NULL;
    }

    if (!ZrLanguageServer_Lsp_GetHover(server->state, server->context, uri, position, &hover) || hover == ZR_NULL) {
        return cJSON_CreateNull();
    }

    result = serialize_hover(hover);
    free_hover(server->state, hover);
    return result != NULL ? result : cJSON_CreateNull();
}

static cJSON *handle_definition_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray locations = {0};
    SZrLspPosition position;
    const char *uriText;
    SZrString *uri;
    cJSON *result;

    if (!get_uri_and_position(server, params, &uriText, &uri, &position)) {
        return NULL;
    }

    if (!ZrLanguageServer_Lsp_GetDefinition(server->state, server->context, uri, position, &locations)) {
        return cJSON_CreateArray();
    }

    result = serialize_locations_array(&locations);
    free_locations_array(server->state, &locations);
    return result;
}

static cJSON *handle_references_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray locations = {0};
    SZrLspPosition position;
    const cJSON *contextJson;
    const cJSON *includeDeclarationJson;
    const char *uriText;
    SZrString *uri;
    TZrBool includeDeclaration = ZR_FALSE;
    cJSON *result;

    if (!get_uri_and_position(server, params, &uriText, &uri, &position)) {
        return NULL;
    }

    contextJson = get_object_item(params, "context");
    includeDeclarationJson = get_object_item(contextJson, "includeDeclaration");
    if (cJSON_IsBool((cJSON *)includeDeclarationJson)) {
        includeDeclaration = cJSON_IsTrue((cJSON *)includeDeclarationJson) ? ZR_TRUE : ZR_FALSE;
    }

    if (!ZrLanguageServer_Lsp_FindReferences(
            server->state,
            server->context,
            uri,
            position,
            includeDeclaration,
            &locations)) {
        return cJSON_CreateArray();
    }

    result = serialize_locations_array(&locations);
    free_locations_array(server->state, &locations);
    return result;
}

static cJSON *handle_document_symbols_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray symbols = {0};
    const char *uriText;
    SZrString *uri;
    cJSON *result;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return NULL;
    }

    if (!ZrLanguageServer_Lsp_GetDocumentSymbols(server->state, server->context, uri, &symbols)) {
        return cJSON_CreateArray();
    }

    result = serialize_symbols_array(&symbols);
    free_symbols_array(server->state, &symbols);
    return result;
}

static cJSON *handle_workspace_symbols_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray symbols = {0};
    const cJSON *queryJson;
    const char *queryText = "";
    SZrString *query;
    cJSON *result;

    if (server == ZR_NULL || params == NULL) {
        return NULL;
    }

    queryJson = get_object_item(params, "query");
    if (cJSON_IsString((cJSON *)queryJson)) {
        const char *text = cJSON_GetStringValue((cJSON *)queryJson);
        if (text != NULL) {
            queryText = text;
        }
    }

    query = ZrCore_String_Create(server->state, queryText, (TZrSize)strlen(queryText));
    if (query == ZR_NULL) {
        return NULL;
    }

    if (!ZrLanguageServer_Lsp_GetWorkspaceSymbols(server->state, server->context, query, &symbols)) {
        return cJSON_CreateArray();
    }

    result = serialize_symbols_array(&symbols);
    free_symbols_array(server->state, &symbols);
    return result;
}

static cJSON *handle_document_highlights_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray highlights = {0};
    SZrLspPosition position;
    const char *uriText;
    SZrString *uri;
    cJSON *result;

    if (!get_uri_and_position(server, params, &uriText, &uri, &position)) {
        return NULL;
    }

    if (!ZrLanguageServer_Lsp_GetDocumentHighlights(server->state, server->context, uri, position, &highlights)) {
        return cJSON_CreateArray();
    }

    result = serialize_highlights_array(&highlights);
    free_highlights_array(server->state, &highlights);
    return result;
}

static cJSON *handle_prepare_rename_request(SZrStdioServer *server, const cJSON *params) {
    SZrLspPosition position;
    const char *uriText;
    SZrString *uri;
    SZrLspRange range;
    SZrString *placeholder = ZR_NULL;
    cJSON *result;
    char *placeholderText;

    if (!get_uri_and_position(server, params, &uriText, &uri, &position)) {
        return NULL;
    }

    if (!ZrLanguageServer_Lsp_PrepareRename(
            server->state,
            server->context,
            uri,
            position,
            &range,
            &placeholder)) {
        return cJSON_CreateNull();
    }

    result = cJSON_CreateObject();
    if (result == NULL) {
        return cJSON_CreateNull();
    }

    placeholderText = zr_string_to_c_string(placeholder);
    cJSON_AddItemToObject(result, "range", serialize_range(range));
    cJSON_AddStringToObject(result, "placeholder", placeholderText != NULL ? placeholderText : "");
    free(placeholderText);
    return result;
}

static cJSON *handle_rename_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray locations = {0};
    SZrLspPosition position;
    const cJSON *newNameJson;
    const char *newNameText;
    const char *uriText;
    SZrString *uri;
    SZrString *newName;
    cJSON *result;

    if (!get_uri_and_position(server, params, &uriText, &uri, &position)) {
        return NULL;
    }

    newNameJson = get_object_item(params, "newName");
    if (!cJSON_IsString((cJSON *)newNameJson)) {
        return NULL;
    }

    newNameText = cJSON_GetStringValue((cJSON *)newNameJson);
    if (newNameText == NULL) {
        return NULL;
    }

    newName = ZrCore_String_Create(server->state, newNameText, (TZrSize)strlen(newNameText));
    if (newName == ZR_NULL) {
        return NULL;
    }

    if (!ZrLanguageServer_Lsp_Rename(
            server->state,
            server->context,
            uri,
            position,
            newName,
            &locations)) {
        return cJSON_CreateNull();
    }

    result = create_workspace_edit(&locations, newName);
    free_locations_array(server->state, &locations);
    return result != NULL ? result : cJSON_CreateNull();
}

static cJSON *handle_initialize_request(void) {
    cJSON *result = cJSON_CreateObject();
    cJSON *capabilities = cJSON_CreateObject();
    cJSON *textDocumentSync = cJSON_CreateObject();
    cJSON *completionProvider = cJSON_CreateObject();
    cJSON *renameProvider = cJSON_CreateObject();
    cJSON *saveOptions = cJSON_CreateObject();
    cJSON *triggerCharacters = cJSON_CreateArray();
    cJSON *serverInfo = cJSON_CreateObject();
    cJSON *workspace = cJSON_CreateObject();
    cJSON *workspaceFolders = cJSON_CreateObject();

    if (result == NULL || capabilities == NULL || textDocumentSync == NULL ||
        completionProvider == NULL || renameProvider == NULL || saveOptions == NULL ||
        triggerCharacters == NULL || serverInfo == NULL || workspace == NULL ||
        workspaceFolders == NULL) {
        cJSON_Delete(result);
        cJSON_Delete(capabilities);
        cJSON_Delete(textDocumentSync);
        cJSON_Delete(completionProvider);
        cJSON_Delete(renameProvider);
        cJSON_Delete(saveOptions);
        cJSON_Delete(triggerCharacters);
        cJSON_Delete(serverInfo);
        cJSON_Delete(workspace);
        cJSON_Delete(workspaceFolders);
        return NULL;
    }

    cJSON_AddBoolToObject(textDocumentSync, "openClose", 1);
    cJSON_AddNumberToObject(textDocumentSync, "change", 2);
    cJSON_AddBoolToObject(saveOptions, "includeText", 0);
    cJSON_AddItemToObject(textDocumentSync, "save", saveOptions);

    cJSON_AddItemToArray(triggerCharacters, cJSON_CreateString("."));
    cJSON_AddItemToArray(triggerCharacters, cJSON_CreateString(":"));
    cJSON_AddBoolToObject(completionProvider, "resolveProvider", 0);
    cJSON_AddItemToObject(completionProvider, "triggerCharacters", triggerCharacters);

    cJSON_AddBoolToObject(renameProvider, "prepareProvider", 1);

    cJSON_AddItemToObject(capabilities, "textDocumentSync", textDocumentSync);
    cJSON_AddItemToObject(capabilities, "completionProvider", completionProvider);
    cJSON_AddBoolToObject(capabilities, "hoverProvider", 1);
    cJSON_AddBoolToObject(capabilities, "definitionProvider", 1);
    cJSON_AddBoolToObject(capabilities, "referencesProvider", 1);
    cJSON_AddItemToObject(capabilities, "renameProvider", renameProvider);
    cJSON_AddBoolToObject(capabilities, "documentSymbolProvider", 1);
    cJSON_AddBoolToObject(capabilities, "workspaceSymbolProvider", 1);
    cJSON_AddBoolToObject(capabilities, "documentHighlightProvider", 1);
    cJSON_AddBoolToObject(workspaceFolders, "supported", 1);
    cJSON_AddBoolToObject(workspaceFolders, "changeNotifications", 1);
    cJSON_AddItemToObject(workspace, "workspaceFolders", workspaceFolders);
    cJSON_AddItemToObject(capabilities, "workspace", workspace);

    cJSON_AddStringToObject(serverInfo, "name", "zr_vm_language_server_stdio");
    cJSON_AddStringToObject(serverInfo, "version", "0.0.1");

    cJSON_AddItemToObject(result, "capabilities", capabilities);
    cJSON_AddItemToObject(result, "serverInfo", serverInfo);
    return result;
}

static void handle_request_message(SZrStdioServer *server,
                                   const cJSON *id,
                                   const char *method,
                                   const cJSON *params) {
    cJSON *result = NULL;

    if (server == ZR_NULL || id == NULL || method == NULL) {
        return;
    }

    if (strcmp(method, "initialize") == 0) {
        result = handle_initialize_request();
        send_result_response(id, result != NULL ? result : cJSON_CreateNull());
        return;
    }

    if (strcmp(method, "shutdown") == 0) {
        server->shutdownRequested = ZR_TRUE;
        send_result_response(id, NULL);
        return;
    }

    if (strcmp(method, "textDocument/completion") == 0) {
        result = handle_completion_request(server, params);
    } else if (strcmp(method, "textDocument/hover") == 0) {
        result = handle_hover_request(server, params);
    } else if (strcmp(method, "textDocument/definition") == 0) {
        result = handle_definition_request(server, params);
    } else if (strcmp(method, "textDocument/references") == 0) {
        result = handle_references_request(server, params);
    } else if (strcmp(method, "textDocument/documentSymbol") == 0) {
        result = handle_document_symbols_request(server, params);
    } else if (strcmp(method, "workspace/symbol") == 0) {
        result = handle_workspace_symbols_request(server, params);
    } else if (strcmp(method, "textDocument/documentHighlight") == 0) {
        result = handle_document_highlights_request(server, params);
    } else if (strcmp(method, "textDocument/prepareRename") == 0) {
        result = handle_prepare_rename_request(server, params);
    } else if (strcmp(method, "textDocument/rename") == 0) {
        result = handle_rename_request(server, params);
    } else {
        send_error_response(id, -32601, "Method not found");
        return;
    }

    if (result == NULL) {
        send_error_response(id, -32602, "Invalid params");
    } else {
        send_result_response(id, result);
    }
}

static void handle_notification_message(SZrStdioServer *server,
                                        const char *method,
                                        const cJSON *params,
                                        int *outShouldExit,
                                        int *outExitCode) {
    if (outShouldExit != NULL) {
        *outShouldExit = 0;
    }
    if (outExitCode != NULL) {
        *outExitCode = 0;
    }

    if (server == ZR_NULL || method == NULL) {
        return;
    }

    if (strcmp(method, "initialized") == 0 ||
        strcmp(method, "workspace/didChangeConfiguration") == 0 ||
        strcmp(method, "workspace/didChangeWatchedFiles") == 0 ||
        strcmp(method, "workspace/didChangeWorkspaceFolders") == 0 ||
        strcmp(method, "$/cancelRequest") == 0) {
        return;
    }

    if (strcmp(method, "textDocument/didOpen") == 0) {
        handle_did_open(server, params);
    } else if (strcmp(method, "textDocument/didChange") == 0) {
        handle_did_change(server, params);
    } else if (strcmp(method, "textDocument/didClose") == 0) {
        handle_did_close(server, params);
    } else if (strcmp(method, "textDocument/didSave") == 0) {
        handle_did_save(server, params);
    } else if (strcmp(method, "exit") == 0) {
        if (outShouldExit != NULL) {
            *outShouldExit = 1;
        }
        if (outExitCode != NULL) {
            *outExitCode = server->shutdownRequested ? 0 : 1;
        }
    }
}

int main(void) {
    SZrStdioServer server;
    SZrCallbackGlobal callbacks = {0};
    char *payload;
    size_t payloadLength;
    int exitCode = 1;

    memset(&server, 0, sizeof(server));

#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    server.global = ZrCore_GlobalState_New(stdio_allocator, ZR_NULL, 0, &callbacks);
    if (server.global == ZR_NULL) {
        return 1;
    }

    server.state = server.global->mainThreadState;
    if (server.state == ZR_NULL) {
        ZrCore_GlobalState_Free(server.global);
        return 1;
    }

    ZrCore_GlobalState_InitRegistry(server.state, server.global);
    server.context = ZrLanguageServer_LspContext_New(server.state);
    if (server.context == ZR_NULL) {
        ZrCore_GlobalState_Free(server.global);
        return 1;
    }

    server.shutdownRequested = ZR_FALSE;

    while ((payload = read_message_payload(&payloadLength)) != NULL) {
        cJSON *message = cJSON_ParseWithLength(payload, payloadLength);
        const cJSON *id;
        const cJSON *methodJson;
        const cJSON *params;
        const char *method;
        int shouldExit = 0;
        int notificationExitCode = 0;

        free(payload);

        if (message == NULL) {
            send_error_response(NULL, -32700, "Parse error");
            continue;
        }

        methodJson = get_object_item(message, "method");
        if (!cJSON_IsString((cJSON *)methodJson)) {
            id = get_object_item(message, "id");
            send_error_response(id, -32600, "Invalid Request");
            cJSON_Delete(message);
            continue;
        }

        method = cJSON_GetStringValue((cJSON *)methodJson);
        id = get_object_item(message, "id");
        params = get_object_item(message, "params");

        if (id != NULL) {
            handle_request_message(&server, id, method, params);
        } else {
            handle_notification_message(&server, method, params, &shouldExit, &notificationExitCode);
            if (shouldExit) {
                exitCode = notificationExitCode;
                cJSON_Delete(message);
                break;
            }
        }

        cJSON_Delete(message);
    }

    if (server.shutdownRequested && exitCode != 0) {
        exitCode = 0;
    }

    ZrLanguageServer_LspContext_Free(server.state, server.context);
    free_uri_cache(&server.uriCache);
    ZrCore_GlobalState_Free(server.global);
    return exitCode;
}
