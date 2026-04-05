#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON/cJSON.h"
#include "unity.h"
#include "runtime_support.h"
#include "zr_vm_core/io.h"
#include "zr_vm_lib_debug/debug.h"
#include "zr_vm_lib_network/network.h"
#include "zr_vm_parser.h"
#include "zr_vm_parser/writer.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef enum EZrDebugExecutionMode {
    ZR_DEBUG_EXECUTION_EXPECT_INT64 = 1,
    ZR_DEBUG_EXECUTION_CAPTURE_FAILURE = 2
} EZrDebugExecutionMode;

typedef struct ZrDebugExecutionThread {
    SZrState *state;
    SZrFunction *function;
    ZrDebugAgent *agent;
    TZrInt64 result;
    TZrBool success;
    TZrBool notifyExceptionOnFailure;
    TZrBool notifyTerminated;
    EZrDebugExecutionMode mode;
#if defined(_WIN32)
    HANDLE handle;
#else
    pthread_t handle;
#endif
} ZrDebugExecutionThread;

typedef struct SZrBinaryFixtureReader {
    TZrByte *bytes;
    TZrSize length;
    TZrBool consumed;
} SZrBinaryFixtureReader;

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

static TZrByte *read_binary_file_owned(const TZrChar *path, TZrSize *outLength) {
    FILE *file;
    long fileSize;
    TZrByte *buffer;

    if (outLength != ZR_NULL) {
        *outLength = 0;
    }
    if (path == ZR_NULL) {
        return ZR_NULL;
    }

    file = fopen(path, "rb");
    if (file == ZR_NULL) {
        return ZR_NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return ZR_NULL;
    }

    fileSize = ftell(file);
    if (fileSize < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return ZR_NULL;
    }

    buffer = (TZrByte *)malloc((size_t)fileSize);
    if (buffer == ZR_NULL) {
        fclose(file);
        return ZR_NULL;
    }

    if (fileSize > 0 && fread(buffer, 1, (size_t)fileSize, file) != (size_t)fileSize) {
        free(buffer);
        fclose(file);
        return ZR_NULL;
    }

    fclose(file);
    if (outLength != ZR_NULL) {
        *outLength = (TZrSize)fileSize;
    }
    return buffer;
}

static TZrBytePtr binary_fixture_reader_read(struct SZrState *state, TZrPtr customData, ZR_OUT TZrSize *size) {
    SZrBinaryFixtureReader *reader = (SZrBinaryFixtureReader *)customData;

    ZR_UNUSED_PARAMETER(state);

    if (reader == ZR_NULL || size == ZR_NULL || reader->consumed || reader->bytes == ZR_NULL) {
        return ZR_NULL;
    }

    reader->consumed = ZR_TRUE;
    *size = reader->length;
    return reader->bytes;
}

static void binary_fixture_reader_close(struct SZrState *state, TZrPtr customData) {
    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(customData);
}

static SZrFunction *load_binary_debug_agent_fixture(SZrState *state,
                                                    SZrFunction *sourceFunction,
                                                    const char *binaryPath,
                                                    const char *moduleName,
                                                    const char *sourceHash) {
    SZrBinaryWriterOptions options;
    TZrByte *buffer = ZR_NULL;
    TZrSize bufferLength = 0;
    SZrBinaryFixtureReader reader;
    SZrIo io;
    SZrIoSource *sourceObject = ZR_NULL;
    SZrFunction *runtimeFunction = ZR_NULL;

    if (state == ZR_NULL || sourceFunction == ZR_NULL || binaryPath == ZR_NULL || moduleName == ZR_NULL ||
        sourceHash == ZR_NULL) {
        return ZR_NULL;
    }

    memset(&options, 0, sizeof(options));
    options.moduleName = moduleName;
    options.moduleHash = sourceHash;
    if (!ZrParser_Writer_WriteBinaryFileWithOptions(state, sourceFunction, binaryPath, &options)) {
        return ZR_NULL;
    }

    buffer = read_binary_file_owned(binaryPath, &bufferLength);
    if (buffer == ZR_NULL) {
        return ZR_NULL;
    }

    memset(&reader, 0, sizeof(reader));
    reader.bytes = buffer;
    reader.length = bufferLength;

    ZrCore_Io_Init(state, &io, binary_fixture_reader_read, binary_fixture_reader_close, &reader);
    sourceObject = ZrCore_Io_ReadSourceNew(&io);
    if (sourceObject == ZR_NULL) {
        free(buffer);
        return ZR_NULL;
    }

    runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
    ZrCore_Io_ReadSourceFree(state->global, sourceObject);
    free(buffer);
    return runtimeFunction;
}

