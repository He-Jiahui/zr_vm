#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "zr_vm_language_server_stdio_internal.h"
#include "stdio_frame_reader.h"
#include "unity.h"

/* 输出重定向测试只借用平台的描述符接口，统一通过同一组测试调用点恢复 stdout。 */
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#define test_dup _dup
#define test_dup2 _dup2
#define test_close _close
#define test_fileno _fileno
#else
#include <unistd.h>
#define test_dup dup
#define test_dup2 dup2
#define test_close close
#define test_fileno fileno
#endif

/* Unity 单测试独占的服务器、JSON 故障注入状态和 stdout 捕获资源；
 * cJSON hooks 属于进程全局状态，必须由 tearDown 在下一用例前复原。 */
static SZrStdioServer *g_server;
static cJSON *g_params;
static cJSON *g_id;
static cJSON *g_response;
static size_t g_attempts;
static size_t g_failureOrdinal;
static size_t g_injectedFailures;
static size_t g_liveJson;
static TZrBool g_persistent;
static size_t g_cancelOrdinal;
static TZrBool g_cancelled;
static FILE *g_output;
static int g_savedStdout;
#ifdef _WIN32
static int g_stdoutMode;
#endif

/* 作为 cJSON hook 记录每个分配点；失败序号与取消序号让测试走到
 * 构造、响应封装的不同阶段，并由配对 free 核对所有权。 */
static void *tracked_json_malloc(size_t size) {
    void *result;
    g_attempts++;
    if (g_cancelOrdinal != 0 && g_attempts >= g_cancelOrdinal) {
        g_cancelled = ZR_TRUE;
    }
    if (g_failureOrdinal != 0 &&
        (g_attempts == g_failureOrdinal || (g_persistent && g_attempts > g_failureOrdinal))) {
        g_injectedFailures++;
        return ZR_NULL;
    }
    result = malloc(size);
    if (result != ZR_NULL) {
        g_liveJson++;
    }
    return result;
}

/* 与故障注入分配器配对，确认失败路径没有遗留 JSON 树。 */
static void tracked_json_free(void *pointer) {
    if (pointer != ZR_NULL) {
        g_liveJson--;
        free(pointer);
    }
}

/* 在单个请求调用前安装全局 hook；persistent 模式覆盖持续 OOM 的回退路径。 */
static void begin_json_tracking(size_t failureOrdinal, TZrBool persistent) {
    cJSON_Hooks hooks = {tracked_json_malloc, tracked_json_free};
    g_attempts = 0;
    g_failureOrdinal = failureOrdinal;
    g_injectedFailures = 0;
    g_liveJson = 0;
    g_persistent = persistent;
    cJSON_InitHooks(&hooks);
}

/* 断言后也由 tearDown 调用，避免后续 Unity 用例继续写入临时流。 */
static void restore_stdout(void) {
    if (g_savedStdout >= 0) {
        fflush(stdout);
        test_dup2(g_savedStdout, test_fileno(stdout));
        test_close(g_savedStdout);
        g_savedStdout = -1;
#ifdef _WIN32
        _setmode(test_fileno(stdout), g_stdoutMode);
#endif
        clearerr(stdout);
    }
}

/* 借用当前进程 stdout 让真正的 JSON-RPC 帧发送路径接受输出故障注入。 */
static void redirect_stdout(FILE *output) {
    fflush(stdout);
#ifdef _WIN32
    g_stdoutMode = _setmode(test_fileno(stdout), _O_BINARY);
#endif
    g_savedStdout = test_dup(test_fileno(stdout));
    TEST_ASSERT_TRUE(g_savedStdout >= 0);
    TEST_ASSERT_TRUE(test_dup2(test_fileno(output), test_fileno(stdout)) >= 0);
}

/* 每例建立新的服务端上下文，使生命周期和协商开关互不污染。 */
void setUp(void) {
    g_savedStdout = -1;
    g_output = ZR_NULL;
    g_response = ZR_NULL;
    g_liveJson = 0;
    g_cancelOrdinal = 0;
    g_cancelled = ZR_FALSE;
    g_server = ZrLanguageServer_StdioServer_New(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_server);
    g_params = cJSON_Parse("{\"capabilities\":{\"general\":{\"positionEncodings\":[\"utf-8\"]},"
                          "\"textDocument\":{\"inlineCompletion\":{},"
                          "\"rangeFormatting\":{\"rangesSupport\":true}}}}");
    g_id = cJSON_CreateNumber(1);
    TEST_ASSERT_NOT_NULL(g_params);
    TEST_ASSERT_NOT_NULL(g_id);
}

