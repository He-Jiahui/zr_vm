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
#include "zr_vm_library/task_runtime.h"
#include "zr_vm_library/project.h"

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

/** @brief TryRun 同步回调的栈上请求；借用 callable，result 与 completed 在捕获返回后供 Task 结算使用。 */
typedef struct ZrVmTaskExecuteRequest {
    const SZrTypeValue *callable;
    SZrTypeValue result;
    TZrBool completed;
} ZrVmTaskExecuteRequest;

/**
 * @brief Job、Task 与 Scheduler 的私有字段协议；字段名由本文件及包含的队列 helper 共用。
 * @note provider 通过 WorkItem 与 await hook API 交接；completion 状态不代表 worker 已退出。
 */
static const TZrChar *kTaskModuleName = "zr.task";
static const TZrChar *kTaskRootSchedulerField = "__zr_task_scheduler";
static const TZrChar *kTaskQueueField = "__zr_task_queue";
static const TZrChar *kTaskQueueHeadField = "__zr_task_queue_head";
static const TZrChar *kTaskIsPumpingField = "__zr_task_is_pumping";
static const TZrChar *kTaskStatusField = "__zr_task_status";
static const TZrChar *kTaskCallableField = "__zr_task_callable";
static const TZrChar *kTaskResultField = "__zr_task_result";
static const TZrChar *kTaskErrorField = "__zr_task_error";
static const TZrChar *kTaskSchedulerOwnerField = "__zr_task_scheduler_owner";
static const TZrChar *kTaskJobCallableField = "__zr_task_job_callable";
static const TZrChar *kTaskJobConsumedField = "__zr_task_job_consumed";
static const TZrChar *kTaskCooperativeTaskField = "__zr_task_cooperative_task";
static const TZrChar *kTaskCooperativeTurnsField = "__zr_task_cooperative_turns";
static const TZrChar *kTaskProviderAwaitRegistrationField = "__zr_task_provider_await_registration";

/* 这些私有字段形成 Job -> Task -> Scheduler 的共享 ABI：Job 仅提供一次性 callable，
 * Task 持有完成状态与归属 scheduler，thread provider 通过公开 handoff API 访问它们。 */

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

    /* BUG: 底层 void setter 在 pin/key 分配失败时静默返回；上层构造 Task、
     * 标记 Job 已消费和登记 await hook 均无法确认字段写入，仍可报告成功。 */
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

/**
 * @brief 借用对象字段值供状态检查或随即复制；缺失字段返回 NULL。
 * @note 返回值不是拥有副本；调用方不能把此地址当作跨分配、字段修改或对象生命周期的稳定句柄。
 */
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

static SZrObject *task_runtime_ensure_current_scheduler(SZrState *state) {
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

    scheduler = task_runtime_get_object_field(state, rootObject, kTaskRootSchedulerField);
    if (scheduler != ZR_NULL) {
        return scheduler;
    }

    scheduler = task_runtime_new_module_typed_object(state, kTaskModuleName, "Scheduler");
    queue = ZrLib_Array_New(state);
    if (scheduler == ZR_NULL || queue == ZR_NULL) {
        return ZR_NULL;
    }

    ZrLib_Value_SetObject(state, &schedulerValue, scheduler, ZR_VALUE_TYPE_OBJECT);
    ZrLib_Value_SetObject(state, &queueValue, queue, ZR_VALUE_TYPE_ARRAY);
    task_runtime_set_value_field(state, rootObject, kTaskRootSchedulerField, &schedulerValue);
    task_runtime_set_value_field(state, scheduler, kTaskQueueField, &queueValue);
    task_runtime_set_int_field(state, scheduler, kTaskQueueHeadField, 0);
    task_runtime_set_bool_field(state, scheduler, kTaskIsPumpingField, ZR_FALSE);
    return scheduler;
}

/**
 * @brief materialize 阶段从已加载模块取得 Scheduler 原型，避免再次导入正在物化的 zr.task。
 * @note 同一 global 根上已有 scheduler 时直接复用；字段发布仍使用通用 void setter。
 */
static SZrObject *task_runtime_ensure_current_scheduler_for_module(SZrState *state, SZrObjectModule *module) {
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

    scheduler = task_runtime_get_object_field(state, rootObject, kTaskRootSchedulerField);
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
    task_runtime_set_value_field(state, rootObject, kTaskRootSchedulerField, &schedulerValue);
    task_runtime_set_value_field(state, scheduler, kTaskQueueField, &queueValue);
    task_runtime_set_int_field(state, scheduler, kTaskQueueHeadField, 0);
    task_runtime_set_bool_field(state, scheduler, kTaskIsPumpingField, ZR_FALSE);
    return scheduler;
}

