#ifndef ZR_TEST_OWNERSHIP_TASK_CALLBACK_CASES_H
#define ZR_TEST_OWNERSHIP_TASK_CALLBACK_CASES_H

#include "zr_vm_core/task_runtime.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_library/task_runtime.h"

static TZrUInt32 g_task_cleanup_drops;
static TZrBool g_task_callback_throws;
static TZrBool g_task_cleanup_saw_abandoned_root;
static SZrAotGcRootFrame *g_task_abandoned_root;
static char g_task_cleanup_result[ZR_VM_SHORT_STRING_MAX + 65u];

static TZrInt64 task_cleanup_collecting_drop(SZrState *state) {
    ++g_task_cleanup_drops;
    for (SZrAotGcRootFrame *frame = state->aotGcRootFrameStack;
         frame != ZR_NULL; frame = frame->previous) {
        if (frame == g_task_abandoned_root) {
            g_task_cleanup_saw_abandoned_root = ZR_TRUE;
            return 0;
        }
    }
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    return 0;
}

static TZrInt64 task_callback_with_suspended_owner(SZrState *state) {
    SZrTypeValue owner;
    SZrClosureNative *destructor = ZrCore_ClosureNative_New(state, 0u);
    SZrString *result;
    SZrAotGcRootFrame abandonedRoot;
    SZrRawObject *abandonedValue = ZR_NULL;
    static const SZrAotGcRootSlot rootSlot = {
        0u, 0u, 0u, 0u, ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS, 0u, 0u
    };
    static const SZrAotGcRootMap rootMap = {1u, &rootSlot};
    TEST_ASSERT_NOT_NULL(destructor);
    destructor->nativeFunction = task_cleanup_collecting_drop;
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(destructor));
    pending_create_shared(&owner);
    ZrCore_ObjectPrototype_AddMeta(state, ((SZrObject *)owner.value.object)->prototype,
            ZR_META_DESTRUCTOR, (SZrFunction *)ZR_CAST_RAW_OBJECT_AS_SUPER(destructor));
    TEST_ASSERT_TRUE(execution_push_exception_handler(state, state->callInfoList, 0u));
    pending_set_return(&owner);
    execution_enter_finally(state, &state->exceptionHandlerStack[state->exceptionHandlerStackLength - 1u]);
    ZrCore_Ownership_ReleaseValue(state, &owner);
    if (g_task_callback_throws) {
        TEST_ASSERT_TRUE(ZrCore_Gc_AotRootFramePush(state, &abandonedRoot,
                (TZrStackValuePointer)&abandonedValue, &rootMap));
        g_task_abandoned_root = &abandonedRoot;
        ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_RUNTIME_ERROR);
    }
    result = ZrCore_String_Create(state, g_task_cleanup_result, sizeof(g_task_cleanup_result) - 1u);
    TEST_ASSERT_NOT_NULL(result);
    ZrCore_Value_InitAsRawObject(state, ZrCore_Stack_GetValue(state->stackTop.valuePointer),
            ZR_CAST_RAW_OBJECT_AS_SUPER(result));
    state->stackTop.valuePointer++;
    return 1;
}