/* 无论用例走到哪条故障路径，都恢复 hooks、描述符和 JSON 所有权。 */
void tearDown(void) {
    cJSON_InitHooks(ZR_NULL);
    restore_stdout();
    if (g_output != ZR_NULL) {
        fclose(g_output);
        g_output = ZR_NULL;
    }
    cJSON_Delete(g_response);
    cJSON_Delete(g_params);
    cJSON_Delete(g_id);
    ZrLanguageServer_StdioServer_Free(g_server);
    g_server = ZR_NULL;
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(0, g_liveJson, "initialize must release every JSON allocation");
}

/* 直接调用 initialize 处理器，验证单个构造点失败会释放结果并回滚协商开关；
 * 返回基准分配次数，供完整故障扫描和晚到取消用例定位阶段。 */
static size_t run_allocation_case(size_t failureOrdinal, TZrBool persistent) {
    SZrLspHandlerResult response;
    TZrBool hasResult;
    char message[96];
    g_server->positionEncoding = ZR_STDIO_POSITION_ENCODING_UTF16;
    g_server->supportsInlineCompletion = ZR_FALSE;
    g_server->supportsRangesFormatting = ZR_FALSE;
    begin_json_tracking(failureOrdinal, persistent);
    response = handle_initialize_request(g_server, g_params);
    hasResult = response.result != ZR_NULL;
    cJSON_Delete(response.result);
    cJSON_InitHooks(ZR_NULL);
    snprintf(message, sizeof(message), "initialize allocation %zu, persistent=%d", failureOrdinal, persistent);
    if (failureOrdinal != 0) {
        TEST_ASSERT_TRUE_MESSAGE(g_injectedFailures > 0, message);
        TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_LSP_HANDLER_INTERNAL_ERROR, response.status, message);
        TEST_ASSERT_FALSE_MESSAGE(hasResult, message);
        TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_STDIO_POSITION_ENCODING_UTF16, g_server->positionEncoding, message);
        TEST_ASSERT_FALSE_MESSAGE(g_server->supportsInlineCompletion, message);
        TEST_ASSERT_FALSE_MESSAGE(g_server->supportsRangesFormatting, message);
    } else {
        TEST_ASSERT_EQUAL_INT(ZR_LSP_HANDLER_OK, response.status);
        TEST_ASSERT_TRUE(hasResult);
    }
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(0, g_liveJson, message);
    return g_attempts;
}

/* 先测基准路径再逐个失败注入；分别覆盖可选能力存在与缺席的结果形状。 */
static void sweep_allocations(TZrBool persistent, TZrBool optional) {
    size_t count;
    if (!optional) {
        cJSON_Delete(g_params);
        g_params = cJSON_Parse("{\"capabilities\":{}}");
        TEST_ASSERT_NOT_NULL(g_params);
    }
    count = run_allocation_case(0, ZR_FALSE);
    TEST_ASSERT_TRUE(count > 0);
    for (size_t index = 1; index <= count; index++) {
        run_allocation_case(index, persistent);
    }
    printf("initialize profile optional=%d persistent=%d: %zu allocation points\n", optional, persistent, count);
}

/* 锁定原生分派器对外公布的核心能力、URI 工作区能力与 UTF-8 协商结果。 */
static void test_initialize_complete_capabilities(void) {
    SZrLspHandlerResult response = handle_initialize_request(g_server, g_params);
    const cJSON *capabilities;
    const cJSON *semantic;
    const cJSON *legend;
    const cJSON *workspace;
    const cJSON *operations;
    g_response = response.result;
    TEST_ASSERT_EQUAL_INT(ZR_LSP_HANDLER_OK, response.status);
    capabilities = get_object_item(g_response, ZR_LSP_FIELD_CAPABILITIES);
    TEST_ASSERT_TRUE(cJSON_IsObject(capabilities));
    TEST_ASSERT_TRUE(g_server->supportsInlineCompletion);
    TEST_ASSERT_TRUE(g_server->supportsRangesFormatting);
    TEST_ASSERT_EQUAL_STRING("utf-8", cJSON_GetStringValue(get_object_item(capabilities, ZR_LSP_FIELD_POSITION_ENCODING)));
    semantic = get_object_item(capabilities, ZR_LSP_FIELD_SEMANTIC_TOKENS_PROVIDER);
    legend = get_object_item(semantic, ZR_LSP_FIELD_LEGEND);
    TEST_ASSERT_EQUAL_INT(ZrLanguageServer_Lsp_SemanticTokenTypeCount(),
                          cJSON_GetArraySize(get_object_item(legend, ZR_LSP_FIELD_TOKEN_TYPES)));
    workspace = get_object_item(capabilities, ZR_LSP_FIELD_WORKSPACE);
    operations = get_object_item(workspace, ZR_LSP_FIELD_FILE_OPERATIONS);
    TEST_ASSERT_TRUE(cJSON_IsObject(get_object_item(operations, ZR_LSP_FIELD_DID_CREATE)));
    TEST_ASSERT_TRUE(cJSON_IsObject(get_object_item(operations, ZR_LSP_FIELD_WILL_RENAME)));
    TEST_ASSERT_TRUE(cJSON_IsObject(get_object_item(operations, ZR_LSP_FIELD_DID_RENAME)));
    TEST_ASSERT_TRUE(cJSON_IsObject(get_object_item(operations, ZR_LSP_FIELD_DID_DELETE)));
}

