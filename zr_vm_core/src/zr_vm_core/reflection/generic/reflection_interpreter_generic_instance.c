//
// Interpreter object context for reflection-created generic reference and boxed value instances.
//

#include "zr_vm_core/reflection.h"

#include "object/object_call_internal.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

/* 这些键组成解释器泛型实例和 MethodSpec 上下文的内部字段协议，须与反射对象构造器写入的键保持一致。 */
static const TZrChar *kInterpreterGenericTypeObjectField = "__zr_genericTypeInfo";
static const TZrChar *kGenericBaseTokenField = "genericBaseToken";
static const TZrChar *kGenericMethodTokenField = "genericMethodToken";
static const TZrChar *kGenericArgumentCountField = "genericArgumentCount";
static const TZrChar *kGenericArgumentsField = "genericArguments";
static const TZrChar *kMetadataRuntimeField = "metadataRuntime";

/* addedByCaller 记录本次是否新建 ignored GC 根；配对释放只撤销本次新增，保留调用方已有的根。
 * BUG: 同一 GC domain 的两个 RUNNING mutator 可经反射入口并发进入；底层 ignored-root 表登记、查询与撤销未串行，可能丢失根或破坏登记索引。 */
static TZrBool reflection_interpreter_generic_pin(
        SZrState *state,
        SZrRawObject *object,
        TZrBool *addedByCaller) {
    return ZrCore_GarbageCollector_IgnoreObjectIfNeededFast(
            state != ZR_NULL ? state->global : ZR_NULL, state, object, addedByCaller);
}

static void reflection_interpreter_generic_unpin(
        SZrGlobalState *global,
        SZrRawObject *object,
        TZrBool addedByCaller) {
    if (addedByCaller && global != ZR_NULL && object != ZR_NULL) {
        ZrCore_GarbageCollector_UnignoreObject(global, object);
    }
}

static TZrBool reflection_interpreter_generic_is_supported_instance(
        const SZrObject *object) {
    return (TZrBool)(object != ZR_NULL &&
                     (object->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT ||
                      object->internalType == ZR_OBJECT_INTERNAL_TYPE_STRUCT));
}

/* object 由调用方在查找期间保活；返回值借用属性表，不能跨对象释放或属性表改写继续使用。 */
static const SZrTypeValue *reflection_interpreter_generic_get_field(
        SZrState *state,
        SZrObject *object,
        const TZrChar *fieldName) {
    SZrString *fieldString;
    SZrTypeValue fieldKey;
    const SZrTypeValue *fieldValue = ZR_NULL;
    TZrBool fieldStringPinned = ZR_FALSE;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }
    fieldString = ZrCore_String_CreateFromNative(state, (TZrNativeString)fieldName);
    if (fieldString != ZR_NULL &&
        reflection_interpreter_generic_pin(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString), &fieldStringPinned)) {
        ZrCore_Value_InitAsRawObject(
                state, &fieldKey, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
        fieldKey.type = ZR_VALUE_TYPE_STRING;
        fieldValue = ZrCore_Object_GetValue(state, object, &fieldKey);
    }
    reflection_interpreter_generic_unpin(state->global,
                                         fieldString != ZR_NULL
                                                 ? ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString)
                                                 : ZR_NULL,
                                         fieldStringPinned);
    return fieldValue;
}

/**
 * @brief 用经过 runtime 重验且走 interpreter-deopt 路由的泛型 carrier 创建语言对象。
 * @pre 传入的非空指针在调用期间有效；instance 借用的请求树在重验期间保持可读。
 * @return 空参数、prototype 非 class/struct、carrier 重验失败或分配失败时返回 NULL；成功返回带隐藏上下文的实例。
 * @note struct prototype 保留 struct 身份；上下文对象由新实例强引用，但 runtime 指针不延长其生命周期。返回前会撤销临时 pin，调用方须为实例建立根。
 */
