#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "zr_vm_language_server_stdio_internal.h"
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
static SZrString *g_uri;
static FILE *g_output;
static int g_savedStdout;
static cJSON *g_message;
static size_t g_jsonAttempts;
static size_t g_jsonFailAt;
static size_t g_jsonFailures;
static size_t g_liveJson;
static TZrBool g_persistentFailure;
#ifdef _WIN32
static int g_stdoutMode;
#endif

static void *json_malloc(size_t size) {
    void *result;

    g_jsonAttempts++;
    if (g_jsonFailAt != 0 &&
        (g_jsonAttempts == g_jsonFailAt || (g_persistentFailure && g_jsonAttempts > g_jsonFailAt))) {
        g_jsonFailures++;
        return ZR_NULL;
    }
    result = malloc(size);
    if (result != ZR_NULL) {
        g_liveJson++;
    }
    return result;
}

static void json_free(void *pointer) {
    if (pointer != ZR_NULL) {
        g_liveJson--;
        free(pointer);
    }
}

static void begin_json_tracking(size_t failAt, TZrBool persistent) {
    cJSON_Hooks hooks = {json_malloc, json_free};
    g_jsonAttempts = 0;
    g_jsonFailAt = failAt;
    g_jsonFailures = 0;
    g_persistentFailure = persistent;
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
        clearerr(stdout);
    }
}

static void redirect_stdout(FILE *output) {
    fflush(stdout);
#ifdef _WIN32
    g_stdoutMode = _setmode(test_fileno(stdout), _O_BINARY);
#endif
    g_savedStdout = test_dup(test_fileno(stdout));
    TEST_ASSERT_TRUE(g_savedStdout >= 0);
    TEST_ASSERT_TRUE(test_dup2(test_fileno(output), test_fileno(stdout)) >= 0);
}

static void publish_current_diagnostics(TZrBool empty) {
    if (empty) {
        publish_empty_diagnostics(g_server, g_uri);
    } else {
        publish_diagnostics(g_server, g_uri);
    }
}

static long capture_diagnostics(TZrBool empty) {
    long bytes;
    char *payload = ZR_NULL;
    TZrSize length = 0;
    SZrStdioFrameReaderLimits limits;

    g_output = tmpfile();
    TEST_ASSERT_NOT_NULL(g_output);
    redirect_stdout(g_output);
    publish_current_diagnostics(empty);
    cJSON_InitHooks(ZR_NULL);
    restore_stdout();
    bytes = ftell(g_output);
    cJSON_Delete(g_message);
    g_message = ZR_NULL;
    if (bytes > 0) {
        rewind(g_output);
        ZrLanguageServer_StdioFrameReader_DefaultLimits(&limits);
        TEST_ASSERT_EQUAL_INT(ZR_STDIO_FRAME_READ_OK,
                             ZrLanguageServer_StdioFrameReader_Read(g_output, &limits, &payload, &length));
        g_message = cJSON_ParseWithLength(payload, length);
        free(payload);
        TEST_ASSERT_NOT_NULL(g_message);
        TEST_ASSERT_EQUAL_STRING(ZR_LSP_METHOD_TEXT_DOCUMENT_PUBLISH_DIAGNOSTICS,
                                cJSON_GetStringValue(get_object_item(g_message, "method")));
        TEST_ASSERT_EQUAL_STRING("file:///diagnostic-publication.zr",
                                cJSON_GetStringValue(get_object_item(get_object_item(g_message, "params"), "uri")));
        TEST_ASSERT_TRUE(cJSON_IsArray(get_object_item(get_object_item(g_message, "params"), "diagnostics")));
    }
    fclose(g_output);
    g_output = ZR_NULL;
    return bytes;
}

static void publish_to_readonly_stdout(TZrBool empty) {
    g_output = fopen(__FILE__, "rb");

    TEST_ASSERT_NOT_NULL(g_output);
    redirect_stdout(g_output);
    publish_current_diagnostics(empty);
    restore_stdout();
    fclose(g_output);
    g_output = ZR_NULL;
}

void setUp(void) {
    g_savedStdout = -1;
    g_output = ZR_NULL;
    g_message = ZR_NULL;
    g_liveJson = 0;
    g_server = ZrLanguageServer_StdioServer_New(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_server);
    g_uri = server_get_cached_uri(g_server, "file:///diagnostic-publication.zr");
    TEST_ASSERT_NOT_NULL(g_uri);
}

void tearDown(void) {
    cJSON_InitHooks(ZR_NULL);
    restore_stdout();
    if (g_output != ZR_NULL) {
        fclose(g_output);
        g_output = ZR_NULL;
    }
    cJSON_Delete(g_message);
    ZrLanguageServer_StdioServer_Free(g_server);
    g_server = ZR_NULL;
    g_uri = ZR_NULL;
    TEST_ASSERT_EQUAL_UINT64(0, g_liveJson);
}

static void test_empty_diagnostics_cache_commits_after_successful_publication(void) {
    long bytes = capture_diagnostics(ZR_TRUE);

    TEST_ASSERT_TRUE(bytes > 0L);
    TEST_ASSERT_EQUAL_UINT(1U, g_server->diagnosticPushCache.count);
    bytes = capture_diagnostics(ZR_TRUE);
    TEST_ASSERT_EQUAL_INT64(0, bytes);
    TEST_ASSERT_EQUAL_UINT(1U, g_server->diagnosticPushCache.count);
}

