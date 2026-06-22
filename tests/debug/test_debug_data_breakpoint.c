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

typedef struct ZrDataBreakpointExecution {
    SZrState *state;
    SZrFunction *function;
    TZrInt64 result;
    TZrBool success;
#if defined(_WIN32)
    HANDLE handle;
#else
    pthread_t handle;
#endif
} ZrDataBreakpointExecution;

typedef struct ZrDataBreakpointSession {
    SZrState *state;
    SZrFunction *function;
    ZrDebugAgent *agent;
    SZrNetworkStream client;
    ZrDataBreakpointExecution thread;
    int nextRequestId;
} ZrDataBreakpointSession;

static SZrFunction *compile_data_breakpoint_source(SZrState *state, const char *sourceLabel, const char *source) {
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

static void data_breakpoint_thread_run(ZrDataBreakpointExecution *thread) {
    if (thread == ZR_NULL) {
        return;
    }

    thread->success = ZrTests_Runtime_Function_ExecuteExpectInt64(thread->state, thread->function, &thread->result);
}

#if defined(_WIN32)
static DWORD WINAPI data_breakpoint_thread_proc(LPVOID argument) {
    data_breakpoint_thread_run((ZrDataBreakpointExecution *)argument);
    return 0;
}
#else
static void *data_breakpoint_thread_proc(void *argument) {
    data_breakpoint_thread_run((ZrDataBreakpointExecution *)argument);
    return ZR_NULL;
}
#endif

static TZrBool data_breakpoint_thread_start(ZrDataBreakpointExecution *thread) {
    if (thread == ZR_NULL) {
        return ZR_FALSE;
    }

    thread->result = 0;
    thread->success = ZR_FALSE;
#if defined(_WIN32)
    thread->handle = CreateThread(NULL, 0, data_breakpoint_thread_proc, thread, 0, NULL);
    return thread->handle != NULL ? ZR_TRUE : ZR_FALSE;
#else
    return pthread_create(&thread->handle, NULL, data_breakpoint_thread_proc, thread) == 0 ? ZR_TRUE : ZR_FALSE;
#endif
}

static void data_breakpoint_thread_join(ZrDataBreakpointExecution *thread) {
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

static void data_breakpoint_client_connect(ZrDebugAgent *agent, SZrNetworkStream *stream) {
    TZrChar endpointText[ZR_NETWORK_ENDPOINT_TEXT_CAPACITY];
    TZrChar error[256];
    SZrNetworkEndpoint endpoint;

    TEST_ASSERT_NOT_NULL(agent);
    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_TRUE(ZrDebug_AgentGetEndpoint(agent, endpointText, sizeof(endpointText)));
    TEST_ASSERT_TRUE(ZrNetwork_ParseEndpoint(endpointText, &endpoint, error, sizeof(error)));
    TEST_ASSERT_TRUE(ZrNetwork_StreamConnectLoopback(&endpoint, 3000, stream, error, sizeof(error)));
}

static void data_breakpoint_send_request(SZrNetworkStream *stream, int id, const char *method, cJSON *params) {
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

static cJSON *data_breakpoint_read_message(SZrNetworkStream *stream) {
    TZrChar frame[ZR_NETWORK_FRAME_BUFFER_CAPACITY];
    TZrSize length = 0;
    cJSON *message;

    TEST_ASSERT_TRUE(ZrNetwork_StreamReadFrame(stream, 5000, frame, sizeof(frame), &length));
    TEST_ASSERT_TRUE(length > 0);
    message = cJSON_Parse(frame);
    TEST_ASSERT_NOT_NULL(message);
    return message;
}

static cJSON *data_breakpoint_expect_response(SZrNetworkStream *stream, int id) {
    cJSON *message = data_breakpoint_read_message(stream);
    cJSON *idItem = cJSON_GetObjectItemCaseSensitive(message, "id");
    cJSON *errorItem = cJSON_GetObjectItemCaseSensitive(message, "error");

    TEST_ASSERT_TRUE(cJSON_IsNumber(idItem));
    TEST_ASSERT_EQUAL_INT(id, (int)idItem->valuedouble);
    TEST_ASSERT_TRUE(errorItem == ZR_NULL);
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItemCaseSensitive(message, "result"));
    return message;
}

static cJSON *data_breakpoint_expect_event(SZrNetworkStream *stream, const char *method) {
    cJSON *message = data_breakpoint_read_message(stream);
    cJSON *methodItem = cJSON_GetObjectItemCaseSensitive(message, "method");

    TEST_ASSERT_TRUE(cJSON_IsString(methodItem));
    TEST_ASSERT_EQUAL_STRING(method, methodItem->valuestring);
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItemCaseSensitive(message, "params"));
    return message;
}

static const char *data_breakpoint_json_string(cJSON *object, const char *field) {
    cJSON *item = cJSON_GetObjectItemCaseSensitive(object, field);
    return cJSON_IsString(item) ? item->valuestring : "";
}

static int data_breakpoint_json_int(cJSON *object, const char *field) {
    cJSON *item = cJSON_GetObjectItemCaseSensitive(object, field);
    if (cJSON_IsBool(item)) {
        return cJSON_IsTrue(item) ? 1 : 0;
    }
    return cJSON_IsNumber(item) ? (int)item->valuedouble : 0;
}

static cJSON *data_breakpoint_find_named_object(cJSON *array, const char *name) {
    int index;

    if (!cJSON_IsArray(array) || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < cJSON_GetArraySize(array); index++) {
        cJSON *item = cJSON_GetArrayItem(array, index);
        if (item != ZR_NULL && strcmp(data_breakpoint_json_string(item, "name"), name) == 0) {
            return item;
        }
    }

    return ZR_NULL;
}

static void data_breakpoint_session_start(ZrDataBreakpointSession *session,
                                          const char *moduleName,
                                          const char *sourcePath,
                                          const char *source) {
    ZrDebugAgentConfig config;
    TZrChar error[256];
    cJSON *params;
    cJSON *message;
    cJSON *result;
    cJSON *capabilities;

    TEST_ASSERT_NOT_NULL(session);
    memset(session, 0, sizeof(*session));
    session->nextRequestId = 1;
    session->state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(session->state);
    session->function = compile_data_breakpoint_source(session->state, sourcePath, source);
    TEST_ASSERT_NOT_NULL(session->function);

    memset(&config, 0, sizeof(config));
    config.address = "127.0.0.1:0";
    config.suspend_on_start = ZR_TRUE;
    config.auth_token = "secret";
    config.stop_on_uncaught_exception = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrDebug_AgentStart(session->state,
                                        session->function,
                                        moduleName,
                                        &config,
                                        &session->agent,
                                        error,
                                        sizeof(error)));

    data_breakpoint_client_connect(session->agent, &session->client);
    session->thread.state = session->state;
    session->thread.function = session->function;
    TEST_ASSERT_TRUE(data_breakpoint_thread_start(&session->thread));

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "authToken", "secret");
    data_breakpoint_send_request(&session->client, session->nextRequestId, "initialize", params);
    message = data_breakpoint_expect_response(&session->client, session->nextRequestId);
    session->nextRequestId++;
    result = cJSON_GetObjectItemCaseSensitive(message, "result");
    capabilities = cJSON_GetObjectItemCaseSensitive(result, "capabilities");
    TEST_ASSERT_TRUE(data_breakpoint_json_int(capabilities, "supportsDataBreakpoints") != 0);
    cJSON_Delete(message);

    message = data_breakpoint_expect_event(&session->client, "initialized");
    cJSON_Delete(message);
    message = data_breakpoint_expect_event(&session->client, "moduleLoaded");
    cJSON_Delete(message);
    message = data_breakpoint_expect_event(&session->client, "stopped");
    TEST_ASSERT_EQUAL_STRING("entry",
                             data_breakpoint_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"),
                                                         "reason"));
    cJSON_Delete(message);
}

