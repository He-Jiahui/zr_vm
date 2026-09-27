#include "zr_vm_language_server_stdio_internal.h"
#include "stdio_json_builder.h"

/* 为响应复制请求 ID；数值 ID 保持 JSON 数值类型，返回树归响应信封。 */
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

/* 队列状态锁：输入线程和主线程只在初始化后、销毁前访问共享字段。 */
static void stdio_request_input_lock(SZrStdioRequestInputState *input) {
#ifdef _WIN32
    EnterCriticalSection(&input->lock);
#else
    pthread_mutex_lock(&input->lock);
#endif
}

/* 与 input_lock 配对，放行主线程取队或读线程发布关闭状态。 */
static void stdio_request_input_unlock(SZrStdioRequestInputState *input) {
#ifdef _WIN32
    LeaveCriticalSection(&input->lock);
#else
    pthread_mutex_unlock(&input->lock);
#endif
}

/* 新消息入队后唤醒一个等待取队的主线程；调用时持有队列锁。 */
static void stdio_request_input_signal(SZrStdioRequestInputState *input) {
#ifdef _WIN32
    WakeConditionVariable(&input->messageAvailable);
#else
    pthread_cond_signal(&input->messageAvailable);
#endif
}

/* 输入关闭或请求停止时唤醒所有队列等待者；不负责中断 FILE 读取。 */
static void stdio_request_input_broadcast(SZrStdioRequestInputState *input) {
#ifdef _WIN32
    WakeAllConditionVariable(&input->messageAvailable);
#else
    pthread_cond_broadcast(&input->messageAvailable);
#endif
}

/* Take 持锁等待队列状态变化，醒后仍须重新检查消息或关闭条件。 */
static void stdio_request_input_wait(SZrStdioRequestInputState *input) {
#ifdef _WIN32
    SleepConditionVariableCS(&input->messageAvailable, &input->lock, INFINITE);
#else
    pthread_cond_wait(&input->messageAvailable, &input->lock);
#endif
}

/* 输入线程将 JSON 树及预留结果移交主线程；队列节点和消息由 Take/Free 接管。 */
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
    /* BUG: 请求 ID 已 Reserve 后若节点申请失败，消息被丢弃却未 Complete；
     * 当前请求无响应，后来同 ID 请求会被注册表判为重复，直到服务器释放。 */
    if (inbound == NULL) {
        cJSON_Delete(message);
        return;
    }

    inbound->message = message;
    inbound->isParseError = isParseError;
    inbound->requestReservation = requestReservation;
    input = &server->requestInput;
    stdio_request_input_lock(input);
    /* TODO: 帧大小有上限，但队列总量没有预算或背压；需用慢处理和连续输入
     * 核查排队增长及节点申请失败时的服务可用性。 */
    if (input->tail != NULL) {
        input->tail->next = inbound;
    } else {
        input->head = inbound;
    }
    input->tail = inbound;
    stdio_request_input_signal(input);
    stdio_request_input_unlock(input);
}

