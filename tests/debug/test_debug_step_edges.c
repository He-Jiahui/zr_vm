#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON/cJSON.h"
#include "runtime_support.h"
#include "unity.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_lib_debug/debug.h"
#include "zr_vm_lib_network/network.h"
#include "zr_vm_parser.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif

/* VM 执行线程与客户端线程并行，state/function 的有效期覆盖整个调试会话。 */
typedef struct ZrStepExecutionThread {
    SZrState *state;
    SZrFunction *function;
    TZrInt64 result;
    TZrBool success;
#if defined(_WIN32)
    HANDLE handle;
#else
    pthread_t handle;
#endif
} ZrStepExecutionThread;

/* 每例独占代理与连接；nextRequestId 绑定顺序消费的 JSON-RPC 响应。 */
typedef struct ZrStepSession {
    SZrState *state;
    SZrFunction *function;
    ZrDebugAgent *agent;
    SZrNetworkStream client;
    ZrStepExecutionThread thread;
    int nextRequestId;
} ZrStepSession;

/* 从一次 stopped 事件复制字段，释放 JSON 后仍可检查跨帧的步进结果。 */
typedef struct ZrObservedStop {
    char reason[32];
    char sourceFile[128];
    char functionName[128];
    int line;
} ZrObservedStop;

/* TODO: 此处手写声明省略 module.h 中的 ZR_API；后续用 Windows 共享构建核对导入约定并改用公共头。 */
TZrBool ZrVmLibSystem_Register(SZrGlobalState *global);

