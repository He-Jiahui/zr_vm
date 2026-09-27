#include "zr_vm_language_server_stdio_internal.h"
#include "stdio_request_progress.h"
#include "unity.h"

/* 独立 Unity 可执行程序串行驱动请求；JSON 树由夹具持有，通知替身仅转移所有权。 */
static SZrStdioServer g_server;
static SZrLspContext g_context;
static cJSON *g_notifications;
static cJSON *g_requestId;
static cJSON *g_cancelId;
static cJSON *g_params;
static cJSON *g_result;
static int g_partialCount;
static int g_cancelAtBatch;
static EZrStdioSendStatus g_sendStatus;
/* cJSON 全局分配钩子只在目标调用期间启用；不覆盖 registry 的 libc 分配。 */
static size_t g_jsonAttempts;
static size_t g_jsonFailAt;
static size_t g_jsonFailures;
static TZrBool g_persistentFailure;

/* 支持单点和持续 OOM，验证通知构造在每一个分配点都能保留调用方结果。 */
static void *json_malloc(size_t size) {
    g_jsonAttempts++;
    if (g_jsonFailAt != 0 &&
        (g_jsonAttempts == g_jsonFailAt || (g_persistentFailure && g_jsonAttempts > g_jsonFailAt))) {
        g_jsonFailures++;
        return ZR_NULL;
    }
    return malloc(size);
}

/* 在夹具建好后开启故障注入，避免把 setup 失败误记为被测发布路径的失败。 */
static void begin_json_tracking(size_t failAt, TZrBool persistent) {
    cJSON_Hooks hooks = {json_malloc, free};
    g_jsonAttempts = 0;
    g_jsonFailAt = failAt;
    g_jsonFailures = 0;
    g_persistentFailure = persistent;
    cJSON_InitHooks(&hooks);
}

/* 代替 stdio 主循环安装的取消回调，把当前 ID 的注册表状态暴露给 progress。 */
static TZrBool request_is_cancelled(void *userData) {
    SZrStdioServer *server = (SZrStdioServer *)userData;
    return ZrLanguageServer_StdioRequestRegistry_IsCancelled(server->requestRegistry,
                                                            server->activeRequestId);
}

/* 链接替身消费 params，并在可重复的发送边界触发取消；不验证真实帧写出。 */
EZrStdioSendStatus send_notification(const char *method, cJSON *params) {
    const cJSON *value;
    const cJSON *items;

    if (g_sendStatus != ZR_STDIO_SEND_OK) {
        cJSON_Delete(params);
        return g_sendStatus;
    }

    value = cJSON_GetObjectItemCaseSensitive(params, ZR_LSP_FIELD_VALUE);
    items = cJSON_GetObjectItemCaseSensitive(value, ZR_LSP_FIELD_ITEMS);

    TEST_ASSERT_EQUAL_STRING(ZR_LSP_METHOD_PROGRESS, method);
    cJSON_AddItemToArray(g_notifications, params);
    if (cJSON_IsArray(value) || cJSON_IsArray(items)) {
        g_partialCount++;
        if (g_partialCount == g_cancelAtBatch) {
            TEST_ASSERT_TRUE(ZrLanguageServer_StdioRequestRegistry_Cancel(
                    g_server.requestRegistry, g_cancelId != ZR_NULL ? g_cancelId : g_requestId));
        }
    }
    return ZR_STDIO_SEND_OK;
}

/* 每例预留数值 ID 7 并安装回调，模拟主循环激活一个请求后的状态。 */
void setUp(void) {
    memset(&g_server, 0, sizeof(g_server));
    memset(&g_context, 0, sizeof(g_context));
    g_server.context = &g_context;
    g_server.requestRegistry = ZrLanguageServer_StdioRequestRegistry_New();
    g_requestId = cJSON_CreateNumber(7);
    g_notifications = cJSON_CreateArray();
    g_params = cJSON_Parse("{\"partialResultToken\":\"partial\",\"workDoneToken\":\"work\"}");
    g_cancelId = ZR_NULL;
    g_result = ZR_NULL;
    g_partialCount = 0;
    g_cancelAtBatch = 0;
    g_sendStatus = ZR_STDIO_SEND_OK;
    TEST_ASSERT_NOT_NULL(g_server.requestRegistry);
    TEST_ASSERT_NOT_NULL(g_requestId);
    TEST_ASSERT_NOT_NULL(g_notifications);
    TEST_ASSERT_NOT_NULL(g_params);
    TEST_ASSERT_EQUAL(ZR_STDIO_REQUEST_RESERVATION_ACCEPTED,
                     ZrLanguageServer_StdioRequestRegistry_Reserve(g_server.requestRegistry,
                                                                  g_requestId));
    g_server.activeRequestId = g_requestId;
    ZrLanguageServer_LspContext_SetRequestCancellationCheck(&g_context, request_is_cancelled,
                                                           &g_server);
}

