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

/* descriptor 元素计数仅用于数组；不接受指针。外部已有定义时沿用。 */
#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

/** @brief TryRun 同步回调的栈上请求；借用 callable，result 与 completed 在捕获返回后供 Task 结算使用。 */
/* 同步 TryRun 请求；callable 借用调用期间的值，结果只在捕获返回后结算。 */
typedef struct ZrVmTaskExecuteRequest {
    /* 借用 Task 字段中的 callable；只供本次同步 CallValue 使用。 */
    const SZrTypeValue *callable;
    /* CallValue 的一个结果槽；handler 清理阶段另以本地根保护其中对象。 */
    SZrTypeValue result;
    /* CallValue 正常返回的布尔结果；不等于 Task terminal 或 worker 退出。 */
    TZrBool completed;
} ZrVmTaskExecuteRequest;

/**
 * @brief Job、Task 与 Scheduler 的私有字段协议；字段名由本文件及包含的队列 helper 共用。
 * @note provider 通过 WorkItem 与 await hook API 交接；completion 状态不代表 worker 已退出。
 */
/* 原生导入与类型导出查询使用同一个模块身份。 */
static const TZrChar *kTaskModuleName = "zr.task";
/* global 根字段保存当前默认 Scheduler，不按 Task 分配新 Scheduler。 */
static const TZrChar *kTaskRootSchedulerField = "__zr_task_scheduler";
/* 当前 Scheduler 持有的数组；耗尽可脱开，replacement 另建。 */
static const TZrChar *kTaskQueueField = "__zr_task_queue";
/* 下一消费位置；耗尽脱开时保留，replacement 发布前确认归零。 */
static const TZrChar *kTaskQueueHeadField = "__zr_task_queue_head";
/* 同 Scheduler 外层 pump 标志，避免 callback 内 schedule 递归执行。 */
static const TZrChar *kTaskIsPumpingField = "__zr_task_is_pumping";
/* CREATED/QUEUED/RUNNING 到 COMPLETED 或 FAULTED 的可观察状态。 */
static const TZrChar *kTaskStatusField = "__zr_task_status";
/* Task 待执行 callable；完成或故障后清空。 */
static const TZrChar *kTaskCallableField = "__zr_task_callable";
/* Task 保存的完成值；result 查询复制此值，未执行 ownership move。 */
static const TZrChar *kTaskResultField = "__zr_task_result";
/* Task 保存的规范化错误或 fallback；result 查询重新抛出。 */
static const TZrChar *kTaskErrorField = "__zr_task_error";
/* Task 归属 Scheduler，供 result 选择 provider hook 或本地推进。 */
static const TZrChar *kTaskSchedulerOwnerField = "__zr_task_scheduler_owner";
/* cold Job 一次性交给 Task 的 callable，不在构造时执行。 */
static const TZrChar *kTaskJobCallableField = "__zr_task_job_callable";
/* Prepare 在建 Task 前设置的消费标志，失败没有回滚承诺。 */
static const TZrChar *kTaskJobConsumedField = "__zr_task_job_consumed";
/* 区分空结果 cooperative 项与需要调用 callable 的项。 */
static const TZrChar *kTaskCooperativeTaskField = "__zr_task_cooperative_task";
/* 还需重排的 cooperative 次数，不是墙钟时长。 */
static const TZrChar *kTaskCooperativeTurnsField = "__zr_task_cooperative_turns";
/* 保存借用 registration 原生指针；其生命周期由 provider 保证。 */
static const TZrChar *kTaskProviderAwaitRegistrationField = "__zr_task_provider_await_registration";

/* 这些私有字段形成 Job -> Task -> Scheduler 的共享 ABI：Job 仅提供一次性 callable，
 * Task 持有完成状态与归属 scheduler，thread provider 通过公开 handoff API 访问它们。 */

/* 取得原生方法接收者的借用对象，供 Job、Task 和 Scheduler 回调访问私有状态。
 * context 来自原生分派或等价测试上下文；仅接受非空 object/array；返回地址不转移所有权。 */