static void data_breakpoint_session_set_line_breakpoint(ZrDataBreakpointSession *session,
                                                        const char *moduleName,
                                                        const char *sourcePath,
                                                        int line) {
    cJSON *params;
    cJSON *message;

    TEST_ASSERT_NOT_NULL(session);
    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "moduleName", moduleName);
    cJSON_AddStringToObject(params, "sourceFile", sourcePath);
    cJSON_AddItemToObject(params, "lines", cJSON_CreateIntArray(&line, 1));
    data_breakpoint_send_request(&session->client, session->nextRequestId, "setBreakpoints", params);
    message = data_breakpoint_expect_event(&session->client, "breakpointResolved");
    TEST_ASSERT_TRUE(data_breakpoint_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "resolved") != 0);
    TEST_ASSERT_EQUAL_INT(line, data_breakpoint_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "line"));
    cJSON_Delete(message);
    message = data_breakpoint_expect_response(&session->client, session->nextRequestId);
    session->nextRequestId++;
    cJSON_Delete(message);
}

static cJSON *data_breakpoint_session_continue_to_stop(ZrDataBreakpointSession *session,
                                                       const char *expectedReason,
                                                       int expectedLine) {
    cJSON *message;
    cJSON *params;

    TEST_ASSERT_NOT_NULL(session);
    data_breakpoint_send_request(&session->client, session->nextRequestId, "continue", ZR_NULL);
    message = data_breakpoint_expect_response(&session->client, session->nextRequestId);
    session->nextRequestId++;
    cJSON_Delete(message);
    message = data_breakpoint_expect_event(&session->client, "continued");
    cJSON_Delete(message);
    message = data_breakpoint_expect_event(&session->client, "stopped");
    params = cJSON_GetObjectItemCaseSensitive(message, "params");
    TEST_ASSERT_EQUAL_STRING(expectedReason, data_breakpoint_json_string(params, "reason"));
    if (expectedLine > 0) {
        TEST_ASSERT_EQUAL_INT(expectedLine, data_breakpoint_json_int(params, "line"));
    }
    TEST_ASSERT_EQUAL_INT(1, data_breakpoint_json_int(params, "threadId"));
    return message;
}

