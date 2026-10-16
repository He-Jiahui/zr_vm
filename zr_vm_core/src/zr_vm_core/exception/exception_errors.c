#include "exception_error_internal.h"
#include "exception_roots_internal.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"

/* 非托管诊断暂存区有固定上限，格式化失败时上层仍可返回状态。 */
/* 缓冲含终止零；实际 length 由 Debug 返回 */
#define ZR_EXCEPTION_TRACEBACK_BUFFER_SIZE 4096u

/* 进程内只读 NUL 文本；不属于 VM 托管字符串 */
static const TZrChar kZrExceptionDefaultRuntimeStatusFaultMessage[] = "Runtime status fault";

/* 错误对象字段由宿主字符串键写入，供解释器和 AOT 的统一 catch/诊断路径读取。 */
/* Root both borrowed inputs before creating the allocating field-name key. */
static void exception_set_object_field_cstring(SZrState *state,
                                               SZrObject *object,
                                               const TZrChar *fieldName,
                                               const SZrTypeValue *value) {
    SZrString *fieldString;
    SZrTypeValue key;
    SZrExceptionRoots roots;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL) {
        return;
    }

    if (!exception_roots_begin(state, &roots)) {
        return;
    }
    roots.objects[0] = ZR_CAST_RAW_OBJECT_AS_SUPER(object);
    exception_roots_set_value(&roots, 1u, value);
    fieldString = ZrCore_String_CreateFromNative(state, (TZrNativeString)fieldName);
    if (fieldString == ZR_NULL) {
        exception_roots_end(state, &roots);
        return;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Object_SetValue(state, ZR_CAST_OBJECT(state, roots.objects[0]),
                           &key, exception_roots_value(&roots, 1u));
    exception_roots_end(state, &roots);
}