/* 读线程先分类并预留请求，以便后来到达的取消通知作用于排队或执行中的 ID。 */
static TZrBool stdio_request_handle_input_message(SZrStdioServer *server, cJSON *message) {
    SZrJsonRpcEnvelope envelope;
    const cJSON *errorId = ZR_NULL;
    EZrStdioRequestReservation requestReservation = ZR_STDIO_REQUEST_RESERVATION_NONE;
    TZrBool shouldStopReader = ZR_FALSE;

    /* TODO: cJSON 解析返回 NULL 也可能源于分配失败；需核查能否区分内存故障
     * 与无效 JSON，避免把有效请求错误地报告为 Parse error。 */
    if (message == NULL) {
        stdio_request_enqueue(server,
                              NULL,
                              ZR_TRUE,
                              ZR_STDIO_REQUEST_RESERVATION_NONE);
        return ZR_FALSE;
    }

    if (ZrLanguageServer_StdioJsonRpc_ParseEnvelope(message, &envelope, &errorId) ==
        ZR_JSON_RPC_ENVELOPE_OK) {
        /* 取消走旁路，不受主线程长请求阻塞；ID 只借用本消息直到 Cancel 返回。 */
        if (envelope.isNotification && strcmp(envelope.method, ZR_LSP_METHOD_CANCEL_REQUEST) == 0) {
            const cJSON *id = cJSON_GetObjectItemCaseSensitive(envelope.params,
                                                                 ZR_LSP_JSON_RPC_FIELD_ID);
            ZrLanguageServer_StdioRequestRegistry_Cancel(server->requestRegistry, id);
            cJSON_Delete(message);
            return ZR_FALSE;
        }
        /* 预留顺序与输入帧顺序一致；exit 入队供主线程处理，读线程随即停读。 */
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

/* 在每次读帧前受锁读取停止标志，避免与 Stop 的写入竞态。 */
static TZrBool stdio_request_input_is_stop_requested(SZrStdioRequestInputState *input) {
    TZrBool stopRequested;

    stdio_request_input_lock(input);
    stopRequested = input->stopRequested;
    stdio_request_input_unlock(input);
    return stopRequested;
}

/* 专属读线程逐帧校验并解析 JSON，向主线程发布消息；EOF/帧错误均关闭输入侧。 */
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

        /* 帧错误只写 stderr 并停止本流；主线程会在队列耗尽后结束。 */
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

    /* 即使异常退出读循环也要唤醒 Take，使剩余队列可被耗尽。 */
    stdio_request_input_lock(&server->requestInput);
    server->requestInput.inputClosed = ZR_TRUE;
    stdio_request_input_broadcast(&server->requestInput);
    stdio_request_input_unlock(&server->requestInput);
}

#ifdef _WIN32
/* Windows 线程入口与 POSIX 入口共享同一读循环及 server 生命周期。 */
static DWORD WINAPI stdio_request_reader_thread(void *userData) {
    stdio_request_read_loop((SZrStdioServer *)userData);
    return 0;
}
#else
/* POSIX 线程入口；server 必须保持存活直到 Join 完成。 */
static void *stdio_request_reader_thread(void *userData) {
    stdio_request_read_loop((SZrStdioServer *)userData);
    return NULL;
}
#endif

/* Server_New 建立队列同步原语；Start 仅在成功后运行，失败由 Server_Free 安全回收。 */
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

/* Server_Start 只创建一个读线程；失败后 Server_Free 仍可清理已初始化的队列。 */
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

/* 请求读线程下次检查标志时退出，并让 Take 不再等待新消息；可重复调用。 */
void ZrLanguageServer_StdioRequestInput_Stop(SZrStdioServer *server) {
    SZrStdioRequestInputState *input;

    if (server == ZR_NULL || !server->requestInput.isInitialized) {
        return;
    }
    input = &server->requestInput;
    /* TODO: Stop 只置位和唤醒队列等待者，不能唤醒已阻塞在 FILE 读帧的线程；
     * 需以保持写端开放的管道覆盖 Shutdown/Free，再确定超时或中断契约。 */
    stdio_request_input_lock(input);
    input->stopRequested = ZR_TRUE;
    input->inputClosed = ZR_TRUE;
    stdio_request_input_broadcast(input);
    stdio_request_input_unlock(input);
}

/* Server_Free 在销毁锁、队列及注册表前等待读线程结束；调用者须确保在途读帧可返回。 */
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

/* Join 后回收尚未被 Take 领取的 JSON 和队列节点，再销毁同步原语。 */
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

/* 主线程领取最早入队消息；关闭后仍先耗尽队列，再以 false 告知主循环退出。 */
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
    /* 条件变量可能无消息醒来，必须在锁内重新验证队列和关闭状态。 */
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

    /* 消息树和预留结果交主线程，节点在此释放；调用方处理后删除 JSON。 */
    *outMessage = inbound->message;
    *outIsParseError = inbound->isParseError;
    *outRequestReservation = inbound->requestReservation;
    free(inbound);
    return ZR_TRUE;
}

/* 主循环借用当前消息中的 ID，供请求处理期间的取消轮询使用。 */
void ZrLanguageServer_StdioRequestInput_Activate(SZrStdioServer *server, const cJSON *id) {
    if (server == NULL) {
        return;
    }

    server->activeRequestId = id;
}

/* 处理器回调通过注册表锁观察读线程写入的取消状态。 */
TZrBool ZrLanguageServer_StdioRequestInput_IsActiveCancelled(SZrStdioServer *server) {
    if (server == NULL || server->activeRequestId == ZR_NULL) {
        return ZR_FALSE;
    }
    return ZrLanguageServer_StdioRequestRegistry_IsCancelled(server->requestRegistry,
                                                              server->activeRequestId);
}

/* 请求处理返回后移除 ID 预留并清空借用指针；之后主循环才能删除原消息树。 */
void ZrLanguageServer_StdioRequestInput_Complete(SZrStdioServer *server, const cJSON *id) {
    if (server == NULL) {
        return;
    }

    ZrLanguageServer_StdioRequestRegistry_Complete(server->requestRegistry, id);
    server->activeRequestId = ZR_NULL;
}