SZrObject *ZrCore_Reflection_NewInterpreterGenericInstanceObject(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        const SZrReflectionDynamicGenericTypeInstance *instance,
        SZrObjectPrototype *openPrototype) {
    SZrReflectionDynamicGenericTypeInstance resolved;
    SZrObject *typeObject;
    SZrObject *object = ZR_NULL;
    SZrObject *result = ZR_NULL;
    SZrString *fieldName = ZR_NULL;
    SZrTypeValue fieldKey;
    SZrTypeValue fieldValue;
    const SZrTypeValue *storedValue;
    TZrBool typeObjectPinned = ZR_FALSE;
    TZrBool objectPinned = ZR_FALSE;
    TZrBool fieldNamePinned = ZR_FALSE;

    /* carrier 可能在生成后过期；先按当前 runtime 重放解析，并且只把 deopt 路由包装成解释器实例。 */
    if (state == ZR_NULL || runtime == ZR_NULL || instance == ZR_NULL || openPrototype == ZR_NULL ||
        (openPrototype->type != ZR_OBJECT_PROTOTYPE_TYPE_CLASS &&
         openPrototype->type != ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) ||
        !ZrCore_Reflection_RevalidateDynamicGenericTypeInstance(runtime, instance, &resolved) ||
        resolved.route != ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_INTERPRETER_DEOPT) {
        return ZR_NULL;
    }

    typeObject = ZrCore_Reflection_BuildDynamicGenericTypeInstanceObject(state, runtime, &resolved);
    if (typeObject == ZR_NULL ||
        !reflection_interpreter_generic_pin(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(typeObject), &typeObjectPinned)) {
        return ZR_NULL;
    }

    object = ZrCore_Object_New(state, openPrototype);
    if (object == ZR_NULL) {
        goto cleanup;
    }
    if (openPrototype->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
        object->internalType = ZR_OBJECT_INTERNAL_TYPE_STRUCT;
    }
    ZrCore_Object_Init(state, object);
    if (!reflection_interpreter_generic_pin(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(object), &objectPinned)) {
        goto cleanup;
    }

    fieldName = ZrCore_String_CreateFromNative(
            state, (TZrNativeString)kInterpreterGenericTypeObjectField);
    if (fieldName == ZR_NULL ||
        !reflection_interpreter_generic_pin(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldName), &fieldNamePinned)) {
        goto cleanup;
    }

    /* 临时根覆盖字段键和上下文安装；读回确认强引用已建立后才将实例交给调用方。 */
    ZrCore_Value_InitAsRawObject(state, &fieldKey, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldName));
    fieldKey.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsRawObject(state, &fieldValue, ZR_CAST_RAW_OBJECT_AS_SUPER(typeObject));
    fieldValue.type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Object_SetValue(state, object, &fieldKey, &fieldValue);
    storedValue = ZrCore_Object_GetValue(state, object, &fieldKey);
    if (storedValue == ZR_NULL || storedValue->type != ZR_VALUE_TYPE_OBJECT ||
        storedValue->value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(typeObject)) {
        goto cleanup;
    }
    result = object;

cleanup:
    reflection_interpreter_generic_unpin(state->global,
                                         fieldName != ZR_NULL
                                                 ? ZR_CAST_RAW_OBJECT_AS_SUPER(fieldName)
                                                 : ZR_NULL,
                                         fieldNamePinned);
    reflection_interpreter_generic_unpin(state->global,
                                         ZR_CAST_RAW_OBJECT_AS_SUPER(object),
                                         objectPinned);
    reflection_interpreter_generic_unpin(state->global,
                                         ZR_CAST_RAW_OBJECT_AS_SUPER(typeObject),
                                         typeObjectPinned);
    return result;
}

/**
 * @brief 从支持的对象或 struct 实例中取出其隐藏的反射泛型类型对象。
 * @pre 非空的 state 和 instanceObject 在读取期间有效。
 * @return 隐藏字段保存反射对象时返回该对象；空参数、普通对象、缺失字段或无效值返回 NULL。
 * @note 返回对象由 instanceObject 持有；若调用方要跨越后续 GC 保留它，须另行建立根。
 */
SZrObject *ZrCore_Reflection_GetInterpreterGenericInstanceTypeObject(
        SZrState *state,
        SZrObject *instanceObject) {
    const SZrTypeValue *fieldValue;
    SZrObject *typeObject = ZR_NULL;
    TZrBool objectPinned = ZR_FALSE;

    if (state == ZR_NULL ||
        !reflection_interpreter_generic_is_supported_instance(instanceObject) ||
        !reflection_interpreter_generic_pin(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(instanceObject), &objectPinned)) {
        return ZR_NULL;
    }

    fieldValue = reflection_interpreter_generic_get_field(
            state, instanceObject, kInterpreterGenericTypeObjectField);
    if (fieldValue != ZR_NULL && fieldValue->type == ZR_VALUE_TYPE_OBJECT &&
        fieldValue->value.object != ZR_NULL) {
        typeObject = ZR_CAST_OBJECT(state, fieldValue->value.object);
        if (!ZrCore_Reflection_IsReflectionObject(state, typeObject)) {
            typeObject = ZR_NULL;
        }
    }

    reflection_interpreter_generic_unpin(
            state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(instanceObject), objectPinned);
    return typeObject;
}