/* 先恢复全局钩子并解绑回调，再释放树和注册表，隔离断言提前退出后的下一例。 */
void tearDown(void) {
    cJSON_InitHooks(ZR_NULL);
    ZrLanguageServer_LspContext_SetRequestCancellationCheck(&g_context, ZR_NULL, ZR_NULL);
    ZrLanguageServer_StdioRequestRegistry_Free(g_server.requestRegistry);
    cJSON_Delete(g_requestId);
    cJSON_Delete(g_cancelId);
    cJSON_Delete(g_notifications);
    cJSON_Delete(g_params);
    cJSON_Delete(g_result);
}

/* 统一构造数组或 workspace diagnostic 对象，begin 通知固定占通知列表第零项。 */
static void prepare_result(const char *method, int itemCount) {
    cJSON *items = cJSON_CreateArray();
    TEST_ASSERT_NOT_NULL(items);
    g_result = items;
    for (int index = 0; index < itemCount; index++) {
        TEST_ASSERT_TRUE(cJSON_AddItemToArray(items, cJSON_CreateNumber(index)));
    }
    if (strcmp(method, ZR_LSP_METHOD_WORKSPACE_DIAGNOSTIC) == 0) {
        g_result = cJSON_CreateObject();
        TEST_ASSERT_NOT_NULL(g_result);
        TEST_ASSERT_TRUE(cJSON_AddItemToObject(g_result, ZR_LSP_FIELD_ITEMS, items));
    }
    TEST_ASSERT_TRUE(stdio_request_progress_prepare(&g_server, method, g_params));
    TEST_ASSERT_TRUE(stdio_request_progress_begin(&g_server, method));
}

/* 借用捕获通知中的数组并核对 token；普通数组和 diagnostic.items 共用断言入口。 */
static const cJSON *partial_items(int notificationIndex, TZrBool workspaceDiagnostic) {
    const cJSON *params = cJSON_GetArrayItem(g_notifications, notificationIndex);
    const cJSON *token = cJSON_GetObjectItemCaseSensitive(params, ZR_LSP_FIELD_TOKEN);
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(params, ZR_LSP_FIELD_VALUE);
    TEST_ASSERT_EQUAL_STRING("partial", cJSON_GetStringValue((cJSON *)token));
    return workspaceDiagnostic ? cJSON_GetObjectItemCaseSensitive(value, ZR_LSP_FIELD_ITEMS)
                               : value;
}

/* 成功和取消路径都必须配对 begin/end，并结束对 params 内 token 的借用。 */
static void expect_progress_ended(void) {
    const cJSON *begin;
    const cJSON *end;
    stdio_request_progress_end(&g_server);
    begin = cJSON_GetArrayItem(g_notifications, 0);
    end = cJSON_GetArrayItem(g_notifications, cJSON_GetArraySize(g_notifications) - 1);
    TEST_ASSERT_EQUAL_STRING("work", cJSON_GetStringValue(
            cJSON_GetObjectItemCaseSensitive(begin, ZR_LSP_FIELD_TOKEN)));
    TEST_ASSERT_EQUAL_STRING("work", cJSON_GetStringValue(
            cJSON_GetObjectItemCaseSensitive(end, ZR_LSP_FIELD_TOKEN)));
    TEST_ASSERT_EQUAL_STRING(ZR_LSP_PROGRESS_KIND_BEGIN, cJSON_GetStringValue(
            cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(begin,
                    ZR_LSP_FIELD_VALUE), ZR_LSP_FIELD_KIND)));
    TEST_ASSERT_EQUAL_STRING(ZR_LSP_PROGRESS_KIND_END, cJSON_GetStringValue(
            cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(end,
                    ZR_LSP_FIELD_VALUE), ZR_LSP_FIELD_KIND)));
    TEST_ASSERT_NULL(g_server.requestProgress.partialResultToken);
    TEST_ASSERT_NULL(g_server.requestProgress.workDoneToken);
    TEST_ASSERT_FALSE(g_server.requestProgress.workDoneBegan);
}

