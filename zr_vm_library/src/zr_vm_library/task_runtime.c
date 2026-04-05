#include "zr_vm_core/task_runtime.h"

#include <string.h>

#include "zr_vm_core/debug.h"
#include "zr_vm_core/execution_control.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_library/project.h"

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

typedef struct ZrVmTaskExecuteRequest {
    const SZrTypeValue *callable;
    SZrTypeValue result;
    TZrBool completed;
} ZrVmTaskExecuteRequest;

static const TZrChar *kTaskModuleName = "zr.task";
static const TZrChar *kCoroutineModuleName = "zr.coroutine";
static const TZrChar *kTaskRootCoroutineSchedulerField = "__zr_coroutine_scheduler";
static const TZrChar *kTaskQueueField = "__zr_task_queue";
static const TZrChar *kTaskQueueHeadField = "__zr_task_queue_head";
static const TZrChar *kTaskAutoCoroutineField = "__zr_task_auto_coroutine";
static const TZrChar *kTaskIsPumpingField = "__zr_task_is_pumping";
static const TZrChar *kTaskStatusField = "__zr_task_status";
static const TZrChar *kTaskCallableField = "__zr_task_callable";
static const TZrChar *kTaskResultField = "__zr_task_result";
static const TZrChar *kTaskErrorField = "__zr_task_error";
static const TZrChar *kTaskSchedulerOwnerField = "__zr_task_scheduler_owner";
static const TZrChar *kTaskRunnerCallableField = "__zr_task_runner_callable";
static const TZrChar *kTaskRunnerStartedField = "__zr_task_runner_started";

static TZrBool task_runtime_scheduler_invoke_step(SZrState *state, SZrObject *scheduler);

static SZrObject *task_runtime_self_object(const ZrLibCallContext *context) {
    SZrTypeValue *selfValue = ZrLib_CallContext_Self(context);

    if (selfValue == ZR_NULL || (selfValue->type != ZR_VALUE_TYPE_OBJECT && selfValue->type != ZR_VALUE_TYPE_ARRAY) ||
        selfValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(context->state, selfValue->value.object);
}

static EZrValueType task_runtime_value_type_for_object(SZrObject *object) {
    return object != ZR_NULL && object->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY ? ZR_VALUE_TYPE_ARRAY
                                                                                       : ZR_VALUE_TYPE_OBJECT;
}

static TZrBool task_runtime_finish_object(SZrState *state, SZrTypeValue *result, SZrObject *object) {
    if (state == ZR_NULL || result == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(state, result, object, task_runtime_value_type_for_object(object));
    return ZR_TRUE;
}

static void task_runtime_set_value_field(SZrState *state,
                                         SZrObject *object,
                                         const TZrChar *fieldName,
                                         const SZrTypeValue *value) {
    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL) {
        return;
    }

    ZrLib_Object_SetFieldCString(state, object, fieldName, value);
}

static void task_runtime_set_null_field(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    SZrTypeValue value;

    ZrLib_Value_SetNull(&value);
    task_runtime_set_value_field(state, object, fieldName, &value);
}

static void task_runtime_set_bool_field(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrBool value) {
    SZrTypeValue fieldValue;

    ZrLib_Value_SetBool(state, &fieldValue, value);
    task_runtime_set_value_field(state, object, fieldName, &fieldValue);
}

static void task_runtime_set_int_field(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrInt64 value) {
    SZrTypeValue fieldValue;

    ZrLib_Value_SetInt(state, &fieldValue, value);
    task_runtime_set_value_field(state, object, fieldName, &fieldValue);
}

static const SZrTypeValue *task_runtime_get_field_value(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrLib_Object_GetFieldCString(state, object, fieldName);
}

static SZrObject *task_runtime_get_object_field(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    const SZrTypeValue *value = task_runtime_get_field_value(state, object, fieldName);

    if (value == ZR_NULL || (value->type != ZR_VALUE_TYPE_OBJECT && value->type != ZR_VALUE_TYPE_ARRAY) ||
        value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, value->value.object);
}

static TZrBool task_runtime_get_bool_field(SZrState *state,
                                           SZrObject *object,
                                           const TZrChar *fieldName,
                                           TZrBool defaultValue) {
    const SZrTypeValue *value = task_runtime_get_field_value(state, object, fieldName);

    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_BOOL) {
        return defaultValue;
    }

    return value->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE;
}

