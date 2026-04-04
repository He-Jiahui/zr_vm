#include <string.h>
#include <time.h>

#include "unity.h"
#include "test_support.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_library/project.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_parser.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_task/module.h"
#include "zr_vm_task/runtime.h"

typedef struct {
    clock_t startTime;
    clock_t endTime;
} SZrTaskTestTimer;

typedef struct {
    TZrBool reported;
    char message[256];
} SZrTaskCapturedParserDiagnostic;

static SZrState *create_task_test_state(void) {
    SZrState *state = ZrTests_State_Create(ZR_NULL);
    SZrGlobalState *global;

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return state;
    }

    global = state->global;
    ZrParser_ToGlobalState_Register(state);
    if (!ZrVmTask_Register(global)) {
        ZrTests_State_Destroy(state);
        return ZR_NULL;
    }

    return state;
}

static SZrFunction *compile_task_source(SZrState *state, const char *source, const char *name) {
    SZrString *sourceName;

    if (state == ZR_NULL || source == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state, name, strlen(name));
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static SZrAstNode *parse_task_source_ast(SZrState *state, const char *source, const char *name) {
    SZrString *sourceName;

    if (state == ZR_NULL || source == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state, (TZrNativeString)name, strlen(name));
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Parse(state, source, strlen(source), sourceName);
}

static void clear_task_parser_diagnostic(SZrTaskCapturedParserDiagnostic *diagnostic) {
    if (diagnostic == ZR_NULL) {
        return;
    }

    memset(diagnostic, 0, sizeof(*diagnostic));
}

static void capture_task_parser_error(TZrPtr userData,
                                      const SZrFileRange *location,
                                      const TZrChar *message,
                                      EZrToken token) {
    SZrTaskCapturedParserDiagnostic *diagnostic = (SZrTaskCapturedParserDiagnostic *)userData;

    ZR_UNUSED_PARAMETER(location);
    ZR_UNUSED_PARAMETER(token);
    if (diagnostic == ZR_NULL || diagnostic->reported) {
        return;
    }

    diagnostic->reported = ZR_TRUE;
    if (message != ZR_NULL) {
        snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", message);
    }
}

static TZrBool task_source_reports_parser_error(SZrState *state, const char *source, const char *name) {
    SZrString *sourceName;
    SZrParserState parserState;
    SZrAstNode *ast;
    SZrTaskCapturedParserDiagnostic diagnostic;

    if (state == ZR_NULL || source == ZR_NULL || name == ZR_NULL) {
        return ZR_TRUE;
    }

    sourceName = ZrCore_String_Create(state, (TZrNativeString)name, strlen(name));
    if (sourceName == ZR_NULL) {
        return ZR_TRUE;
    }

    clear_task_parser_diagnostic(&diagnostic);
    ZrParser_State_Init(&parserState, state, source, strlen(source), sourceName);
    parserState.errorCallback = capture_task_parser_error;
    parserState.errorUserData = &diagnostic;
    parserState.suppressErrorOutput = ZR_TRUE;

    ast = ZrParser_ParseWithState(&parserState);
    if (ast != ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
    }
    ZrParser_State_Free(&parserState);
    return diagnostic.reported;
}

static const ZrLibTypeDescriptor *find_task_type_descriptor(const ZrLibModuleDescriptor *descriptor,
                                                            const char *typeName) {
    TZrSize index;

    if (descriptor == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < descriptor->typeCount; index++) {
        const ZrLibTypeDescriptor *typeDescriptor = &descriptor->types[index];
        if (typeDescriptor->name != ZR_NULL && strcmp(typeDescriptor->name, typeName) == 0) {
            return typeDescriptor;
        }
    }

    return ZR_NULL;
}

static void test_project_config_defaults_enable_local_async_manual_threads_disabled(void) {
    SZrTaskTestTimer timer = {0};
    const char *json =
            "{\n"
            "  \"name\": \"task_default_project\",\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\"\n"
            "}";
    SZrState *state;
    SZrLibrary_Project *project;

    timer.startTime = clock();
    state = create_task_test_state();
    TEST_ASSERT_NOT_NULL(state);

    project = ZrLibrary_Project_New(state, json, "tests/fixtures/projects/hello_world/hello_world.zrp");
    TEST_ASSERT_NOT_NULL(project);
    TEST_ASSERT_FALSE(project->supportMultithread);
    TEST_ASSERT_TRUE(project->autoCoroutine);

    ZrLibrary_Project_Free(state, project);
    ZrTests_State_Destroy(state);
    timer.endTime = clock();
}

static void test_project_config_reads_supportMultithread_and_autoCoroutine_flags(void) {
    const char *json =
            "{\n"
            "  \"name\": \"task_mt_project\",\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"supportMultithread\": true,\n"
            "  \"autoCoroutine\": false\n"
            "}";
    SZrState *state = create_task_test_state();
    SZrLibrary_Project *project;

    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, json, "tests/fixtures/projects/hello_world/hello_world.zrp");
    TEST_ASSERT_NOT_NULL(project);
    TEST_ASSERT_TRUE(project->supportMultithread);
    TEST_ASSERT_FALSE(project->autoCoroutine);

    ZrLibrary_Project_Free(state, project);
    ZrTests_State_Destroy(state);
}