/* 首批后的取消不得继续发送余项，也不能提前消费原结果。 */
static void test_cancel_at_first_batch_stops_remaining_results(void) {
    cJSON *original;
    g_cancelAtBatch = 1;
    prepare_result(ZR_LSP_METHOD_WORKSPACE_SYMBOL, 129);
    original = g_result;
    TEST_ASSERT_FALSE(stdio_request_progress_publish_partial_result(
            &g_server, ZR_LSP_METHOD_WORKSPACE_SYMBOL, &g_result));
    TEST_ASSERT_EQUAL_PTR(original, g_result);
    TEST_ASSERT_EQUAL_INT(1, g_partialCount);
    TEST_ASSERT_EQUAL_INT(64, cJSON_GetArraySize(partial_items(1, ZR_FALSE)));
    TEST_ASSERT_TRUE(ZrLanguageServer_LspContext_IsRequestCancellationRequested(&g_context));
    expect_progress_ended();
}

/* 末批发布边界仍可能收到取消，不能因为已发完就提交成功完成值。 */
static void test_cancel_at_last_batch_does_not_complete_with_null(void) {
    cJSON *original;
    g_cancelAtBatch = 2;
    prepare_result(ZR_LSP_METHOD_WORKSPACE_SYMBOL, 65);
    original = g_result;
    TEST_ASSERT_FALSE(stdio_request_progress_publish_partial_result(
            &g_server, ZR_LSP_METHOD_WORKSPACE_SYMBOL, &g_result));
    TEST_ASSERT_EQUAL_PTR(original, g_result);
    TEST_ASSERT_EQUAL_INT(2, g_partialCount);
    TEST_ASSERT_EQUAL_INT(1, cJSON_GetArraySize(partial_items(2, ZR_FALSE)));
    expect_progress_ended();
}

/* 对象包裹的 diagnostic 批次也必须遵守末批取消检查。 */
static void test_workspace_diagnostic_last_batch_observes_cancellation(void) {
    cJSON *original;
    g_cancelAtBatch = 1;
    prepare_result(ZR_LSP_METHOD_WORKSPACE_DIAGNOSTIC, 1);
    original = g_result;
    TEST_ASSERT_FALSE(stdio_request_progress_publish_partial_result(
            &g_server, ZR_LSP_METHOD_WORKSPACE_DIAGNOSTIC, &g_result));
    TEST_ASSERT_EQUAL_PTR(original, g_result);
    TEST_ASSERT_EQUAL_INT(1, g_partialCount);
    TEST_ASSERT_EQUAL_INT(1, cJSON_GetArraySize(partial_items(1, ZR_TRUE)));
    expect_progress_ended();
}

/* 字符串 "7" 与数值 7 的预留独立，取消通知不能跨 ID 类型误伤。 */
static void test_string_id_cancellation_does_not_cancel_numeric_request(void) {
    g_cancelId = cJSON_CreateString("7");
    TEST_ASSERT_NOT_NULL(g_cancelId);
    TEST_ASSERT_EQUAL(ZR_STDIO_REQUEST_RESERVATION_ACCEPTED,
                     ZrLanguageServer_StdioRequestRegistry_Reserve(g_server.requestRegistry,
                                                                  g_cancelId));
    g_cancelAtBatch = 1;
    prepare_result(ZR_LSP_METHOD_WORKSPACE_SYMBOL, 65);
    TEST_ASSERT_TRUE(stdio_request_progress_publish_partial_result(
            &g_server, ZR_LSP_METHOD_WORKSPACE_SYMBOL, &g_result));
    TEST_ASSERT_TRUE(cJSON_IsNull(g_result));
    TEST_ASSERT_EQUAL_INT(2, g_partialCount);
    TEST_ASSERT_FALSE(ZrLanguageServer_LspContext_IsRequestCancellationRequested(&g_context));
    TEST_ASSERT_TRUE(ZrLanguageServer_StdioRequestRegistry_IsCancelled(
            g_server.requestRegistry, g_cancelId));
    expect_progress_ended();
}

