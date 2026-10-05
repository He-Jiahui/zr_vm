//
// Created by HeJiahui on 2025/6/18.
//

#include "zr_vm_core/exception.h"
#include "exception_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/closure.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/log.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

/* Stack traces expose 0 as "no mapped source line" instead of the debug-hook-only 0xFFFFFFFF sentinel. */
/* 零同时写入结构化帧与格式化默认行 */
#define ZR_EXCEPTION_SOURCE_LINE_NONE ((TZrUInt32)0u)
/* 非托管诊断暂存区有固定上限，格式化失败时上层仍可返回状态。 */
/* 缓冲含终止零；实际 length 由 Debug 返回 */
#define ZR_EXCEPTION_TRACEBACK_BUFFER_SIZE 4096u

/* 进程内只读 NUL 文本；不属于 VM 托管字符串 */
static const TZrChar kZrExceptionDefaultRuntimeStatusFaultMessage[] = "Runtime status fault";

/* 错误对象字段由宿主字符串键写入，供解释器和 AOT 的统一 catch/诊断路径读取。 */
/* 字段名转成 VM 字符串后作键；无效参数或键构造普通失败会静默返回，函数没有写入成功信号。 */
static void exception_set_object_field_cstring(SZrState *state,
                                               SZrObject *object,
                                               const TZrChar *fieldName,
                                               const SZrTypeValue *value) {
    SZrString *fieldString;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL) {
        return;
    }

    fieldString = ZrCore_String_CreateFromNative(state, (TZrNativeString)fieldName);
    if (fieldString == ZR_NULL) {
        return;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Object_SetValue(state, object, &key, value);
}