static void test_zr_task_root_module_registers_into_native_registry(void) {
    SZrState *state = create_task_test_state();
    const ZrLibModuleDescriptor *descriptor;

    TEST_ASSERT_NOT_NULL(state);
    descriptor = ZrLibrary_NativeRegistry_FindModule(state->global, "zr.task");
    TEST_ASSERT_NOT_NULL(descriptor);
    TEST_ASSERT_EQUAL_STRING("zr.task", descriptor->moduleName);
    TEST_ASSERT_TRUE(descriptor->functionCount > 0);
    TEST_ASSERT_TRUE(descriptor->typeCount > 0);

    ZrTests_State_Destroy(state);
}

static void test_zr_task_module_exposes_generic_concurrency_wrappers(void) {
    SZrState *state = create_task_test_state();
    const ZrLibModuleDescriptor *descriptor;
    const ZrLibTypeDescriptor *asyncType;
    const ZrLibTypeDescriptor *channelType;
    const ZrLibTypeDescriptor *mutexType;
    const ZrLibTypeDescriptor *sharedType;
    const ZrLibTypeDescriptor *transferType;
    const ZrLibTypeDescriptor *weakSharedType;
    const ZrLibTypeDescriptor *atomicBoolType;
    const ZrLibTypeDescriptor *atomicIntType;
    const ZrLibTypeDescriptor *atomicUIntType;

    TEST_ASSERT_NOT_NULL(state);
    descriptor = ZrLibrary_NativeRegistry_FindModule(state->global, "zr.task");
    TEST_ASSERT_NOT_NULL(descriptor);

    asyncType = find_task_type_descriptor(descriptor, "Async");
    channelType = find_task_type_descriptor(descriptor, "Channel");
    mutexType = find_task_type_descriptor(descriptor, "Mutex");
    sharedType = find_task_type_descriptor(descriptor, "Shared");
    transferType = find_task_type_descriptor(descriptor, "Transfer");
    weakSharedType = find_task_type_descriptor(descriptor, "WeakShared");
    atomicBoolType = find_task_type_descriptor(descriptor, "AtomicBool");
    atomicIntType = find_task_type_descriptor(descriptor, "AtomicInt");
    atomicUIntType = find_task_type_descriptor(descriptor, "AtomicUInt");

    TEST_ASSERT_NOT_NULL(asyncType);
    TEST_ASSERT_NOT_NULL(channelType);
    TEST_ASSERT_NOT_NULL(mutexType);
    TEST_ASSERT_NOT_NULL(sharedType);
    TEST_ASSERT_NOT_NULL(transferType);
    TEST_ASSERT_NOT_NULL(weakSharedType);
    TEST_ASSERT_NOT_NULL(atomicBoolType);
    TEST_ASSERT_NOT_NULL(atomicIntType);
    TEST_ASSERT_NOT_NULL(atomicUIntType);

    TEST_ASSERT_EQUAL_UINT64(1, asyncType->genericParameterCount);
    TEST_ASSERT_EQUAL_UINT64(1, channelType->genericParameterCount);
    TEST_ASSERT_EQUAL_UINT64(1, mutexType->genericParameterCount);
    TEST_ASSERT_EQUAL_UINT64(1, sharedType->genericParameterCount);
    TEST_ASSERT_EQUAL_UINT64(1, transferType->genericParameterCount);
    TEST_ASSERT_EQUAL_UINT64(1, weakSharedType->genericParameterCount);
    TEST_ASSERT_EQUAL_UINT64(0, atomicBoolType->genericParameterCount);
    TEST_ASSERT_EQUAL_UINT64(1, atomicIntType->genericParameterCount);
    TEST_ASSERT_EQUAL_UINT64(1, atomicUIntType->genericParameterCount);

    ZrTests_State_Destroy(state);
}

