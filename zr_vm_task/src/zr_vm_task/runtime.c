#include "zr_vm_task/runtime.h"

#include <string.h>

#include "zr_vm_core/debug.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/project.h"

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

enum {
    ZR_VM_TASK_STATUS_CREATED = 0,
    ZR_VM_TASK_STATUS_QUEUED = 1,
    ZR_VM_TASK_STATUS_RUNNING = 2,
    ZR_VM_TASK_STATUS_COMPLETED = 3,
    ZR_VM_TASK_STATUS_FAULTED = 4
};

typedef struct ZrVmTaskExecuteRequest {
    const SZrTypeValue *callable;
    SZrTypeValue result;
    TZrBool completed;
} ZrVmTaskExecuteRequest;

static const TZrChar *kTaskMainSchedulerField = "__zr_task_scheduler";
static const TZrChar *kTaskWorkerSchedulerField = "__zr_task_worker_scheduler";
static const TZrChar *kTaskQueueField = "__zr_task_queue";
static const TZrChar *kTaskQueueHeadField = "__zr_task_queue_head";
static const TZrChar *kTaskAutoCoroutineField = "__zr_task_auto_coroutine";
static const TZrChar *kTaskSupportMultithreadField = "__zr_task_support_multithread";
static const TZrChar *kTaskIsPumpingField = "__zr_task_is_pumping";
static const TZrChar *kTaskStatusField = "__zr_task_status";
static const TZrChar *kTaskCallableField = "__zr_task_callable";
static const TZrChar *kTaskResultField = "__zr_task_result";
static const TZrChar *kTaskErrorField = "__zr_task_error";
static const TZrChar *kTaskSchedulerOwnerField = "__zr_task_scheduler_owner";

static SZrObject *zr_vm_task_self_object(const ZrLibCallContext *context) {
    SZrTypeValue *selfValue = ZrLib_CallContext_Self(context);
    if (selfValue == ZR_NULL || (selfValue->type != ZR_VALUE_TYPE_OBJECT && selfValue->type != ZR_VALUE_TYPE_ARRAY) ||
        selfValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(context->state, selfValue->value.object);
}

static EZrValueType zr_vm_task_value_type_for_object(SZrObject *object) {
    return object != ZR_NULL && object->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY ? ZR_VALUE_TYPE_ARRAY
                                                                                       : ZR_VALUE_TYPE_OBJECT;
}

static TZrBool zr_vm_task_finish_object(SZrState *state, SZrTypeValue *result, SZrObject *object) {
    if (state == ZR_NULL || result == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(state, result, object, zr_vm_task_value_type_for_object(object));
    return ZR_TRUE;
}

static void zr_vm_task_set_value_field(SZrState *state,
                                       SZrObject *object,
                                       const TZrChar *fieldName,
                                       const SZrTypeValue *value) {
    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL) {
        return;
    }

    ZrLib_Object_SetFieldCString(state, object, fieldName, value);
}

static void zr_vm_task_set_null_field(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    SZrTypeValue value;
    ZrLib_Value_SetNull(&value);
    zr_vm_task_set_value_field(state, object, fieldName, &value);
}

static void zr_vm_task_set_bool_field(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrBool value) {
    SZrTypeValue fieldValue;
    ZrLib_Value_SetBool(state, &fieldValue, value);
    zr_vm_task_set_value_field(state, object, fieldName, &fieldValue);
}

static void zr_vm_task_set_int_field(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrInt64 value) {
    SZrTypeValue fieldValue;
    ZrLib_Value_SetInt(state, &fieldValue, value);
    zr_vm_task_set_value_field(state, object, fieldName, &fieldValue);
}

static const SZrTypeValue *zr_vm_task_get_field_value(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrLib_Object_GetFieldCString(state, object, fieldName);
}

static SZrObject *zr_vm_task_get_object_field(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    const SZrTypeValue *value = zr_vm_task_get_field_value(state, object, fieldName);
    if (value == ZR_NULL || (value->type != ZR_VALUE_TYPE_OBJECT && value->type != ZR_VALUE_TYPE_ARRAY) ||
        value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, value->value.object);
}

static TZrBool zr_vm_task_get_bool_field(SZrState *state,
                                         SZrObject *object,
                                         const TZrChar *fieldName,
                                         TZrBool defaultValue) {
    const SZrTypeValue *value = zr_vm_task_get_field_value(state, object, fieldName);
    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_BOOL) {
        return defaultValue;
    }

    return value->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE;
}