/* 源标签须与行断点请求一致，否则测试不能证明真实步进位置。 */
static SZrFunction *compile_step_edge_source(SZrState *state, const char *sourceLabel, const char *source) {
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

/* 平台线程入口只负责执行 VM；结果应在 join 后由客户端线程读取。 */
static void step_thread_run(ZrStepExecutionThread *thread) {
    if (thread == ZR_NULL) {
        return;
    }

    thread->success = ZrTests_Runtime_Function_ExecuteExpectInt64(thread->state, thread->function, &thread->result);
}

#if defined(_WIN32)
static DWORD WINAPI step_thread_proc(LPVOID argument) {
    step_thread_run((ZrStepExecutionThread *)argument);
    return 0;
}
#else
static void *step_thread_proc(void *argument) {
    step_thread_run((ZrStepExecutionThread *)argument);
    return ZR_NULL;
}
#endif

/* 代理启动和连接完成后运行 VM，使首个 entry 停点可被客户端观察。 */
static TZrBool step_thread_start(ZrStepExecutionThread *thread) {
    if (thread == ZR_NULL) {
        return ZR_FALSE;
    }

    thread->result = 0;
    thread->success = ZR_FALSE;
#if defined(_WIN32)
    thread->handle = CreateThread(NULL, 0, step_thread_proc, thread, 0, NULL);
    return thread->handle != NULL ? ZR_TRUE : ZR_FALSE;
#else
    return pthread_create(&thread->handle, NULL, step_thread_proc, thread) == 0 ? ZR_TRUE : ZR_FALSE;
#endif
}

static void step_thread_join(ZrStepExecutionThread *thread) {
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

/* 连接代理实际分配的 loopback 端口，允许各 Unity 可执行文件并行运行。 */
static void step_client_connect(ZrDebugAgent *agent, SZrNetworkStream *stream) {
    TZrChar endpointText[ZR_NETWORK_ENDPOINT_TEXT_CAPACITY];
    TZrChar error[256];
    SZrNetworkEndpoint endpoint;

    TEST_ASSERT_NOT_NULL(agent);
    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_TRUE(ZrDebug_AgentGetEndpoint(agent, endpointText, sizeof(endpointText)));
    TEST_ASSERT_TRUE(ZrNetwork_ParseEndpoint(endpointText, &endpoint, error, sizeof(error)));
    TEST_ASSERT_TRUE(ZrNetwork_StreamConnectLoopback(&endpoint, 3000, stream, error, sizeof(error)));
}

/* params 由请求对象接管并随序列化销毁；调用方不再持有它。 */
static void step_client_send_request(SZrNetworkStream *stream, int id, const char *method, cJSON *params) {
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

static cJSON *step_client_read_message(SZrNetworkStream *stream) {
    TZrChar frame[ZR_NETWORK_FRAME_BUFFER_CAPACITY];
    TZrSize length = 0;
    cJSON *message;

    TEST_ASSERT_TRUE(ZrNetwork_StreamReadFrame(stream, 5000, frame, sizeof(frame), &length));
    TEST_ASSERT_TRUE(length > 0);
    message = cJSON_Parse(frame);
    TEST_ASSERT_NOT_NULL(message);
    return message;
}

/* 依赖代理按请求号返回应答；返回对象需由上层会话释放。 */
static cJSON *step_client_expect_response(SZrNetworkStream *stream, int id) {
    cJSON *message = step_client_read_message(stream);
    cJSON *idItem = cJSON_GetObjectItemCaseSensitive(message, "id");
    cJSON *errorItem = cJSON_GetObjectItemCaseSensitive(message, "error");

    TEST_ASSERT_TRUE(cJSON_IsNumber(idItem));
    TEST_ASSERT_EQUAL_INT(id, (int)idItem->valuedouble);
    TEST_ASSERT_TRUE(errorItem == ZR_NULL);
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItemCaseSensitive(message, "result"));
    return message;
}

static cJSON *step_client_expect_event(SZrNetworkStream *stream, const char *method) {
    cJSON *message = step_client_read_message(stream);
    cJSON *methodItem = cJSON_GetObjectItemCaseSensitive(message, "method");

    TEST_ASSERT_TRUE(cJSON_IsString(methodItem));
    TEST_ASSERT_EQUAL_STRING(method, methodItem->valuestring);
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItemCaseSensitive(message, "params"));
    return message;
}

static const char *step_json_string(cJSON *object, const char *field) {
    cJSON *item = cJSON_GetObjectItemCaseSensitive(object, field);
    return cJSON_IsString(item) ? item->valuestring : "";
}

static int step_json_int(cJSON *object, const char *field) {
    cJSON *item = cJSON_GetObjectItemCaseSensitive(object, field);
    if (cJSON_IsBool(item)) {
        return cJSON_IsTrue(item) ? 1 : 0;
    }
    return cJSON_IsNumber(item) ? (int)item->valuedouble : 0;
}

static void step_observed_copy_text(char *buffer, size_t bufferSize, const char *text) {
    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    snprintf(buffer, bufferSize, "%s", text != ZR_NULL ? text : "");
}

/* 停点事件字段在 cJSON_Delete 后失效，先复制用于最终跨场景断言。 */
static void step_capture_stop(cJSON *message, ZrObservedStop *outStop) {
    cJSON *params;

    TEST_ASSERT_NOT_NULL(message);
    TEST_ASSERT_NOT_NULL(outStop);

    memset(outStop, 0, sizeof(*outStop));
    params = cJSON_GetObjectItemCaseSensitive(message, "params");
    TEST_ASSERT_NOT_NULL(params);
    step_observed_copy_text(outStop->reason, sizeof(outStop->reason), step_json_string(params, "reason"));
    step_observed_copy_text(outStop->sourceFile, sizeof(outStop->sourceFile), step_json_string(params, "sourceFile"));
    step_observed_copy_text(outStop->functionName, sizeof(outStop->functionName), step_json_string(params, "functionName"));
    outStop->line = step_json_int(params, "line");
}

/* 按需注册 zr.system 原生模块，并确认入口暂停后才允许行断点和步进请求。 */
static void step_session_start(ZrStepSession *session,
                               const char *moduleName,
                               const char *sourcePath,
                               const char *source,
                               TZrBool registerSystem) {
    ZrDebugAgentConfig config;
    TZrChar error[256];
    cJSON *params;
    cJSON *message;

    TEST_ASSERT_NOT_NULL(session);
    memset(session, 0, sizeof(*session));
    session->nextRequestId = 1;
    session->state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(session->state);
    if (registerSystem) {
        TEST_ASSERT_TRUE(ZrVmLibSystem_Register(session->state->global));
    }

    session->function = compile_step_edge_source(session->state, sourcePath, source);
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

    step_client_connect(session->agent, &session->client);
    session->thread.state = session->state;
    session->thread.function = session->function;
    TEST_ASSERT_TRUE(step_thread_start(&session->thread));

    params = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(params);
    cJSON_AddStringToObject(params, "authToken", "secret");
    step_client_send_request(&session->client, session->nextRequestId, "initialize", params);
    message = step_client_expect_response(&session->client, session->nextRequestId);
    session->nextRequestId++;
    cJSON_Delete(message);
    message = step_client_expect_event(&session->client, "initialized");
    cJSON_Delete(message);
    message = step_client_expect_event(&session->client, "moduleLoaded");
    cJSON_Delete(message);
    message = step_client_expect_event(&session->client, "stopped");
    TEST_ASSERT_EQUAL_STRING("entry",
                             step_json_string(cJSON_GetObjectItemCaseSensitive(message, "params"), "reason"));
    cJSON_Delete(message);
}

static void step_session_set_line_breakpoint(ZrStepSession *session,
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
    step_client_send_request(&session->client, session->nextRequestId, "setBreakpoints", params);
    message = step_client_expect_event(&session->client, "breakpointResolved");
    TEST_ASSERT_TRUE(step_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "resolved") != 0);
    TEST_ASSERT_EQUAL_INT(line, step_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "line"));
    cJSON_Delete(message);
    message = step_client_expect_response(&session->client, session->nextRequestId);
    session->nextRequestId++;
    cJSON_Delete(message);
}

