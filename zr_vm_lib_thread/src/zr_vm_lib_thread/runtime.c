#include "runtime_internal.h"

#include <string.h>

#include "zr_vm_core/debug.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/task_runtime.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/project.h"

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

typedef struct ZrVmTaskExecuteRequest {
    const SZrTypeValue *callable;
    SZrTypeValue result;
    TZrBool completed;
} ZrVmTaskExecuteRequest;

static const TZrChar *kTaskMainSchedulerField = "__zr_task_scheduler";
static const TZrChar *kTaskQueueField = "__zr_task_queue";
static const TZrChar *kTaskQueueHeadField = "__zr_task_queue_head";
static const TZrChar *kTaskAutoCoroutineField = "__zr_task_auto_coroutine";
static const TZrChar *kTaskSupportMultithreadField = "__zr_task_support_multithread";
static const TZrChar *kTaskIsPumpingField = "__zr_task_is_pumping";
static const TZrChar *kTaskSchedulerRuntimeField = "__zr_task_scheduler_runtime";
static const TZrChar *kTaskPendingWorkersField = "__zr_task_pending_workers";
static const TZrChar *kTaskLastWorkerIsolateIdField = "__zr_task_last_worker_isolate_id";
static const TZrChar *kTaskStatusField = "__zr_task_status";
static const TZrChar *kTaskCallableField = "__zr_task_callable";
static const TZrChar *kTaskResultField = "__zr_task_result";
static const TZrChar *kTaskErrorField = "__zr_task_error";
static const TZrChar *kTaskSchedulerOwnerField = "__zr_task_scheduler_owner";
static const TZrChar *kTaskRunnerCallableField = "__zr_task_runner_callable";
static const TZrChar *kTaskRunnerStartedField = "__zr_task_runner_started";
static const TZrChar *kThreadObjectSchedulerField = "__zr_thread_scheduler";
static const TZrUInt32 kTaskSchedulerExternalWaitMs = 1u;

static TZrBool zr_vm_task_spawn_on_scheduler(ZrLibCallContext *context,
                                             SZrTypeValue *result,
                                             SZrObject *scheduler);

SZrObject *zr_vm_task_self_object(const ZrLibCallContext *context) {
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

TZrBool zr_vm_task_finish_object(SZrState *state, SZrTypeValue *result, SZrObject *object) {
    if (state == ZR_NULL || result == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(state, result, object, zr_vm_task_value_type_for_object(object));
    return ZR_TRUE;
}

void zr_vm_task_set_value_field(SZrState *state,
                                SZrObject *object,
                                const TZrChar *fieldName,
                                const SZrTypeValue *value) {
    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL) {
        return;
    }

    ZrLib_Object_SetFieldCString(state, object, fieldName, value);
}

void zr_vm_task_set_null_field(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    SZrTypeValue value;
    ZrLib_Value_SetNull(&value);
    zr_vm_task_set_value_field(state, object, fieldName, &value);
}

void zr_vm_task_set_bool_field(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrBool value) {
    SZrTypeValue fieldValue;
    ZrLib_Value_SetBool(state, &fieldValue, value);
    zr_vm_task_set_value_field(state, object, fieldName, &fieldValue);
}

void zr_vm_task_set_int_field(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrInt64 value) {
    SZrTypeValue fieldValue;
    ZrLib_Value_SetInt(state, &fieldValue, value);
    zr_vm_task_set_value_field(state, object, fieldName, &fieldValue);
}

void zr_vm_task_set_uint_field(SZrState *state,
                               SZrObject *object,
                               const TZrChar *fieldName,
                               TZrUInt64 value) {
    SZrTypeValue fieldValue;
    ZrCore_Value_InitAsUInt(state, &fieldValue, value);
    zr_vm_task_set_value_field(state, object, fieldName, &fieldValue);
}

const SZrTypeValue *zr_vm_task_get_field_value(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrLib_Object_GetFieldCString(state, object, fieldName);
}

SZrObject *zr_vm_task_get_object_field(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    const SZrTypeValue *value = zr_vm_task_get_field_value(state, object, fieldName);
    if (value == ZR_NULL || (value->type != ZR_VALUE_TYPE_OBJECT && value->type != ZR_VALUE_TYPE_ARRAY) ||
        value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, value->value.object);
}

TZrBool zr_vm_task_get_bool_field(SZrState *state,
                                  SZrObject *object,
                                  const TZrChar *fieldName,
                                  TZrBool defaultValue) {
    const SZrTypeValue *value = zr_vm_task_get_field_value(state, object, fieldName);
    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_BOOL) {
        return defaultValue;
    }

    return value->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE;
}

TZrInt64 zr_vm_task_get_int_field(SZrState *state,
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

TZrUInt64 zr_vm_task_get_uint_field(SZrState *state,
                                    SZrObject *object,
                                    const TZrChar *fieldName,
                                    TZrUInt64 defaultValue) {
    const SZrTypeValue *value = zr_vm_task_get_field_value(state, object, fieldName);
    if (value == ZR_NULL) {
        return defaultValue;
    }

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        return value->value.nativeObject.nativeUInt64;
    }
    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type) && value->value.nativeObject.nativeInt64 >= 0) {
        return (TZrUInt64)value->value.nativeObject.nativeInt64;
    }
    return defaultValue;
}