#include "task_runtime_scheduler_queue.inc"

static void task_runtime_execute_callable_body(SZrState *state, TZrPtr arguments) {
    ZrVmTaskExecuteRequest *request = (ZrVmTaskExecuteRequest *)arguments;

    if (request == ZR_NULL || request->callable == ZR_NULL) {
        return;
    }

    request->completed = ZrLib_CallValue(state, request->callable, ZR_NULL, ZR_NULL, 0, &request->result);
}

/**
 * @brief 把当前异常或后备错误结算到 Task，再清理本次执行遗留的 control/exception 状态。
 * @note callable 与 result 被清空，状态设为 FAULTED；不释放 provider 的 WorkItem 根或 worker 资源。
 */
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

/**
 * @brief 在 TryRun 边界同步调用 Task callable，并恢复调用者的栈锚点后结算成功或异常。
 * @note 保存原异常 handler 深度和 AOT root 链；清理新增 handler 时临时登记 handle/result，
 * 随后恢复原链及栈位置。provider 持有的 WorkItem 根仍由 provider 显式释放。
 */
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
    SZrAotGcRootFrame *savedRootFrame;
    TZrUInt32 savedRootDepth;

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
    savedRootFrame = state->aotGcRootFrameStack;
    savedRootDepth = state->aotGcRootFrameDepth;
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

    /* Job 回调可能触发 GC 或异常跳转。保存调用帧锚点，并在返回后以本地根重新定位
     * Task/结果，才能把完成状态写回原 caller-domain Task。 */
    status = ZrCore_Exception_TryRun(state, task_runtime_execute_callable_body, &request);
    {
        static const SZrAotGcRootSlot slots[] = {
            {0u, 0u, 0u, 0u, ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS, 0u, 0u},
            {0u, (TZrUInt32)sizeof(SZrRawObject *), 0u, 0u,
             ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS, 0u, 0u}
        };
        static const SZrAotGcRootMap map = {2u, slots};
        SZrRawObject *roots[] = {
            ZR_CAST_RAW_OBJECT_AS_SUPER(handle),
            ZrCore_Value_IsGarbageCollectable(&request.result)
                    ? ZrCore_Value_GetRawObject(&request.result) : ZR_NULL
        };
        SZrAotGcRootFrame rootFrame;
        EZrThreadStatus handlerStatus = ZR_THREAD_STATUS_MEMORY_ERROR;
        state->aotGcRootFrameStack = savedRootFrame;
        state->aotGcRootFrameDepth = savedRootDepth;
        if (ZrCore_Gc_AotRootFramePush(state, &rootFrame, (TZrStackValuePointer)roots, &map)) {
            handlerStatus = execution_discard_exception_handlers_to_depth(
                    state, savedExceptionHandlerStackLength);
            handle = (SZrObject *)roots[0];
            if (roots[1] != ZR_NULL) {
                request.result.value.object = roots[1];
            }
            (void)ZrCore_Gc_AotRootFramePop(state, &rootFrame);
        }
        state->aotGcRootFrameStack = savedRootFrame;
        state->aotGcRootFrameDepth = savedRootDepth;
        if (status == ZR_THREAD_STATUS_FINE) {
            status = handlerStatus;
        }
    }
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

/**
 * @brief 从本地队列推进一个元素；先移动 head，允许运行中的 Job 在同一队列尾部提交新工作。
 * @note cooperative Task 每次剩余 turns 大于零时减一并重新入队；耗尽时尝试分离旧队列，
 * 不把仍挂载的旧数组游标归零。返回 false 也可能表示缺失/拒绝队列或执行失败。
 */
static TZrBool task_runtime_scheduler_step_internal(SZrState *state, SZrObject *scheduler) {
    SZrObject *queue;
    SZrObject *handle;
    TZrInt64 head;
    TZrInt64 remainingTurns;
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
        (void)task_runtime_scheduler_detach_queue(state, scheduler, queue);
        return ZR_FALSE;
    }

    task_runtime_set_int_field(state, scheduler, kTaskQueueHeadField, head + 1);
    if ((queuedValue->type != ZR_VALUE_TYPE_OBJECT && queuedValue->type != ZR_VALUE_TYPE_ARRAY) ||
        queuedValue->value.object == ZR_NULL) {
        return ZR_TRUE;
    }

    handle = ZR_CAST_OBJECT(state, queuedValue->value.object);
    if (!task_runtime_get_bool_field(state, handle, kTaskCooperativeTaskField, ZR_FALSE)) {
        return task_runtime_execute_task(state, handle);
    }

    remainingTurns = task_runtime_get_int_field(state, handle, kTaskCooperativeTurnsField, 0);
    if (remainingTurns > 0) {
        task_runtime_set_int_field(state, handle, kTaskCooperativeTurnsField, remainingTurns - 1);
        if (!ZrLib_Array_PushValue(state, queue, queuedValue)) {
            SZrTypeValue errorValue;
            ZrLib_Value_SetString(state, &errorValue, "Scheduler rejected cooperative task");
            return task_runtime_handle_mark_faulted(state, handle, ZR_THREAD_STATUS_RUNTIME_ERROR, &errorValue);
        }
        return ZR_TRUE;
    }

    task_runtime_set_null_field(state, handle, kTaskResultField);
    task_runtime_set_null_field(state, handle, kTaskErrorField);
    task_runtime_set_null_field(state, handle, kTaskCallableField);
    task_runtime_set_int_field(state, handle, kTaskStatusField, ZR_VM_TASK_STATUS_COMPLETED);
    return ZR_TRUE;
}