/* 跨越 64 项边界，锁定 diagnostic.items 的封装、顺序与最终空报告形状。 */
static void test_workspace_diagnostic_batches_preserve_items_and_order(void) {
    const cJSON *first;
    const cJSON *last;
    prepare_result(ZR_LSP_METHOD_WORKSPACE_DIAGNOSTIC, 65);
    TEST_ASSERT_TRUE(stdio_request_progress_publish_partial_result(
            &g_server, ZR_LSP_METHOD_WORKSPACE_DIAGNOSTIC, &g_result));
    TEST_ASSERT_TRUE(cJSON_IsObject(g_result));
    TEST_ASSERT_TRUE(cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(g_result, ZR_LSP_FIELD_ITEMS)));
    TEST_ASSERT_EQUAL_INT(0, cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(g_result, ZR_LSP_FIELD_ITEMS)));
    TEST_ASSERT_EQUAL_INT(2, g_partialCount);
    first = partial_items(1, ZR_TRUE);
    last = partial_items(2, ZR_TRUE);
    TEST_ASSERT_EQUAL_INT(64, cJSON_GetArraySize(first));
    TEST_ASSERT_EQUAL_INT(1, cJSON_GetArraySize(last));
    TEST_ASSERT_EQUAL_INT(0, cJSON_GetArrayItem(first, 0)->valueint);
    TEST_ASSERT_EQUAL_INT(63, cJSON_GetArrayItem(first, 63)->valueint);
    TEST_ASSERT_EQUAL_INT(64, cJSON_GetArrayItem(last, 0)->valueint);
    expect_progress_ended();
}

/* 空报告无需 partial 通知，最终响应仍保留 WorkspaceDiagnosticReport 对象。 */
static void test_empty_workspace_diagnostic_completes_with_empty_report(void) {
    prepare_result(ZR_LSP_METHOD_WORKSPACE_DIAGNOSTIC, 0);
    TEST_ASSERT_TRUE(stdio_request_progress_publish_partial_result(
            &g_server, ZR_LSP_METHOD_WORKSPACE_DIAGNOSTIC, &g_result));
    TEST_ASSERT_TRUE(cJSON_IsObject(g_result));
    TEST_ASSERT_TRUE(cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(g_result, ZR_LSP_FIELD_ITEMS)));
    TEST_ASSERT_EQUAL_INT(0, cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(g_result, ZR_LSP_FIELD_ITEMS)));
    TEST_ASSERT_EQUAL_INT(0, g_partialCount);
    expect_progress_ended();
}

/* 缺少 partial token 时只能走完整对象响应，work-done 进度仍正常配对。 */
static void test_workspace_diagnostic_without_partial_token_preserves_full_report(void) {
    cJSON *original;
    cJSON_DeleteItemFromObjectCaseSensitive(g_params, ZR_LSP_FIELD_PARTIAL_RESULT_TOKEN);
    prepare_result(ZR_LSP_METHOD_WORKSPACE_DIAGNOSTIC, 65);
    original = g_result;
    TEST_ASSERT_TRUE(stdio_request_progress_publish_partial_result(
            &g_server, ZR_LSP_METHOD_WORKSPACE_DIAGNOSTIC, &g_result));
    TEST_ASSERT_EQUAL_PTR(original, g_result);
    TEST_ASSERT_EQUAL_INT(65, cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(g_result, ZR_LSP_FIELD_ITEMS)));
    TEST_ASSERT_EQUAL_INT(0, g_partialCount);
    expect_progress_ended();
}

/* 普通数组也不因 workDoneToken 存在就被错误地流式消费。 */
static void test_omitted_partial_token_preserves_ordinary_result(void) {
    cJSON *original;
    cJSON_DeleteItemFromObjectCaseSensitive(g_params, ZR_LSP_FIELD_PARTIAL_RESULT_TOKEN);
    prepare_result(ZR_LSP_METHOD_WORKSPACE_SYMBOL, 65);
    original = g_result;
    TEST_ASSERT_TRUE(stdio_request_progress_publish_partial_result(
            &g_server, ZR_LSP_METHOD_WORKSPACE_SYMBOL, &g_result));
    TEST_ASSERT_EQUAL_PTR(original, g_result);
    TEST_ASSERT_EQUAL_INT(65, cJSON_GetArraySize(g_result));
    TEST_ASSERT_EQUAL_INT(0, g_partialCount);
    expect_progress_ended();
}