static int data_breakpoint_session_scope_id(ZrDataBreakpointSession *session, const char *scopeName) {
    cJSON *params;
    cJSON *message;
    cJSON *result;
    cJSON *scopes;
    cJSON *scope;
    int scopeId;

    TEST_ASSERT_NOT_NULL(session);
    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "threadId", 1);
    cJSON_AddNumberToObject(params, "frameId", 1);
    data_breakpoint_send_request(&session->client, session->nextRequestId, "scopes", params);
    message = data_breakpoint_expect_response(&session->client, session->nextRequestId);
    session->nextRequestId++;
    result = cJSON_GetObjectItemCaseSensitive(message, "result");
    scopes = cJSON_GetObjectItemCaseSensitive(result, "scopes");
    TEST_ASSERT_TRUE(cJSON_IsArray(scopes));
    scope = data_breakpoint_find_named_object(scopes, scopeName);
    TEST_ASSERT_NOT_NULL(scope);
    scopeId = data_breakpoint_json_int(scope, "variablesReference");
    if (scopeId == 0) {
        scopeId = data_breakpoint_json_int(scope, "scopeId");
    }
    TEST_ASSERT_TRUE(scopeId > 0);
    cJSON_Delete(message);
    return scopeId;
}

static void data_breakpoint_session_expect_variable(ZrDataBreakpointSession *session,
                                                    int scopeId,
                                                    const char *name,
                                                    const char *expectedValue) {
    cJSON *params;
    cJSON *message;
    cJSON *result;
    cJSON *values;
    cJSON *value;

    TEST_ASSERT_NOT_NULL(session);
    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "threadId", 1);
    cJSON_AddNumberToObject(params, "scopeId", scopeId);
    data_breakpoint_send_request(&session->client, session->nextRequestId, "variables", params);
    message = data_breakpoint_expect_response(&session->client, session->nextRequestId);
    session->nextRequestId++;
    result = cJSON_GetObjectItemCaseSensitive(message, "result");
    values = cJSON_GetObjectItemCaseSensitive(result, "variables");
    TEST_ASSERT_TRUE(cJSON_IsArray(values));
    value = data_breakpoint_find_named_object(values, name);
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_STRING(expectedValue, data_breakpoint_json_string(value, "value"));
    cJSON_Delete(message);
}