static void test_empty_diagnostics_write_failure_does_not_update_cache(void) {
    long bytes;

    publish_to_readonly_stdout(ZR_TRUE);
    TEST_ASSERT_EQUAL_UINT(0U, g_server->diagnosticPushCache.count);
    bytes = capture_diagnostics(ZR_TRUE);
    TEST_ASSERT_TRUE(bytes > 0L);
    TEST_ASSERT_EQUAL_UINT(1U, g_server->diagnosticPushCache.count);
}

static void update_document(TZrSize version, TZrBool withDiagnostic) {
    const char *source = withDiagnostic
                            ? "fn main(): int {\n var amount: int = 3.75;\n return 0;\n}\n"
                            : "fn publicationTarget(): int { return 1; }\n";

    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_UpdateDocument(
            g_server->state, g_server->context, g_uri, source, strlen(source), version));
}

static void test_diagnostic_write_failure_preserves_previous_published_version(void) {
    update_document(1, ZR_FALSE);
    TEST_ASSERT_TRUE(capture_diagnostics(ZR_FALSE) > 0L);
    TEST_ASSERT_EQUAL_UINT(1U, g_server->diagnosticPushCache.count);
    TEST_ASSERT_TRUE(g_server->diagnosticPushCache.items[0].hasDocumentVersion);
    TEST_ASSERT_EQUAL_UINT(1U, g_server->diagnosticPushCache.items[0].documentVersion);
    update_document(2, ZR_FALSE);
    publish_to_readonly_stdout(ZR_FALSE);
    TEST_ASSERT_EQUAL_UINT(1U, g_server->diagnosticPushCache.items[0].documentVersion);
    TEST_ASSERT_TRUE(capture_diagnostics(ZR_FALSE) > 0L);
    TEST_ASSERT_EQUAL_UINT(2U, g_server->diagnosticPushCache.items[0].documentVersion);
    TEST_ASSERT_EQUAL_INT(2, get_object_item(get_object_item(g_message, "params"), "version")->valueint);
    TEST_ASSERT_EQUAL_INT64(0, capture_diagnostics(ZR_FALSE));
}

static size_t run_allocation_case(TZrBool empty, size_t failAt, TZrBool persistent) {
    long bytes;

    for (size_t index = 0; index < g_server->diagnosticPushCache.count; index++) {
        free(g_server->diagnosticPushCache.items[index].uriText);
    }
    free(g_server->diagnosticPushCache.items);
    memset(&g_server->diagnosticPushCache, 0, sizeof(g_server->diagnosticPushCache));
    begin_json_tracking(failAt, persistent);
    bytes = capture_diagnostics(empty);
    if (failAt != 0) {
        TEST_ASSERT_TRUE(g_jsonFailures > 0);
        TEST_ASSERT_EQUAL_INT64(0, bytes);
        TEST_ASSERT_EQUAL_UINT(0U, g_server->diagnosticPushCache.count);
    } else {
        TEST_ASSERT_TRUE(bytes > 0L);
        TEST_ASSERT_EQUAL_UINT(1U, g_server->diagnosticPushCache.count);
    }
    TEST_ASSERT_EQUAL_UINT64(0, g_liveJson);
    return g_jsonAttempts;
}

static void sweep_allocations(TZrBool empty, TZrBool withDiagnostic) {
    size_t count;

    if (!empty) {
        update_document(1, withDiagnostic);
    }
    count = run_allocation_case(empty, 0, ZR_FALSE);
    if (withDiagnostic) {
        TEST_ASSERT_TRUE(cJSON_GetArraySize(get_object_item(get_object_item(g_message, "params"), "diagnostics")) > 0);
    }
    for (size_t index = 1; index <= count; index++) {
        run_allocation_case(empty, index, ZR_FALSE);
        run_allocation_case(empty, index, ZR_TRUE);
    }
    printf("diagnostic publication empty=%d withDiagnostic=%d: %zu allocation points\n", empty, withDiagnostic, count);
}

static void test_empty_diagnostic_allocation_failures_do_not_publish(void) {
    sweep_allocations(ZR_TRUE, ZR_FALSE);
}

static void test_document_diagnostic_allocation_failures_do_not_publish(void) {
    sweep_allocations(ZR_FALSE, ZR_FALSE);
}

static void test_nonempty_diagnostic_allocation_failures_do_not_publish(void) {
    sweep_allocations(ZR_FALSE, ZR_TRUE);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_empty_diagnostics_cache_commits_after_successful_publication);
    RUN_TEST(test_empty_diagnostics_write_failure_does_not_update_cache);
    RUN_TEST(test_diagnostic_write_failure_preserves_previous_published_version);
    RUN_TEST(test_empty_diagnostic_allocation_failures_do_not_publish);
    RUN_TEST(test_document_diagnostic_allocation_failures_do_not_publish);
    RUN_TEST(test_nonempty_diagnostic_allocation_failures_do_not_publish);
    return UNITY_END();
}