/* 客户端提供可选能力时，单点 OOM 仍应保持能力发布原子性。 */
static void test_initialize_transient_allocation_failures(void) { sweep_allocations(ZR_FALSE, ZR_TRUE); }
/* 持续 OOM 也必须让可选能力构造路径释放全部结果。 */
static void test_initialize_persistent_allocation_failures(void) { sweep_allocations(ZR_TRUE, ZR_TRUE); }
/* 基础客户端不提供可选能力时，同样扫描所有结果构造点。 */
static void test_initialize_base_transient_allocation_failures(void) { sweep_allocations(ZR_FALSE, ZR_FALSE); }
/* 基础能力的持续 OOM 路径不得留下部分已发布状态。 */
static void test_initialize_base_persistent_allocation_failures(void) { sweep_allocations(ZR_TRUE, ZR_FALSE); }

/* 直接处理器在分派层之外调用时仍需拒绝无效 params。 */
static void test_initialize_invalid_params(void) {
    SZrLspHandlerResult response = handle_initialize_request(g_server, ZR_NULL);
    g_response = response.result;
    TEST_ASSERT_EQUAL_INT(ZR_LSP_HANDLER_INVALID_PARAMS, response.status);
    TEST_ASSERT_NULL(g_response);
}

/* 为请求处理器的入口取消检查提供稳定回调。 */
static TZrBool always_cancelled(void *userData) {
    ZR_UNUSED_PARAMETER(userData);
    return ZR_TRUE;
}

/* 入口取消不得改变默认位置编码或可选能力状态。 */
static void test_initialize_cancelled_result_rolls_back_capabilities(void) {
    SZrLspHandlerResult response;
    ZrLanguageServer_LspContext_SetRequestCancellationCheck(g_server->context, always_cancelled, ZR_NULL);
    response = handle_initialize_request(g_server, g_params);
    g_response = response.result;
    ZrLanguageServer_LspContext_SetRequestCancellationCheck(g_server->context, ZR_NULL, ZR_NULL);
    TEST_ASSERT_EQUAL_INT(ZR_LSP_HANDLER_CANCELLED, response.status);
    TEST_ASSERT_NULL(g_response);
    TEST_ASSERT_FALSE(g_server->supportsInlineCompletion);
    TEST_ASSERT_FALSE(g_server->supportsRangesFormatting);
    TEST_ASSERT_EQUAL_INT(ZR_STDIO_POSITION_ENCODING_UTF16, g_server->positionEncoding);
}

/* 由分配 hook 在最后一个构造点触发，用于模拟响应生成末尾的取消。 */
static TZrBool allocation_cancelled(void *userData) {
    ZR_UNUSED_PARAMETER(userData);
    return g_cancelled;
}

