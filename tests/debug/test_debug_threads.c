#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON/cJSON.h"
#include "runtime_support.h"
#include "unity.h"
#include "zr_vm_lib_debug/debug.h"
#include "zr_vm_lib_network/network.h"
#include "zr_vm_parser.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef struct ZrThreadDebugExecution {
    SZrState *state;
    SZrFunction *function;
    TZrInt64 result;
    TZrBool success;
#if defined(_WIN32)
    HANDLE handle;
#else
    pthread_t handle;
#endif
} ZrThreadDebugExecution;

static SZrFunction *compile_thread_debug_source(SZrState *state, const char *sourceLabel, const char *source) {
    SZrString *sourceName;

    if (state == ZR_NULL || sourceLabel == ZR_NULL || source == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state, (TZrNativeString)sourceLabel, strlen(sourceLabel));
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static SZrFunction *compile_thread_debug_fixture(SZrState *state, const char *sourceLabel) {
    const char *source =
            "func addOne(value: int): int {\n"
            "    var base = value + 1;\n"
            "    return base;\n"
            "}\n"
            "var first = addOne(4);\n"
            "var second = first + 2;\n"
            "return second;";

    return compile_thread_debug_source(state, sourceLabel, source);
}

static void thread_debug_execution_run(ZrThreadDebugExecution *thread) {
    if (thread == ZR_NULL) {
        return;
    }

    thread->success = ZrTests_Runtime_Function_ExecuteExpectInt64(thread->state, thread->function, &thread->result);
}

#if defined(_WIN32)
static DWORD WINAPI thread_debug_execution_proc(LPVOID argument) {
    thread_debug_execution_run((ZrThreadDebugExecution *)argument);
    return 0;
}
#else
static void *thread_debug_execution_proc(void *argument) {
    thread_debug_execution_run((ZrThreadDebugExecution *)argument);
    return ZR_NULL;
}
#endif

static TZrBool thread_debug_execution_start(ZrThreadDebugExecution *thread) {
    if (thread == ZR_NULL) {
        return ZR_FALSE;
    }

    thread->result = 0;
    thread->success = ZR_FALSE;
#if defined(_WIN32)
    thread->handle = CreateThread(NULL, 0, thread_debug_execution_proc, thread, 0, NULL);
    return thread->handle != NULL ? ZR_TRUE : ZR_FALSE;
#else
    return pthread_create(&thread->handle, NULL, thread_debug_execution_proc, thread) == 0 ? ZR_TRUE : ZR_FALSE;
#endif
}

static void thread_debug_execution_join(ZrThreadDebugExecution *thread) {
    if (thread == ZR_NULL) {
        return;
    }

#if defined(_WIN32)
    if (thread->handle != NULL) {
        WaitForSingleObject(thread->handle, INFINITE);
        CloseHandle(thread->handle);
        thread->handle = NULL;
    }
#else
    pthread_join(thread->handle, NULL);
#endif
}

static void thread_debug_client_connect(ZrDebugAgent *agent, SZrNetworkStream *stream) {
    TZrChar endpointText[ZR_NETWORK_ENDPOINT_TEXT_CAPACITY];
    TZrChar error[256];
    SZrNetworkEndpoint endpoint;

    TEST_ASSERT_NOT_NULL(agent);
    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_TRUE(ZrDebug_AgentGetEndpoint(agent, endpointText, sizeof(endpointText)));
    TEST_ASSERT_TRUE(ZrNetwork_ParseEndpoint(endpointText, &endpoint, error, sizeof(error)));
    TEST_ASSERT_TRUE(ZrNetwork_StreamConnectLoopback(&endpoint, 3000, stream, error, sizeof(error)));
}

static void thread_debug_send_request(SZrNetworkStream *stream, int id, const char *method, cJSON *params) {
    cJSON *request = cJSON_CreateObject();
    char *text;

    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_NOT_NULL(method);
    TEST_ASSERT_NOT_NULL(request);

    cJSON_AddStringToObject(request, "jsonrpc", "2.0");
    cJSON_AddNumberToObject(request, "id", id);
    cJSON_AddStringToObject(request, "method", method);
    cJSON_AddItemToObject(request, "params", params != ZR_NULL ? params : cJSON_CreateObject());

    text = cJSON_PrintUnformatted(request);
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_TRUE(ZrNetwork_StreamWriteFrame(stream, text, strlen(text)));
    cJSON_free(text);
    cJSON_Delete(request);
}

static cJSON *thread_debug_read_message(SZrNetworkStream *stream) {
    TZrChar frame[ZR_NETWORK_FRAME_BUFFER_CAPACITY];
    TZrSize length = 0;
    cJSON *message;

    TEST_ASSERT_TRUE(ZrNetwork_StreamReadFrame(stream, 5000, frame, sizeof(frame), &length));
    TEST_ASSERT_TRUE(length > 0);
    message = cJSON_Parse(frame);
    TEST_ASSERT_NOT_NULL(message);
    return message;
}

static cJSON *thread_debug_expect_response(SZrNetworkStream *stream, int id) {
    cJSON *message = thread_debug_read_message(stream);
    cJSON *idItem = cJSON_GetObjectItemCaseSensitive(message, "id");
    cJSON *errorItem = cJSON_GetObjectItemCaseSensitive(message, "error");

    TEST_ASSERT_TRUE(cJSON_IsNumber(idItem));
    TEST_ASSERT_EQUAL_INT(id, (int)idItem->valuedouble);
    TEST_ASSERT_TRUE(errorItem == ZR_NULL);
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItemCaseSensitive(message, "result"));
    return message;
}