static TZrInt64 zr_vm_task_get_int_field(SZrState *state,
                                         SZrObject *object,
                                         const TZrChar *fieldName,
                                         TZrInt64 defaultValue) {
    const SZrTypeValue *value = zr_vm_task_get_field_value(state, object, fieldName);
    if (value == ZR_NULL) {
        return defaultValue;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        return value->value.nativeObject.nativeInt64;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        return (TZrInt64)value->value.nativeObject.nativeUInt64;
    }
    return defaultValue;
}

static SZrObject *zr_vm_task_root_object(SZrState *state) {
    if (state == ZR_NULL || state->global == ZR_NULL || state->global->zrObject.type != ZR_VALUE_TYPE_OBJECT ||
        state->global->zrObject.value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, state->global->zrObject.value.object);
}

static TZrBool zr_vm_task_default_support_multithread(SZrState *state) {
    SZrLibrary_Project *project;

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return ZR_FALSE;
    }

    project = (SZrLibrary_Project *)state->global->userData;
    return project != ZR_NULL && project->supportMultithread ? ZR_TRUE : ZR_FALSE;
}

static TZrBool zr_vm_task_default_auto_coroutine(SZrState *state) {
    SZrLibrary_Project *project;

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return ZR_TRUE;
    }

    project = (SZrLibrary_Project *)state->global->userData;
    return project == ZR_NULL || project->autoCoroutine ? ZR_TRUE : ZR_FALSE;
}

static SZrObject *zr_vm_task_ensure_scheduler_with_field(SZrState *state, const TZrChar *fieldName) {
    SZrObject *rootObject;
    SZrObject *scheduler;
    SZrObject *queue;
    SZrTypeValue schedulerValue;
    SZrTypeValue queueValue;

    if (state == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    rootObject = zr_vm_task_root_object(state);
    if (rootObject == ZR_NULL) {
        return ZR_NULL;
    }

    scheduler = zr_vm_task_get_object_field(state, rootObject, fieldName);
    if (scheduler != ZR_NULL) {
        return scheduler;
    }

    scheduler = ZrLib_Type_NewInstance(state, "Scheduler");
    if (scheduler == ZR_NULL) {
        scheduler = ZrLib_Object_New(state);
        if (scheduler == ZR_NULL) {
            return ZR_NULL;
        }
    }

    queue = ZrLib_Array_New(state);
    if (queue == ZR_NULL) {
        return ZR_NULL;
    }

    ZrLib_Value_SetObject(state, &schedulerValue, scheduler, ZR_VALUE_TYPE_OBJECT);
    ZrLib_Value_SetObject(state, &queueValue, queue, ZR_VALUE_TYPE_ARRAY);
    zr_vm_task_set_value_field(state, rootObject, fieldName, &schedulerValue);
    zr_vm_task_set_value_field(state, scheduler, kTaskQueueField, &queueValue);
    zr_vm_task_set_int_field(state, scheduler, kTaskQueueHeadField, 0);
    zr_vm_task_set_bool_field(state, scheduler, kTaskAutoCoroutineField, zr_vm_task_default_auto_coroutine(state));
    zr_vm_task_set_bool_field(state,
                              scheduler,
                              kTaskSupportMultithreadField,
                              zr_vm_task_default_support_multithread(state));
    zr_vm_task_set_bool_field(state, scheduler, kTaskIsPumpingField, ZR_FALSE);
    return scheduler;
}

static SZrObject *zr_vm_task_main_scheduler(SZrState *state) {
    return zr_vm_task_ensure_scheduler_with_field(state, kTaskMainSchedulerField);
}

static SZrObject *zr_vm_task_worker_scheduler(SZrState *state) {
    return zr_vm_task_ensure_scheduler_with_field(state, kTaskWorkerSchedulerField);
}

static SZrObject *zr_vm_task_scheduler_queue(SZrState *state, SZrObject *scheduler) {
    SZrObject *queue = zr_vm_task_get_object_field(state, scheduler, kTaskQueueField);
    SZrTypeValue queueValue;

    if (queue != ZR_NULL && queue->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return queue;
    }

    queue = ZrLib_Array_New(state);
    if (queue == ZR_NULL) {
        return ZR_NULL;
    }

    ZrLib_Value_SetObject(state, &queueValue, queue, ZR_VALUE_TYPE_ARRAY);
    zr_vm_task_set_value_field(state, scheduler, kTaskQueueField, &queueValue);
    return queue;
}

static void zr_vm_task_execute_callable_body(SZrState *state, TZrPtr arguments) {
    ZrVmTaskExecuteRequest *request = (ZrVmTaskExecuteRequest *)arguments;

    if (request == ZR_NULL || request->callable == ZR_NULL) {
        return;
    }

    request->completed = ZrLib_CallValue(state, request->callable, ZR_NULL, ZR_NULL, 0, &request->result);
}

static ZR_NO_RETURN void zr_vm_task_raise_fault(SZrState *state, const SZrTypeValue *errorValue) {
    if (state != ZR_NULL && errorValue != ZR_NULL &&
        (ZrCore_Exception_NormalizeThrownValue(state,
                                              errorValue,
                                              state->callInfoList,
                                              ZR_THREAD_STATUS_EXCEPTION_ERROR) ||
         ZrCore_Exception_NormalizeStatus(state, ZR_THREAD_STATUS_EXCEPTION_ERROR))) {
        ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_EXCEPTION_ERROR);
    }

    ZrCore_Debug_RunError(state, "Task fault");
}