/* typeObject 由本 helper 固定；先核对 runtime 与 owner 身份，避免跨元数据域复用同值 token。 */
static SZrObject *reflection_interpreter_generic_resolve_parameter_type_object(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        SZrObject *typeObject,
        const TZrChar *genericOwnerField,
        TZrMetadataToken genericOwnerToken,
        TZrUInt32 parameterIndex) {
    SZrReflectionResolvedGenericParameter parameter;
    SZrObject *argumentsObject = ZR_NULL;
    SZrObject *result = ZR_NULL;
    const SZrTypeValue *fieldValue;
    const SZrTypeValue *argumentValue;
    SZrTypeValue argumentKey;
    TZrUInt32 argumentCount;
    TZrBool typeObjectPinned = ZR_FALSE;
    TZrBool argumentsPinned = ZR_FALSE;

    if (state == ZR_NULL || runtime == ZR_NULL || typeObject == ZR_NULL ||
        genericOwnerField == ZR_NULL ||
        !ZrCore_Reflection_IsReflectionObject(state, typeObject) ||
        !reflection_interpreter_generic_pin(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(typeObject), &typeObjectPinned) ||
        !ZrCore_Reflection_ResolveGenericParameter(
                runtime, genericOwnerToken, parameterIndex, &parameter)) {
        goto cleanup;
    }

    fieldValue = reflection_interpreter_generic_get_field(
            state, typeObject, kMetadataRuntimeField);
    if (fieldValue == ZR_NULL || fieldValue->type != ZR_VALUE_TYPE_NATIVE_POINTER ||
        fieldValue->value.nativeObject.nativePointer != runtime) {
        goto cleanup;
    }
    fieldValue = reflection_interpreter_generic_get_field(
            state, typeObject, genericOwnerField);
    if (fieldValue == ZR_NULL || !ZR_VALUE_IS_TYPE_INT(fieldValue->type) ||
        fieldValue->value.nativeObject.nativeInt64 < 0 ||
        (TZrUInt64)fieldValue->value.nativeObject.nativeInt64 != parameter.ownerToken) {
        goto cleanup;
    }
    /* count 字段、数组类型和物化后的实际长度必须一致，索引才对应该 owner 的泛型实参。 */
    fieldValue = reflection_interpreter_generic_get_field(
            state, typeObject, kGenericArgumentCountField);
    if (fieldValue == ZR_NULL || !ZR_VALUE_IS_TYPE_INT(fieldValue->type) ||
        fieldValue->value.nativeObject.nativeInt64 <= 0 ||
        (TZrUInt64)fieldValue->value.nativeObject.nativeInt64 > (TZrUInt64)(~(TZrUInt32)0u)) {
        goto cleanup;
    }
    argumentCount = (TZrUInt32)fieldValue->value.nativeObject.nativeInt64;
    if (parameter.parameterIndex >= argumentCount) {
        goto cleanup;
    }

    fieldValue = reflection_interpreter_generic_get_field(
            state, typeObject, kGenericArgumentsField);
    if (fieldValue == ZR_NULL || fieldValue->type != ZR_VALUE_TYPE_ARRAY ||
        fieldValue->value.object == ZR_NULL) {
        goto cleanup;
    }
    argumentsObject = ZR_CAST_OBJECT(state, fieldValue->value.object);
    if (argumentsObject->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY ||
        !ZrCore_Object_SuperArrayMaterializeGeneric(state, argumentsObject) ||
        argumentsObject->nodeMap.elementCount != argumentCount ||
        !reflection_interpreter_generic_pin(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(argumentsObject), &argumentsPinned)) {
        goto cleanup;
    }

    ZrCore_Value_InitAsInt(state, &argumentKey, parameter.parameterIndex);
    argumentValue = ZrCore_Object_GetValue(state, argumentsObject, &argumentKey);
    if (argumentValue != ZR_NULL && argumentValue->type == ZR_VALUE_TYPE_OBJECT &&
        argumentValue->value.object != ZR_NULL) {
        SZrObject *candidate = ZR_CAST_OBJECT(state, argumentValue->value.object);
        if (ZrCore_Reflection_IsReflectionObject(state, candidate)) {
            result = candidate;
        }
    }

cleanup:
    reflection_interpreter_generic_unpin(state != ZR_NULL ? state->global : ZR_NULL,
                                         argumentsObject != ZR_NULL
                                                 ? ZR_CAST_RAW_OBJECT_AS_SUPER(argumentsObject)
                                                 : ZR_NULL,
                                         argumentsPinned);
    reflection_interpreter_generic_unpin(state != ZR_NULL ? state->global : ZR_NULL,
                                         typeObject != ZR_NULL
                                                 ? ZR_CAST_RAW_OBJECT_AS_SUPER(typeObject)
                                                 : ZR_NULL,
                                         typeObjectPinned);
    return result;
}