static void assert_task_suspended_owner_cleanup(TZrBool throws) {
    const ZrLibModuleDescriptor *module;
    const ZrLibTypeDescriptor *jobType = ZR_NULL;
    const ZrLibTypeDescriptor *taskType = ZR_NULL;
    SZrTypeValue schedulerValue, callableValue, jobValue, constructed, taskValue, result;
    SZrRawObject *taskObject = ZR_NULL;
    SZrClosureNative *callable;
    SZrFunction *schedulerFunction;
    SZrObject *job;
    ZrLibCallContext context = {0};
    ZrLibraryTaskRuntimeWorkItem workItem;
    TZrUInt32 rootDepth = g_state->aotGcRootFrameDepth;
    g_task_callback_throws = throws;
    g_task_cleanup_drops = 0u;
    g_task_cleanup_saw_abandoned_root = ZR_FALSE;
    g_task_abandoned_root = ZR_NULL;
    memset(g_task_cleanup_result, 't', sizeof(g_task_cleanup_result) - 1u);
    g_task_cleanup_result[sizeof(g_task_cleanup_result) - 1u] = '\0';
    TEST_ASSERT_TRUE(ZrCore_TaskRuntime_RegisterBuiltins(g_state->global));
    schedulerFunction = compile_source("var task = import(\"zr.task\"); return task.currentScheduler;");
    TEST_ASSERT_NOT_NULL(schedulerFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(g_state, schedulerFunction, &schedulerValue));
    ZrCore_Function_Free(g_state, schedulerFunction);
    module = ZrLibrary_NativeRegistry_FindModule(g_state->global, "zr.task");
    TEST_ASSERT_NOT_NULL(module);
    for (TZrSize index = 0u; index < module->typeCount; ++index) {
        if (strcmp(module->types[index].name, "Job") == 0) { jobType = &module->types[index]; }
        if (strcmp(module->types[index].name, "Task") == 0) { taskType = &module->types[index]; }
    }
    TEST_ASSERT_NOT_NULL(jobType);
    TEST_ASSERT_NOT_NULL(taskType);
    callable = ZrCore_ClosureNative_New(g_state, 0u);
    TEST_ASSERT_NOT_NULL(callable);
    callable->nativeFunction = task_callback_with_suspended_owner;
    ZrCore_Value_InitAsRawObject(g_state, &callableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(callable));
    job = ZrCore_Object_New(g_state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(job);
    ZrCore_Object_Init(g_state, job);
    ZrLib_Value_SetObject(g_state, &jobValue, job, ZR_VALUE_TYPE_OBJECT);
    context.state = g_state;
    context.selfValue = &jobValue;
    context.argumentValues = &callableValue;
    context.argumentCount = 1u;
    TEST_ASSERT_TRUE(jobType->metaMethods[0].callback(&context, &constructed));
    TEST_ASSERT_TRUE(ZrLibrary_TaskRuntime_PrepareJob(g_state,
            (SZrObject *)schedulerValue.value.object, job, &taskValue, &workItem));
    TEST_ASSERT_TRUE(ZrLibrary_TaskRuntime_ExecutePreparedJob(g_state, &workItem));
    TEST_ASSERT_EQUAL_UINT32(1u, g_task_cleanup_drops);
    TEST_ASSERT_FALSE(g_task_cleanup_saw_abandoned_root);
    TEST_ASSERT_EQUAL_UINT32(rootDepth, g_state->aotGcRootFrameDepth);
    TEST_ASSERT_EQUAL_UINT32(0u, g_state->exceptionHandlerStackLength);
    TEST_ASSERT_TRUE(ZrCore_GcRootHandle_Resolve(g_state, &workItem.taskRoot, &taskObject));
    TEST_ASSERT_TRUE(ZrLibrary_TaskRuntime_IsTaskComplete(g_state, (SZrObject *)taskObject));
    if (!throws) {
        TZrBool checkedResult = ZR_FALSE;
        ZrLib_Value_SetObject(g_state, &taskValue, (SZrObject *)taskObject, ZR_VALUE_TYPE_OBJECT);
        context.selfValue = &taskValue;
        context.argumentCount = 0u;
        for (TZrSize index = 0u; index < taskType->methodCount; ++index) {
            if (strcmp(taskType->methods[index].name, "result") == 0) {
                TEST_ASSERT_TRUE(taskType->methods[index].callback(&context, &result));
                TEST_ASSERT_EQUAL_STRING(g_task_cleanup_result,
                        ZrCore_String_GetNativeString((SZrString *)result.value.object));
                checkedResult = ZR_TRUE;
                break;
            }
        }
        TEST_ASSERT_TRUE(checkedResult);
    }
    ZrLibrary_TaskRuntime_ReleasePreparedJob(g_state, &workItem);
}

static void test_task_result_survives_suspended_owner_cleanup_gc(void) {
    assert_task_suspended_owner_cleanup(ZR_FALSE);
}

static void test_task_failure_unlinks_abandoned_callback_roots_before_drop(void) {
    assert_task_suspended_owner_cleanup(ZR_TRUE);
}

#endif
