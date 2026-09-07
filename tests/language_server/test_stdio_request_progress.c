#include "zr_vm_language_server_stdio_internal.h"
#include "stdio_request_progress.h"
#include "unity.h"

static SZrStdioServer g_server;
static SZrLspContext g_context;
static cJSON *g_notifications;
static cJSON *g_requestId;
static cJSON *g_cancelId;
static cJSON *g_params;
static cJSON *g_result;
static int g_partialCount;
static int g_cancelAtBatch;

static TZrBool request_is_cancelled(void *userData) {
    SZrStdioServer *server = (SZrStdioServer *)userData;
    return ZrLanguageServer_StdioRequestRegistry_IsCancelled(server->requestRegistry,
                                                            server->activeRequestId);
}

/* Link this receiver in place of the transport to cancel at an exact send boundary. */
void send_notification(const char *method, cJSON *params) {
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(params, ZR_LSP_FIELD_VALUE);
    const cJSON *items = cJSON_GetObjectItemCaseSensitive(value, ZR_LSP_FIELD_ITEMS);

    TEST_ASSERT_EQUAL_STRING(ZR_LSP_METHOD_PROGRESS, method);
    cJSON_AddItemToArray(g_notifications, params);
    if (cJSON_IsArray(value) || cJSON_IsArray(items)) {
        g_partialCount++;
        if (g_partialCount == g_cancelAtBatch) {
            TEST_ASSERT_TRUE(ZrLanguageServer_StdioRequestRegistry_Cancel(
                    g_server.requestRegistry, g_cancelId != ZR_NULL ? g_cancelId : g_requestId));
        }
    }
}

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

void tearDown(void) {
    ZrLanguageServer_LspContext_SetRequestCancellationCheck(&g_context, ZR_NULL, ZR_NULL);
    ZrLanguageServer_StdioRequestRegistry_Free(g_server.requestRegistry);
    cJSON_Delete(g_requestId);
    cJSON_Delete(g_cancelId);
    cJSON_Delete(g_notifications);
    cJSON_Delete(g_params);
    cJSON_Delete(g_result);
}

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

static const cJSON *partial_items(int notificationIndex, TZrBool workspaceDiagnostic) {
    const cJSON *params = cJSON_GetArrayItem(g_notifications, notificationIndex);
    const cJSON *token = cJSON_GetObjectItemCaseSensitive(params, ZR_LSP_FIELD_TOKEN);
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(params, ZR_LSP_FIELD_VALUE);
    TEST_ASSERT_EQUAL_STRING("partial", cJSON_GetStringValue((cJSON *)token));
    return workspaceDiagnostic ? cJSON_GetObjectItemCaseSensitive(value, ZR_LSP_FIELD_ITEMS)
                               : value;
}

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

static void test_workspace_diagnostic_batches_preserve_items_and_order(void) {
    const cJSON *first;
    const cJSON *last;
    prepare_result(ZR_LSP_METHOD_WORKSPACE_DIAGNOSTIC, 65);
    TEST_ASSERT_TRUE(stdio_request_progress_publish_partial_result(
            &g_server, ZR_LSP_METHOD_WORKSPACE_DIAGNOSTIC, &g_result));
    TEST_ASSERT_TRUE(cJSON_IsNull(g_result));
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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_cancel_at_first_batch_stops_remaining_results);
    RUN_TEST(test_cancel_at_last_batch_does_not_complete_with_null);
    RUN_TEST(test_workspace_diagnostic_last_batch_observes_cancellation);
    RUN_TEST(test_string_id_cancellation_does_not_cancel_numeric_request);
    RUN_TEST(test_workspace_diagnostic_batches_preserve_items_and_order);
    RUN_TEST(test_omitted_partial_token_preserves_ordinary_result);
    return UNITY_END();
}
