#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "zr_vm_language_server_stdio_internal.h"
#include "stdio_frame_reader.h"
#include "unity.h"

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

static void tracked_json_free(void *pointer) {
    if (pointer != ZR_NULL) {
        g_liveJson--;
        free(pointer);
    }
}

static void begin_json_tracking(size_t failureOrdinal, TZrBool persistent) {
    cJSON_Hooks hooks = {tracked_json_malloc, tracked_json_free};
    g_attempts = 0;
    g_failureOrdinal = failureOrdinal;
    g_injectedFailures = 0;
    g_liveJson = 0;
    g_persistent = persistent;
    cJSON_InitHooks(&hooks);
}

static void restore_stdout(void) {
    if (g_savedStdout >= 0) {
        fflush(stdout);
        test_dup2(g_savedStdout, test_fileno(stdout));
        test_close(g_savedStdout);
        g_savedStdout = -1;
#ifdef _WIN32
        _setmode(test_fileno(stdout), g_stdoutMode);
#endif
    }
}

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

static void test_initialize_transient_allocation_failures(void) { sweep_allocations(ZR_FALSE, ZR_TRUE); }
static void test_initialize_persistent_allocation_failures(void) { sweep_allocations(ZR_TRUE, ZR_TRUE); }
static void test_initialize_base_transient_allocation_failures(void) { sweep_allocations(ZR_FALSE, ZR_FALSE); }
static void test_initialize_base_persistent_allocation_failures(void) { sweep_allocations(ZR_TRUE, ZR_FALSE); }

static void test_initialize_invalid_params(void) {
    SZrLspHandlerResult response = handle_initialize_request(g_server, ZR_NULL);
    g_response = response.result;
    TEST_ASSERT_EQUAL_INT(ZR_LSP_HANDLER_INVALID_PARAMS, response.status);
    TEST_ASSERT_NULL(g_response);
}

static TZrBool always_cancelled(void *userData) {
    ZR_UNUSED_PARAMETER(userData);
    return ZR_TRUE;
}

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

static TZrBool allocation_cancelled(void *userData) {
    ZR_UNUSED_PARAMETER(userData);
    return g_cancelled;
}

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

static void capture_request(const char *method, size_t failureOrdinal) {
    char *payload = ZR_NULL;
    TZrSize length = 0;
    EZrStdioFrameReadStatus frameStatus;
    SZrStdioFrameReaderLimits limits;
    cJSON_Delete(g_response);
    g_response = ZR_NULL;
    g_output = tmpfile();
    TEST_ASSERT_NOT_NULL(g_output);
    fflush(stdout);
#ifdef _WIN32
    g_stdoutMode = _setmode(test_fileno(stdout), _O_BINARY);
#endif
    g_savedStdout = test_dup(test_fileno(stdout));
    TEST_ASSERT_TRUE(g_savedStdout >= 0);
    TEST_ASSERT_TRUE(test_dup2(test_fileno(g_output), test_fileno(stdout)) >= 0);
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

static void expect_error(int code) {
    const cJSON *error = get_object_item(g_response, ZR_LSP_JSON_RPC_FIELD_ERROR);
    const cJSON *actual = get_object_item(error, ZR_LSP_JSON_RPC_FIELD_CODE);
    TEST_ASSERT_TRUE(cJSON_IsNumber(actual));
    TEST_ASSERT_EQUAL_INT(code, actual->valueint);
}

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
    RUN_TEST(test_initialize_cancelled_request_keeps_lifecycle_new);
    return UNITY_END();
}