/* 输出失败不能建立已开始状态，避免后续错误路径发出无对应 begin 的 end。 */
static void test_work_done_begin_does_not_commit_when_publication_fails(void) {
    TEST_ASSERT_TRUE(stdio_request_progress_prepare(
            &g_server, ZR_LSP_METHOD_WORKSPACE_SYMBOL, g_params));
    g_sendStatus = ZR_STDIO_SEND_IO_ERROR;
    TEST_ASSERT_FALSE(stdio_request_progress_begin(&g_server, ZR_LSP_METHOD_WORKSPACE_SYMBOL));
    TEST_ASSERT_FALSE(g_server.requestProgress.workDoneBegan);
    TEST_ASSERT_EQUAL_INT(0, cJSON_GetArraySize(g_notifications));
    stdio_request_progress_clear(&g_server);
}

/* 发布失败后调用方仍持有原结果，才能走统一错误清理路径。 */
static void test_partial_result_publication_failure_preserves_result(void) {
    cJSON *original;

    prepare_result(ZR_LSP_METHOD_WORKSPACE_SYMBOL, 65);
    original = g_result;
    g_sendStatus = ZR_STDIO_SEND_BUILD_ERROR;
    TEST_ASSERT_FALSE(stdio_request_progress_publish_partial_result(
            &g_server, ZR_LSP_METHOD_WORKSPACE_SYMBOL, &g_result));
    TEST_ASSERT_EQUAL_PTR(original, g_result);
    TEST_ASSERT_EQUAL_INT(0, g_partialCount);
    TEST_ASSERT_EQUAL_INT(1, cJSON_GetArraySize(g_notifications));
    g_sendStatus = ZR_STDIO_SEND_OK;
    expect_progress_ended();
}

/* 一次运行只注入 begin 构造阶段；零故障调用提供后续逐点扫描的动态上界。 */
static size_t run_work_done_allocation_case(size_t failAt, TZrBool persistent) {
    TZrBool success;

    cJSON_Delete(g_notifications);
    g_notifications = cJSON_CreateArray();
    TEST_ASSERT_NOT_NULL(g_notifications);
    TEST_ASSERT_TRUE(stdio_request_progress_prepare(&g_server, ZR_LSP_METHOD_WORKSPACE_SYMBOL, g_params));
    begin_json_tracking(failAt, persistent);
    success = stdio_request_progress_begin(&g_server, ZR_LSP_METHOD_WORKSPACE_SYMBOL);
    cJSON_InitHooks(ZR_NULL);
    if (failAt != 0) {
        TEST_ASSERT_TRUE(g_jsonFailures > 0);
        TEST_ASSERT_FALSE(success);
        TEST_ASSERT_FALSE(g_server.requestProgress.workDoneBegan);
        TEST_ASSERT_EQUAL_INT(0, cJSON_GetArraySize(g_notifications));
    } else {
        TEST_ASSERT_TRUE(success);
        TEST_ASSERT_TRUE(g_server.requestProgress.workDoneBegan);
        TEST_ASSERT_EQUAL_INT(1, cJSON_GetArraySize(g_notifications));
    }
    stdio_request_progress_clear(&g_server);
    return g_jsonAttempts;
}

/* 同时扫描单次 OOM 与持续 OOM，防止失败恢复分支再次分配时漏掉错误。 */
static void test_work_done_allocation_failures_do_not_publish(void) {
    size_t count = run_work_done_allocation_case(0, ZR_FALSE);

    for (size_t index = 1; index <= count; index++) {
        run_work_done_allocation_case(index, ZR_FALSE);
        run_work_done_allocation_case(index, ZR_TRUE);
    }
    printf("work-done begin: %zu allocation points\n", count);
}

