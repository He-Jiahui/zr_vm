#include "zr_vm_language_server_stdio_internal.h"
#include "unity.h"

typedef enum EHandlerQuery {
    QUERY_WORKSPACE_SYMBOLS,
    QUERY_DOCUMENT_SYMBOLS,
    QUERY_REFERENCES,
    QUERY_RENAME,
    QUERY_HIGHLIGHTS,
} EHandlerQuery;

static const char g_source[] =
        "fn cancellationTarget(value: int): int { return value; }\n"
        "fn cancellationCallerA(): int { return cancellationTarget(1); }\n"
        "fn cancellationCallerB(): int { return cancellationTarget(2); }\n";

static SZrStdioServer *g_server;
static SZrString *g_uri;
static SZrString *g_query;
static SZrString *g_newName;
static SZrArray g_probe;
static cJSON *g_params;
static cJSON *g_response;
static EHandlerQuery g_kind;
static EZrLspHandlerStatus g_handlerStatus;
static size_t g_liveBlocks;
static size_t g_jsonAllocationAttempts;
static size_t g_checkCount;
static size_t g_cancelAtCheck;
static TZrBool g_cancelObserved;

static const char *g_navigationMethods[] = {
        ZR_LSP_METHOD_TEXT_DOCUMENT_HOVER,
        ZR_LSP_METHOD_ZR_RICH_HOVER,
        ZR_LSP_METHOD_TEXT_DOCUMENT_SIGNATURE_HELP,
        ZR_LSP_METHOD_TEXT_DOCUMENT_INLAY_HINT,
        ZR_LSP_METHOD_TEXT_DOCUMENT_DEFINITION,
        ZR_LSP_METHOD_ZR_NATIVE_DECLARATION_DOCUMENT,
        ZR_LSP_METHOD_TEXT_DOCUMENT_REFERENCES,
        ZR_LSP_METHOD_TEXT_DOCUMENT_DOCUMENT_SYMBOL,
        ZR_LSP_METHOD_WORKSPACE_SYMBOL,
        ZR_LSP_METHOD_TEXT_DOCUMENT_DOCUMENT_HIGHLIGHT,
};

static void *fail_json_allocation(size_t size) {
    ZR_UNUSED_PARAMETER(size);
    return ZR_NULL;
}

static void *fail_first_json_allocation(size_t size) {
    return g_jsonAllocationAttempts++ == 0 ? ZR_NULL : malloc(size);
}

static TZrPtr tracking_allocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize,
                                TZrSize newSize, TZrInt64 flag) {
    TZrPtr result;
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(originalSize);
    ZR_UNUSED_PARAMETER(flag);
    if (newSize == 0) {
        if (pointer != ZR_NULL) {
            g_liveBlocks--;
        }
        free(pointer);
        return ZR_NULL;
    }
    if (pointer == ZR_NULL) {
        result = malloc(newSize);
        if (result != ZR_NULL) {
            g_liveBlocks++;
        }
        return result;
    }
    return realloc(pointer, newSize);
}