static SZrObject *task_runtime_self_object(const ZrLibCallContext *context) {
    SZrTypeValue *selfValue = ZrLib_CallContext_Self(context);

    if (selfValue == ZR_NULL || (selfValue->type != ZR_VALUE_TYPE_OBJECT && selfValue->type != ZR_VALUE_TYPE_ARRAY) ||
        selfValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(context->state, selfValue->value.object);
}

/* 让对象返回值保留 array 标签，其他对象统一按 object 包装。
 * 只决定值标签，不检查原型或创建对象；null 也给出 object 标签，调用者先检查空值。 */
static EZrValueType task_runtime_value_type_for_object(SZrObject *object) {
    return object != ZR_NULL && object->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY ? ZR_VALUE_TYPE_ARRAY
                                                                                       : ZR_VALUE_TYPE_OBJECT;
}

/* 把已构建的对象发布到调用者结果槽，复用正确对象标签。
 * state/result/object 非空；写结果槽不创建跨域根或释放对象。 */
static TZrBool task_runtime_finish_object(SZrState *state, SZrTypeValue *result, SZrObject *object) {
    if (state == ZR_NULL || result == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(state, result, object, task_runtime_value_type_for_object(object));
    return ZR_TRUE;
}

/* 统一写入私有状态字段，使 Job 消费、Task 结算和 provider 登记共用字段协议。
 * void 底层接口没有提交确认；BUG 仅静态证明 ignore registry 扩容失败时写入可被跳过。 */
static void task_runtime_set_value_field(SZrState *state,
                                         SZrObject *object,
                                         const TZrChar *fieldName,
                                         const SZrTypeValue *value) {
    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL) {
        return;
    }

    /* BUG: ignore registry 已满且宿主 ARRAY 扩容分配返回 null 时，底层 pin 失败
     * 会跳过字段写入；此 void 包装不回传失败，登记 hook 等调用者仍可报告成功。 */
    ZrLib_Object_SetFieldCString(state, object, fieldName, value);
}

/* 解除私有字段保存的 callable/result/error 等引用。
 * 继承 void 字段写入的失败限制；此处没有 ownership 显式释放协议。 */
static void task_runtime_set_null_field(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    SZrTypeValue value;

    ZrLib_Value_SetNull(&value);
    task_runtime_set_value_field(state, object, fieldName, &value);
}

/* 写入消费、泵送和 cooperative 标志，供后续状态分支读取。
 * 继承 void setter 限制；标志写入本身不是同步或线程互斥。 */
static void task_runtime_set_bool_field(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrBool value) {
    SZrTypeValue fieldValue;

    ZrLib_Value_SetBool(state, &fieldValue, value);
    task_runtime_set_value_field(state, object, fieldName, &fieldValue);
}

/* 写入 Task 状态、队头和 cooperative 轮数的整数字段。
 * 值按 int64 包装；更新队列 helper 的已有 pair 使用另一个可检查 setter。 */
static void task_runtime_set_int_field(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrInt64 value) {
    SZrTypeValue fieldValue;

    ZrLib_Value_SetInt(state, &fieldValue, value);
    task_runtime_set_value_field(state, object, fieldName, &fieldValue);
}

/**
 * @brief 借用对象字段值供状态检查或随即复制；缺失字段返回 NULL。
 * @note 返回值不是拥有副本；调用方不能把此地址当作跨分配、字段修改或对象生命周期的稳定句柄。
 */
/* 借用私有字段值供立即检查或复制，缺失字段返回 null。
 * 字段地址不是稳定根；不能跨字段改动、分配或对象生命周期长期保存。 */
static const SZrTypeValue *task_runtime_get_field_value(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrLib_Object_GetFieldCString(state, object, fieldName);
}

/* 从私有字段提取 queue、scheduler 等对象引用。
 * 接受 object/array 标签且对象非空；仅借用，不验证能力原型、不创建 GC 根。 */
static SZrObject *task_runtime_get_object_field(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    const SZrTypeValue *value = task_runtime_get_field_value(state, object, fieldName);

    if (value == ZR_NULL || (value->type != ZR_VALUE_TYPE_OBJECT && value->type != ZR_VALUE_TYPE_ARRAY) ||
        value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, value->value.object);
}

/* 读取私有布尔标志，并为缺失或异型字段采用调用者默认值。
 * 只对 bool 标签解码；不会修复损坏字段。 */
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

/* 读取状态、队头或轮数，兼容有符号和无符号整数字段。
 * uint64 按现有 cast 转 int64；缺失或非整数采用默认值，不增加范围保证。 */
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

/* 向结果槽交付已有完成值，缺失值以 null 表示。
 * result 必须非空；state/value 非空时使用 Value_Copy，不承诺 ownership move 或跨域 transfer。 */
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

/* 取得当前 global 的根对象以保存默认 Scheduler 能力。
 * 只接受 global 的 object 标签；借用对象且不创建根。 */
static SZrObject *task_runtime_root_object(SZrState *state) {
    if (state == ZR_NULL || state->global == ZR_NULL || state->global->zrObject.type != ZR_VALUE_TYPE_OBJECT ||
        state->global->zrObject.value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, state->global->zrObject.value.object);
}

/* 把调度契约违反规范化为当前线程错误，让原生回调返回失败。
 * 正常返回 false 且设置状态；字符串创建/规范化失败可经 DebugRunError 非局部离开。 */
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

/* 在 Task.result 消费已故障 Task 时重新抛出其保存的错误。
 * 不正常返回成功；Normalize/Throw 使用当前 state 恢复点，调用者不能依赖其后普通返回清理。 */
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

/* 先复用已加载 zr.task，再经 Module_ImportByPath 导入，供运行期建实例。
 * 导入可能分配或失败；物化回调使用 loaded-module 专用路径避免递归导入。 */
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

/* 从运行期导入的模块读取指定类型导出，为 typed object 创建提供原型。
 * export 名称创建可能分配；导出值为模块借用值。 */
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

/* 只从正在物化的已有模块取类型导出，避免初始化 Scheduler 时再次导入自身。
 * module/name 非空；名称创建可能分配；返回模块字段借用值。 */
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

/* 按导出 prototype 或按名称构造 Job/Task/Scheduler，保留可用原生类型身份。
 * 原型存在时用 WithPrototype，否则用按名 NewInstance；任一路径返回 null 才回退普通对象，回退不保证同一原型身份。 */
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

/* 物化期间使用已有模块原型或按名建对象，免除循环导入。
 * 有原型用 WithPrototype，无原型用 NewInstance；失败回退普通对象；不保证所需字段已写入。 */
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

/* 在 global 根字段复用默认 Scheduler，缺失时建立本地队列并发布能力。
 * root 字段先发布 Scheduler，随后写 queue/head/pumping；void 写入失败没有事务回滚。 */
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
/* 在 zr.task 物化过程中建立同一个默认 Scheduler，供 currentScheduler 导出。
 * 使用 loaded-module 构造路径；复用已有根字段；字段初始化仍受 void setter 限制。 */
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

/* 作为 TryRun 同步回调执行 Job 的无参数 callable，并交回一个完成结果。
 * request 仅在外层同步调用存活；CallValue 返回布尔并请求一结果，不执行 core task-frame poll。 */
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
/* 把执行错误留在 Task.error 并清除工作引用，恢复线程可继续处理其他 Task 的状态。
 * 优先保存当前异常，必要时规范化或采用 fallback；不释放 WorkItem 根、不等待 worker 退出。 */
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
/* 同步执行 Task callable，把回调结果或异常结算到原 Task。
 * 保存栈锚点/handler 深度/根链；清理残留 handler 时本地根保护 Task 与结果；true 可表示已故障结算。TODO: callback 内 GC 时 handle 裸地址的稳定性需结合上游 pin/domain root 重定位核查，清理期局部根不能授予该窗口信用。 */
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
    /* 丢弃 callback 根链后，临时根保护 Task/result，清理 callback 遗留 handler 并采用清理失败状态。
     * handler 清理会 ownership drop 和可能 GC；不恢复 callback 的悬空栈根。 */
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
/* 消费一个队头项或 cooperative 轮次，让等待和外层 pump 共用推进规则。
 * 先推进 head 再运行回调；缺失项脱开耗尽数组；无效项也算已推进；cooperative 不是墙钟 timer。 */
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

    /* 消费游标在回调前推进，嵌套入队只追加后续项。
     * 耗尽数组只脱开，不复位其旧 head；避免完成项重放。 */
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
/* 在同一 Scheduler 的外层调用中排空可推进项，让嵌套 schedule 留待当前回调返回。
 * pumping 标志抑制递归 pump；计数是 step 次数；不是 OS 线程调度或 coreframe suspend。 */
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

/* 构造保存 callable、完成值、错误与归属 Scheduler 的 Task 对象。
 * 初始 CREATED；返回借用 VM 对象，provider 需另建 WorkItem 根；字段写入无提交确认。 */
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
/* 把 yieldNow/delay 转成当前本地 Scheduler 的空结果 cooperative Task。
 * 轮数而非时间；队列 push 失败故障化；非 pumping 时立即 pump；不接入 coreframe runtime。 */
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
/* 消费 terminal Task，或通过所属 provider/本地 queue 推进到可读完成值。
 * provider hook 先接管并可按其策略同步等待；handled 后递归重查，hook 必须真正推进；无 hook 的本地队列在 pumping 时不递归推进 pending Task；完成值可重复复制。 */
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
    /* provider hook 负责外部推进，无 hook 时本地等待只在非 pumping 状态推进。
     * handled hook 返回后递归重查；hook 需真实推进以终结等待。 */
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
/** @brief 消费 cold Job 并交付带 domain GC 根的 WorkItem，供本地或 thread provider 接管。
 * @note outItem 必须是空新槽；消费标志在建 Task 前写入，失败不回滚；成功后同 domain Release；不跨域复制 callable。 */
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
    /* 消费 cold Job 后转移 callable 到 Task 并建立 domain root 交接。
     * 建 Task 或 root 失败不恢复消费；outItem 必须新槽，同 domain Release。 */
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

/** @brief 在兼容 domain 的 state 中解析 WorkItem Task 并同步结算 callable。
 * @note 有效未释放 WorkItem；不释放根；true 包括 Task 故障已结算，不代表 callable 成功。 */
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

/** @brief 让 provider 的提交、启动或传输失败成为原 Task 的错误。
 * @note 同 domain WorkItem；不释放根；没有 terminal 幂等保护，调用者须控制一次结算顺序。 */
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

/** @brief 结束 provider 对 Task 的保活交接，释放 domain 根并清空 WorkItem。
 * @note state 必须属于原 root domain；只结束这份根句柄，不等待 worker 或改变 Task 状态。 */
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
/** @brief 让 isolated provider 在 caller domain 读取已准备 Task 的 callable，再自行建立跨域传输。
 * @note 复制到调用者槽，不转移 WorkItem、不创建跨域根；callable 缺失或根解析失败返回 false。 */
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
/** @brief 由 caller domain 接收 isolated 完成值并结算原 Task。
 * @note 值跨域 Claim/Commit 由 provider 先完成；terminal Task 拒绝重复成功；不释放 WorkItem；void setter 限制仍在。 */
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
/* 本地 Scheduler 消费 Job、入队并在外层泵送，最后释放临时 WorkItem。
 * enqueue 拒绝后故障化并释放；嵌套 pump 被抑制；这不是 provider worker handoff。 */
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

/** @brief 向库内或测试调用者提供默认本地 Scheduler 的 Job 提交入口。
 * @note 同 domain Job/result 有效；返回 Task 结果槽；内部可能立即同步执行，失败消费不回滚。 */
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
/** @brief 把 provider 的同步等待入口以借用 registration 指针保存到 Scheduler。
 * @note registration/context/hook 须覆盖 Scheduler 使用期；没有复制、析构或 unregister；void setter 可静默跳过写入。 */
TZrBool ZrLibrary_TaskRuntime_RegisterAwaitHook(
        SZrState *state,
        SZrObject *scheduler,
        const ZrLibraryTaskRuntimeAwaitRegistration *registration) {
    SZrTypeValue registrationValue;

    if (state == ZR_NULL || scheduler == ZR_NULL || registration == ZR_NULL || registration->awaitHook == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsNativePointer(state, &registrationValue, (TZrPtr)registration);
    /* BUG: scheduler 尚未 ignored、registry 已满且宿主 ARRAY 扩容失败时，
     * hook 字段可未写而返回 true；pending Task.result 随后不能选择 provider 等待。 */
    task_runtime_set_value_field(state, scheduler, kTaskProviderAwaitRegistrationField, &registrationValue);
    return ZR_TRUE;
}

/**
 * @brief 发现有效登记时将 outHandled 设为 true，再同步调用其 awaitHook(state, task, context)。
 * @note 没有有效登记返回 true 且保持 outHandled=false；hook 返回 false 时 handled 仍表示已选择 provider。
 */
/** @brief 辨别 Scheduler 是否有 provider 等待能力，并同步转交 Task 和借用 context。
 * @note outHandled 先置 false；无 hook 为成功但未处理；有 hook 先置 true 即使回调失败；不释放 registration。 */
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
/** @brief 供等待循环和重复完成检查识别 COMPLETED/FAULTED 两种 terminal 状态。
 * @note 纯状态查询；不推进 queue、不释放根、不证明 worker 已结束或 frame 已 free。 */
TZrBool ZrLibrary_TaskRuntime_IsTaskComplete(SZrState *state, SZrObject *task) {
    TZrInt64 status;

    if (state == ZR_NULL || task == ZR_NULL) {
        return ZR_FALSE;
    }
    status = task_runtime_get_int_field(state, task, kTaskStatusField, ZR_VM_TASK_STATUS_CREATED);
    return status == ZR_VM_TASK_STATUS_COMPLETED || status == ZR_VM_TASK_STATUS_FAULTED;
}

/** @brief Job constructor 只保存 callable 并初始化 consumed=false；实际执行从 scheduler 交接路径开始。 */
/* Job 原生构造器保存 cold callable 和未消费标志，工作等到 schedule 才执行。
 * context/self 有效且参数0可读为函数；返回同一 Job 接收者；没有立即执行或跨域复制。 */
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

/* Task.result 描述符回调把接收者交给等待与错误重抛路径。
 * 有效原生 context；result 非空；运行中的同泵 pending Task 受同步等待限制。 */
static TZrBool task_runtime_task_result(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *handle = task_runtime_self_object(context);

    return task_runtime_wait_for_task(context->state, handle, result);
}

/* Task.isCompleted 回调只交付 terminal 状态布尔值。
 * 错误接收者返回 false；检查不会推进 Task 或回收 provider 资源。 */
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

/* Scheduler.schedule 描述符消费 Job 参数并返回本地 Task。
 * context/self/job/result 有效；descriptor contractRole 供编译器识别消费第0实参；执行仍由本地 queue 完成。 */
static TZrBool task_runtime_scheduler_schedule_method(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *scheduler = task_runtime_self_object(context);
    SZrObject *job;

    if (context == ZR_NULL || result == ZR_NULL || scheduler == ZR_NULL ||
        !ZrLib_CallContext_ReadObject(context, 0, &job)) {
        return ZR_FALSE;
    }

    return task_runtime_schedule_job_on_scheduler(context->state, scheduler, job, result);
}

/* yieldNow 原生模块回调创建一个 cooperative 轮次的空结果 Task。
 * 保证为当前 Scheduler 路径的轮次语义；不保证 OS yield 或异步 frame 挂起。 */
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
/* delay 原生回调把 ReadInt 转换出的非负值解释为 cooperative 轮数。
 * descriptor 声明 Duration，此实现使用 ReadInt（含无符号整数及浮点转换）；不保证墙钟延迟。TODO: 从 native 参数验证和 Duration 类型消费入口核查转换契约。 */
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
/* zr.task 模块物化末尾导出默认 currentScheduler 能力。
 * 传入已建好类型的模块，避免自递归导入；AddPubExport 为 void，true 不独立确认导出写入成功。 */
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

/* Job、Task 与 schedule 共用载荷类型参数 T 的静态描述。 */
static const ZrLibGenericParameterDescriptor g_task_single_generic_parameter[] = {
        {
                .name = "T",
                .documentation = "The task payload type.",
        },
};

/* 第0参数的 Job<T> 形状与 schedule 消费 role 一起进入编译器契约。 */
static const ZrLibParameterDescriptor g_scheduler_schedule_parameters[] = {
        {"job", "zr.task.Job<T>", "The cold Job consumed by this scheduler."},
};

/* 发布 Duration 参数形状；当前本地回调仍按整数轮数读取，不能借文案授予 timer 语义。 */
static const ZrLibParameterDescriptor g_delay_parameters[] = {
        {"duration", "Duration", "The provider-owned timer duration."},
};

/* 隐藏 callable 参数是 cold Job 的工作载体，构造阶段不执行。 */
static const ZrLibParameterDescriptor g_job_constructor_parameters[] = {
        {"callable", "function", "Hidden callable backing an async function body."},
};

/* Task 完成查询与结果消费回调表，经 registry 建 native closure 后分派。 */
static const ZrLibMethodDescriptor g_task_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("result", 0, 0, task_runtime_task_result, "T",
                                      "Resolve the task and return its completion value.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("isCompleted", 0, 0, task_runtime_task_is_completed, "bool",
                                      "Return whether the task has completed or faulted.", ZR_FALSE, ZR_NULL, 0),
};

/* schedule 的泛型、消费 role 与回调共同定义本地能力入口。 */
static const ZrLibMethodDescriptor g_task_scheduler_methods[] = {
        {"schedule", 1, 1, task_runtime_scheduler_schedule_method, "zr.task.Task<T>",
         "Consume a cold Job and publish its Task completion handle.", ZR_FALSE, g_scheduler_schedule_parameters,
         ZR_ARRAY_COUNT(g_scheduler_schedule_parameters), ZR_MEMBER_CONTRACT_ROLE_TASK_SCHEDULER_SCHEDULE,
         g_task_single_generic_parameter, ZR_ARRAY_COUNT(g_task_single_generic_parameter), 0U},
};

/* Job constructor 元方法及 TASK_JOB_CONSTRUCT role，共用静态 callable 描述。 */
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

/* yieldNow/delay 的回调与 role；本地实现提供 cooperative 轮次。 */
static const ZrLibFunctionDescriptor g_task_functions[] = {
        {"yieldNow", 0, 0, task_runtime_yield_now, "zr.task.Task<void>",
         "Yield once through the current scheduler's Task completion ABI.", ZR_NULL, 0, ZR_NULL, 0,
         ZR_MEMBER_CONTRACT_ROLE_TASK_YIELD_NOW, 0U},
        {"delay", 1, 1, task_runtime_delay, "zr.task.Task<void>",
         "Complete through the current scheduler after a provider Duration.", g_delay_parameters,
         ZR_ARRAY_COUNT(g_delay_parameters), ZR_NULL, 0, ZR_MEMBER_CONTRACT_ROLE_TASK_DELAY, 0U},
};

/* Task handle、非 Copy 的 Job 与 Scheduler capability 的 protocol/type 形状。 */
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

/* 供工具展示的静态提示；文字不是 timer、worker 或 frame 的执行证明。 */
static const ZrLibTypeHintDescriptor g_task_hints[] = {
        {"currentScheduler", "property", "currentScheduler: Scheduler",
         "Readonly current Scheduler capability."},
        {"Task", "type", "class Task<T>", "Started task handle that can be awaited."},
        {"Job", "type", "struct Job<T>", "Cold non-Copy work consumed by Scheduler.schedule."},
        {"Scheduler", "type", "interface Scheduler", "Canonical scheduler capability."},
        {"yieldNow", "function", "yieldNow(): Task<void>", "Yield through currentScheduler."},
        {"delay", "function", "delay(duration: Duration): Task<void>", "Timer Task through currentScheduler."},
};

/* 原生提示 schema/module 的嵌入式字节串，保持原样供元数据消费。 */
static const TZrChar g_task_hints_json[] =
        "{\n"
        "  \"schema\": \"zr.native.hints/v1\",\n"
        "  \"module\": \"zr.task\"\n"
        "}\n";

/**
 * @brief 单一 zr.task runtime-phase 描述符把 Job constructor、Task 查询与 Scheduler 回调公开给 native registry。
 * @note contract role 和 protocol mask 供编译器识别能力；实际调度与资源所有权由对应 runtime 路径承担。
 */
/* 静态 runtime provider 身份汇合所有表，物化 hook 发布 currentScheduler。 */
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

/** @brief 为 CLI、worker 或测试 global 挂接 registry 并注册 zr.task 运行期描述符。
 * @note global 有效；注册不执行 Job，也不创建 core task-frame；registry 持有静态 descriptor 身份。 */
TZrBool ZrCore_TaskRuntime_RegisterBuiltins(SZrGlobalState *global) {
    if (global == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrLibrary_NativeRegistry_Attach(global)) {
        return ZR_FALSE;
    }

    return ZrLibrary_NativeRegistry_RegisterModule(global, &g_task_descriptor);
}
