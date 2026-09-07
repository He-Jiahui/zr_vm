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

typedef enum EOutputCase {
    OUTPUT_RESULT,
    OUTPUT_NULL_RESULT,
    OUTPUT_ERROR,
    OUTPUT_NOTIFICATION,
    OUTPUT_NULL_NOTIFICATION
} EOutputCase;

static size_t g_attempts;
static size_t g_failAt;
static size_t g_live;
static size_t g_failures;
static TZrBool g_persistent;
static FILE *g_output;
static int g_savedStdout;
#ifdef _WIN32
static int g_stdoutMode;
#endif
static cJSON *g_id;

static void *json_malloc(size_t size) {
    void *pointer;
    g_attempts++;
    if (g_failAt != 0 && (g_attempts == g_failAt || (g_persistent && g_attempts > g_failAt))) {
        g_failures++;
        return NULL;
    }
    pointer = malloc(size);
    if (pointer != NULL) {
        g_live++;
    }
    return pointer;
}

static void json_free(void *pointer) {
    if (pointer != NULL) {
        g_live--;
        free(pointer);
    }
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

void setUp(void) {
    g_savedStdout = -1;
    g_output = NULL;
    g_live = 0;
    g_id = cJSON_CreateString("request-identity");
    TEST_ASSERT_NOT_NULL(g_id);
}

void tearDown(void) {
    restore_stdout();
    cJSON_InitHooks(NULL);
    if (g_output != NULL) {
        fclose(g_output);
        g_output = NULL;
    }
    cJSON_Delete(g_id);
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(0, g_live, "transport must release all owned JSON allocations");
}

static void capture_stdout(void) {
    g_output = tmpfile();
    TEST_ASSERT_NOT_NULL(g_output);
    fflush(stdout);
#ifdef _WIN32
    g_stdoutMode = _setmode(test_fileno(stdout), _O_BINARY);
#endif
    g_savedStdout = test_dup(test_fileno(stdout));
    TEST_ASSERT_TRUE(g_savedStdout >= 0);
    TEST_ASSERT_TRUE(test_dup2(test_fileno(g_output), test_fileno(stdout)) >= 0);
}

static size_t run_output_case(EOutputCase outputCase, size_t failAt, TZrBool persistent) {
    cJSON_Hooks hooks = {json_malloc, json_free};
    cJSON *owned = NULL;
    char *payload = NULL;
    TZrSize length = 0;
    SZrStdioFrameReaderLimits limits;
    EZrStdioFrameReadStatus readStatus;
    EZrStdioSendStatus sendStatus = ZR_STDIO_SEND_BUILD_ERROR;
    cJSON *message;
    char description[100];
    size_t attempts;
    long bytes;

    capture_stdout();
    g_attempts = 0;
    g_failAt = 0;
    g_failures = 0;
    g_live = 0;
    g_persistent = persistent;
    cJSON_InitHooks(&hooks);
    if (outputCase == OUTPUT_RESULT || outputCase == OUTPUT_NOTIFICATION) {
        owned = cJSON_Parse("{\"items\":[\"one\",\"two\"]}");
    }
    g_attempts = 0;
    g_failAt = failAt;
    switch (outputCase) {
        case OUTPUT_RESULT:
        case OUTPUT_NULL_RESULT:
            sendStatus = send_result_response(g_id, owned);
            break;
        case OUTPUT_ERROR:
            sendStatus = send_error_response(g_id, -32603, "Internal error");
            break;
        case OUTPUT_NOTIFICATION:
        case OUTPUT_NULL_NOTIFICATION:
            sendStatus = send_notification("$/progress", owned);
            break;
    }
    attempts = g_attempts;
    cJSON_InitHooks(NULL);
    restore_stdout();
    bytes = ftell(g_output);
    snprintf(description, sizeof(description), "output case %d allocation %zu persistent %d",
             outputCase, failAt, persistent);
    if (failAt != 0) {
        TEST_ASSERT_TRUE_MESSAGE(g_failures > 0, description);
        TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_STDIO_SEND_BUILD_ERROR, sendStatus, description);
        TEST_ASSERT_EQUAL_INT64_MESSAGE(0, bytes, description);
    } else {
        TEST_ASSERT_EQUAL_INT(ZR_STDIO_SEND_OK, sendStatus);
        rewind(g_output);
        ZrLanguageServer_StdioFrameReader_DefaultLimits(&limits);
        readStatus = ZrLanguageServer_StdioFrameReader_Read(g_output, &limits, &payload, &length);
        TEST_ASSERT_EQUAL_INT(ZR_STDIO_FRAME_READ_OK, readStatus);
        message = cJSON_ParseWithLength(payload, length);
        free(payload);
        TEST_ASSERT_NOT_NULL(message);
        TEST_ASSERT_EQUAL_STRING("2.0", cJSON_GetStringValue(get_object_item(message, "jsonrpc")));
        if (outputCase == OUTPUT_RESULT || outputCase == OUTPUT_NULL_RESULT || outputCase == OUTPUT_ERROR) {
            TEST_ASSERT_TRUE(cJSON_Compare(g_id, get_object_item(message, "id"), 1));
            TEST_ASSERT_EQUAL_INT(3, cJSON_GetArraySize(message));
            TEST_ASSERT_NOT_NULL(get_object_item(message, outputCase == OUTPUT_ERROR ? "error" : "result"));
        } else {
            TEST_ASSERT_EQUAL_STRING("$/progress", cJSON_GetStringValue(get_object_item(message, "method")));
            TEST_ASSERT_EQUAL_INT(3, cJSON_GetArraySize(message));
            TEST_ASSERT_NOT_NULL(get_object_item(message, "params"));
        }
        cJSON_Delete(message);
    }
    fclose(g_output);
    g_output = NULL;
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(0, g_live, description);
    return attempts;
}

