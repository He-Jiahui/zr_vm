#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON/cJSON.h"
#include "unity.h"
#include "runtime_support.h"
#include "zr_vm_lib_debug/debug.h"
#include "zr_vm_lib_network/network.h"
#include "zr_vm_parser.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef struct ZrDebugExecutionThread {
    SZrState *state;
    SZrFunction *function;
    TZrInt64 result;
    TZrBool success;
#if defined(_WIN32)
    HANDLE handle;
#else
    pthread_t handle;
#endif
} ZrDebugExecutionThread;

static SZrFunction *compile_debug_agent_source(SZrState *state, const char *sourceLabel, const char *source) {
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

static SZrFunction *compile_debug_agent_fixture(SZrState *state, const char *sourceLabel) {
    const char *source =
            "func addOne(value: int): int {\n"
            "    var base = value + 1;\n"
            "    return base;\n"
            "}\n"
            "var first = addOne(4);\n"
            "var second = first + 2;\n"
            "return second;";

    return compile_debug_agent_source(state, sourceLabel, source);
}

static void debug_execution_thread_run(ZrDebugExecutionThread *thread) {
    if (thread == ZR_NULL) {
        return;
    }

    thread->success = ZrTests_Runtime_Function_ExecuteExpectInt64(thread->state, thread->function, &thread->result);
}

#if defined(_WIN32)
static DWORD WINAPI debug_execution_thread_proc(LPVOID argument) {
    ZrDebugExecutionThread *thread = (ZrDebugExecutionThread *)argument;
    debug_execution_thread_run(thread);
    return 0;
}
#else
static void *debug_execution_thread_proc(void *argument) {
    ZrDebugExecutionThread *thread = (ZrDebugExecutionThread *)argument;
    debug_execution_thread_run(thread);
    return ZR_NULL;
}
#endif

static TZrBool debug_execution_thread_start(ZrDebugExecutionThread *thread) {
    if (thread == ZR_NULL) {
        return ZR_FALSE;
    }

    thread->result = 0;
    thread->success = ZR_FALSE;
#if defined(_WIN32)
    thread->handle = CreateThread(NULL, 0, debug_execution_thread_proc, thread, 0, NULL);
    return thread->handle != NULL ? ZR_TRUE : ZR_FALSE;
#else
    return pthread_create(&thread->handle, NULL, debug_execution_thread_proc, thread) == 0 ? ZR_TRUE : ZR_FALSE;
#endif
}

static void debug_execution_thread_join(ZrDebugExecutionThread *thread) {
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

static void debug_client_connect(ZrDebugAgent *agent, SZrNetworkStream *stream) {
    TZrChar endpointText[ZR_NETWORK_ENDPOINT_TEXT_CAPACITY];
    TZrChar error[256];
    SZrNetworkEndpoint endpoint;

    TEST_ASSERT_NOT_NULL(agent);
    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_TRUE(ZrDebug_AgentGetEndpoint(agent, endpointText, sizeof(endpointText)));
    TEST_ASSERT_TRUE(ZrNetwork_ParseEndpoint(endpointText, &endpoint, error, sizeof(error)));
    TEST_ASSERT_TRUE(ZrNetwork_StreamConnectLoopback(&endpoint, 3000, stream, error, sizeof(error)));
}

static void debug_client_send_request(SZrNetworkStream *stream, int id, const char *method, cJSON *params) {
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

static cJSON *debug_client_read_message(SZrNetworkStream *stream) {
    TZrChar frame[ZR_NETWORK_FRAME_BUFFER_CAPACITY];
    TZrSize length = 0;
    cJSON *message;

    TEST_ASSERT_TRUE(ZrNetwork_StreamReadFrame(stream, 5000, frame, sizeof(frame), &length));
    TEST_ASSERT_TRUE(length > 0);

    message = cJSON_Parse(frame);
    TEST_ASSERT_NOT_NULL(message);
    return message;
}

static cJSON *debug_client_expect_response(SZrNetworkStream *stream, int id) {
    cJSON *message = debug_client_read_message(stream);
    cJSON *idItem = cJSON_GetObjectItemCaseSensitive(message, "id");
    cJSON *errorItem = cJSON_GetObjectItemCaseSensitive(message, "error");

    TEST_ASSERT_TRUE(cJSON_IsNumber(idItem));
    TEST_ASSERT_EQUAL_INT(id, (int)idItem->valuedouble);
    TEST_ASSERT_TRUE(errorItem == ZR_NULL);
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItemCaseSensitive(message, "result"));
    return message;
}

static cJSON *debug_client_expect_event(SZrNetworkStream *stream, const char *method) {
    cJSON *message = debug_client_read_message(stream);
    cJSON *methodItem = cJSON_GetObjectItemCaseSensitive(message, "method");

    TEST_ASSERT_TRUE(cJSON_IsString(methodItem));
    TEST_ASSERT_EQUAL_STRING(method, methodItem->valuestring);
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItemCaseSensitive(message, "params"));
    return message;
}

static const char *debug_json_string(cJSON *object, const char *field) {
    cJSON *item = cJSON_GetObjectItemCaseSensitive(object, field);
    return cJSON_IsString(item) ? item->valuestring : "";
}

static int debug_json_int(cJSON *object, const char *field) {
    cJSON *item = cJSON_GetObjectItemCaseSensitive(object, field);
    if (cJSON_IsBool(item)) {
        return cJSON_IsTrue(item) ? 1 : 0;
    }
    return cJSON_IsNumber(item) ? (int)item->valuedouble : 0;
}

static void debug_client_initialize(SZrNetworkStream *client, const char *moduleName) {
    cJSON *message;
    cJSON *params = cJSON_CreateObject();

    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "authToken", "secret");
    debug_client_send_request(client, 1, "initialize", params);

    message = debug_client_expect_response(client, 1);
    TEST_ASSERT_EQUAL_STRING("zrdbg/1", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "result"), "protocol"));
    cJSON_Delete(message);

    message = debug_client_expect_event(client, "initialized");
    cJSON_Delete(message);

    message = debug_client_expect_event(client, "moduleLoaded");
    TEST_ASSERT_EQUAL_STRING(moduleName,
                             debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "moduleName"));
    cJSON_Delete(message);

    message = debug_client_expect_event(client, "stopped");
    TEST_ASSERT_EQUAL_STRING("entry", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "reason"));
    cJSON_Delete(message);
}