static cJSON *thread_debug_expect_event(SZrNetworkStream *stream, const char *method) {
    cJSON *message = thread_debug_read_message(stream);
    cJSON *methodItem = cJSON_GetObjectItemCaseSensitive(message, "method");

    TEST_ASSERT_TRUE(cJSON_IsString(methodItem));
    TEST_ASSERT_EQUAL_STRING(method, methodItem->valuestring);
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItemCaseSensitive(message, "params"));
    return message;
}

static const char *thread_debug_json_string(cJSON *object, const char *field) {
    cJSON *item = cJSON_GetObjectItemCaseSensitive(object, field);
    return cJSON_IsString(item) ? item->valuestring : "";
}

static int thread_debug_json_int(cJSON *object, const char *field) {
    cJSON *item = cJSON_GetObjectItemCaseSensitive(object, field);
    if (cJSON_IsBool(item)) {
        return cJSON_IsTrue(item) ? 1 : 0;
    }
    return cJSON_IsNumber(item) ? (int)item->valuedouble : 0;
}

static cJSON *thread_debug_find_named_object(cJSON *array, const char *name) {
    int index;

    if (!cJSON_IsArray(array) || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < cJSON_GetArraySize(array); index++) {
        cJSON *item = cJSON_GetArrayItem(array, index);
        if (item != ZR_NULL && strcmp(thread_debug_json_string(item, "name"), name) == 0) {
            return item;
        }
    }

    return ZR_NULL;
}