/* 晚到取消已拥有结果树时，结果封装器仍须回收树并回滚协商开关。 */
static void test_initialize_late_cancellation_releases_result(void) {
    SZrLspHandlerResult response;
    size_t allocations = run_allocation_case(0, ZR_FALSE);
    g_server->positionEncoding = ZR_STDIO_POSITION_ENCODING_UTF16;
    g_server->supportsInlineCompletion = ZR_FALSE;
    g_server->supportsRangesFormatting = ZR_FALSE;
    g_cancelOrdinal = allocations;
    begin_json_tracking(0, ZR_FALSE);
    ZrLanguageServer_LspContext_SetRequestCancellationCheck(g_server->context, allocation_cancelled, ZR_NULL);
    response = handle_initialize_request(g_server, g_params);
    cJSON_Delete(response.result);
    cJSON_InitHooks(ZR_NULL);
    ZrLanguageServer_LspContext_SetRequestCancellationCheck(g_server->context, ZR_NULL, ZR_NULL);
    TEST_ASSERT_TRUE(g_cancelled);
    TEST_ASSERT_EQUAL_INT(ZR_LSP_HANDLER_CANCELLED, response.status);
    TEST_ASSERT_NULL(response.result);
    TEST_ASSERT_FALSE(g_server->supportsInlineCompletion);
    TEST_ASSERT_FALSE(g_server->supportsRangesFormatting);
    TEST_ASSERT_EQUAL_INT(ZR_STDIO_POSITION_ENCODING_UTF16, g_server->positionEncoding);
    TEST_ASSERT_EQUAL_UINT64(0, g_liveJson);
}

/* 走完整请求分派与真实帧发送，再读回 JSON-RPC 帧，核对生命周期只随成功发送推进。 */
static void capture_request(const char *method, size_t failureOrdinal) {
    char *payload = ZR_NULL;
    TZrSize length = 0;
    EZrStdioFrameReadStatus frameStatus;
    SZrStdioFrameReaderLimits limits;
    cJSON_Delete(g_response);
    g_response = ZR_NULL;
    g_output = tmpfile();
    TEST_ASSERT_NOT_NULL(g_output);
    redirect_stdout(g_output);
    begin_json_tracking(failureOrdinal, ZR_FALSE);
    handle_request_message(g_server, g_id, method, g_params);
    cJSON_InitHooks(ZR_NULL);
    restore_stdout();
    rewind(g_output);
    ZrLanguageServer_StdioFrameReader_DefaultLimits(&limits);
    frameStatus = ZrLanguageServer_StdioFrameReader_Read(g_output, &limits, &payload, &length);
    if (frameStatus == ZR_STDIO_FRAME_READ_OK) {
        g_response = cJSON_ParseWithLength(payload, length);
    }
    free(payload);
    fclose(g_output);
    g_output = ZR_NULL;
    TEST_ASSERT_EQUAL_INT(ZR_STDIO_FRAME_READ_OK, frameStatus);
    TEST_ASSERT_NOT_NULL(g_response);
    TEST_ASSERT_EQUAL_UINT64(0, g_liveJson);
}

/* 用只读流制造确定的发送失败，验证失败后允许 initialize/shutdown 重试。 */
static void write_request_to_readonly_stdout(const char *method) {
    g_output = fopen(__FILE__, "rb");

    TEST_ASSERT_NOT_NULL(g_output);
    redirect_stdout(g_output);
    handle_request_message(g_server, g_id, method, g_params);
    restore_stdout();
    fclose(g_output);
    g_output = ZR_NULL;
}

/* 处理器成功之后在响应外壳阶段注入 OOM，要求不输出部分帧。 */
static void fail_response_envelope_allocation(const char *method, size_t failureOrdinal) {
    g_output = tmpfile();
    TEST_ASSERT_NOT_NULL(g_output);
    redirect_stdout(g_output);
    begin_json_tracking(failureOrdinal, ZR_FALSE);
    handle_request_message(g_server, g_id, method, g_params);
    cJSON_InitHooks(ZR_NULL);
    restore_stdout();
    TEST_ASSERT_TRUE(g_injectedFailures > 0);
    TEST_ASSERT_EQUAL_INT64(0, ftell(g_output));
    TEST_ASSERT_EQUAL_UINT64(0, g_liveJson);
    fclose(g_output);
    g_output = ZR_NULL;
}

/* 从捕获帧读取协议错误码，使下列用例核对实际可见响应。 */
static void expect_error(int code) {
    const cJSON *error = get_object_item(g_response, ZR_LSP_JSON_RPC_FIELD_ERROR);
    const cJSON *actual = get_object_item(error, ZR_LSP_JSON_RPC_FIELD_CODE);
    TEST_ASSERT_TRUE(cJSON_IsNumber(actual));
    TEST_ASSERT_EQUAL_INT(code, actual->valueint);
}