static TZrInt64 task_runtime_get_int_field(SZrState *state,
                                           SZrObject *object,
                                           const TZrChar *fieldName,
                                           TZrInt64 defaultValue) {
    const SZrTypeValue *value = task_runtime_get_field_value(state, object, fieldName);

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

static TZrBool task_runtime_copy_value_or_null(SZrState *state, const SZrTypeValue *value, SZrTypeValue *result) {
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

static SZrObject *task_runtime_root_object(SZrState *state) {
    if (state == ZR_NULL || state->global == ZR_NULL || state->global->zrObject.type != ZR_VALUE_TYPE_OBJECT ||
        state->global->zrObject.value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, state->global->zrObject.value.object);
}

static TZrBool task_runtime_default_auto_coroutine(SZrState *state) {
    SZrLibrary_Project *project;

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return ZR_TRUE;
    }

    project = (SZrLibrary_Project *)state->global->userData;
    return project == ZR_NULL || project->autoCoroutine ? ZR_TRUE : ZR_FALSE;
}

static TZrBool task_runtime_raise_runtime_error(SZrState *state, const TZrChar *message) {
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

static ZR_NO_RETURN void task_runtime_raise_fault(SZrState *state, const SZrTypeValue *errorValue) {
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

static SZrObject *task_runtime_import_module(SZrState *state, const TZrChar *moduleName) {
    SZrString *moduleNameString;
    SZrObjectModule *loadedModule;

    if (state == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    loadedModule = ZrLib_Module_GetLoaded(state, moduleName);
    if (loadedModule != ZR_NULL) {
        return (SZrObject *)loadedModule;
    }

    moduleNameString = ZrCore_String_Create(state, (TZrNativeString)moduleName, strlen(moduleName));
    if (moduleNameString == ZR_NULL) {
        return ZR_NULL;
    }

    return (SZrObject *)ZrCore_Module_ImportByPath(state, moduleNameString);
}

static const SZrTypeValue *task_runtime_get_module_export(SZrState *state,
                                                          const TZrChar *moduleName,
                                                          const TZrChar *exportName) {
    SZrObjectModule *module;
    SZrString *exportNameString;

    if (state == ZR_NULL || moduleName == ZR_NULL || exportName == ZR_NULL) {
        return ZR_NULL;
    }

    module = (SZrObjectModule *)task_runtime_import_module(state, moduleName);
    if (module == ZR_NULL) {
        return ZR_NULL;
    }

    exportNameString = ZrCore_String_Create(state, (TZrNativeString)exportName, strlen(exportName));
    if (exportNameString == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_Module_GetPubExport(state, module, exportNameString);
}

static const SZrTypeValue *task_runtime_get_loaded_module_export(SZrState *state,
                                                                 SZrObjectModule *module,
                                                                 const TZrChar *exportName) {
    SZrString *exportNameString;

    if (state == ZR_NULL || module == ZR_NULL || exportName == ZR_NULL) {
        return ZR_NULL;
    }

    exportNameString = ZrCore_String_Create(state, (TZrNativeString)exportName, strlen(exportName));
    if (exportNameString == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_Module_GetPubExport(state, module, exportNameString);
}

static SZrObject *task_runtime_new_module_typed_object(SZrState *state,
                                                       const TZrChar *moduleName,
                                                       const TZrChar *typeName) {
    SZrObject *object = ZR_NULL;
    const SZrTypeValue *typeValue;
    SZrObjectPrototype *prototype = ZR_NULL;

    if (state == ZR_NULL || moduleName == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    typeValue = task_runtime_get_module_export(state, moduleName, typeName);
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

static SZrObject *task_runtime_new_loaded_module_typed_object(SZrState *state,
                                                              SZrObjectModule *module,
                                                              const TZrChar *typeName) {
    SZrObject *object = ZR_NULL;
    const SZrTypeValue *typeValue;
    SZrObjectPrototype *prototype = ZR_NULL;

    if (state == ZR_NULL || module == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    typeValue = task_runtime_get_loaded_module_export(state, module, typeName);
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

static SZrObject *task_runtime_ensure_coroutine_scheduler(SZrState *state) {
    SZrObject *rootObject;
    SZrObject *scheduler;
    SZrObject *queue;
    SZrTypeValue schedulerValue;
    SZrTypeValue queueValue;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    rootObject = task_runtime_root_object(state);
    if (rootObject == ZR_NULL) {
        return ZR_NULL;
    }

    scheduler = task_runtime_get_object_field(state, rootObject, kTaskRootCoroutineSchedulerField);
    if (scheduler != ZR_NULL) {
        return scheduler;
    }

    scheduler = task_runtime_new_module_typed_object(state, kCoroutineModuleName, "Scheduler");
    queue = ZrLib_Array_New(state);
    if (scheduler == ZR_NULL || queue == ZR_NULL) {
        return ZR_NULL;
    }

    ZrLib_Value_SetObject(state, &schedulerValue, scheduler, ZR_VALUE_TYPE_OBJECT);
    ZrLib_Value_SetObject(state, &queueValue, queue, ZR_VALUE_TYPE_ARRAY);
    task_runtime_set_value_field(state, rootObject, kTaskRootCoroutineSchedulerField, &schedulerValue);
    task_runtime_set_value_field(state, scheduler, kTaskQueueField, &queueValue);
    task_runtime_set_int_field(state, scheduler, kTaskQueueHeadField, 0);
    task_runtime_set_bool_field(state, scheduler, kTaskAutoCoroutineField, task_runtime_default_auto_coroutine(state));
    task_runtime_set_bool_field(state, scheduler, kTaskIsPumpingField, ZR_FALSE);
    return scheduler;
}

static SZrObject *task_runtime_ensure_coroutine_scheduler_for_module(SZrState *state, SZrObjectModule *module) {
    SZrObject *rootObject;
    SZrObject *scheduler;
    SZrObject *queue;
    SZrTypeValue schedulerValue;
    SZrTypeValue queueValue;

    if (state == ZR_NULL || module == ZR_NULL) {
        return ZR_NULL;
    }

    rootObject = task_runtime_root_object(state);
    if (rootObject == ZR_NULL) {
        return ZR_NULL;
    }

    scheduler = task_runtime_get_object_field(state, rootObject, kTaskRootCoroutineSchedulerField);
    if (scheduler != ZR_NULL) {
        return scheduler;
    }

    scheduler = task_runtime_new_loaded_module_typed_object(state, module, "Scheduler");
    queue = ZrLib_Array_New(state);
    if (scheduler == ZR_NULL || queue == ZR_NULL) {
        return ZR_NULL;
    }

    ZrLib_Value_SetObject(state, &schedulerValue, scheduler, ZR_VALUE_TYPE_OBJECT);
    ZrLib_Value_SetObject(state, &queueValue, queue, ZR_VALUE_TYPE_ARRAY);
    task_runtime_set_value_field(state, rootObject, kTaskRootCoroutineSchedulerField, &schedulerValue);
    task_runtime_set_value_field(state, scheduler, kTaskQueueField, &queueValue);
    task_runtime_set_int_field(state, scheduler, kTaskQueueHeadField, 0);
    task_runtime_set_bool_field(state, scheduler, kTaskAutoCoroutineField, task_runtime_default_auto_coroutine(state));
    task_runtime_set_bool_field(state, scheduler, kTaskIsPumpingField, ZR_FALSE);
    return scheduler;
}

static SZrObject *task_runtime_scheduler_queue(SZrState *state, SZrObject *scheduler) {
    SZrObject *queue = task_runtime_get_object_field(state, scheduler, kTaskQueueField);
    SZrTypeValue queueValue;

    if (queue != ZR_NULL && queue->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return queue;
    }

    queue = ZrLib_Array_New(state);
    if (queue == ZR_NULL) {
        return ZR_NULL;
    }

    ZrLib_Value_SetObject(state, &queueValue, queue, ZR_VALUE_TYPE_ARRAY);
    task_runtime_set_value_field(state, scheduler, kTaskQueueField, &queueValue);
    return queue;
}

static void task_runtime_execute_callable_body(SZrState *state, TZrPtr arguments) {
    ZrVmTaskExecuteRequest *request = (ZrVmTaskExecuteRequest *)arguments;

    if (request == ZR_NULL || request->callable == ZR_NULL) {
        return;
    }

    request->completed = ZrLib_CallValue(state, request->callable, ZR_NULL, ZR_NULL, 0, &request->result);
}

static TZrBool task_runtime_handle_mark_faulted(SZrState *state,
                                                SZrObject *handle,
                                                EZrThreadStatus status,
                                                const SZrTypeValue *fallbackError) {
    SZrTypeValue errorValue;

    if (state == ZR_NULL || handle == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetNull(&errorValue);

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

    task_runtime_set_value_field(state, handle, kTaskErrorField, &errorValue);
    task_runtime_set_null_field(state, handle, kTaskResultField);
    task_runtime_set_null_field(state, handle, kTaskCallableField);
    task_runtime_set_int_field(state, handle, kTaskStatusField, ZR_VM_TASK_STATUS_FAULTED);
    execution_clear_pending_control(state);
    ZrCore_Exception_ClearCurrent(state);
    state->threadStatus = ZR_THREAD_STATUS_FINE;
    return ZR_TRUE;
}

static TZrBool task_runtime_execute_task(SZrState *state, SZrObject *handle) {
    const SZrTypeValue *callable;
    ZrVmTaskExecuteRequest request;
    EZrThreadStatus status;
    SZrTypeValue errorValue;
    SZrCallInfo *savedCallInfo;
    SZrFunctionStackAnchor savedStackTopAnchor;
    SZrFunctionStackAnchor savedCallInfoBaseAnchor;
    SZrFunctionStackAnchor savedCallInfoTopAnchor;
    SZrFunctionStackAnchor savedCallInfoReturnAnchor;
    TZrBool hasSavedCallInfoBase = ZR_FALSE;
    TZrBool hasSavedCallInfoTop = ZR_FALSE;
    TZrBool hasSavedCallInfoReturn = ZR_FALSE;
    TZrUInt32 savedExceptionHandlerStackLength;

    if (state == ZR_NULL || handle == ZR_NULL) {
        return ZR_FALSE;
    }

    callable = task_runtime_get_field_value(state, handle, kTaskCallableField);
    if (callable == ZR_NULL) {
        ZrLib_Value_SetString(state, &errorValue, "Task callable is missing");
        return task_runtime_handle_mark_faulted(state, handle, ZR_THREAD_STATUS_RUNTIME_ERROR, &errorValue);
    }

    task_runtime_set_int_field(state, handle, kTaskStatusField, ZR_VM_TASK_STATUS_RUNNING);
    ZrLib_Value_SetNull(&request.result);
    request.callable = callable;
    request.completed = ZR_FALSE;
    savedCallInfo = state->callInfoList;
    savedExceptionHandlerStackLength = state->exceptionHandlerStackLength;
    ZrCore_Function_StackAnchorInit(state, state->stackTop.valuePointer, &savedStackTopAnchor);
    if (savedCallInfo != ZR_NULL && savedCallInfo->functionBase.valuePointer != ZR_NULL) {
        ZrCore_Function_StackAnchorInit(state, savedCallInfo->functionBase.valuePointer, &savedCallInfoBaseAnchor);
        hasSavedCallInfoBase = ZR_TRUE;
    }
    if (savedCallInfo != ZR_NULL && savedCallInfo->functionTop.valuePointer != ZR_NULL) {
        ZrCore_Function_StackAnchorInit(state, savedCallInfo->functionTop.valuePointer, &savedCallInfoTopAnchor);
        hasSavedCallInfoTop = ZR_TRUE;
    }
    if (savedCallInfo != ZR_NULL && savedCallInfo->hasReturnDestination && savedCallInfo->returnDestination != ZR_NULL) {
        ZrCore_Function_StackAnchorInit(state, savedCallInfo->returnDestination, &savedCallInfoReturnAnchor);
        hasSavedCallInfoReturn = ZR_TRUE;
    }

    status = ZrCore_Exception_TryRun(state, task_runtime_execute_callable_body, &request);
    state->stackTop.valuePointer = ZrCore_Function_StackAnchorRestore(state, &savedStackTopAnchor);
    if (savedCallInfo != ZR_NULL) {
        state->callInfoList = savedCallInfo;
        if (hasSavedCallInfoBase) {
            savedCallInfo->functionBase.valuePointer = ZrCore_Function_StackAnchorRestore(state, &savedCallInfoBaseAnchor);
        }
        if (hasSavedCallInfoTop) {
            savedCallInfo->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(state, &savedCallInfoTopAnchor);
        }
        if (hasSavedCallInfoReturn) {
            savedCallInfo->returnDestination = ZrCore_Function_StackAnchorRestore(state, &savedCallInfoReturnAnchor);
        }
    }
    state->exceptionHandlerStackLength = savedExceptionHandlerStackLength;
    if (status == ZR_THREAD_STATUS_FINE && request.completed && state->threadStatus == ZR_THREAD_STATUS_FINE) {
        task_runtime_set_value_field(state, handle, kTaskResultField, &request.result);
        task_runtime_set_null_field(state, handle, kTaskErrorField);
        task_runtime_set_null_field(state, handle, kTaskCallableField);
        task_runtime_set_int_field(state, handle, kTaskStatusField, ZR_VM_TASK_STATUS_COMPLETED);
        return ZR_TRUE;
    }

    if (status == ZR_THREAD_STATUS_FINE) {
        status = state->threadStatus != ZR_THREAD_STATUS_FINE ? state->threadStatus : ZR_THREAD_STATUS_RUNTIME_ERROR;
    }
    return task_runtime_handle_mark_faulted(state, handle, status, request.completed ? &request.result : ZR_NULL);
}

static TZrBool task_runtime_scheduler_step_internal(SZrState *state, SZrObject *scheduler) {
    SZrObject *queue;
    TZrInt64 head;
    const SZrTypeValue *queuedValue;

    if (state == ZR_NULL || scheduler == ZR_NULL) {
        return ZR_FALSE;
    }

    queue = task_runtime_scheduler_queue(state, scheduler);
    if (queue == ZR_NULL) {
        return ZR_FALSE;
    }

    head = task_runtime_get_int_field(state, scheduler, kTaskQueueHeadField, 0);
    if (head < 0) {
        head = 0;
    }

    queuedValue = ZrLib_Array_Get(state, queue, (TZrSize)head);
    if (queuedValue == ZR_NULL) {
        task_runtime_set_int_field(state, scheduler, kTaskQueueHeadField, 0);
        return ZR_FALSE;
    }

    task_runtime_set_int_field(state, scheduler, kTaskQueueHeadField, head + 1);
    if ((queuedValue->type != ZR_VALUE_TYPE_OBJECT && queuedValue->type != ZR_VALUE_TYPE_ARRAY) ||
        queuedValue->value.object == ZR_NULL) {
        return ZR_TRUE;
    }

    return task_runtime_execute_task(state, ZR_CAST_OBJECT(state, queuedValue->value.object));
}

static TZrInt64 task_runtime_scheduler_pump_internal(SZrState *state, SZrObject *scheduler) {
    TZrInt64 executed = 0;

    if (state == ZR_NULL || scheduler == ZR_NULL) {
        return 0;
    }

    if (task_runtime_get_bool_field(state, scheduler, kTaskIsPumpingField, ZR_FALSE)) {
        return 0;
    }

    task_runtime_set_bool_field(state, scheduler, kTaskIsPumpingField, ZR_TRUE);
    while (task_runtime_scheduler_step_internal(state, scheduler)) {
        executed++;
    }
    task_runtime_set_bool_field(state, scheduler, kTaskIsPumpingField, ZR_FALSE);
    return executed;
}

static TZrBool task_runtime_create_task_handle(SZrState *state,
                                               SZrObject *scheduler,
                                               const SZrTypeValue *callable,
                                               SZrTypeValue *result) {
    SZrObject *handle;
    SZrTypeValue schedulerValue;

    if (state == ZR_NULL || scheduler == ZR_NULL || callable == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    handle = task_runtime_new_module_typed_object(state, kTaskModuleName, "Task");
    if (handle == ZR_NULL) {
        return ZR_FALSE;
    }

    task_runtime_set_int_field(state, handle, kTaskStatusField, ZR_VM_TASK_STATUS_CREATED);
    task_runtime_set_value_field(state, handle, kTaskCallableField, callable);
    task_runtime_set_null_field(state, handle, kTaskResultField);
    task_runtime_set_null_field(state, handle, kTaskErrorField);
    ZrLib_Value_SetObject(state, &schedulerValue, scheduler, ZR_VALUE_TYPE_OBJECT);
    task_runtime_set_value_field(state, handle, kTaskSchedulerOwnerField, &schedulerValue);
    return task_runtime_finish_object(state, result, handle);
}

static TZrBool task_runtime_wait_for_task(SZrState *state, SZrObject *handle, SZrTypeValue *result) {
    TZrInt64 status;
    SZrObject *scheduler;
    TZrBool autoCoroutine;
    TZrBool isPumping;
    const SZrTypeValue *value;

    if (state == ZR_NULL || handle == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    status = task_runtime_get_int_field(state, handle, kTaskStatusField, ZR_VM_TASK_STATUS_CREATED);
    if (status == ZR_VM_TASK_STATUS_COMPLETED) {
        value = task_runtime_get_field_value(state, handle, kTaskResultField);
        return task_runtime_copy_value_or_null(state, value, result);
    }
    if (status == ZR_VM_TASK_STATUS_FAULTED) {
        task_runtime_raise_fault(state, task_runtime_get_field_value(state, handle, kTaskErrorField));
    }

    scheduler = task_runtime_get_object_field(state, handle, kTaskSchedulerOwnerField);
    if (scheduler == ZR_NULL) {
        scheduler = task_runtime_ensure_coroutine_scheduler(state);
    }
    autoCoroutine = scheduler != ZR_NULL && task_runtime_get_bool_field(state, scheduler, kTaskAutoCoroutineField, ZR_TRUE);
    isPumping = scheduler != ZR_NULL && task_runtime_get_bool_field(state, scheduler, kTaskIsPumpingField, ZR_FALSE);

    if (scheduler != ZR_NULL && autoCoroutine) {
        while (ZR_TRUE) {
            status = task_runtime_get_int_field(state, handle, kTaskStatusField, ZR_VM_TASK_STATUS_CREATED);
            if (status == ZR_VM_TASK_STATUS_COMPLETED || status == ZR_VM_TASK_STATUS_FAULTED) {
                return task_runtime_wait_for_task(state, handle, result);
            }

            if (!task_runtime_scheduler_invoke_step(state, scheduler)) {
                if (state->threadStatus != ZR_THREAD_STATUS_FINE || state->hasCurrentException) {
                    return ZR_FALSE;
                }
                break;
            }
        }
    }

    if (isPumping) {
        ZrCore_Debug_RunError(state, "Task is still pending on an active scheduler frame");
    }

    ZrCore_Debug_RunError(state,
                          "Task is still pending while autoCoroutine is disabled; call Scheduler.pump() first");
}

static SZrObject *task_runtime_default_scheduler(SZrState *state) {
    const SZrTypeValue *exportValue;

    exportValue = task_runtime_get_module_export(state, kTaskModuleName, "defaultScheduler");
    if (exportValue == ZR_NULL || (exportValue->type != ZR_VALUE_TYPE_OBJECT && exportValue->type != ZR_VALUE_TYPE_ARRAY) ||
        exportValue->value.object == ZR_NULL) {
        return task_runtime_ensure_coroutine_scheduler(state);
    }

    return ZR_CAST_OBJECT(state, exportValue->value.object);
}

static TZrBool task_runtime_scheduler_invoke_start(SZrState *state,
                                                   SZrObject *scheduler,
                                                   SZrObject *runner,
                                                   SZrTypeValue *result) {
    SZrString *memberName;
    SZrTypeValue receiverValue;
    SZrTypeValue argumentValue;

    if (state == ZR_NULL || scheduler == ZR_NULL || runner == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    memberName = ZrCore_String_Create(state, "start", strlen("start"));
    if (memberName == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(state, &receiverValue, scheduler, ZR_VALUE_TYPE_OBJECT);
    ZrLib_Value_SetObject(state, &argumentValue, runner, task_runtime_value_type_for_object(runner));
    return ZrCore_Object_InvokeMember(state, &receiverValue, memberName, &argumentValue, 1, result);
}

static TZrBool task_runtime_scheduler_invoke_step(SZrState *state, SZrObject *scheduler) {
    SZrString *memberName;
    SZrTypeValue receiverValue;
    SZrTypeValue stepResult;
    TZrBool invoked;

    if (state == ZR_NULL || scheduler == ZR_NULL) {
        return ZR_FALSE;
    }

    memberName = ZrCore_String_Create(state, "step", strlen("step"));
    if (memberName == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(state, &receiverValue, scheduler, ZR_VALUE_TYPE_OBJECT);
    ZrLib_Value_SetNull(&stepResult);
    invoked = ZrCore_Object_InvokeMember(state, &receiverValue, memberName, ZR_NULL, 0, &stepResult);
    if (!invoked) {
        return ZR_FALSE;
    }

    if (stepResult.type == ZR_VALUE_TYPE_BOOL) {
        return stepResult.value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE;
    }
    if (ZR_VALUE_IS_TYPE_SIGNED_INT(stepResult.type)) {
        return stepResult.value.nativeObject.nativeInt64 != 0 ? ZR_TRUE : ZR_FALSE;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(stepResult.type)) {
        return stepResult.value.nativeObject.nativeUInt64 != 0 ? ZR_TRUE : ZR_FALSE;
    }
    return ZR_FALSE;
}

static TZrBool task_runtime_start_runner_on_scheduler(SZrState *state,
                                                      SZrObject *scheduler,
                                                      SZrObject *runner,
                                                      SZrTypeValue *result) {
    const SZrTypeValue *callable;
    SZrObject *queue;

    if (state == ZR_NULL || scheduler == ZR_NULL || runner == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (task_runtime_get_bool_field(state, runner, kTaskRunnerStartedField, ZR_FALSE)) {
        return task_runtime_raise_runtime_error(state, "TaskRunner.start() can only be called once");
    }

    callable = task_runtime_get_field_value(state, runner, kTaskRunnerCallableField);
    if (callable == ZR_NULL) {
        return task_runtime_raise_runtime_error(state, "TaskRunner is missing its callable");
    }

    if (!task_runtime_create_task_handle(state, scheduler, callable, result) ||
        result->type != ZR_VALUE_TYPE_OBJECT || result->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    task_runtime_set_bool_field(state, runner, kTaskRunnerStartedField, ZR_TRUE);
    queue = task_runtime_scheduler_queue(state, scheduler);
    if (queue == ZR_NULL || !ZrLib_Array_PushValue(state, queue, result)) {
        return ZR_FALSE;
    }

    task_runtime_set_int_field(state,
                               ZR_CAST_OBJECT(state, result->value.object),
                               kTaskStatusField,
                               ZR_VM_TASK_STATUS_QUEUED);
    if (task_runtime_get_bool_field(state, scheduler, kTaskAutoCoroutineField, ZR_TRUE) &&
        !task_runtime_get_bool_field(state, scheduler, kTaskIsPumpingField, ZR_FALSE)) {
        task_runtime_scheduler_pump_internal(state, scheduler);
    }

    return ZR_TRUE;
}

static TZrBool task_runtime_create_runner(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *runner;
    SZrTypeValue *callable;

    if (context == ZR_NULL || result == ZR_NULL || !ZrLib_CallContext_ReadFunction(context, 0, &callable)) {
        return ZR_FALSE;
    }

    runner = task_runtime_new_module_typed_object(context->state, kTaskModuleName, "TaskRunner");
    if (runner == ZR_NULL) {
        return ZR_FALSE;
    }

    task_runtime_set_value_field(context->state, runner, kTaskRunnerCallableField, callable);
    task_runtime_set_bool_field(context->state, runner, kTaskRunnerStartedField, ZR_FALSE);
    return task_runtime_finish_object(context->state, result, runner);
}

static TZrBool task_runtime_await_hidden(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *handle;
    if (context == ZR_NULL || result == ZR_NULL || !ZrLib_CallContext_ReadObject(context, 0, &handle)) {
        return ZR_FALSE;
    }

    return task_runtime_wait_for_task(context->state, handle, result);
}

static TZrBool task_runtime_runner_start(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *runner = task_runtime_self_object(context);
    SZrObject *scheduler;

    if (context == ZR_NULL || result == ZR_NULL || runner == ZR_NULL) {
        return ZR_FALSE;
    }

    scheduler = task_runtime_default_scheduler(context->state);
    return task_runtime_scheduler_invoke_start(context->state, scheduler, runner, result);
}

static TZrBool task_runtime_task_result(ZrLibCallContext *context, SZrTypeValue *result) {
    return task_runtime_wait_for_task(context->state, task_runtime_self_object(context), result);
}

static TZrBool task_runtime_task_is_completed(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = task_runtime_self_object(context);
    TZrInt64 status;

    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    status = task_runtime_get_int_field(context->state, self, kTaskStatusField, ZR_VM_TASK_STATUS_CREATED);
    ZrLib_Value_SetBool(context->state,
                        result,
                        (TZrBool)(status == ZR_VM_TASK_STATUS_COMPLETED || status == ZR_VM_TASK_STATUS_FAULTED));
    return ZR_TRUE;
}

static TZrBool task_runtime_scheduler_start_method(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *scheduler = task_runtime_self_object(context);
    SZrObject *runner;

    if (context == ZR_NULL || result == ZR_NULL || scheduler == ZR_NULL ||
        !ZrLib_CallContext_ReadObject(context, 0, &runner)) {
        return ZR_FALSE;
    }

    return task_runtime_start_runner_on_scheduler(context->state, scheduler, runner, result);
}

static TZrBool task_runtime_scheduler_step_method(ZrLibCallContext *context, SZrTypeValue *result) {
    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetBool(context->state,
                        result,
                        task_runtime_scheduler_step_internal(context->state, task_runtime_self_object(context)));
    return ZR_TRUE;
}

static TZrBool task_runtime_scheduler_pump_method(ZrLibCallContext *context, SZrTypeValue *result) {
    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetInt(context->state,
                       result,
                       task_runtime_scheduler_pump_internal(context->state, task_runtime_self_object(context)));
    return ZR_TRUE;
}

static TZrBool task_runtime_scheduler_set_auto_method(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = task_runtime_self_object(context);
    TZrBool autoCoroutine;

    if (self == ZR_NULL || result == ZR_NULL || !ZrLib_CallContext_ReadBool(context, 0, &autoCoroutine)) {
        return ZR_FALSE;
    }

    task_runtime_set_bool_field(context->state, self, kTaskAutoCoroutineField, autoCoroutine);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

static TZrBool task_runtime_scheduler_get_auto_method(ZrLibCallContext *context, SZrTypeValue *result) {
    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetBool(context->state,
                        result,
                        task_runtime_get_bool_field(context->state,
                                                    task_runtime_self_object(context),
                                                    kTaskAutoCoroutineField,
                                                    ZR_TRUE));
    return ZR_TRUE;
}

static TZrBool task_runtime_coroutine_start_function(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *runner;

    if (context == ZR_NULL || result == ZR_NULL || !ZrLib_CallContext_ReadObject(context, 0, &runner)) {
        return ZR_FALSE;
    }

    return task_runtime_start_runner_on_scheduler(context->state,
                                                  task_runtime_ensure_coroutine_scheduler(context->state),
                                                  runner,
                                                  result);
}

static TZrBool task_runtime_task_module_materialize(SZrState *state,
                                                    SZrObjectModule *module,
                                                    const ZrLibModuleDescriptor *descriptor) {
    SZrObject *scheduler;
    SZrString *fieldName;
    SZrTypeValue value;

    ZR_UNUSED_PARAMETER(descriptor);
    if (state == ZR_NULL || module == ZR_NULL) {
        return ZR_FALSE;
    }

    scheduler = task_runtime_ensure_coroutine_scheduler(state);
    fieldName = ZrCore_String_Create(state, "defaultScheduler", strlen("defaultScheduler"));
    if (scheduler == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(state, &value, scheduler, ZR_VALUE_TYPE_OBJECT);
    ZrCore_Module_AddPubExport(state, module, fieldName, &value);
    return ZR_TRUE;
}

static TZrBool task_runtime_coroutine_module_materialize(SZrState *state,
                                                         SZrObjectModule *module,
                                                         const ZrLibModuleDescriptor *descriptor) {
    SZrObject *scheduler;
    SZrString *fieldName;
    SZrTypeValue value;

    ZR_UNUSED_PARAMETER(descriptor);
    if (state == ZR_NULL || module == ZR_NULL) {
        return ZR_FALSE;
    }

    scheduler = task_runtime_ensure_coroutine_scheduler_for_module(state, module);
    fieldName = ZrCore_String_Create(state, "coroutineScheduler", strlen("coroutineScheduler"));
    if (scheduler == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(state, &value, scheduler, ZR_VALUE_TYPE_OBJECT);
    ZrCore_Module_AddPubExport(state, module, fieldName, &value);
    return ZR_TRUE;
}

static const ZrLibGenericParameterDescriptor g_task_single_generic_parameter[] = {
        {
                .name = "T",
                .documentation = "The task payload type.",
        },
};

static const ZrLibParameterDescriptor g_scheduler_start_parameters[] = {
        {"runner", "TaskRunner<T>", "The cold runner to schedule."},
};

static const ZrLibParameterDescriptor g_create_runner_parameters[] = {
        {"callable", "function", "Hidden callable backing an async function body."},
};

static const ZrLibParameterDescriptor g_await_parameters[] = {
        {"task", "Task<T>", "The started task to await."},
};

static const ZrLibParameterDescriptor g_scheduler_set_auto_parameters[] = {
        {"enabled", "bool", "Whether the scheduler should auto-pump queued work."},
};

static const ZrLibMethodDescriptor g_runner_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("start", 0, 0, task_runtime_runner_start, "Task<T>",
                                      "Start this cold task runner on zr.task.defaultScheduler.", ZR_FALSE, ZR_NULL, 0),
};

static const ZrLibMethodDescriptor g_task_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("result", 0, 0, task_runtime_task_result, "T",
                                      "Resolve the task and return its completion value.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("isCompleted", 0, 0, task_runtime_task_is_completed, "bool",
                                      "Return whether the task has completed or faulted.", ZR_FALSE, ZR_NULL, 0),
};

static const ZrLibMethodDescriptor g_scheduler_methods[] = {
        {"start", 1, 1, task_runtime_scheduler_start_method, "Task<T>",
         "Queue a TaskRunner on this scheduler.", ZR_FALSE, g_scheduler_start_parameters,
         ZR_ARRAY_COUNT(g_scheduler_start_parameters), 0U, g_task_single_generic_parameter,
         ZR_ARRAY_COUNT(g_task_single_generic_parameter)},
        ZR_LIB_METHOD_DESCRIPTOR_INIT("step", 0, 0, task_runtime_scheduler_step_method, "bool",
                                      "Execute one runnable task if present.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("pump", 0, 0, task_runtime_scheduler_pump_method, "int",
                                      "Drain runnable tasks until the queue becomes empty.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("setAutoCoroutine", 1, 1, task_runtime_scheduler_set_auto_method, "null",
                                      "Enable or disable automatic scheduler pumping.", ZR_FALSE,
                                      g_scheduler_set_auto_parameters,
                                      ZR_ARRAY_COUNT(g_scheduler_set_auto_parameters)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("getAutoCoroutine", 0, 0, task_runtime_scheduler_get_auto_method, "bool",
                                      "Return whether automatic scheduler pumping is enabled.", ZR_FALSE, ZR_NULL, 0),
};

static const TZrChar *g_scheduler_implements[] = {"IScheduler"};

static const ZrLibFunctionDescriptor g_task_functions[] = {
        {"__createTaskRunner", 1, 1, task_runtime_create_runner, "TaskRunner<T>",
         "Internal helper used by %async lowering.", g_create_runner_parameters,
         ZR_ARRAY_COUNT(g_create_runner_parameters), g_task_single_generic_parameter,
         ZR_ARRAY_COUNT(g_task_single_generic_parameter)},
        {"__awaitTask", 1, 1, task_runtime_await_hidden, "T",
         "Internal helper used by %await lowering.", g_await_parameters, ZR_ARRAY_COUNT(g_await_parameters),
         g_task_single_generic_parameter, ZR_ARRAY_COUNT(g_task_single_generic_parameter)},
};

static const ZrLibFunctionDescriptor g_coroutine_functions[] = {
        {"start", 1, 1, task_runtime_coroutine_start_function, "Task<T>",
         "Queue a TaskRunner on the coroutine scheduler.", g_scheduler_start_parameters,
         ZR_ARRAY_COUNT(g_scheduler_start_parameters), g_task_single_generic_parameter,
         ZR_ARRAY_COUNT(g_task_single_generic_parameter)},
};

static const ZrLibTypeDescriptor g_task_types[] = {
        ZR_LIB_TYPE_DESCRIPTOR_INIT("IScheduler", ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE, ZR_NULL, 0, g_scheduler_methods,
                                    ZR_ARRAY_COUNT(g_scheduler_methods), ZR_NULL, 0,
                                    "Scheduler interface implemented by task schedulers.", ZR_NULL, ZR_NULL, 0,
                                    ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0),
        ZR_LIB_TYPE_DESCRIPTOR_INIT("TaskRunner", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, g_runner_methods,
                                    ZR_ARRAY_COUNT(g_runner_methods), ZR_NULL, 0,
                                    "Cold async runner produced by %async declarations.", ZR_NULL, ZR_NULL, 0,
                                    ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL,
                                    g_task_single_generic_parameter, ZR_ARRAY_COUNT(g_task_single_generic_parameter)),
        ZR_LIB_TYPE_DESCRIPTOR_INIT("Task", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, g_task_methods,
                                    ZR_ARRAY_COUNT(g_task_methods), ZR_NULL, 0,
                                    "Started task handle that can be awaited.", ZR_NULL, ZR_NULL, 0, ZR_NULL, 0,
                                    ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL, g_task_single_generic_parameter,
                                    ZR_ARRAY_COUNT(g_task_single_generic_parameter)),
};

static const ZrLibTypeDescriptor g_coroutine_types[] = {
        ZR_LIB_TYPE_DESCRIPTOR_INIT("Scheduler", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, g_scheduler_methods,
                                    ZR_ARRAY_COUNT(g_scheduler_methods), ZR_NULL, 0,
                                    "Built-in isolate coroutine scheduler.", ZR_NULL, g_scheduler_implements,
                                    ZR_ARRAY_COUNT(g_scheduler_implements), ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE,
                                    ZR_NULL, ZR_NULL, 0),
};

static const ZrLibTypeHintDescriptor g_task_hints[] = {
        {"defaultScheduler", "property", "defaultScheduler: IScheduler",
         "Read or replace the scheduler used by TaskRunner.start()."},
        {"IScheduler", "type", "interface IScheduler", "Scheduler interface for task dispatchers."},
        {"TaskRunner", "type", "class TaskRunner<T>", "Cold async runner produced by %async."},
        {"Task", "type", "class Task<T>", "Started task handle that can be awaited."},
};

static const ZrLibTypeHintDescriptor g_coroutine_hints[] = {
        {"coroutineScheduler", "property", "coroutineScheduler: Scheduler",
         "The isolate-wide built-in coroutine scheduler instance."},
        {"start", "function", "start(runner: TaskRunner<T>): Task<T>",
         "Queue a TaskRunner on the coroutine scheduler."},
        {"Scheduler", "type", "class Scheduler implements zr.task.IScheduler",
         "Built-in coroutine scheduler type."},
};

static const TZrChar g_task_hints_json[] =
        "{\n"
        "  \"schema\": \"zr.native.hints/v1\",\n"
        "  \"module\": \"zr.task\"\n"
        "}\n";

static const TZrChar g_coroutine_hints_json[] =
        "{\n"
        "  \"schema\": \"zr.native.hints/v1\",\n"
        "  \"module\": \"zr.coroutine\"\n"
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
        "Built-in task abstractions and scheduler interfaces for %async/%await.",
        ZR_NULL,
        0,
        "2.0.0",
        ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
        0,
        task_runtime_task_module_materialize,
};

static const ZrLibModuleDescriptor g_coroutine_descriptor = {
        ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        "zr.coroutine",
        ZR_NULL,
        0,
        g_coroutine_functions,
        ZR_ARRAY_COUNT(g_coroutine_functions),
        g_coroutine_types,
        ZR_ARRAY_COUNT(g_coroutine_types),
        g_coroutine_hints,
        ZR_ARRAY_COUNT(g_coroutine_hints),
        g_coroutine_hints_json,
        "Built-in isolate coroutine scheduler for zr_vm.",
        ZR_NULL,
        0,
        "2.0.0",
        ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
        0,
        task_runtime_coroutine_module_materialize,
};

TZrBool ZrCore_TaskRuntime_RegisterBuiltins(SZrGlobalState *global) {
    if (global == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrLibrary_NativeRegistry_Attach(global)) {
        return ZR_FALSE;
    }

    return ZrLibrary_NativeRegistry_RegisterModule(global, &g_task_descriptor) &&
           ZrLibrary_NativeRegistry_RegisterModule(global, &g_coroutine_descriptor);
}