/* 串行写出一个 JSON-RPC stdio 帧并始终消费 message；调用方须独占 stdout 帧流。 */
EZrStdioSendStatus send_json_message(cJSON *message) {
    char *payload;
    size_t payloadLength;
    EZrStdioSendStatus status = ZR_STDIO_SEND_OK;

    if (message == NULL) {
        return ZR_STDIO_SEND_BUILD_ERROR;
    }

    payload = cJSON_PrintUnformatted(message);
    if (payload == NULL) {
        cJSON_Delete(message);
        return ZR_STDIO_SEND_BUILD_ERROR;
    }

    /* 写出失败可能已经留下部分帧，状态只供调用方终止或记录，不能原样重试。 */
    payloadLength = strlen(payload);
    if (fprintf(stdout, "%s %zu\r\n\r\n", ZR_LSP_STDIO_CONTENT_LENGTH_HEADER_PREFIX, payloadLength) < 0 ||
        fwrite(payload, 1, payloadLength, stdout) != payloadLength ||
        fflush(stdout) != 0) {
        status = ZR_STDIO_SEND_IO_ERROR;
    }

    cJSON_free(payload);
    cJSON_Delete(message);
    return status;
}

/* 构造共享的 JSON-RPC 版本与 ID 外壳；ID 被复制，原请求消息仍由主循环持有。 */
static cJSON *create_response_envelope(const cJSON *id) {
    cJSON *message = cJSON_CreateObject();

    if (message == NULL ||
        cJSON_AddStringToObject(message, ZR_LSP_JSON_RPC_FIELD_JSONRPC, ZR_LSP_JSON_RPC_VERSION) == NULL ||
        !stdio_json_add_owned_item(message, ZR_LSP_JSON_RPC_FIELD_ID, duplicate_id(id))) {
        cJSON_Delete(message);
        return NULL;
    }
    return message;
}

/* 把结果树交给响应外壳，所有失败状态也消费 result；NULL 映射 JSON null。 */
EZrStdioSendStatus send_result_response(const cJSON *id, cJSON *result) {
    cJSON *message = create_response_envelope(id);

    if (message == NULL) {
        cJSON_Delete(result);
        return ZR_STDIO_SEND_BUILD_ERROR;
    }
    if (!stdio_json_add_owned_item(message, ZR_LSP_JSON_RPC_FIELD_RESULT,
                                   result != NULL ? result : cJSON_CreateNull())) {
        cJSON_Delete(message);
        return ZR_STDIO_SEND_BUILD_ERROR;
    }

    return send_json_message(message);
}

/* 以借用的 ID 和错误文本构造协议错误响应，沿统一帧输出路径消费新树。 */
EZrStdioSendStatus send_error_response(const cJSON *id, int code, const char *messageText) {
    cJSON *message = create_response_envelope(id);
    cJSON *errorObject;

    if (message == NULL ||
        (errorObject = cJSON_AddObjectToObject(message, ZR_LSP_JSON_RPC_FIELD_ERROR)) == NULL ||
        cJSON_AddNumberToObject(errorObject, ZR_LSP_JSON_RPC_FIELD_CODE, code) == NULL ||
        cJSON_AddStringToObject(errorObject, ZR_LSP_JSON_RPC_FIELD_MESSAGE,
                                messageText != NULL ? messageText : "Unknown error") == NULL) {
        cJSON_Delete(message);
        return ZR_STDIO_SEND_BUILD_ERROR;
    }

    return send_json_message(message);
}

/* 供诊断和进度路径发布通知；成功构造时交给帧输出，失败时就地释放 params。 */
EZrStdioSendStatus send_notification(const char *method, cJSON *params) {
    cJSON *message = cJSON_CreateObject();

    if (message == NULL ||
        cJSON_AddStringToObject(message, ZR_LSP_JSON_RPC_FIELD_JSONRPC, ZR_LSP_JSON_RPC_VERSION) == NULL ||
        cJSON_AddStringToObject(message, ZR_LSP_JSON_RPC_FIELD_METHOD, method) == NULL) {
        cJSON_Delete(message);
        cJSON_Delete(params);
        return ZR_STDIO_SEND_BUILD_ERROR;
    }
    if (!stdio_json_add_owned_item(message, ZR_LSP_JSON_RPC_FIELD_PARAMS,
                                   params != NULL ? params : cJSON_CreateNull())) {
        cJSON_Delete(message);
        return ZR_STDIO_SEND_BUILD_ERROR;
    }

    return send_json_message(message);
}
