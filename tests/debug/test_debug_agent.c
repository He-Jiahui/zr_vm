#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON/cJSON.h"
#include "path_support.h"
#include "unity.h"
#include "runtime_support.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/string.h"
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
    ZR_DEBUG_EXECUTION_CAPTURE_FAILURE = 2,
    ZR_DEBUG_EXECUTION_CAPTURE_VALUE = 3
} EZrDebugExecutionMode;

typedef struct ZrDebugExecutionThread {
    SZrState *state;
    SZrFunction *function;
    ZrDebugAgent *agent;
    TZrInt64 result;
    SZrTypeValue resultValue;
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

typedef struct ZrDebugProjectFileReader {
    TZrByte *bytes;
    TZrSize length;
    TZrBool consumed;
} ZrDebugProjectFileReader;

static const char *gDebugProjectName = ZR_NULL;

TZrBool ZrVmLibNetwork_Register(SZrGlobalState *global);
TZrBool ZrVmLibSystem_Register(SZrGlobalState *global);
TZrBool ZrVmLibContainer_Register(SZrGlobalState *global);

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

static SZrFunction *compile_debug_agent_project_entry(SZrState *state,
                                                      const char *projectName,
                                                      const char *relativePath,
                                                      TZrChar *outSourcePath,
                                                      TZrSize outSourcePathCapacity) {
    TZrSize sourceLength = 0;
    TZrChar *source = ZR_NULL;
    SZrString *sourceName = ZR_NULL;
    SZrFunction *function = ZR_NULL;

    if (state == ZR_NULL || projectName == ZR_NULL || relativePath == ZR_NULL || outSourcePath == ZR_NULL ||
        outSourcePathCapacity == 0) {
        return ZR_NULL;
    }

    if (!ZrTests_Path_GetProjectFile(projectName, relativePath, outSourcePath, outSourcePathCapacity)) {
        return ZR_NULL;
    }

    source = ZrTests_ReadTextFile(outSourcePath, &sourceLength);
    if (source == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state, outSourcePath, strlen(outSourcePath));
    if (sourceName != ZR_NULL) {
        function = ZrParser_Source_Compile(state, source, sourceLength, sourceName);
    }

    free(source);
    return function;
}

static TZrBytePtr debug_project_file_reader_read(SZrState *state, TZrPtr customData, TZrSize *size) {
    ZrDebugProjectFileReader *reader = (ZrDebugProjectFileReader *)customData;

    ZR_UNUSED_PARAMETER(state);

    if (reader == ZR_NULL || size == ZR_NULL || reader->consumed || reader->bytes == ZR_NULL) {
        if (size != ZR_NULL) {
            *size = 0;
        }
        return ZR_NULL;
    }

    reader->consumed = ZR_TRUE;
    *size = reader->length;
    return reader->bytes;
}

static void debug_project_file_reader_close(SZrState *state, TZrPtr customData) {
    ZrDebugProjectFileReader *reader = (ZrDebugProjectFileReader *)customData;

    ZR_UNUSED_PARAMETER(state);

    if (reader != ZR_NULL) {
        free(reader->bytes);
        free(reader);
    }
}

static TZrBool debug_project_source_loader(SZrState *state, TZrNativeString sourcePath, TZrNativeString md5, SZrIo *io) {
    TZrChar relativePath[ZR_TESTS_PATH_MAX];
    TZrChar fullPath[ZR_TESTS_PATH_MAX];
    TZrBytePtr bytes = ZR_NULL;
    TZrSize length = 0;
    ZrDebugProjectFileReader *reader;

    ZR_UNUSED_PARAMETER(md5);

    if (state == ZR_NULL || sourcePath == ZR_NULL || io == ZR_NULL || gDebugProjectName == ZR_NULL) {
        return ZR_FALSE;
    }

    snprintf(relativePath, sizeof(relativePath), "src/%s.zr", sourcePath);
    if (!ZrTests_Path_GetProjectFile(gDebugProjectName, relativePath, fullPath, sizeof(fullPath))) {
        return ZR_FALSE;
    }

    if (!ZrTests_ReadFileBytes(fullPath, &bytes, &length) || bytes == ZR_NULL || length == 0) {
        free(bytes);
        return ZR_FALSE;
    }

    reader = (ZrDebugProjectFileReader *)malloc(sizeof(*reader));
    if (reader == ZR_NULL) {
        free(bytes);
        return ZR_FALSE;
    }

    reader->bytes = bytes;
    reader->length = length;
    reader->consumed = ZR_FALSE;
    ZrCore_Io_Init(state, io, debug_project_file_reader_read, debug_project_file_reader_close, reader);
    io->isBinary = ZR_FALSE;
    return ZR_TRUE;
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

static SZrFunction *compile_debug_agent_entry_locals_fixture(SZrState *state, const char *sourceLabel) {
    const char *source =
            "var first = 1;\n"
            "var second = first + 2;\n"
            "return second;\n";

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
    } else if (thread->mode == ZR_DEBUG_EXECUTION_CAPTURE_VALUE) {
        thread->success = ZrTests_Runtime_Function_Execute(thread->state, thread->function, &thread->resultValue);
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
    ZrCore_Value_ResetAsNull(&thread->resultValue);
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

static void debug_assert_stopped_location(cJSON *message,
                                          const char *reason,
                                          const char *sourceFile,
                                          const char *functionName,
                                          int minLine,
                                          int maxLine,
                                          int *outLine) {
    cJSON *params = cJSON_GetObjectItemCaseSensitive(message, "params");
    int line;
    char locationMessage[128];

    TEST_ASSERT_NOT_NULL(params);
    TEST_ASSERT_EQUAL_STRING(reason, debug_json_string(params, "reason"));
    TEST_ASSERT_EQUAL_STRING(sourceFile, debug_json_string(params, "sourceFile"));
    TEST_ASSERT_EQUAL_STRING(functionName, debug_json_string(params, "functionName"));

    line = debug_json_int(params, "line");
    snprintf(locationMessage,
             sizeof(locationMessage),
             "expected stopped line in [%d, %d], actual=%d",
             minLine,
             maxLine,
             line);
    TEST_ASSERT_TRUE_MESSAGE(line >= minLine, locationMessage);
    TEST_ASSERT_TRUE_MESSAGE(line <= maxLine, locationMessage);

    if (outLine != ZR_NULL) {
        *outLine = line;
    }
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

static void test_debug_agent_exposes_entry_script_locals_with_initializers(void) {
    const char *sourcePath = "debug_agent_entry_locals_fixture.zr";
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
    cJSON *firstItem;
    cJSON *secondItem;
    int localsScopeId;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_agent_entry_locals_fixture(state, sourcePath);
    TEST_ASSERT_NOT_NULL(function);

    memset(&config, 0, sizeof(config));
    config.address = "127.0.0.1:0";
    config.suspend_on_start = ZR_TRUE;
    config.auth_token = "secret";
    config.stop_on_uncaught_exception = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrDebug_AgentStart(state, function, "tests.debug.entry_locals", &config, &agent, error, sizeof(error)));
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
    TEST_ASSERT_EQUAL_STRING("tests.debug.entry_locals",
                             debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "moduleName"));
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "stopped");
    TEST_ASSERT_EQUAL_STRING("entry", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "reason"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "moduleName", "tests.debug.entry_locals");
    cJSON_AddStringToObject(params, "sourceFile", sourcePath);
    cJSON_AddItemToObject(params, "lines", cJSON_CreateIntArray((const int[]){3}, 1));
    debug_client_send_request(&client, 2, "setBreakpoints", params);

    message = debug_client_expect_event(&client, "breakpointResolved");
    TEST_ASSERT_TRUE(debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "resolved") != 0);
    TEST_ASSERT_EQUAL_INT(3, debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "line"));
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
    TEST_ASSERT_EQUAL_INT(3, debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "line"));
    cJSON_Delete(message);

    debug_client_send_request(&client, 4, "stackTrace", ZR_NULL);
    message = debug_client_expect_response(&client, 4);
    frames = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "frames");
    TEST_ASSERT_TRUE(cJSON_IsArray(frames));
    TEST_ASSERT_TRUE(cJSON_GetArraySize(frames) >= 1);
    topFrame = cJSON_GetArrayItem(frames, 0);
    TEST_ASSERT_EQUAL_STRING(sourcePath, debug_json_string(topFrame, "sourceFile"));
    TEST_ASSERT_EQUAL_INT(3, debug_json_int(topFrame, "line"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "frameId", 1);
    debug_client_send_request(&client, 5, "scopes", params);
    message = debug_client_expect_response(&client, 5);
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
    debug_client_send_request(&client, 6, "variables", params);
    message = debug_client_expect_response(&client, 6);
    values = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "variables");
    TEST_ASSERT_TRUE(cJSON_IsArray(values));
    firstItem = debug_find_named_object(values, "first");
    secondItem = debug_find_named_object(values, "second");
    TEST_ASSERT_NOT_NULL(firstItem);
    TEST_ASSERT_NOT_NULL(secondItem);
    TEST_ASSERT_EQUAL_STRING("1", debug_json_string(firstItem, "value"));
    TEST_ASSERT_EQUAL_STRING("3", debug_json_string(secondItem, "value"));
    cJSON_Delete(message);

    debug_client_send_request(&client, 7, "continue", ZR_NULL);
    message = debug_client_expect_response(&client, 7);
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "continued");
    cJSON_Delete(message);

    debug_execution_thread_join(&thread);
    TEST_ASSERT_TRUE(thread.success);
    TEST_ASSERT_EQUAL_INT64(3, thread.result);
    ZrDebug_NotifyTerminated(agent, thread.success);

    message = debug_client_expect_event(&client, "terminated");
    TEST_ASSERT_TRUE(debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "success") != 0);
    cJSON_Delete(message);

    ZrNetwork_StreamClose(&client);
    ZrDebug_AgentStop(agent);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_agent_expands_object_members_and_runtime_globals(void) {
    const char *sourcePath = "debug_agent_object_globals_fixture.zr";
    const char *source =
            "var system = %import(\"zr.system\");\n"
            "func makeProfile() {\n"
            "    var profile = { name: \"alice\", score: 7, nested: { enabled: true } };\n"
            "    return profile;\n"
            "}\n"
            "var user = makeProfile();\n"
            "return 1;";
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
    cJSON *values;
    cJSON *localsScope;
    cJSON *globalsScope;
    cJSON *profileItem;
    cJSON *nestedItem;
    cJSON *globalsZrStateItem;
    cJSON *globalsLoadedModulesItem;
    int localsScopeId;
    int globalsScopeId;
    int profileHandle;
    int nestedHandle;
    int zrStateHandle;
    int loadedModulesHandle;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrVmLibSystem_Register(state->global));
    function = compile_debug_agent_source(state, sourcePath, source);
    TEST_ASSERT_NOT_NULL(function);

    memset(&config, 0, sizeof(config));
    config.address = "127.0.0.1:0";
    config.suspend_on_start = ZR_TRUE;
    config.auth_token = "secret";
    config.stop_on_uncaught_exception = ZR_TRUE;

    TEST_ASSERT_TRUE(
            ZrDebug_AgentStart(state, function, "tests.debug.object_globals", &config, &agent, error, sizeof(error)));
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
    TEST_ASSERT_EQUAL_STRING("tests.debug.object_globals",
                             debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "moduleName"));
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "stopped");
    TEST_ASSERT_EQUAL_STRING("entry", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "reason"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "moduleName", "tests.debug.object_globals");
    cJSON_AddStringToObject(params, "sourceFile", sourcePath);
    cJSON_AddItemToObject(params, "lines", cJSON_CreateIntArray((const int[]){4}, 1));
    debug_client_send_request(&client, 2, "setBreakpoints", params);

    message = debug_client_expect_event(&client, "breakpointResolved");
    TEST_ASSERT_TRUE(debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "resolved") != 0);
    TEST_ASSERT_EQUAL_INT(4, debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "line"));
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
    TEST_ASSERT_EQUAL_STRING("makeProfile", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "functionName"));
    TEST_ASSERT_EQUAL_INT(4, debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "line"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "frameId", 1);
    debug_client_send_request(&client, 4, "scopes", params);
    message = debug_client_expect_response(&client, 4);
    scopes = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "scopes");
    TEST_ASSERT_TRUE(cJSON_IsArray(scopes));
    localsScope = debug_find_named_object(scopes, "Locals");
    globalsScope = debug_find_named_object(scopes, "Globals");
    TEST_ASSERT_NOT_NULL(localsScope);
    TEST_ASSERT_NOT_NULL(globalsScope);
    localsScopeId = debug_json_int(localsScope, "scopeId");
    globalsScopeId = debug_json_int(globalsScope, "scopeId");
    TEST_ASSERT_TRUE(localsScopeId > 0);
    TEST_ASSERT_TRUE(globalsScopeId > 0);
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "scopeId", localsScopeId);
    debug_client_send_request(&client, 5, "variables", params);
    message = debug_client_expect_response(&client, 5);
    values = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "variables");
    TEST_ASSERT_TRUE(cJSON_IsArray(values));
    profileItem = debug_find_named_object(values, "profile");
    TEST_ASSERT_NOT_NULL(profileItem);
    profileHandle = debug_json_int(profileItem, "variablesReference");
    TEST_ASSERT_TRUE(profileHandle > 0);
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "scopeId", profileHandle);
    debug_client_send_request(&client, 6, "variables", params);
    message = debug_client_expect_response(&client, 6);
    values = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "variables");
    TEST_ASSERT_TRUE(cJSON_IsArray(values));
    TEST_ASSERT_NOT_NULL(debug_find_named_object(values, "name"));
    TEST_ASSERT_EQUAL_STRING("alice", debug_json_string(debug_find_named_object(values, "name"), "value"));
    TEST_ASSERT_NOT_NULL(debug_find_named_object(values, "score"));
    TEST_ASSERT_EQUAL_STRING("7", debug_json_string(debug_find_named_object(values, "score"), "value"));
    nestedItem = debug_find_named_object(values, "nested");
    TEST_ASSERT_NOT_NULL(nestedItem);
    nestedHandle = debug_json_int(nestedItem, "variablesReference");
    TEST_ASSERT_TRUE(nestedHandle > 0);
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "scopeId", nestedHandle);
    debug_client_send_request(&client, 7, "variables", params);
    message = debug_client_expect_response(&client, 7);
    values = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "variables");
    TEST_ASSERT_TRUE(cJSON_IsArray(values));
    TEST_ASSERT_NOT_NULL(debug_find_named_object(values, "enabled"));
    TEST_ASSERT_EQUAL_STRING("true", debug_json_string(debug_find_named_object(values, "enabled"), "value"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "scopeId", globalsScopeId);
    debug_client_send_request(&client, 8, "variables", params);
    message = debug_client_expect_response(&client, 8);
    values = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "variables");
    TEST_ASSERT_TRUE(cJSON_IsArray(values));
    globalsZrStateItem = debug_find_named_object(values, "zrState");
    globalsLoadedModulesItem = debug_find_named_object(values, "loadedModules");
    TEST_ASSERT_NOT_NULL(globalsZrStateItem);
    TEST_ASSERT_NOT_NULL(globalsLoadedModulesItem);
    zrStateHandle = debug_json_int(globalsZrStateItem, "variablesReference");
    loadedModulesHandle = debug_json_int(globalsLoadedModulesItem, "variablesReference");
    TEST_ASSERT_TRUE(zrStateHandle > 0);
    TEST_ASSERT_TRUE(loadedModulesHandle > 0);
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "scopeId", zrStateHandle);
    debug_client_send_request(&client, 9, "variables", params);
    message = debug_client_expect_response(&client, 9);
    values = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "variables");
    TEST_ASSERT_TRUE(cJSON_IsArray(values));
    TEST_ASSERT_NOT_NULL(debug_find_named_object(values, "loadedModuleCount"));
    TEST_ASSERT_NOT_NULL(debug_find_named_object(values, "frameDepth"));
    TEST_ASSERT_NOT_NULL(debug_find_named_object(values, "hasCurrentException"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "scopeId", loadedModulesHandle);
    debug_client_send_request(&client, 10, "variables", params);
    message = debug_client_expect_response(&client, 10);
    values = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "variables");
    TEST_ASSERT_TRUE(cJSON_IsArray(values));
    if (cJSON_GetArraySize(values) > 0) {
        TEST_ASSERT_TRUE(debug_json_string(cJSON_GetArrayItem(values, 0), "name")[0] != '\0');
    }
    cJSON_Delete(message);

    debug_client_send_request(&client, 11, "continue", ZR_NULL);
    message = debug_client_expect_response(&client, 11);
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "continued");
    cJSON_Delete(message);

    debug_execution_thread_join(&thread);
    TEST_ASSERT_TRUE(thread.success);
    TEST_ASSERT_EQUAL_INT64(1, thread.result);
    ZrDebug_NotifyTerminated(agent, thread.success);

    message = debug_client_expect_event(&client, "terminated");
    TEST_ASSERT_TRUE(debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "success") != 0);
    cJSON_Delete(message);

    ZrNetwork_StreamClose(&client);
    ZrDebug_AgentStop(agent);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_agent_expands_native_network_methods_without_asserting(void) {
    const char *sourcePath = "debug_agent_native_network_methods_fixture.zr";
    const char *source =
            "var network = %import(\"zr.network\");\n"
            "var tcp = network.tcp;\n"
            "func openListener(): int {\n"
            "    var listener = tcp.listen(\"127.0.0.1\", 0);\n"
            "    var port = listener.port();\n"
            "    listener.close();\n"
            "    return port;\n"
            "}\n"
            "return openListener();";
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
    cJSON *values;
    cJSON *localsScope;
    cJSON *listenerItem;
    cJSON *prototypeItem;
    cJSON *methodsItem;
    cJSON *closeItem;
    cJSON *portItem;
    int localsScopeId;
    int listenerHandle;
    int prototypeHandle;
    int methodsHandle;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrVmLibNetwork_Register(state->global));
    function = compile_debug_agent_source(state, sourcePath, source);
    TEST_ASSERT_NOT_NULL(function);

    memset(&config, 0, sizeof(config));
    config.address = "127.0.0.1:0";
    config.suspend_on_start = ZR_TRUE;
    config.auth_token = "secret";
    config.stop_on_uncaught_exception = ZR_TRUE;

    TEST_ASSERT_TRUE(
            ZrDebug_AgentStart(state, function, "tests.debug.native_network_methods", &config, &agent, error, sizeof(error)));
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
    TEST_ASSERT_EQUAL_STRING("tests.debug.native_network_methods",
                             debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "moduleName"));
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "stopped");
    TEST_ASSERT_EQUAL_STRING("entry", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "reason"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "moduleName", "tests.debug.native_network_methods");
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
    TEST_ASSERT_EQUAL_STRING("openListener", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "functionName"));
    TEST_ASSERT_EQUAL_INT(5, debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "line"));
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
    listenerItem = debug_find_named_object(values, "listener");
    TEST_ASSERT_NOT_NULL(listenerItem);
    listenerHandle = debug_json_int(listenerItem, "variablesReference");
    TEST_ASSERT_TRUE(listenerHandle > 0);
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "scopeId", listenerHandle);
    debug_client_send_request(&client, 6, "variables", params);
    message = debug_client_expect_response(&client, 6);
    values = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "variables");
    TEST_ASSERT_TRUE(cJSON_IsArray(values));
    prototypeItem = debug_find_named_object(values, "$prototype");
    TEST_ASSERT_NOT_NULL(prototypeItem);
    TEST_ASSERT_NULL(debug_find_named_object(values, "$methods"));
    prototypeHandle = debug_json_int(prototypeItem, "variablesReference");
    TEST_ASSERT_TRUE(prototypeHandle > 0);
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "scopeId", prototypeHandle);
    debug_client_send_request(&client, 7, "variables", params);
    message = debug_client_expect_response(&client, 7);
    values = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "variables");
    TEST_ASSERT_TRUE(cJSON_IsArray(values));
    methodsItem = debug_find_named_object(values, "$methods");
    TEST_ASSERT_NOT_NULL(methodsItem);
    methodsHandle = debug_json_int(methodsItem, "variablesReference");
    TEST_ASSERT_TRUE(methodsHandle > 0);
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "scopeId", methodsHandle);
    debug_client_send_request(&client, 8, "variables", params);
    message = debug_client_expect_response(&client, 8);
    values = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "variables");
    TEST_ASSERT_TRUE(cJSON_IsArray(values));
    closeItem = debug_find_named_object(values, "close");
    portItem = debug_find_named_object(values, "port");
    TEST_ASSERT_NOT_NULL(closeItem);
    TEST_ASSERT_NOT_NULL(portItem);
    TEST_ASSERT_TRUE(debug_json_string(closeItem, "value")[0] != '\0');
    TEST_ASSERT_TRUE(debug_json_string(portItem, "value")[0] != '\0');
    cJSON_Delete(message);

    debug_client_send_request(&client, 9, "continue", ZR_NULL);
    message = debug_client_expect_response(&client, 9);
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "continued");
    cJSON_Delete(message);

    debug_execution_thread_join(&thread);
    TEST_ASSERT_TRUE(thread.success);
    TEST_ASSERT_TRUE(thread.result > 0);
    ZrDebug_NotifyTerminated(agent, thread.success);

    message = debug_client_expect_event(&client, "terminated");
    TEST_ASSERT_TRUE(debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "success") != 0);
    cJSON_Delete(message);

    ZrNetwork_StreamClose(&client);
    ZrDebug_AgentStop(agent);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_agent_reports_richer_stack_scopes_and_safe_evaluate(void) {
    const char *sourcePath = "debug_agent_rich_inspection_fixture.zr";
    const char *source =
            "class BossHero {\n"
            "    pub static var created: int = 0;\n"
            "    pri var _hp: int = 0;\n"
            "\n"
            "    pub @constructor(seed: int) {\n"
            "        this._hp = seed;\n"
            "        BossHero.created = BossHero.created + 1;\n"
            "    }\n"
            "\n"
            "    pub total(delta: int): int {\n"
            "        var profile = { name: \"alice\", nested: { enabled: true } };\n"
            "        var total = this._hp + delta;\n"
            "        return total + BossHero.created;\n"
            "    }\n"
            "}\n"
            "var boss = new BossHero(30);\n"
            "return boss.total(7);";
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
    cJSON *receiver;
    cJSON *arguments;
    cJSON *firstArgument;
    cJSON *scopes;
    cJSON *argumentsScope;
    cJSON *localsScope;
    cJSON *prototypeScope;
    cJSON *staticsScope;
    cJSON *values;
    cJSON *profileItem;
    cJSON *createdItem;
    cJSON *prototypeItem;
    int argumentsScopeId;
    int localsScopeId;
    int prototypeScopeId;
    int staticsScopeId;
    int profileHandle;
    int evaluateHandle;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrVmLibContainer_Register(state->global));
    function = compile_debug_agent_source(state, sourcePath, source);
    TEST_ASSERT_NOT_NULL(function);

    memset(&config, 0, sizeof(config));
    config.address = "127.0.0.1:0";
    config.suspend_on_start = ZR_TRUE;
    config.auth_token = "secret";
    config.stop_on_uncaught_exception = ZR_TRUE;

    TEST_ASSERT_TRUE(
            ZrDebug_AgentStart(state, function, "tests.debug.rich_inspection", &config, &agent, error, sizeof(error)));
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
    TEST_ASSERT_EQUAL_STRING("tests.debug.rich_inspection",
                             debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "moduleName"));
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "stopped");
    TEST_ASSERT_EQUAL_STRING("entry", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "reason"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "moduleName", "tests.debug.rich_inspection");
    cJSON_AddStringToObject(params, "sourceFile", sourcePath);
    {
        cJSON *breakpoints = cJSON_CreateArray();
        cJSON *breakpoint = cJSON_CreateObject();
        TEST_ASSERT_NOT_NULL(breakpoints);
        TEST_ASSERT_NOT_NULL(breakpoint);
        cJSON_AddNumberToObject(breakpoint, "line", 13);
        cJSON_AddItemToArray(breakpoints, breakpoint);
        cJSON_AddItemToObject(params, "breakpoints", breakpoints);
    }
    debug_client_send_request(&client, 2, "setBreakpoints", params);
    message = debug_client_expect_event(&client, "breakpointResolved");
    TEST_ASSERT_TRUE(debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "resolved") != 0);
    TEST_ASSERT_EQUAL_INT(13, debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "line"));
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
    TEST_ASSERT_EQUAL_STRING("total", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "functionName"));
    TEST_ASSERT_EQUAL_INT(13, debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "line"));
    cJSON_Delete(message);

    debug_client_send_request(&client, 4, "stackTrace", ZR_NULL);
    message = debug_client_expect_response(&client, 4);
    frames = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "frames");
    TEST_ASSERT_TRUE(cJSON_IsArray(frames));
    TEST_ASSERT_TRUE(cJSON_GetArraySize(frames) >= 2);
    topFrame = cJSON_GetArrayItem(frames, 0);
    TEST_ASSERT_EQUAL_STRING("tests.debug.rich_inspection", debug_json_string(topFrame, "moduleName"));
    TEST_ASSERT_EQUAL_STRING("total", debug_json_string(topFrame, "functionName"));
    TEST_ASSERT_EQUAL_INT(0, debug_json_int(topFrame, "frameDepth"));
    TEST_ASSERT_EQUAL_STRING("method", debug_json_string(topFrame, "callKind"));
    TEST_ASSERT_TRUE(debug_json_int(topFrame, "returnSlot") >= 0);
    TEST_ASSERT_FALSE(debug_json_int(topFrame, "isExceptionFrame") != 0);
    receiver = cJSON_GetObjectItemCaseSensitive(topFrame, "receiver");
    TEST_ASSERT_TRUE(cJSON_IsObject(receiver));
    TEST_ASSERT_EQUAL_STRING("this", debug_json_string(receiver, "name"));
    TEST_ASSERT_TRUE(debug_json_int(receiver, "variablesReference") > 0);
    arguments = cJSON_GetObjectItemCaseSensitive(topFrame, "arguments");
    TEST_ASSERT_TRUE(cJSON_IsArray(arguments));
    TEST_ASSERT_EQUAL_INT(1, cJSON_GetArraySize(arguments));
    firstArgument = cJSON_GetArrayItem(arguments, 0);
    TEST_ASSERT_EQUAL_STRING("delta", debug_json_string(firstArgument, "name"));
    TEST_ASSERT_EQUAL_STRING("7", debug_json_string(firstArgument, "value"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "frameId", 1);
    debug_client_send_request(&client, 5, "scopes", params);
    message = debug_client_expect_response(&client, 5);
    scopes = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "scopes");
    TEST_ASSERT_TRUE(cJSON_IsArray(scopes));
    argumentsScope = debug_find_named_object(scopes, "Arguments");
    localsScope = debug_find_named_object(scopes, "Locals");
    prototypeScope = debug_find_named_object(scopes, "Prototype");
    staticsScope = debug_find_named_object(scopes, "Statics");
    TEST_ASSERT_NOT_NULL(argumentsScope);
    TEST_ASSERT_NOT_NULL(localsScope);
    TEST_ASSERT_NOT_NULL(prototypeScope);
    TEST_ASSERT_NOT_NULL(staticsScope);
    argumentsScopeId = debug_json_int(argumentsScope, "scopeId");
    localsScopeId = debug_json_int(localsScope, "scopeId");
    prototypeScopeId = debug_json_int(prototypeScope, "scopeId");
    staticsScopeId = debug_json_int(staticsScope, "scopeId");
    TEST_ASSERT_TRUE(argumentsScopeId > 0);
    TEST_ASSERT_TRUE(localsScopeId > 0);
    TEST_ASSERT_TRUE(prototypeScopeId > 0);
    TEST_ASSERT_TRUE(staticsScopeId > 0);
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "scopeId", argumentsScopeId);
    debug_client_send_request(&client, 6, "variables", params);
    message = debug_client_expect_response(&client, 6);
    values = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "variables");
    TEST_ASSERT_TRUE(cJSON_IsArray(values));
    TEST_ASSERT_EQUAL_STRING("7", debug_json_string(debug_find_named_object(values, "delta"), "value"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "scopeId", localsScopeId);
    debug_client_send_request(&client, 7, "variables", params);
    message = debug_client_expect_response(&client, 7);
    values = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "variables");
    TEST_ASSERT_TRUE(cJSON_IsArray(values));
    TEST_ASSERT_EQUAL_STRING("37", debug_json_string(debug_find_named_object(values, "total"), "value"));
    profileItem = debug_find_named_object(values, "profile");
    TEST_ASSERT_NOT_NULL(profileItem);
    profileHandle = debug_json_int(profileItem, "variablesReference");
    TEST_ASSERT_TRUE(profileHandle > 0);
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "scopeId", profileHandle);
    debug_client_send_request(&client, 8, "variables", params);
    message = debug_client_expect_response(&client, 8);
    values = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "variables");
    TEST_ASSERT_TRUE(cJSON_IsArray(values));
    TEST_ASSERT_NOT_NULL(debug_find_named_object(values, "name"));
    prototypeItem = debug_find_named_object(values, "$prototype");
    TEST_ASSERT_NOT_NULL(prototypeItem);
    TEST_ASSERT_TRUE(debug_json_int(prototypeItem, "variablesReference") > 0);
    TEST_ASSERT_NULL(debug_find_named_object(values, "$metadata"));
    TEST_ASSERT_NULL(debug_find_named_object(values, "$members"));
    TEST_ASSERT_NULL(debug_find_named_object(values, "$methods"));
    TEST_ASSERT_NULL(debug_find_named_object(values, "$properties"));
    TEST_ASSERT_NULL(debug_find_named_object(values, "$statics"));
    TEST_ASSERT_NULL(debug_find_named_object(values, "$protocols"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "scopeId", prototypeScopeId);
    debug_client_send_request(&client, 9, "variables", params);
    message = debug_client_expect_response(&client, 9);
    values = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "variables");
    TEST_ASSERT_TRUE(cJSON_IsArray(values));
    TEST_ASSERT_NOT_NULL(debug_find_named_object(values, "memberDescriptorCount"));
    TEST_ASSERT_NOT_NULL(debug_find_named_object(values, "managedFieldCount"));
    TEST_ASSERT_NOT_NULL(debug_find_named_object(values, "indexContract"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "scopeId", staticsScopeId);
    debug_client_send_request(&client, 10, "variables", params);
    message = debug_client_expect_response(&client, 10);
    values = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "variables");
    TEST_ASSERT_TRUE(cJSON_IsArray(values));
    createdItem = debug_find_named_object(values, "created");
    TEST_ASSERT_NOT_NULL(createdItem);
    TEST_ASSERT_EQUAL_STRING("1", debug_json_string(createdItem, "value"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "expression", "delta + 1");
    cJSON_AddNumberToObject(params, "frameId", 1);
    debug_client_send_request(&client, 11, "evaluate", params);
    message = debug_client_expect_response(&client, 11);
    TEST_ASSERT_EQUAL_STRING("8", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "result"), "value"));
    TEST_ASSERT_EQUAL_STRING("int", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "result"), "type"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "expression", "this._hp + delta");
    cJSON_AddNumberToObject(params, "frameId", 1);
    debug_client_send_request(&client, 12, "evaluate", params);
    message = debug_client_expect_response(&client, 12);
    TEST_ASSERT_EQUAL_STRING("37", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "result"), "value"));
    evaluateHandle = debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "result"), "variablesReference");
    TEST_ASSERT_TRUE(evaluateHandle == 0);
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "expression", "this.hp");
    cJSON_AddNumberToObject(params, "frameId", 1);
    debug_client_send_request(&client, 13, "evaluate", params);
    message = debug_client_expect_error_response(&client, 13, -32003);
    cJSON_Delete(message);

    debug_client_send_request(&client, 14, "continue", ZR_NULL);
    message = debug_client_expect_response(&client, 14);
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "continued");
    cJSON_Delete(message);

    debug_execution_thread_join(&thread);
    TEST_ASSERT_TRUE(thread.success);
    TEST_ASSERT_EQUAL_INT64(38, thread.result);
    ZrDebug_NotifyTerminated(agent, thread.success);

    message = debug_client_expect_event(&client, "terminated");
    TEST_ASSERT_TRUE(debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "success") != 0);
    cJSON_Delete(message);

    ZrNetwork_StreamClose(&client);
    ZrDebug_AgentStop(agent);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_agent_separates_instance_metadata_and_supports_index_windows(void) {
    const char *sourcePath = "debug_agent_index_window_fixture.zr";
    const char *source =
            "class Holder {\n"
            "    pub inspect(): int {\n"
            "        var fixed = [1, 2, 3, 4, 5, 6];\n"
            "        var snapshot = { label: \"demo\", count: 1 };\n"
            "        var holderType = Holder;\n"
            "        var total = 7;\n"
            "        return total;\n"
            "    }\n"
            "}\n"
            "var holder = new Holder();\n"
            "return holder.inspect();";
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
    cJSON *snapshotItem;
    cJSON *holderTypeItem;
    cJSON *fixedItem;
    cJSON *prototypeItem;
    cJSON *metadataItem;
    cJSON *methodsItem;
    int localsScopeId;
    int snapshotHandle;
    int holderTypeHandle;
    int fixedHandle;
    int evaluateHandle;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_agent_source(state, sourcePath, source);
    TEST_ASSERT_NOT_NULL(function);

    memset(&config, 0, sizeof(config));
    config.address = "127.0.0.1:0";
    config.suspend_on_start = ZR_TRUE;
    config.auth_token = "secret";
    config.stop_on_uncaught_exception = ZR_TRUE;

    TEST_ASSERT_TRUE(
            ZrDebug_AgentStart(state, function, "tests.debug.index_windows", &config, &agent, error, sizeof(error)));
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
    TEST_ASSERT_EQUAL_STRING("tests.debug.index_windows",
                             debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "moduleName"));
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "stopped");
    TEST_ASSERT_EQUAL_STRING("entry", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "reason"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "moduleName", "tests.debug.index_windows");
    cJSON_AddStringToObject(params, "sourceFile", sourcePath);
    cJSON_AddItemToObject(params, "lines", cJSON_CreateIntArray((const int[]){6}, 1));
    debug_client_send_request(&client, 2, "setBreakpoints", params);
    message = debug_client_expect_event(&client, "breakpointResolved");
    TEST_ASSERT_TRUE(debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "resolved") != 0);
    TEST_ASSERT_EQUAL_INT(6, debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "line"));
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
    TEST_ASSERT_EQUAL_STRING("inspect", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "functionName"));
    TEST_ASSERT_EQUAL_INT(6, debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "line"));
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
    snapshotItem = debug_find_named_object(values, "snapshot");
    holderTypeItem = debug_find_named_object(values, "holderType");
    fixedItem = debug_find_named_object(values, "fixed");
    TEST_ASSERT_NOT_NULL(snapshotItem);
    TEST_ASSERT_NOT_NULL(holderTypeItem);
    TEST_ASSERT_NOT_NULL(fixedItem);
    snapshotHandle = debug_json_int(snapshotItem, "variablesReference");
    holderTypeHandle = debug_json_int(holderTypeItem, "variablesReference");
    fixedHandle = debug_json_int(fixedItem, "variablesReference");
    TEST_ASSERT_TRUE(snapshotHandle > 0);
    TEST_ASSERT_TRUE(holderTypeHandle > 0);
    TEST_ASSERT_TRUE(fixedHandle > 0);
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "scopeId", snapshotHandle);
    debug_client_send_request(&client, 6, "variables", params);
    message = debug_client_expect_response(&client, 6);
    values = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "variables");
    TEST_ASSERT_TRUE(cJSON_IsArray(values));
    TEST_ASSERT_NOT_NULL(debug_find_named_object(values, "label"));
    TEST_ASSERT_NOT_NULL(debug_find_named_object(values, "count"));
    prototypeItem = debug_find_named_object(values, "$prototype");
    TEST_ASSERT_NOT_NULL(prototypeItem);
    TEST_ASSERT_NULL(debug_find_named_object(values, "$metadata"));
    TEST_ASSERT_NULL(debug_find_named_object(values, "$members"));
    TEST_ASSERT_NULL(debug_find_named_object(values, "$methods"));
    TEST_ASSERT_NULL(debug_find_named_object(values, "$properties"));
    TEST_ASSERT_NULL(debug_find_named_object(values, "$statics"));
    TEST_ASSERT_NULL(debug_find_named_object(values, "$protocols"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "scopeId", holderTypeHandle);
    debug_client_send_request(&client, 7, "variables", params);
    message = debug_client_expect_response(&client, 7);
    values = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "variables");
    TEST_ASSERT_TRUE(cJSON_IsArray(values));
    metadataItem = debug_find_named_object(values, "$metadata");
    methodsItem = debug_find_named_object(values, "$methods");
    TEST_ASSERT_NOT_NULL(metadataItem);
    TEST_ASSERT_NOT_NULL(methodsItem);
    TEST_ASSERT_TRUE(debug_json_int(metadataItem, "variablesReference") > 0);
    TEST_ASSERT_TRUE(debug_json_int(methodsItem, "variablesReference") > 0);
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "scopeId", fixedHandle);
    cJSON_AddNumberToObject(params, "start", 1);
    cJSON_AddNumberToObject(params, "count", 2);
    debug_client_send_request(&client, 8, "variables", params);
    message = debug_client_expect_response(&client, 8);
    TEST_ASSERT_EQUAL_INT(6, debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "result"), "indexedVariables"));
    values = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "variables");
    TEST_ASSERT_TRUE(cJSON_IsArray(values));
    TEST_ASSERT_EQUAL_INT(2, cJSON_GetArraySize(values));
    TEST_ASSERT_EQUAL_STRING("2", debug_json_string(cJSON_GetArrayItem(values, 0), "value"));
    TEST_ASSERT_EQUAL_STRING("3", debug_json_string(cJSON_GetArrayItem(values, 1), "value"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "expression", "fixed[1..3]");
    cJSON_AddNumberToObject(params, "frameId", 1);
    debug_client_send_request(&client, 9, "evaluate", params);
    message = debug_client_expect_response(&client, 9);
    evaluateHandle = debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "result"), "variablesReference");
    TEST_ASSERT_TRUE(evaluateHandle > 0);
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "scopeId", evaluateHandle);
    debug_client_send_request(&client, 10, "variables", params);
    message = debug_client_expect_response(&client, 10);
    values = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "variables");
    TEST_ASSERT_TRUE(cJSON_IsArray(values));
    TEST_ASSERT_EQUAL_INT(2, cJSON_GetArraySize(values));
    TEST_ASSERT_EQUAL_STRING("2", debug_json_string(cJSON_GetArrayItem(values, 0), "value"));
    TEST_ASSERT_EQUAL_STRING("3", debug_json_string(cJSON_GetArrayItem(values, 1), "value"));
    cJSON_Delete(message);

    debug_client_send_request(&client, 11, "continue", ZR_NULL);
    message = debug_client_expect_response(&client, 11);
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "continued");
    cJSON_Delete(message);

    debug_execution_thread_join(&thread);
    TEST_ASSERT_TRUE(thread.success);
    TEST_ASSERT_EQUAL_INT64(7, thread.result);
    ZrDebug_NotifyTerminated(agent, thread.success);
    message = debug_client_expect_event(&client, "terminated");
    cJSON_Delete(message);

    ZrNetwork_StreamClose(&client);
    ZrDebug_AgentStop(agent);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_agent_supports_function_hit_condition_and_log_breakpoints(void) {
    const char *sourcePath = "debug_agent_control_fixture.zr";
    const char *source =
            "func addOne(value: int): int {\n"
            "    return value + 1;\n"
            "}\n"
            "func run(): int {\n"
            "    var total = 0;\n"
            "    var index = 0;\n"
            "    while (index < 4) {\n"
            "        total = total + addOne(index);\n"
            "        index = index + 1;\n"
            "    }\n"
            "    return total;\n"
            "}\n"
            "return run();";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugAgentConfig config;
    ZrDebugAgent *agent = ZR_NULL;
    SZrNetworkStream client;
    ZrDebugExecutionThread thread;
    TZrChar error[256];
    cJSON *message;
    cJSON *params;
    cJSON *breakpoints;
    cJSON *breakpoint;
    cJSON *functionBreakpoints;
    cJSON *functionBreakpoint;
    cJSON *outputParams;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_agent_source(state, sourcePath, source);
    TEST_ASSERT_NOT_NULL(function);

    memset(&config, 0, sizeof(config));
    config.address = "127.0.0.1:0";
    config.suspend_on_start = ZR_TRUE;
    config.auth_token = "secret";
    config.stop_on_uncaught_exception = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrDebug_AgentStart(state, function, "tests.debug.control", &config, &agent, error, sizeof(error)));
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
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "stopped");
    TEST_ASSERT_EQUAL_STRING("entry", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "reason"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "moduleName", "tests.debug.control");
    cJSON_AddStringToObject(params, "sourceFile", sourcePath);
    breakpoints = cJSON_CreateArray();
    TEST_ASSERT_NOT_NULL(breakpoints);
    breakpoint = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(breakpoint);
    cJSON_AddNumberToObject(breakpoint, "line", 2);
    cJSON_AddStringToObject(breakpoint, "hitCondition", "2");
    cJSON_AddItemToArray(breakpoints, breakpoint);
    breakpoint = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(breakpoint);
    cJSON_AddNumberToObject(breakpoint, "line", 2);
    cJSON_AddStringToObject(breakpoint, "condition", "value == 0");
    cJSON_AddStringToObject(breakpoint, "logMessage", "log value={value}");
    cJSON_AddItemToArray(breakpoints, breakpoint);
    cJSON_AddItemToObject(params, "breakpoints", breakpoints);
    debug_client_send_request(&client, 2, "setBreakpoints", params);
    message = debug_client_expect_event(&client, "breakpointResolved");
    TEST_ASSERT_TRUE(debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "resolved") != 0);
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "breakpointResolved");
    TEST_ASSERT_TRUE(debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "resolved") != 0);
    cJSON_Delete(message);
    message = debug_client_expect_response(&client, 2);
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    functionBreakpoints = cJSON_CreateArray();
    TEST_ASSERT_NOT_NULL(functionBreakpoints);
    functionBreakpoint = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(functionBreakpoint);
    cJSON_AddStringToObject(functionBreakpoint, "name", "addOne");
    cJSON_AddStringToObject(functionBreakpoint, "condition", "value == 2");
    cJSON_AddItemToArray(functionBreakpoints, functionBreakpoint);
    cJSON_AddItemToObject(params, "breakpoints", functionBreakpoints);
    debug_client_send_request(&client, 3, "setFunctionBreakpoints", params);
    message = debug_client_expect_event(&client, "breakpointResolved");
    TEST_ASSERT_TRUE(debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "resolved") != 0);
    TEST_ASSERT_EQUAL_STRING("addOne", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "functionName"));
    cJSON_Delete(message);
    message = debug_client_expect_response(&client, 3);
    cJSON_Delete(message);

    debug_client_send_request(&client, 4, "continue", ZR_NULL);
    message = debug_client_expect_response(&client, 4);
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "continued");
    cJSON_Delete(message);

    message = debug_client_expect_event(&client, "output");
    outputParams = cJSON_GetObjectItemCaseSensitive(message, "params");
    TEST_ASSERT_EQUAL_STRING("console", debug_json_string(outputParams, "category"));
    TEST_ASSERT_TRUE(strstr(debug_json_string(outputParams, "output"), "log value=0") != ZR_NULL);
    cJSON_Delete(message);

    message = debug_client_expect_event(&client, "stopped");
    TEST_ASSERT_EQUAL_STRING("breakpoint", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "reason"));
    TEST_ASSERT_EQUAL_STRING("addOne", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "functionName"));
    TEST_ASSERT_EQUAL_INT(2, debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "line"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "expression", "value");
    cJSON_AddNumberToObject(params, "frameId", 1);
    debug_client_send_request(&client, 5, "evaluate", params);
    message = debug_client_expect_response(&client, 5);
    TEST_ASSERT_EQUAL_STRING("1", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "result"), "value"));
    cJSON_Delete(message);

    debug_client_send_request(&client, 6, "continue", ZR_NULL);
    message = debug_client_expect_response(&client, 6);
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "continued");
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "stopped");
    TEST_ASSERT_EQUAL_STRING("breakpoint", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "reason"));
    TEST_ASSERT_EQUAL_STRING("addOne", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "functionName"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "expression", "value");
    cJSON_AddNumberToObject(params, "frameId", 1);
    debug_client_send_request(&client, 7, "evaluate", params);
    message = debug_client_expect_response(&client, 7);
    TEST_ASSERT_EQUAL_STRING("2", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "result"), "value"));
    cJSON_Delete(message);

    debug_client_send_request(&client, 8, "continue", ZR_NULL);
    message = debug_client_expect_response(&client, 8);
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "continued");
    cJSON_Delete(message);

    debug_execution_thread_join(&thread);
    TEST_ASSERT_TRUE(thread.success);
    TEST_ASSERT_EQUAL_INT64(10, thread.result);
    ZrDebug_NotifyTerminated(agent, thread.success);

    message = debug_client_expect_event(&client, "terminated");
    TEST_ASSERT_TRUE(debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "success") != 0);
    cJSON_Delete(message);

    ZrNetwork_StreamClose(&client);
    ZrDebug_AgentStop(agent);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_agent_supports_caught_exception_breakpoints(void) {
    const char *sourcePath = "debug_agent_caught_exception_fixture.zr";
    const char *source =
            "func handle(flag: bool): int {\n"
            "    try {\n"
            "        if (flag) { throw \"boom\"; }\n"
            "    } catch (error) {\n"
            "        var message = error;\n"
            "        return 41;\n"
            "    }\n"
            "    return 0;\n"
            "}\n"
            "return handle(true);";
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
    cJSON *exceptionScope;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_debug_agent_source(state, sourcePath, source);
    TEST_ASSERT_NOT_NULL(function);

    memset(&config, 0, sizeof(config));
    config.address = "127.0.0.1:0";
    config.suspend_on_start = ZR_TRUE;
    config.auth_token = "secret";
    config.stop_on_uncaught_exception = ZR_TRUE;

    TEST_ASSERT_TRUE(
            ZrDebug_AgentStart(state, function, "tests.debug.caught_exception", &config, &agent, error, sizeof(error)));
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
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "stopped");
    TEST_ASSERT_EQUAL_STRING("entry", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "reason"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddBoolToObject(params, "caught", 1);
    cJSON_AddBoolToObject(params, "uncaught", 0);
    debug_client_send_request(&client, 2, "setExceptionBreakpoints", params);
    message = debug_client_expect_response(&client, 2);
    cJSON_Delete(message);

    debug_client_send_request(&client, 3, "continue", ZR_NULL);
    message = debug_client_expect_response(&client, 3);
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "continued");
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "stopped");
    TEST_ASSERT_EQUAL_STRING("exception", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "reason"));
    TEST_ASSERT_EQUAL_STRING("caught", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "exceptionKind"));
    TEST_ASSERT_EQUAL_STRING("handle", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "functionName"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "frameId", 1);
    debug_client_send_request(&client, 4, "scopes", params);
    message = debug_client_expect_response(&client, 4);
    scopes = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "scopes");
    TEST_ASSERT_TRUE(cJSON_IsArray(scopes));
    exceptionScope = debug_find_named_object(scopes, "Exception");
    TEST_ASSERT_NOT_NULL(exceptionScope);
    cJSON_Delete(message);

    debug_client_send_request(&client, 5, "continue", ZR_NULL);
    message = debug_client_expect_response(&client, 5);
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "continued");
    cJSON_Delete(message);

    debug_execution_thread_join(&thread);
    TEST_ASSERT_TRUE(thread.success);
    TEST_ASSERT_EQUAL_INT64(41, thread.result);
    ZrDebug_NotifyTerminated(agent, thread.success);

    message = debug_client_expect_event(&client, "terminated");
    TEST_ASSERT_TRUE(debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "success") != 0);
    cJSON_Delete(message);

    ZrNetwork_StreamClose(&client);
    ZrDebug_AgentStop(agent);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_debug_agent_matches_source_breakpoints_across_separator_variants(void) {
    const char *runtimeSourcePath = "virtual\\debug\\separator_fixture.zr";
    const char *requestSourcePath = "virtual/debug/separator_fixture.zr";
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
    function = compile_debug_agent_fixture(state, runtimeSourcePath);
    TEST_ASSERT_NOT_NULL(function);

    memset(&config, 0, sizeof(config));
    config.address = "127.0.0.1:0";
    config.suspend_on_start = ZR_TRUE;
    config.auth_token = "secret";
    config.stop_on_uncaught_exception = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrDebug_AgentStart(state,
                                        function,
                                        "tests.debug.separator_variants",
                                        &config,
                                        &agent,
                                        error,
                                        sizeof(error)));
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
    TEST_ASSERT_EQUAL_STRING(runtimeSourcePath,
                             debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "sourceFile"));
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "stopped");
    TEST_ASSERT_EQUAL_STRING("entry", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "reason"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "moduleName", "tests.debug.separator_variants");
    cJSON_AddStringToObject(params, "sourceFile", requestSourcePath);
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
    message = debug_client_expect_event(&client, "continued");
    cJSON_Delete(message);

    ZrDebug_Pause(agent);
    message = debug_client_expect_event(&client, "stopped");
    debug_assert_stopped_location(message, "pause", sourcePath, "spin", 4, 10, &pausedLine);
    cJSON_Delete(message);

    debug_client_send_request(&client, 6, "stackTrace", ZR_NULL);
    message = debug_client_expect_response(&client, 6);
    frames = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "frames");
    TEST_ASSERT_TRUE(cJSON_IsArray(frames));
    TEST_ASSERT_TRUE(cJSON_GetArraySize(frames) >= 2);
    topFrame = cJSON_GetArrayItem(frames, 0);
    TEST_ASSERT_EQUAL_STRING("spin", debug_json_string(topFrame, "functionName"));
    TEST_ASSERT_EQUAL_STRING(sourcePath, debug_json_string(topFrame, "sourceFile"));
    TEST_ASSERT_TRUE(debug_json_int(topFrame, "line") >= 4);
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