/**
 * @brief 按实例的泛型 owner 和参数索引返回已经构造的实参反射对象。
 * @pre 非空的 state、runtime 和 instanceObject 在调用期间有效。
 * @return 空参数返回 NULL；隐藏上下文属于 runtime 和 genericOwnerToken 且数组形状及 parameterIndex 匹配时返回实参对象，否则返回 NULL。
 * @note 返回对象由实例的泛型参数数组持有；若要在实例之外跨 GC 保留，调用方须另行建立根。
 */
SZrObject *ZrCore_Reflection_ResolveInterpreterGenericParameterTypeObject(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        SZrObject *instanceObject,
        TZrMetadataToken genericOwnerToken,
        TZrUInt32 parameterIndex) {
    SZrObject *typeObject;
    SZrObject *result = ZR_NULL;
    TZrBool instancePinned = ZR_FALSE;

    if (state == ZR_NULL ||
        !reflection_interpreter_generic_is_supported_instance(instanceObject) ||
        !reflection_interpreter_generic_pin(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(instanceObject), &instancePinned)) {
        return ZR_NULL;
    }
    typeObject = ZrCore_Reflection_GetInterpreterGenericInstanceTypeObject(state, instanceObject);
    if (typeObject != ZR_NULL) {
        result = reflection_interpreter_generic_resolve_parameter_type_object(
                state,
                runtime,
                typeObject,
                kGenericBaseTokenField,
                genericOwnerToken,
                parameterIndex);
    }
    reflection_interpreter_generic_unpin(
            state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(instanceObject), instancePinned);
    return result;
}

/**
 * @brief 将实例的类型泛型上下文绑定到一个活动的解释器 CallInfo。
 * @pre 非空的 state、callInfo 和 instanceObject 在调用期间有效；非空 callInfo 属于当前活动帧。
 * @return 能提取实例上下文且 callInfo 为非 native 帧时返回 true；空参数或上下文无效时返回 false，旧类型上下文保持清空。
 * @note 活动 CallInfo 的上下文槽由 GC 扫描；帧退出或复用后不再替调用方持有对象。
 */
