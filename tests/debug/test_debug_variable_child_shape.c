#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON/cJSON.h"
#include "unity.h"
#include "runtime_support.h"
#include "zr_vm_core/string.h"
#include "zr_vm_lib_debug/debug.h"
#include "zr_vm_lib_network/network.h"
#include "zr_vm_lib_system/module.h"
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

static SZrFunction *compile_debug_source(SZrState *state, const char *sourceLabel, const char *source) {
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
    if (errorItem != ZR_NULL) {
        char *errorText = cJSON_PrintUnformatted(errorItem);
        printf("Unexpected debug protocol error: %s\n", errorText != ZR_NULL ? errorText : "<unprintable>");
        cJSON_free(errorText);
    }
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

static void debug_assert_text_contains(const char *text, const char *needle) {
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(needle);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(text, needle), text);
}

static void debug_assert_no_reference_summary(cJSON *object) {
    TEST_ASSERT_EQUAL_STRING("", debug_json_string(object, "referenceSummary"));
}

static int debug_json_int(cJSON *object, const char *field) {
    cJSON *item = cJSON_GetObjectItemCaseSensitive(object, field);
    if (cJSON_IsBool(item)) {
        return cJSON_IsTrue(item) ? 1 : 0;
    }
    return cJSON_IsNumber(item) ? (int)item->valuedouble : 0;
}

static cJSON *debug_find_named_object(cJSON *array, const char *name) {
    int index;

    if (!cJSON_IsArray(array) || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < cJSON_GetArraySize(array); index++) {
        cJSON *item = cJSON_GetArrayItem(array, index);
        if (item != ZR_NULL && strcmp(debug_json_string(item, "name"), name) == 0) {
            return item;
        }
    }

    return ZR_NULL;
}

static void debug_client_initialize(SZrNetworkStream *client, const char *moduleName) {
    cJSON *message;
    cJSON *params = cJSON_CreateObject();

    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "authToken", "secret");
    debug_client_send_request(client, 1, "initialize", params);

    message = debug_client_expect_response(client, 1);
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