static void test_manual_scheduler_pump_executes_spawned_task(void) {
    static const char *source =
            "var task = %import(\"zr.task\");\n"
            "var scheduler = task.currentScheduler();\n"
            "scheduler.setAutoCoroutine(false);\n"
            "var handle = task.spawn(() -> { return 7; });\n"
            "scheduler.pump();\n"
            "return handle.result();\n";
    SZrState *state = create_task_test_state();
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_task_source(state, source, "task_manual_pump_test.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(7, result);

    ZrTests_State_Destroy(state);
}

static void test_spawn_thread_rejected_when_multithread_disabled(void) {
    static const char *source =
            "var task = %import(\"zr.task\");\n"
            "try {\n"
            "    task.spawnThread(() -> { return 1; });\n"
            "    return 0;\n"
            "} catch (err) {\n"
            "    return 1;\n"
            "}\n";
    SZrState *state = create_task_test_state();
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_task_source(state, source, "task_spawn_thread_disabled_test.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrTests_State_Destroy(state);
}

static void test_percent_async_and_await_lower_to_zr_task_runtime(void) {
    static const char *source =
            "%async func addOne(value: int): int {\n"
            "    return value + 1;\n"
            "}\n"
            "var handle = addOne(9);\n"
            "return %await handle;\n";
    SZrState *state = create_task_test_state();
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_task_source(state, source, "task_async_await_sugar_test.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(10, result);

    ZrTests_State_Destroy(state);
}

static void test_percent_async_wraps_declared_return_type_to_async(void) {
    static const char *source =
            "%async func addOne(value: int): int {\n"
            "    return value + 1;\n"
            "}\n"
            "return 0;\n";
    SZrState *state = create_task_test_state();
    SZrAstNode *ast;
    SZrAstNode *statement;
    SZrType *returnType;

    TEST_ASSERT_NOT_NULL(state);
    ast = parse_task_source_ast(state, source, "task_async_return_wrap_test.zr");
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_TRUE(ast->data.script.statements->count >= 1);

    statement = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(statement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, statement->type);
    returnType = statement->data.functionDeclaration.returnType;
    TEST_ASSERT_NOT_NULL(returnType);
    TEST_ASSERT_NOT_NULL(returnType->name);
    TEST_ASSERT_EQUAL_INT(ZR_AST_GENERIC_TYPE, returnType->name->type);
    TEST_ASSERT_NOT_NULL(returnType->name->data.genericType.name);
    TEST_ASSERT_EQUAL_STRING("Async",
                             ZrCore_String_GetNativeString(returnType->name->data.genericType.name->name));
    TEST_ASSERT_NOT_NULL(returnType->name->data.genericType.params);
    TEST_ASSERT_EQUAL_UINT64(1, returnType->name->data.genericType.params->count);

    ZrParser_Ast_Free(state, ast);
    ZrTests_State_Destroy(state);
}

static void test_percent_mutex_and_percent_atomic_type_sugar_compile(void) {
    static const char *source =
            "var guarded: %shared %mutex int;\n"
            "var flag: %atomic bool;\n"
            "var counter: %atomic int;\n"
            "var total: %atomic uint;\n"
            "return 0;\n";
    SZrState *state = create_task_test_state();
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_FALSE(task_source_reports_parser_error(state, source, "task_mutex_atomic_type_sugar_test.zr"));
    function = compile_task_source(state, source, "task_mutex_atomic_type_sugar_test.zr");
    TEST_ASSERT_NOT_NULL(function);

    ZrTests_State_Destroy(state);
}

static void test_percent_atomic_rejects_non_scalar_reference_type(void) {
    static const char *source =
            "var invalid: %atomic string;\n"
            "return 0;\n";
    SZrState *state = create_task_test_state();

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(task_source_reports_parser_error(state, source, "task_atomic_invalid_type_test.zr"));

    ZrTests_State_Destroy(state);
}

static void test_percent_await_is_rejected_outside_async_context(void) {
    static const char *source =
            "func invalid(): int {\n"
            "    var task = %import(\"zr.task\");\n"
            "    var handle = task.spawn(() -> { return 1; });\n"
            "    return %await handle;\n"
            "}\n"
            "return invalid();\n";
    SZrState *state = create_task_test_state();
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_task_source(state, source, "task_await_outside_async_test.zr");
    TEST_ASSERT_NULL(function);

    ZrTests_State_Destroy(state);
}

static void test_borrowed_value_cannot_cross_await_boundary(void) {
    static const char *source =
            "%async func invalid(value: %borrowed string): string {\n"
            "    var task = %import(\"zr.task\");\n"
            "    var handle = task.spawn(() -> { return 1; });\n"
            "    %await handle;\n"
            "    return value;\n"
            "}\n"
            "return 0;\n";
    SZrState *state = create_task_test_state();
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_task_source(state, source, "task_borrow_across_await_test.zr");
    TEST_ASSERT_NULL(function);

    ZrTests_State_Destroy(state);
}

static void test_borrowed_value_used_before_await_still_compiles(void) {
    static const char *source =
            "%async func valid(value: %borrowed string): string {\n"
            "    var before = value;\n"
            "    var task = %import(\"zr.task\");\n"
            "    var handle = task.spawn(() -> { return 1; });\n"
            "    %await handle;\n"
            "    return \"done\";\n"
            "}\n"
            "return 0;\n";
    SZrState *state = create_task_test_state();
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_FALSE(task_source_reports_parser_error(state, source, "task_borrow_before_await_ok_test.zr"));
    function = compile_task_source(state, source, "task_borrow_before_await_ok_test.zr");
    TEST_ASSERT_NOT_NULL(function);

    ZrTests_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_project_config_defaults_enable_local_async_manual_threads_disabled);
    RUN_TEST(test_project_config_reads_supportMultithread_and_autoCoroutine_flags);
    RUN_TEST(test_zr_task_root_module_registers_into_native_registry);
    RUN_TEST(test_zr_task_module_exposes_generic_concurrency_wrappers);
    RUN_TEST(test_manual_scheduler_pump_executes_spawned_task);
    RUN_TEST(test_spawn_thread_rejected_when_multithread_disabled);
    RUN_TEST(test_percent_async_and_await_lower_to_zr_task_runtime);
    RUN_TEST(test_percent_async_wraps_declared_return_type_to_async);
    RUN_TEST(test_percent_mutex_and_percent_atomic_type_sugar_compile);
    RUN_TEST(test_percent_atomic_rejects_non_scalar_reference_type);
    RUN_TEST(test_percent_await_is_rejected_outside_async_context);
    RUN_TEST(test_borrowed_value_cannot_cross_await_boundary);
    RUN_TEST(test_borrowed_value_used_before_await_still_compiles);
    return UNITY_END();
}