static TZrBool calibrate_cancellation(void *userData) {
    const SZrArray *result = (const SZrArray *)userData;
    g_checkCount++;
    if (result->length > 0) {
        g_cancelAtCheck = g_checkCount;
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

static TZrBool cancel_at_calibrated_check(void *userData) {
    ZR_UNUSED_PARAMETER(userData);
    g_checkCount++;
    if (g_checkCount >= g_cancelAtCheck) {
        g_cancelObserved = ZR_TRUE;
    }
    return g_cancelObserved;
}

static void free_probe(void) {
    if (g_kind == QUERY_WORKSPACE_SYMBOLS || g_kind == QUERY_DOCUMENT_SYMBOLS) {
        free_symbols_array(g_server->state, &g_probe);
    } else if (g_kind == QUERY_HIGHLIGHTS) {
        free_highlights_array(g_server->state, &g_probe);
    } else {
        free_locations_array(g_server->state, &g_probe);
    }
    memset(&g_probe, 0, sizeof(g_probe));
}

void setUp(void) {
    SZrCallbackGlobal callbacks = {0};
    g_liveBlocks = 0;
    g_checkCount = 0;
    g_cancelAtCheck = 0;
    g_cancelObserved = ZR_FALSE;
    g_jsonAllocationAttempts = 0;
    g_params = ZR_NULL;
    g_response = ZR_NULL;
    memset(&g_probe, 0, sizeof(g_probe));
    g_server = (SZrStdioServer *)calloc(1, sizeof(*g_server));
    TEST_ASSERT_NOT_NULL(g_server);
    g_server->global = ZrCore_GlobalState_New(tracking_allocator, ZR_NULL, 0, &callbacks);
    TEST_ASSERT_NOT_NULL(g_server->global);
    g_server->state = g_server->global->mainThreadState;
    TEST_ASSERT_NOT_NULL(g_server->state);
    ZrCore_GlobalState_InitRegistry(g_server->state, g_server->global);
    g_server->context = ZrLanguageServer_LspContext_New(g_server->state);
    TEST_ASSERT_NOT_NULL(g_server->context);
    g_uri = server_get_cached_uri(g_server, "file:///handler-cancellation.zr");
    g_query = ZrCore_String_Create(g_server->state, "", 0);
    g_newName = ZrCore_String_Create(g_server->state, "renamedTarget", strlen("renamedTarget"));
    TEST_ASSERT_NOT_NULL(g_uri);
    TEST_ASSERT_NOT_NULL(g_query);
    TEST_ASSERT_NOT_NULL(g_newName);
    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_UpdateDocument(
            g_server->state, g_server->context, g_uri, g_source, strlen(g_source), 1));
    g_params = cJSON_Parse("{\"textDocument\":{\"uri\":\"file:///handler-cancellation.zr\"},"
                          "\"position\":{\"line\":0,\"character\":3},\"query\":\"\","
                          "\"uri\":\"file:///handler-cancellation.zr\","
                          "\"range\":{\"start\":{\"line\":0,\"character\":0},"
                          "\"end\":{\"line\":2,\"character\":1}},"
                          "\"context\":{\"includeDeclaration\":true},\"newName\":\"renamedTarget\"}");
    TEST_ASSERT_NOT_NULL(g_params);
}

void tearDown(void) {
    cJSON_Delete(g_response);
    cJSON_Delete(g_params);
    if (g_server != ZR_NULL) {
        ZrLanguageServer_LspContext_SetRequestCancellationCheck(g_server->context, ZR_NULL, ZR_NULL);
        if (g_probe.isValid) {
            free_probe();
        }
        ZrLanguageServer_StdioServer_Free(g_server);
        g_server = ZR_NULL;
    }
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(0, g_liveBlocks, "cancelled handler must release every runtime allocation");
}

static TZrBool run_provider(void) {
    SZrLspPosition position = {0, 3};
    switch (g_kind) {
        case QUERY_WORKSPACE_SYMBOLS:
            return ZrLanguageServer_Lsp_GetWorkspaceSymbols(g_server->state, g_server->context,
                                                            g_query, &g_probe);
        case QUERY_DOCUMENT_SYMBOLS:
            return ZrLanguageServer_Lsp_GetDocumentSymbols(g_server->state, g_server->context,
                                                           g_uri, &g_probe);
        case QUERY_REFERENCES:
            return ZrLanguageServer_Lsp_FindReferences(g_server->state, g_server->context,
                                                       g_uri, position, ZR_TRUE, &g_probe);
        case QUERY_RENAME:
            return ZrLanguageServer_Lsp_Rename(g_server->state, g_server->context,
                                               g_uri, position, g_newName, &g_probe);
        case QUERY_HIGHLIGHTS:
            return ZrLanguageServer_Lsp_GetDocumentHighlights(g_server->state, g_server->context,
                                                              g_uri, position, &g_probe);
    }
    return ZR_FALSE;
}

static cJSON *run_handler(void) {
    static const char *methods[] = {
            ZR_LSP_METHOD_WORKSPACE_SYMBOL,
            ZR_LSP_METHOD_TEXT_DOCUMENT_DOCUMENT_SYMBOL,
            ZR_LSP_METHOD_TEXT_DOCUMENT_REFERENCES,
            ZR_LSP_METHOD_TEXT_DOCUMENT_RENAME,
            ZR_LSP_METHOD_TEXT_DOCUMENT_DOCUMENT_HIGHLIGHT,
    };
    cJSON *result = ZR_NULL;
    TEST_ASSERT_TRUE(dispatch_request_method(g_server, methods[g_kind], g_params,
                                            &result, &g_handlerStatus));
    return result;
}

static void expect_cancelled_handler_cleanup(EHandlerQuery kind) {
    g_kind = kind;
    TEST_ASSERT_TRUE(run_provider());
    TEST_ASSERT_TRUE_MESSAGE(g_probe.length > 1, "fixture must provide multiple results");
    free_probe();

    // Calibrate against the provider's first result instead of assuming a fixed check count.
    ZrLanguageServer_LspContext_SetRequestCancellationCheck(
            g_server->context, calibrate_cancellation, &g_probe);
    TEST_ASSERT_FALSE(run_provider());
    TEST_ASSERT_EQUAL_UINT64(1, g_probe.length);
    TEST_ASSERT_TRUE(g_cancelAtCheck > 0);
    ZrLanguageServer_LspContext_SetRequestCancellationCheck(g_server->context, ZR_NULL, ZR_NULL);
    free_probe();

    g_checkCount = 0;
    ZrLanguageServer_LspContext_SetRequestCancellationCheck(
            g_server->context, cancel_at_calibrated_check, ZR_NULL);
    g_response = run_handler();
    TEST_ASSERT_TRUE_MESSAGE(g_cancelObserved, "handler must reach the calibrated cancellation check");
    if (g_kind == QUERY_RENAME) {
        TEST_ASSERT_EQUAL_INT(ZR_LSP_HANDLER_OK, g_handlerStatus);
        TEST_ASSERT_TRUE(cJSON_IsNull(g_response) ||
                         (cJSON_IsArray(g_response) && cJSON_GetArraySize(g_response) == 0));
    } else {
        TEST_ASSERT_EQUAL_INT(ZR_LSP_HANDLER_CANCELLED, g_handlerStatus);
        TEST_ASSERT_NULL(g_response);
    }
    ZrLanguageServer_LspContext_SetRequestCancellationCheck(g_server->context, ZR_NULL, ZR_NULL);
}

static void test_workspace_symbols_release_cancelled_result(void) {
    expect_cancelled_handler_cleanup(QUERY_WORKSPACE_SYMBOLS);
}

static void test_document_symbols_release_cancelled_result(void) {
    expect_cancelled_handler_cleanup(QUERY_DOCUMENT_SYMBOLS);
}

static void test_references_release_cancelled_result(void) {
    expect_cancelled_handler_cleanup(QUERY_REFERENCES);
}

static void test_rename_release_cancelled_result(void) {
    expect_cancelled_handler_cleanup(QUERY_RENAME);
}

static void test_highlights_release_cancelled_result(void) {
    expect_cancelled_handler_cleanup(QUERY_HIGHLIGHTS);
}

static void test_ordinary_handlers_release_results(void) {
    for (g_kind = QUERY_WORKSPACE_SYMBOLS; g_kind <= QUERY_HIGHLIGHTS; g_kind++) {
        g_response = run_handler();
        TEST_ASSERT_EQUAL_INT(ZR_LSP_HANDLER_OK, g_handlerStatus);
        if (g_kind == QUERY_RENAME) {
            TEST_ASSERT_TRUE(cJSON_IsObject(g_response));
        } else {
            TEST_ASSERT_TRUE(cJSON_IsArray(g_response));
            TEST_ASSERT_TRUE(cJSON_GetArraySize(g_response) > 1);
        }
        cJSON_Delete(g_response);
        g_response = ZR_NULL;
    }
}

static void expect_navigation_allocation_failure(void *(*allocator)(size_t)) {
    cJSON_Hooks hooks = {allocator, free};
    for (size_t index = 0; index < sizeof(g_navigationMethods) / sizeof(g_navigationMethods[0]); index++) {
        EZrLspHandlerStatus status = ZR_LSP_HANDLER_OK;
        int handled;
        g_jsonAllocationAttempts = 0;
        cJSON_InitHooks(&hooks);
        handled = dispatch_request_method(g_server, g_navigationMethods[index], g_params, &g_response, &status);
        cJSON_InitHooks(ZR_NULL);
        TEST_ASSERT_TRUE(handled);
        TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_LSP_HANDLER_INTERNAL_ERROR, status, g_navigationMethods[index]);
        TEST_ASSERT_NULL(g_response);
    }
}