TZrBool ZrCore_Reflection_BindInterpreterGenericInstanceCallInfo(
        SZrState *state,
        SZrCallInfo *callInfo,
        SZrObject *instanceObject) {
    SZrObject *typeObject;

    /* 复用帧时先丢弃旧根；本次验证失败也不能遗留上一调用的泛型上下文。 */
    if (callInfo != ZR_NULL) {
        ZrCore_Value_ResetAsNull(&callInfo->interpreterGenericContext);
    }
    if (state == ZR_NULL || callInfo == ZR_NULL || instanceObject == ZR_NULL ||
        ZrCore_CallInfo_IsNative(callInfo)) {
        return ZR_FALSE;
    }
    typeObject = ZrCore_Reflection_GetInterpreterGenericInstanceTypeObject(state, instanceObject);
    if (typeObject == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsRawObject(
            state,
            &callInfo->interpreterGenericContext,
            ZR_CAST_RAW_OBJECT_AS_SUPER(typeObject));
    callInfo->interpreterGenericContext.type = ZR_VALUE_TYPE_OBJECT;
    return ZR_TRUE;
}

/**
 * @brief 读取非 native CallInfo 已绑定的类型泛型上下文。
 * @return 活动帧保存了反射对象时返回该对象；native 帧、空槽或无效对象返回 NULL。
 * @note 返回引用的 GC 根随 CallInfo 活跃期存在；要跨帧保留需由调用方另行建立根。
 */
SZrObject *ZrCore_Reflection_GetInterpreterGenericCallInfoTypeObject(
        SZrState *state,
        SZrCallInfo *callInfo) {
    SZrObject *typeObject;

    if (state == ZR_NULL || callInfo == ZR_NULL || ZrCore_CallInfo_IsNative(callInfo) ||
        callInfo->interpreterGenericContext.type != ZR_VALUE_TYPE_OBJECT ||
        callInfo->interpreterGenericContext.value.object == ZR_NULL) {
        return ZR_NULL;
    }
    typeObject = ZR_CAST_OBJECT(
            state, callInfo->interpreterGenericContext.value.object);
    return ZrCore_Reflection_IsReflectionObject(state, typeObject) ? typeObject : ZR_NULL;
}

/**
 * @brief 从活动 CallInfo 的类型上下文解析指定 owner 的泛型实参。
 * @pre 非空的 state、runtime 和 callInfo 在调用期间有效；非空 callInfo 保持为 GC 扫描的活动帧。
 * @return 空参数返回 NULL；CallInfo 类型上下文属于 runtime 与 genericOwnerToken 且 parameterIndex 有对应实参时返回对象，否则返回 NULL。
 * @note 返回对象由 CallInfo 的泛型上下文图持有；跨帧或后续 GC 保留需另行建立根。
 */
SZrObject *ZrCore_Reflection_ResolveInterpreterGenericCallInfoParameterTypeObject(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        SZrCallInfo *callInfo,
        TZrMetadataToken genericOwnerToken,
        TZrUInt32 parameterIndex) {
    SZrObject *typeObject = ZrCore_Reflection_GetInterpreterGenericCallInfoTypeObject(
            state, callInfo);

    if (typeObject == ZR_NULL) {
        return ZR_NULL;
    }
    return reflection_interpreter_generic_resolve_parameter_type_object(
            state,
            runtime,
            typeObject,
            kGenericBaseTokenField,
            genericOwnerToken,
            parameterIndex);
}

/**
 * @brief 将 MethodSpec 构造出的泛型方法上下文绑定到活动的解释器 CallInfo。
 * @pre 非空的 state、runtime 和 callInfo 在调用期间有效；非空 callInfo 属于当前活动帧，runtime 在上下文使用期间保持有效。
 * @return MethodSpec 有效且 callInfo 为非 native 帧、上下文构造成功时返回 true；空参数或其他失败返回 false，旧方法上下文保持清空。
 * @note 上下文对象只保存 metadataRuntime 的 native 指针；使用期间 runtime 必须仍然有效。
 */
TZrBool ZrCore_Reflection_BindInterpreterGenericMethodSpecCallInfo(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        SZrCallInfo *callInfo,
        TZrMetadataToken methodSpecToken) {
    SZrObject *contextObject;

    /* 类型上下文与方法上下文分槽保存；复用 CallInfo 前只清理本次要重绑的槽。 */
    if (callInfo != ZR_NULL) {
        ZrCore_Value_ResetAsNull(&callInfo->interpreterGenericMethodContext);
    }
    if (state == ZR_NULL || runtime == ZR_NULL || callInfo == ZR_NULL ||
        ZrCore_CallInfo_IsNative(callInfo)) {
        return ZR_FALSE;
    }
    contextObject = ZrCore_Reflection_BuildMethodSpecGenericContextObject(
            state, runtime, methodSpecToken);
    if (contextObject == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsRawObject(
            state,
            &callInfo->interpreterGenericMethodContext,
            ZR_CAST_RAW_OBJECT_AS_SUPER(contextObject));
    callInfo->interpreterGenericMethodContext.type = ZR_VALUE_TYPE_OBJECT;
    return ZR_TRUE;
}

/**
 * @brief 读取非 native CallInfo 已绑定的 MethodSpec 泛型上下文对象。
 * @return 活动帧保存有效反射对象时返回它；native 帧、空槽或无效对象返回 NULL。
 * @note GC 根随 CallInfo 活跃期存在；结果要跨帧保留时，调用方须另行建立根。
 */
SZrObject *ZrCore_Reflection_GetInterpreterGenericMethodCallInfoContextObject(
        SZrState *state,
        SZrCallInfo *callInfo) {
    SZrObject *contextObject;

    if (state == ZR_NULL || callInfo == ZR_NULL || ZrCore_CallInfo_IsNative(callInfo) ||
        callInfo->interpreterGenericMethodContext.type != ZR_VALUE_TYPE_OBJECT ||
        callInfo->interpreterGenericMethodContext.value.object == ZR_NULL) {
        return ZR_NULL;
    }
    contextObject = ZR_CAST_OBJECT(
            state, callInfo->interpreterGenericMethodContext.value.object);
    return ZrCore_Reflection_IsReflectionObject(state, contextObject)
                   ? contextObject
                   : ZR_NULL;
}

/**
 * @brief 从活动 CallInfo 的 MethodSpec 上下文解析指定方法 owner 的泛型实参。
 * @pre 非空的 state、runtime 和 callInfo 在调用期间有效；非空 callInfo 保持为 GC 扫描的活动帧。
 * @return 空参数返回 NULL；MethodSpec 上下文属于 runtime 与 genericMethodToken 且 parameterIndex 有对应实参时返回对象，否则返回 NULL。
 * @note 返回对象由 CallInfo 的方法上下文图持有；若需跨帧保留，调用方须另行建立根。
 */
SZrObject *ZrCore_Reflection_ResolveInterpreterGenericMethodCallInfoParameterTypeObject(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        SZrCallInfo *callInfo,
        TZrMetadataToken genericMethodToken,
        TZrUInt32 parameterIndex) {
    SZrObject *contextObject =
            ZrCore_Reflection_GetInterpreterGenericMethodCallInfoContextObject(
                    state, callInfo);

    if (contextObject == ZR_NULL) {
        return ZR_NULL;
    }
    return reflection_interpreter_generic_resolve_parameter_type_object(
            state,
            runtime,
            contextObject,
            kGenericMethodTokenField,
            genericMethodToken,
            parameterIndex);
}

/**
 * @brief 以 MethodSpec 的方法泛型上下文调用调用方已解析出的解释器函数。
 * @pre 非空的 state、runtime、function 和 result 在调用期间有效；function 须由调用方保证对应 methodSpecToken 的底层方法。
 *      非空 arguments 须覆盖 argumentCount 个值，其中 GC 值在上下文构造期间保持有根。
 * @return 空参数、非零 argumentCount 却无 arguments 或校验/调用失败时返回 false；完整泛型元数及函数形状匹配且调用成功时返回 true。
 * @note 本函数在通用分发复制或固定参数前构造 MethodSpec 上下文；native 参数数组须由调用方保活其中的 GC 值。
 */
TZrBool ZrCore_Reflection_InvokeInterpreterGenericMethodSpecResolvedFunction(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        TZrMetadataToken methodSpecToken,
        SZrFunction *function,
        const SZrTypeValue *arguments,
        TZrSize argumentCount,
        SZrTypeValue *result) {
    SZrMetadataRuntimeMethodSpecSignatureView signatureView;
    SZrReflectionResolvedGenericParameter genericParameter;
    SZrObject *contextObject = ZR_NULL;
    SZrTypeValue contextValue;
    TZrBool functionPinned = ZR_FALSE;
    TZrBool contextPinned = ZR_FALSE;
    TZrBool invoked = ZR_FALSE;
    TZrUInt32 genericArgumentIndex;

    if (result != ZR_NULL) {
        ZrCore_Value_ResetAsNull(result);
    }
    if (state == ZR_NULL || runtime == ZR_NULL || function == ZR_NULL ||
        result == ZR_NULL || state->threadStatus != ZR_THREAD_STATUS_FINE ||
        (argumentCount > 0u && arguments == ZR_NULL) ||
        function->super.type != ZR_RAW_OBJECT_TYPE_FUNCTION || function->super.isNative ||
        function->instructionsList == ZR_NULL || function->hasVariableArguments ||
        function->parameterCount != argumentCount ||
        !ZrCore_MetadataRuntime_ReadMethodSpecSignatureView(
                runtime, methodSpecToken, &signatureView) ||
        signatureView.argumentCount == 0u) {
        goto cleanup;
    }
    /* 要求 MethodSpec 覆盖该方法的全部泛型形参；多一个或少一个都不进入方法体。 */
    for (genericArgumentIndex = 0u;
         genericArgumentIndex < signatureView.argumentCount;
         ++genericArgumentIndex) {
        if (!ZrCore_Reflection_ResolveGenericParameter(
                    runtime,
                    signatureView.methodToken,
                    genericArgumentIndex,
                    &genericParameter)) {
            goto cleanup;
        }
    }
    if (ZrCore_Reflection_ResolveGenericParameter(
                runtime,
                signatureView.methodToken,
                signatureView.argumentCount,
                &genericParameter) ||
        !reflection_interpreter_generic_pin(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(function), &functionPinned)) {
        goto cleanup;
    }

    /* TODO: 上下文构造会分配 GC 对象，而 arguments 尚未由通用调用器复制或固定；
     * 核对仓外 native 调用方是否为托管实参保留根，必要时在此先固定输入。 */
    contextObject = ZrCore_Reflection_BuildMethodSpecGenericContextObject(
            state, runtime, methodSpecToken);
    if (contextObject == ZR_NULL ||
        !reflection_interpreter_generic_pin(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(contextObject), &contextPinned)) {
        goto cleanup;
    }
    ZrCore_Value_InitAsRawObject(
            state, &contextValue, ZR_CAST_RAW_OBJECT_AS_SUPER(contextObject));
    contextValue.type = ZR_VALUE_TYPE_OBJECT;
    invoked = ZrCore_Object_CallFunctionWithReceiverAndInterpreterGenericContexts(
            state,
            function,
            ZR_NULL,
            arguments,
            argumentCount,
            ZR_NULL,
            &contextValue,
            result);

cleanup:
    reflection_interpreter_generic_unpin(state != ZR_NULL ? state->global : ZR_NULL,
                                         contextObject != ZR_NULL
                                                 ? ZR_CAST_RAW_OBJECT_AS_SUPER(contextObject)
                                                 : ZR_NULL,
                                         contextPinned);
    reflection_interpreter_generic_unpin(state != ZR_NULL ? state->global : ZR_NULL,
                                         function != ZR_NULL
                                                 ? ZR_CAST_RAW_OBJECT_AS_SUPER(function)
                                                 : ZR_NULL,
                                         functionPinned);
    return invoked;
}

/**
 * @brief 从 MethodSpec 解析底层解释器函数，并用对应的方法泛型上下文同步调用。
 * @pre 非空的 state、runtime 和 result 在调用期间有效；非空 arguments 覆盖 argumentCount 个可读值，其中 GC 值在上下文构造可能触发的 GC 期间保持有根。
 * @return 空参数、非零 argumentCount 却无 arguments、MethodSpec/函数形状无效或调用失败时返回 false；成功时返回 true。
 * @note 通常由此入口解析 function；直接调用 ResolvedFunction 版本时调用方须确保函数身份与 MethodSpec 一致。
 */
TZrBool ZrCore_Reflection_InvokeInterpreterGenericMethodSpec(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        TZrMetadataToken methodSpecToken,
        const SZrTypeValue *arguments,
        TZrSize argumentCount,
        SZrTypeValue *result) {
    SZrMetadataRuntimeMethodSpecSignatureView signatureView;
    SZrMetadataRuntimeInterpreterMethodBindingView methodBindingView;

    if (result != ZR_NULL) {
        ZrCore_Value_ResetAsNull(result);
    }
    if (state == ZR_NULL || runtime == ZR_NULL || result == ZR_NULL ||
        !ZrCore_MetadataRuntime_ReadMethodSpecSignatureView(
                runtime, methodSpecToken, &signatureView) ||
        !ZrCore_MetadataRuntime_ReadInterpreterMethodBindingView(
                state, runtime, signatureView.methodToken, &methodBindingView)) {
        return ZR_FALSE;
    }
    return ZrCore_Reflection_InvokeInterpreterGenericMethodSpecResolvedFunction(
            state,
            runtime,
            methodSpecToken,
            methodBindingView.function,
            arguments,
            argumentCount,
            result);
}

/**
 * @brief 将解释器方法作为实例方法调用，并为方法体安装该闭合泛型实例的类型上下文。
 * @pre 非空的 state、runtime、instanceObject、function 和 result 在调用期间有效；function 须由调用方保证属于 genericOwnerToken 对应的方法；
 *      非空 arguments 须覆盖 argumentCount 个值，其中 GC 值在反射上下文读取期间保持有根。
 * @return 空参数、非零 argumentCount 却无 arguments、owner/函数形状/参数数不匹配或调用失败时返回 false；成功时返回 true。
 * @note 反射字段读取在键未驻留时可能先于通用分发分配键串并触发 GC；native 参数数组中的 GC 值须由调用方先保活。
 */
TZrBool ZrCore_Reflection_InvokeInterpreterGenericInstanceResolvedMethod(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        SZrObject *instanceObject,
        TZrMetadataToken genericOwnerToken,
        SZrFunction *function,
        const SZrTypeValue *arguments,
        TZrSize argumentCount,
        SZrTypeValue *result) {
    SZrObject *typeObject = ZR_NULL;
    const SZrTypeValue *runtimeValue;
    const SZrTypeValue *ownerValue;
    SZrTypeValue receiverValue;
    SZrTypeValue contextValue;
    TZrBool instancePinned = ZR_FALSE;
    TZrBool functionPinned = ZR_FALSE;
    TZrBool typeObjectPinned = ZR_FALSE;
    TZrBool invoked = ZR_FALSE;

    if (result != ZR_NULL) {
        ZrCore_Value_ResetAsNull(result);
    }
    if (state == ZR_NULL || runtime == ZR_NULL || instanceObject == ZR_NULL ||
        function == ZR_NULL || result == ZR_NULL || state->threadStatus != ZR_THREAD_STATUS_FINE ||
        (argumentCount > 0u && arguments == ZR_NULL) ||
        function->super.type != ZR_RAW_OBJECT_TYPE_FUNCTION || function->super.isNative ||
        function->instructionsList == ZR_NULL || function->hasVariableArguments ||
        function->parameterCount != argumentCount + 1u ||
        !reflection_interpreter_generic_pin(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(function), &functionPinned) ||
        !reflection_interpreter_generic_pin(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(instanceObject), &instancePinned)) {
        goto cleanup;
    }

    /* TODO: 取泛型上下文在键未驻留时可能分配字段键串并触发 GC，早于通用调用器固定 arguments；
     * 核对仓外 native 调用方的 GC 根约定，必要时先固定参数。 */
    typeObject = ZrCore_Reflection_GetInterpreterGenericInstanceTypeObject(
            state, instanceObject);
    if (typeObject == ZR_NULL ||
        !reflection_interpreter_generic_pin(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(typeObject), &typeObjectPinned)) {
        goto cleanup;
    }
    runtimeValue = reflection_interpreter_generic_get_field(
            state, typeObject, kMetadataRuntimeField);
    if (runtimeValue == ZR_NULL || runtimeValue->type != ZR_VALUE_TYPE_NATIVE_POINTER ||
        runtimeValue->value.nativeObject.nativePointer != runtime) {
        goto cleanup;
    }
    ownerValue = reflection_interpreter_generic_get_field(
            state, typeObject, kGenericBaseTokenField);
    if (ownerValue == ZR_NULL || !ZR_VALUE_IS_TYPE_INT(ownerValue->type) ||
        ownerValue->value.nativeObject.nativeInt64 < 0 ||
        (TZrUInt64)ownerValue->value.nativeObject.nativeInt64 != genericOwnerToken) {
        goto cleanup;
    }

    /* receiver 单独传递，故函数形参数量比显式 arguments 多一；类型上下文随新 CallInfo 入栈。 */
    ZrCore_Value_InitAsRawObject(
            state, &receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(instanceObject));
    receiverValue.type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsRawObject(
            state, &contextValue, ZR_CAST_RAW_OBJECT_AS_SUPER(typeObject));
    contextValue.type = ZR_VALUE_TYPE_OBJECT;
    invoked = ZrCore_Object_CallFunctionWithReceiverAndInterpreterGenericContext(
            state,
            function,
            &receiverValue,
            arguments,
            argumentCount,
            &contextValue,
            result);

cleanup:
    reflection_interpreter_generic_unpin(state != ZR_NULL ? state->global : ZR_NULL,
                                         typeObject != ZR_NULL
                                                 ? ZR_CAST_RAW_OBJECT_AS_SUPER(typeObject)
                                                 : ZR_NULL,
                                         typeObjectPinned);
    reflection_interpreter_generic_unpin(state != ZR_NULL ? state->global : ZR_NULL,
                                         instanceObject != ZR_NULL
                                                 ? ZR_CAST_RAW_OBJECT_AS_SUPER(instanceObject)
                                                 : ZR_NULL,
                                         instancePinned);
    reflection_interpreter_generic_unpin(state != ZR_NULL ? state->global : ZR_NULL,
                                         function != ZR_NULL
                                                 ? ZR_CAST_RAW_OBJECT_AS_SUPER(function)
                                                 : ZR_NULL,
                                         functionPinned);
    return invoked;
}