static void data_breakpoint_session_data_breakpoint_info(ZrDataBreakpointSession *session,
                                                         int scopeId,
                                                         const char *name,
                                                         char *dataId,
                                                         size_t dataIdSize) {
    cJSON *params;
    cJSON *message;
    cJSON *result;
    cJSON *accessTypes;
    const char *resultDataId;

    TEST_ASSERT_NOT_NULL(session);
    TEST_ASSERT_NOT_NULL(dataId);
    TEST_ASSERT_TRUE(dataIdSize > 0);
    dataId[0] = '\0';

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "threadId", 1);
    cJSON_AddNumberToObject(params, "variablesReference", scopeId);
    cJSON_AddStringToObject(params, "name", name);
    data_breakpoint_send_request(&session->client, session->nextRequestId, "dataBreakpointInfo", params);
    message = data_breakpoint_expect_response(&session->client, session->nextRequestId);
    session->nextRequestId++;
    result = cJSON_GetObjectItemCaseSensitive(message, "result");
    resultDataId = data_breakpoint_json_string(result, "dataId");
    TEST_ASSERT_TRUE(resultDataId[0] != '\0');
    TEST_ASSERT_TRUE(strstr(resultDataId, name) != ZR_NULL);
    TEST_ASSERT_TRUE(data_breakpoint_json_int(result, "canPersist") != 0);
    accessTypes = cJSON_GetObjectItemCaseSensitive(result, "accessTypes");
    TEST_ASSERT_TRUE(cJSON_IsArray(accessTypes));
    TEST_ASSERT_EQUAL_STRING("write", cJSON_GetArrayItem(accessTypes, 0)->valuestring);
    snprintf(dataId, dataIdSize, "%s", resultDataId);
    dataId[dataIdSize - 1u] = '\0';
    cJSON_Delete(message);
}

static void data_breakpoint_session_set_data_breakpoint(ZrDataBreakpointSession *session, const char *dataId) {
    cJSON *params;
    cJSON *breakpoints;
    cJSON *breakpoint;
    cJSON *message;
    cJSON *result;
    cJSON *results;
    cJSON *verified;

    TEST_ASSERT_NOT_NULL(session);
    TEST_ASSERT_NOT_NULL(dataId);
    params = cJSON_CreateObject();
    breakpoints = cJSON_CreateArray();
    breakpoint = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    TEST_ASSERT_NOT_NULL(breakpoints);
    TEST_ASSERT_NOT_NULL(breakpoint);
    cJSON_AddStringToObject(breakpoint, "dataId", dataId);
    cJSON_AddStringToObject(breakpoint, "accessType", "write");
    cJSON_AddItemToArray(breakpoints, breakpoint);
    cJSON_AddItemToObject(params, "breakpoints", breakpoints);
    data_breakpoint_send_request(&session->client, session->nextRequestId, "setDataBreakpoints", params);
    message = data_breakpoint_expect_response(&session->client, session->nextRequestId);
    session->nextRequestId++;
    result = cJSON_GetObjectItemCaseSensitive(message, "result");
    results = cJSON_GetObjectItemCaseSensitive(result, "breakpoints");
    TEST_ASSERT_TRUE(cJSON_IsArray(results));
    verified = cJSON_GetArrayItem(results, 0);
    TEST_ASSERT_NOT_NULL(verified);
    TEST_ASSERT_TRUE(data_breakpoint_json_int(verified, "verified") != 0);
    TEST_ASSERT_EQUAL_STRING(dataId, data_breakpoint_json_string(verified, "id"));
    cJSON_Delete(message);
}

static void data_breakpoint_session_finish(ZrDataBreakpointSession *session, TZrInt64 expectedResult) {
    cJSON *message;

    TEST_ASSERT_NOT_NULL(session);
    data_breakpoint_send_request(&session->client, session->nextRequestId, "continue", ZR_NULL);
    message = data_breakpoint_expect_response(&session->client, session->nextRequestId);
    session->nextRequestId++;
    cJSON_Delete(message);
    message = data_breakpoint_expect_event(&session->client, "continued");
    cJSON_Delete(message);

    data_breakpoint_thread_join(&session->thread);
    ZrDebug_NotifyTerminated(session->agent, session->thread.success);
    message = data_breakpoint_expect_event(&session->client, "terminated");
    TEST_ASSERT_TRUE(data_breakpoint_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "success") != 0);
    cJSON_Delete(message);

    TEST_ASSERT_TRUE(session->thread.success);
    TEST_ASSERT_EQUAL_INT64(expectedResult, session->thread.result);
}

static void data_breakpoint_session_cleanup(ZrDataBreakpointSession *session) {
    if (session == ZR_NULL) {
        return;
    }

    ZrNetwork_StreamClose(&session->client);
    if (session->agent != ZR_NULL) {
        ZrDebug_AgentStop(session->agent);
        session->agent = ZR_NULL;
    }
    if (session->function != ZR_NULL) {
        ZrCore_Function_Free(session->state, session->function);
        session->function = ZR_NULL;
    }
    if (session->state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(session->state);
        session->state = ZR_NULL;
    }
}

