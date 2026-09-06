#include "zr_vm_language_server_stdio_internal.h"

static cJSON *duplicate_id(const cJSON *id) {
    char number[64];
    int length;

    if (id == NULL) {
        return cJSON_CreateNull();
    }
    if (cJSON_IsNumber((cJSON *)id)) {
        length = snprintf(number, sizeof(number), "%.17g", id->valuedouble);
        if (length > 0 && (size_t)length < sizeof(number)) {
            return cJSON_CreateRaw(number);
        }
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

static void stdio_request_enqueue(SZrStdioServer *server,
                                  cJSON *message,
                                  TZrBool isParseError,
                                  EZrStdioRequestReservation requestReservation) {
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
    inbound->requestReservation = requestReservation;
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

static TZrBool stdio_request_handle_input_message(SZrStdioServer *server, cJSON *message) {
    SZrJsonRpcEnvelope envelope;
    const cJSON *errorId = ZR_NULL;
    EZrStdioRequestReservation requestReservation = ZR_STDIO_REQUEST_RESERVATION_NONE;
    TZrBool shouldStopReader = ZR_FALSE;

    if (message == NULL) {
        stdio_request_enqueue(server,
                              NULL,
                              ZR_TRUE,
                              ZR_STDIO_REQUEST_RESERVATION_NONE);
        return ZR_FALSE;
    }

    if (ZrLanguageServer_StdioJsonRpc_ParseEnvelope(message, &envelope, &errorId) ==
        ZR_JSON_RPC_ENVELOPE_OK) {
        if (envelope.isNotification && strcmp(envelope.method, ZR_LSP_METHOD_CANCEL_REQUEST) == 0) {
            const cJSON *id = cJSON_GetObjectItemCaseSensitive(envelope.params,
                                                                 ZR_LSP_JSON_RPC_FIELD_ID);
            ZrLanguageServer_StdioRequestRegistry_Cancel(server->requestRegistry, id);
            cJSON_Delete(message);
            return ZR_FALSE;
        }
        if (envelope.isRequest) {
            requestReservation = ZrLanguageServer_StdioRequestRegistry_Reserve(server->requestRegistry,
                                                                                  envelope.id);
        } else if (envelope.isNotification && strcmp(envelope.method, ZR_LSP_METHOD_EXIT) == 0) {
            shouldStopReader = ZR_TRUE;
        }
    }
    stdio_request_enqueue(server, message, ZR_FALSE, requestReservation);
    return shouldStopReader;
}

static TZrBool stdio_request_input_is_stop_requested(SZrStdioRequestInputState *input) {
    TZrBool stopRequested;

    stdio_request_input_lock(input);
    stopRequested = input->stopRequested;
    stdio_request_input_unlock(input);
    return stopRequested;
}

static void stdio_request_read_loop(SZrStdioServer *server) {
    SZrStdioFrameReaderLimits frameLimits;
    FILE *inputFile;

    ZrLanguageServer_StdioFrameReader_DefaultLimits(&frameLimits);
    inputFile = server->requestInput.input != NULL ? server->requestInput.input : stdin;
    for (;;) {
        char *payload = NULL;
        TZrSize payloadLength = 0;
        EZrStdioFrameReadStatus frameStatus;

        if (stdio_request_input_is_stop_requested(&server->requestInput)) {
            break;
        }
        frameStatus = ZrLanguageServer_StdioFrameReader_Read(
                inputFile,
                &frameLimits,
                &payload,
                &payloadLength);

        if (frameStatus != ZR_STDIO_FRAME_READ_OK) {
            if (frameStatus != ZR_STDIO_FRAME_READ_EOF) {
                fprintf(stderr,
                        "LSP stdio frame reader: %s\n",
                        ZrLanguageServer_StdioFrameReader_StatusName(frameStatus));
            }
            break;
        }

        cJSON *message = cJSON_ParseWithLength(payload, payloadLength);
        free(payload);
        if (stdio_request_handle_input_message(server, message)) {
            break;
        }
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
    input->isInitialized = ZR_TRUE;
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_StdioRequestInput_Start(SZrStdioServer *server) {
    SZrStdioRequestInputState *input;

    if (server == NULL || !server->requestInput.isInitialized || server->requestInput.readerStarted) {
        return ZR_FALSE;
    }
    input = &server->requestInput;

#ifdef _WIN32
    input->readerThread = CreateThread(NULL, 0, stdio_request_reader_thread, server, 0, NULL);
    if (input->readerThread == NULL) {
        return ZR_FALSE;
    }
#else
    if (pthread_create(&input->readerThread, NULL, stdio_request_reader_thread, server) != 0) {
        return ZR_FALSE;
    }
#endif
    input->readerStarted = ZR_TRUE;
    return ZR_TRUE;
}

void ZrLanguageServer_StdioRequestInput_Stop(SZrStdioServer *server) {
    SZrStdioRequestInputState *input;

    if (server == ZR_NULL || !server->requestInput.isInitialized) {
        return;
    }
    input = &server->requestInput;
    stdio_request_input_lock(input);
    input->stopRequested = ZR_TRUE;
    input->inputClosed = ZR_TRUE;
    stdio_request_input_broadcast(input);
    stdio_request_input_unlock(input);
}

void ZrLanguageServer_StdioRequestInput_Join(SZrStdioServer *server) {
    SZrStdioRequestInputState *input;

    if (server == ZR_NULL || !server->requestInput.readerStarted) {
        return;
    }
    input = &server->requestInput;
#ifdef _WIN32
    if (WaitForSingleObject(input->readerThread, INFINITE) == WAIT_OBJECT_0) {
        CloseHandle(input->readerThread);
        input->readerThread = NULL;
        input->readerStarted = ZR_FALSE;
    }
#else
    if (pthread_join(input->readerThread, NULL) == 0) {
        input->readerStarted = ZR_FALSE;
    }
#endif
}

void ZrLanguageServer_StdioRequestInput_Free(SZrStdioServer *server) {
    SZrStdioRequestInputState *input;
    SZrStdioInboundMessage *inbound;

    if (server == ZR_NULL || !server->requestInput.isInitialized) {
        return;
    }
    input = &server->requestInput;
    inbound = input->head;
    while (inbound != ZR_NULL) {
        SZrStdioInboundMessage *next = inbound->next;

        cJSON_Delete(inbound->message);
        free(inbound);
        inbound = next;
    }
    input->head = ZR_NULL;
    input->tail = ZR_NULL;
#ifdef _WIN32
    DeleteCriticalSection(&input->lock);
#else
    pthread_cond_destroy(&input->messageAvailable);
    pthread_mutex_destroy(&input->lock);
#endif
    memset(input, 0, sizeof(*input));
}

TZrBool ZrLanguageServer_StdioRequestInput_Take(SZrStdioServer *server,
                                                 cJSON **outMessage,
                                                 TZrBool *outIsParseError,
                                                 EZrStdioRequestReservation *outRequestReservation) {
    SZrStdioRequestInputState *input;
    SZrStdioInboundMessage *inbound;

    if (outMessage != NULL) {
        *outMessage = NULL;
    }
    if (outIsParseError != NULL) {
        *outIsParseError = ZR_FALSE;
    }
    if (outRequestReservation != NULL) {
        *outRequestReservation = ZR_STDIO_REQUEST_RESERVATION_NONE;
    }
    if (server == NULL || !server->requestInput.isInitialized || outMessage == NULL ||
        outIsParseError == NULL || outRequestReservation == NULL) {
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
    *outRequestReservation = inbound->requestReservation;
    free(inbound);
    return ZR_TRUE;
}

void ZrLanguageServer_StdioRequestInput_Activate(SZrStdioServer *server, const cJSON *id) {
    if (server == NULL) {
        return;
    }

    server->activeRequestId = id;
}

TZrBool ZrLanguageServer_StdioRequestInput_IsActiveCancelled(SZrStdioServer *server) {
    if (server == NULL || server->activeRequestId == ZR_NULL) {
        return ZR_FALSE;
    }
    return ZrLanguageServer_StdioRequestRegistry_IsCancelled(server->requestRegistry,
                                                              server->activeRequestId);
}

void ZrLanguageServer_StdioRequestInput_Complete(SZrStdioServer *server, const cJSON *id) {
    if (server == NULL) {
        return;
    }

    ZrLanguageServer_StdioRequestRegistry_Complete(server->requestRegistry, id);
    server->activeRequestId = ZR_NULL;
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