/* 返回对象字段的借用地址；字段名转换可能分配，返回值不可当成独立拥有的副本。 */
static const SZrTypeValue *exception_get_object_field_cstring(SZrState *state,
                                                              SZrObject *object,
                                                              const TZrChar *fieldName) {
    SZrString *fieldString;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    fieldString = ZrCore_String_CreateFromNative(state, (TZrNativeString)fieldName);
    if (fieldString == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(state, object, &key);
}

/* 栈帧列表使用运行时数组对象，Error.stacks 可继续交给用户代码检查。 */
/* 成功后已初始化，失败返回空；调用方负责把数组接入异常对象。 */
static SZrObject *exception_new_array(SZrState *state) {
    SZrObject *array;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    array = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    if (array != ZR_NULL) {
        ZrCore_Object_Init(state, array);
    }

    return array;
}

/* 只接受 ARRAY internalType；返回真表示已请求 SetValue，不提供独立分配失败恢复边界。 */
static TZrBool exception_array_push_value(SZrState *state, SZrObject *array, const SZrTypeValue *value) {
    SZrTypeValue key;

    if (state == ZR_NULL || array == ZR_NULL || value == ZR_NULL ||
        array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsInt(state, &key, (TZrInt64)ZrCore_Object_SuperArrayLength(array));
    ZrCore_Object_SetValue(state, array, &key, value);
    return ZR_TRUE;
}

/* 类型化 catch 和原生错误名都从全局 zr 对象查原型，而不是按字符串直接比较。 */
/* 仅接受 object prototype 内部类型；空 state/global、缺键或普通对象都视为未找到。 */
static SZrObjectPrototype *exception_lookup_prototype(SZrState *state, SZrString *typeName) {
    SZrObject *zrObject;
    SZrTypeValue key;
    const SZrTypeValue *value;

    if (state == ZR_NULL || state->global == ZR_NULL || typeName == ZR_NULL ||
        state->global->zrObject.type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_NULL;
    }

    zrObject = ZR_CAST_OBJECT(state, state->global->zrObject.value.object);
    if (zrObject == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(typeName));
    key.type = ZR_VALUE_TYPE_STRING;
    value = ZrCore_Object_GetValue(state, zrObject, &key);
    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_OBJECT || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    if (ZR_CAST_OBJECT(state, value->value.object)->internalType != ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE) {
        return ZR_NULL;
    }

    return (SZrObjectPrototype *)ZR_CAST_OBJECT(state, value->value.object);
}

/* 名称必须为有效 NUL 文本；创建键失败时返回空，分配也可能抛出。 */
static SZrObjectPrototype *exception_lookup_prototype_cstring(SZrState *state, const TZrChar *typeName) {
    SZrString *typeString;

    if (state == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    typeString = ZrCore_String_CreateFromNative(state, (TZrNativeString)typeName);
    if (typeString == ZR_NULL) {
        return ZR_NULL;
    }

    return exception_lookup_prototype(state, typeString);
}

/* 比较指针身份并沿 superPrototype 前进；依赖原型链无环，空链不匹配。 */
static TZrBool exception_prototype_inherits(SZrObjectPrototype *prototype, SZrObjectPrototype *target) {
    while (prototype != ZR_NULL) {
        if (prototype == target) {
            return ZR_TRUE;
        }
        prototype = prototype->superPrototype;
    }

    return ZR_FALSE;
}

/* 已是 Error 子类的抛出值可以保留原型与用户字段，不再包一层 Error。 */
/* 值必须为非空 OBJECT 且原型继承 global.errorPrototype；普通对象不算 Error。 */
static TZrBool exception_value_is_error_object(SZrState *state, const SZrTypeValue *value) {
    SZrObject *object;

    if (state == ZR_NULL || value == ZR_NULL || value->type != ZR_VALUE_TYPE_OBJECT || value->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    object = ZR_CAST_OBJECT(state, value->value.object);
    if (object == ZR_NULL || object->prototype == ZR_NULL || state->global == ZR_NULL ||
        state->global->errorPrototype == ZR_NULL) {
        return ZR_FALSE;
    }

    return exception_prototype_inherits(object->prototype, state->global->errorPrototype);
}

/* 原生错误时栈顶未必是诊断载荷，只接受字符串或 Error，避免误报普通对象。 */
/* 字符串、null、Error 可借用；其它值被丢弃以启用稳定默认消息，null 不等于没有传入槽。 */
static const SZrTypeValue *exception_normalize_status_stack_payload(struct SZrState *state,
                                                                    const SZrTypeValue *payload) {
    if (payload == ZR_NULL) {
        return ZR_NULL;
    }

    if (payload->type == ZR_VALUE_TYPE_STRING || payload->type == ZR_VALUE_TYPE_NULL) {
        return payload;
    }

    if (exception_value_is_error_object(state, payload)) {
        return payload;
    }

    /*
     * The slot below stackTop is not always a diagnostic string after partial native / VM unwinding.
     * Plain objects stringify as "[object type=…]" and hide the real fault; drop them so status errors
     * fall back to a stable message source inside exception_create_status_error.
     */
    return ZR_NULL;
}

/* 将线程状态映射到公开 Error 原型；不存在特化原型时退回基础 Error。 */
/* MemoryError 与 ExceptionError 单独查询，其它状态选择 RuntimeError；缺特化原型回退 errorPrototype。 */
static SZrObjectPrototype *exception_status_prototype(SZrState *state, EZrThreadStatus status) {
    SZrObjectPrototype *prototype = ZR_NULL;

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return ZR_NULL;
    }

    switch (status) {
        case ZR_THREAD_STATUS_MEMORY_ERROR:
            prototype = exception_lookup_prototype_cstring(state, "MemoryError");
            break;
        case ZR_THREAD_STATUS_EXCEPTION_ERROR:
            prototype = exception_lookup_prototype_cstring(state, "ExceptionError");
            break;
        case ZR_THREAD_STATUS_RUNTIME_ERROR:
        default:
            prototype = exception_lookup_prototype_cstring(state, "RuntimeError");
            break;
    }

    return prototype != ZR_NULL ? prototype : state->global->errorPrototype;
}

/* 无指令表、无 PC 或 PC 在表前时返回零；依赖 callInfo 的 PC 与函数指令表属于同一数组。 */
static TZrMemoryOffset exception_compute_instruction_offset(const SZrCallInfo *callInfo, const SZrFunction *function) {
    if (callInfo == ZR_NULL || function == ZR_NULL || function->instructionsList == ZR_NULL ||
        callInfo->context.context.programCounter == ZR_NULL) {
        return 0;
    }

    if (callInfo->context.context.programCounter < function->instructionsList) {
        return 0;
    }

    return (TZrMemoryOffset)(callInfo->context.context.programCounter - function->instructionsList);
}

/* 不自行解释 callInfo union；metadata 缺失的帧由上层跳过。 */
static SZrFunction *exception_call_info_function(SZrState *state, SZrCallInfo *callInfo) {
    return ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, callInfo);
}

/* 先复制 tagged value，避免转换就地修改输入；复制本身不是 GC 根，转换可能分配并抛出。 */
static void exception_set_message_field_from_value(SZrState *state, SZrObject *object, const SZrTypeValue *value) {
    SZrTypeValue messageValue;
    SZrTypeValue stableValue;
    SZrString *messageString;

    if (state == ZR_NULL || object == ZR_NULL) {
        return;
    }

    if (value == ZR_NULL) {
        ZrCore_Value_ResetAsNull(&messageValue);
    } else {
        stableValue = *value;
        messageString = ZrCore_Value_ConvertToString(state, &stableValue);
        if (messageString == ZR_NULL) {
            ZrCore_Value_ResetAsNull(&messageValue);
        } else {
            ZrCore_Value_InitAsRawObject(state, &messageValue, ZR_CAST_RAW_OBJECT_AS_SUPER(messageString));
            messageValue.type = ZR_VALUE_TYPE_STRING;
        }
    }

    exception_set_object_field_cstring(state, object, "message", &messageValue);
}

/* 从抛出点遍历调用链，把函数、源行和指令偏移物化成 Error.stacks。 */
/* 没有元数据函数的帧跳过；StackFrame 原型缺失返回空，单帧创建普通失败仅停止追加并返回已有数组。 */
static SZrObject *exception_capture_stack_frames(SZrState *state, SZrCallInfo *throwCallInfo) {
    SZrObject *frames;
    SZrCallInfo *callInfo;

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->stackFramePrototype == ZR_NULL) {
        return ZR_NULL;
    }

    frames = exception_new_array(state);
    if (frames == ZR_NULL) {
        return ZR_NULL;
    }

    callInfo = throwCallInfo != ZR_NULL ? throwCallInfo : state->callInfoList;
    while (callInfo != ZR_NULL) {
        SZrFunction *function = exception_call_info_function(state, callInfo);
        TZrMemoryOffset instructionOffset;
        TZrUInt32 sourceLine;
        SZrObject *frameObject;
        SZrTypeValue frameValue;
        SZrTypeValue stringValue;
        SZrTypeValue intValue;
/* 继续previous链而不生成虚构函数名帧 */

        if (function == ZR_NULL) {
            callInfo = callInfo->previous;
            continue;
        }

        instructionOffset = exception_compute_instruction_offset(callInfo, function);
        sourceLine = ZrCore_Exception_FindSourceLine(function, instructionOffset);
        frameObject = ZrCore_Object_New(state, state->global->stackFramePrototype);
        if (frameObject == ZR_NULL) {
            break;
        }
        ZrCore_Object_Init(state, frameObject);

        if (function->functionName != ZR_NULL) {
            ZrCore_Value_InitAsRawObject(state, &stringValue, ZR_CAST_RAW_OBJECT_AS_SUPER(function->functionName));
            stringValue.type = ZR_VALUE_TYPE_STRING;
        } else {
            ZrCore_Value_ResetAsNull(&stringValue);
        }
        exception_set_object_field_cstring(state, frameObject, "functionName", &stringValue);

        if (function->sourceCodeList != ZR_NULL) {
            ZrCore_Value_InitAsRawObject(state, &stringValue, ZR_CAST_RAW_OBJECT_AS_SUPER(function->sourceCodeList));
            stringValue.type = ZR_VALUE_TYPE_STRING;
        } else {
            ZrCore_Value_ResetAsNull(&stringValue);
        }
        exception_set_object_field_cstring(state, frameObject, "sourceFile", &stringValue);

        ZrCore_Value_InitAsInt(state, &intValue, (TZrInt64)sourceLine);
        exception_set_object_field_cstring(state, frameObject, "sourceLine", &intValue);
        ZrCore_Value_InitAsInt(state, &intValue, (TZrInt64)instructionOffset);
        exception_set_object_field_cstring(state, frameObject, "instructionOffset", &intValue);

        ZrCore_Value_InitAsRawObject(state, &frameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(frameObject));
        frameValue.type = ZR_VALUE_TYPE_OBJECT;
        if (!exception_array_push_value(state, frames, &frameValue)) {
            break;
        }

        callInfo = callInfo->previous;
    }

    return frames;
}

/* 为解释器、AOT 和日志共享的 Error 对象填充 message、exception 与两种栈轨迹。 */
/* 先复制借用来源再构造字段；stack 文本来自当前链，stacks 可从指定 throwCallInfo 起步，两者起点未必相同。 */
static TZrBool exception_apply_error_fields(SZrState *state,
                                            SZrObject *errorObject,
                                            const SZrTypeValue *messageSource,
                                            const SZrTypeValue *exceptionValue,
                                            SZrCallInfo *throwCallInfo) {
    TZrChar tracebackBuffer[ZR_EXCEPTION_TRACEBACK_BUFFER_SIZE];
    TZrSize tracebackLength = 0u;
    SZrObject *frames;
    SZrTypeValue value;
    SZrTypeValue stableMessageSource;
    SZrTypeValue stableExceptionValue;
    const SZrTypeValue *messageSourceStablePtr = ZR_NULL;
    const SZrTypeValue *exceptionValueStablePtr = ZR_NULL;

    if (state == ZR_NULL || errorObject == ZR_NULL) {
        return ZR_FALSE;
    }

    if (messageSource != ZR_NULL) {
        stableMessageSource = *messageSource;
/* 副本避免字段被改写影响读取，但不提供独立GC保活 */
        messageSourceStablePtr = &stableMessageSource;
    }
    if (exceptionValue != ZR_NULL) {
        stableExceptionValue = *exceptionValue;
        exceptionValueStablePtr = &stableExceptionValue;
    }

    tracebackLength = ZrCore_Debug_Traceback(state,
                                             ZR_NULL,
                                             0u,
                                             0u,
                                             tracebackBuffer,
                                             sizeof(tracebackBuffer));
    frames = exception_capture_stack_frames(state, throwCallInfo);
    if (frames == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(frames));
    value.type = ZR_VALUE_TYPE_ARRAY;
    exception_set_object_field_cstring(state, errorObject, "stacks", &value);
    if (tracebackLength > 0u) {
        SZrString *tracebackString = ZrCore_String_Create(state, tracebackBuffer, tracebackLength);
        if (tracebackString != ZR_NULL) {
            ZrCore_Value_InitAsRawObject(state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(tracebackString));
            value.type = ZR_VALUE_TYPE_STRING;
            exception_set_object_field_cstring(state, errorObject, "stack", &value);
        }
    }
    exception_set_message_field_from_value(state, errorObject, messageSourceStablePtr);

    if (exceptionValueStablePtr != ZR_NULL) {
        exception_set_object_field_cstring(state, errorObject, "exception", exceptionValueStablePtr);
    } else {
        ZrCore_Value_ResetAsNull(&value);
        exception_set_object_field_cstring(state, errorObject, "exception", &value);
    }

    return ZR_TRUE;
}

/* 缺字段、null 或无效对象回退原抛出值；返回的字段值仍是借用。 */
static const SZrTypeValue *exception_error_message_source(SZrState *state,
                                                          SZrObject *errorObject,
                                                          const SZrTypeValue *fallback) {
    const SZrTypeValue *messageValue;

    if (state == ZR_NULL || errorObject == ZR_NULL) {
        return fallback;
    }

    messageValue = exception_get_object_field_cstring(state, errorObject, "message");
    if (messageValue == ZR_NULL || messageValue->type == ZR_VALUE_TYPE_NULL) {
        return fallback;
    }

    return messageValue;
}

/* 当前异常值保存在 state 中，供异常调度和宿主在 TryRun 返回后读取。 */
/* 只有字段构造完成后才设置 hasCurrentException；state 中保存 tagged value，不释放对象。 */
static TZrBool exception_set_current_error_object(SZrState *state, SZrObject *errorObject, EZrThreadStatus status) {
    if (state == ZR_NULL || errorObject == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(state, &state->currentException, ZR_CAST_RAW_OBJECT_AS_SUPER(errorObject));
    state->currentException.type = ZR_VALUE_TYPE_OBJECT;
    state->currentExceptionStatus = status;
    state->hasCurrentException = ZR_TRUE;
    return ZR_TRUE;
}

/* 原生状态错误没有显式 throw 值时创建统一 Error；内存错误可借用预建消息。 */
/* MemoryError 可借用全局预建消息，但 Error 与字段仍需分配；其余无消息状态使用固定默认文本。 */
static TZrBool exception_create_status_error(SZrState *state,
                                             EZrThreadStatus status,
                                             const SZrTypeValue *payload,
                                             SZrCallInfo *throwCallInfo) {
    SZrObjectPrototype *prototype;
    SZrObject *errorObject;
    SZrTypeValue memoryMessageValue;
    SZrTypeValue defaultStatusMessageValue;
    SZrString *defaultStatusMessageString;
    const SZrTypeValue *messageSource;

    if (state == ZR_NULL) {
        return ZR_FALSE;
    }

    prototype = exception_status_prototype(state, status);
    if (prototype == ZR_NULL) {
        return ZR_FALSE;
    }

    errorObject = ZrCore_Object_New(state, prototype);
    if (errorObject == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Object_Init(state, errorObject);

    messageSource = payload;
    if (messageSource == ZR_NULL && status == ZR_THREAD_STATUS_MEMORY_ERROR && state->global != ZR_NULL &&
        state->global->memoryErrorMessage != ZR_NULL) {
        ZrCore_Value_InitAsRawObject(state, &memoryMessageValue,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(state->global->memoryErrorMessage));
        memoryMessageValue.type = ZR_VALUE_TYPE_STRING;
        messageSource = &memoryMessageValue;
    }
    if (messageSource == ZR_NULL) {
        defaultStatusMessageString =
                ZrCore_String_CreateFromNative(state, (TZrNativeString)kZrExceptionDefaultRuntimeStatusFaultMessage);
        if (defaultStatusMessageString == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrCore_Value_InitAsRawObject(state,
                                     &defaultStatusMessageValue,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(defaultStatusMessageString));
        defaultStatusMessageValue.type = ZR_VALUE_TYPE_STRING;
        messageSource = &defaultStatusMessageValue;
    }
    if (!exception_apply_error_fields(state, errorObject, messageSource, payload, throwCallInfo)) {
        return ZR_FALSE;
    }

    return exception_set_current_error_object(state, errorObject, status);
}

/* 名称解析失败返回假；保存 RUNTIME_ERROR 而不自行 Throw，调用方继续 handler 分派或失败回退。 */
TZrBool ZrCore_Exception_RaiseNamedRuntimeError(
        SZrState *state,
        const TZrChar *prototypeName,
        const TZrChar *message,
        SZrCallInfo *throwCallInfo) {
    SZrObjectPrototype *prototype;
    SZrObject *errorObject;
    SZrString *messageString;
    SZrTypeValue messageValue;

    if (state == ZR_NULL || prototypeName == ZR_NULL || message == ZR_NULL) {
        return ZR_FALSE;
    }
    prototype = exception_lookup_prototype_cstring(state, prototypeName);
    if (prototype == ZR_NULL) {
        return ZR_FALSE;
    }
    errorObject = ZrCore_Object_New(state, prototype);
    if (errorObject == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Object_Init(state, errorObject);

    messageString = ZrCore_String_CreateFromNative(
            state, (TZrNativeString)message);
    if (messageString == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsRawObject(
            state, &messageValue, ZR_CAST_RAW_OBJECT_AS_SUPER(messageString));
    messageValue.type = ZR_VALUE_TYPE_STRING;
    if (!exception_apply_error_fields(
                state, errorObject, &messageValue, ZR_NULL, throwCallInfo)) {
        return ZR_FALSE;
    }
    return exception_set_current_error_object(
            state, errorObject, ZR_THREAD_STATUS_RUNTIME_ERROR);
}

static void exception_throw_on_state(SZrState *state, EZrThreadStatus errorCode);

/* state 必须非空；本地恢复先还原入口 GC scopes，转发分支采用整域 unwind；没有恢复点最终 abort。 */
static void exception_throw_impl(
        SZrState *state, EZrThreadStatus errorCode, TZrBool restoreLocalRootSnapshot) {
    if (state->exceptionRecoverPoint != ZR_NULL) {
        /*
         * Some native paths (for example legacy IO helpers) longjmp with a thread status without
         * first materializing currentException. Test harness and VM diagnostics assume
         * hasCurrentException matches recoverable error codes before unwinding.
         * Debug_RunError already normalizes; this covers raw Throw sites and keeps threadStatus
         * consistent with exceptionLongJump.status.
         */
        /* BUG: 已初始化 Error 原型且无当前异常时，持续 OOM 经 GcMalloc -> Throw(MEMORY_ERROR) -> NormalizeStatus -> Error/键分配重入 Throw；恢复点尚未跳转，递归可耗尽宿主栈。 */
        if (!state->hasCurrentException &&
            (errorCode == ZR_THREAD_STATUS_RUNTIME_ERROR || errorCode == ZR_THREAD_STATUS_EXCEPTION_ERROR ||
             errorCode == ZR_THREAD_STATUS_MEMORY_ERROR)) {
            (void)ZrCore_Exception_NormalizeStatus(state, errorCode);
        }
/* 入口域注册与外层scope必须存活；转发分支不读取主state私有快照 */
        state->threadStatus = errorCode;
        state->exceptionRecoverPoint->status = errorCode;
        if (restoreLocalRootSnapshot) {
            ZrCore_Exception_RestoreLocalTryRunScopes(state);
        } else {
            ZrCore_GcDomain_MutatorUnwindScopes(state);
        }
        ZR_EXCEPTION_NATIVE_THROW(state, state->exceptionRecoverPoint);
    }

    if (state == ZR_NULL || state->global == ZR_NULL) {
        ZR_ABORT();
    }

    /* TODO: ResetThread 清异常三字段后本分支才复制到 main；尚未找到合法的同原生线程 secondary/no-local-recovery 调用链。下一步核查宿主 state API、worker 入口及 root/close 回调的恢复点与线程归属，再判断此顺序是否造成可达的原 Error 丢失。 */
    if (state != state->global->mainThreadState) {
        errorCode = ZrCore_State_ResetThread(state, errorCode);
        state->threadStatus = errorCode;
        if (state->global->mainThreadState != ZR_NULL && state->global->mainThreadState->exceptionRecoverPoint != ZR_NULL) {
            state->global->mainThreadState->currentException = state->currentException;
            state->global->mainThreadState->currentExceptionStatus = state->currentExceptionStatus;
            state->global->mainThreadState->hasCurrentException = state->hasCurrentException;
            exception_throw_on_state(state->global->mainThreadState, errorCode);
        }
    }

/* panic回调借用state；返回不代表错误已恢复 */
    if (state->global->panicHandlingFunction != ZR_NULL) {
        ZR_THREAD_UNLOCK(state);
        state->global->panicHandlingFunction(state);
    }
    ZR_ABORT();
}

/* 这是旧转发路径，不读取主线程私有 TryRun 快照；真正跨原生线程 longjmp 不在本地恢复契约内。 */
static void exception_throw_on_state(SZrState *state, EZrThreadStatus errorCode) {
    /* This legacy forwarding path jumps to another thread's recovery point.
     * Its private TryRun root snapshot belongs to that thread, so do not access
     * or mutate it from the forwarding state. Cross-thread longjmp semantics
     * remain outside this root-lifecycle guarantee. */
    exception_throw_impl(state, errorCode, ZR_FALSE);
}

/* 抛出首先回当前线程的恢复点；没有恢复点的 worker 尝试向主线程转发。 */
/* 非空 state；不保证普通 C 局部资源清理，命中恢复点时即使 FINE 也非局部离开。 */
void ZrCore_Exception_Throw(SZrState *state, EZrThreadStatus errorCode) {
    exception_throw_impl(state, errorCode, ZR_TRUE);
}

/* TODO: 目前只回传 status，不按 level 停止嵌套层；需沿 State_ResetThread 的 level=1 调用核对停止层级预期。 */
/* state 与 level 当前均不参与处理；TODO: 沿 State_ResetThread 的 level=1 调用确认是否仍需要分层停止语义。 */
EZrThreadStatus ZrCore_Exception_TryStop(SZrState *state, TZrMemoryOffset level, EZrThreadStatus status) {
    ZR_TODO_PARAMETER(state);
    ZR_TODO_PARAMETER(level);
    return status;
}

/* 把线程当前异常或最后的栈顶载荷移至调用结果槽，随后恢复 VM 栈顶。 */
/* previousTop 须属于有效可写栈且其前一槽可作 top；写完 stackTop=previousTop-1，未保证 top 指向结果之后。 */
void ZrCore_Exception_MarkError(SZrState *state, EZrThreadStatus errorCode, TZrStackValuePointer previousTop) {
    if (state == ZR_NULL || previousTop == ZR_NULL) {
        return;
    }

    switch (errorCode) {
        case ZR_THREAD_STATUS_FINE:
            ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(previousTop));
            break;
        case ZR_THREAD_STATUS_MEMORY_ERROR:
            if (state->hasCurrentException) {
                ZrCore_Stack_CopyValue(state, previousTop, &state->currentException);
            } else {
                ZrCore_Stack_SetRawObjectValue(state, previousTop,
                                         ZR_CAST_RAW_OBJECT_AS_SUPER(state->global->memoryErrorMessage));
            }
            break;
        default:
            if (state->hasCurrentException) {
                ZrCore_Stack_CopyValue(state, previousTop, &state->currentException);
            } else if (state->stackTop.valuePointer > state->stackBase.valuePointer) {
                ZrCore_Stack_CopyValue(state, previousTop, &(state->stackTop.valuePointer - 1)->value);
            } else {
                ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(previousTop));
            }
            break;
    }

    state->stackTop.valuePointer = previousTop - 1;
}

/* 同时清 value/status/has 标志；不改 threadStatus，不执行对象销毁或 handler 清理。 */
void ZrCore_Exception_ClearCurrent(struct SZrState *state) {
    if (state == ZR_NULL) {
        return;
    }

    ZrCore_Value_ResetAsNull(&state->currentException);
    state->currentExceptionStatus = ZR_THREAD_STATUS_FINE;
    state->hasCurrentException = ZR_FALSE;
}

/* 解释器/AOT 抛出任意值时，在当前线程保存可供 catch 检查的 Error 对象。 */
/* 已有 Error 保留对象身份但重建诊断字段；其它值包装基础 Error；假返回不保证已改字段回滚。 */
TZrBool ZrCore_Exception_NormalizeThrownValue(struct SZrState *state,
                                              const SZrTypeValue *payload,
                                              struct SZrCallInfo *throwCallInfo,
                                              EZrThreadStatus status) {
    SZrObject *errorObject;
    SZrObjectPrototype *prototype;
    SZrTypeValue payloadCopy;
    SZrTypeValue selfValue;

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->errorPrototype == ZR_NULL || payload == ZR_NULL) {
        return ZR_FALSE;
    }

/* selfValue是旧对象载荷；字段修改失败不回滚旧诊断 */
    payloadCopy = *payload;
    if (exception_value_is_error_object(state, &payloadCopy)) {
        errorObject = ZR_CAST_OBJECT(state, payloadCopy.value.object);
        selfValue = payloadCopy;
        if (!exception_apply_error_fields(state,
                                          errorObject,
                                          exception_error_message_source(state, errorObject, &payloadCopy),
                                          &selfValue,
                                          throwCallInfo)) {
            return ZR_FALSE;
        }
        return exception_set_current_error_object(state, errorObject, status);
    }

    prototype = state->global->errorPrototype;
    errorObject = ZrCore_Object_New(state, prototype);
    if (errorObject == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Object_Init(state, errorObject);

    if (!exception_apply_error_fields(state, errorObject, &payloadCopy, &payloadCopy, throwCallInfo)) {
        return ZR_FALSE;
    }

    return exception_set_current_error_object(state, errorObject, status);
}

/* 原生失败状态在宿主边界也需对应 Error 对象，已有异常则沿用该对象。 */
/* EXECUTION_TERMINATED 不归一化；已有异常只改 currentExceptionStatus；其它状态可查看末尾栈槽并分配。 */
TZrBool ZrCore_Exception_NormalizeStatus(struct SZrState *state, EZrThreadStatus status) {
    const SZrTypeValue *payload = ZR_NULL;
    SZrCallInfo *throwCallInfo;

    if (state == ZR_NULL || status == ZR_THREAD_STATUS_EXECUTION_TERMINATED) {
        return ZR_FALSE;
    }

/* 不重建栈或消息，不改变threadStatus */
    if (state->hasCurrentException) {
        state->currentExceptionStatus = status;
        return ZR_TRUE;
    }

    throwCallInfo = state->callInfoList;
    if (state->stackTop.valuePointer != ZR_NULL && state->stackBase.valuePointer != ZR_NULL &&
        state->stackTop.valuePointer > state->stackBase.valuePointer) {
        payload = exception_normalize_status_stack_payload(state,
                                                           &(state->stackTop.valuePointer - 1)->value);
    }

    return exception_create_status_error(state, status, payload, throwCallInfo);
}

/* 类型化 catch 使用原型继承关系；未知类型当前退回基础 Error。 */
/* 只匹配 Error 继承；TODO: 未找到非空类型名时仍回退基础 Error，沿 parser catchClause.typeName 生成与校验核查未知类型准入。 */
TZrBool ZrCore_Exception_CatchMatchesTypeName(struct SZrState *state,
                                              const SZrTypeValue *errorValue,
                                              struct SZrString *typeName) {
    SZrObjectPrototype *expectedPrototype;
    SZrObject *errorObject;

    if (state == ZR_NULL || errorValue == ZR_NULL || !exception_value_is_error_object(state, errorValue)) {
        return ZR_FALSE;
    }

    errorObject = ZR_CAST_OBJECT(state, errorValue->value.object);
    if (errorObject == ZR_NULL || errorObject->prototype == ZR_NULL) {
        return ZR_FALSE;
    }

/* TODO: 沿parser catch类型生成/诊断核对未解析名称是否可进入此分支 */
    expectedPrototype = typeName != ZR_NULL ? exception_lookup_prototype(state, typeName) : state->global->errorPrototype;
    /* TODO: 有注解但原型未找到时会匹配所有 Error；需验证编译阶段是否必然拒绝未知类型。 */
    if (expectedPrototype == ZR_NULL) {
        expectedPrototype = state->global->errorPrototype;
    }

    return exception_prototype_inherits(errorObject->prototype, expectedPrototype);
}

/* 调试位置表按指令顺序组织，异常与 AOT 诊断共享此最近前驱查找。 */
/* 依赖 executionLocationInfoList 按 instructionOffset 递增；无映射返回零，不使用 debug hook 的无行哨兵。 */
TZrUInt32 ZrCore_Exception_FindSourceLine(struct SZrFunction *function, TZrMemoryOffset instructionOffset) {
    TZrUInt32 bestLine = ZR_EXCEPTION_SOURCE_LINE_NONE;

    if (function == ZR_NULL || function->executionLocationInfoList == ZR_NULL || function->executionLocationInfoLength == 0) {
        return ZR_EXCEPTION_SOURCE_LINE_NONE;
    }

    for (TZrUInt32 index = 0; index < function->executionLocationInfoLength; index++) {
        SZrFunctionExecutionLocationInfo *info = &function->executionLocationInfoList[index];
        if (info->currentInstructionOffset > instructionOffset) {
            break;
        }
        bestLine = info->lineInSource;
    }

    return bestLine;
}

/* 未捕获异常的可增长宿主文本缓冲区，最终由 PrintUnhandled/LogUnhandled 释放。 */
/* 不由 VM allocator 管理，格式化正常返回转交 Print/Log free */
typedef struct SZrExceptionTextBuilder {
/* 空初始化；成功后交输出函数free，reserve失败仍保留原块 */
    TZrChar *buffer;
/* 不含末尾零；append仅在reserve成功后增加 */
    TZrSize length;
/* 必须容纳length及终止零；reserve只增长，不表示有效文本长度 */
    TZrSize capacity;
} SZrExceptionTextBuilder;

/* 按需要扩充诊断文本；失败时保留旧缓冲区供调用方释放。 */
/* 容量含终止零；原块由最终格式化消费者释放；TODO: 沿 append/appendf 的长度加法及倍增核查超大诊断的 SIZE_MAX 边界。 */
static TZrBool exception_text_builder_reserve(SZrExceptionTextBuilder *builder, TZrSize requiredLength) {
    TZrChar *newBuffer;
    TZrSize newCapacity;

    if (builder == ZR_NULL) {
        return ZR_FALSE;
    }

    if (requiredLength <= builder->capacity) {
        return ZR_TRUE;
    }

    newCapacity = builder->capacity > 0 ? builder->capacity : 256;
    while (newCapacity < requiredLength) {
        newCapacity *= 2;
    }

    newBuffer = (TZrChar *)realloc(builder->buffer, newCapacity);
    if (newBuffer == ZR_NULL) {
        return ZR_FALSE;
    }

    builder->buffer = newBuffer;
    builder->capacity = newCapacity;
    return ZR_TRUE;
}

/* text 必须有效且不含待输出的内嵌零；reserve 失败不增加 length。 */
static TZrBool exception_text_builder_append(SZrExceptionTextBuilder *builder, const TZrChar *text) {
    TZrSize textLength;

    if (builder == ZR_NULL || text == ZR_NULL) {
        return ZR_FALSE;
    }

    textLength = strlen(text);
    if (!exception_text_builder_reserve(builder, builder->length + textLength + 1)) {
        return ZR_FALSE;
    }

    memcpy(builder->buffer + builder->length, text, textLength);
    builder->length += textLength;
    builder->buffer[builder->length] = '\0';
    return ZR_TRUE;
}

/* va_copy 用于测长度；失败分支结束两份参数列表，成功长度不包含末尾零。 */
static TZrBool exception_text_builder_appendf(SZrExceptionTextBuilder *builder, const TZrChar *format, ...) {
    va_list args;
    va_list copyArgs;
    int requiredLength;

    if (builder == ZR_NULL || format == ZR_NULL) {
        return ZR_FALSE;
    }

    va_start(args, format);
    va_copy(copyArgs, args);
    requiredLength = vsnprintf(ZR_NULL, 0, format, copyArgs);
    va_end(copyArgs);
    if (requiredLength < 0 ||
        !exception_text_builder_reserve(builder, builder->length + (TZrSize)requiredLength + 1)) {
        va_end(args);
        return ZR_FALSE;
    }

    vsnprintf(builder->buffer + builder->length,
              builder->capacity - builder->length,
              format,
              args);
    va_end(args);
    builder->length += (TZrSize)requiredLength;
    return ZR_TRUE;
}

/* 把 Error 对象或普通抛出值转换成宿主可输出文本；返回 malloc 缓冲区。 */
/* 优先已保存 stack 文本，再退回 stacks；成功返回 malloc/realloc 缓冲区，普通失败释放已建文本，VM Throw 可绕过该清理。 */
static TZrChar *exception_format_unhandled_text(struct SZrState *state, const SZrTypeValue *errorValue) {
    SZrObject *errorObject;
    const SZrTypeValue *messageValue;
    const SZrTypeValue *payloadValue;
    const SZrTypeValue *stackValue;
    const SZrTypeValue *stacksValue;
    const TZrChar *typeName = "Error";
    SZrString *payloadSummary = ZR_NULL;
    SZrExceptionTextBuilder builder;

    memset(&builder, 0, sizeof(builder));
    if (state == ZR_NULL || errorValue == ZR_NULL) {
        return ZR_NULL;
    }

    if (!exception_value_is_error_object(state, errorValue)) {
        payloadSummary = ZrCore_Value_ConvertToString(state, (SZrTypeValue *)errorValue);
        if (!exception_text_builder_appendf(&builder,
                                            "Unhandled exception: %s\n",
                                            payloadSummary != ZR_NULL ? ZrCore_String_GetNativeString(payloadSummary) : "<unknown>")) {
            free(builder.buffer);
            return ZR_NULL;
        }
        return builder.buffer;
    }

    errorObject = ZR_CAST_OBJECT(state, errorValue->value.object);
    if (errorObject != ZR_NULL && errorObject->prototype != ZR_NULL && errorObject->prototype->name != ZR_NULL) {
        typeName = ZrCore_String_GetNativeString(errorObject->prototype->name);
    }

    messageValue = exception_get_object_field_cstring(state, errorObject, "message");
    payloadValue = exception_get_object_field_cstring(state, errorObject, "exception");
    stackValue = exception_get_object_field_cstring(state, errorObject, "stack");
    stacksValue = exception_get_object_field_cstring(state, errorObject, "stacks");
    if (payloadValue != ZR_NULL) {
        payloadSummary = ZrCore_Value_ConvertToString(state, (SZrTypeValue *)payloadValue);
    }

    if (!exception_text_builder_append(&builder, typeName != ZR_NULL ? typeName : "Error")) {
        free(builder.buffer);
        return ZR_NULL;
    }
    if (messageValue != ZR_NULL && messageValue->type == ZR_VALUE_TYPE_STRING && messageValue->value.object != ZR_NULL) {
        if (!exception_text_builder_appendf(&builder,
                                            ": %s",
                                            ZrCore_String_GetNativeString(ZR_CAST_STRING(state, messageValue->value.object)))) {
            free(builder.buffer);
            return ZR_NULL;
        }
    }
    if (!exception_text_builder_append(&builder, "\n")) {
        free(builder.buffer);
        return ZR_NULL;
    }
    if (payloadSummary != ZR_NULL) {
        if (!exception_text_builder_appendf(&builder,
                                            "payload: %s\n",
                                            ZrCore_String_GetNativeString(payloadSummary))) {
            free(builder.buffer);
            return ZR_NULL;
        }
    }

/* 仅接受非空字符串；必要时补换行然后直接返回 */
    if (stackValue != ZR_NULL && stackValue->type == ZR_VALUE_TYPE_STRING && stackValue->value.object != ZR_NULL) {
        const TZrChar *stackText = ZrCore_String_GetNativeString(ZR_CAST_STRING(state, stackValue->value.object));
        TZrSize stackLength = stackText != ZR_NULL ? strlen(stackText) : 0u;
        if (stackLength > 0u) {
            if (!exception_text_builder_append(&builder, stackText)) {
                free(builder.buffer);
                return ZR_NULL;
            }
            if (stackText[stackLength - 1u] != '\n' && !exception_text_builder_append(&builder, "\n")) {
                free(builder.buffer);
                return ZR_NULL;
            }
            return builder.buffer;
        }
    }

/* 物化失败释放宿主builder；可能的Throw不走这条普通失败清理 */
    if (stacksValue != ZR_NULL && stacksValue->type == ZR_VALUE_TYPE_ARRAY && stacksValue->value.object != ZR_NULL) {
        SZrObject *frames = ZR_CAST_OBJECT(state, stacksValue->value.object);
        if (!ZrCore_Object_SuperArrayMaterializeGeneric(state, frames)) {
            free(builder.buffer);
            return ZR_NULL;
        }
        TZrSize frameCount = frames->nodeMap.elementCount;
        for (TZrSize index = 0; index < frameCount; index++) {
            SZrTypeValue key;
            const SZrTypeValue *frameValue;
            const SZrTypeValue *functionNameValue;
            const SZrTypeValue *sourceFileValue;
            const SZrTypeValue *sourceLineValue;
            const SZrTypeValue *instructionOffsetValue;
            const TZrChar *functionName = "<anonymous>";
            const TZrChar *sourceFile = "<unknown>";
            TZrInt64 sourceLine = (TZrInt64)ZR_EXCEPTION_SOURCE_LINE_NONE;
            TZrInt64 instructionOffset = 0;

            ZrCore_Value_InitAsInt(state, &key, (TZrInt64)index);
            frameValue = ZrCore_Object_GetValue(state, frames, &key);
            if (frameValue == ZR_NULL || frameValue->type != ZR_VALUE_TYPE_OBJECT || frameValue->value.object == ZR_NULL) {
                continue;
            }

            functionNameValue = exception_get_object_field_cstring(state, ZR_CAST_OBJECT(state, frameValue->value.object), "functionName");
            sourceFileValue = exception_get_object_field_cstring(state, ZR_CAST_OBJECT(state, frameValue->value.object), "sourceFile");
            sourceLineValue = exception_get_object_field_cstring(state, ZR_CAST_OBJECT(state, frameValue->value.object), "sourceLine");
            instructionOffsetValue =
                    exception_get_object_field_cstring(state, ZR_CAST_OBJECT(state, frameValue->value.object), "instructionOffset");

            if (functionNameValue != ZR_NULL && functionNameValue->type == ZR_VALUE_TYPE_STRING && functionNameValue->value.object != ZR_NULL) {
                functionName = ZrCore_String_GetNativeString(ZR_CAST_STRING(state, functionNameValue->value.object));
            }
            if (sourceFileValue != ZR_NULL && sourceFileValue->type == ZR_VALUE_TYPE_STRING && sourceFileValue->value.object != ZR_NULL) {
                sourceFile = ZrCore_String_GetNativeString(ZR_CAST_STRING(state, sourceFileValue->value.object));
            }
            if (sourceLineValue != ZR_NULL && ZR_VALUE_IS_TYPE_INT(sourceLineValue->type)) {
                sourceLine = sourceLineValue->value.nativeObject.nativeInt64;
            }
            if (instructionOffsetValue != ZR_NULL && ZR_VALUE_IS_TYPE_INT(instructionOffsetValue->type)) {
                instructionOffset = instructionOffsetValue->value.nativeObject.nativeInt64;
            }

            if (!exception_text_builder_appendf(&builder,
                                                "  at %s (%s:%lld, ip=%lld)\n",
                                                functionName != ZR_NULL ? functionName : "<anonymous>",
                                                sourceFile != ZR_NULL ? sourceFile : "<unknown>",
                                                (long long)sourceLine,
                                                (long long)instructionOffset)) {
                free(builder.buffer);
                return ZR_NULL;
            }
        }
    }

    return builder.buffer;
}

/* 空 FILE 选择 stderr；格式化普通失败不输出；正常输出后 free 文本，不关闭借用 stream。 */
void ZrCore_Exception_PrintUnhandled(struct SZrState *state, const SZrTypeValue *errorValue, FILE *stream) {
    FILE *output = stream != ZR_NULL ? stream : stderr;
    TZrChar *text = exception_format_unhandled_text(state, errorValue);

    if (output == ZR_NULL || text == ZR_NULL) {
        return;
    }

    fputs(text, output);
    fflush(output);
    free(text);
}

/* 按 EXCEPTION/STDERR/DIAGNOSTIC 分类发送；调用后释放宿主文本，路由须同步消费或复制文本。 */
void ZrCore_Exception_LogUnhandled(struct SZrState *state, const SZrTypeValue *errorValue) {
    TZrChar *text = exception_format_unhandled_text(state, errorValue);

    if (text == ZR_NULL) {
        return;
    }

    ZrCore_Log_Write(state,
                     ZR_LOG_LEVEL_EXCEPTION,
                     ZR_OUTPUT_CHANNEL_STDERR,
                     ZR_OUTPUT_KIND_DIAGNOSTIC,
                     text);
    free(text);
}