/* 返回对象字段的借用地址；字段名转换可能分配，返回值不可当成独立拥有的副本。 */
const SZrTypeValue *exception_get_object_field_cstring(SZrState *state,
                                                              SZrObject *object,
                                                              const TZrChar *fieldName) {
    SZrString *fieldString;
    SZrTypeValue key;
    const SZrTypeValue *result;
    SZrExceptionRoots roots;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    if (!exception_roots_begin(state, &roots)) {
        return ZR_NULL;
    }
    roots.objects[0] = ZR_CAST_RAW_OBJECT_AS_SUPER(object);
    fieldString = ZrCore_String_CreateFromNative(state, (TZrNativeString)fieldName);
    if (fieldString == ZR_NULL) {
        exception_roots_end(state, &roots);
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;
    result = ZrCore_Object_GetValue(state, ZR_CAST_OBJECT(state, roots.objects[0]), &key);
    exception_roots_end(state, &roots);
    return result;
}

/* 栈帧列表使用运行时数组对象，Error.stacks 可继续交给用户代码检查。 */
/* 成功后已初始化，失败返回空；调用方负责把数组接入异常对象。 */
static SZrObject *exception_new_array(SZrState *state) {
    SZrObject *array;
    SZrExceptionRoots roots;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }
    if (!exception_roots_begin(state, &roots)) {
        return ZR_NULL;
    }

    array = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    if (array != ZR_NULL) {
        roots.objects[0] = ZR_CAST_RAW_OBJECT_AS_SUPER(array);
        ZrCore_Object_Init(state, array);
        array = ZR_CAST_OBJECT(state, roots.objects[0]);
    }

    exception_roots_end(state, &roots);
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
TZrBool exception_value_is_error_object(SZrState *state, const SZrTypeValue *value) {
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

/* Keep the recipient and input alive while conversion and key creation allocate. */
static void exception_set_message_field_from_value(SZrState *state, SZrObject *object, const SZrTypeValue *value) {
    SZrTypeValue messageValue;
    SZrTypeValue stableValue;
    SZrString *messageString;
    SZrExceptionRoots roots;

    if (state == ZR_NULL || object == ZR_NULL) {
        return;
    }
    if (!exception_roots_begin(state, &roots)) {
        return;
    }
    roots.objects[0] = ZR_CAST_RAW_OBJECT_AS_SUPER(object);
    exception_roots_set_value(&roots, 1u, value);

    if (value == ZR_NULL) {
        ZrCore_Value_ResetAsNull(&messageValue);
    } else {
        stableValue = *exception_roots_value(&roots, 1u);
        messageString = ZrCore_Value_ConvertToString(state, &stableValue);
        if (messageString == ZR_NULL) {
            ZrCore_Value_ResetAsNull(&messageValue);
        } else {
            ZrCore_Value_InitAsRawObject(state, &messageValue, ZR_CAST_RAW_OBJECT_AS_SUPER(messageString));
            messageValue.type = ZR_VALUE_TYPE_STRING;
        }
    }

    exception_set_object_field_cstring(state, ZR_CAST_OBJECT(state, roots.objects[0]), "message", &messageValue);
    exception_roots_end(state, &roots);
}

/* 从抛出点遍历调用链，把函数、源行和指令偏移物化成 Error.stacks。 */
/* 没有元数据函数的帧跳过；StackFrame 原型缺失返回空，单帧创建普通失败仅停止追加并返回已有数组。 */
static SZrObject *exception_capture_stack_frames(SZrState *state, SZrCallInfo *throwCallInfo) {
    SZrObject *frames;
    SZrCallInfo *callInfo;
    SZrExceptionRoots roots;

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->stackFramePrototype == ZR_NULL) {
        return ZR_NULL;
    }
    if (!exception_roots_begin(state, &roots)) {
        return ZR_NULL;
    }

    frames = exception_new_array(state);
    if (frames == ZR_NULL) {
        exception_roots_end(state, &roots);
        return ZR_NULL;
    }
    roots.objects[0] = ZR_CAST_RAW_OBJECT_AS_SUPER(frames);

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
        roots.objects[1] = ZR_CAST_RAW_OBJECT_AS_SUPER(function);

        instructionOffset = exception_compute_instruction_offset(callInfo, function);
        sourceLine = ZrCore_Exception_FindSourceLine(function, instructionOffset);
        frameObject = ZrCore_Object_New(state, state->global->stackFramePrototype);
        if (frameObject == ZR_NULL) {
            break;
        }
        roots.objects[2] = ZR_CAST_RAW_OBJECT_AS_SUPER(frameObject);
        ZrCore_Object_Init(state, frameObject);

        function = (SZrFunction *)roots.objects[1];
        if (function->functionName != ZR_NULL) {
            ZrCore_Value_InitAsRawObject(state, &stringValue, ZR_CAST_RAW_OBJECT_AS_SUPER(function->functionName));
            stringValue.type = ZR_VALUE_TYPE_STRING;
        } else {
            ZrCore_Value_ResetAsNull(&stringValue);
        }
        exception_set_object_field_cstring(state, ZR_CAST_OBJECT(state, roots.objects[2]), "functionName", &stringValue);

        function = (SZrFunction *)roots.objects[1];
        if (function->sourceCodeList != ZR_NULL) {
            ZrCore_Value_InitAsRawObject(state, &stringValue, ZR_CAST_RAW_OBJECT_AS_SUPER(function->sourceCodeList));
            stringValue.type = ZR_VALUE_TYPE_STRING;
        } else {
            ZrCore_Value_ResetAsNull(&stringValue);
        }
        exception_set_object_field_cstring(state, ZR_CAST_OBJECT(state, roots.objects[2]), "sourceFile", &stringValue);

        ZrCore_Value_InitAsInt(state, &intValue, (TZrInt64)sourceLine);
        exception_set_object_field_cstring(state, ZR_CAST_OBJECT(state, roots.objects[2]), "sourceLine", &intValue);
        ZrCore_Value_InitAsInt(state, &intValue, (TZrInt64)instructionOffset);
        exception_set_object_field_cstring(state, ZR_CAST_OBJECT(state, roots.objects[2]), "instructionOffset", &intValue);

        ZrCore_Value_InitAsRawObject(state, &frameValue, roots.objects[2]);
        frameValue.type = ZR_VALUE_TYPE_OBJECT;
        if (!exception_array_push_value(state, ZR_CAST_OBJECT(state, roots.objects[0]), &frameValue)) {
            break;
        }
        roots.objects[1] = ZR_NULL;
        roots.objects[2] = ZR_NULL;

        callInfo = callInfo->previous;
    }

    frames = ZR_CAST_OBJECT(state, roots.objects[0]);
    exception_roots_end(state, &roots);
    return frames;
}