static void debug_execution_thread_run(ZrDebugExecutionThread *thread) {
    if (thread == ZR_NULL) {
        return;
    }

    if (thread->mode == ZR_DEBUG_EXECUTION_CAPTURE_FAILURE) {
        SZrTypeValue ignoredResult;
        thread->success = ZrTests_Runtime_Function_ExecuteCaptureFailure(thread->state, thread->function, &ignoredResult);
        if (!thread->success && thread->notifyExceptionOnFailure && thread->agent != ZR_NULL) {
            ZrDebug_NotifyException(thread->agent);
        }
    } else {
        thread->success = ZrTests_Runtime_Function_ExecuteExpectInt64(thread->state, thread->function, &thread->result);
    }

    if (thread->notifyTerminated && thread->agent != ZR_NULL) {
        ZrDebug_NotifyTerminated(thread->agent, thread->success);
    }
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
    if (thread->mode == 0) {
        thread->mode = ZR_DEBUG_EXECUTION_EXPECT_INT64;
    }
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

    TEST_ASSERT_TRUE(ZrNetwork_StreamReadFrame(stream, 5000, frame, sizeof(frame), &length));
    TEST_ASSERT_TRUE(length > 0);

    cJSON *message = cJSON_Parse(frame);
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

static cJSON *debug_client_expect_error_response(SZrNetworkStream *stream, int id, int code) {
    cJSON *message = debug_client_read_message(stream);
    cJSON *idItem = cJSON_GetObjectItemCaseSensitive(message, "id");
    cJSON *errorItem = cJSON_GetObjectItemCaseSensitive(message, "error");
    cJSON *codeItem;

    TEST_ASSERT_TRUE(cJSON_IsNumber(idItem));
    TEST_ASSERT_EQUAL_INT(id, (int)idItem->valuedouble);
    TEST_ASSERT_NOT_NULL(errorItem);
    codeItem = cJSON_GetObjectItemCaseSensitive(errorItem, "code");
    TEST_ASSERT_TRUE(cJSON_IsNumber(codeItem));
    TEST_ASSERT_EQUAL_INT(code, (int)codeItem->valuedouble);
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

static void test_debug_agent_rejects_invalid_token_and_continues_execution(void) {
    const char *sourcePath = "debug_agent_invalid_token_fixture.zr";
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
    config.stop_on_uncaught_exception = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrDebug_AgentStart(state, function, "tests.debug.invalid_token", &config, &agent, error, sizeof(error)));
    memset(&client, 0, sizeof(client));
    debug_client_connect(agent, &client);

    debug_client_send_request(&client, 1, "initialize", cJSON_CreateObject());

    memset(&thread, 0, sizeof(thread));
    thread.state = state;
    thread.function = function;
    TEST_ASSERT_TRUE(debug_execution_thread_start(&thread));

    cJSON *message = debug_client_expect_error_response(&client, 1, -32001);
    cJSON_Delete(message);

    debug_execution_thread_join(&thread);
    TEST_ASSERT_TRUE(thread.success);
    TEST_ASSERT_EQUAL_INT64(7, thread.result);

    ZrNetwork_StreamClose(&client);
    ZrDebug_AgentStop(agent);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_agent_wait_for_client_accepts_initialize_without_auth_token(void) {
    const char *sourcePath = "debug_agent_no_auth_wait_fixture.zr";
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
    config.wait_for_client = ZR_TRUE;
    config.auth_token = ZR_NULL;
    config.stop_on_uncaught_exception = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrDebug_AgentStart(state, function, "tests.debug.no_auth_wait", &config, &agent, error, sizeof(error)));
    memset(&client, 0, sizeof(client));
    debug_client_connect(agent, &client);

    memset(&thread, 0, sizeof(thread));
    thread.state = state;
    thread.function = function;
    TEST_ASSERT_TRUE(debug_execution_thread_start(&thread));

    debug_client_send_request(&client, 1, "initialize", cJSON_CreateObject());
    message = debug_client_expect_response(&client, 1);
    TEST_ASSERT_EQUAL_STRING("zrdbg/1", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "result"), "protocol"));
    cJSON_Delete(message);

    message = debug_client_expect_event(&client, "initialized");
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "moduleLoaded");
    TEST_ASSERT_EQUAL_STRING("tests.debug.no_auth_wait",
                             debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "moduleName"));
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "stopped");
    TEST_ASSERT_EQUAL_STRING("entry", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "reason"));
    cJSON_Delete(message);

    debug_client_send_request(&client, 2, "continue", ZR_NULL);
    message = debug_client_expect_response(&client, 2);
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "continued");
    cJSON_Delete(message);

    debug_execution_thread_join(&thread);
    TEST_ASSERT_TRUE(thread.success);
    TEST_ASSERT_EQUAL_INT64(7, thread.result);

    ZrNetwork_StreamClose(&client);
    ZrDebug_AgentStop(agent);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_agent_wait_for_client_stays_paused_after_invalid_auth(void) {
    const char *sourcePath = "debug_agent_wait_invalid_auth_fixture.zr";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugAgentConfig config;
    ZrDebugAgent *agent = ZR_NULL;
    SZrNetworkStream firstClient;
    SZrNetworkStream secondClient;
    ZrDebugExecutionThread thread;
    TZrChar error[256];
    cJSON *message;
    cJSON *params;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_agent_fixture(state, sourcePath);
    TEST_ASSERT_NOT_NULL(function);

    memset(&config, 0, sizeof(config));
    config.address = "127.0.0.1:0";
    config.suspend_on_start = ZR_TRUE;
    config.wait_for_client = ZR_TRUE;
    config.auth_token = "secret";
    config.stop_on_uncaught_exception = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrDebug_AgentStart(state, function, "tests.debug.wait_invalid_auth", &config, &agent, error, sizeof(error)));
    memset(&firstClient, 0, sizeof(firstClient));
    debug_client_connect(agent, &firstClient);

    memset(&thread, 0, sizeof(thread));
    thread.state = state;
    thread.function = function;
    TEST_ASSERT_TRUE(debug_execution_thread_start(&thread));

    debug_client_send_request(&firstClient, 1, "initialize", cJSON_CreateObject());
    message = debug_client_expect_error_response(&firstClient, 1, -32001);
    cJSON_Delete(message);
    ZrNetwork_StreamClose(&firstClient);

    memset(&secondClient, 0, sizeof(secondClient));
    debug_client_connect(agent, &secondClient);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "authToken", "secret");
    debug_client_send_request(&secondClient, 2, "initialize", params);
    message = debug_client_expect_response(&secondClient, 2);
    TEST_ASSERT_EQUAL_STRING("zrdbg/1", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "result"), "protocol"));
    cJSON_Delete(message);

    message = debug_client_expect_event(&secondClient, "initialized");
    cJSON_Delete(message);
    message = debug_client_expect_event(&secondClient, "moduleLoaded");
    TEST_ASSERT_EQUAL_STRING("tests.debug.wait_invalid_auth",
                             debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "moduleName"));
    cJSON_Delete(message);
    message = debug_client_expect_event(&secondClient, "stopped");
    TEST_ASSERT_EQUAL_STRING("entry", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "reason"));
    cJSON_Delete(message);

    debug_client_send_request(&secondClient, 3, "continue", ZR_NULL);
    message = debug_client_expect_response(&secondClient, 3);
    cJSON_Delete(message);
    message = debug_client_expect_event(&secondClient, "continued");
    cJSON_Delete(message);

    debug_execution_thread_join(&thread);
    TEST_ASSERT_TRUE(thread.success);
    TEST_ASSERT_EQUAL_INT64(7, thread.result);

    ZrNetwork_StreamClose(&secondClient);
    ZrDebug_AgentStop(agent);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_agent_breakpoint_step_and_variable_snapshots_over_tcp(void) {
    const char *sourcePath = "debug_agent_roundtrip_fixture.zr";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugAgentConfig config;
    ZrDebugAgent *agent = ZR_NULL;
    SZrNetworkStream client;
    ZrDebugExecutionThread thread;
    TZrChar error[256];
    cJSON *message;
    cJSON *params;
    cJSON *frames;
    cJSON *scopes;
    cJSON *values;
    cJSON *topFrame;
    cJSON *localsScope;
    cJSON *valueItem;
    cJSON *baseItem;
    int localsScopeId;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_agent_fixture(state, sourcePath);
    TEST_ASSERT_NOT_NULL(function);

    memset(&config, 0, sizeof(config));
    config.address = "127.0.0.1:0";
    config.suspend_on_start = ZR_TRUE;
    config.auth_token = "secret";
    config.stop_on_uncaught_exception = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrDebug_AgentStart(state, function, "tests.debug.roundtrip", &config, &agent, error, sizeof(error)));
    memset(&client, 0, sizeof(client));
    debug_client_connect(agent, &client);

    memset(&thread, 0, sizeof(thread));
    thread.state = state;
    thread.function = function;
    TEST_ASSERT_TRUE(debug_execution_thread_start(&thread));

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "authToken", "secret");
    debug_client_send_request(&client, 1, "initialize", params);

    message = debug_client_expect_response(&client, 1);
    TEST_ASSERT_EQUAL_STRING("zrdbg/1", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "result"), "protocol"));
    cJSON_Delete(message);

    message = debug_client_expect_event(&client, "initialized");
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "moduleLoaded");
    TEST_ASSERT_EQUAL_STRING("tests.debug.roundtrip",
                             debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "moduleName"));
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "stopped");
    TEST_ASSERT_EQUAL_STRING("entry", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "reason"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "moduleName", "tests.debug.roundtrip");
    cJSON_AddStringToObject(params, "sourceFile", sourcePath);
    cJSON_AddItemToObject(params, "lines", cJSON_CreateIntArray((const int[]){2}, 1));
    debug_client_send_request(&client, 2, "setBreakpoints", params);

    message = debug_client_expect_event(&client, "breakpointResolved");
    TEST_ASSERT_TRUE(debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "resolved") != 0);
    TEST_ASSERT_EQUAL_INT(2, debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "line"));
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
    TEST_ASSERT_EQUAL_STRING("addOne", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "functionName"));
    TEST_ASSERT_EQUAL_INT(2, debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "line"));
    cJSON_Delete(message);

    debug_client_send_request(&client, 4, "stackTrace", ZR_NULL);
    message = debug_client_expect_response(&client, 4);
    frames = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "frames");
    TEST_ASSERT_TRUE(cJSON_IsArray(frames));
    TEST_ASSERT_TRUE(cJSON_GetArraySize(frames) >= 2);
    topFrame = cJSON_GetArrayItem(frames, 0);
    TEST_ASSERT_EQUAL_STRING("addOne", debug_json_string(topFrame, "functionName"));
    TEST_ASSERT_EQUAL_INT(2, debug_json_int(topFrame, "line"));
    cJSON_Delete(message);

    debug_client_send_request(&client, 5, "next", ZR_NULL);
    message = debug_client_expect_response(&client, 5);
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "continued");
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "stopped");
    TEST_ASSERT_EQUAL_STRING("step", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "reason"));
    TEST_ASSERT_EQUAL_STRING("addOne", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "functionName"));
    TEST_ASSERT_EQUAL_INT(3, debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "line"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "frameId", 1);
    debug_client_send_request(&client, 6, "scopes", params);
    message = debug_client_expect_response(&client, 6);
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
    debug_client_send_request(&client, 7, "variables", params);
    message = debug_client_expect_response(&client, 7);
    values = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "variables");
    TEST_ASSERT_TRUE(cJSON_IsArray(values));
    valueItem = debug_find_named_object(values, "value");
    baseItem = debug_find_named_object(values, "base");
    TEST_ASSERT_NOT_NULL(valueItem);
    TEST_ASSERT_NOT_NULL(baseItem);
    TEST_ASSERT_EQUAL_STRING("4", debug_json_string(valueItem, "value"));
    TEST_ASSERT_EQUAL_STRING("5", debug_json_string(baseItem, "value"));
    cJSON_Delete(message);

    debug_client_send_request(&client, 8, "continue", ZR_NULL);
    message = debug_client_expect_response(&client, 8);
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "continued");
    cJSON_Delete(message);

    debug_execution_thread_join(&thread);
    TEST_ASSERT_TRUE(thread.success);
    TEST_ASSERT_EQUAL_INT64(7, thread.result);
    ZrDebug_NotifyTerminated(agent, thread.success);

    message = debug_client_expect_event(&client, "terminated");
    TEST_ASSERT_TRUE(debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "success") != 0);
    cJSON_Delete(message);

    ZrNetwork_StreamClose(&client);
    ZrDebug_AgentStop(agent);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_agent_step_in_and_out_cross_call_boundaries(void) {
    const char *sourcePath = "debug_agent_step_boundaries_fixture.zr";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugAgentConfig config;
    ZrDebugAgent *agent = ZR_NULL;
    SZrNetworkStream client;
    ZrDebugExecutionThread thread;
    TZrChar error[256];
    cJSON *message;
    cJSON *params;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_agent_fixture(state, sourcePath);
    TEST_ASSERT_NOT_NULL(function);

    memset(&config, 0, sizeof(config));
    config.address = "127.0.0.1:0";
    config.suspend_on_start = ZR_TRUE;
    config.auth_token = "secret";
    config.stop_on_uncaught_exception = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrDebug_AgentStart(state, function, "tests.debug.step_boundaries", &config, &agent, error, sizeof(error)));
    memset(&client, 0, sizeof(client));
    debug_client_connect(agent, &client);

    memset(&thread, 0, sizeof(thread));
    thread.state = state;
    thread.function = function;
    TEST_ASSERT_TRUE(debug_execution_thread_start(&thread));

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "authToken", "secret");
    debug_client_send_request(&client, 1, "initialize", params);
    message = debug_client_expect_response(&client, 1);
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "initialized");
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "moduleLoaded");
    TEST_ASSERT_EQUAL_STRING("tests.debug.step_boundaries",
                             debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "moduleName"));
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "stopped");
    TEST_ASSERT_EQUAL_STRING("entry", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "reason"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "moduleName", "tests.debug.step_boundaries");
    cJSON_AddStringToObject(params, "sourceFile", sourcePath);
    cJSON_AddItemToObject(params, "lines", cJSON_CreateIntArray((const int[]){5}, 1));
    debug_client_send_request(&client, 2, "setBreakpoints", params);
    message = debug_client_expect_event(&client, "breakpointResolved");
    TEST_ASSERT_TRUE(debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "resolved") != 0);
    TEST_ASSERT_EQUAL_INT(5, debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "line"));
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
    TEST_ASSERT_EQUAL_INT(5, debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "line"));
    cJSON_Delete(message);

    debug_client_send_request(&client, 4, "stepIn", ZR_NULL);
    message = debug_client_expect_response(&client, 4);
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "continued");
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "stopped");
    TEST_ASSERT_EQUAL_STRING("step", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "reason"));
    TEST_ASSERT_EQUAL_STRING("addOne", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "functionName"));
    TEST_ASSERT_EQUAL_INT(2, debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "line"));
    cJSON_Delete(message);

    debug_client_send_request(&client, 5, "stepOut", ZR_NULL);
    message = debug_client_expect_response(&client, 5);
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "continued");
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "stopped");
    TEST_ASSERT_EQUAL_STRING("step", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "reason"));
    TEST_ASSERT_EQUAL_STRING(sourcePath, debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "sourceFile"));
    TEST_ASSERT_EQUAL_INT(5, debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "line"));
    cJSON_Delete(message);

    debug_client_send_request(&client, 6, "continue", ZR_NULL);
    message = debug_client_expect_response(&client, 6);
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "continued");
    cJSON_Delete(message);

    debug_execution_thread_join(&thread);
    TEST_ASSERT_TRUE(thread.success);
    TEST_ASSERT_EQUAL_INT64(7, thread.result);
    ZrDebug_NotifyTerminated(agent, thread.success);

    message = debug_client_expect_event(&client, "terminated");
    TEST_ASSERT_TRUE(debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "success") != 0);
    cJSON_Delete(message);

    ZrNetwork_StreamClose(&client);
    ZrDebug_AgentStop(agent);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_agent_pause_stops_at_next_safepoint_and_preserves_stack(void) {
    const char *sourcePath = "debug_agent_pause_fixture.zr";
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
            "var pausedResult = spin(100000);\n"
            "return pausedResult;";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugAgentConfig config;
    ZrDebugAgent *agent = ZR_NULL;
    SZrNetworkStream client;
    ZrDebugExecutionThread thread;
    TZrChar error[256];
    cJSON *message;
    cJSON *params;
    cJSON *frames;
    cJSON *topFrame;
    cJSON *scopes;
    cJSON *localsScope;
    cJSON *values;
    cJSON *totalItem;
    cJSON *indexItem;
    int localsScopeId;
    int pausedLine;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_agent_source(state, sourcePath, source);
    TEST_ASSERT_NOT_NULL(function);

    memset(&config, 0, sizeof(config));
    config.address = "127.0.0.1:0";
    config.suspend_on_start = ZR_TRUE;
    config.auth_token = "secret";
    config.stop_on_uncaught_exception = ZR_TRUE;

    TEST_ASSERT_TRUE(
            ZrDebug_AgentStart(state, function, "tests.debug.pause_safepoint", &config, &agent, error, sizeof(error)));
    memset(&client, 0, sizeof(client));
    debug_client_connect(agent, &client);

    memset(&thread, 0, sizeof(thread));
    thread.state = state;
    thread.function = function;
    TEST_ASSERT_TRUE(debug_execution_thread_start(&thread));

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "authToken", "secret");
    debug_client_send_request(&client, 1, "initialize", params);
    message = debug_client_expect_response(&client, 1);
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "initialized");
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "moduleLoaded");
    TEST_ASSERT_EQUAL_STRING("tests.debug.pause_safepoint",
                             debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "moduleName"));
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "stopped");
    TEST_ASSERT_EQUAL_STRING("entry", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "reason"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "moduleName", "tests.debug.pause_safepoint");
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
    TEST_ASSERT_EQUAL_INT(7, debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "line"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "moduleName", "tests.debug.pause_safepoint");
    cJSON_AddStringToObject(params, "sourceFile", sourcePath);
    cJSON_AddItemToObject(params, "lines", cJSON_CreateArray());
    debug_client_send_request(&client, 4, "setBreakpoints", params);
    message = debug_client_expect_response(&client, 4);
    cJSON_Delete(message);

    debug_client_send_request(&client, 5, "continue", ZR_NULL);
    message = debug_client_expect_response(&client, 5);
    cJSON_Delete(message);

    ZrDebug_Pause(agent);
    message = debug_client_expect_event(&client, "continued");
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "stopped");
    TEST_ASSERT_EQUAL_STRING("pause", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "reason"));
    TEST_ASSERT_EQUAL_STRING(sourcePath, debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "sourceFile"));
    TEST_ASSERT_EQUAL_STRING("spin", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "functionName"));
    pausedLine = debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "line");
    TEST_ASSERT_TRUE(pausedLine >= 6);
    TEST_ASSERT_TRUE(pausedLine <= 10);
    cJSON_Delete(message);

    debug_client_send_request(&client, 6, "stackTrace", ZR_NULL);
    message = debug_client_expect_response(&client, 6);
    frames = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "frames");
    TEST_ASSERT_TRUE(cJSON_IsArray(frames));
    TEST_ASSERT_TRUE(cJSON_GetArraySize(frames) >= 2);
    topFrame = cJSON_GetArrayItem(frames, 0);
    TEST_ASSERT_EQUAL_STRING("spin", debug_json_string(topFrame, "functionName"));
    TEST_ASSERT_EQUAL_STRING(sourcePath, debug_json_string(topFrame, "sourceFile"));
    TEST_ASSERT_TRUE(debug_json_int(topFrame, "line") >= 6);
    TEST_ASSERT_TRUE(debug_json_int(topFrame, "line") <= 10);
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "frameId", 1);
    debug_client_send_request(&client, 7, "scopes", params);
    message = debug_client_expect_response(&client, 7);
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
    debug_client_send_request(&client, 8, "variables", params);
    message = debug_client_expect_response(&client, 8);
    values = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "variables");
    TEST_ASSERT_TRUE(cJSON_IsArray(values));
    totalItem = debug_find_named_object(values, "total");
    indexItem = debug_find_named_object(values, "index");
    TEST_ASSERT_NOT_NULL(totalItem);
    TEST_ASSERT_NOT_NULL(indexItem);
    TEST_ASSERT_TRUE(debug_json_string(totalItem, "value")[0] != '\0');
    TEST_ASSERT_TRUE(debug_json_string(indexItem, "value")[0] != '\0');
    cJSON_Delete(message);

    debug_client_send_request(&client, 9, "continue", ZR_NULL);
    message = debug_client_expect_response(&client, 9);
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