static void test_debug_threads_enumerates_main_thread_and_routes_snapshots(void) {
    const char *sourcePath = "debug_threads_fixture.zr";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugAgentConfig config;
    ZrDebugAgent *agent = ZR_NULL;
    SZrNetworkStream client;
    ZrThreadDebugExecution thread;
    TZrChar error[256];
    cJSON *message;
    cJSON *params;
    cJSON *result;
    cJSON *capabilities;
    cJSON *threads;
    cJSON *threadObject;
    cJSON *frames;
    cJSON *topFrame;
    cJSON *scopes;
    cJSON *localsScope;
    cJSON *values;
    cJSON *valueItem;
    cJSON *baseItem;
    int localsScopeId;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_thread_debug_fixture(state, sourcePath);
    TEST_ASSERT_NOT_NULL(function);

    memset(&config, 0, sizeof(config));
    config.address = "127.0.0.1:0";
    config.suspend_on_start = ZR_TRUE;
    config.auth_token = "secret";
    config.stop_on_uncaught_exception = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrDebug_AgentStart(state, function, "tests.debug.threads", &config, &agent, error, sizeof(error)));
    memset(&client, 0, sizeof(client));
    thread_debug_client_connect(agent, &client);

    memset(&thread, 0, sizeof(thread));
    thread.state = state;
    thread.function = function;
    TEST_ASSERT_TRUE(thread_debug_execution_start(&thread));

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "authToken", "secret");
    thread_debug_send_request(&client, 1, "initialize", params);

    message = thread_debug_expect_response(&client, 1);
    result = cJSON_GetObjectItemCaseSensitive(message, "result");
    capabilities = cJSON_GetObjectItemCaseSensitive(result, "capabilities");
    TEST_ASSERT_EQUAL_STRING("zrdbg/1", thread_debug_json_string(result, "protocol"));
    TEST_ASSERT_TRUE(thread_debug_json_int(capabilities, "supportsThreads") != 0);
    cJSON_Delete(message);

    message = thread_debug_expect_event(&client, "initialized");
    cJSON_Delete(message);
    message = thread_debug_expect_event(&client, "moduleLoaded");
    cJSON_Delete(message);
    message = thread_debug_expect_event(&client, "stopped");
    params = cJSON_GetObjectItemCaseSensitive(message, "params");
    TEST_ASSERT_EQUAL_STRING("entry", thread_debug_json_string(params, "reason"));
    TEST_ASSERT_EQUAL_INT(1, thread_debug_json_int(params, "threadId"));
    cJSON_Delete(message);

    thread_debug_send_request(&client, 2, "threads", ZR_NULL);
    message = thread_debug_expect_response(&client, 2);
    threads = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "threads");
    TEST_ASSERT_TRUE(cJSON_IsArray(threads));
    TEST_ASSERT_EQUAL_INT(1, cJSON_GetArraySize(threads));
    threadObject = cJSON_GetArrayItem(threads, 0);
    TEST_ASSERT_EQUAL_INT(1, thread_debug_json_int(threadObject, "threadId"));
    TEST_ASSERT_EQUAL_STRING("main", thread_debug_json_string(threadObject, "name"));
    TEST_ASSERT_TRUE(thread_debug_json_int(threadObject, "current") != 0);
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "moduleName", "tests.debug.threads");
    cJSON_AddStringToObject(params, "sourceFile", sourcePath);
    cJSON_AddItemToObject(params, "lines", cJSON_CreateIntArray((const int[]){2}, 1));
    thread_debug_send_request(&client, 3, "setBreakpoints", params);
    message = thread_debug_expect_event(&client, "breakpointResolved");
    TEST_ASSERT_TRUE(thread_debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "resolved") != 0);
    cJSON_Delete(message);
    message = thread_debug_expect_response(&client, 3);
    cJSON_Delete(message);

    thread_debug_send_request(&client, 4, "continue", ZR_NULL);
    message = thread_debug_expect_response(&client, 4);
    cJSON_Delete(message);
    message = thread_debug_expect_event(&client, "continued");
    cJSON_Delete(message);
    message = thread_debug_expect_event(&client, "stopped");
    params = cJSON_GetObjectItemCaseSensitive(message, "params");
    TEST_ASSERT_EQUAL_STRING("breakpoint", thread_debug_json_string(params, "reason"));
    TEST_ASSERT_EQUAL_INT(1, thread_debug_json_int(params, "threadId"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "threadId", 1);
    thread_debug_send_request(&client, 5, "stackTrace", params);
    message = thread_debug_expect_response(&client, 5);
    result = cJSON_GetObjectItemCaseSensitive(message, "result");
    TEST_ASSERT_EQUAL_INT(1, thread_debug_json_int(result, "threadId"));
    frames = cJSON_GetObjectItemCaseSensitive(result, "frames");
    TEST_ASSERT_TRUE(cJSON_IsArray(frames));
    TEST_ASSERT_TRUE(cJSON_GetArraySize(frames) >= 2);
    topFrame = cJSON_GetArrayItem(frames, 0);
    TEST_ASSERT_EQUAL_INT(1, thread_debug_json_int(topFrame, "threadId"));
    TEST_ASSERT_EQUAL_STRING("addOne", thread_debug_json_string(topFrame, "functionName"));
    cJSON_Delete(message);

    thread_debug_send_request(&client, 6, "next", ZR_NULL);
    message = thread_debug_expect_response(&client, 6);
    cJSON_Delete(message);
    message = thread_debug_expect_event(&client, "continued");
    cJSON_Delete(message);
    message = thread_debug_expect_event(&client, "stopped");
    params = cJSON_GetObjectItemCaseSensitive(message, "params");
    TEST_ASSERT_EQUAL_STRING("step", thread_debug_json_string(params, "reason"));
    TEST_ASSERT_EQUAL_INT(1, thread_debug_json_int(params, "threadId"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "threadId", 1);
    cJSON_AddNumberToObject(params, "frameId", 1);
    thread_debug_send_request(&client, 7, "scopes", params);
    message = thread_debug_expect_response(&client, 7);
    result = cJSON_GetObjectItemCaseSensitive(message, "result");
    TEST_ASSERT_EQUAL_INT(1, thread_debug_json_int(result, "threadId"));
    scopes = cJSON_GetObjectItemCaseSensitive(result, "scopes");
    TEST_ASSERT_TRUE(cJSON_IsArray(scopes));
    localsScope = thread_debug_find_named_object(scopes, "Locals");
    TEST_ASSERT_NOT_NULL(localsScope);
    TEST_ASSERT_EQUAL_INT(1, thread_debug_json_int(localsScope, "threadId"));
    localsScopeId = thread_debug_json_int(localsScope, "scopeId");
    TEST_ASSERT_TRUE(localsScopeId > 0);
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "threadId", 1);
    cJSON_AddNumberToObject(params, "scopeId", localsScopeId);
    thread_debug_send_request(&client, 8, "variables", params);
    message = thread_debug_expect_response(&client, 8);
    result = cJSON_GetObjectItemCaseSensitive(message, "result");
    TEST_ASSERT_EQUAL_INT(1, thread_debug_json_int(result, "threadId"));
    values = cJSON_GetObjectItemCaseSensitive(result, "variables");
    TEST_ASSERT_TRUE(cJSON_IsArray(values));
    valueItem = thread_debug_find_named_object(values, "value");
    baseItem = thread_debug_find_named_object(values, "base");
    TEST_ASSERT_NOT_NULL(valueItem);
    TEST_ASSERT_NOT_NULL(baseItem);
    TEST_ASSERT_EQUAL_STRING("4", thread_debug_json_string(valueItem, "value"));
    TEST_ASSERT_EQUAL_STRING("5", thread_debug_json_string(baseItem, "value"));
    cJSON_Delete(message);

    thread_debug_send_request(&client, 9, "continue", ZR_NULL);
    message = thread_debug_expect_response(&client, 9);
    cJSON_Delete(message);
    message = thread_debug_expect_event(&client, "continued");
    cJSON_Delete(message);

    thread_debug_execution_join(&thread);
    TEST_ASSERT_TRUE(thread.success);
    TEST_ASSERT_EQUAL_INT64(7, thread.result);
    ZrDebug_NotifyTerminated(agent, thread.success);

    message = thread_debug_expect_event(&client, "terminated");
    TEST_ASSERT_TRUE(thread_debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "success") != 0);
    cJSON_Delete(message);

    ZrNetwork_StreamClose(&client);
    ZrDebug_AgentStop(agent);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_debug_threads_enumerates_main_thread_and_routes_snapshots);
    return UNITY_END();
}