/* 处理器 OOM 后请求门禁维持 NEW，客户端仍可重试 initialize。 */
static void test_initialize_failure_keeps_lifecycle_new(void) {
    capture_request(ZR_LSP_METHOD_INITIALIZE, 1);
    expect_error(ZR_LSP_JSON_RPC_INTERNAL_ERROR_CODE);
    TEST_ASSERT_TRUE(ZrLanguageServer_StdioLifecycle_IsNew(&g_server->lifecycle));
    TEST_ASSERT_FALSE(ZrLanguageServer_StdioLifecycle_CanProcessRequest(&g_server->lifecycle));
    capture_request(ZR_LSP_METHOD_TEXT_DOCUMENT_HOVER, 0);
    expect_error(ZR_LSP_JSON_RPC_SERVER_NOT_INITIALIZED_CODE);
    capture_request(ZR_LSP_METHOD_INITIALIZE, 0);
    TEST_ASSERT_TRUE(cJSON_IsObject(get_object_item(g_response, ZR_LSP_JSON_RPC_FIELD_RESULT)));
    TEST_ASSERT_TRUE(ZrLanguageServer_StdioLifecycle_CanProcessRequest(&g_server->lifecycle));
    capture_request(ZR_LSP_METHOD_INITIALIZE, 0);
    expect_error(ZR_LSP_JSON_RPC_INVALID_REQUEST_CODE);
}

/* 写出失败不得把未被客户端收到的 initialize 视作完成。 */
static void test_initialize_output_failure_keeps_lifecycle_new(void) {
    write_request_to_readonly_stdout(ZR_LSP_METHOD_INITIALIZE);
    TEST_ASSERT_TRUE(ZrLanguageServer_StdioLifecycle_IsNew(&g_server->lifecycle));
    TEST_ASSERT_FALSE(ZrLanguageServer_StdioLifecycle_CanProcessRequest(&g_server->lifecycle));
    capture_request(ZR_LSP_METHOD_INITIALIZE, 0);
    TEST_ASSERT_TRUE(cJSON_IsObject(get_object_item(g_response, ZR_LSP_JSON_RPC_FIELD_RESULT)));
}

/* shutdown 响应未送达时仍允许客户端再次请求关闭。 */
static void test_shutdown_output_failure_keeps_lifecycle_active(void) {
    capture_request(ZR_LSP_METHOD_INITIALIZE, 0);
    TEST_ASSERT_TRUE(ZrLanguageServer_StdioLifecycle_CanProcessRequest(&g_server->lifecycle));
    write_request_to_readonly_stdout(ZR_LSP_METHOD_SHUTDOWN);
    TEST_ASSERT_FALSE(ZrLanguageServer_StdioLifecycle_IsShutdown(&g_server->lifecycle));
    TEST_ASSERT_TRUE(ZrLanguageServer_StdioLifecycle_CanProcessRequest(&g_server->lifecycle));
    capture_request(ZR_LSP_METHOD_SHUTDOWN, 0);
    TEST_ASSERT_TRUE(ZrLanguageServer_StdioLifecycle_IsShutdown(&g_server->lifecycle));
}

/* 结果树成功但响应外壳失败时，不推进生命周期并允许重试。 */
static void test_initialize_envelope_failure_keeps_lifecycle_new(void) {
    size_t handlerAllocations = run_allocation_case(0, ZR_FALSE);

    fail_response_envelope_allocation(ZR_LSP_METHOD_INITIALIZE, handlerAllocations + 1);
    TEST_ASSERT_TRUE(ZrLanguageServer_StdioLifecycle_IsNew(&g_server->lifecycle));
    capture_request(ZR_LSP_METHOD_INITIALIZE, 0);
    TEST_ASSERT_TRUE(cJSON_IsObject(get_object_item(g_response, ZR_LSP_JSON_RPC_FIELD_RESULT)));
}

/* shutdown 外壳分配失败时，运行态请求门禁不得提前关闭。 */
static void test_shutdown_envelope_failure_keeps_lifecycle_running(void) {
    capture_request(ZR_LSP_METHOD_INITIALIZE, 0);
    ZrLanguageServer_StdioLifecycle_MarkInitialized(&g_server->lifecycle);
    fail_response_envelope_allocation(ZR_LSP_METHOD_SHUTDOWN, 1);
    TEST_ASSERT_EQUAL_INT(ZR_STDIO_LIFECYCLE_RUNNING, g_server->lifecycle.state);
    capture_request(ZR_LSP_METHOD_SHUTDOWN, 0);
    TEST_ASSERT_TRUE(ZrLanguageServer_StdioLifecycle_IsShutdown(&g_server->lifecycle));
}