/* 已发送批次不可撤回；失败仍须保留原结果，并保证捕获到的每一批完整有序。 */
static size_t run_partial_allocation_case(const char *method, size_t failAt, TZrBool persistent) {
    cJSON *original;
    TZrBool success;
    TZrBool workspaceDiagnostic = strcmp(method, ZR_LSP_METHOD_WORKSPACE_DIAGNOSTIC) == 0;

    cJSON_Delete(g_result);
    g_result = ZR_NULL;
    cJSON_Delete(g_notifications);
    g_notifications = cJSON_CreateArray();
    TEST_ASSERT_NOT_NULL(g_notifications);
    g_partialCount = 0;
    prepare_result(method, 65);
    original = g_result;
    begin_json_tracking(failAt, persistent);
    success = stdio_request_progress_publish_partial_result(&g_server, method, &g_result);
    cJSON_InitHooks(ZR_NULL);
    if (failAt != 0) {
        TEST_ASSERT_TRUE(g_jsonFailures > 0);
        TEST_ASSERT_FALSE(success);
        TEST_ASSERT_EQUAL_PTR(original, g_result);
    } else {
        TEST_ASSERT_TRUE(success);
        if (workspaceDiagnostic) {
            TEST_ASSERT_TRUE(cJSON_IsObject(g_result));
            TEST_ASSERT_TRUE(cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(g_result, ZR_LSP_FIELD_ITEMS)));
            TEST_ASSERT_EQUAL_INT(0, cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(g_result, ZR_LSP_FIELD_ITEMS)));
        } else {
            TEST_ASSERT_TRUE(cJSON_IsNull(g_result));
        }
        TEST_ASSERT_EQUAL_INT(2, g_partialCount);
    }
    TEST_ASSERT_EQUAL_INT(g_partialCount + 1, cJSON_GetArraySize(g_notifications));
    for (int index = 0; index < g_partialCount; index++) {
        const cJSON *items = partial_items(index + 1, workspaceDiagnostic);
        TEST_ASSERT_EQUAL_INT(index == 0 ? 64 : 1, cJSON_GetArraySize(items));
        TEST_ASSERT_EQUAL_INT(index * 64, cJSON_GetArrayItem(items, 0)->valueint);
    }
    stdio_request_progress_clear(&g_server);
    return g_jsonAttempts;
}

/* 用同一故障矩阵覆盖普通数组与 diagnostic 对象的不同所有权转移路径。 */
static void sweep_partial_allocations(const char *method) {
    size_t count = run_partial_allocation_case(method, 0, ZR_FALSE);

    for (size_t index = 1; index <= count; index++) {
        run_partial_allocation_case(method, index, ZR_FALSE);
        run_partial_allocation_case(method, index, ZR_TRUE);
    }
    printf("partial result %s: %zu allocation points\n", method, count);
}

/* 数组 partial 的任意分配失败不得替换调用方结果。 */
static void test_array_partial_allocation_failures_preserve_result(void) {
    sweep_partial_allocations(ZR_LSP_METHOD_WORKSPACE_SYMBOL);
}

/* diagnostic 额外对象和最终空 items 的分配失败也须维持同一所有权契约。 */
static void test_workspace_diagnostic_partial_allocation_failures_preserve_result(void) {
    sweep_partial_allocations(ZR_LSP_METHOD_WORKSPACE_DIAGNOSTIC);
}

/* CMake 通过 zr_vm_lsp_stdio_progress_tests.cmake 注册此 Unity 可执行程序。
 * 这里集中列出取消、批次和分配失败的回归场景。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_cancel_at_first_batch_stops_remaining_results);
    RUN_TEST(test_cancel_at_last_batch_does_not_complete_with_null);
    RUN_TEST(test_workspace_diagnostic_last_batch_observes_cancellation);
    RUN_TEST(test_string_id_cancellation_does_not_cancel_numeric_request);
    RUN_TEST(test_workspace_diagnostic_batches_preserve_items_and_order);
    RUN_TEST(test_empty_workspace_diagnostic_completes_with_empty_report);
    RUN_TEST(test_workspace_diagnostic_without_partial_token_preserves_full_report);
    RUN_TEST(test_omitted_partial_token_preserves_ordinary_result);
    RUN_TEST(test_work_done_begin_does_not_commit_when_publication_fails);
    RUN_TEST(test_partial_result_publication_failure_preserves_result);
    RUN_TEST(test_work_done_allocation_failures_do_not_publish);
    RUN_TEST(test_array_partial_allocation_failures_preserve_result);
    RUN_TEST(test_workspace_diagnostic_partial_allocation_failures_preserve_result);
    return UNITY_END();
}