static void sweep_output(EOutputCase outputCase) {
    size_t allocations = run_output_case(outputCase, 0, ZR_FALSE);
    TEST_ASSERT_TRUE(allocations > 0);
    for (size_t index = 1; index <= allocations; index++) {
        run_output_case(outputCase, index, ZR_FALSE);
        run_output_case(outputCase, index, ZR_TRUE);
    }
    printf("transport output case %d: %zu allocation points\n", outputCase, allocations);
}

static void test_result_allocation_failures(void) { sweep_output(OUTPUT_RESULT); }
static void test_null_result_allocation_failures(void) { sweep_output(OUTPUT_NULL_RESULT); }
static void test_error_allocation_failures(void) { sweep_output(OUTPUT_ERROR); }
static void test_notification_allocation_failures(void) { sweep_output(OUTPUT_NOTIFICATION); }
static void test_null_notification_allocation_failures(void) { sweep_output(OUTPUT_NULL_NOTIFICATION); }

static void test_output_preserves_typed_id(void) {
    const char *ids[] = {"null", "1", "-9007199254740991", "9007199254740991", "\"escaped\\\"id\\n\""};
    for (size_t index = 0; index < sizeof(ids) / sizeof(ids[0]); index++) {
        cJSON_Delete(g_id);
        g_id = cJSON_Parse(ids[index]);
        TEST_ASSERT_NOT_NULL(g_id);
        run_output_case(OUTPUT_RESULT, 0, ZR_FALSE);
        run_output_case(OUTPUT_ERROR, 0, ZR_FALSE);
    }
}

static void test_output_write_failure_releases_json(void) {
    cJSON_Hooks hooks = {json_malloc, json_free};
    EZrStdioSendStatus status;
    g_output = fopen(__FILE__, "rb");
    TEST_ASSERT_NOT_NULL(g_output);
    fflush(stdout);
#ifdef _WIN32
    g_stdoutMode = _setmode(test_fileno(stdout), _O_BINARY);
#endif
    g_savedStdout = test_dup(test_fileno(stdout));
    TEST_ASSERT_TRUE(g_savedStdout >= 0);
    TEST_ASSERT_TRUE(test_dup2(test_fileno(g_output), test_fileno(stdout)) >= 0);
    g_failAt = 0;
    cJSON_InitHooks(&hooks);
    status = send_result_response(g_id, cJSON_CreateArray());
    cJSON_InitHooks(NULL);
    restore_stdout();
    TEST_ASSERT_EQUAL_INT(ZR_STDIO_SEND_IO_ERROR, status);
    TEST_ASSERT_EQUAL_UINT64(0, g_live);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_result_allocation_failures);
    RUN_TEST(test_null_result_allocation_failures);
    RUN_TEST(test_error_allocation_failures);
    RUN_TEST(test_notification_allocation_failures);
    RUN_TEST(test_null_notification_allocation_failures);
    RUN_TEST(test_output_preserves_typed_id);
    RUN_TEST(test_output_write_failure_releases_json);
    return UNITY_END();
}