/* 已激活请求被取消后，shutdown 不应提交状态；解除预留后才可成功关闭。 */
static void test_cancelled_shutdown_keeps_lifecycle_running(void) {
    capture_request(ZR_LSP_METHOD_INITIALIZE, 0);
    ZrLanguageServer_StdioLifecycle_MarkInitialized(&g_server->lifecycle);
    TEST_ASSERT_EQUAL_INT(ZR_STDIO_REQUEST_RESERVATION_ACCEPTED,
                         ZrLanguageServer_StdioRequestRegistry_Reserve(g_server->requestRegistry, g_id));
    ZrLanguageServer_StdioRequestInput_Activate(g_server, g_id);
    TEST_ASSERT_TRUE(ZrLanguageServer_StdioRequestRegistry_Cancel(g_server->requestRegistry, g_id));
    capture_request(ZR_LSP_METHOD_SHUTDOWN, 0);
    expect_error(ZR_LSP_JSON_RPC_REQUEST_CANCELLED_CODE);
    TEST_ASSERT_EQUAL_INT(ZR_STDIO_LIFECYCLE_RUNNING, g_server->lifecycle.state);
    ZrLanguageServer_StdioRequestInput_Complete(g_server, g_id);
    capture_request(ZR_LSP_METHOD_SHUTDOWN, 0);
    TEST_ASSERT_TRUE(cJSON_IsNull(get_object_item(g_response, ZR_LSP_JSON_RPC_FIELD_RESULT)));
    TEST_ASSERT_TRUE(ZrLanguageServer_StdioLifecycle_IsShutdown(&g_server->lifecycle));
}

/* 分派层取消 initialize 后仍未初始化，协商开关保持默认且允许新的请求重试。 */
static void test_initialize_cancelled_request_keeps_lifecycle_new(void) {
    TEST_ASSERT_EQUAL_INT(ZR_STDIO_REQUEST_RESERVATION_ACCEPTED,
                          ZrLanguageServer_StdioRequestRegistry_Reserve(g_server->requestRegistry, g_id));
    ZrLanguageServer_StdioRequestInput_Activate(g_server, g_id);
    TEST_ASSERT_TRUE(ZrLanguageServer_StdioRequestRegistry_Cancel(g_server->requestRegistry, g_id));
    capture_request(ZR_LSP_METHOD_INITIALIZE, 0);
    expect_error(ZR_LSP_JSON_RPC_REQUEST_CANCELLED_CODE);
    TEST_ASSERT_TRUE(ZrLanguageServer_StdioLifecycle_IsNew(&g_server->lifecycle));
    TEST_ASSERT_FALSE(g_server->supportsInlineCompletion);
    TEST_ASSERT_FALSE(g_server->supportsRangesFormatting);
    ZrLanguageServer_StdioRequestInput_Complete(g_server, g_id);
    capture_request(ZR_LSP_METHOD_TEXT_DOCUMENT_HOVER, 0);
    expect_error(ZR_LSP_JSON_RPC_SERVER_NOT_INITIALIZED_CODE);
    capture_request(ZR_LSP_METHOD_INITIALIZE, 0);
    TEST_ASSERT_TRUE(cJSON_IsObject(get_object_item(g_response, ZR_LSP_JSON_RPC_FIELD_RESULT)));
}

/* 由 stdio handler 的 CMake 测试矩阵登记为 initialize 可执行入口；
 * 各用例串行覆盖响应能力、故障注入、取消和生命周期边界。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_initialize_complete_capabilities);
    RUN_TEST(test_initialize_transient_allocation_failures);
    RUN_TEST(test_initialize_persistent_allocation_failures);
    RUN_TEST(test_initialize_base_transient_allocation_failures);
    RUN_TEST(test_initialize_base_persistent_allocation_failures);
    RUN_TEST(test_initialize_invalid_params);
    RUN_TEST(test_initialize_cancelled_result_rolls_back_capabilities);
    RUN_TEST(test_initialize_late_cancellation_releases_result);
    RUN_TEST(test_initialize_failure_keeps_lifecycle_new);
    RUN_TEST(test_initialize_output_failure_keeps_lifecycle_new);
    RUN_TEST(test_shutdown_output_failure_keeps_lifecycle_active);
    RUN_TEST(test_initialize_envelope_failure_keeps_lifecycle_new);
    RUN_TEST(test_shutdown_envelope_failure_keeps_lifecycle_running);
    RUN_TEST(test_cancelled_shutdown_keeps_lifecycle_running);
    RUN_TEST(test_initialize_cancelled_request_keeps_lifecycle_new);
    return UNITY_END();
}
