#include "zr_vm_language_server_stdio_internal.h"

static cJSON *duplicate_id(const cJSON *id) {
    if (id == NULL) {
        return cJSON_CreateNull();
    }
    return cJSON_Duplicate(id, 1);
}

static void stdio_request_input_lock(SZrStdioRequestInputState *input) {
#ifdef _WIN32
    EnterCriticalSection(&input->lock);
#else
    pthread_mutex_lock(&input->lock);
#endif
}

static void stdio_request_input_unlock(SZrStdioRequestInputState *input) {
#ifdef _WIN32
    LeaveCriticalSection(&input->lock);
#else
    pthread_mutex_unlock(&input->lock);
#endif
}

static void stdio_request_input_signal(SZrStdioRequestInputState *input) {
#ifdef _WIN32
    WakeConditionVariable(&input->messageAvailable);
#else
    pthread_cond_signal(&input->messageAvailable);
#endif
}

static void stdio_request_input_broadcast(SZrStdioRequestInputState *input) {
#ifdef _WIN32
    WakeAllConditionVariable(&input->messageAvailable);
#else
    pthread_cond_broadcast(&input->messageAvailable);
#endif
}

static void stdio_request_input_wait(SZrStdioRequestInputState *input) {
#ifdef _WIN32
    SleepConditionVariableCS(&input->messageAvailable, &input->lock, INFINITE);
#else
    pthread_cond_wait(&input->messageAvailable, &input->lock);
#endif
}

static char *stdio_request_id_key(const cJSON *id) {
    if (id == NULL) {
        return NULL;
    }
    return cJSON_PrintUnformatted(id);
}