/**
 * @brief 用私有 pumping 标记抑制同一 scheduler 的嵌套 pump，循环推进至 step 返回 false。
 * @return 成功返回 true 的 step 次数，包含 cooperative 重排和跳过元素，不等于执行 Job 数。
 * @note 标记的正常清理由循环后的 setter 完成；本函数没有独立 TryRun 或 provider worker 等待。
 */
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

/**
 * @brief 构造不含 callable 的 cooperative Task，入队后在非 pumping 状态下立即推进本地队列。
 * @note turns 是重新入队的剩余次数；队列交接失败会尝试把已返回的 handle 结算为 faulted。
 */
static TZrBool task_runtime_create_cooperative_task(SZrState *state,
                                                    SZrObject *scheduler,
                                                    TZrInt64 turns,
                                                    SZrTypeValue *result) {
    SZrObject *handle;
    SZrObject *queue;
    SZrTypeValue schedulerValue;

    if (state == ZR_NULL || scheduler == ZR_NULL || result == ZR_NULL || turns < 0) {
        return ZR_FALSE;
    }

    handle = task_runtime_new_module_typed_object(state, kTaskModuleName, "Task");
    if (handle == ZR_NULL) {
        return ZR_FALSE;
    }

    task_runtime_set_int_field(state, handle, kTaskStatusField, ZR_VM_TASK_STATUS_QUEUED);
    task_runtime_set_null_field(state, handle, kTaskCallableField);
    task_runtime_set_null_field(state, handle, kTaskResultField);
    task_runtime_set_null_field(state, handle, kTaskErrorField);
    task_runtime_set_bool_field(state, handle, kTaskCooperativeTaskField, ZR_TRUE);
    task_runtime_set_int_field(state, handle, kTaskCooperativeTurnsField, turns);
    ZrLib_Value_SetObject(state, &schedulerValue, scheduler, ZR_VALUE_TYPE_OBJECT);
    task_runtime_set_value_field(state, handle, kTaskSchedulerOwnerField, &schedulerValue);
    if (!task_runtime_finish_object(state, result, handle)) {
        return ZR_FALSE;
    }

    queue = task_runtime_scheduler_queue(state, scheduler);
    if (queue == ZR_NULL || !ZrLib_Array_PushValue(state, queue, result)) {
        SZrTypeValue errorValue;
        ZrLib_Value_SetString(state, &errorValue, "Scheduler rejected cooperative task");
        return task_runtime_handle_mark_faulted(state, handle, ZR_THREAD_STATUS_RUNTIME_ERROR, &errorValue);
    }

    if (!task_runtime_get_bool_field(state, scheduler, kTaskIsPumpingField, ZR_FALSE)) {
        task_runtime_scheduler_pump_internal(state, scheduler);
    }
    return ZR_TRUE;
}

/**
 * @brief Task.result 的等待路径：先处理终态，再尝试所属 scheduler 的 provider hook，最后推进本地队列。
 * @note hook 声明已接管且成功返回后递归复查终态；provider 应使 Task 能继续结算。
 * 正在 pump 的本地 frame 不递归 pump pending Task，无法完成时报告运行错误。
 */