static void test_debug_protocol_reports_per_value_child_shape_metadata(void) {
    const char *sourcePath = "debug_variable_child_shape_fixture.zr";
    const char *source =
#if defined(_MSC_VER)
            "var system = %import(\"zr.system\");\n"
            "func pauseHere(inside) {\n"
            "    return 1;\n"
            "}\n"
            "var marker = pauseHere(7);\n"
            "return 1;";
    const char *valueScopeName = "Arguments";
    const char *expectedReferenceSummary = "argument inside";
    const int breakpointLine = 3;
#else
            "var system = %import(\"zr.system\");\n"
            "func pauseHere() {\n"
            "    var inside = 7;\n"
            "    return 1;\n"
            "}\n"
            "var marker = pauseHere();\n"
            "return 1;";
    const char *valueScopeName = "Locals";
    const char *expectedReferenceSummary = "local inside";
    const int breakpointLine = 4;
#endif
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugAgentConfig config;
    ZrDebugAgent *agent = ZR_NULL;
    SZrNetworkStream client;
    ZrDebugExecutionThread thread;
    TZrChar error[256];
    cJSON *message;
    cJSON *params;
    cJSON *scopes;
    cJSON *valueScope;
    cJSON *globalsScope;
    cJSON *values;
    cJSON *insideItem;
    cJSON *zrItem;
    cJSON *result;
    int valueScopeId;
    int globalsScopeId;
    int zrHandle;
    int zrItemNamedVariables;
    int zrItemIndexedVariables;
    int zrNamedVariables;
    int zrIndexedVariables;
    int evaluateHandle;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrVmLibSystem_Register(state->global));
    function = compile_debug_source(state, sourcePath, source);
    TEST_ASSERT_NOT_NULL(function);

    memset(&config, 0, sizeof(config));
    config.address = "127.0.0.1:0";
    config.suspend_on_start = ZR_TRUE;
    config.auth_token = "secret";

    TEST_ASSERT_TRUE(
            ZrDebug_AgentStart(state, function, "tests.debug.child_shape", &config, &agent, error, sizeof(error)));
    memset(&client, 0, sizeof(client));
    debug_client_connect(agent, &client);

    memset(&thread, 0, sizeof(thread));
    thread.state = state;
    thread.function = function;
    TEST_ASSERT_TRUE(debug_execution_thread_start(&thread));

    debug_client_initialize(&client, "tests.debug.child_shape");

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "moduleName", "tests.debug.child_shape");
    cJSON_AddStringToObject(params, "sourceFile", sourcePath);
    cJSON_AddItemToObject(params, "lines", cJSON_CreateIntArray(&breakpointLine, 1));
    debug_client_send_request(&client, 2, "setBreakpoints", params);
    message = debug_client_expect_event(&client, "breakpointResolved");
    TEST_ASSERT_TRUE(debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "resolved") != 0);
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
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "frameId", 1);
    debug_client_send_request(&client, 4, "scopes", params);
    message = debug_client_expect_response(&client, 4);
    scopes = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "scopes");
    TEST_ASSERT_TRUE(cJSON_IsArray(scopes));
    valueScope = debug_find_named_object(scopes, valueScopeName);
    TEST_ASSERT_NOT_NULL(valueScope);
    globalsScope = debug_find_named_object(scopes, "Globals");
    TEST_ASSERT_NOT_NULL(globalsScope);
    valueScopeId = debug_json_int(valueScope, "scopeId");
    globalsScopeId = debug_json_int(globalsScope, "scopeId");
    TEST_ASSERT_TRUE(valueScopeId > 0);
    TEST_ASSERT_TRUE(globalsScopeId > 0);
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "scopeId", valueScopeId);
    debug_client_send_request(&client, 5, "variables", params);
    message = debug_client_expect_response(&client, 5);
    values = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "variables");
    TEST_ASSERT_TRUE(cJSON_IsArray(values));
    insideItem = debug_find_named_object(values, "inside");
    TEST_ASSERT_NOT_NULL(insideItem);
    TEST_ASSERT_EQUAL_STRING("7", debug_json_string(insideItem, "value"));
    debug_assert_text_contains(debug_json_string(insideItem, "referenceSummary"), expectedReferenceSummary);
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "scopeId", globalsScopeId);
    debug_client_send_request(&client, 6, "variables", params);
    message = debug_client_expect_response(&client, 6);
    values = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "variables");
    TEST_ASSERT_TRUE(cJSON_IsArray(values));
    zrItem = debug_find_named_object(values, "zr");
    TEST_ASSERT_NOT_NULL(zrItem);
    zrHandle = debug_json_int(zrItem, "variablesReference");
    zrItemNamedVariables = debug_json_int(zrItem, "namedVariables");
    zrItemIndexedVariables = debug_json_int(zrItem, "indexedVariables");
    TEST_ASSERT_TRUE(zrHandle > 0);
    debug_assert_text_contains(debug_json_string(zrItem, "semanticSummary"), "expandable");
    debug_assert_text_contains(debug_json_string(zrItem, "semanticSummary"), "named");
    debug_assert_text_contains(debug_json_string(zrItem, "referenceSummary"), "global zr");
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "scopeId", zrHandle);
    debug_client_send_request(&client, 7, "variables", params);
    message = debug_client_expect_response(&client, 7);
    result = cJSON_GetObjectItemCaseSensitive(message, "result");
    zrNamedVariables = debug_json_int(result, "namedVariables");
    zrIndexedVariables = debug_json_int(result, "indexedVariables");
    TEST_ASSERT_TRUE(zrNamedVariables > 0 || zrIndexedVariables > 0);
    TEST_ASSERT_EQUAL_INT(zrNamedVariables, zrItemNamedVariables);
    TEST_ASSERT_EQUAL_INT(zrIndexedVariables, zrItemIndexedVariables);
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "expression", "zr");
    cJSON_AddNumberToObject(params, "frameId", 1);
    debug_client_send_request(&client, 8, "evaluate", params);
    message = debug_client_expect_response(&client, 8);
    result = cJSON_GetObjectItemCaseSensitive(message, "result");
    evaluateHandle = debug_json_int(result, "variablesReference");
    TEST_ASSERT_TRUE(evaluateHandle > 0);
    TEST_ASSERT_EQUAL_INT(zrNamedVariables, debug_json_int(result, "namedVariables"));
    TEST_ASSERT_EQUAL_INT(zrIndexedVariables, debug_json_int(result, "indexedVariables"));
    debug_assert_text_contains(debug_json_string(result, "semanticSummary"), "expandable");
    debug_assert_text_contains(debug_json_string(result, "semanticSummary"), "named");
    debug_assert_text_contains(debug_json_string(result, "semanticSummary"), "reference read zr");
    debug_assert_text_contains(debug_json_string(result, "referenceSummary"), "global zr");
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "expression", "inside");
    cJSON_AddNumberToObject(params, "frameId", 1);
    debug_client_send_request(&client, 9, "evaluate", params);
    message = debug_client_expect_response(&client, 9);
    result = cJSON_GetObjectItemCaseSensitive(message, "result");
    TEST_ASSERT_EQUAL_STRING("7", debug_json_string(result, "value"));
    debug_assert_text_contains(debug_json_string(result, "referenceSummary"), expectedReferenceSummary);
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "expression", "inside + 1");
    cJSON_AddNumberToObject(params, "frameId", 1);
    debug_client_send_request(&client, 10, "evaluate", params);
    message = debug_client_expect_response(&client, 10);
    result = cJSON_GetObjectItemCaseSensitive(message, "result");
    TEST_ASSERT_EQUAL_STRING("8", debug_json_string(result, "value"));
    debug_assert_text_contains(debug_json_string(result, "referenceSummary"), expectedReferenceSummary);
    debug_assert_text_contains(debug_json_string(result, "semanticSummary"), "reference read inside");
    debug_assert_text_contains(debug_json_string(result, "semanticSummary"), "range 8..8");
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "expression", "1 + 2");
    cJSON_AddNumberToObject(params, "frameId", 1);
    debug_client_send_request(&client, 11, "evaluate", params);
    message = debug_client_expect_response(&client, 11);
    result = cJSON_GetObjectItemCaseSensitive(message, "result");
    debug_assert_text_contains(debug_json_string(result, "semanticSummary"), "integer 3");
    debug_assert_text_contains(debug_json_string(result, "semanticSummary"), "expression binary exact");
    debug_assert_text_contains(debug_json_string(result, "semanticSummary"), "constant 3");
    debug_assert_text_contains(debug_json_string(result, "semanticSummary"), "range 3..3");
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "expression", "(1 + 2) * 3");
    cJSON_AddNumberToObject(params, "frameId", 1);
    debug_client_send_request(&client, 12, "evaluate", params);
    message = debug_client_expect_response(&client, 12);
    result = cJSON_GetObjectItemCaseSensitive(message, "result");
    TEST_ASSERT_EQUAL_STRING("9", debug_json_string(result, "value"));
    debug_assert_text_contains(debug_json_string(result, "semanticSummary"), "constant 9");
    debug_assert_text_contains(debug_json_string(result, "semanticSummary"), "constant 3");
    debug_assert_text_contains(debug_json_string(result, "semanticSummary"), "range 3..3");
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "expression", "true || false");
    cJSON_AddNumberToObject(params, "frameId", 1);
    debug_client_send_request(&client, 13, "evaluate", params);
    message = debug_client_expect_response(&client, 13);
    result = cJSON_GetObjectItemCaseSensitive(message, "result");
    debug_assert_text_contains(debug_json_string(result, "semanticSummary"), "logical true");
    debug_assert_text_contains(debug_json_string(result, "semanticSummary"), "short-circuits");
    debug_assert_text_contains(debug_json_string(result, "semanticSummary"), "unreachable because short-circuit skips evaluation");
    debug_assert_no_reference_summary(result);
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "expression", "true ? 1 : 2");
    cJSON_AddNumberToObject(params, "frameId", 1);
    debug_client_send_request(&client, 14, "evaluate", params);
    message = debug_client_expect_response(&client, 14);
    result = cJSON_GetObjectItemCaseSensitive(message, "result");
    TEST_ASSERT_EQUAL_STRING("1", debug_json_string(result, "value"));
    debug_assert_text_contains(debug_json_string(result, "semanticSummary"), "logical true");
    debug_assert_text_contains(debug_json_string(result, "semanticSummary"), "unreachable because a constant branch skips evaluation");
    debug_assert_no_reference_summary(result);
    cJSON_Delete(message);

    debug_client_send_request(&client, 15, "continue", ZR_NULL);
    message = debug_client_expect_response(&client, 15);
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "continued");
    cJSON_Delete(message);

    debug_execution_thread_join(&thread);
    TEST_ASSERT_TRUE(thread.success);
    TEST_ASSERT_EQUAL_INT64(1, thread.result);

    ZrNetwork_StreamClose(&client);
    ZrDebug_AgentStop(agent);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_protocol_expands_union_variant_payloads(void) {
    const char *sourcePath = "debug_union_variant_payload_fixture.zr";
    const char *source =
            "union Shape {\n"
            "    Empty;\n"
            "    Circle(radius: int);\n"
            "    Rect { width: int; height: int; }\n"
            "}\n"
            "func pauseHere() {\n"
            "    var circle: Shape = Shape.Circle(7);\n"
            "    var rect: Shape = Shape.Rect { width: 3, height: 4 };\n"
            "    return 1;\n"
            "}\n"
            "var marker = pauseHere();\n"
            "return 1;";
    const int breakpointLine = 9;
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugAgentConfig config;
    ZrDebugAgent *agent = ZR_NULL;
    SZrNetworkStream client;
    ZrDebugExecutionThread thread;
    TZrChar error[256];
    cJSON *message;
    cJSON *params;
    cJSON *scopes;
    cJSON *localsScope;
    cJSON *values;
    cJSON *circleItem;
    cJSON *rectItem;
    cJSON *variantItem;
    cJSON *radiusItem;
    cJSON *widthItem;
    cJSON *heightItem;
    cJSON *result;
    int localsScopeId;
    int circleHandle;
    int rectHandle;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_source(state, sourcePath, source);
    TEST_ASSERT_NOT_NULL(function);

    memset(&config, 0, sizeof(config));
    config.address = "127.0.0.1:0";
    config.suspend_on_start = ZR_TRUE;
    config.auth_token = "secret";

    TEST_ASSERT_TRUE(
            ZrDebug_AgentStart(state, function, "tests.debug.union_variant_payload", &config, &agent, error, sizeof(error)));
    memset(&client, 0, sizeof(client));
    debug_client_connect(agent, &client);

    memset(&thread, 0, sizeof(thread));
    thread.state = state;
    thread.function = function;
    TEST_ASSERT_TRUE(debug_execution_thread_start(&thread));

    debug_client_initialize(&client, "tests.debug.union_variant_payload");

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "moduleName", "tests.debug.union_variant_payload");
    cJSON_AddStringToObject(params, "sourceFile", sourcePath);
    cJSON_AddItemToObject(params, "lines", cJSON_CreateIntArray(&breakpointLine, 1));
    debug_client_send_request(&client, 2, "setBreakpoints", params);
    message = debug_client_expect_event(&client, "breakpointResolved");
    TEST_ASSERT_TRUE(debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "resolved") != 0);
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
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "frameId", 1);
    debug_client_send_request(&client, 4, "scopes", params);
    message = debug_client_expect_response(&client, 4);
    scopes = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "scopes");
    TEST_ASSERT_TRUE(cJSON_IsArray(scopes));
    localsScope = debug_find_named_object(scopes, "Locals");
    TEST_ASSERT_NOT_NULL(localsScope);
    localsScopeId = debug_json_int(localsScope, "scopeId");
    TEST_ASSERT_TRUE(localsScopeId > 0);
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "scopeId", localsScopeId);
    debug_client_send_request(&client, 5, "variables", params);
    message = debug_client_expect_response(&client, 5);
    values = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "variables");
    TEST_ASSERT_TRUE(cJSON_IsArray(values));
    circleItem = debug_find_named_object(values, "circle");
    rectItem = debug_find_named_object(values, "rect");
    TEST_ASSERT_NOT_NULL(circleItem);
    TEST_ASSERT_NOT_NULL(rectItem);
    TEST_ASSERT_EQUAL_STRING("Shape", debug_json_string(circleItem, "type"));
    TEST_ASSERT_EQUAL_STRING("<union Shape.Circle>", debug_json_string(circleItem, "value"));
    TEST_ASSERT_EQUAL_INT(2, debug_json_int(circleItem, "namedVariables"));
    TEST_ASSERT_EQUAL_INT(0, debug_json_int(circleItem, "indexedVariables"));
    debug_assert_text_contains(debug_json_string(circleItem, "semanticSummary"), "union Shape.Circle");
    TEST_ASSERT_EQUAL_STRING("Shape", debug_json_string(rectItem, "type"));
    TEST_ASSERT_EQUAL_STRING("<union Shape.Rect>", debug_json_string(rectItem, "value"));
    TEST_ASSERT_EQUAL_INT(3, debug_json_int(rectItem, "namedVariables"));
    TEST_ASSERT_EQUAL_INT(0, debug_json_int(rectItem, "indexedVariables"));
    debug_assert_text_contains(debug_json_string(rectItem, "semanticSummary"), "union Shape.Rect");
    circleHandle = debug_json_int(circleItem, "variablesReference");
    rectHandle = debug_json_int(rectItem, "variablesReference");
    TEST_ASSERT_TRUE(circleHandle > 0);
    TEST_ASSERT_TRUE(rectHandle > 0);
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "scopeId", circleHandle);
    debug_client_send_request(&client, 6, "variables", params);
    message = debug_client_expect_response(&client, 6);
    result = cJSON_GetObjectItemCaseSensitive(message, "result");
    TEST_ASSERT_EQUAL_INT(2, debug_json_int(result, "namedVariables"));
    TEST_ASSERT_EQUAL_INT(0, debug_json_int(result, "indexedVariables"));
    values = cJSON_GetObjectItemCaseSensitive(result, "variables");
    TEST_ASSERT_TRUE(cJSON_IsArray(values));
    variantItem = debug_find_named_object(values, "variant");
    radiusItem = debug_find_named_object(values, "radius");
    TEST_ASSERT_NOT_NULL(variantItem);
    TEST_ASSERT_NOT_NULL(radiusItem);
    TEST_ASSERT_EQUAL_STRING("Circle", debug_json_string(variantItem, "value"));
    TEST_ASSERT_EQUAL_STRING("7", debug_json_string(radiusItem, "value"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "scopeId", rectHandle);
    debug_client_send_request(&client, 7, "variables", params);
    message = debug_client_expect_response(&client, 7);
    result = cJSON_GetObjectItemCaseSensitive(message, "result");
    TEST_ASSERT_EQUAL_INT(3, debug_json_int(result, "namedVariables"));
    TEST_ASSERT_EQUAL_INT(0, debug_json_int(result, "indexedVariables"));
    values = cJSON_GetObjectItemCaseSensitive(result, "variables");
    TEST_ASSERT_TRUE(cJSON_IsArray(values));
    variantItem = debug_find_named_object(values, "variant");
    widthItem = debug_find_named_object(values, "width");
    heightItem = debug_find_named_object(values, "height");
    TEST_ASSERT_NOT_NULL(variantItem);
    TEST_ASSERT_NOT_NULL(widthItem);
    TEST_ASSERT_NOT_NULL(heightItem);
    TEST_ASSERT_EQUAL_STRING("Rect", debug_json_string(variantItem, "value"));
    TEST_ASSERT_EQUAL_STRING("3", debug_json_string(widthItem, "value"));
    TEST_ASSERT_EQUAL_STRING("4", debug_json_string(heightItem, "value"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "expression", "circle.variant");
    cJSON_AddNumberToObject(params, "frameId", 1);
    debug_client_send_request(&client, 8, "evaluate", params);
    message = debug_client_expect_response(&client, 8);
    result = cJSON_GetObjectItemCaseSensitive(message, "result");
    TEST_ASSERT_EQUAL_STRING("Circle", debug_json_string(result, "value"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "expression", "circle.radius");
    cJSON_AddNumberToObject(params, "frameId", 1);
    debug_client_send_request(&client, 9, "evaluate", params);
    message = debug_client_expect_response(&client, 9);
    result = cJSON_GetObjectItemCaseSensitive(message, "result");
    TEST_ASSERT_EQUAL_STRING("7", debug_json_string(result, "value"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "expression", "rect.width + rect.height");
    cJSON_AddNumberToObject(params, "frameId", 1);
    debug_client_send_request(&client, 10, "evaluate", params);
    message = debug_client_expect_response(&client, 10);
    result = cJSON_GetObjectItemCaseSensitive(message, "result");
    TEST_ASSERT_EQUAL_STRING("7", debug_json_string(result, "value"));
    cJSON_Delete(message);

    debug_client_send_request(&client, 11, "continue", ZR_NULL);
    message = debug_client_expect_response(&client, 11);
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "continued");
    cJSON_Delete(message);

    debug_execution_thread_join(&thread);
    TEST_ASSERT_TRUE(thread.success);
    TEST_ASSERT_EQUAL_INT64(1, thread.result);

    ZrNetwork_StreamClose(&client);
    ZrDebug_AgentStop(agent);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_evaluate_semantic_summary_uses_closure_captures(void) {
    const char *sourcePath = "debug_closure_capture_semantic_summary_fixture.zr";
    const char *source =
            "func makeRunner() {\n"
            "    var seed = 4;\n"
            "    return () => {\n"
            "        return seed + 1;\n"
            "    };\n"
            "}\n"
            "var runner = makeRunner();\n"
            "return runner();";
    const int breakpointLine = 4;
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugAgentConfig config;
    ZrDebugAgent *agent = ZR_NULL;
    SZrNetworkStream client;
    ZrDebugExecutionThread thread;
    TZrChar error[256];
    cJSON *message;
    cJSON *params;
    cJSON *result;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_source(state, sourcePath, source);
    TEST_ASSERT_NOT_NULL(function);

    memset(&config, 0, sizeof(config));
    config.address = "127.0.0.1:0";
    config.suspend_on_start = ZR_TRUE;
    config.auth_token = "secret";

    TEST_ASSERT_TRUE(
            ZrDebug_AgentStart(state, function, "tests.debug.closure_capture_semantic", &config, &agent, error, sizeof(error)));
    memset(&client, 0, sizeof(client));
    debug_client_connect(agent, &client);

    memset(&thread, 0, sizeof(thread));
    thread.state = state;
    thread.function = function;
    TEST_ASSERT_TRUE(debug_execution_thread_start(&thread));

    debug_client_initialize(&client, "tests.debug.closure_capture_semantic");

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "moduleName", "tests.debug.closure_capture_semantic");
    cJSON_AddStringToObject(params, "sourceFile", sourcePath);
    cJSON_AddItemToObject(params, "lines", cJSON_CreateIntArray(&breakpointLine, 1));
    debug_client_send_request(&client, 2, "setBreakpoints", params);
    message = debug_client_expect_event(&client, "breakpointResolved");
    TEST_ASSERT_TRUE(debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "resolved") != 0);
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
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "expression", "seed + 1");
    cJSON_AddNumberToObject(params, "frameId", 1);
    debug_client_send_request(&client, 4, "evaluate", params);
    message = debug_client_expect_response(&client, 4);
    result = cJSON_GetObjectItemCaseSensitive(message, "result");
    TEST_ASSERT_EQUAL_STRING("5", debug_json_string(result, "value"));
    debug_assert_text_contains(debug_json_string(result, "referenceSummary"), "closure seed");
    debug_assert_text_contains(debug_json_string(result, "semanticSummary"), "reference read seed");
    debug_assert_text_contains(debug_json_string(result, "semanticSummary"), "expression binary exact");
    debug_assert_text_contains(debug_json_string(result, "semanticSummary"), "range 5..5");
    debug_assert_text_contains(debug_json_string(result, "semanticSummary"), "unsigned range 5..5");
    cJSON_Delete(message);

    debug_client_send_request(&client, 5, "continue", ZR_NULL);
    message = debug_client_expect_response(&client, 5);
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "continued");
    cJSON_Delete(message);

    debug_execution_thread_join(&thread);
    TEST_ASSERT_TRUE(thread.success);
    TEST_ASSERT_EQUAL_INT64(5, thread.result);

    ZrNetwork_StreamClose(&client);
    ZrDebug_AgentStop(agent);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_evaluate_index_window_reports_base_reference_summary(void) {
    const char *sourcePath = "debug_index_window_reference_fixture.zr";
    const char *source =
            "func pauseHere() {\n"
            "    return 1;\n"
            "}\n"
            "var marker = pauseHere();\n"
            "return 1;";
    const int breakpointLine = 2;
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrString *arrayText;
    SZrObject *arrayObject = ZR_NULL;
    ZrDebugAgentConfig config;
    ZrDebugAgent *agent = ZR_NULL;
    SZrNetworkStream client;
    ZrDebugExecutionThread thread;
    TZrChar error[256];
    cJSON *message;
    cJSON *params;
    cJSON *result;

    TEST_ASSERT_NOT_NULL(state);
    arrayText = ZrCore_String_CreateFromNative(state, "abcd");
    TEST_ASSERT_NOT_NULL(arrayText);
    TEST_ASSERT_TRUE(ZrCore_String_ToByteArray(state, arrayText, &arrayObject));
    TEST_ASSERT_NOT_NULL(arrayObject);
    ZrCore_Value_InitAsRawObject(state, &state->global->zrObject, ZR_CAST_RAW_OBJECT_AS_SUPER(arrayObject));
    state->global->zrObject.type = ZR_VALUE_TYPE_ARRAY;
    function = compile_debug_source(state, sourcePath, source);
    TEST_ASSERT_NOT_NULL(function);

    memset(&config, 0, sizeof(config));
    config.address = "127.0.0.1:0";
    config.suspend_on_start = ZR_TRUE;
    config.auth_token = "secret";

    TEST_ASSERT_TRUE(
            ZrDebug_AgentStart(state, function, "tests.debug.index_window_reference", &config, &agent, error, sizeof(error)));
    memset(&client, 0, sizeof(client));
    debug_client_connect(agent, &client);

    memset(&thread, 0, sizeof(thread));
    thread.state = state;
    thread.function = function;
    TEST_ASSERT_TRUE(debug_execution_thread_start(&thread));

    debug_client_initialize(&client, "tests.debug.index_window_reference");

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "moduleName", "tests.debug.index_window_reference");
    cJSON_AddStringToObject(params, "sourceFile", sourcePath);
    cJSON_AddItemToObject(params, "lines", cJSON_CreateIntArray(&breakpointLine, 1));
    debug_client_send_request(&client, 2, "setBreakpoints", params);
    message = debug_client_expect_event(&client, "breakpointResolved");
    TEST_ASSERT_TRUE(debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "resolved") != 0);
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
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "expression", "zr[1..3]");
    cJSON_AddNumberToObject(params, "frameId", 1);
    debug_client_send_request(&client, 4, "evaluate", params);
    message = debug_client_expect_response(&client, 4);
    result = cJSON_GetObjectItemCaseSensitive(message, "result");
    TEST_ASSERT_TRUE(debug_json_int(result, "variablesReference") > 0);
    TEST_ASSERT_EQUAL_INT(2, debug_json_int(result, "indexedVariables"));
    debug_assert_text_contains(debug_json_string(result, "semanticSummary"), "indexed window");
    debug_assert_text_contains(debug_json_string(result, "referenceSummary"), "global zr");
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "expression", "zr[1]");
    cJSON_AddNumberToObject(params, "frameId", 1);
    debug_client_send_request(&client, 5, "evaluate", params);
    message = debug_client_expect_response(&client, 5);
    result = cJSON_GetObjectItemCaseSensitive(message, "result");
    debug_assert_text_contains(debug_json_string(result, "referenceSummary"), "global zr");
    debug_assert_text_contains(debug_json_string(result, "referenceSummary"), "index access");
    cJSON_Delete(message);

    debug_client_send_request(&client, 6, "continue", ZR_NULL);
    message = debug_client_expect_response(&client, 6);
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "continued");
    cJSON_Delete(message);

    debug_execution_thread_join(&thread);
    TEST_ASSERT_TRUE(thread.success);
    TEST_ASSERT_EQUAL_INT64(1, thread.result);

    ZrNetwork_StreamClose(&client);
    ZrDebug_AgentStop(agent);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

void setUp(void) {}

void tearDown(void) {}

#define RUN_DEBUG_TEST(filter, testName) \
    do { \
        if ((filter) == ZR_NULL || strcmp((filter), #testName) == 0) { \
            RUN_TEST(testName); \
        } \
    } while (0)

int main(int argc, char **argv) {
    const char *filter = argc > 1 ? argv[1] : ZR_NULL;
    UNITY_BEGIN();
    RUN_DEBUG_TEST(filter, test_debug_protocol_reports_per_value_child_shape_metadata);
    RUN_DEBUG_TEST(filter, test_debug_protocol_expands_union_variant_payloads);
    RUN_DEBUG_TEST(filter, test_debug_evaluate_semantic_summary_uses_closure_captures);
    RUN_DEBUG_TEST(filter, test_debug_evaluate_index_window_reports_base_reference_summary);
    return UNITY_END();
}