static void step_session_continue_to_breakpoint(ZrStepSession *session, int expectedLine) {
    cJSON *message;
    cJSON *params;

    TEST_ASSERT_NOT_NULL(session);
    step_client_send_request(&session->client, session->nextRequestId, "continue", ZR_NULL);
    message = step_client_expect_response(&session->client, session->nextRequestId);
    session->nextRequestId++;
    cJSON_Delete(message);
    message = step_client_expect_event(&session->client, "continued");
    cJSON_Delete(message);
    message = step_client_expect_event(&session->client, "stopped");
    params = cJSON_GetObjectItemCaseSensitive(message, "params");
    TEST_ASSERT_EQUAL_STRING("breakpoint", step_json_string(params, "reason"));
    TEST_ASSERT_EQUAL_INT(expectedLine, step_json_int(params, "line"));
    cJSON_Delete(message);
}

/* 所有边界案例共用 response/continued/stopped 事件顺序，比较的是下一可见停点。 */
static void step_session_step(ZrStepSession *session, const char *method, ZrObservedStop *outStop) {
    cJSON *message;

    TEST_ASSERT_NOT_NULL(session);
    step_client_send_request(&session->client, session->nextRequestId, method, ZR_NULL);
    message = step_client_expect_response(&session->client, session->nextRequestId);
    session->nextRequestId++;
    cJSON_Delete(message);
    message = step_client_expect_event(&session->client, "continued");
    cJSON_Delete(message);
    message = step_client_expect_event(&session->client, "stopped");
    step_capture_stop(message, outStop);
    cJSON_Delete(message);
}