static TZrBool task_runtime_wait_for_task(SZrState *state, SZrObject *handle, SZrTypeValue *result) {
    TZrInt64 status;
    SZrObject *scheduler;
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
        scheduler = task_runtime_ensure_current_scheduler(state);
    }
    if (scheduler != ZR_NULL) {
        TZrBool providerHandled = ZR_FALSE;

        if (!ZrLibrary_TaskRuntime_AwaitProviderTask(state, scheduler, handle, &providerHandled)) {
            return ZR_FALSE;
        }
        if (providerHandled) {
            return task_runtime_wait_for_task(state, handle, result);
        }
    }
    if (scheduler != ZR_NULL && !task_runtime_get_bool_field(state, scheduler, kTaskIsPumpingField, ZR_FALSE)) {
        while (ZR_TRUE) {
            status = task_runtime_get_int_field(state, handle, kTaskStatusField, ZR_VM_TASK_STATUS_CREATED);
            if (status == ZR_VM_TASK_STATUS_COMPLETED || status == ZR_VM_TASK_STATUS_FAULTED) {
                return task_runtime_wait_for_task(state, handle, result);
            }

            if (!task_runtime_scheduler_step_internal(state, scheduler)) {
                if (state->threadStatus != ZR_THREAD_STATUS_FINE || state->hasCurrentException) {
                    return ZR_FALSE;
                }
                break;
            }
        }
    }

    ZrCore_Debug_RunError(state, "Task is still pending on an active scheduler frame");
}

/**
 * @brief 消费 cold Job，形成 caller-domain Task 并把其 GC 根交给 provider 的 WorkItem。
 * @pre outItem 不得覆盖尚未释放的成功 WorkItem；后续 resolve、结算与 release 使用同一 GC 域。
 * @note 消费标记先于 Task 分配；失败后不恢复 Job callable。成功后需要显式 ReleasePreparedJob，
 * Execute/Fault/Complete 均不替 provider 释放该根；私有字段写入仍受通用 void setter 的限制。
 */