static TZrBool zr_vm_task_handle_mark_faulted(SZrState *state,
                                              SZrObject *handle,
                                              EZrThreadStatus status,
                                              const SZrTypeValue *fallbackError) {
    SZrTypeValue errorValue;

    if (state == ZR_NULL || handle == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!state->hasCurrentException && !ZrCore_Exception_NormalizeStatus(state, status)) {
        return ZR_FALSE;
    }

    if (state->hasCurrentException) {
        ZrCore_Value_Copy(state, &errorValue, &state->currentException);
    } else if (fallbackError != ZR_NULL) {
        ZrCore_Value_Copy(state, &errorValue, fallbackError);
    } else {
        ZrLib_Value_SetString(state, &errorValue, "Task fault");
    }

    zr_vm_task_set_value_field(state, handle, kTaskErrorField, &errorValue);
    zr_vm_task_set_null_field(state, handle, kTaskResultField);
    zr_vm_task_set_null_field(state, handle, kTaskCallableField);
    zr_vm_task_set_int_field(state, handle, kTaskStatusField, ZR_VM_TASK_STATUS_FAULTED);
    ZrCore_State_ResetThread(state, ZR_THREAD_STATUS_FINE);
    return ZR_TRUE;
}

static TZrBool zr_vm_task_execute_handle(SZrState *state, SZrObject *handle) {
    const SZrTypeValue *callable;
    ZrVmTaskExecuteRequest request;
    EZrThreadStatus status;
    SZrTypeValue errorValue;

    if (state == ZR_NULL || handle == ZR_NULL) {
        return ZR_FALSE;
    }

    callable = zr_vm_task_get_field_value(state, handle, kTaskCallableField);
    if (callable == ZR_NULL) {
        ZrLib_Value_SetString(state, &errorValue, "Task callable is missing");
        return zr_vm_task_handle_mark_faulted(state, handle, ZR_THREAD_STATUS_RUNTIME_ERROR, &errorValue);
    }

    zr_vm_task_set_int_field(state, handle, kTaskStatusField, ZR_VM_TASK_STATUS_RUNNING);
    ZrLib_Value_SetNull(&request.result);
    request.callable = callable;
    request.completed = ZR_FALSE;

    status = ZrCore_Exception_TryRun(state, zr_vm_task_execute_callable_body, &request);
    if (status == ZR_THREAD_STATUS_FINE && request.completed && state->threadStatus == ZR_THREAD_STATUS_FINE) {
        zr_vm_task_set_value_field(state, handle, kTaskResultField, &request.result);
        zr_vm_task_set_null_field(state, handle, kTaskErrorField);
        zr_vm_task_set_null_field(state, handle, kTaskCallableField);
        zr_vm_task_set_int_field(state, handle, kTaskStatusField, ZR_VM_TASK_STATUS_COMPLETED);
        return ZR_TRUE;
    }

    if (status == ZR_THREAD_STATUS_FINE) {
        status = state->threadStatus != ZR_THREAD_STATUS_FINE ? state->threadStatus : ZR_THREAD_STATUS_RUNTIME_ERROR;
    }
    return zr_vm_task_handle_mark_faulted(state, handle, status, request.completed ? &request.result : ZR_NULL);
}