/* 收尾必须先恢复并 join VM，再用代理报告 terminated；连接关闭发生在 cleanup。 */
static void step_session_finish(ZrStepSession *session, TZrInt64 expectedResult) {
    cJSON *message;

    TEST_ASSERT_NOT_NULL(session);
    step_client_send_request(&session->client, session->nextRequestId, "continue", ZR_NULL);
    message = step_client_expect_response(&session->client, session->nextRequestId);
    session->nextRequestId++;
    cJSON_Delete(message);
    message = step_client_expect_event(&session->client, "continued");
    cJSON_Delete(message);

    step_thread_join(&session->thread);
    ZrDebug_NotifyTerminated(session->agent, session->thread.success);
    message = step_client_expect_event(&session->client, "terminated");
    TEST_ASSERT_TRUE(step_json_int(cJSON_GetObjectItemCaseSensitive(message, "params"), "success") != 0);
    cJSON_Delete(message);

    TEST_ASSERT_TRUE(session->thread.success);
    TEST_ASSERT_EQUAL_INT64(expectedResult, session->thread.result);
}

/* 执行线程已退出后，依序释放流、代理、编译函数和运行状态。 */
static void step_session_cleanup(ZrStepSession *session) {
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

/* 尾调用可能复用物理栈帧；next 应按用户可见调用层次跳到调用点之后。 */
static void test_step_over_tail_call_stops_after_logical_tail_frame(void) {
    const char *moduleName = "tests.debug.step_edges.tail";
    const char *sourcePath = "debug_step_tail_call.zr";
    const char *source =
            "fn leaf(value: int): int {\n"
            "    var computed = value + 1;\n"
            "    return computed;\n"
            "}\n"
            "fn tail(value: int): int {\n"
            "    return leaf(value);\n"
            "}\n"
            "var result = tail(4);\n"
            "var after = result + 2;\n"
            "return after;\n";
    ZrStepSession session;
    ZrObservedStop stop;
    TZrBool stoppedInTailCallee;

    step_session_start(&session, moduleName, sourcePath, source, ZR_FALSE);
    step_session_set_line_breakpoint(&session, moduleName, sourcePath, 6);
    step_session_continue_to_breakpoint(&session, 6);
    step_session_step(&session, "next", &stop);
    step_session_finish(&session, 7);
    step_session_cleanup(&session);

    stoppedInTailCallee = (TZrBool)(strcmp(stop.reason, "step") == 0 &&
                                    strcmp(stop.functionName, "leaf") == 0 &&
                                    stop.line < 8);
    TEST_ASSERT_FALSE_MESSAGE(stoppedInTailCallee,
                              "step over at a tail call must not stop inside the tail-called callee");
    TEST_ASSERT_EQUAL_STRING("step", stop.reason);
    TEST_ASSERT_EQUAL_STRING(sourcePath, stop.sourceFile);
    TEST_ASSERT_TRUE_MESSAGE(stop.line >= 8, "tail-call step over should resume at the caller's next visible location");
}

/* 原生函数无 ZR 源帧；stepIn 应回到 visit 的下一条可见语句。 */
static void test_step_in_native_call_behaves_like_step_over(void) {
    const char *moduleName = "tests.debug.step_edges.native";
    const char *sourcePath = "debug_step_native_call.zr";
    const char *source =
            "let system = import(\"zr.system\");\n"
            "fn visit(value: int): int {\n"
            "    system.console.printLine(\"native-step-edge\");\n"
            "    var after = value + 1;\n"
            "    return after;\n"
            "}\n"
            "return visit(4);\n";
    ZrStepSession session;
    ZrObservedStop stop;

    step_session_start(&session, moduleName, sourcePath, source, ZR_TRUE);
    step_session_set_line_breakpoint(&session, moduleName, sourcePath, 3);
    step_session_continue_to_breakpoint(&session, 3);
    step_session_step(&session, "stepIn", &stop);
    step_session_finish(&session, 5);
    step_session_cleanup(&session);

    TEST_ASSERT_EQUAL_STRING("step", stop.reason);
    TEST_ASSERT_EQUAL_STRING(sourcePath, stop.sourceFile);
    TEST_ASSERT_EQUAL_STRING("visit", stop.functionName);
    TEST_ASSERT_EQUAL_INT(4, stop.line);
}

/* 抛异常会跳过直接父帧；stepOut 应落在实际接住异常的 top 帧。 */
static void test_step_out_after_exception_unwind_stops_at_catcher(void) {
    const char *moduleName = "tests.debug.step_edges.unwind";
    const char *sourcePath = "debug_step_exception_unwind.zr";
    const char *source =
            "fn fail(): int {\n"
            "    throw \"boom\";\n"
            "}\n"
            "fn wrapper(): int {\n"
            "    return fail();\n"
            "}\n"
            "fn top(): int {\n"
            "    try {\n"
            "        return wrapper();\n"
            "    } catch (error) {\n"
            "        var recovered = 9;\n"
            "        return recovered;\n"
            "    }\n"
            "}\n"
            "return top();\n";
    ZrStepSession session;
    ZrObservedStop stop;

    step_session_start(&session, moduleName, sourcePath, source, ZR_FALSE);
    step_session_set_line_breakpoint(&session, moduleName, sourcePath, 2);
    step_session_continue_to_breakpoint(&session, 2);
    step_session_step(&session, "stepOut", &stop);
    step_session_finish(&session, 9);
    step_session_cleanup(&session);

    TEST_ASSERT_EQUAL_STRING("step", stop.reason);
    TEST_ASSERT_EQUAL_STRING(sourcePath, stop.sourceFile);
    TEST_ASSERT_EQUAL_STRING("top", stop.functionName);
    TEST_ASSERT_TRUE_MESSAGE(stop.line >= 8 && stop.line <= 12,
                             "step out after unwind should stop at the next visible parent-frame location");
}

/* 递归子调用共用源行，next 应按帧深度越过子调用而非按行号误停。 */
static void test_step_over_recursive_same_line_skips_child_call(void) {
    const char *moduleName = "tests.debug.step_edges.recursive";
    const char *sourcePath = "debug_step_recursive_same_line.zr";
    const char *source =
            "fn bounce(value: int): int {\n"
            "    if (value <= 0) { return 1; }\n"
            "    var nested = bounce(value - 1); return nested + 1;\n"
            "}\n"
            "var result = bounce(2);\n"
            "var after = result + 3;\n"
            "return after;\n";
    ZrStepSession session;
    ZrObservedStop stop;
    TZrBool stoppedInRecursiveChild;

    step_session_start(&session, moduleName, sourcePath, source, ZR_FALSE);
    step_session_set_line_breakpoint(&session, moduleName, sourcePath, 3);
    step_session_continue_to_breakpoint(&session, 3);
    step_session_step(&session, "next", &stop);
    step_session_finish(&session, 6);
    step_session_cleanup(&session);

    stoppedInRecursiveChild = (TZrBool)(strcmp(stop.reason, "step") == 0 &&
                                        strcmp(stop.functionName, "bounce") == 0 &&
                                        stop.line == 3);
    TEST_ASSERT_FALSE_MESSAGE(stoppedInRecursiveChild,
                              "step over must not stop in a recursive child call on the same source line");
    TEST_ASSERT_EQUAL_STRING("step", stop.reason);
    TEST_ASSERT_EQUAL_STRING(sourcePath, stop.sourceFile);
    TEST_ASSERT_TRUE_MESSAGE(stop.line >= 5, "recursive same-line step over should return to the caller side");
}

void setUp(void) {}

/* BUG: 线程启动后断言失败会经 Unity longjmp 跳过尾部 join/cleanup；空 tearDown 留下代理和暂停线程，影响下一用例。 */
void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_step_over_tail_call_stops_after_logical_tail_frame);
    RUN_TEST(test_step_in_native_call_behaves_like_step_over);
    RUN_TEST(test_step_out_after_exception_unwind_stops_at_catcher);
    RUN_TEST(test_step_over_recursive_same_line_skips_child_call);
    return UNITY_END();
}
