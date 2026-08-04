#include <string.h>

#include "unity.h"
#include "test_support.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/task_runtime.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_parser.h"

static SZrState *create_task_job_scheduler_test_state(void) {
    SZrState *state = ZrTests_State_Create(ZR_NULL);

    if (state == ZR_NULL || state->global == ZR_NULL || !ZrCore_TaskRuntime_RegisterBuiltins(state->global)) {
        ZrTests_State_Destroy(state);
        return ZR_NULL;
    }

    ZrParser_ToGlobalState_Register(state);

    return state;
}

static const ZrLibTypeDescriptor *find_type_descriptor(const ZrLibModuleDescriptor *module,
                                                        const char *typeName) {
    TZrSize index;

    if (module == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < module->typeCount; index++) {
        if (module->types[index].name != ZR_NULL && strcmp(module->types[index].name, typeName) == 0) {
            return &module->types[index];
        }
    }

    return ZR_NULL;
}

static SZrFunction *compile_task_job_scheduler_source(SZrState *state, const char *source, const char *name) {
    SZrString *sourceName;

    if (state == ZR_NULL || source == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state, (TZrNativeString)name, strlen(name));
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static const ZrLibMethodDescriptor *find_method_descriptor(const ZrLibTypeDescriptor *type,
                                                            const char *methodName) {
    TZrSize index;

    if (type == ZR_NULL || methodName == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < type->methodCount; index++) {
        if (type->methods[index].name != ZR_NULL && strcmp(type->methods[index].name, methodName) == 0) {
            return &type->methods[index];
        }
    }

    return ZR_NULL;
}

static const ZrLibFunctionDescriptor *find_function_descriptor(const ZrLibModuleDescriptor *module,
                                                                const char *functionName) {
    TZrSize index;

    if (module == ZR_NULL || functionName == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < module->functionCount; index++) {
        if (module->functions[index].name != ZR_NULL && strcmp(module->functions[index].name, functionName) == 0) {
            return &module->functions[index];
        }
    }

    return ZR_NULL;
}

static void assert_task_completes_with_void(SZrState *state,
                                            const ZrLibMethodDescriptor *resultMethod,
                                            SZrTypeValue *taskValue) {
    ZrLibCallContext context = {0};
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(resultMethod);
    TEST_ASSERT_NOT_NULL(resultMethod->callback);
    TEST_ASSERT_NOT_NULL(taskValue);
    context.state = state;
    context.selfValue = taskValue;
    TEST_ASSERT_TRUE(resultMethod->callback(&context, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL, result.type);
}

static void test_zr_task_descriptor_publishes_job_scheduler_contract(void) {
    SZrState *state = create_task_job_scheduler_test_state();
    const ZrLibModuleDescriptor *taskModule;
    const ZrLibTypeDescriptor *jobType;
    const ZrLibTypeDescriptor *schedulerType;
    const ZrLibMethodDescriptor *schedule;
    const ZrLibFunctionDescriptor *yieldNow;
    const ZrLibFunctionDescriptor *delay;

    TEST_ASSERT_NOT_NULL(state);
    taskModule = ZrLibrary_NativeRegistry_FindModule(state->global, "zr.task");
    TEST_ASSERT_NOT_NULL(taskModule);
    TEST_ASSERT_EQUAL_STRING("3.0.0", taskModule->moduleVersion);

    jobType = find_type_descriptor(taskModule, "Job");
    TEST_ASSERT_NOT_NULL(jobType);
    TEST_ASSERT_EQUAL_UINT64(1U, jobType->genericParameterCount);
    TEST_ASSERT_TRUE((jobType->protocolMask & ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_TASK_JOB)) != 0U);
    TEST_ASSERT_EQUAL_UINT64(1U, jobType->metaMethodCount);
    TEST_ASSERT_EQUAL_INT(ZR_META_CONSTRUCTOR, jobType->metaMethods[0].metaType);
    TEST_ASSERT_EQUAL_UINT(ZR_MEMBER_CONTRACT_ROLE_TASK_JOB_CONSTRUCT, jobType->metaMethods[0].contractRole);
    TEST_ASSERT_EQUAL_STRING("zr.task.Job<T>", jobType->metaMethods[0].returnTypeName);
    TEST_ASSERT_NOT_NULL(jobType->metaMethods[0].callback);

    schedulerType = find_type_descriptor(taskModule, "Scheduler");
    TEST_ASSERT_NOT_NULL(schedulerType);
    TEST_ASSERT_TRUE((schedulerType->protocolMask & ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_TASK_SCHEDULER)) != 0U);
    schedule = find_method_descriptor(schedulerType, "schedule");
    TEST_ASSERT_NOT_NULL(schedule);
    TEST_ASSERT_EQUAL_UINT(ZR_MEMBER_CONTRACT_ROLE_TASK_SCHEDULER_SCHEDULE, schedule->contractRole);
    TEST_ASSERT_EQUAL_STRING("zr.task.Task<T>", schedule->returnTypeName);
    TEST_ASSERT_EQUAL_UINT64(1U, schedule->genericParameterCount);
    TEST_ASSERT_EQUAL_STRING("zr.task.Job<T>", schedule->parameters[0].typeName);

    yieldNow = find_function_descriptor(taskModule, "yieldNow");
    delay = find_function_descriptor(taskModule, "delay");
    TEST_ASSERT_NOT_NULL(yieldNow);
    TEST_ASSERT_NOT_NULL(delay);
    TEST_ASSERT_EQUAL_UINT(ZR_MEMBER_CONTRACT_ROLE_TASK_YIELD_NOW, yieldNow->contractRole);
    TEST_ASSERT_EQUAL_UINT(ZR_MEMBER_CONTRACT_ROLE_TASK_DELAY, delay->contractRole);
    TEST_ASSERT_EQUAL_STRING("zr.task.Task<void>", yieldNow->returnTypeName);
    TEST_ASSERT_EQUAL_STRING("zr.task.Task<void>", delay->returnTypeName);
    TEST_ASSERT_EQUAL_UINT64(1U, delay->parameterCount);
    TEST_ASSERT_EQUAL_STRING("Duration", delay->parameters[0].typeName);
    TEST_ASSERT_NOT_NULL(yieldNow->callback);
    TEST_ASSERT_NOT_NULL(delay->callback);

    ZrTests_State_Destroy(state);
}