static SZrStdioRequestCancellation *stdio_request_find_locked(SZrStdioRequestInputState *input,
                                                               const char *idKey) {
    SZrStdioRequestCancellation *current;

    if (input == NULL || idKey == NULL) {
        return NULL;
    }

    current = input->requests;
    while (current != NULL) {
        if (strcmp(current->idKey, idKey) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

static void stdio_request_register(SZrStdioServer *server, const cJSON *id) {
    SZrStdioRequestInputState *input;
    SZrStdioRequestCancellation *request;
    char *idKey;

    if (server == NULL || id == NULL) {
        return;
    }

    idKey = stdio_request_id_key(id);
    if (idKey == NULL) {
        return;
    }

    input = &server->requestInput;
    stdio_request_input_lock(input);
    if (stdio_request_find_locked(input, idKey) != NULL) {
        stdio_request_input_unlock(input);
        free(idKey);
        return;
    }

    request = (SZrStdioRequestCancellation *)calloc(1, sizeof(SZrStdioRequestCancellation));
    if (request != NULL) {
        request->idKey = idKey;
        request->next = input->requests;
        input->requests = request;
        idKey = NULL;
    }
    stdio_request_input_unlock(input);
    free(idKey);
}

static void stdio_request_cancel(SZrStdioServer *server, const cJSON *id) {
    SZrStdioRequestInputState *input;
    SZrStdioRequestCancellation *request;
    char *idKey;

    if (server == NULL || id == NULL) {
        return;
    }

    idKey = stdio_request_id_key(id);
    if (idKey == NULL) {
        return;
    }

    input = &server->requestInput;
    stdio_request_input_lock(input);
    request = stdio_request_find_locked(input, idKey);
    if (request != NULL) {
        request->cancelled = ZR_TRUE;
    }
    stdio_request_input_unlock(input);
    free(idKey);
}

static void stdio_request_enqueue(SZrStdioServer *server, cJSON *message, TZrBool isParseError) {
    SZrStdioInboundMessage *inbound;
    SZrStdioRequestInputState *input;

    if (server == NULL) {
        cJSON_Delete(message);
        return;
    }

    inbound = (SZrStdioInboundMessage *)calloc(1, sizeof(SZrStdioInboundMessage));
    if (inbound == NULL) {
        cJSON_Delete(message);
        return;
    }

    inbound->message = message;
    inbound->isParseError = isParseError;
    input = &server->requestInput;
    stdio_request_input_lock(input);
    if (input->tail != NULL) {
        input->tail->next = inbound;
    } else {
        input->head = inbound;
    }
    input->tail = inbound;
    stdio_request_input_signal(input);
    stdio_request_input_unlock(input);
}

static void stdio_request_handle_input_message(SZrStdioServer *server, cJSON *message) {
    const cJSON *methodJson;
    const cJSON *params;
    const cJSON *id;
    const char *method;

    if (message == NULL) {
        stdio_request_enqueue(server, NULL, ZR_TRUE);
        return;
    }

    methodJson = cJSON_GetObjectItemCaseSensitive(message, ZR_LSP_JSON_RPC_FIELD_METHOD);
    method = cJSON_IsString((cJSON *)methodJson) ? cJSON_GetStringValue((cJSON *)methodJson) : NULL;
    params = cJSON_GetObjectItemCaseSensitive(message, ZR_LSP_JSON_RPC_FIELD_PARAMS);
    if (method != NULL && strcmp(method, ZR_LSP_METHOD_CANCEL_REQUEST) == 0) {
        id = cJSON_GetObjectItemCaseSensitive(params, ZR_LSP_JSON_RPC_FIELD_ID);
        stdio_request_cancel(server, id);
        cJSON_Delete(message);
        return;
    }

    id = cJSON_GetObjectItemCaseSensitive(message, ZR_LSP_JSON_RPC_FIELD_ID);
    if (id != NULL) {
        stdio_request_register(server, id);
    }
    stdio_request_enqueue(server, message, ZR_FALSE);
}

static void stdio_request_read_loop(SZrStdioServer *server) {
    char *payload;
    size_t payloadLength;

    while ((payload = read_message_payload(&payloadLength)) != NULL) {
        cJSON *message = cJSON_ParseWithLength(payload, payloadLength);
        free(payload);
        stdio_request_handle_input_message(server, message);
    }

    stdio_request_input_lock(&server->requestInput);
    server->requestInput.inputClosed = ZR_TRUE;
    stdio_request_input_broadcast(&server->requestInput);
    stdio_request_input_unlock(&server->requestInput);
}

#ifdef _WIN32
static DWORD WINAPI stdio_request_reader_thread(void *userData) {
    stdio_request_read_loop((SZrStdioServer *)userData);
    return 0;
}
#else
static void *stdio_request_reader_thread(void *userData) {
    stdio_request_read_loop((SZrStdioServer *)userData);
    return NULL;
}
#endif

TZrBool ZrLanguageServer_StdioRequestInput_Init(SZrStdioServer *server) {
    SZrStdioRequestInputState *input;

    if (server == NULL) {
        return ZR_FALSE;
    }

    input = &server->requestInput;
#ifdef _WIN32
    InitializeCriticalSection(&input->lock);
    InitializeConditionVariable(&input->messageAvailable);
#else
    if (pthread_mutex_init(&input->lock, NULL) != 0) {
        return ZR_FALSE;
    }
    if (pthread_cond_init(&input->messageAvailable, NULL) != 0) {
        pthread_mutex_destroy(&input->lock);
        return ZR_FALSE;
    }
#endif
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_StdioRequestInput_Start(SZrStdioServer *server) {
#ifdef _WIN32
    HANDLE threadHandle;
#else
    pthread_t thread;
#endif

    if (server == NULL) {
        return ZR_FALSE;
    }

#ifdef _WIN32
    threadHandle = CreateThread(NULL, 0, stdio_request_reader_thread, server, 0, NULL);
    if (threadHandle == NULL) {
        return ZR_FALSE;
    }
    CloseHandle(threadHandle);
#else
    if (pthread_create(&thread, NULL, stdio_request_reader_thread, server) != 0) {
        return ZR_FALSE;
    }
    pthread_detach(thread);
#endif
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_StdioRequestInput_Take(SZrStdioServer *server,
                                                 cJSON **outMessage,
                                                 TZrBool *outIsParseError) {
    SZrStdioRequestInputState *input;
    SZrStdioInboundMessage *inbound;

    if (outMessage != NULL) {
        *outMessage = NULL;
    }
    if (outIsParseError != NULL) {
        *outIsParseError = ZR_FALSE;
    }
    if (server == NULL || outMessage == NULL || outIsParseError == NULL) {
        return ZR_FALSE;
    }

    input = &server->requestInput;
    stdio_request_input_lock(input);
    while (input->head == NULL && !input->inputClosed) {
        stdio_request_input_wait(input);
    }
    inbound = input->head;
    if (inbound == NULL) {
        stdio_request_input_unlock(input);
        return ZR_FALSE;
    }
    input->head = inbound->next;
    if (input->head == NULL) {
        input->tail = NULL;
    }
    stdio_request_input_unlock(input);

    *outMessage = inbound->message;
    *outIsParseError = inbound->isParseError;
    free(inbound);
    return ZR_TRUE;
}

void ZrLanguageServer_StdioRequestInput_Activate(SZrStdioServer *server, const cJSON *id) {
    char *idKey;

    if (server == NULL) {
        return;
    }

    idKey = stdio_request_id_key(id);
    stdio_request_input_lock(&server->requestInput);
    free(server->activeRequestIdKey);
    server->activeRequestIdKey = idKey;
    stdio_request_input_unlock(&server->requestInput);
}

TZrBool ZrLanguageServer_StdioRequestInput_IsActiveCancelled(SZrStdioServer *server) {
    SZrStdioRequestCancellation *request;
    TZrBool cancelled = ZR_FALSE;

    if (server == NULL) {
        return ZR_FALSE;
    }

    stdio_request_input_lock(&server->requestInput);
    request = stdio_request_find_locked(&server->requestInput, server->activeRequestIdKey);
    if (request != NULL) {
        cancelled = request->cancelled;
    }
    stdio_request_input_unlock(&server->requestInput);
    return cancelled;
}

void ZrLanguageServer_StdioRequestInput_Complete(SZrStdioServer *server, const cJSON *id) {
    SZrStdioRequestInputState *input;
    SZrStdioRequestCancellation **slot;
    SZrStdioRequestCancellation *request;
    char *idKey;

    if (server == NULL) {
        return;
    }

    idKey = stdio_request_id_key(id);
    input = &server->requestInput;
    stdio_request_input_lock(input);
    if (idKey != NULL) {
        slot = &input->requests;
        while (*slot != NULL && strcmp((*slot)->idKey, idKey) != 0) {
            slot = &(*slot)->next;
        }
        if (*slot != NULL) {
            request = *slot;
            *slot = request->next;
            free(request->idKey);
            free(request);
        }
    }
    free(server->activeRequestIdKey);
    server->activeRequestIdKey = NULL;
    stdio_request_input_unlock(input);
    free(idKey);
}

void send_json_message(cJSON *message) {
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
    fprintf(stdout, "%s %zu\r\n\r\n", ZR_LSP_STDIO_CONTENT_LENGTH_HEADER_PREFIX, payloadLength);
    fwrite(payload, 1, payloadLength, stdout);
    fflush(stdout);

    free(payload);
    cJSON_Delete(message);
}

void send_result_response(const cJSON *id, cJSON *result) {
    cJSON *message = cJSON_CreateObject();

    if (message == NULL) {
        cJSON_Delete(result);
        return;
    }

    cJSON_AddStringToObject(message, ZR_LSP_JSON_RPC_FIELD_JSONRPC, ZR_LSP_JSON_RPC_VERSION);
    cJSON_AddItemToObject(message, ZR_LSP_JSON_RPC_FIELD_ID, duplicate_id(id));
    if (result == NULL) {
        cJSON_AddNullToObject(message, ZR_LSP_JSON_RPC_FIELD_RESULT);
    } else {
        cJSON_AddItemToObject(message, ZR_LSP_JSON_RPC_FIELD_RESULT, result);
    }

    send_json_message(message);
}

void send_error_response(const cJSON *id, int code, const char *messageText) {
    cJSON *message = cJSON_CreateObject();
    cJSON *errorObject = cJSON_CreateObject();

    if (message == NULL || errorObject == NULL) {
        cJSON_Delete(message);
        cJSON_Delete(errorObject);
        return;
    }

    cJSON_AddStringToObject(message, ZR_LSP_JSON_RPC_FIELD_JSONRPC, ZR_LSP_JSON_RPC_VERSION);
    cJSON_AddItemToObject(message, ZR_LSP_JSON_RPC_FIELD_ID, duplicate_id(id));
    cJSON_AddNumberToObject(errorObject, ZR_LSP_JSON_RPC_FIELD_CODE, code);
    cJSON_AddStringToObject(
            errorObject, ZR_LSP_JSON_RPC_FIELD_MESSAGE, messageText != NULL ? messageText : "Unknown error");
    cJSON_AddItemToObject(message, ZR_LSP_JSON_RPC_FIELD_ERROR, errorObject);

    send_json_message(message);
}

void send_notification(const char *method, cJSON *params) {
    cJSON *message = cJSON_CreateObject();

    if (message == NULL) {
        cJSON_Delete(params);
        return;
    }

    cJSON_AddStringToObject(message, ZR_LSP_JSON_RPC_FIELD_JSONRPC, ZR_LSP_JSON_RPC_VERSION);
    cJSON_AddStringToObject(message, ZR_LSP_JSON_RPC_FIELD_METHOD, method);
    if (params == NULL) {
        cJSON_AddNullToObject(message, ZR_LSP_JSON_RPC_FIELD_PARAMS);
    } else {
        cJSON_AddItemToObject(message, ZR_LSP_JSON_RPC_FIELD_PARAMS, params);
    }

    send_json_message(message);
}

char *read_message_payload(size_t *outLength) {
    char headerLine[ZR_LSP_STDIO_HEADER_BUFFER_LENGTH];
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

        if (starts_with_case_insensitive(headerLine, ZR_LSP_STDIO_CONTENT_LENGTH_HEADER_PREFIX)) {
            const char *valueText =
                    skip_spaces(headerLine + strlen(ZR_LSP_STDIO_CONTENT_LENGTH_HEADER_PREFIX));
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