static TZrBool zr_vm_task_scheduler_step_internal(SZrState *state, SZrObject *scheduler) {
    SZrObject *queue;
    TZrInt64 head;
    const SZrTypeValue *queuedValue;
    SZrObject *handle;
    SZrObject *newQueue;
    SZrTypeValue queueValue;

    if (state == ZR_NULL || scheduler == ZR_NULL) {
        return ZR_FALSE;
    }

    queue = zr_vm_task_scheduler_queue(state, scheduler);
    if (queue == ZR_NULL) {
        return ZR_FALSE;
    }

    head = zr_vm_task_get_int_field(state, scheduler, kTaskQueueHeadField, 0);
    if (head < 0) {
        head = 0;
    }

    queuedValue = ZrLib_Array_Get(state, queue, (TZrSize)head);
    if (queuedValue == ZR_NULL) {
        if (head > 0 || ZrLib_Array_Length(queue) > 0) {
            newQueue = ZrLib_Array_New(state);
            if (newQueue != ZR_NULL) {
                ZrLib_Value_SetObject(state, &queueValue, newQueue, ZR_VALUE_TYPE_ARRAY);
                zr_vm_task_set_value_field(state, scheduler, kTaskQueueField, &queueValue);
            }
        }
        zr_vm_task_set_int_field(state, scheduler, kTaskQueueHeadField, 0);
        return ZR_FALSE;
    }

    zr_vm_task_set_int_field(state, scheduler, kTaskQueueHeadField, head + 1);
    if ((queuedValue->type != ZR_VALUE_TYPE_OBJECT && queuedValue->type != ZR_VALUE_TYPE_ARRAY) ||
        queuedValue->value.object == ZR_NULL) {
        return ZR_TRUE;
    }

    handle = ZR_CAST_OBJECT(state, queuedValue->value.object);
    zr_vm_task_execute_handle(state, handle);
    return ZR_TRUE;
}

static TZrInt64 zr_vm_task_scheduler_pump_internal(SZrState *state, SZrObject *scheduler) {
    TZrInt64 executed = 0;

    if (state == ZR_NULL || scheduler == ZR_NULL) {
        return 0;
    }

    if (zr_vm_task_get_bool_field(state, scheduler, kTaskIsPumpingField, ZR_FALSE)) {
        return 0;
    }

    zr_vm_task_set_bool_field(state, scheduler, kTaskIsPumpingField, ZR_TRUE);
    while (zr_vm_task_scheduler_step_internal(state, scheduler)) {
        executed++;
    }
    zr_vm_task_set_bool_field(state, scheduler, kTaskIsPumpingField, ZR_FALSE);
    return executed;
}