TZrBool ZrLibrary_TaskRuntime_PrepareJob(SZrState *state,
                                         SZrObject *scheduler,
                                         SZrObject *job,
                                         SZrTypeValue *result,
                                         ZrLibraryTaskRuntimeWorkItem *outItem) {
    const SZrTypeValue *callable;
    SZrObject *handle;

    if (outItem != ZR_NULL) {
        memset(outItem, 0, sizeof(*outItem));
    }
    if (state == ZR_NULL || scheduler == ZR_NULL || job == ZR_NULL || result == ZR_NULL || outItem == ZR_NULL) {
        return ZR_FALSE;
    }

    if (task_runtime_get_bool_field(state, job, kTaskJobConsumedField, ZR_FALSE)) {
        return task_runtime_raise_runtime_error(state, "Job can only be scheduled once");
    }

    callable = task_runtime_get_field_value(state, job, kTaskJobCallableField);
    if (callable == ZR_NULL) {
        return task_runtime_raise_runtime_error(state, "Job is missing its callable");
    }

    /* 一次性消费先于 provider 排队；即使后续建立 Task/GC 根失败也不重放 callable。 */
    task_runtime_set_bool_field(state, job, kTaskJobConsumedField, ZR_TRUE);
    if (!task_runtime_create_task_handle(state, scheduler, callable, result) ||
        result->type != ZR_VALUE_TYPE_OBJECT || result->value.object == ZR_NULL) {
        task_runtime_set_null_field(state, job, kTaskJobCallableField);
        return ZR_FALSE;
    }
    task_runtime_set_null_field(state, job, kTaskJobCallableField);

    handle = ZR_CAST_OBJECT(state, result->value.object);
    task_runtime_set_int_field(state, handle, kTaskStatusField, ZR_VM_TASK_STATUS_QUEUED);
    if (!ZrCore_GcRootHandle_Create(state, ZR_CAST_RAW_OBJECT_AS_SUPER(handle), &outItem->taskRoot)) {
        SZrTypeValue errorValue;
        ZrLib_Value_SetString(state, &errorValue, "Scheduler could not root Job completion");
        task_runtime_handle_mark_faulted(state, handle, ZR_THREAD_STATUS_RUNTIME_ERROR, &errorValue);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrLibrary_TaskRuntime_ExecutePreparedJob(SZrState *state,
                                                  ZrLibraryTaskRuntimeWorkItem *item) {
    SZrRawObject *rawTask = ZR_NULL;
    SZrObject *task;

    if (state == ZR_NULL || item == ZR_NULL ||
        !ZrCore_GcRootHandle_Resolve(state, &item->taskRoot, &rawTask) || rawTask == ZR_NULL) {
        return ZR_FALSE;
    }
    task = ZR_CAST_OBJECT(state, rawTask);
    return task_runtime_execute_task(state, task);
}

void ZrLibrary_TaskRuntime_FaultPreparedJob(SZrState *state,
                                            ZrLibraryTaskRuntimeWorkItem *item,
                                            const TZrChar *message) {
    SZrRawObject *rawTask = ZR_NULL;
    SZrObject *task;
    SZrTypeValue errorValue;

    if (state == ZR_NULL || item == ZR_NULL ||
        !ZrCore_GcRootHandle_Resolve(state, &item->taskRoot, &rawTask) || rawTask == ZR_NULL) {
        return;
    }
    task = ZR_CAST_OBJECT(state, rawTask);
    ZrLib_Value_SetString(state, &errorValue, message != ZR_NULL ? message : "ThreadScheduler failed Job execution");
    task_runtime_handle_mark_faulted(state, task, ZR_THREAD_STATUS_RUNTIME_ERROR, &errorValue);
}

void ZrLibrary_TaskRuntime_ReleasePreparedJob(SZrState *state,
                                              ZrLibraryTaskRuntimeWorkItem *item) {
    if (state == ZR_NULL || item == ZR_NULL) {
        return;
    }
    ZrCore_GcRootHandle_Release(state, &item->taskRoot);
    memset(item, 0, sizeof(*item));
}

/**
 * @brief 从 caller-domain Task 复制保留的 callable 给 provider 准备传输；不会消费第二个 Job。
 * @note 副本仍属于当前域；跨域隔离或序列化由 provider 完成，返回值不是 worker-domain Task 根。
 */
TZrBool ZrLibrary_TaskRuntime_CopyPreparedCallable(
        SZrState *state,
        const ZrLibraryTaskRuntimeWorkItem *item,
        SZrTypeValue *outCallable) {
    SZrRawObject *rawTask = ZR_NULL;
    SZrObject *task;
    const SZrTypeValue *callable;

    if (state == ZR_NULL || item == ZR_NULL || outCallable == ZR_NULL ||
        !ZrCore_GcRootHandle_Resolve(state, &item->taskRoot, &rawTask) || rawTask == ZR_NULL) {
        return ZR_FALSE;
    }
    task = ZR_CAST_OBJECT(state, rawTask);
    callable = task_runtime_get_field_value(state, task, kTaskCallableField);
    if (callable == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Value_Copy(state, outCallable, callable);
    return ZR_TRUE;
}

/**
 * @brief 将 provider 返回的当前域结果写入尚未终结的 Task，清空 callable/error 并设为 COMPLETED。
 * @note 重复终态结算返回 false；成功不表示 worker teardown 已完成，也不释放 item 的 GC 根。
 */
TZrBool ZrLibrary_TaskRuntime_CompletePreparedJob(
        SZrState *state,
        ZrLibraryTaskRuntimeWorkItem *item,
        const SZrTypeValue *result) {
    SZrRawObject *rawTask = ZR_NULL;
    SZrObject *task;

    if (state == ZR_NULL || item == ZR_NULL || result == ZR_NULL ||
        !ZrCore_GcRootHandle_Resolve(state, &item->taskRoot, &rawTask) || rawTask == ZR_NULL) {
        return ZR_FALSE;
    }
    task = ZR_CAST_OBJECT(state, rawTask);
    if (ZrLibrary_TaskRuntime_IsTaskComplete(state, task)) {
        return ZR_FALSE;
    }
    task_runtime_set_value_field(state, task, kTaskResultField, result);
    task_runtime_set_null_field(state, task, kTaskErrorField);
    task_runtime_set_null_field(state, task, kTaskCallableField);
    task_runtime_set_int_field(state, task, kTaskStatusField, ZR_VM_TASK_STATUS_COMPLETED);
    return ZR_TRUE;
}

/**
 * @brief 按 prepare、入队、非重入 pump、release 的顺序实现本地 Scheduler.schedule。
 * @note 队列拒绝时先故障结算再释放根；正常同步返回也释放根。Job 的消费不因交接失败而撤销。
 */
static TZrBool task_runtime_schedule_job_on_scheduler(SZrState *state,
                                                       SZrObject *scheduler,
                                                       SZrObject *job,
                                                       SZrTypeValue *result) {
    ZrLibraryTaskRuntimeWorkItem item;
    SZrObject *queue;

    if (!ZrLibrary_TaskRuntime_PrepareJob(state, scheduler, job, result, &item)) {
        return ZR_FALSE;
    }
    queue = task_runtime_scheduler_queue(state, scheduler);
    if (queue == ZR_NULL || !ZrLib_Array_PushValue(state, queue, result)) {
        ZrLibrary_TaskRuntime_FaultPreparedJob(state, &item, "Scheduler rejected Job after consume");
        ZrLibrary_TaskRuntime_ReleasePreparedJob(state, &item);
        return ZR_FALSE;
    }
    if (!task_runtime_get_bool_field(state, scheduler, kTaskIsPumpingField, ZR_FALSE)) {
        task_runtime_scheduler_pump_internal(state, scheduler);
    }
    ZrLibrary_TaskRuntime_ReleasePreparedJob(state, &item);
    return ZR_TRUE;
}

TZrBool ZrLibrary_TaskRuntime_ScheduleJob(SZrState *state,
                                          SZrObject *scheduler,
                                          SZrObject *job,
                                          SZrTypeValue *result) {
    return task_runtime_schedule_job_on_scheduler(state, scheduler, job, result);
}

/**
 * @brief 在 scheduler 中保存 provider 登记结构的原始指针，供 Task.result 调用；不复制或拥有登记。
 * @pre registration、awaitHook 和 context 必须保持有效到 scheduler 不再可能使用该等待路径。
 * @note 此入口不拥有 provider 资源，返回 true 的字段写入限制见函数体保留的 BUG。
 */
TZrBool ZrLibrary_TaskRuntime_RegisterAwaitHook(
        SZrState *state,
        SZrObject *scheduler,
        const ZrLibraryTaskRuntimeAwaitRegistration *registration) {
    SZrTypeValue registrationValue;

    if (state == ZR_NULL || scheduler == ZR_NULL || registration == ZR_NULL || registration->awaitHook == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsNativePointer(state, &registrationValue, (TZrPtr)registration);
    /* BUG: setter 可能静默失败，本函数仍返回 true。ThreadScheduler 随后接受 Job，
     * 但 Task.result 找不到等待钩子而退回本地队列等待，无法处理 provider 任务。 */
    task_runtime_set_value_field(state, scheduler, kTaskProviderAwaitRegistrationField, &registrationValue);
    return ZR_TRUE;
}

/**
 * @brief 发现有效登记时将 outHandled 设为 true，再同步调用其 awaitHook(state, task, context)。
 * @note 没有有效登记返回 true 且保持 outHandled=false；hook 返回 false 时 handled 仍表示已选择 provider。
 */
TZrBool ZrLibrary_TaskRuntime_AwaitProviderTask(
        SZrState *state,
        SZrObject *scheduler,
        SZrObject *task,
        TZrBool *outHandled) {
    const SZrTypeValue *registrationValue;
    const ZrLibraryTaskRuntimeAwaitRegistration *registration;

    if (outHandled != ZR_NULL) {
        *outHandled = ZR_FALSE;
    }
    if (state == ZR_NULL || scheduler == ZR_NULL || task == ZR_NULL) {
        return ZR_FALSE;
    }
    registrationValue = task_runtime_get_field_value(state, scheduler, kTaskProviderAwaitRegistrationField);
    if (registrationValue == ZR_NULL || registrationValue->type != ZR_VALUE_TYPE_NATIVE_POINTER ||
        registrationValue->value.nativeObject.nativePointer == ZR_NULL) {
        return ZR_TRUE;
    }
    registration = (const ZrLibraryTaskRuntimeAwaitRegistration *)registrationValue->value.nativeObject.nativePointer;
    if (registration->awaitHook == ZR_NULL) {
        return ZR_TRUE;
    }
    if (outHandled != ZR_NULL) {
        *outHandled = ZR_TRUE;
    }
    return registration->awaitHook(state, task, registration->context);
}

/** @brief 只观察 COMPLETED/FAULTED 状态；不推进队列、不等待 provider worker 退出，也不释放 WorkItem。 */
TZrBool ZrLibrary_TaskRuntime_IsTaskComplete(SZrState *state, SZrObject *task) {
    TZrInt64 status;

    if (state == ZR_NULL || task == ZR_NULL) {
        return ZR_FALSE;
    }
    status = task_runtime_get_int_field(state, task, kTaskStatusField, ZR_VM_TASK_STATUS_CREATED);
    return status == ZR_VM_TASK_STATUS_COMPLETED || status == ZR_VM_TASK_STATUS_FAULTED;
}

/** @brief Job constructor 只保存 callable 并初始化 consumed=false；实际执行从 scheduler 交接路径开始。 */
static TZrBool task_runtime_create_job(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *job = task_runtime_self_object(context);
    SZrTypeValue *callable;

    if (context == ZR_NULL || result == ZR_NULL || job == ZR_NULL ||
        !ZrLib_CallContext_ReadFunction(context, 0, &callable)) {
        return ZR_FALSE;
    }

    task_runtime_set_value_field(context->state, job, kTaskJobCallableField, callable);
    task_runtime_set_bool_field(context->state, job, kTaskJobConsumedField, ZR_FALSE);
    return task_runtime_finish_object(context->state, result, job);
}

static TZrBool task_runtime_task_result(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *handle = task_runtime_self_object(context);

    return task_runtime_wait_for_task(context->state, handle, result);
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

static TZrBool task_runtime_scheduler_schedule_method(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *scheduler = task_runtime_self_object(context);
    SZrObject *job;

    if (context == ZR_NULL || result == ZR_NULL || scheduler == ZR_NULL ||
        !ZrLib_CallContext_ReadObject(context, 0, &job)) {
        return ZR_FALSE;
    }

    return task_runtime_schedule_job_on_scheduler(context->state, scheduler, job, result);
}

static TZrBool task_runtime_yield_now(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *scheduler;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    scheduler = task_runtime_ensure_current_scheduler(context->state);
    return scheduler != ZR_NULL && task_runtime_create_cooperative_task(context->state, scheduler, 1, result);
}

/**
 * @brief 当前本地 delay 将非负整数解释为 cooperative 重排次数，返回 Task<void>。
 * @note descriptor 参数写作 Duration；此 callback 实际读取整数 turns，没有墙钟、休眠或 timer provider 调用。
 */
static TZrBool task_runtime_delay(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *scheduler;
    TZrInt64 turns;

    if (context == ZR_NULL || result == ZR_NULL || !ZrLib_CallContext_ReadInt(context, 0, &turns) || turns < 0) {
        return ZR_FALSE;
    }

    scheduler = task_runtime_ensure_current_scheduler(context->state);
    return scheduler != ZR_NULL && task_runtime_create_cooperative_task(context->state, scheduler, turns, result);
}

/** @brief 从正在物化的模块建立或复用 global 当前 scheduler，并发布 currentScheduler 导出。 */
static TZrBool task_runtime_task_module_materialize(SZrState *state,
                                                    SZrObjectModule *module,
                                                    const ZrLibModuleDescriptor *descriptor) {
    SZrObject *scheduler;
    SZrString *currentSchedulerName;
    SZrTypeValue value;

    ZR_UNUSED_PARAMETER(descriptor);
    if (state == ZR_NULL || module == ZR_NULL) {
        return ZR_FALSE;
    }

    scheduler = task_runtime_ensure_current_scheduler_for_module(state, module);
    currentSchedulerName = ZrCore_String_Create(state, "currentScheduler", strlen("currentScheduler"));
    if (scheduler == ZR_NULL || currentSchedulerName == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(state, &value, scheduler, ZR_VALUE_TYPE_OBJECT);
    ZrCore_Module_AddPubExport(state, module, currentSchedulerName, &value);
    return ZR_TRUE;
}

static const ZrLibGenericParameterDescriptor g_task_single_generic_parameter[] = {
        {
                .name = "T",
                .documentation = "The task payload type.",
        },
};

static const ZrLibParameterDescriptor g_scheduler_schedule_parameters[] = {
        {"job", "zr.task.Job<T>", "The cold Job consumed by this scheduler."},
};

static const ZrLibParameterDescriptor g_delay_parameters[] = {
        {"duration", "Duration", "The provider-owned timer duration."},
};

static const ZrLibParameterDescriptor g_job_constructor_parameters[] = {
        {"callable", "function", "Hidden callable backing an async function body."},
};

static const ZrLibMethodDescriptor g_task_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("result", 0, 0, task_runtime_task_result, "T",
                                      "Resolve the task and return its completion value.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("isCompleted", 0, 0, task_runtime_task_is_completed, "bool",
                                      "Return whether the task has completed or faulted.", ZR_FALSE, ZR_NULL, 0),
};

static const ZrLibMethodDescriptor g_task_scheduler_methods[] = {
        {"schedule", 1, 1, task_runtime_scheduler_schedule_method, "zr.task.Task<T>",
         "Consume a cold Job and publish its Task completion handle.", ZR_FALSE, g_scheduler_schedule_parameters,
         ZR_ARRAY_COUNT(g_scheduler_schedule_parameters), ZR_MEMBER_CONTRACT_ROLE_TASK_SCHEDULER_SCHEDULE,
         g_task_single_generic_parameter, ZR_ARRAY_COUNT(g_task_single_generic_parameter), 0U},
};

static const ZrLibMetaMethodDescriptor g_job_meta_methods[] = {
        {
                .metaType = ZR_META_CONSTRUCTOR,
                .minArgumentCount = 1,
                .maxArgumentCount = 1,
                .callback = task_runtime_create_job,
                .returnTypeName = "zr.task.Job<T>",
                .documentation = "Create a cold Job from a callable returning T or Task<T>.",
                .parameters = g_job_constructor_parameters,
                .parameterCount = ZR_ARRAY_COUNT(g_job_constructor_parameters),
                .genericParameters = g_task_single_generic_parameter,
                .genericParameterCount = ZR_ARRAY_COUNT(g_task_single_generic_parameter),
                .contractRole = ZR_MEMBER_CONTRACT_ROLE_TASK_JOB_CONSTRUCT,
        },
};

static const ZrLibFunctionDescriptor g_task_functions[] = {
        {"yieldNow", 0, 0, task_runtime_yield_now, "zr.task.Task<void>",
         "Yield once through the current scheduler's Task completion ABI.", ZR_NULL, 0, ZR_NULL, 0,
         ZR_MEMBER_CONTRACT_ROLE_TASK_YIELD_NOW, 0U},
        {"delay", 1, 1, task_runtime_delay, "zr.task.Task<void>",
         "Complete through the current scheduler after a provider Duration.", g_delay_parameters,
         ZR_ARRAY_COUNT(g_delay_parameters), ZR_NULL, 0, ZR_MEMBER_CONTRACT_ROLE_TASK_DELAY, 0U},
};

static const ZrLibTypeDescriptor g_task_types[] = {
        ZR_LIB_TYPE_DESCRIPTOR_PROTOCOL_INIT("Task", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, g_task_methods,
                                             ZR_ARRAY_COUNT(g_task_methods), ZR_NULL, 0,
                                             "Started task handle that can be awaited.", ZR_NULL, ZR_NULL, 0, ZR_NULL, 0,
                                             ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL, g_task_single_generic_parameter,
                                             ZR_ARRAY_COUNT(g_task_single_generic_parameter),
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_TASK_HANDLE)),
        ZR_LIB_TYPE_DESCRIPTOR_PROTOCOL_INIT("Job", ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, ZR_NULL, 0, ZR_NULL, 0,
                                             g_job_meta_methods, ZR_ARRAY_COUNT(g_job_meta_methods),
                                             "Cold non-Copy work consumed exactly once by Scheduler.schedule.", ZR_NULL,
                                             ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE,
                                             "init Job<T>(callable: fn() -> T | fn() -> zr.task.Task<T>)",
                                             g_task_single_generic_parameter, ZR_ARRAY_COUNT(g_task_single_generic_parameter),
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_TASK_JOB)),
        ZR_LIB_TYPE_DESCRIPTOR_PROTOCOL_INIT("Scheduler", ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE, ZR_NULL, 0,
                                             g_task_scheduler_methods, ZR_ARRAY_COUNT(g_task_scheduler_methods),
                                             ZR_NULL, 0, "Capability that consumes a Job and returns a Task.", ZR_NULL,
                                             ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0,
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_TASK_SCHEDULER)),
};

static const ZrLibTypeHintDescriptor g_task_hints[] = {
        {"currentScheduler", "property", "currentScheduler: Scheduler",
         "Readonly current Scheduler capability."},
        {"Task", "type", "class Task<T>", "Started task handle that can be awaited."},
        {"Job", "type", "struct Job<T>", "Cold non-Copy work consumed by Scheduler.schedule."},
        {"Scheduler", "type", "interface Scheduler", "Canonical scheduler capability."},
        {"yieldNow", "function", "yieldNow(): Task<void>", "Yield through currentScheduler."},
        {"delay", "function", "delay(duration: Duration): Task<void>", "Timer Task through currentScheduler."},
};

static const TZrChar g_task_hints_json[] =
        "{\n"
        "  \"schema\": \"zr.native.hints/v1\",\n"
        "  \"module\": \"zr.task\"\n"
        "}\n";

/**
 * @brief 单一 zr.task runtime-phase 描述符把 Job constructor、Task 查询与 Scheduler 回调公开给 native registry。
 * @note contract role 和 protocol mask 供编译器识别能力；实际调度与资源所有权由对应 runtime 路径承担。
 */
static const ZrLibModuleDescriptor g_task_descriptor = {
        .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        .moduleName = "zr.task",
        .functions = g_task_functions,
        .functionCount = ZR_ARRAY_COUNT(g_task_functions),
        .types = g_task_types,
        .typeCount = ZR_ARRAY_COUNT(g_task_types),
        .typeHints = g_task_hints,
        .typeHintCount = ZR_ARRAY_COUNT(g_task_hints),
        .typeHintsJson = g_task_hints_json,
        .documentation = "Built-in Task, Job, and Scheduler abstractions.",
        .moduleVersion = "3.0.0",
        .minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
        .onMaterialize = task_runtime_task_module_materialize,
        .providerPhase = ZR_LIBRARY_PROVIDER_PHASE_RUNTIME,
        .publicContractHash = "zr.task:v3:task-job-scheduler",
};

TZrBool ZrCore_TaskRuntime_RegisterBuiltins(SZrGlobalState *global) {
    if (global == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrLibrary_NativeRegistry_Attach(global)) {
        return ZR_FALSE;
    }

    return ZrLibrary_NativeRegistry_RegisterModule(global, &g_task_descriptor);
}