static void test_navigation_json_allocation_failure_is_internal(void) {
    expect_navigation_allocation_failure(fail_json_allocation);
}

static void test_navigation_first_json_allocation_failure_is_internal(void) {
    expect_navigation_allocation_failure(fail_first_json_allocation);
}

static void test_navigation_cancelled_status_is_explicit(void) {
    ZrLanguageServer_LspContext_SetRequestCancellationCheck(
            g_server->context, cancel_at_calibrated_check, ZR_NULL);
    for (size_t index = 0; index < sizeof(g_navigationMethods) / sizeof(g_navigationMethods[0]); index++) {
        EZrLspHandlerStatus status = ZR_LSP_HANDLER_OK;
        TEST_ASSERT_TRUE(dispatch_request_method(g_server, g_navigationMethods[index], g_params,
                                                &g_response, &status));
        TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_LSP_HANDLER_CANCELLED, status, g_navigationMethods[index]);
        TEST_ASSERT_NULL(g_response);
    }
    TEST_ASSERT_TRUE(g_cancelObserved);
}

static void test_navigation_invalid_params_remain_invalid(void) {
    for (size_t index = 0; index < sizeof(g_navigationMethods) / sizeof(g_navigationMethods[0]); index++) {
        EZrLspHandlerStatus status = ZR_LSP_HANDLER_OK;
        TEST_ASSERT_TRUE(dispatch_request_method(g_server, g_navigationMethods[index], ZR_NULL,
                                                &g_response, &status));
        TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_LSP_HANDLER_INVALID_PARAMS, status, g_navigationMethods[index]);
        TEST_ASSERT_NULL(g_response);
    }
}