static void debug_client_initialize_running(SZrNetworkStream *client, const char *moduleName) {
    cJSON *message;
    cJSON *params = cJSON_CreateObject();

    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "authToken", "secret");
    debug_client_send_request(client, 1, "initialize", params);

    message = debug_client_expect_response(client, 1);
    TEST_ASSERT_EQUAL_STRING("zrdbg/1", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "result"), "protocol"));
    cJSON_Delete(message);

    message = debug_client_expect_event(client, "initialized");
    cJSON_Delete(message);

    message = debug_client_expect_event(client, "moduleLoaded");
    TEST_ASSERT_EQUAL_STRING(moduleName,
                             debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "moduleName"));
    cJSON_Delete(message);
}

static void test_debug_agent_pause_request_over_tcp_stops_at_next_safepoint(void) {
    const char *sourcePath = "debug_agent_protocol_pause_fixture.zr";
    const char *source =
            "func spin(limit: int): int {\n"
            "    var total = 0;\n"
            "    var index = 0;\n"
            "    while (index < limit) {\n"
            "        var burst = 0;\n"
            "        while (burst < 20) {\n"
            "            total = total + 1;\n"
            "            burst = burst + 1;\n"
            "        }\n"
            "        index = index + 1;\n"
            "    }\n"
            "    return total;\n"
            "}\n"
            "return spin(100000);";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugAgentConfig config;
    ZrDebugAgent *agent = ZR_NULL;
    SZrNetworkStream client;
    ZrDebugExecutionThread thread;
    TZrChar error[256];
    cJSON *message;
    cJSON *params;
    int pausedLine;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_agent_source(state, sourcePath, source);
    TEST_ASSERT_NOT_NULL(function);

    memset(&config, 0, sizeof(config));
    config.address = "127.0.0.1:0";
    config.suspend_on_start = ZR_TRUE;
    config.auth_token = "secret";

    TEST_ASSERT_TRUE(
            ZrDebug_AgentStart(state, function, "tests.debug.protocol_pause", &config, &agent, error, sizeof(error)));
    memset(&client, 0, sizeof(client));
    debug_client_connect(agent, &client);

    memset(&thread, 0, sizeof(thread));
    thread.state = state;
    thread.function = function;
    TEST_ASSERT_TRUE(debug_execution_thread_start(&thread));

    debug_client_initialize(&client, "tests.debug.protocol_pause");

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "moduleName", "tests.debug.protocol_pause");
    cJSON_AddStringToObject(params, "sourceFile", sourcePath);
    cJSON_AddItemToObject(params, "lines", cJSON_CreateIntArray((const int[]){7}, 1));
    debug_client_send_request(&client, 2, "setBreakpoints", params);
    message = debug_client_expect_event(&client, "breakpointResolved");
    TEST_ASSERT_TRUE(debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "resolved") != 0);
    TEST_ASSERT_EQUAL_INT(7, debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "line"));
    cJSON_Delete(message);
    message = debug_client_expect_response(&client, 2);
    cJSON_Delete(message);

    debug_client_send_request(&client, 3, "continue", ZR_NULL);
    message = debug_client_expect_response(&client, 3);
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "continued");
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "stopped");
    TEST_ASSERT_EQUAL_STRING("breakpoint", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "reason"));
    TEST_ASSERT_EQUAL_STRING("spin", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "functionName"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "moduleName", "tests.debug.protocol_pause");
    cJSON_AddStringToObject(params, "sourceFile", sourcePath);
    cJSON_AddItemToObject(params, "lines", cJSON_CreateArray());
    debug_client_send_request(&client, 4, "setBreakpoints", params);
    message = debug_client_expect_response(&client, 4);
    cJSON_Delete(message);

    debug_client_send_request(&client, 5, "continue", ZR_NULL);
    message = debug_client_expect_response(&client, 5);
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "continued");
    cJSON_Delete(message);

    debug_client_send_request(&client, 6, "pause", ZR_NULL);
    message = debug_client_expect_response(&client, 6);
    cJSON_Delete(message);

    message = debug_client_expect_event(&client, "stopped");
    TEST_ASSERT_EQUAL_STRING("pause", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "reason"));
    TEST_ASSERT_EQUAL_STRING(sourcePath, debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "sourceFile"));
    TEST_ASSERT_EQUAL_STRING("spin", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "functionName"));
    pausedLine = debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "line");
    TEST_ASSERT_TRUE(pausedLine >= 6);
    TEST_ASSERT_TRUE(pausedLine <= 10);
    cJSON_Delete(message);

    debug_client_send_request(&client, 7, "continue", ZR_NULL);
    message = debug_client_expect_response(&client, 7);
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "continued");
    cJSON_Delete(message);

    debug_execution_thread_join(&thread);
    TEST_ASSERT_TRUE(thread.success);
    TEST_ASSERT_EQUAL_INT64(2000000, thread.result);
    ZrDebug_NotifyTerminated(agent, thread.success);

    message = debug_client_expect_event(&client, "terminated");
    TEST_ASSERT_TRUE(debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "success") != 0);
    cJSON_Delete(message);

    ZrNetwork_StreamClose(&client);
    ZrDebug_AgentStop(agent);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_agent_disconnect_request_while_paused_resumes_target(void) {
    const char *sourcePath = "debug_agent_disconnect_fixture.zr";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugAgentConfig config;
    ZrDebugAgent *agent = ZR_NULL;
    SZrNetworkStream client;
    ZrDebugExecutionThread thread;
    TZrChar error[256];
    cJSON *message;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_agent_fixture(state, sourcePath);
    TEST_ASSERT_NOT_NULL(function);

    memset(&config, 0, sizeof(config));
    config.address = "127.0.0.1:0";
    config.suspend_on_start = ZR_TRUE;
    config.auth_token = "secret";

    TEST_ASSERT_TRUE(
            ZrDebug_AgentStart(state, function, "tests.debug.disconnect_resume", &config, &agent, error, sizeof(error)));
    memset(&client, 0, sizeof(client));
    debug_client_connect(agent, &client);

    memset(&thread, 0, sizeof(thread));
    thread.state = state;
    thread.function = function;
    TEST_ASSERT_TRUE(debug_execution_thread_start(&thread));

    debug_client_initialize(&client, "tests.debug.disconnect_resume");

    debug_client_send_request(&client, 2, "disconnect", ZR_NULL);
    message = debug_client_expect_response(&client, 2);
    cJSON_Delete(message);

    debug_execution_thread_join(&thread);
    TEST_ASSERT_TRUE(thread.success);
    TEST_ASSERT_EQUAL_INT64(7, thread.result);

    ZrNetwork_StreamClose(&client);
    ZrDebug_AgentStop(agent);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_agent_raw_socket_close_while_paused_resumes_target(void) {
    const char *sourcePath = "debug_agent_raw_close_fixture.zr";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugAgentConfig config;
    ZrDebugAgent *agent = ZR_NULL;
    SZrNetworkStream client;
    ZrDebugExecutionThread thread;
    TZrChar error[256];

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_agent_fixture(state, sourcePath);
    TEST_ASSERT_NOT_NULL(function);

    memset(&config, 0, sizeof(config));
    config.address = "127.0.0.1:0";
    config.suspend_on_start = ZR_TRUE;
    config.auth_token = "secret";

    TEST_ASSERT_TRUE(
            ZrDebug_AgentStart(state, function, "tests.debug.raw_close_resume", &config, &agent, error, sizeof(error)));
    memset(&client, 0, sizeof(client));
    debug_client_connect(agent, &client);

    memset(&thread, 0, sizeof(thread));
    thread.state = state;
    thread.function = function;
    TEST_ASSERT_TRUE(debug_execution_thread_start(&thread));

    debug_client_initialize(&client, "tests.debug.raw_close_resume");

    ZrNetwork_StreamClose(&client);

    debug_execution_thread_join(&thread);
    TEST_ASSERT_TRUE(thread.success);
    TEST_ASSERT_EQUAL_INT64(7, thread.result);

    ZrDebug_AgentStop(agent);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_agent_running_socket_close_allows_reconnect_and_pause(void) {
    const char *sourcePath = "debug_agent_running_reconnect_fixture.zr";
    const char *source =
            "func spin(limit: int): int {\n"
            "    var total = 0;\n"
            "    var index = 0;\n"
            "    while (index < limit) {\n"
            "        var burst = 0;\n"
            "        while (burst < 50) {\n"
            "            total = total + 1;\n"
            "            burst = burst + 1;\n"
            "        }\n"
            "        index = index + 1;\n"
            "    }\n"
            "    return total;\n"
            "}\n"
            "return spin(100000);";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugAgentConfig config;
    ZrDebugAgent *agent = ZR_NULL;
    SZrNetworkStream firstClient;
    SZrNetworkStream secondClient;
    ZrDebugExecutionThread thread;
    TZrChar error[256];
    cJSON *message;
    int pausedLine;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_agent_source(state, sourcePath, source);
    TEST_ASSERT_NOT_NULL(function);

    memset(&config, 0, sizeof(config));
    config.address = "127.0.0.1:0";
    config.suspend_on_start = ZR_TRUE;
    config.auth_token = "secret";

    TEST_ASSERT_TRUE(ZrDebug_AgentStart(state,
                                        function,
                                        "tests.debug.running_reconnect",
                                        &config,
                                        &agent,
                                        error,
                                        sizeof(error)));

    memset(&firstClient, 0, sizeof(firstClient));
    debug_client_connect(agent, &firstClient);

    memset(&thread, 0, sizeof(thread));
    thread.state = state;
    thread.function = function;
    TEST_ASSERT_TRUE(debug_execution_thread_start(&thread));

    debug_client_initialize(&firstClient, "tests.debug.running_reconnect");

    debug_client_send_request(&firstClient, 2, "continue", ZR_NULL);
    message = debug_client_expect_response(&firstClient, 2);
    cJSON_Delete(message);
    message = debug_client_expect_event(&firstClient, "continued");
    cJSON_Delete(message);

    ZrNetwork_StreamClose(&firstClient);

    memset(&secondClient, 0, sizeof(secondClient));
    debug_client_connect(agent, &secondClient);
    debug_client_initialize_running(&secondClient, "tests.debug.running_reconnect");

    debug_client_send_request(&secondClient, 2, "pause", ZR_NULL);
    message = debug_client_expect_response(&secondClient, 2);
    cJSON_Delete(message);

    message = debug_client_expect_event(&secondClient, "stopped");
    TEST_ASSERT_EQUAL_STRING("pause", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "reason"));
    TEST_ASSERT_EQUAL_STRING(sourcePath, debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "sourceFile"));
    TEST_ASSERT_EQUAL_STRING("spin", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "functionName"));
    pausedLine = debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "line");
    TEST_ASSERT_TRUE(pausedLine >= 6);
    TEST_ASSERT_TRUE(pausedLine <= 10);
    cJSON_Delete(message);

    debug_client_send_request(&secondClient, 3, "continue", ZR_NULL);
    message = debug_client_expect_response(&secondClient, 3);
    cJSON_Delete(message);
    message = debug_client_expect_event(&secondClient, "continued");
    cJSON_Delete(message);

    debug_execution_thread_join(&thread);
    TEST_ASSERT_TRUE(thread.success);
    TEST_ASSERT_EQUAL_INT64(5000000, thread.result);
    ZrDebug_NotifyTerminated(agent, thread.success);

    message = debug_client_expect_event(&secondClient, "terminated");
    TEST_ASSERT_TRUE(debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "success") != 0);
    cJSON_Delete(message);

    ZrNetwork_StreamClose(&secondClient);
    ZrDebug_AgentStop(agent);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_debug_agent_pause_request_over_tcp_stops_at_next_safepoint);
    RUN_TEST(test_debug_agent_disconnect_request_while_paused_resumes_target);
    RUN_TEST(test_debug_agent_raw_socket_close_while_paused_resumes_target);
    RUN_TEST(test_debug_agent_running_socket_close_allows_reconnect_and_pause);
    return UNITY_END();
}