/* 为解释器、AOT 和日志共享的 Error 对象填充 message、exception 与两种栈轨迹。 */
/* Four temporary roots cover the recipient, both inputs, and the unpublished
 * frame array. stack text and stacks retain their existing capture origins. */
static TZrBool exception_apply_error_fields(SZrState *state,
                                            SZrObject *errorObject,
                                            const SZrTypeValue *messageSource,
                                            const SZrTypeValue *exceptionValue,
                                            SZrCallInfo *throwCallInfo) {
    TZrChar tracebackBuffer[ZR_EXCEPTION_TRACEBACK_BUFFER_SIZE];
    TZrSize tracebackLength = 0u;
    SZrObject *frames;
    SZrTypeValue value;
    SZrExceptionRoots roots;

    if (state == ZR_NULL || errorObject == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!exception_roots_begin(state, &roots)) {
        return ZR_FALSE;
    }
    roots.objects[0] = ZR_CAST_RAW_OBJECT_AS_SUPER(errorObject);
    exception_roots_set_value(&roots, 1u, messageSource);
    exception_roots_set_value(&roots, 2u, exceptionValue);

    tracebackLength = ZrCore_Debug_Traceback(state,
                                             ZR_NULL,
                                             0u,
                                             0u,
                                             tracebackBuffer,
                                             sizeof(tracebackBuffer));
    frames = exception_capture_stack_frames(state, throwCallInfo);
    if (frames == ZR_NULL) {
        exception_roots_end(state, &roots);
        return ZR_FALSE;
    }
    roots.objects[3] = ZR_CAST_RAW_OBJECT_AS_SUPER(frames);

    ZrCore_Value_InitAsRawObject(state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(frames));
    value.type = ZR_VALUE_TYPE_ARRAY;
    exception_set_object_field_cstring(state, ZR_CAST_OBJECT(state, roots.objects[0]), "stacks", &value);
    roots.objects[3] = ZR_NULL; /* The completed edge now owns the array. */
    if (tracebackLength > 0u) {
        SZrString *tracebackString = ZrCore_String_Create(state, tracebackBuffer, tracebackLength);
        if (tracebackString != ZR_NULL) {
            ZrCore_Value_InitAsRawObject(state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(tracebackString));
            value.type = ZR_VALUE_TYPE_STRING;
            exception_set_object_field_cstring(state, ZR_CAST_OBJECT(state, roots.objects[0]), "stack", &value);
        }
    }
    exception_set_message_field_from_value(state, ZR_CAST_OBJECT(state, roots.objects[0]),
                                           exception_roots_value(&roots, 1u));
    exception_set_object_field_cstring(state, ZR_CAST_OBJECT(state, roots.objects[0]), "exception",
                                      exception_roots_value(&roots, 2u));

    exception_roots_end(state, &roots);
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
    SZrExceptionRoots roots;
    TZrBool result = ZR_FALSE;

    if (state == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!exception_roots_begin(state, &roots)) {
        return ZR_FALSE;
    }
    exception_roots_set_value(&roots, 0u, payload);

    prototype = exception_status_prototype(state, status);
    if (prototype == ZR_NULL) {
        goto done;
    }

    errorObject = ZrCore_Object_New(state, prototype);
    if (errorObject == ZR_NULL) {
        goto done;
    }
    roots.objects[1] = ZR_CAST_RAW_OBJECT_AS_SUPER(errorObject);
    ZrCore_Object_Init(state, errorObject);

    messageSource = payload != ZR_NULL ? exception_roots_value(&roots, 0u) : ZR_NULL;
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
            goto done;
        }
        ZrCore_Value_InitAsRawObject(state,
                                     &defaultStatusMessageValue,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(defaultStatusMessageString));
        defaultStatusMessageValue.type = ZR_VALUE_TYPE_STRING;
        messageSource = &defaultStatusMessageValue;
    }
    exception_roots_set_value(&roots, 2u, messageSource);
    if (!exception_apply_error_fields(state, ZR_CAST_OBJECT(state, roots.objects[1]),
                                      exception_roots_value(&roots, 2u),
                                      payload != ZR_NULL ? exception_roots_value(&roots, 0u) : ZR_NULL,
                                      throwCallInfo)) {
        goto done;
    }

    result = exception_set_current_error_object(state, ZR_CAST_OBJECT(state, roots.objects[1]), status);