static void test_debug_agent_language_gauntlet_project_breakpoint_pause_and_result(void) {
    const char *projectName = "language_debug_gauntlet";
    const char *moduleName = "tests.debug.language_gauntlet";
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    SZrState *state;
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
    cJSON *callerFrame;
    cJSON *scopes;
    cJSON *localsScope;
    cJSON *values;
    cJSON *totalItem;
    cJSON *indexItem;
    cJSON *burstItem;
    cJSON *bannerItem;
    cJSON *checksumBaseItem;
    int pausedLine;
    int topFrameId;
    int callerFrameId;
    int localsScopeId;
    SZrString *resultString;

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);
    gDebugProjectName = projectName;
    state->global->sourceLoader = debug_project_source_loader;

    function = compile_debug_agent_project_entry(state, projectName, "src/main.zr", sourcePath, sizeof(sourcePath));
    TEST_ASSERT_NOT_NULL(function);

    memset(&config, 0, sizeof(config));
    config.address = "127.0.0.1:0";
    config.suspend_on_start = ZR_TRUE;
    config.auth_token = "secret";
    config.stop_on_uncaught_exception = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrDebug_AgentStart(state, function, moduleName, &config, &agent, error, sizeof(error)));
    memset(&client, 0, sizeof(client));
    debug_client_connect(agent, &client);

    memset(&thread, 0, sizeof(thread));
    thread.state = state;
    thread.function = function;
    thread.mode = ZR_DEBUG_EXECUTION_CAPTURE_VALUE;
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
    TEST_ASSERT_EQUAL_STRING(moduleName, debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "moduleName"));
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "stopped");
    TEST_ASSERT_EQUAL_STRING("entry", debug_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "reason"));
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "moduleName", moduleName);
    cJSON_AddStringToObject(params, "sourceFile", sourcePath);
    cJSON_AddItemToObject(params, "lines", cJSON_CreateIntArray((const int[]){9}, 1));
    debug_client_send_request(&client, 2, "setBreakpoints", params);
    message = debug_client_expect_event(&client, "breakpointResolved");
    TEST_ASSERT_TRUE(debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "resolved") != 0);
    TEST_ASSERT_EQUAL_INT(9, debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "line"));
    cJSON_Delete(message);
    message = debug_client_expect_response(&client, 2);
    cJSON_Delete(message);

    debug_client_send_request(&client, 3, "continue", ZR_NULL);
    message = debug_client_expect_response(&client, 3);
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "continued");
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "stopped");
    debug_assert_stopped_location(message, "breakpoint", sourcePath, "spin", 9, 9, ZR_NULL);
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "moduleName", moduleName);
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

    ZrDebug_Pause(agent);
    message = debug_client_expect_event(&client, "stopped");
    debug_assert_stopped_location(message, "pause", sourcePath, "spin", 7, 13, &pausedLine);
    cJSON_Delete(message);

    debug_client_send_request(&client, 6, "stackTrace", ZR_NULL);
    message = debug_client_expect_response(&client, 6);
    frames = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "frames");
    TEST_ASSERT_TRUE(cJSON_IsArray(frames));
    TEST_ASSERT_TRUE(cJSON_GetArraySize(frames) >= 2);
    topFrame = cJSON_GetArrayItem(frames, 0);
    callerFrame = cJSON_GetArrayItem(frames, 1);
    TEST_ASSERT_EQUAL_STRING("spin", debug_json_string(topFrame, "functionName"));
    TEST_ASSERT_EQUAL_STRING(sourcePath, debug_json_string(topFrame, "sourceFile"));
    TEST_ASSERT_TRUE(debug_json_int(topFrame, "line") >= 7);
    TEST_ASSERT_TRUE(debug_json_int(topFrame, "line") <= 13);
    TEST_ASSERT_EQUAL_STRING("runGauntlet", debug_json_string(callerFrame, "functionName"));
    TEST_ASSERT_EQUAL_STRING(sourcePath, debug_json_string(callerFrame, "sourceFile"));
    topFrameId = debug_json_int(topFrame, "frameId");
    callerFrameId = debug_json_int(callerFrame, "frameId");
    TEST_ASSERT_TRUE(topFrameId > 0);
    TEST_ASSERT_TRUE(callerFrameId > 0);
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "frameId", topFrameId);
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
    burstItem = debug_find_named_object(values, "burst");
    TEST_ASSERT_NOT_NULL(totalItem);
    TEST_ASSERT_NOT_NULL(indexItem);
    TEST_ASSERT_TRUE(debug_json_string(totalItem, "value")[0] != '\0');
    TEST_ASSERT_TRUE(debug_json_string(indexItem, "value")[0] != '\0');
    if (burstItem == ZR_NULL) {
        TEST_ASSERT_TRUE(pausedLine == 7 || pausedLine == 8);
    } else {
        TEST_ASSERT_TRUE(debug_json_string(burstItem, "value")[0] != '\0');
    }
    cJSON_Delete(message);

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddNumberToObject(params, "frameId", callerFrameId);
    debug_client_send_request(&client, 9, "scopes", params);
    message = debug_client_expect_response(&client, 9);
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
    debug_client_send_request(&client, 10, "variables", params);
    message = debug_client_expect_response(&client, 10);
    values = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(message, "result"), "variables");
    TEST_ASSERT_TRUE(cJSON_IsArray(values));
    bannerItem = debug_find_named_object(values, "banner");
    checksumBaseItem = debug_find_named_object(values, "checksumBase");
    TEST_ASSERT_NOT_NULL(bannerItem);
    TEST_ASSERT_NOT_NULL(checksumBaseItem);
    TEST_ASSERT_EQUAL_STRING("GAUNTLET_OK", debug_json_string(bannerItem, "value"));
    TEST_ASSERT_EQUAL_STRING("1610", debug_json_string(checksumBaseItem, "value"));
    cJSON_Delete(message);

    debug_client_send_request(&client, 11, "continue", ZR_NULL);
    message = debug_client_expect_response(&client, 11);
    cJSON_Delete(message);
    message = debug_client_expect_event(&client, "continued");
    cJSON_Delete(message);

    debug_execution_thread_join(&thread);
    TEST_ASSERT_TRUE(thread.success);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, thread.resultValue.type);
    resultString = ZR_CAST_STRING(state, thread.resultValue.value.object);
    TEST_ASSERT_NOT_NULL(resultString);
    TEST_ASSERT_EQUAL_STRING("GAUNTLET_OK checksum=13910", ZrCore_String_GetNativeString(resultString));
    ZrDebug_NotifyTerminated(agent, thread.success);

    message = debug_client_expect_event(&client, "terminated");
    TEST_ASSERT_TRUE(debug_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "success") != 0);
    cJSON_Delete(message);

    ZrNetwork_StreamClose(&client);
    ZrDebug_AgentStop(agent);
    ZrCore_Function_Free(state, function);
    state->global->sourceLoader = ZR_NULL;
    gDebugProjectName = ZR_NULL;
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
    RUN_TEST(test_debug_agent_exposes_entry_script_locals_with_initializers);
    RUN_TEST(test_debug_agent_expands_object_members_and_runtime_globals);
    RUN_TEST(test_debug_agent_expands_native_network_methods_without_asserting);
    RUN_TEST(test_debug_agent_reports_richer_stack_scopes_and_safe_evaluate);
    RUN_TEST(test_debug_agent_separates_instance_metadata_and_supports_index_windows);
    RUN_TEST(test_debug_agent_supports_function_hit_condition_and_log_breakpoints);
    RUN_TEST(test_debug_agent_supports_caught_exception_breakpoints);
    RUN_TEST(test_debug_agent_matches_source_breakpoints_across_separator_variants);
    RUN_TEST(test_debug_agent_step_in_and_out_cross_call_boundaries);
    RUN_TEST(test_debug_agent_pause_stops_at_next_safepoint_and_preserves_stack);
    RUN_TEST(test_debug_agent_reports_uncaught_exception_from_runtime_without_cli_bridge);
    RUN_TEST(test_debug_agent_hits_source_breakpoints_on_binary_loaded_functions);
    RUN_TEST(test_debug_agent_language_gauntlet_project_breakpoint_pause_and_result);
    return UNITY_END();
}