static void test_yield_now_and_delay_complete_through_task_result_abi(void) {
    SZrState *state = create_task_job_scheduler_test_state();
    const ZrLibModuleDescriptor *taskModule;
    const ZrLibTypeDescriptor *taskType;
    const ZrLibMethodDescriptor *resultMethod;
    const ZrLibFunctionDescriptor *yieldNow;
    const ZrLibFunctionDescriptor *delay;
    ZrLibCallContext yieldContext = {0};
    ZrLibCallContext delayContext = {0};
    SZrTypeValue delayArgument;
    SZrTypeValue yieldTask;
    SZrTypeValue delayTask;

    TEST_ASSERT_NOT_NULL(state);
    taskModule = ZrLibrary_NativeRegistry_FindModule(state->global, "zr.task");
    TEST_ASSERT_NOT_NULL(taskModule);
    taskType = find_type_descriptor(taskModule, "Task");
    TEST_ASSERT_NOT_NULL(taskType);
    resultMethod = find_method_descriptor(taskType, "result");
    yieldNow = find_function_descriptor(taskModule, "yieldNow");
    delay = find_function_descriptor(taskModule, "delay");
    TEST_ASSERT_NOT_NULL(yieldNow);
    TEST_ASSERT_NOT_NULL(delay);

    yieldContext.state = state;
    TEST_ASSERT_TRUE(yieldNow->callback(&yieldContext, &yieldTask));
    assert_task_completes_with_void(state, resultMethod, &yieldTask);

    ZrLib_Value_SetInt(state, &delayArgument, 0);
    delayContext.state = state;
    delayContext.argumentValues = &delayArgument;
    delayContext.argumentCount = 1U;
    TEST_ASSERT_TRUE(delay->callback(&delayContext, &delayTask));
    assert_task_completes_with_void(state, resultMethod, &delayTask);

    ZrTests_State_Destroy(state);
}

static void test_scheduler_consumes_job_at_source_call_boundary(void) {
    static const char *source =
            "var task = import(\"zr.task\");\n"
            "var job = init task.Job<int>(fn() => { return 7; });\n"
            "var first = task.currentScheduler.schedule<int>(job);\n"
            "var second = task.currentScheduler.schedule<int>(job);\n"
            "return 0;\n";
    SZrState *state = create_task_job_scheduler_test_state();

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NULL(compile_task_job_scheduler_source(
            state,
            source,
            "task_job_scheduler_single_consume.zr"));

    ZrTests_State_Destroy(state);
}

static void test_discarded_task_expression_is_rejected(void) {
    static const char *source =
            "var task = import(\"zr.task\");\n"
            "task.yieldNow();\n"
            "return 0;\n";
    SZrState *state = create_task_job_scheduler_test_state();

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NULL(compile_task_job_scheduler_source(state, source, "task_job_scheduler_must_use.zr"));

    ZrTests_State_Destroy(state);
}

static void test_job_constructor_and_current_scheduler_schedule_complete_callable_once(void) {
    static const char *source =
            "var task = import(\"zr.task\");\n"
            "var job = init task.Job<int>(fn() => { return 7; });\n"
            "var completion = task.currentScheduler.schedule<int>(job);\n"
            "var value = completion.result();\n"
            "return value;\n";
    SZrState *state = create_task_job_scheduler_test_state();
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_task_job_scheduler_source(state, source, "task_job_scheduler_round_trip.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(7, result);

    ZrTests_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_zr_task_descriptor_publishes_job_scheduler_contract);
    RUN_TEST(test_job_constructor_and_current_scheduler_schedule_complete_callable_once);
    RUN_TEST(test_yield_now_and_delay_complete_through_task_result_abi);
    RUN_TEST(test_scheduler_consumes_job_at_source_call_boundary);
    RUN_TEST(test_discarded_task_expression_is_rejected);
    return UNITY_END();
}
