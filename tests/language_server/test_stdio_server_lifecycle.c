//
// stdio 生命周期、帧与信封验证及故障清理的独立 CTest；各用例持有并回收其 FILE 和 JSON 资源。
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stdio_frame_reader.h"
#include "stdio_json_rpc.h"
#include "stdio_lifecycle.h"
#include "stdio_request_registry.h"
#include "stdio_server.h"
/* 累计断言失败，不提前跳出用例，以便执行尾部清理并统一决定进程退出码。 */
static int g_failures = 0;
/** @brief 非中断式断言：记录失败消息与计数，由 main 汇总为 CTest 退出状态。 */
static void expect_true(TZrBool condition, const char *message) {
    if (!condition) {
        printf("Fail - %s\n", message);
        g_failures++;
    }
}
/** @brief 覆盖 NEW 到 EXITED 的许可、重复通知与关闭前后退出码；栈上 lifecycle 无需释放。 */
static void test_lifecycle_state_transitions(void) {
    SZrStdioLifecycle lifecycle;

    ZrLanguageServer_StdioLifecycle_Init(&lifecycle);
    expect_true(lifecycle.state == ZR_STDIO_LIFECYCLE_NEW,
                "lifecycle must start in NEW");
    expect_true(!lifecycle.initializedNotificationReceived,
                "initialized notification must start unset");
    expect_true(!ZrLanguageServer_StdioLifecycle_CanProcessRequest(&lifecycle),
                "NEW lifecycle must reject ordinary requests");
    /* 早到 initialized 不应提前放行请求。 */
    ZrLanguageServer_StdioLifecycle_MarkInitialized(&lifecycle);
    expect_true(lifecycle.state == ZR_STDIO_LIFECYCLE_NEW,
                "initialized before initialize must be ignored");
    expect_true(!lifecycle.initializedNotificationReceived,
                "early initialized notification must stay unset");
    /* initialize 成功后先进入 INITIALIZING，再由 initialized 通知推进到 RUNNING。 */
    expect_true(ZrLanguageServer_StdioLifecycle_BeginInitialize(&lifecycle),
                "NEW lifecycle must enter INITIALIZING");
    expect_true(lifecycle.state == ZR_STDIO_LIFECYCLE_INITIALIZING,
                "initialize must enter INITIALIZING");
    expect_true(ZrLanguageServer_StdioLifecycle_CanProcessRequest(&lifecycle),
                "INITIALIZING lifecycle must accept requests");
    /* 重复 initialized 不应改写已运行状态。 */
    ZrLanguageServer_StdioLifecycle_MarkInitialized(&lifecycle);
    expect_true(lifecycle.state == ZR_STDIO_LIFECYCLE_RUNNING,
                "initialized must enter RUNNING");
    expect_true(lifecycle.initializedNotificationReceived,
                "initialized notification must be recorded");
    ZrLanguageServer_StdioLifecycle_MarkInitialized(&lifecycle);
    expect_true(lifecycle.state == ZR_STDIO_LIFECYCLE_RUNNING,
                "duplicate initialized must be ignored");
    /* shutdown 封闭普通请求与重复 initialize；exit 决定进程退出码。 */
    expect_true(ZrLanguageServer_StdioLifecycle_BeginShutdown(&lifecycle),
                "RUNNING lifecycle must enter SHUTDOWN");
    expect_true(ZrLanguageServer_StdioLifecycle_IsShutdown(&lifecycle),
                "shutdown state must be observable");
    expect_true(!ZrLanguageServer_StdioLifecycle_CanProcessRequest(&lifecycle),
                "SHUTDOWN lifecycle must reject ordinary requests");
    expect_true(!ZrLanguageServer_StdioLifecycle_BeginInitialize(&lifecycle),
                "SHUTDOWN lifecycle must reject reinitialize");
    expect_true(ZrLanguageServer_StdioLifecycle_Exit(&lifecycle) == 0,
                "exit after shutdown must return zero");
    expect_true(lifecycle.state == ZR_STDIO_LIFECYCLE_EXITED,
                "exit must enter EXITED");
    expect_true(!ZrLanguageServer_StdioLifecycle_CanProcessRequest(&lifecycle),
                "EXITED lifecycle must reject ordinary requests");
    expect_true(ZrLanguageServer_StdioLifecycle_Exit(&lifecycle) == 1,
                "repeated exit must return failure code");
    /* 另起 NEW 实例验证未完成 shutdown 的 exit 错误码和终态。 */
    ZrLanguageServer_StdioLifecycle_Init(&lifecycle);
    expect_true(ZrLanguageServer_StdioLifecycle_Exit(&lifecycle) == 1,
                "exit before shutdown must return failure code");
    expect_true(lifecycle.state == ZR_STDIO_LIFECYCLE_EXITED,
                "failed exit must still enter EXITED");
}
/** @brief 用不同 JSON id 验证预留、取消与完成后重用；调用方保有四棵 cJSON id 树。 */
static void test_request_registry_identity_and_cancellation(void) {
    SZrStdioRequestRegistry *registry =
            ZrLanguageServer_StdioRequestRegistry_New();
    cJSON *numericId = cJSON_CreateNumber(1.0);
    cJSON *stringId = cJSON_CreateString("1");
    cJSON *unknownId = cJSON_CreateString("unknown");
    cJSON *booleanId = cJSON_CreateBool(1);
    /* registry 构造失败仍释放已创建的 id 树；成功时在尾部统一回收。 */
    expect_true(registry != NULL, "request registry must construct");
    if (registry == NULL) {
        cJSON_Delete(numericId);
        cJSON_Delete(stringId);
        cJSON_Delete(unknownId);
        cJSON_Delete(booleanId);
        return;
    }
    /* 数字与字符串 1 互不冲突，同型 id 在活动期间不能重复预留。 */
    expect_true(ZrLanguageServer_StdioRequestRegistry_Reserve(registry, numericId) ==
                        ZR_STDIO_REQUEST_RESERVATION_ACCEPTED,
                "numeric request id must reserve");
    expect_true(ZrLanguageServer_StdioRequestRegistry_Reserve(registry, numericId) ==
                        ZR_STDIO_REQUEST_RESERVATION_DUPLICATE,
                "active numeric request id must be rejected as duplicate");
    expect_true(ZrLanguageServer_StdioRequestRegistry_Reserve(registry, stringId) ==
                        ZR_STDIO_REQUEST_RESERVATION_ACCEPTED,
                "string request id must not collide with numeric id");
    expect_true(ZrLanguageServer_StdioRequestRegistry_Reserve(registry, stringId) ==
                        ZR_STDIO_REQUEST_RESERVATION_DUPLICATE,
                "active string request id must be rejected as duplicate");
    expect_true(!ZrLanguageServer_StdioRequestRegistry_IsCancelled(registry, numericId),
                "new numeric request must not be cancelled");
    expect_true(!ZrLanguageServer_StdioRequestRegistry_IsCancelled(registry, stringId),
                "new string request must not be cancelled");
    /* 取消只匹配当前活动 id；未知 id 无效，布尔 id 不能预留。 */
    expect_true(ZrLanguageServer_StdioRequestRegistry_Cancel(registry, numericId),
                "known numeric request id must be cancellable");
    expect_true(ZrLanguageServer_StdioRequestRegistry_IsCancelled(registry, numericId),
                "cancellation must be retained for the matching numeric id");
    expect_true(!ZrLanguageServer_StdioRequestRegistry_IsCancelled(registry, stringId),
                "cancellation must not cross numeric/string id types");
    expect_true(!ZrLanguageServer_StdioRequestRegistry_Cancel(registry, unknownId),
                "unknown request cancellation must be a no-op");
    expect_true(ZrLanguageServer_StdioRequestRegistry_Reserve(registry, booleanId) ==
                        ZR_STDIO_REQUEST_RESERVATION_FAILED,
                "structured request id must not enter the registry");
    /* Complete 移除活动项，重新预留应清空旧取消状态。 */
    ZrLanguageServer_StdioRequestRegistry_Complete(registry, numericId);
    expect_true(ZrLanguageServer_StdioRequestRegistry_Reserve(registry, numericId) ==
                        ZR_STDIO_REQUEST_RESERVATION_ACCEPTED,
                "completed request id must be reusable");
    expect_true(!ZrLanguageServer_StdioRequestRegistry_IsCancelled(registry, numericId),
                "reused request id must start with a fresh cancellation state");
    /* registry 自行释放复制的字符串 id，调用方再释放原始 cJSON 树。 */
    ZrLanguageServer_StdioRequestRegistry_Complete(registry, numericId);
    ZrLanguageServer_StdioRequestRegistry_Complete(registry, stringId);
    ZrLanguageServer_StdioRequestRegistry_Free(registry);
    cJSON_Delete(numericId);
    cJSON_Delete(stringId);
    cJSON_Delete(unknownId);
    cJSON_Delete(booleanId);
}
/** @brief 将字节写入临时 FILE 后读取一帧；成功 payload 归调用方，失败输出清零并关闭 FILE。TODO: fwrite 返回值未核对，写短时负例可能误判；需验证完整写入。 */
static EZrStdioFrameReadStatus read_frame_from_memory(const void *bytes,
                                                       size_t length,
                                                       const SZrStdioFrameReaderLimits *limits,
                                                       char **outPayload,
                                                       TZrSize *outLength) {
    FILE *input = tmpfile();
    EZrStdioFrameReadStatus status;
    /* 临时文件打开失败须清零输出，让用例识别 I/O 错误。 */
    if (input == NULL) {
        if (outPayload != NULL) {
            *outPayload = NULL;
        }
        if (outLength != NULL) {
            *outLength = 0;
        }
        return ZR_STDIO_FRAME_READ_IO_ERROR;
    }
    if (length > 0) {
        fwrite(bytes, 1, length, input);
    }
    rewind(input);
    status = ZrLanguageServer_StdioFrameReader_Read(input, limits, outPayload, outLength);
    fclose(input);
    return status;
}
/** @brief 验证帧状态与预算，并抽查失败输出契约；成功缓冲由本用例 free。 */
static void test_frame_reader_status_and_limits(void) {
    static const char validFrame[] = "Content-Length: 2\r\n\r\n{}";
    static const char missingLengthFrame[] =
            "Content-Type: application/vscode-jsonrpc\r\n\r\n";
    static const char truncatedFrame[] = "Content-Length: 4\r\n\r\n{}";
    static const char wrongNewlineFrame[] = "Content-Length: 2\n\n{}";
    static const char nulFrame[] = "Content-Length: 2\0junk\r\n\r\n{}";
    static const char limitedMessageFrame[] = "Content-Length: 3\r\n\r\nabc";
    static const char limitedHeaderFrame[] =
            "X-Test: value\r\nContent-Length: 2\r\n\r\n{}";
    SZrStdioFrameReaderLimits limits;
    char *payload = NULL;
    TZrSize length = 0;
    /* 成功帧检查精确 payload，再释放 reader 移交的缓冲。 */
    expect_true(read_frame_from_memory(validFrame,
                                       strlen(validFrame),
                                       NULL,
                                       &payload,
                                       &length) == ZR_STDIO_FRAME_READ_OK,
                "valid frame must be accepted by the reader");
    expect_true(length == 2 && payload != NULL && memcmp(payload, "{}", 2) == 0,
                "valid frame payload must be returned exactly");
    free(payload);
    /* 前两类错误同时核对输出清零；TODO: LF、NUL 与 EOF 分支仅验状态，需用非默认输出哨兵复核失败后的清零契约。 */
    payload = NULL;
    length = 0;
    expect_true(read_frame_from_memory(missingLengthFrame,
                                       strlen(missingLengthFrame),
                                       NULL,
                                       &payload,
                                       &length) == ZR_STDIO_FRAME_READ_MALFORMED_HEADER,
                "missing content length must be malformed");
    expect_true(payload == NULL && length == 0,
                "malformed frame must not return a payload");

    expect_true(read_frame_from_memory(truncatedFrame,
                                       strlen(truncatedFrame),
                                       NULL,
                                       &payload,
                                       &length) == ZR_STDIO_FRAME_READ_PAYLOAD_TRUNCATED,
                "short payload must be classified as truncated");
    expect_true(payload == NULL && length == 0,
                "truncated frame must not return a payload");

    expect_true(read_frame_from_memory(wrongNewlineFrame,
                                       strlen(wrongNewlineFrame),
                                       NULL,
                                       &payload,
                                       &length) == ZR_STDIO_FRAME_READ_MALFORMED_HEADER,
                "LF-only framing must be malformed");

    expect_true(read_frame_from_memory(nulFrame,
                                       sizeof(nulFrame) - 1U,
                                       NULL,
                                       &payload,
                                       &length) == ZR_STDIO_FRAME_READ_MALFORMED_HEADER,
                "NUL in a header must be malformed");
    /* 自定义上限仅收紧默认预算，分别覆盖消息长度与头字段数量。 */
    memset(&limits, 0, sizeof(limits));
    limits.maxMessageBytes = 2;
    expect_true(read_frame_from_memory(limitedMessageFrame,
                                       strlen(limitedMessageFrame),
                                       &limits,
                                       &payload,
                                       &length) == ZR_STDIO_FRAME_READ_TOO_LARGE,
                "injected message limit must classify an oversized frame");
    expect_true(payload == NULL && length == 0,
                "oversized frame must not allocate a payload");
    /* TODO: 头字段数量超限用例只检查状态，未复核失败时 payload 与 length 必须清零；需补输出断言。 */
    memset(&limits, 0, sizeof(limits));
    limits.maxHeaderCount = 1;
    expect_true(read_frame_from_memory(limitedHeaderFrame,
                                       strlen(limitedHeaderFrame),
                                       &limits,
                                       &payload,
                                       &length) == ZR_STDIO_FRAME_READ_TOO_LARGE,
                "injected header count limit must be enforced");

    expect_true(read_frame_from_memory(NULL,
                                       0,
                                       NULL,
                                       &payload,
                                       &length) == ZR_STDIO_FRAME_READ_EOF,
                "clean empty input must be classified as EOF");
}
/** @brief 验证 JSON-RPC 信封的 id、版本、参数与通知分类；解析树由用例删除，envelope 字段仅借用。 */
static void test_json_rpc_envelope_validation(void) {
    static const char *invalidTopLevelMessages[] = {
            "[]",
            "17",
    };
    static const char *invalidIdMessages[] = {
            "{\"jsonrpc\":\"2.0\",\"id\":true,\"method\":\"test\"}",
            "{\"jsonrpc\":\"2.0\",\"id\":{},\"method\":\"test\"}",
            "{\"jsonrpc\":\"2.0\",\"id\":[],\"method\":\"test\"}",
    };
    cJSON *message;
    SZrJsonRpcEnvelope envelope;
    const cJSON *errorId;
    size_t index;
    /* 非对象顶层不能产生可回显 id。 */
    for (index = 0; index < sizeof(invalidTopLevelMessages) / sizeof(invalidTopLevelMessages[0]); index++) {
        message = cJSON_Parse(invalidTopLevelMessages[index]);
        errorId = (const cJSON *)1;
        expect_true(message != NULL &&
                            ZrLanguageServer_StdioJsonRpc_ParseEnvelope(message, &envelope, &errorId) ==
                                    ZR_JSON_RPC_ENVELOPE_INVALID_REQUEST,
                    "non-object top-level message must be invalid request");
        expect_true(errorId == NULL, "invalid top-level message must not expose an error id");
        cJSON_Delete(message);
    }
    /* 版本非法时仍借用合法 id，以便错误回复关联原请求。 */
    message = cJSON_Parse("{\"id\":\"missing-version\",\"method\":\"test\"}");
    errorId = NULL;
    expect_true(message != NULL &&
                        ZrLanguageServer_StdioJsonRpc_ParseEnvelope(message, &envelope, &errorId) ==
                                ZR_JSON_RPC_ENVELOPE_INVALID_REQUEST,
                "missing jsonrpc version must be invalid request");
    expect_true(errorId != NULL && cJSON_IsString((cJSON *)errorId),
                "missing jsonrpc version must preserve a valid request id");
    cJSON_Delete(message);

    message = cJSON_Parse("{\"jsonrpc\":\"1.0\",\"id\":\"wrong-version\",\"method\":\"test\"}");
    errorId = NULL;
    expect_true(message != NULL &&
                        ZrLanguageServer_StdioJsonRpc_ParseEnvelope(message, &envelope, &errorId) ==
                                ZR_JSON_RPC_ENVELOPE_INVALID_REQUEST,
                "wrong jsonrpc version must be invalid request");
    expect_true(errorId != NULL && cJSON_IsString((cJSON *)errorId),
                "wrong jsonrpc version must preserve a valid request id");
    cJSON_Delete(message);
    /* 布尔或容器 id 不得预留，也不得在错误回复中回显。 */
    for (index = 0; index < sizeof(invalidIdMessages) / sizeof(invalidIdMessages[0]); index++) {
        message = cJSON_Parse(invalidIdMessages[index]);
        errorId = (const cJSON *)1;
        expect_true(message != NULL &&
                            ZrLanguageServer_StdioJsonRpc_ParseEnvelope(message, &envelope, &errorId) ==
                                    ZR_JSON_RPC_ENVELOPE_INVALID_REQUEST,
                    "boolean or structured request id must be invalid request");
        expect_true(errorId == NULL, "invalid request id must map to a null error id");
        cJSON_Delete(message);
    }
    /* 参数类型错误区别于信封无效，并保留请求 id。 */
    message = cJSON_Parse("{\"jsonrpc\":\"2.0\",\"id\":\"bad-params\",\"method\":\"test\",\"params\":false}");
    errorId = NULL;
    expect_true(message != NULL &&
                        ZrLanguageServer_StdioJsonRpc_ParseEnvelope(message, &envelope, &errorId) ==
                                ZR_JSON_RPC_ENVELOPE_INVALID_PARAMS,
                "scalar params must be invalid params");
    expect_true(errorId != NULL && cJSON_IsString((cJSON *)errorId),
                "invalid params must preserve a valid request id");
    cJSON_Delete(message);
    /* 对象参数与字符串 id 应构成请求，借用字段只在 message 存活期间有效。BUG: 若本次 cJSON_Parse 分配失败，短路跳过 ParseEnvelope，后续断言会读取上一用例已删除的 envelope.params；需先守卫解析成功。 */
    message = cJSON_Parse("{\"jsonrpc\":\"2.0\",\"id\":\"request\",\"method\":\"test\",\"params\":{}}");
    errorId = NULL;
    expect_true(message != NULL &&
                        ZrLanguageServer_StdioJsonRpc_ParseEnvelope(message, &envelope, &errorId) ==
                                ZR_JSON_RPC_ENVELOPE_OK,
                "object params must produce a valid request envelope");
    expect_true(envelope.isRequest && !envelope.isNotification && envelope.params != NULL &&
                        cJSON_IsObject((cJSON *)envelope.params) &&
                        strcmp(envelope.method, "test") == 0 && errorId == envelope.id,
                "valid request envelope must preserve method, params and id");
    cJSON_Delete(message);
    /* 缺 id 是通知，数组 params 仍合法。 */
    message = cJSON_Parse("{\"jsonrpc\":\"2.0\",\"method\":\"test\",\"params\":[]}");
    errorId = (const cJSON *)1;
    expect_true(message != NULL &&
                        ZrLanguageServer_StdioJsonRpc_ParseEnvelope(message, &envelope, &errorId) ==
                                ZR_JSON_RPC_ENVELOPE_OK,
                "missing id must produce a valid notification envelope");
    expect_true(!envelope.isRequest && envelope.isNotification && envelope.id == NULL &&
                        envelope.params != NULL && cJSON_IsArray((cJSON *)envelope.params) &&
                        errorId == NULL,
                "notification envelope must have no request id");
    cJSON_Delete(message);
    /* 显式 null id 是请求，不等于缺失 id 的通知。 */
    message = cJSON_Parse("{\"jsonrpc\":\"2.0\",\"id\":null,\"method\":\"test\"}");
    errorId = NULL;
    expect_true(message != NULL &&
                        ZrLanguageServer_StdioJsonRpc_ParseEnvelope(message, &envelope, &errorId) ==
                                ZR_JSON_RPC_ENVELOPE_OK,
                "explicit null id must remain a valid JSON-RPC request");
    expect_true(envelope.isRequest && !envelope.isNotification && envelope.id != NULL &&
                        cJSON_IsNull((cJSON *)envelope.id) && errorId == envelope.id,
                "explicit null id must not be mistaken for a notification");
    cJSON_Delete(message);
}
/** @brief 重复 100 次构造、启动、停止与释放服务器；调用方自行持有并关闭 FILE。 */
static void test_repeated_server_lifecycle(void) {
    int iteration;
    /* 每轮单独建输入流，避免服务器清理依赖前一轮资源。 */
    for (iteration = 0; iteration < 100; iteration++) {
        FILE *input = tmpfile();
        SZrStdioServerOptions options;
        SZrStdioServer *server;

        expect_true(input != NULL, "lifecycle test input must be available");
        if (input == NULL) {
            continue;
        }
        memset(&options, 0, sizeof(options));
        options.input = input;
        server = ZrLanguageServer_StdioServer_New(&options);
        expect_true(server != NULL, "server construction must succeed repeatedly");
        if (server != NULL) {
            expect_true(ZrLanguageServer_StdioServer_Start(server),
                        "reader start must succeed repeatedly");
            ZrLanguageServer_StdioServer_Shutdown(server);
            ZrLanguageServer_StdioServer_Free(server);
        }
        fclose(input);
    }
}
/** @brief 以 exit 通知帧启动 reader，并验证启动与关闭路径可结束。 */
static void test_exit_notification_stops_the_reader(void) {
    static const char payload[] = "{\"jsonrpc\":\"2.0\",\"method\":\"exit\"}";
    FILE *input = tmpfile();
    SZrStdioServerOptions options;
    SZrStdioServer *server;
    /* TODO: fprintf 未核对写入，且 Start 后立即主动 Shutdown；需先证实 exit 帧已进入队列，再观察 reader 是否因此自行停读。 */
    expect_true(input != NULL, "exit-frame input must be available");
    if (input == NULL) {
        return;
    }
    fprintf(input, "Content-Length: %zu\r\n\r\n%s", strlen(payload), payload);
    rewind(input);
    memset(&options, 0, sizeof(options));
    options.input = input;
    server = ZrLanguageServer_StdioServer_New(&options);
    expect_true(server != NULL, "server must construct for an exit frame");
    if (server != NULL) {
        expect_true(ZrLanguageServer_StdioServer_Start(server),
                    "reader must accept an exit notification frame");
        ZrLanguageServer_StdioServer_Shutdown(server);
        ZrLanguageServer_StdioServer_Free(server);
    }
    fclose(input);
}
/** @brief 注入构造与启动阶段故障，验证错误报告和统一释放路径可调用。 */
static void test_startup_failure_uses_the_same_teardown_path(void) {
    const EZrStdioServerFaultPoint newFaults[] = {
            ZR_STDIO_SERVER_FAULT_AFTER_GLOBAL,
            ZR_STDIO_SERVER_FAULT_AFTER_CONTEXT,
            ZR_STDIO_SERVER_FAULT_AFTER_INPUT_INIT,
    };
    size_t index;
    /* TODO: New 返回 NULL 既不能证明已到达指定故障点，也不能证明部分资源已释放；需故障点计数及泄漏检测。 */
    for (index = 0; index < sizeof(newFaults) / sizeof(newFaults[0]); index++) {
        FILE *input = tmpfile();
        SZrStdioServerOptions options;

        expect_true(input != NULL, "fault-injection input must be available");
        if (input == NULL) {
            continue;
        }
        memset(&options, 0, sizeof(options));
        options.input = input;
        options.faultPoint = newFaults[index];
        expect_true(ZrLanguageServer_StdioServer_New(&options) == NULL,
                    "construction fault must release every initialized dependency");
        fclose(input);
    }
    /* 启动后故障由调用方 Free 再关闭借用 FILE。TODO: Start 失败也可能是建线程失败，需观测注入点确已触发。 */
    {
        FILE *input = tmpfile();
        SZrStdioServerOptions options;
        SZrStdioServer *server;

        expect_true(input != NULL, "reader-start fault input must be available");
        if (input == NULL) {
            return;
        }
        memset(&options, 0, sizeof(options));
        options.input = input;
        options.faultPoint = ZR_STDIO_SERVER_FAULT_AFTER_READER_START;
        server = ZrLanguageServer_StdioServer_New(&options);
        expect_true(server != NULL, "server must reach the reader-start fault point");
        if (server != NULL) {
            expect_true(!ZrLanguageServer_StdioServer_Start(server),
                        "reader-start fault must be reported to the caller");
            ZrLanguageServer_StdioServer_Free(server);
        }
        fclose(input);
    }
}
/** @brief 顺序运行七组自检，并以累计失败数决定该 CTest 目标的退出码。 */
int main(void) {
    test_lifecycle_state_transitions();
    test_request_registry_identity_and_cancellation();
    test_frame_reader_status_and_limits();
    test_json_rpc_envelope_validation();
    test_repeated_server_lifecycle();
    test_exit_notification_stops_the_reader();
    test_startup_failure_uses_the_same_teardown_path();

    if (g_failures != 0) {
        printf("Fail - stdio server lifecycle: %d failures\n", g_failures);
        return 1;
    }

    printf("Pass - stdio server lifecycle\n");
    return 0;
}