static TZrBool zr_vm_task_create_async_handle(SZrState *state,
                                              SZrObject *scheduler,
                                              const SZrTypeValue *callable,
                                              SZrTypeValue *result) {
    SZrObject *handle;
    SZrTypeValue schedulerValue;

    if (state == ZR_NULL || scheduler == ZR_NULL || callable == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    handle = ZrLib_Type_NewInstance(state, "Async");
    if (handle == ZR_NULL) {
        handle = ZrLib_Object_New(state);
        if (handle == ZR_NULL) {
            return ZR_FALSE;
        }
    }

    zr_vm_task_set_int_field(state, handle, kTaskStatusField, ZR_VM_TASK_STATUS_CREATED);
    zr_vm_task_set_value_field(state, handle, kTaskCallableField, callable);
    zr_vm_task_set_null_field(state, handle, kTaskResultField);
    zr_vm_task_set_null_field(state, handle, kTaskErrorField);
    ZrLib_Value_SetObject(state, &schedulerValue, scheduler, ZR_VALUE_TYPE_OBJECT);
    zr_vm_task_set_value_field(state, handle, kTaskSchedulerOwnerField, &schedulerValue);
    return zr_vm_task_finish_object(state, result, handle);
}

static TZrBool zr_vm_task_wait_for_handle(SZrState *state, SZrObject *handle, SZrTypeValue *result) {
    TZrInt64 status;
    SZrObject *scheduler;
    TZrBool autoCoroutine;
    TZrBool isPumping;
    const SZrTypeValue *value;

    if (state == ZR_NULL || handle == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    status = zr_vm_task_get_int_field(state, handle, kTaskStatusField, ZR_VM_TASK_STATUS_CREATED);
    if (status == ZR_VM_TASK_STATUS_COMPLETED) {
        value = zr_vm_task_get_field_value(state, handle, kTaskResultField);
        if (value != ZR_NULL) {
            ZrCore_Value_Copy(state, result, value);
        } else {
            ZrLib_Value_SetNull(result);
        }
        return ZR_TRUE;
    }
    if (status == ZR_VM_TASK_STATUS_FAULTED) {
        zr_vm_task_raise_fault(state, zr_vm_task_get_field_value(state, handle, kTaskErrorField));
    }

    scheduler = zr_vm_task_get_object_field(state, handle, kTaskSchedulerOwnerField);
    if (scheduler == ZR_NULL) {
        scheduler = zr_vm_task_main_scheduler(state);
    }
    autoCoroutine = scheduler != ZR_NULL && zr_vm_task_get_bool_field(state, scheduler, kTaskAutoCoroutineField, ZR_TRUE);
    isPumping = scheduler != ZR_NULL && zr_vm_task_get_bool_field(state, scheduler, kTaskIsPumpingField, ZR_FALSE);

    if (scheduler != ZR_NULL && autoCoroutine && !isPumping) {
        zr_vm_task_scheduler_pump_internal(state, scheduler);
        return zr_vm_task_wait_for_handle(state, handle, result);
    }

    if (isPumping) {
        ZrCore_Debug_RunError(state, "Task is still pending on an active scheduler frame");
    }

    ZrCore_Debug_RunError(state,
                          "Task is still pending while autoCoroutine is disabled; call Scheduler.pump() first");
}

static TZrBool zr_vm_task_current_scheduler(ZrLibCallContext *context, SZrTypeValue *result) {
    return zr_vm_task_finish_object(context->state, result, zr_vm_task_main_scheduler(context->state));
}

static TZrBool zr_vm_task_spawn_on_scheduler(ZrLibCallContext *context,
                                             SZrTypeValue *result,
                                             SZrObject *scheduler) {
    SZrTypeValue *callable = ZR_NULL;
    SZrObject *queue;

    if (context == ZR_NULL || result == ZR_NULL || scheduler == ZR_NULL ||
        !ZrLib_CallContext_ReadFunction(context, 0, &callable)) {
        return ZR_FALSE;
    }

    if (!zr_vm_task_create_async_handle(context->state, scheduler, callable, result) ||
        result->type != ZR_VALUE_TYPE_OBJECT || result->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    queue = zr_vm_task_scheduler_queue(context->state, scheduler);
    if (queue == ZR_NULL || !ZrLib_Array_PushValue(context->state, queue, result)) {
        return ZR_FALSE;
    }

    zr_vm_task_set_int_field(context->state,
                             ZR_CAST_OBJECT(context->state, result->value.object),
                             kTaskStatusField,
                             ZR_VM_TASK_STATUS_QUEUED);
    if (zr_vm_task_get_bool_field(context->state, scheduler, kTaskAutoCoroutineField, ZR_TRUE) &&
        !zr_vm_task_get_bool_field(context->state, scheduler, kTaskIsPumpingField, ZR_FALSE)) {
        zr_vm_task_scheduler_pump_internal(context->state, scheduler);
    }

    return ZR_TRUE;
}

static TZrBool zr_vm_task_spawn(ZrLibCallContext *context, SZrTypeValue *result) {
    return zr_vm_task_spawn_on_scheduler(context, result, zr_vm_task_main_scheduler(context->state));
}

static TZrBool zr_vm_task_spawn_thread(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrBool supportMultithread;
    SZrTypeValue errorValue;

    if (context == ZR_NULL || context->state == ZR_NULL) {
        return ZR_FALSE;
    }

    supportMultithread = zr_vm_task_default_support_multithread(context->state);
    if (!supportMultithread) {
        ZrLib_Value_SetString(context->state, &errorValue, "spawnThread requires supportMultithread = true");
        if (!ZrCore_Exception_NormalizeThrownValue(context->state,
                                                  &errorValue,
                                                  context->state->callInfoList,
                                                  ZR_THREAD_STATUS_EXCEPTION_ERROR) &&
            !ZrCore_Exception_NormalizeStatus(context->state, ZR_THREAD_STATUS_EXCEPTION_ERROR)) {
            ZrCore_Debug_RunError(context->state, "spawnThread requires supportMultithread = true");
        }
        context->state->threadStatus = context->state->currentExceptionStatus != ZR_THREAD_STATUS_FINE
                                               ? context->state->currentExceptionStatus
                                               : ZR_THREAD_STATUS_EXCEPTION_ERROR;
        return ZR_FALSE;
    }

    return zr_vm_task_spawn_on_scheduler(context, result, zr_vm_task_worker_scheduler(context->state));
}

static TZrBool zr_vm_task_await(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *handle;

    if (context == ZR_NULL || result == ZR_NULL || !ZrLib_CallContext_ReadObject(context, 0, &handle)) {
        return ZR_FALSE;
    }

    return zr_vm_task_wait_for_handle(context->state, handle, result);
}

static TZrBool zr_vm_task_yield_now(ZrLibCallContext *context, SZrTypeValue *result) {
    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_vm_task_scheduler_step_internal(context->state, zr_vm_task_main_scheduler(context->state));
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

static TZrBool zr_vm_task_sleep(ZrLibCallContext *context, SZrTypeValue *result) {
    ZR_UNUSED_PARAMETER(context);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

static TZrBool zr_vm_task_async_result(ZrLibCallContext *context, SZrTypeValue *result) {
    return zr_vm_task_wait_for_handle(context->state, zr_vm_task_self_object(context), result);
}

static TZrBool zr_vm_task_async_is_completed(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_vm_task_self_object(context);
    TZrInt64 status;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    status = zr_vm_task_get_int_field(context->state, self, kTaskStatusField, ZR_VM_TASK_STATUS_CREATED);
    ZrLib_Value_SetBool(context->state,
                        result,
                        (TZrBool)(status == ZR_VM_TASK_STATUS_COMPLETED || status == ZR_VM_TASK_STATUS_FAULTED));
    return ZR_TRUE;
}

static TZrBool zr_vm_task_scheduler_pump(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrLib_Value_SetInt(context->state,
                       result,
                       zr_vm_task_scheduler_pump_internal(context->state, zr_vm_task_self_object(context)));
    return ZR_TRUE;
}

static TZrBool zr_vm_task_scheduler_step(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrLib_Value_SetBool(context->state,
                        result,
                        zr_vm_task_scheduler_step_internal(context->state, zr_vm_task_self_object(context)));
    return ZR_TRUE;
}

static TZrBool zr_vm_task_scheduler_set_auto(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = zr_vm_task_self_object(context);
    TZrBool autoCoroutine;

    if (self == ZR_NULL || result == ZR_NULL || !ZrLib_CallContext_ReadBool(context, 0, &autoCoroutine)) {
        return ZR_FALSE;
    }

    zr_vm_task_set_bool_field(context->state, self, kTaskAutoCoroutineField, autoCoroutine);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

static TZrBool zr_vm_task_scheduler_get_auto(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrLib_Value_SetBool(context->state,
                        result,
                        zr_vm_task_get_bool_field(context->state,
                                                  zr_vm_task_self_object(context),
                                                  kTaskAutoCoroutineField,
                                                  ZR_TRUE));
    return ZR_TRUE;
}

static const ZrLibFunctionDescriptor g_task_functions[] = {
        {"spawn", 1, 1, zr_vm_task_spawn, "Async", "Queue a callable on the current scheduler.", ZR_NULL, 0},
        {"spawnThread", 1, 1, zr_vm_task_spawn_thread, "Async",
         "Queue a callable on the worker scheduler when multithreading is enabled.", ZR_NULL, 0},
        {"currentScheduler", 0, 0, zr_vm_task_current_scheduler, "Scheduler",
         "Return the current isolate scheduler.", ZR_NULL, 0},
        {"await", 1, 1, zr_vm_task_await, "value", "Await an Async handle and return its completion value.", ZR_NULL,
         0},
        {"yieldNow", 0, 0, zr_vm_task_yield_now, "null", "Step the scheduler once.", ZR_NULL, 0},
        {"sleep", 1, 1, zr_vm_task_sleep, "null", "Placeholder sleep primitive for the task runtime.", ZR_NULL, 0},
};

static const ZrLibMethodDescriptor g_async_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("result", 0, 0, zr_vm_task_async_result, "value",
                                      "Resolve the async handle and return its result.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("isCompleted", 0, 0, zr_vm_task_async_is_completed, "bool",
                                      "Return whether the async handle has completed or faulted.", ZR_FALSE, ZR_NULL,
                                      0),
};

static const ZrLibMethodDescriptor g_scheduler_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("pump", 0, 0, zr_vm_task_scheduler_pump, "int",
                                      "Drain the scheduler queue and return the number of executed tasks.", ZR_FALSE,
                                      ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("step", 0, 0, zr_vm_task_scheduler_step, "bool",
                                      "Execute a single queued task.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("setAutoCoroutine", 1, 1, zr_vm_task_scheduler_set_auto, "null",
                                      "Enable or disable automatic coroutine pumping.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("getAutoCoroutine", 0, 0, zr_vm_task_scheduler_get_auto, "bool",
                                      "Return the scheduler autoCoroutine flag.", ZR_FALSE, ZR_NULL, 0),
};

static const ZrLibGenericParameterDescriptor g_task_single_generic_parameter[] = {
        {
                .name = "T",
                .documentation = "The wrapped task payload type.",
        },
};

static const ZrLibTypeDescriptor g_task_types[] = {
        ZR_LIB_TYPE_DESCRIPTOR_INIT("Async", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, g_async_methods,
                                    ZR_ARRAY_COUNT(g_async_methods), ZR_NULL, 0,
                                    "Task handle representing queued or completed async work.", ZR_NULL, ZR_NULL, 0,
                                    ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL,
                                    g_task_single_generic_parameter, ZR_ARRAY_COUNT(g_task_single_generic_parameter)),
        ZR_LIB_TYPE_DESCRIPTOR_INIT("Scheduler", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, g_scheduler_methods,
                                    ZR_ARRAY_COUNT(g_scheduler_methods), ZR_NULL, 0,
                                    "Cooperative task scheduler for the current isolate.", ZR_NULL, ZR_NULL, 0,
                                    ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0),
        ZR_LIB_TYPE_DESCRIPTOR_INIT("Channel", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, 0,
                                    "Placeholder channel wrapper for the concurrency runtime.", ZR_NULL, ZR_NULL, 0,
                                    ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL,
                                    g_task_single_generic_parameter, ZR_ARRAY_COUNT(g_task_single_generic_parameter)),
        ZR_LIB_TYPE_DESCRIPTOR_INIT("Mutex", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, 0,
                                    "Placeholder mutex wrapper for the concurrency runtime.", ZR_NULL, ZR_NULL, 0,
                                    ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL,
                                    g_task_single_generic_parameter, ZR_ARRAY_COUNT(g_task_single_generic_parameter)),
        ZR_LIB_TYPE_DESCRIPTOR_INIT("Shared", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, 0,
                                    "Shared wrapper placeholder for future cross-isolate safe values.", ZR_NULL,
                                    ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL,
                                    g_task_single_generic_parameter, ZR_ARRAY_COUNT(g_task_single_generic_parameter)),
        ZR_LIB_TYPE_DESCRIPTOR_INIT("Transfer", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, 0,
                                    "Transfer wrapper placeholder for move-only isolate handoff.", ZR_NULL, ZR_NULL,
                                    0, ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL,
                                    g_task_single_generic_parameter, ZR_ARRAY_COUNT(g_task_single_generic_parameter)),
        ZR_LIB_TYPE_DESCRIPTOR_INIT("WeakShared", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL,
                                    0, "Weak shared wrapper placeholder for future shared arena handles.", ZR_NULL,
                                    ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL,
                                    g_task_single_generic_parameter, ZR_ARRAY_COUNT(g_task_single_generic_parameter)),
        ZR_LIB_TYPE_DESCRIPTOR_INIT("AtomicBool", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, 0,
                                    "Placeholder atomic bool wrapper.", ZR_NULL, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL,
                                    ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0),
        ZR_LIB_TYPE_DESCRIPTOR_INIT("AtomicInt", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, 0,
                                    "Placeholder atomic int wrapper.", ZR_NULL, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL,
                                    ZR_FALSE, ZR_FALSE, ZR_NULL, g_task_single_generic_parameter,
                                    ZR_ARRAY_COUNT(g_task_single_generic_parameter)),
        ZR_LIB_TYPE_DESCRIPTOR_INIT("AtomicUInt", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL,
                                    0, "Placeholder atomic uint wrapper.", ZR_NULL, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL,
                                    ZR_FALSE, ZR_FALSE, ZR_NULL, g_task_single_generic_parameter,
                                    ZR_ARRAY_COUNT(g_task_single_generic_parameter)),
};

static const ZrLibTypeHintDescriptor g_task_hints[] = {
        {"spawn", "function", "spawn(fn: function): Async<T>", "Queue a callable on the local scheduler."},
        {"spawnThread", "function", "spawnThread(fn: function): Async<T>",
         "Queue a callable on the worker scheduler when multithreading is enabled."},
        {"currentScheduler", "function", "currentScheduler(): Scheduler",
         "Return the current isolate scheduler."},
        {"await", "function", "await(handle: Async<T>): T", "Await an async handle."},
        {"Async", "type", "class Async<T>", "Task handle representing async work."},
        {"Scheduler", "type", "class Scheduler", "Cooperative scheduler wrapper."},
        {"Channel", "type", "class Channel<T>", "Task channel wrapper."},
        {"Mutex", "type", "class Mutex<T>", "Task mutex wrapper."},
        {"Shared", "type", "class Shared<T>", "Shared-value wrapper placeholder."},
        {"Transfer", "type", "class Transfer<T>", "Transfer wrapper placeholder."},
        {"WeakShared", "type", "class WeakShared<T>", "Weak shared wrapper placeholder."},
        {"AtomicBool", "type", "class AtomicBool", "Atomic bool wrapper."},
        {"AtomicInt", "type", "class AtomicInt<T>", "Atomic int wrapper."},
        {"AtomicUInt", "type", "class AtomicUInt<T>", "Atomic uint wrapper."},
};

static const TZrChar g_task_hints_json[] =
        "{\n"
        "  \"schema\": \"zr.native.hints/v1\",\n"
        "  \"module\": \"zr.task\"\n"
        "}\n";

static const ZrLibModuleDescriptor g_task_descriptor = {
        ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        "zr.task",
        ZR_NULL,
        0,
        g_task_functions,
        ZR_ARRAY_COUNT(g_task_functions),
        g_task_types,
        ZR_ARRAY_COUNT(g_task_types),
        g_task_hints,
        ZR_ARRAY_COUNT(g_task_hints),
        g_task_hints_json,
        "Cooperative async, coroutine, and scheduler primitives for zr_vm.",
        ZR_NULL,
        0,
        "1.0.0",
        ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
        0,
};

const ZrLibModuleDescriptor *ZrVmTask_Runtime_GetModuleDescriptor(void) {
    return &g_task_descriptor;
}