static void test_debug_agent_reports_uncaught_exception_from_runtime_without_cli_bridge(void) {
    const char *sourcePath = "debug_agent_uncaught_exception_fixture.zr";
    const char *source =
            "func explode(): int {\n"
            "    throw \"boom\";\n"
            "}\n"
            "return explode();";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugAgentConfig config;
    ZrDebugAgent *agent = ZR_NULL;
    SZrNetworkStream client;
    ZrDebugExecutionThread thread;
    TZrChar error[256];
    cJSON *message;
    cJSON *params;
    cJSON *frames;
    cJSON *topFrame;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_agent_source(state, sourcePath, source);
    TEST_ASSERT_NOT_NULL(function);

    memset(&config, 0, sizeof(config));
    config.address = "127.0.0.1:0";
    config.wait_for_client = ZR_TRUE;
    config.auth_token = "secret";
    config.stop_on_uncaught_exception = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrDebug_AgentStart(state,
                                        function,
                                        "tests.debug.uncaught_exception_runtime",
                                        &config,
                                        &agent,
                                        error,
                                        sizeof(error)));
    memset(&client, 0, sizeof(client));
    debug_client_connect(agent, &client);

    memset(&thread, 0, sizeof(thread));
    thread.state = state;
    thread.function = function;
    thread.agent = agent;
    thread.mode = ZR_DEBUG_EXECUTION_CAPTURE_FAILURE;
    thread.notifyExceptionOnFailure = ZR_TRUE;
    thread.notifyTerminated = ZR_TRUE;
    TEST_ASSERT_TRUE(debug_execution_thread_start(&thread));

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "authToken", "secret");
    debug_client_send_request(&client, 1, "initialize", params);
    message = debug_client_expect_response(&client, 1);
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "initialized");
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "moduleLoaded");
    TEST_ASSERT_EQUAL_STRING("tests.debug.uncaught_exception_runtime",
                             debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "moduleName"));
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "stopped");
    TEST_ASSERT_EQUAL_STRING("entry", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "reason"));
    cJSON_Delete(message);

    debug_client_send_request(&client, 2, "continue", ZR_NULL);
    message = debug_client_expect_response(&client, 2);
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "continued");
    cJSON_Delete(message);

    message = debug_client_expect_event(&client, "stopped");
    TEST_ASSERT_EQUAL_STRING("exception", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "reason"));
    TEST_ASSERT_EQUAL_STRING(sourcePath, debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "sourceFile"));
    TEST_ASSERT_EQUAL_STRING("explode", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "functionName"));
    TEST_ASSERT_EQUAL_INT(2, debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "line"));
    TEST_ASSERT_TRUE(debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "instructionIndex") >= 0);
    cJSON_Delete(message);

    debug_client_send_request(&client, 3, "stackTrace", ZR_NULL);
    message = debug_client_expect_response(&client, 3);
    frames = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "frames");
    TEST_ASSERT_TRUE(cJSON_IsArray(frames));
    TEST_ASSERT_TRUE(cJSON_GetArraySize(frames) >= 1);
    topFrame = cJSON_GetArrayItem(frames, 0);
    TEST_ASSERT_EQUAL_STRING("explode", debug_json_string(topFrame, "functionName"));
    TEST_ASSERT_EQUAL_STRING(sourcePath, debug_json_string(topFrame, "sourceFile"));
    TEST_ASSERT_EQUAL_INT(2, debug_json_int(topFrame, "line"));
    cJSON_Delete(message);

    debug_client_send_request(&client, 4, "continue", ZR_NULL);
    message = debug_client_expect_response(&client, 4);
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "continued");
    cJSON_Delete(message);

    debug_execution_thread_join(&thread);
    TEST_ASSERT_FALSE(thread.success);
    TEST_ASSERT_TRUE(state->hasCurrentException);

    message = debug_client_expect_event(&client, "terminated");
    TEST_ASSERT_FALSE(debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "success") != 0);
    cJSON_Delete(message);

    ZrCore_State_ResetThread(state, state->currentExceptionStatus);
    ZrNetwork_StreamClose(&client);
    ZrDebug_AgentStop(agent);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_agent_hits_source_breakpoints_on_binary_loaded_functions(void) {
    const char *sourcePath = "debug_agent_binary_fixture.zr";
    const char *binaryPath = "debug_agent_binary_fixture.zro";
    const char *moduleName = "tests.debug.binary_roundtrip";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *sourceFunction;
    SZrFunction *runtimeFunction;
    ZrDebugAgentConfig config;
    ZrDebugAgent *agent = ZR_NULL;
    SZrNetworkStream client;
    ZrDebugExecutionThread thread;
    TZrChar error[256];
    cJSON *message;
    cJSON *params;

    TEST_ASSERT_NOT_NULL(state);
    sourceFunction = compile_debug_agent_fixture(state, sourcePath);
    TEST_ASSERT_NOT_NULL(sourceFunction);
    runtimeFunction = load_binary_debug_agent_fixture(state,
                                                      sourceFunction,
                                                      binaryPath,
                                                      moduleName,
                                                      "debug-agent-binary-hash");
    TEST_ASSERT_NOT_NULL(runtimeFunction);

    memset(&config, 0, sizeof(config));
    config.address = "127.0.0.1:0";
    config.suspend_on_start = ZR_TRUE;
    config.auth_token = "secret";
    config.stop_on_uncaught_exception = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrDebug_AgentStart(state, runtimeFunction, moduleName, &config, &agent, error, sizeof(error)));
    memset(&client, 0, sizeof(client));
    debug_client_connect(agent, &client);

    memset(&thread, 0, sizeof(thread));
    thread.state = state;
    thread.function = runtimeFunction;
    TEST_ASSERT_TRUE(debug_execution_thread_start(&thread));

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "authToken", "secret");
    debug_client_send_request(&client, 1, "initialize", params);
    message = debug_client_expect_response(&client, 1);
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "initialized");
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "moduleLoaded");
    TEST_ASSERT_EQUAL_STRING(moduleName,
                             debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "moduleName"));
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "stopped");
    TEST_ASSERT_EQUAL_STRING("entry", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "reason"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "moduleName", moduleName);
    cJSON_AddStringToObject(params, "sourceFile", sourcePath);
    cJSON_AddItemToObject(params, "lines", cJSON_CreateIntArray((const int[]){2}, 1));
    debug_client_send_request(&client, 2, "setBreakpoints", params);
    message = debug_client_expect_event(&client, "breakpointResolved");
    TEST_ASSERT_TRUE(debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "resolved") != 0);
    TEST_ASSERT_EQUAL_INT(2, debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "line"));
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
    TEST_ASSERT_EQUAL_STRING("addOne", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "functionName"));
    TEST_ASSERT_EQUAL_INT(2, debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "line"));
    cJSON_Delete(message);

    debug_client_send_request(&client, 4, "continue", ZR_NULL);
    message = debug_client_expect_response(&client, 4);
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "continued");
    cJSON_Delete(message);

    debug_execution_thread_join(&thread);
    TEST_ASSERT_TRUE(thread.success);
    TEST_ASSERT_EQUAL_INT64(7, thread.result);
    ZrDebug_NotifyTerminated(agent, thread.success);

    message = debug_client_expect_event(&client, "terminated");
    TEST_ASSERT_TRUE(debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "success") != 0);
    cJSON_Delete(message);

    ZrNetwork_StreamClose(&client);
    ZrDebug_AgentStop(agent);
    remove(binaryPath);
    ZrCore_Function_Free(state, sourceFunction);
    ZrCore_Function_Free(state, runtimeFunction);
    ZrTests_Runtime_State_Destroy(state);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_debug_agent_rejects_invalid_token_and_continues_execution);
    RUN_TEST(test_debug_agent_wait_for_client_accepts_initialize_without_auth_token);
    RUN_TEST(test_debug_agent_wait_for_client_stays_paused_after_invalid_auth);
    RUN_TEST(test_debug_agent_breakpoint_step_and_variable_snapshots_over_tcp);
    RUN_TEST(test_debug_agent_step_in_and_out_cross_call_boundaries);
    RUN_TEST(test_debug_agent_pause_stops_at_next_safepoint_and_preserves_stack);
    RUN_TEST(test_debug_agent_reports_uncaught_exception_from_runtime_without_cli_bridge);
    RUN_TEST(test_debug_agent_hits_source_breakpoints_on_binary_loaded_functions);
    return UNITY_END();
}