done:
    exception_roots_end(state, &roots);
    return result;
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
    SZrExceptionRoots roots;
    TZrBool result = ZR_FALSE;

    if (state == ZR_NULL || prototypeName == ZR_NULL || message == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!exception_roots_begin(state, &roots)) {
        return ZR_FALSE;
    }
    prototype = exception_lookup_prototype_cstring(state, prototypeName);
    if (prototype == ZR_NULL) {
        goto done;
    }
    errorObject = ZrCore_Object_New(state, prototype);
    if (errorObject == ZR_NULL) {
        goto done;
    }
    roots.objects[0] = ZR_CAST_RAW_OBJECT_AS_SUPER(errorObject);
    ZrCore_Object_Init(state, errorObject);

    messageString = ZrCore_String_CreateFromNative(
            state, (TZrNativeString)message);
    if (messageString == ZR_NULL) {
        goto done;
    }
    ZrCore_Value_InitAsRawObject(
            state, &messageValue, ZR_CAST_RAW_OBJECT_AS_SUPER(messageString));
    messageValue.type = ZR_VALUE_TYPE_STRING;
    if (!exception_apply_error_fields(
                state, ZR_CAST_OBJECT(state, roots.objects[0]), &messageValue, ZR_NULL, throwCallInfo)) {
        goto done;
    }
    result = exception_set_current_error_object(
            state, ZR_CAST_OBJECT(state, roots.objects[0]), ZR_THREAD_STATUS_RUNTIME_ERROR);
done:
    exception_roots_end(state, &roots);
    return result;
}

/* 解释器/AOT 抛出任意值时，在当前线程保存可供 catch 检查的 Error 对象。 */
/* 已有 Error 保留对象身份但重建诊断字段；其它值包装基础 Error；假返回不保证已改字段回滚。 */
TZrBool ZrCore_Exception_NormalizeThrownValue(struct SZrState *state,
                                              const SZrTypeValue *payload,
                                              struct SZrCallInfo *throwCallInfo,
                                              EZrThreadStatus status) {
    SZrObject *errorObject;
    SZrObjectPrototype *prototype;
    SZrExceptionRoots roots;
    const SZrTypeValue *messageSource;
    TZrBool result = ZR_FALSE;

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->errorPrototype == ZR_NULL || payload == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!exception_roots_begin(state, &roots)) {
        return ZR_FALSE;
    }
    exception_roots_set_value(&roots, 0u, payload);

    /* Existing Errors keep identity and fields; a partial field write is not
     * rolled back. Resolve the borrowed message before reloading the object. */
    if (exception_value_is_error_object(state, exception_roots_value(&roots, 0u))) {
        roots.objects[1] = roots.objects[0];
        messageSource = exception_error_message_source(
                state, ZR_CAST_OBJECT(state, roots.objects[1]), ZR_NULL);
        exception_roots_set_value(&roots, 2u, messageSource != ZR_NULL
                ? messageSource : exception_roots_value(&roots, 0u));
    } else {
        prototype = state->global->errorPrototype;
        errorObject = ZrCore_Object_New(state, prototype);
        if (errorObject == ZR_NULL) {
            goto done;
        }
        roots.objects[1] = ZR_CAST_RAW_OBJECT_AS_SUPER(errorObject);
        ZrCore_Object_Init(state, errorObject);
        exception_roots_set_value(&roots, 2u, exception_roots_value(&roots, 0u));
    }

    if (!exception_apply_error_fields(state, ZR_CAST_OBJECT(state, roots.objects[1]),
                                      exception_roots_value(&roots, 2u),
                                      exception_roots_value(&roots, 0u), throwCallInfo)) {
        goto done;
    }

    result = exception_set_current_error_object(state, ZR_CAST_OBJECT(state, roots.objects[1]), status);
done:
    exception_roots_end(state, &roots);
    return result;
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

    /* Bootstrap errors precede the string table and base Error prototype.
     * Do not allocate lookup strings before that infrastructure is ready. */
    if (state->global == ZR_NULL || state->global->errorPrototype == ZR_NULL) {
        return ZR_FALSE;
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
