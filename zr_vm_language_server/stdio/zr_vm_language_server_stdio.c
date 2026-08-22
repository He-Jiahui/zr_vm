#include "zr_vm_language_server_stdio_internal.h"

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

int main(void) {
    SZrStdioServerOptions options = {0};
    SZrStdioServer *server;
    int exitCode = 1;

#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    options.input = stdin;
    server = ZrLanguageServer_StdioServer_New(&options);
    if (server == ZR_NULL) {
        return 1;
    }
    if (!ZrLanguageServer_StdioServer_Start(server)) {
        ZrLanguageServer_StdioServer_Free(server);
        return 1;
    }

    for (;;) {
        cJSON *message = NULL;
        TZrBool isParseError = ZR_FALSE;
        EZrStdioRequestReservation requestReservation = ZR_STDIO_REQUEST_RESERVATION_NONE;
        SZrJsonRpcEnvelope envelope;
        EZrJsonRpcEnvelopeStatus envelopeStatus;
        const cJSON *errorId = ZR_NULL;
        int shouldExit = 0;
        int notificationExitCode = 0;

        if (!ZrLanguageServer_StdioRequestInput_Take(server,
                                                      &message,
                                                      &isParseError,
                                                      &requestReservation)) {
            break;
        }

        if (isParseError) {
            send_error_response(NULL, ZR_LSP_JSON_RPC_PARSE_ERROR_CODE, "Parse error");
            continue;
        }

        envelopeStatus = ZrLanguageServer_StdioJsonRpc_ParseEnvelope(message, &envelope, &errorId);
        if (envelopeStatus != ZR_JSON_RPC_ENVELOPE_OK) {
            if (envelopeStatus == ZR_JSON_RPC_ENVELOPE_INVALID_PARAMS && envelope.isNotification) {
                fprintf(stderr, "Ignoring invalid JSON-RPC notification params for %s\n",
                        envelope.method != ZR_NULL ? envelope.method : "<unknown>");
            } else {
                send_error_response(errorId,
                                    envelopeStatus == ZR_JSON_RPC_ENVELOPE_INVALID_PARAMS
                                            ? ZR_LSP_JSON_RPC_INVALID_PARAMS_CODE
                                            : ZR_LSP_JSON_RPC_INVALID_REQUEST_CODE,
                                    envelopeStatus == ZR_JSON_RPC_ENVELOPE_INVALID_PARAMS
                                            ? "Invalid params"
                                            : "Invalid Request");
            }
            cJSON_Delete(message);
            continue;
        }

        ZrLanguageServer_StdioTrace_Log(server,
                                        "inbound",
                                        envelope.isRequest ? "request" : "notification",
                                        envelope.method,
                                        envelope.isNotification);

        if (envelope.isRequest) {
            if (requestReservation == ZR_STDIO_REQUEST_RESERVATION_DUPLICATE) {
                send_error_response(envelope.id,
                                    ZR_LSP_JSON_RPC_INVALID_REQUEST_CODE,
                                    "Invalid Request");
                ZrLanguageServer_StdioTrace_Log(server,
                                                "outbound",
                                                "response",
                                                envelope.method,
                                                ZR_FALSE);
                cJSON_Delete(message);
                continue;
            }
            if (requestReservation != ZR_STDIO_REQUEST_RESERVATION_ACCEPTED) {
                send_error_response(envelope.id,
                                    ZR_LSP_JSON_RPC_INTERNAL_ERROR_CODE,
                                    "Internal error");
                ZrLanguageServer_StdioTrace_Log(server,
                                                "outbound",
                                                "response",
                                                envelope.method,
                                                ZR_FALSE);
                cJSON_Delete(message);
                continue;
            }
            ZrLanguageServer_StdioRequestInput_Activate(server, envelope.id);
            ZrLanguageServer_StdioTrace_Log(server,
                                            "outbound",
                                            "response",
                                            envelope.method,
                                            ZR_FALSE);
            handle_request_message(server, envelope.id, envelope.method, envelope.params);
            ZrLanguageServer_StdioRequestInput_Complete(server, envelope.id);
        } else {
            handle_notification_message(server,
                                        envelope.method,
                                        envelope.params,
                                        &shouldExit,
                                        &notificationExitCode);
            if (shouldExit) {
                exitCode = notificationExitCode;
                cJSON_Delete(message);
                break;
            }
        }

        cJSON_Delete(message);
    }

    if (ZrLanguageServer_StdioLifecycle_IsShutdown(&server->lifecycle) && exitCode != 0) {
        exitCode = 0;
    }

    ZrLanguageServer_StdioServer_Free(server);
    fflush(stdout);
    fflush(stderr);
    return exitCode;
}
