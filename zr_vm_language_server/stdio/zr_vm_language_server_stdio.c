#include "zr_vm_language_server_stdio_internal.h"

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

int starts_with_case_insensitive(const char *text, const char *prefix) {
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

const char *skip_spaces(const char *text) {
    while (text != NULL && (*text == ' ' || *text == '\t')) {
        text++;
    }
    return text;
}

char *duplicate_string_range(const char *text, size_t length) {
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

char *duplicate_c_string(const char *text) {
    if (text == NULL) {
        return NULL;
    }
    return duplicate_string_range(text, strlen(text));
}

char *zr_string_to_c_string(SZrString *value) {
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

SZrString *server_get_cached_uri(SZrStdioServer *server, const char *uriText) {
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
        size_t newCapacity = server->uriCache.capacity == 0
                                     ? ZR_LSP_ARRAY_INITIAL_CAPACITY
                                     : server->uriCache.capacity * ZR_LSP_DYNAMIC_CAPACITY_GROWTH_FACTOR;
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
        ZrCore_String_Create(server->state, (TZrNativeString)uriText, (TZrSize)strlen(uriText));
    if (server->uriCache.items[server->uriCache.count].value == ZR_NULL) {
        free(server->uriCache.items[server->uriCache.count].text);
        server->uriCache.items[server->uriCache.count].text = NULL;
        return ZR_NULL;
    }

    server->uriCache.count++;
    return server->uriCache.items[server->uriCache.count - 1].value;
}

void free_uri_cache(SZrUriCache *cache) {
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
            send_error_response(NULL, ZR_LSP_JSON_RPC_PARSE_ERROR_CODE, "Parse error");
            continue;
        }

        methodJson = get_object_item(message, ZR_LSP_JSON_RPC_FIELD_METHOD);
        if (!cJSON_IsString((cJSON *)methodJson)) {
            id = get_object_item(message, ZR_LSP_JSON_RPC_FIELD_ID);
            send_error_response(id, ZR_LSP_JSON_RPC_INVALID_REQUEST_CODE, "Invalid Request");
            cJSON_Delete(message);
            continue;
        }

        method = cJSON_GetStringValue((cJSON *)methodJson);
        id = get_object_item(message, ZR_LSP_JSON_RPC_FIELD_ID);
        params = get_object_item(message, ZR_LSP_JSON_RPC_FIELD_PARAMS);

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

    /*
     * The stdio server currently serves as a process boundary: once it receives EOF/exit,
     * the entire address space is about to be reclaimed by the OS. Manual teardown has been
     * observed to trigger access violations on shutdown, so keep exit reliable by flushing the
     * transports and letting process termination reclaim runtime state.
     */
    fflush(stdout);
    fflush(stderr);
    return exitCode;
}