TZrBool zr_vm_task_copy_value_or_null(SZrState *state, const SZrTypeValue *value, SZrTypeValue *result) {
    if (result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (state != ZR_NULL && value != ZR_NULL) {
        ZrCore_Value_Copy(state, result, value);
    } else {
        ZrLib_Value_SetNull(result);
    }
    return ZR_TRUE;
}

SZrObject *zr_vm_task_root_object(SZrState *state) {
    if (state == ZR_NULL || state->global == ZR_NULL || state->global->zrObject.type != ZR_VALUE_TYPE_OBJECT ||
        state->global->zrObject.value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, state->global->zrObject.value.object);
}

TZrBool zr_vm_task_default_support_multithread(SZrState *state) {
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

TZrUInt64 zr_vm_task_next_worker_isolate_id(void) {
    static TZrUInt64 nextId = 0x1000u;

    nextId++;
    return nextId;
}

static ZrVmTaskSchedulerRuntime *zr_vm_task_scheduler_alloc_runtime(TZrUInt64 isolateId) {
    ZrVmTaskSchedulerRuntime *runtime = (ZrVmTaskSchedulerRuntime *)malloc(sizeof(*runtime));

    if (runtime == ZR_NULL) {
        return ZR_NULL;
    }

    memset(runtime, 0, sizeof(*runtime));
    zr_vm_task_sync_mutex_init(&runtime->mutex);
    zr_vm_task_sync_condition_init(&runtime->condition);
    runtime->isolateId = isolateId;
    return runtime;
}

void zr_vm_task_scheduler_signal_runtime(ZrVmTaskSchedulerRuntime *runtime) {
    if (runtime == ZR_NULL) {
        return;
    }

    zr_vm_task_sync_mutex_lock(&runtime->mutex);
    zr_vm_task_sync_condition_signal(&runtime->condition);
    zr_vm_task_sync_mutex_unlock(&runtime->mutex);
}

ZrVmTaskSchedulerRuntime *zr_vm_task_scheduler_get_runtime(SZrState *state, SZrObject *scheduler) {
    const SZrTypeValue *runtimeValue;
    ZrVmTaskSchedulerRuntime *runtime;
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || scheduler == ZR_NULL) {
        return ZR_NULL;
    }

    runtimeValue = zr_vm_task_get_field_value(state, scheduler, kTaskSchedulerRuntimeField);
    if (runtimeValue != ZR_NULL && runtimeValue->type == ZR_VALUE_TYPE_NATIVE_POINTER &&
        runtimeValue->value.nativeObject.nativePointer != ZR_NULL) {
        return (ZrVmTaskSchedulerRuntime *)runtimeValue->value.nativeObject.nativePointer;
    }

    runtime = zr_vm_task_scheduler_alloc_runtime(state->global != ZR_NULL ? state->global->hashSeed : 0u);
    if (runtime == ZR_NULL) {
        return ZR_NULL;
    }

    ZrLib_Value_SetNativePointer(state, &fieldValue, runtime);
    zr_vm_task_set_value_field(state, scheduler, kTaskSchedulerRuntimeField, &fieldValue);
    return runtime;
}

void zr_vm_task_record_last_worker_isolate(SZrState *state, TZrUInt64 isolateId) {
    SZrObject *rootObject = zr_vm_task_root_object(state);

    if (rootObject != ZR_NULL) {
        zr_vm_task_set_uint_field(state, rootObject, kTaskLastWorkerIsolateIdField, isolateId);
    }
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

    scheduler = zr_vm_task_new_typed_object(state, "Scheduler");
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
    zr_vm_task_scheduler_get_runtime(state, scheduler);
    return scheduler;
}

SZrObject *zr_vm_task_main_scheduler(SZrState *state) {
    return zr_vm_task_ensure_scheduler_with_field(state, kTaskMainSchedulerField);
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

static void zr_vm_task_scheduler_pending_worker_remove(SZrState *state, SZrObject *scheduler, SZrObject *handle) {
    SZrObject *pendingArray;
    TZrSize index;
    SZrTypeValue key;
    SZrTypeValue nullValue;

    if (state == ZR_NULL || scheduler == ZR_NULL || handle == ZR_NULL) {
        return;
    }

    pendingArray = zr_vm_task_get_object_field(state, scheduler, kTaskPendingWorkersField);
    if (pendingArray == ZR_NULL || pendingArray->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return;
    }

    for (index = 0; index < ZrLib_Array_Length(pendingArray); index++) {
        const SZrTypeValue *value = ZrLib_Array_Get(state, pendingArray, index);
        if (value != ZR_NULL && value->type == ZR_VALUE_TYPE_OBJECT &&
            value->value.object == ZR_CAST_RAW_OBJECT_AS_SUPER(handle)) {
            ZrLib_Value_SetNull(&nullValue);
            ZrCore_Value_InitAsInt(state, &key, (TZrInt64)index);
            ZrCore_Object_SetValue(state, pendingArray, &key, &nullValue);
            return;
        }
    }
}

static TZrBool zr_vm_task_scheduler_has_pending_workers(SZrState *state, SZrObject *scheduler) {
    SZrObject *pendingArray;
    TZrSize index;

    if (state == ZR_NULL || scheduler == ZR_NULL) {
        return ZR_FALSE;
    }

    pendingArray = zr_vm_task_get_object_field(state, scheduler, kTaskPendingWorkersField);
    if (pendingArray == ZR_NULL || pendingArray->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return ZR_FALSE;
    }

    for (index = 0; index < ZrLib_Array_Length(pendingArray); index++) {
        const SZrTypeValue *value = ZrLib_Array_Get(state, pendingArray, index);
        if (value != ZR_NULL &&
            (value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY) &&
            value->value.object != ZR_NULL) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

TZrBool zr_vm_task_scheduler_process_external(SZrState *state, SZrObject *scheduler) {
    ZrVmTaskSchedulerRuntime *runtime;
    ZrVmTaskSchedulerMessage *message = ZR_NULL;
    SZrTypeValue value;

    if (state == ZR_NULL || scheduler == ZR_NULL) {
        return ZR_FALSE;
    }

    runtime = zr_vm_task_scheduler_get_runtime(state, scheduler);
    if (runtime == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_vm_task_sync_mutex_lock(&runtime->mutex);
    message = runtime->head;
    if (message != ZR_NULL) {
        runtime->head = message->next;
        if (runtime->head == ZR_NULL) {
            runtime->tail = ZR_NULL;
        }
    }
    zr_vm_task_sync_mutex_unlock(&runtime->mutex);

    if (message == ZR_NULL) {
        return ZR_FALSE;
    }

    if (message->handle != ZR_NULL) {
        if (message->kind == ZR_VM_TASK_SCHEDULER_MESSAGE_COMPLETE &&
            zr_vm_task_transport_decode_value(state, &message->payload, &value)) {
            zr_vm_task_set_value_field(state, message->handle, kTaskResultField, &value);
            zr_vm_task_set_null_field(state, message->handle, kTaskErrorField);
            zr_vm_task_set_null_field(state, message->handle, kTaskCallableField);
            zr_vm_task_set_int_field(state, message->handle, kTaskStatusField, ZR_VM_TASK_STATUS_COMPLETED);
        } else {
            if (!zr_vm_task_transport_decode_value(state, &message->payload, &value)) {
                ZrLib_Value_SetString(state, &value, "Worker task fault");
            }
            zr_vm_task_set_value_field(state, message->handle, kTaskErrorField, &value);
            zr_vm_task_set_null_field(state, message->handle, kTaskResultField);
            zr_vm_task_set_null_field(state, message->handle, kTaskCallableField);
            zr_vm_task_set_int_field(state, message->handle, kTaskStatusField, ZR_VM_TASK_STATUS_FAULTED);
        }

        zr_vm_task_scheduler_pending_worker_remove(state, scheduler, message->handle);
    }

    zr_vm_task_transport_clear(&message->payload);
    free(message);
    return ZR_TRUE;
}

TZrBool zr_vm_task_scheduler_wait_for_external(SZrState *state, SZrObject *scheduler, TZrUInt32 timeoutMs) {
    ZrVmTaskSchedulerRuntime *runtime;
    TZrBool hasMessage;

    if (state == ZR_NULL || scheduler == ZR_NULL) {
        return ZR_FALSE;
    }

    runtime = zr_vm_task_scheduler_get_runtime(state, scheduler);
    if (runtime == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_vm_task_sync_mutex_lock(&runtime->mutex);
    hasMessage = runtime->head != ZR_NULL ? ZR_TRUE : ZR_FALSE;
    zr_vm_task_sync_mutex_unlock(&runtime->mutex);
    if (!hasMessage && timeoutMs > 0u) {
        /* Keep the external wait path Helgrind-clean by polling the queue with a short sleep. */
        zr_vm_task_sync_sleep_ms(timeoutMs);
        zr_vm_task_sync_mutex_lock(&runtime->mutex);
        hasMessage = runtime->head != ZR_NULL ? ZR_TRUE : ZR_FALSE;
        zr_vm_task_sync_mutex_unlock(&runtime->mutex);
    }
    return hasMessage;
}

TZrBool zr_vm_task_raise_runtime_error(SZrState *state, const TZrChar *message) {
    SZrTypeValue errorValue;

    if (state == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetString(state, &errorValue, message != ZR_NULL ? message : "Task runtime error");
    if (!ZrCore_Exception_NormalizeThrownValue(state,
                                               &errorValue,
                                               state->callInfoList,
                                               ZR_THREAD_STATUS_EXCEPTION_ERROR) &&
        !ZrCore_Exception_NormalizeStatus(state, ZR_THREAD_STATUS_EXCEPTION_ERROR)) {
        ZrCore_Debug_RunError(state, (TZrNativeString)(message != ZR_NULL ? message : "Task runtime error"));
    }

    state->threadStatus = state->currentExceptionStatus != ZR_THREAD_STATUS_FINE
                                  ? state->currentExceptionStatus
                                  : ZR_THREAD_STATUS_EXCEPTION_ERROR;
    return ZR_FALSE;
}

TZrBool zr_vm_task_require_multithread(SZrState *state, const TZrChar *message) {
    if (zr_vm_task_default_support_multithread(state)) {
        return ZR_TRUE;
    }

    return zr_vm_task_raise_runtime_error(state, message);
}

SZrObject *zr_vm_task_new_typed_object(SZrState *state, const TZrChar *typeName) {
    SZrObject *object;
    const SZrTypeValue *typeValue;
    SZrObjectPrototype *prototype = ZR_NULL;

    if (state == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    typeValue = ZrLib_Module_GetExport(state, "zr.thread", typeName);
    if (typeValue != ZR_NULL && typeValue->type == ZR_VALUE_TYPE_OBJECT && typeValue->value.object != ZR_NULL) {
        SZrObject *typeObject = ZR_CAST_OBJECT(state, typeValue->value.object);
        if (typeObject != ZR_NULL && typeObject->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE) {
            prototype = (SZrObjectPrototype *)typeObject;
        }
    }

    object = prototype != ZR_NULL ? ZrLib_Type_NewInstanceWithPrototype(state, prototype)
                                  : ZrLib_Type_NewInstance(state, typeName);
    if (object == ZR_NULL) {
        object = ZrLib_Object_New(state);
    }
    return object;
}

static SZrObject *zr_vm_thread_new_task_object(SZrState *state) {
    SZrObject *object;
    const SZrTypeValue *typeValue;
    SZrObjectPrototype *prototype = ZR_NULL;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    typeValue = ZrLib_Module_GetExport(state, "zr.task", "Task");
    if (typeValue != ZR_NULL && typeValue->type == ZR_VALUE_TYPE_OBJECT && typeValue->value.object != ZR_NULL) {
        SZrObject *typeObject = ZR_CAST_OBJECT(state, typeValue->value.object);
        if (typeObject != ZR_NULL && typeObject->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE) {
            prototype = (SZrObjectPrototype *)typeObject;
        }
    }

    object = prototype != ZR_NULL ? ZrLib_Type_NewInstanceWithPrototype(state, prototype)
                                  : ZrLib_Type_NewInstance(state, "Task");
    if (object == ZR_NULL) {
        object = ZrLib_Object_New(state);
    }
    return object;
}

SZrObject *zr_vm_task_resolve_construct_target(ZrLibCallContext *context) {
    SZrObject *self;
    SZrObjectPrototype *ownerPrototype;
    SZrObjectPrototype *targetPrototype;

    if (context == ZR_NULL || context->state == ZR_NULL) {
        return ZR_NULL;
    }

    self = zr_vm_task_self_object(context);
    ownerPrototype = ZrLib_CallContext_OwnerPrototype(context);
    if (self != ZR_NULL && ownerPrototype != ZR_NULL && ZrCore_Object_IsInstanceOfPrototype(self, ownerPrototype)) {
        return self;
    }

    targetPrototype = ZrLib_CallContext_GetConstructTargetPrototype(context);
    if (targetPrototype != ZR_NULL) {
        return ZrLib_Type_NewInstanceWithPrototype(context->state, targetPrototype);
    }

    if (ownerPrototype != ZR_NULL) {
        return ZrLib_Type_NewInstanceWithPrototype(context->state, ownerPrototype);
    }

    return ZR_NULL;
}

TZrBool zr_vm_task_is_integer_value(const SZrTypeValue *value) {
    return value != ZR_NULL && (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type));
}

TZrBool zr_vm_task_read_strict_int(const ZrLibCallContext *context, TZrSize index, TZrInt64 *outValue) {
    SZrTypeValue *value = ZrLib_CallContext_Argument(context, index);

    if (value == ZR_NULL) {
        ZrLib_CallContext_RaiseArityError(context, (TZrUInt16)(index + 1), UINT16_MAX);
    }
    if (!zr_vm_task_is_integer_value(value)) {
        ZrLib_CallContext_RaiseTypeError(context, index, "int");
    }

    if (outValue != ZR_NULL) {
        *outValue = ZR_VALUE_IS_TYPE_SIGNED_INT(value->type) ? value->value.nativeObject.nativeInt64
                                                             : (TZrInt64)value->value.nativeObject.nativeUInt64;
    }
    return ZR_TRUE;
}

TZrBool zr_vm_task_read_strict_uint(const ZrLibCallContext *context, TZrSize index, TZrUInt64 *outValue) {
    SZrTypeValue *value = ZrLib_CallContext_Argument(context, index);

    if (value == ZR_NULL) {
        ZrLib_CallContext_RaiseArityError(context, (TZrUInt16)(index + 1), UINT16_MAX);
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        if (outValue != ZR_NULL) {
            *outValue = value->value.nativeObject.nativeUInt64;
        }
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type) && value->value.nativeObject.nativeInt64 >= 0) {
        if (outValue != ZR_NULL) {
            *outValue = (TZrUInt64)value->value.nativeObject.nativeInt64;
        }
        return ZR_TRUE;
    }

    ZrLib_CallContext_RaiseTypeError(context, index, "uint");
}

TZrBool zr_vm_task_value_equals(const SZrTypeValue *lhs, const SZrTypeValue *rhs) {
    if (lhs == ZR_NULL || rhs == ZR_NULL) {
        return ZR_FALSE;
    }
    if (lhs->type == ZR_VALUE_TYPE_NULL || rhs->type == ZR_VALUE_TYPE_NULL) {
        return lhs->type == rhs->type;
    }
    if (lhs->type == ZR_VALUE_TYPE_BOOL && rhs->type == ZR_VALUE_TYPE_BOOL) {
        return lhs->value.nativeObject.nativeBool == rhs->value.nativeObject.nativeBool;
    }
    if (zr_vm_task_is_integer_value(lhs) && zr_vm_task_is_integer_value(rhs)) {
        if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(lhs->type) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(rhs->type)) {
            TZrInt64 lhsSigned = ZR_VALUE_IS_TYPE_SIGNED_INT(lhs->type) ? lhs->value.nativeObject.nativeInt64 : 0;
            TZrInt64 rhsSigned = ZR_VALUE_IS_TYPE_SIGNED_INT(rhs->type) ? rhs->value.nativeObject.nativeInt64 : 0;
            if ((ZR_VALUE_IS_TYPE_SIGNED_INT(lhs->type) && lhsSigned < 0) ||
                (ZR_VALUE_IS_TYPE_SIGNED_INT(rhs->type) && rhsSigned < 0)) {
                return ZR_FALSE;
            }
            return (ZR_VALUE_IS_TYPE_UNSIGNED_INT(lhs->type) ? lhs->value.nativeObject.nativeUInt64 : (TZrUInt64)lhsSigned) ==
                   (ZR_VALUE_IS_TYPE_UNSIGNED_INT(rhs->type) ? rhs->value.nativeObject.nativeUInt64 : (TZrUInt64)rhsSigned);
        }
        return lhs->value.nativeObject.nativeInt64 == rhs->value.nativeObject.nativeInt64;
    }
    return lhs->type == rhs->type && lhs->value.object == rhs->value.object;
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

    if (zr_vm_task_scheduler_process_external(state, scheduler)) {
        return ZR_TRUE;
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
        TZrBool hasPendingWorkers = zr_vm_task_scheduler_has_pending_workers(state, scheduler);
        TZrBool waited = ZR_FALSE;
        TZrBool processedExternal = ZR_FALSE;

        if (hasPendingWorkers) {
            waited = zr_vm_task_scheduler_wait_for_external(state, scheduler, kTaskSchedulerExternalWaitMs);
            processedExternal = waited ? zr_vm_task_scheduler_process_external(state, scheduler) : ZR_FALSE;
        }

        if (processedExternal) {
            return ZR_TRUE;
        }

        if (head > 0 || ZrLib_Array_Length(queue) > 0) {
            newQueue = ZrLib_Array_New(state);
            if (newQueue != ZR_NULL) {
                ZrLib_Value_SetObject(state, &queueValue, newQueue, ZR_VALUE_TYPE_ARRAY);
                zr_vm_task_set_value_field(state, scheduler, kTaskQueueField, &queueValue);
            }
        }
        zr_vm_task_set_int_field(state, scheduler, kTaskQueueHeadField, 0);
        return hasPendingWorkers;
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

    handle = zr_vm_thread_new_task_object(state);
    if (handle == ZR_NULL) {
        return ZR_FALSE;
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
        while (ZR_TRUE) {
            status = zr_vm_task_get_int_field(state, handle, kTaskStatusField, ZR_VM_TASK_STATUS_CREATED);
            if (status == ZR_VM_TASK_STATUS_COMPLETED || status == ZR_VM_TASK_STATUS_FAULTED) {
                return zr_vm_task_wait_for_handle(state, handle, result);
            }

            if (zr_vm_task_scheduler_step_internal(state, scheduler)) {
                continue;
            }

            zr_vm_task_scheduler_wait_for_external(state, scheduler, 10u);
        }
    }

    if (isPumping) {
        ZrCore_Debug_RunError(state, "Task is still pending on an active scheduler frame");
    }

    ZrCore_Debug_RunError(state,
                          "Task is still pending while autoCoroutine is disabled; call Scheduler.pump() first");
}

static SZrObject *zr_vm_thread_create_scheduler(SZrState *state) {
    SZrObject *scheduler;
    SZrObject *queue;
    SZrTypeValue queueValue;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    scheduler = zr_vm_task_new_typed_object(state, "Scheduler");
    queue = ZrLib_Array_New(state);
    if (scheduler == ZR_NULL || queue == ZR_NULL) {
        return ZR_NULL;
    }

    ZrLib_Value_SetObject(state, &queueValue, queue, ZR_VALUE_TYPE_ARRAY);
    zr_vm_task_set_value_field(state, scheduler, kTaskQueueField, &queueValue);
    zr_vm_task_set_int_field(state, scheduler, kTaskQueueHeadField, 0);
    zr_vm_task_set_bool_field(state, scheduler, kTaskAutoCoroutineField, zr_vm_task_default_auto_coroutine(state));
    zr_vm_task_set_bool_field(state, scheduler, kTaskSupportMultithreadField, zr_vm_task_default_support_multithread(state));
    zr_vm_task_set_bool_field(state, scheduler, kTaskIsPumpingField, ZR_FALSE);
    zr_vm_task_scheduler_get_runtime(state, scheduler);
    return scheduler;
}

static const SZrTypeValue *zr_vm_thread_runner_callable(SZrState *state, SZrObject *runner) {
    return zr_vm_task_get_field_value(state, runner, kTaskRunnerCallableField);
}

static TZrBool zr_vm_thread_mark_runner_started(SZrState *state, SZrObject *runner) {
    if (state == ZR_NULL || runner == ZR_NULL) {
        return ZR_FALSE;
    }
    if (zr_vm_task_get_bool_field(state, runner, kTaskRunnerStartedField, ZR_FALSE)) {
        return zr_vm_task_raise_runtime_error(state, "TaskRunner.start() can only be called once");
    }

    zr_vm_task_set_bool_field(state, runner, kTaskRunnerStartedField, ZR_TRUE);
    return ZR_TRUE;
}

static TZrBool zr_vm_task_current_scheduler(ZrLibCallContext *context, SZrTypeValue *result) {
    return zr_vm_task_finish_object(context->state, result, zr_vm_task_main_scheduler(context->state));
}

static TZrBool zr_vm_thread_scheduler_start(ZrLibCallContext *context, SZrTypeValue *result) {
    return zr_vm_task_spawn_on_scheduler(context, result, zr_vm_task_self_object(context));
}

	static TZrBool zr_vm_task_spawn_on_scheduler(ZrLibCallContext *context,
	                                             SZrTypeValue *result,
	                                             SZrObject *scheduler) {
	    SZrObject *runner;
	    SZrObject *handle;
	    const SZrTypeValue *callable;

	    if (context == ZR_NULL || result == ZR_NULL || scheduler == ZR_NULL ||
	        !zr_vm_task_require_multithread(context->state, "zr.thread requires supportMultithread = true") ||
	        !ZrLib_CallContext_ReadObject(context, 0, &runner)) {
	        return ZR_FALSE;
    }

	    callable = zr_vm_thread_runner_callable(context->state, runner);
	    if (callable == ZR_NULL) {
	        return zr_vm_task_raise_runtime_error(context->state, "Scheduler.start() expects a zr.task.TaskRunner<T>");
	    }
	    if (!zr_vm_thread_mark_runner_started(context->state, runner)) {
	        return ZR_FALSE;
	    }
    if (!zr_vm_task_create_async_handle(context->state, scheduler, callable, result) ||
        result->type != ZR_VALUE_TYPE_OBJECT || result->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    handle = ZR_CAST_OBJECT(context->state, result->value.object);
    zr_vm_task_set_int_field(context->state,
                             handle,
                             kTaskStatusField,
                             ZR_VM_TASK_STATUS_QUEUED);
    return zr_vm_task_spawn_thread_worker(context, callable, result, scheduler);
}

static TZrBool zr_vm_task_spawn(ZrLibCallContext *context, SZrTypeValue *result) {
    return zr_vm_task_spawn_on_scheduler(context, result, zr_vm_task_main_scheduler(context->state));
}

static TZrBool zr_vm_task_spawn_thread(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *threadHandle;
    SZrObject *scheduler;
    SZrTypeValue schedulerValue;

    if (context == ZR_NULL || context->state == ZR_NULL || result == ZR_NULL ||
        !zr_vm_task_require_multithread(context->state, "spawnThread requires supportMultithread = true")) {
        return ZR_FALSE;
    }

    threadHandle = zr_vm_task_new_typed_object(context->state, "Thread");
    scheduler = zr_vm_thread_create_scheduler(context->state);
    if (threadHandle == ZR_NULL || scheduler == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(context->state, &schedulerValue, scheduler, ZR_VALUE_TYPE_OBJECT);
    zr_vm_task_set_value_field(context->state, threadHandle, kThreadObjectSchedulerField, &schedulerValue);
    return zr_vm_task_finish_object(context->state, result, threadHandle);
}

static TZrBool zr_vm_thread_thread_start(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *threadHandle;
    SZrObject *scheduler;

    if (context == ZR_NULL || context->state == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    threadHandle = zr_vm_task_self_object(context);
    scheduler = zr_vm_task_get_object_field(context->state, threadHandle, kThreadObjectSchedulerField);
    if (scheduler == ZR_NULL) {
        SZrTypeValue schedulerValue;

        scheduler = zr_vm_thread_create_scheduler(context->state);
        if (scheduler == ZR_NULL) {
            return ZR_FALSE;
        }

        ZrLib_Value_SetObject(context->state, &schedulerValue, scheduler, ZR_VALUE_TYPE_OBJECT);
        zr_vm_task_set_value_field(context->state, threadHandle, kThreadObjectSchedulerField, &schedulerValue);
    }

    return zr_vm_task_spawn_on_scheduler(context, result, scheduler);
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
        {"spawnThread", 0, 0, zr_vm_task_spawn_thread, "Thread",
         "Create a worker thread launcher backed by a dedicated scheduler.", ZR_NULL, 0},
        {"getCurrentThreadScheduler", 0, 0, zr_vm_task_current_scheduler, "Scheduler",
         "Return the current isolate's zr.thread scheduler wrapper.", ZR_NULL, 0},
};

static const ZrLibMethodDescriptor g_async_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("result", 0, 0, zr_vm_task_async_result, "value",
                                      "Resolve the async handle and return its result.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("isCompleted", 0, 0, zr_vm_task_async_is_completed, "bool",
                                      "Return whether the async handle has completed or faulted.", ZR_FALSE, ZR_NULL,
                                      0),
};

static const ZrLibMethodDescriptor g_scheduler_methods[] = {
        {"start", 1, 1, zr_vm_thread_scheduler_start, "Task<T>",
         "Launch a TaskRunner on this thread scheduler.", ZR_FALSE, ZR_NULL, 0, 0U, ZR_NULL, 0},
        ZR_LIB_METHOD_DESCRIPTOR_INIT("pump", 0, 0, zr_vm_task_scheduler_pump, "int",
                                      "Drain worker completions and queued work for this scheduler.", ZR_FALSE,
                                      ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("step", 0, 0, zr_vm_task_scheduler_step, "bool",
                                      "Execute one scheduler step, including worker completions.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("setAutoCoroutine", 1, 1, zr_vm_task_scheduler_set_auto, "null",
                                      "Enable or disable automatic scheduler pumping.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("getAutoCoroutine", 0, 0, zr_vm_task_scheduler_get_auto, "bool",
                                      "Return the scheduler autoCoroutine flag.", ZR_FALSE, ZR_NULL, 0),
};

static const ZrLibMethodDescriptor g_thread_methods[] = {
        {"start", 1, 1, zr_vm_thread_thread_start, "Task<T>",
         "Launch a TaskRunner on this thread.", ZR_FALSE, ZR_NULL, 0, 0U, ZR_NULL, 0},
};

static const ZrLibMethodDescriptor g_channel_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("send", 1, 1, zr_vm_task_channel_send, "null",
                                      "Append a value to the channel queue.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("recv", 0, 0, zr_vm_task_channel_recv, "T",
                                      "Pop the next queued value, or null when the channel is empty.", ZR_FALSE,
                                      ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("close", 0, 0, zr_vm_task_channel_close, "null",
                                      "Close the channel and reject future sends.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("isClosed", 0, 0, zr_vm_task_channel_is_closed, "bool",
                                      "Return whether the channel has been closed.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("length", 0, 0, zr_vm_task_channel_length, "int",
                                      "Return the number of queued values still pending receipt.", ZR_FALSE, ZR_NULL,
                                      0),
};

static const ZrLibMethodDescriptor g_shared_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("load", 0, 0, zr_vm_task_shared_load, "T",
                                      "Return the current shared value, or null when released.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("store", 1, 1, zr_vm_task_shared_store, "null",
                                      "Replace the current shared value.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("clone", 0, 0, zr_vm_task_shared_clone, "Shared<T>",
                                      "Create another strong shared handle to the same cell.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("downgrade", 0, 0, zr_vm_task_shared_downgrade, "WeakShared<T>",
                                      "Create a weak shared handle.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("release", 0, 0, zr_vm_task_shared_release, "null",
                                      "Release this strong shared handle.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("isAlive", 0, 0, zr_vm_task_shared_is_alive, "bool",
                                      "Return whether the shared cell still has strong ownership.", ZR_FALSE, ZR_NULL,
                                      0),
};

static const ZrLibMethodDescriptor g_weak_shared_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("upgrade", 0, 0, zr_vm_task_weak_shared_upgrade, "Shared<T>",
                                      "Upgrade a weak handle to a strong shared handle when the cell is alive.",
                                      ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("isAlive", 0, 0, zr_vm_task_weak_shared_is_alive, "bool",
                                      "Return whether the referenced shared cell is still alive.", ZR_FALSE, ZR_NULL,
                                      0),
};

static const ZrLibMethodDescriptor g_transfer_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("take", 0, 0, zr_vm_task_transfer_take, "T",
                                      "Move the transfer payload out of the handle once.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("isTaken", 0, 0, zr_vm_task_transfer_is_taken, "bool",
                                      "Return whether the transfer payload has already been consumed.", ZR_FALSE,
                                      ZR_NULL, 0),
};

static const ZrLibMethodDescriptor g_mutex_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("load", 0, 0, zr_vm_task_mutex_load, "T",
                                      "Return the current mutex-protected value.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("lock", 0, 0, zr_vm_task_mutex_lock, "T",
                                      "Acquire the mutex and return the current value.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("unlock", 1, 1, zr_vm_task_mutex_unlock, "null",
                                      "Replace the value and release the mutex.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("isLocked", 0, 0, zr_vm_task_mutex_is_locked, "bool",
                                      "Return whether the mutex is currently locked.", ZR_FALSE, ZR_NULL, 0),
};

static const ZrLibMethodDescriptor g_atomic_bool_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("load", 0, 0, zr_vm_task_atomic_bool_load, "bool",
                                      "Load the current atomic bool value.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("store", 1, 1, zr_vm_task_atomic_bool_store, "null",
                                      "Store a new atomic bool value.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("compareExchange", 2, 2, zr_vm_task_atomic_bool_compare_exchange, "bool",
                                      "Compare the current value and replace it when equal.", ZR_FALSE, ZR_NULL, 0),
};

static const ZrLibMethodDescriptor g_atomic_int_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("load", 0, 0, zr_vm_task_atomic_int_load, "T",
                                      "Load the current atomic integer value.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("store", 1, 1, zr_vm_task_atomic_int_store, "null",
                                      "Store a new atomic integer value.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("compareExchange", 2, 2, zr_vm_task_atomic_int_compare_exchange, "bool",
                                      "Compare the current value and replace it when equal.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("fetchAdd", 1, 1, zr_vm_task_atomic_int_fetch_add, "T",
                                      "Add a delta and return the previous value.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("fetchSub", 1, 1, zr_vm_task_atomic_int_fetch_sub, "T",
                                      "Subtract a delta and return the previous value.", ZR_FALSE, ZR_NULL, 0),
};

static const ZrLibMethodDescriptor g_atomic_uint_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("load", 0, 0, zr_vm_task_atomic_uint_load, "T",
                                      "Load the current atomic unsigned integer value.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("store", 1, 1, zr_vm_task_atomic_uint_store, "null",
                                      "Store a new atomic unsigned integer value.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("compareExchange", 2, 2, zr_vm_task_atomic_uint_compare_exchange, "bool",
                                      "Compare the current value and replace it when equal.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("fetchAdd", 1, 1, zr_vm_task_atomic_uint_fetch_add, "T",
                                      "Add a delta and return the previous value.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("fetchSub", 1, 1, zr_vm_task_atomic_uint_fetch_sub, "T",
                                      "Subtract a delta and return the previous value.", ZR_FALSE, ZR_NULL, 0),
};

static const ZrLibMetaMethodDescriptor g_shared_meta_methods[] = {
        {ZR_META_CONSTRUCTOR, 1, 1, zr_vm_task_shared_construct, "Shared<T>",
         "Construct a shared wrapper cell from a value.", ZR_NULL, 0},
};

static const ZrLibMetaMethodDescriptor g_channel_meta_methods[] = {
        {ZR_META_CONSTRUCTOR, 0, 0, zr_vm_task_channel_construct, "Channel<T>",
         "Construct a same-isolate FIFO channel wrapper.", ZR_NULL, 0},
};

static const ZrLibMetaMethodDescriptor g_transfer_meta_methods[] = {
        {ZR_META_CONSTRUCTOR, 1, 1, zr_vm_task_transfer_construct, "Transfer<T>",
         "Construct a single-consumer transfer wrapper from a value.", ZR_NULL, 0},
};

static const ZrLibMetaMethodDescriptor g_mutex_meta_methods[] = {
        {ZR_META_CONSTRUCTOR, 1, 1, zr_vm_task_mutex_construct, "Mutex<T>",
         "Construct a mutex wrapper around a value.", ZR_NULL, 0},
};

static const ZrLibMetaMethodDescriptor g_atomic_bool_meta_methods[] = {
        {ZR_META_CONSTRUCTOR, 1, 1, zr_vm_task_atomic_bool_construct, "AtomicBool",
         "Construct an atomic bool wrapper.", ZR_NULL, 0},
};

static const ZrLibMetaMethodDescriptor g_atomic_int_meta_methods[] = {
        {ZR_META_CONSTRUCTOR, 1, 1, zr_vm_task_atomic_int_construct, "AtomicInt<T>",
         "Construct an atomic integer wrapper.", ZR_NULL, 0},
};

static const ZrLibMetaMethodDescriptor g_atomic_uint_meta_methods[] = {
        {ZR_META_CONSTRUCTOR, 1, 1, zr_vm_task_atomic_uint_construct, "AtomicUInt<T>",
         "Construct an atomic unsigned integer wrapper.", ZR_NULL, 0},
};

static const ZrLibGenericParameterDescriptor g_task_single_generic_parameter[] = {
        {
                .name = "T",
                .documentation = "The payload type carried by the task or transport handle.",
        },
};

static const TZrChar *g_scheduler_implements[] = {"IScheduler"};

static const ZrLibTypeDescriptor g_task_types[] = {
        ZR_LIB_TYPE_DESCRIPTOR_INIT("Scheduler", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, g_scheduler_methods,
                                    ZR_ARRAY_COUNT(g_scheduler_methods), ZR_NULL, 0,
                                    "Worker-backed scheduler that integrates with zr.task.Task awaiting.", ZR_NULL,
                                    g_scheduler_implements, ZR_ARRAY_COUNT(g_scheduler_implements),
                                    ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0),
        ZR_LIB_TYPE_DESCRIPTOR_INIT("Thread", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, g_thread_methods,
                                    ZR_ARRAY_COUNT(g_thread_methods), ZR_NULL, 0,
                                    "Thread launcher that owns a worker scheduler.", ZR_NULL, ZR_NULL, 0,
                                    ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0),
        ZR_LIB_TYPE_DESCRIPTOR_INIT("Channel", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, g_channel_methods,
                                    ZR_ARRAY_COUNT(g_channel_methods), g_channel_meta_methods,
                                    ZR_ARRAY_COUNT(g_channel_meta_methods),
                                    "Cross-isolate FIFO channel wrapper with scheduler wakeups.", ZR_NULL,
                                    ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE, "Channel()",
                                    g_task_single_generic_parameter, ZR_ARRAY_COUNT(g_task_single_generic_parameter)),
        ZR_LIB_TYPE_DESCRIPTOR_INIT("Shared", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, g_shared_methods,
                                    ZR_ARRAY_COUNT(g_shared_methods), g_shared_meta_methods,
                                    ZR_ARRAY_COUNT(g_shared_meta_methods),
                                    "Shared wrapper with strong and weak handle semantics for cross-isolate cells.",
                                    ZR_NULL, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE, "Shared(value: T)",
                                    g_task_single_generic_parameter, ZR_ARRAY_COUNT(g_task_single_generic_parameter)),
        ZR_LIB_TYPE_DESCRIPTOR_INIT("Transfer", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, g_transfer_methods,
                                    ZR_ARRAY_COUNT(g_transfer_methods), g_transfer_meta_methods,
                                    ZR_ARRAY_COUNT(g_transfer_meta_methods),
                                    "Single-consumer transfer wrapper used to hand off a value once.", ZR_NULL,
                                    ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE, "Transfer(value: T)",
                                    g_task_single_generic_parameter, ZR_ARRAY_COUNT(g_task_single_generic_parameter)),
        ZR_LIB_TYPE_DESCRIPTOR_INIT("WeakShared", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, g_weak_shared_methods,
                                    ZR_ARRAY_COUNT(g_weak_shared_methods), ZR_NULL, 0,
                                    "Weak shared wrapper that can upgrade back to a live Shared handle.", ZR_NULL,
                                    ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL,
                                    g_task_single_generic_parameter, ZR_ARRAY_COUNT(g_task_single_generic_parameter)),
};

static const ZrLibTypeHintDescriptor g_task_hints[] = {
        {"spawnThread", "function", "spawnThread(): Thread", "Create a worker thread launcher."},
        {"getCurrentThreadScheduler", "function", "getCurrentThreadScheduler(): Scheduler",
         "Return the current isolate's zr.thread scheduler wrapper."},
        {"Scheduler", "type", "class Scheduler implements zr.task.IScheduler",
         "Worker-backed scheduler that can start TaskRunner<T> instances."},
        {"Thread", "type", "class Thread", "Worker thread launcher with a start(runner) method."},
        {"Channel", "type", "class Channel<T>", "Cross-isolate FIFO channel wrapper."},
        {"Shared", "type", "class Shared<T>", "Shared-value wrapper with strong and weak handles."},
        {"Transfer", "type", "class Transfer<T>", "Single-consumer transfer wrapper."},
        {"WeakShared", "type", "class WeakShared<T>", "Weak shared handle that can upgrade to Shared<T>."},
};

static const TZrChar g_task_hints_json[] =
        "{\n"
        "  \"schema\": \"zr.native.hints/v1\",\n"
        "  \"module\": \"zr.thread\"\n"
        "}\n";

static const ZrLibModuleDescriptor g_task_descriptor = {
        ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        "zr.thread",
        ZR_NULL,
        0,
        g_task_functions,
        ZR_ARRAY_COUNT(g_task_functions),
        g_task_types,
        ZR_ARRAY_COUNT(g_task_types),
        g_task_hints,
        ZR_ARRAY_COUNT(g_task_hints),
        g_task_hints_json,
        "Worker thread, cross-isolate transport, and shared-handle primitives for zr_vm.",
        ZR_NULL,
        0,
        "1.0.0",
        ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
        0,
};

const ZrLibModuleDescriptor *ZrVmThread_Runtime_GetModuleDescriptor(void) {
    return &g_task_descriptor;
}