static void test_navigation_valid_results_remain_successful(void) {
    for (size_t index = 0; index < sizeof(g_navigationMethods) / sizeof(g_navigationMethods[0]); index++) {
        EZrLspHandlerStatus status = ZR_LSP_HANDLER_INTERNAL_ERROR;
        TEST_ASSERT_TRUE(dispatch_request_method(g_server, g_navigationMethods[index], g_params,
                                                &g_response, &status));
        TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_LSP_HANDLER_OK, status, g_navigationMethods[index]);
        TEST_ASSERT_NOT_NULL(g_response);
        cJSON_Delete(g_response);
        g_response = ZR_NULL;
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_navigation_json_allocation_failure_is_internal);
    RUN_TEST(test_navigation_first_json_allocation_failure_is_internal);
    RUN_TEST(test_navigation_cancelled_status_is_explicit);
    RUN_TEST(test_navigation_invalid_params_remain_invalid);
    RUN_TEST(test_navigation_valid_results_remain_successful);
    RUN_TEST(test_ordinary_handlers_release_results);
    RUN_TEST(test_workspace_symbols_release_cancelled_result);
    RUN_TEST(test_document_symbols_release_cancelled_result);
    RUN_TEST(test_references_release_cancelled_result);
    RUN_TEST(test_rename_release_cancelled_result);
    RUN_TEST(test_highlights_release_cancelled_result);
    return UNITY_END();
}