static void test_debug_data_breakpoint_stops_when_local_value_changes(void) {
    const char *moduleName = "tests.debug.data_breakpoint";
    const char *sourcePath = "debug_data_breakpoint_local.zr";
    const char *source =
            "func watchTarget(): int {\n"
            "    var watched = 1;\n"
            "    var other = 10;\n"
            "    watched = watched + 1;\n"
            "    other = watched + other;\n"
            "    return other;\n"
            "}\n"
            "return watchTarget();\n";
    ZrDataBreakpointSession session;
    cJSON *message;
    cJSON *params;
    int localsScopeId;
    char dataId[ZR_DEBUG_TEXT_CAPACITY];

    data_breakpoint_session_start(&session, moduleName, sourcePath, source);
    data_breakpoint_session_set_line_breakpoint(&session, moduleName, sourcePath, 3);

    message = data_breakpoint_session_continue_to_stop(&session, "breakpoint", 3);
    cJSON_Delete(message);
    localsScopeId = data_breakpoint_session_scope_id(&session, "Locals");
    data_breakpoint_session_expect_variable(&session, localsScopeId, "watched", "1");
    data_breakpoint_session_data_breakpoint_info(&session, localsScopeId, "watched", dataId, sizeof(dataId));
    data_breakpoint_session_set_data_breakpoint(&session, dataId);

    message = data_breakpoint_session_continue_to_stop(&session, "dataBreakpoint", 5);
    params = cJSON_GetObjectItemCaseSensitive(message, "params");
    TEST_ASSERT_EQUAL_STRING(dataId, data_breakpoint_json_string(params, "dataId"));
    TEST_ASSERT_TRUE(strstr(data_breakpoint_json_string(params, "description"), "watched") != ZR_NULL);
    cJSON_Delete(message);
    data_breakpoint_session_expect_variable(&session, localsScopeId, "watched", "2");

    data_breakpoint_session_finish(&session, 12);
    data_breakpoint_session_cleanup(&session);
}

static void test_debug_data_breakpoint_stops_when_upvalue_changes(void) {
    const char *moduleName = "tests.debug.data_breakpoint.upvalue";
    const char *sourcePath = "debug_data_breakpoint_upvalue.zr";
    const char *source =
            "var run = () => {\n"
            "    var captured = 4;\n"
            "    var bump = () => {\n"
            "        captured = captured + 3;\n"
            "        var observed = captured + 1;\n"
            "        return observed;\n"
            "    };\n"
            "    return bump();\n"
            "};\n"
            "return run();\n";
    ZrDataBreakpointSession session;
    cJSON *message;
    cJSON *params;
    int closureScopeId;
    char dataId[ZR_DEBUG_TEXT_CAPACITY];

    data_breakpoint_session_start(&session, moduleName, sourcePath, source);
    data_breakpoint_session_set_line_breakpoint(&session, moduleName, sourcePath, 4);

    message = data_breakpoint_session_continue_to_stop(&session, "breakpoint", 4);
    cJSON_Delete(message);
    closureScopeId = data_breakpoint_session_scope_id(&session, "Closures");
    data_breakpoint_session_expect_variable(&session, closureScopeId, "captured", "4");
    data_breakpoint_session_data_breakpoint_info(&session, closureScopeId, "captured", dataId, sizeof(dataId));
    data_breakpoint_session_set_data_breakpoint(&session, dataId);

    message = data_breakpoint_session_continue_to_stop(&session, "dataBreakpoint", 4);
    params = cJSON_GetObjectItemCaseSensitive(message, "params");
    TEST_ASSERT_EQUAL_STRING(dataId, data_breakpoint_json_string(params, "dataId"));
    TEST_ASSERT_TRUE(strstr(data_breakpoint_json_string(params, "description"), "captured") != ZR_NULL);
    cJSON_Delete(message);
    data_breakpoint_session_expect_variable(&session, closureScopeId, "captured", "7");

    data_breakpoint_session_finish(&session, 8);
    data_breakpoint_session_cleanup(&session);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_debug_data_breakpoint_stops_when_local_value_changes);
    RUN_TEST(test_debug_data_breakpoint_stops_when_upvalue_changes);
    return UNITY_END();
}
