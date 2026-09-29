#include "zr_vm_core/reflection.h"

#include "zr_vm_core/closure.h"
#include "zr_vm_core/constant_reference.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/execution_control.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/value.h"

#include <stdio.h>
#include <string.h>

/* 反射 TypeDescriptor 到运行时 prototype 的私有连接；由 TypeOfValue/元数据物化时写入，
 * 构造入口只借此回到真实 prototype，不把 descriptor 当作对象布局的权威副本。 */
static const TZrChar *kConstructionPrototypeField =
        "__zr_reflection_prototype";
/* entry function 持有 prototype/member 常量表；构造器选择从编译元数据回溯方法常量。 */
static const TZrChar *kConstructionEntryFunctionField =
        "__zr_reflection_entry_function";
/* 每个 descriptor 按参数数量及类型签名缓存选择结果，含失败结果以免反复扫描元数据。 */
static const TZrChar *kConstructionCacheFieldPrefix =
        "__zr_reflection_constructor_arity_";

/* 缓存中的整数编码：只有无显式构造器的零参数隐式构造可用 IMPLICIT。 */
enum {
    ZR_REFLECTION_CONSTRUCTION_CACHE_IMPLICIT = 0,
    ZR_REFLECTION_CONSTRUCTION_CACHE_NOT_FOUND = 1,
    ZR_REFLECTION_CONSTRUCTION_CACHE_AMBIGUOUS = 2,
};

/* TODO: 计数器是进程级且未同步；当前测试串行使用，需确认调试 API 是否承诺并发安全。 */
static SZrReflectionConstructionCacheStats gConstructionCacheStats;

/* TryRun 的上下文把可移动 VM 状态之外的构造目标和参数保持在调用者帧中；result 故意丢弃。 */
typedef struct SZrReflectionConstructorInvokeRequest {
    /* binder 已选出的 metadata function；不经再次动态重载查找。 */
    SZrFunction *constructor;
    /* 新实例 receiver 在调用期间由 NativeCallPin 保活。 */
    SZrTypeValue *receiver;
    /* 借用调用方参数数组，只在同步 TryRun 生命周期内有效。 */
    const SZrTypeValue *arguments;
    TZrSize argumentCount;
    SZrTypeValue *result;
    TZrBool invoked;
} SZrReflectionConstructorInvokeRequest;

/* 把 constructor 调用放进 TryRun 的可捕获边界；异常转换由外层 CreateInstance 统一收尾。 */
static void construction_invoke_body(SZrState *state, TZrPtr arguments) {
    SZrReflectionConstructorInvokeRequest *request =
            (SZrReflectionConstructorInvokeRequest *)arguments;

    if (state == ZR_NULL || request == ZR_NULL) {
        return;
    }
    request->invoked = ZrCore_Object_InvokeResolvedFunction(
            state,
            request->constructor,
            ZR_FALSE,
            request->receiver,
            request->arguments,
            request->argumentCount,
            request->result);
}

/* 构造器异常已转成 status 后清掉同一执行状态的异常与 pending-control，避免泄漏给调用者。 */
static void construction_clear_caught_exception(SZrState *state) {
    if (state == ZR_NULL) {
        return;
    }
    ZrCore_Exception_ClearCurrent(state);
    state->threadStatus = ZR_THREAD_STATUS_FINE;
    state->pendingControl.kind = ZR_VM_PENDING_CONTROL_NONE;
    state->pendingControl.callInfo = ZR_NULL;
    state->pendingControl.targetInstructionOffset = 0u;
    state->pendingControl.valueSlot = 0u;
    ZrCore_Value_ResetAsNull(&state->pendingControl.value);
    state->pendingControl.hasValue = ZR_FALSE;
}

/* 测试在限定的一组 binder 请求前重置计数；并发请求时不能把计数解释为 runtime 局部值。 */
void ZrCore_Reflection_DebugResetConstructionCacheStats(void) {
    memset(&gConstructionCacheStats, 0, sizeof(gConstructionCacheStats));
}

/* 与 reset 配对，供缓存命中/未命中用例检查 descriptor 缓存是否隔离。 */
SZrReflectionConstructionCacheStats
ZrCore_Reflection_DebugGetConstructionCacheStats(void) {
    return gConstructionCacheStats;
}

/* 允许内部失败路径统一填写可选 status，避免每个公共入口重复处理 NULL 输出槽。 */
static void construction_set_status(
        EZrReflectionConstructionStatus *outStatus,
        EZrReflectionConstructionStatus status) {
    if (outStatus != ZR_NULL) {
        *outStatus = status;
    }
}

/* 只用于读取反射对象上的内部字符串键；返回对象存储中的借用值，不转移所有权。 */
static const SZrTypeValue *construction_get_field(
        SZrState *state,
        SZrObject *object,
        const TZrChar *name) {
    SZrString *keyString;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    keyString = ZrCore_String_CreateFromNative(
            state, (TZrNativeString)name);
    if (keyString == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Value_InitAsRawObject(
            state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(state, object, &key);
}

/* 写 descriptor 私有字段供 binder 缓存复用；失败通过 VM threadStatus 暴露给当前调用。 */
static TZrBool construction_set_field(
        SZrState *state,
        SZrObject *object,
        const TZrChar *name,
        const SZrTypeValue *value) {
    SZrString *keyString;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || name == ZR_NULL ||
        value == ZR_NULL) {
        return ZR_FALSE;
    }
    keyString = ZrCore_String_CreateFromNative(
            state, (TZrNativeString)name);
    if (keyString == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsRawObject(
            state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Object_SetValue(state, object, &key, value);
    return state->threadStatus == ZR_THREAD_STATUS_FINE;
}

/* 将签名压成 descriptor 字段名；固定缓冲区不足时放弃缓存，构造语义仍由慢路径决定。 */
static TZrBool construction_cache_key(
        TZrSize argumentCount,
        TZrUInt64 argumentSignature,
        TZrChar *buffer,
        TZrSize bufferSize) {
    int length;

    if (buffer == ZR_NULL || bufferSize == 0u) {
        return ZR_FALSE;
    }
    length = snprintf(
            buffer,
            bufferSize,
            "%s%llu_%016llx",
            kConstructionCacheFieldPrefix,
            (unsigned long long)argumentCount,
            (unsigned long long)argumentSignature);
    return (TZrBool)(length > 0 && (TZrSize)length < bufferSize);
}

/* binder 签名的逐字节累积器；调用者负责提供有效、稳定的内存区间。 */
static TZrUInt64 construction_hash_bytes(
        TZrUInt64 hash,
        const void *bytes,
        TZrSize byteCount) {
    const TZrByte *position = (const TZrByte *)bytes;

    for (TZrSize index = 0u; index < byteCount; index++) {
        hash ^= position[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

/* TODO: 计划键只保存 64 位摘要，命中后没有原始形状复核；确认是否接受摘要碰撞风险。
 * 对象身份按 prototype 名折叠，array 元素类型也不入键，需与重载兼容契约一并确认。 */
static TZrUInt64 construction_argument_signature(
        SZrState *state,
        const SZrTypeValue *arguments,
        TZrSize argumentCount) {
    TZrUInt64 hash = UINT64_C(1469598103934665603);

    for (TZrSize index = 0u; index < argumentCount; index++) {
        const SZrTypeValue *argument = &arguments[index];
        TZrUInt32 valueType = (TZrUInt32)argument->type;

        hash = construction_hash_bytes(hash, &valueType, sizeof(valueType));
        if ((argument->type == ZR_VALUE_TYPE_OBJECT ||
             argument->type == ZR_VALUE_TYPE_ARRAY) &&
            argument->value.object != ZR_NULL &&
            argument->value.object->type == ZR_RAW_OBJECT_TYPE_OBJECT) {
            SZrObject *object = ZR_CAST_OBJECT(state, argument->value.object);

            if (object->prototype != ZR_NULL &&
                object->prototype->name != ZR_NULL) {
                const TZrChar *name = ZrCore_String_GetNativeString(
                        object->prototype->name);

                if (name != ZR_NULL) {
                    TZrSize nameLength = strlen(name);

                    hash = construction_hash_bytes(
                            hash, &nameLength, sizeof(nameLength));
                    hash = construction_hash_bytes(
                            hash, name, nameLength);
                }
            }
        }
    }
    return hash;
}

/* TODO: 用户类型目前按 metadata 短名称比较；确认跨模块同名类型是否应具备 nominal 区分。 */
static TZrBool construction_type_name_is(
        const SZrFunctionTypedTypeRef *type,
        const TZrChar *expected) {
    const TZrChar *name;

    if (type == ZR_NULL || type->typeName == ZR_NULL || expected == ZR_NULL) {
        return ZR_FALSE;
    }
    name = ZrCore_String_GetNativeString(type->typeName);
    return name != ZR_NULL && strcmp(name, expected) == 0;
}

/* TODO: array 参数只按外层 ARRAY 匹配，数值族分数只选候选而不转换实参；核对 binder 契约。
 * 其余评分表达精确/近祖先优先于数值族，再优先于 object 通配。 */
static TZrInt32 construction_argument_match_score(
        SZrState *state,
        const SZrTypeValue *argument,
        const SZrFunctionTypedTypeRef *parameterType) {
    if (argument == ZR_NULL || parameterType == ZR_NULL) {
        return -1;
    }
    if (argument->type == ZR_VALUE_TYPE_NULL) {
        return parameterType->isNullable ||
                       parameterType->baseType == ZR_VALUE_TYPE_NULL ||
                       parameterType->baseType == ZR_VALUE_TYPE_OBJECT
                       ? 1
                       : -1;
    }
    if (parameterType->isArray) {
        return argument->type == ZR_VALUE_TYPE_ARRAY ? 0 : -1;
    }
    if (parameterType->baseType == argument->type) {
        if (argument->type != ZR_VALUE_TYPE_OBJECT ||
            parameterType->typeName == ZR_NULL ||
            construction_type_name_is(parameterType, "object")) {
            return 0;
        }
        if (argument->value.object != ZR_NULL &&
            argument->value.object->type == ZR_RAW_OBJECT_TYPE_OBJECT) {
            SZrObjectPrototype *prototype =
                    ZR_CAST_OBJECT(state, argument->value.object)->prototype;
            TZrInt32 score = 0;

            while (prototype != ZR_NULL) {
                if (prototype->name != ZR_NULL &&
                    ZrCore_String_Equal(
                            prototype->name, parameterType->typeName)) {
                    return score;
                }
                prototype = prototype->superPrototype;
                score++;
            }
        }
        return -1;
    }
    if (parameterType->baseType == ZR_VALUE_TYPE_OBJECT &&
        (parameterType->typeName == ZR_NULL ||
         construction_type_name_is(parameterType, "object"))) {
        return 8;
    }
    if (ZR_VALUE_IS_TYPE_SIGNED_INT(parameterType->baseType) &&
        ZR_VALUE_IS_TYPE_SIGNED_INT(argument->type)) {
        return 2;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(parameterType->baseType) &&
        ZR_VALUE_IS_TYPE_UNSIGNED_INT(argument->type)) {
        return 2;
    }
    if (ZR_VALUE_IS_TYPE_FLOAT(parameterType->baseType) &&
        ZR_VALUE_IS_TYPE_FLOAT(argument->type)) {
        return 2;
    }
    return -1;
}

/* 缺少参数签名记录时，非零实参候选以弱分数保留作 fallback；需确认这符合重载约定。 */
static TZrInt32 construction_signature_match_score(
        SZrState *state,
        SZrFunction *function,
        const SZrTypeValue *arguments,
        TZrSize argumentCount) {
    TZrInt32 totalScore = 0;

    if (function == ZR_NULL) {
        return -1;
    }
    if (function->parameterMetadataCount == 0u) {
        return argumentCount == 0u ? 0 : 64;
    }
    if (function->parameterMetadata == ZR_NULL ||
        function->parameterMetadataCount != argumentCount) {
        return -1;
    }
    for (TZrSize index = 0u; index < argumentCount; index++) {
        TZrInt32 score = construction_argument_match_score(
                state,
                &arguments[index],
                &function->parameterMetadata[index].type);

        if (score < 0) {
            return -1;
        }
        totalScore += score;
    }
    return totalScore;
}

/* 从 descriptor 私有连接取出并校验内部 prototype；结果是 GC 管理对象的借用指针。 */
static SZrObjectPrototype *construction_get_prototype(
        SZrState *state,
        SZrObject *descriptor) {
    const SZrTypeValue *value = construction_get_field(
            state, descriptor, kConstructionPrototypeField);
    SZrObject *object;

    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_OBJECT ||
        value->value.object == ZR_NULL) {
        return ZR_NULL;
    }
    object = ZR_CAST_OBJECT(state, value->value.object);
    return object != ZR_NULL &&
                           object->internalType ==
                                   ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE
                   ? (SZrObjectPrototype *)object
                   : ZR_NULL;
}

/* 取得编译此 prototype 所在模块的 metadata function，后续用于读取成员表和常量池。 */
static SZrFunction *construction_get_entry_function(
        SZrState *state,
        SZrObjectPrototype *prototype) {
    const SZrTypeValue *value;

    if (prototype == ZR_NULL) {
        return ZR_NULL;
    }
    value = construction_get_field(
            state, &prototype->super, kConstructionEntryFunctionField);
    return ZrCore_Closure_GetMetadataFunctionFromValue(state, value);
}

/* 在 entry function 的紧凑 prototype 元数据中定位当前 prototype；坏长度或映射缺项即失败。 */
static const SZrCompiledPrototypeInfo *construction_get_compiled_info(
        SZrFunction *entryFunction,
        SZrObjectPrototype *prototype) {
    const TZrByte *position;
    TZrSize remaining;

    if (entryFunction == ZR_NULL || prototype == ZR_NULL ||
        entryFunction->prototypeData == ZR_NULL ||
        entryFunction->prototypeInstances == ZR_NULL ||
        entryFunction->prototypeDataLength <= sizeof(TZrUInt32)) {
        return ZR_NULL;
    }
    position = entryFunction->prototypeData + sizeof(TZrUInt32);
    remaining = entryFunction->prototypeDataLength - sizeof(TZrUInt32);
    for (TZrUInt32 index = 0u;
         index < entryFunction->prototypeCount &&
         index < entryFunction->prototypeInstancesLength;
         index++) {
        const SZrCompiledPrototypeInfo *info;
        TZrSize size;

        if (remaining < sizeof(SZrCompiledPrototypeInfo)) {
            return ZR_NULL;
        }
        info = (const SZrCompiledPrototypeInfo *)position;
        size = sizeof(SZrCompiledPrototypeInfo) +
               (TZrSize)info->inheritsCount * sizeof(TZrUInt32) +
               (TZrSize)info->decoratorsCount * sizeof(TZrUInt32) +
               (TZrSize)info->membersCount * sizeof(SZrCompiledMemberInfo);
        if (size > remaining) {
            return ZR_NULL;
        }
        if (entryFunction->prototypeInstances[index] == prototype) {
            return info;
        }
        position += size;
        remaining -= size;
    }
    return ZR_NULL;
}

/* 只挑当前 prototype 上 public、显式声明且 arity 相同的 constructor；同分候选留给 binder 报歧义。 */
static SZrFunction *construction_select_constructor(
        SZrState *state,
        SZrObjectPrototype *prototype,
        const SZrTypeValue *arguments,
        TZrSize argumentCount,
        TZrBool *outHasDeclaredConstructor,
        TZrUInt32 *outMatchCount,
        const SZrTypeValue **outFunctionValue) {
    SZrFunction *entryFunction = construction_get_entry_function(
            state, prototype);
    const SZrCompiledPrototypeInfo *info =
            construction_get_compiled_info(entryFunction, prototype);
    const SZrCompiledMemberInfo *members;
    SZrFunction *match = ZR_NULL;
    TZrInt32 bestScore = INT32_MAX;

    if (outHasDeclaredConstructor != ZR_NULL) {
        *outHasDeclaredConstructor = ZR_FALSE;
    }
    if (outMatchCount != ZR_NULL) {
        *outMatchCount = 0u;
    }
    if (outFunctionValue != ZR_NULL) {
        *outFunctionValue = ZR_NULL;
    }
    if (info == ZR_NULL || entryFunction == ZR_NULL) {
        return ZR_NULL;
    }
    members = (const SZrCompiledMemberInfo *)((const TZrByte *)info +
            sizeof(SZrCompiledPrototypeInfo) +
            (TZrSize)info->inheritsCount * sizeof(TZrUInt32) +
            (TZrSize)info->decoratorsCount * sizeof(TZrUInt32));
    for (TZrUInt32 index = 0u; index < info->membersCount; index++) {
        const SZrCompiledMemberInfo *member = &members[index];
        const SZrTypeValue *functionValue;
        SZrFunction *function;
        TZrInt32 matchScore;

        if (member->isMetaMethod == 0u ||
            member->metaType != ZR_META_CONSTRUCTOR) {
            continue;
        }
        if (outHasDeclaredConstructor != ZR_NULL) {
            *outHasDeclaredConstructor = ZR_TRUE;
        }
        if (member->accessModifier != ZR_ACCESS_CONSTANT_PUBLIC ||
            member->parameterCount != argumentCount ||
            member->functionConstantIndex >=
                    entryFunction->constantValueLength) {
            continue;
        }
        functionValue = &entryFunction->constantValueList[
                member->functionConstantIndex];
        function = ZrCore_Closure_GetMetadataFunctionFromValue(
                state, functionValue);
        if (function == ZR_NULL) {
            continue;
        }
        matchScore = construction_signature_match_score(
                state, function, arguments, argumentCount);
        if (matchScore < 0 || matchScore > bestScore) {
            continue;
        }
        if (matchScore < bestScore) {
            bestScore = matchScore;
            match = function;
            if (outFunctionValue != ZR_NULL) {
                *outFunctionValue = functionValue;
            }
            if (outMatchCount != ZR_NULL) {
                *outMatchCount = 1u;
            }
        } else if (outMatchCount != ZR_NULL) {
            (*outMatchCount)++;
        }
    }
    return match;
}

/* 读取成功、隐式构造、无匹配或歧义计划；命中时不重新解析 descriptor 的成员表。 */
static TZrBool construction_read_cached_plan(
        SZrState *state,
        SZrObject *descriptor,
        TZrSize argumentCount,
        TZrUInt64 argumentSignature,
        SZrFunction **outConstructor,
        EZrReflectionConstructionStatus *outStatus) {
    TZrChar key[96];
    const SZrTypeValue *cached;
    TZrInt64 cacheCode;

    if (outConstructor != ZR_NULL) {
        *outConstructor = ZR_NULL;
    }
    if (!construction_cache_key(
                argumentCount, argumentSignature, key, sizeof(key))) {
        return ZR_FALSE;
    }
    cached = construction_get_field(state, descriptor, key);
    if (cached == ZR_NULL) {
        return ZR_FALSE;
    }
    if (ZR_VALUE_IS_TYPE_INT(cached->type)) {
        cacheCode = ZR_VALUE_IS_TYPE_SIGNED_INT(cached->type)
                            ? cached->value.nativeObject.nativeInt64
                            : (TZrInt64)cached->value.nativeObject.nativeUInt64;
        switch (cacheCode) {
            case ZR_REFLECTION_CONSTRUCTION_CACHE_IMPLICIT:
                construction_set_status(
                        outStatus, ZR_REFLECTION_CONSTRUCTION_STATUS_OK);
                break;
            case ZR_REFLECTION_CONSTRUCTION_CACHE_NOT_FOUND:
                construction_set_status(
                        outStatus,
                        ZR_REFLECTION_CONSTRUCTION_STATUS_CONSTRUCTOR_NOT_FOUND);
                break;
            case ZR_REFLECTION_CONSTRUCTION_CACHE_AMBIGUOUS:
                construction_set_status(
                        outStatus,
                        ZR_REFLECTION_CONSTRUCTION_STATUS_CONSTRUCTOR_AMBIGUOUS);
                break;
            default:
                return ZR_FALSE;
        }
    } else {
        SZrFunction *constructor =
                ZrCore_Closure_GetMetadataFunctionFromValue(state, cached);

        if (constructor == ZR_NULL) {
            return ZR_FALSE;
        }
        if (outConstructor != ZR_NULL) {
            *outConstructor = constructor;
        }
        construction_set_status(
                outStatus, ZR_REFLECTION_CONSTRUCTION_STATUS_OK);
    }
    gConstructionCacheStats.hitCount++;
    return ZR_TRUE;
}

/* 将成功/负计划存回 descriptor；键隔离参数形状，descriptor 身份隔离不同模块代际。 */
static void construction_store_cached_plan(
        SZrState *state,
        SZrObject *descriptor,
        TZrSize argumentCount,
        TZrUInt64 argumentSignature,
        const SZrTypeValue *functionValue,
        EZrReflectionConstructionStatus status) {
    TZrChar key[96];
    SZrTypeValue cached;

    if (!construction_cache_key(
                argumentCount, argumentSignature, key, sizeof(key))) {
        return;
    }
    if (functionValue != ZR_NULL &&
        status == ZR_REFLECTION_CONSTRUCTION_STATUS_OK) {
        ZrCore_Value_Copy(state, &cached, functionValue);
    } else {
        TZrInt64 cacheCode;

        switch (status) {
            case ZR_REFLECTION_CONSTRUCTION_STATUS_OK:
                cacheCode = ZR_REFLECTION_CONSTRUCTION_CACHE_IMPLICIT;
                break;
            case ZR_REFLECTION_CONSTRUCTION_STATUS_CONSTRUCTOR_AMBIGUOUS:
                cacheCode = ZR_REFLECTION_CONSTRUCTION_CACHE_AMBIGUOUS;
                break;
            default:
                cacheCode = ZR_REFLECTION_CONSTRUCTION_CACHE_NOT_FOUND;
                break;
        }
        ZrCore_Value_InitAsInt(state, &cached, cacheCode);
    }
    construction_set_field(state, descriptor, key, &cached);
}

/* 统一 cache-first 的构造器绑定；无显式 ctor 仅对零实参回退为隐式构造。 */
static SZrFunction *construction_bind_constructor(
        SZrState *state,
        SZrObject *descriptor,
        SZrObjectPrototype *prototype,
        const SZrTypeValue *arguments,
        TZrSize argumentCount,
        EZrReflectionConstructionStatus *outStatus) {
    SZrFunction *constructor;
    const SZrTypeValue *functionValue = ZR_NULL;
    TZrBool hasDeclaredConstructor = ZR_FALSE;
    TZrUInt32 matchCount = 0u;
    EZrReflectionConstructionStatus status;
    TZrUInt64 argumentSignature = construction_argument_signature(
            state, arguments, argumentCount);

    if (construction_read_cached_plan(
                state,
                descriptor,
                argumentCount,
                argumentSignature,
                &constructor,
                outStatus)) {
        return constructor;
    }
    gConstructionCacheStats.missCount++;
    constructor = construction_select_constructor(
            state,
            prototype,
            arguments,
            argumentCount,
            &hasDeclaredConstructor,
            &matchCount,
            &functionValue);
    if (matchCount > 1u) {
        status = ZR_REFLECTION_CONSTRUCTION_STATUS_CONSTRUCTOR_AMBIGUOUS;
        constructor = ZR_NULL;
        functionValue = ZR_NULL;
    } else if (constructor == ZR_NULL &&
               (hasDeclaredConstructor || argumentCount != 0u)) {
        status = ZR_REFLECTION_CONSTRUCTION_STATUS_CONSTRUCTOR_NOT_FOUND;
    } else {
        status = ZR_REFLECTION_CONSTRUCTION_STATUS_OK;
    }
    construction_set_status(outStatus, status);
    construction_store_cached_plan(
            state,
            descriptor,
            argumentCount,
            argumentSignature,
            functionValue,
            status);
    return constructor;
}

/* 通过受校验的 TypeId 读取类别，拒绝仅凭 descriptor 外形伪造的构造资格。 */
static TZrBool construction_read_category(
        SZrState *state,
        SZrObject *descriptor,
        EZrReflectionTypeCategory *outCategory) {
    const SZrTypeValue *idValue = construction_get_field(
            state, descriptor, "id");
    SZrReflectionTypeIdentity identity;

    if (outCategory != ZR_NULL) {
        *outCategory = ZR_REFLECTION_TYPE_CATEGORY_ERASED;
    }
    if (idValue == ZR_NULL || idValue->type != ZR_VALUE_TYPE_OBJECT ||
        idValue->value.object == ZR_NULL ||
        !ZrCore_Reflection_ReadTypeIdObject(
                state,
                ZR_CAST_OBJECT(state, idValue->value.object),
                &identity,
                ZR_NULL)) {
        return ZR_FALSE;
    }
    if (outCategory != ZR_NULL) {
        *outCategory = identity.category;
    }
    return ZR_TRUE;
}

/* descriptor native 的前置门：类别与实际 prototype 必须同时支持构造，供脚本快速探测。 */
TZrBool ZrCore_Reflection_RequireConstructible(
        SZrState *state,
        SZrObject *typeDescriptor,
        EZrReflectionConstructionStatus *outStatus) {
    EZrReflectionTypeCategory category;
    SZrObjectPrototype *prototype;

    construction_set_status(
            outStatus, ZR_REFLECTION_CONSTRUCTION_STATUS_INVALID_ARGUMENT);
    if (state == ZR_NULL || typeDescriptor == ZR_NULL ||
        !ZrCore_Reflection_IsReflectionObject(state, typeDescriptor) ||
        !construction_read_category(state, typeDescriptor, &category)) {
        return ZR_FALSE;
    }
    if (category != ZR_REFLECTION_TYPE_CATEGORY_CONCRETE_CLASS &&
        category != ZR_REFLECTION_TYPE_CATEGORY_INSTANCE_CLASS &&
        category != ZR_REFLECTION_TYPE_CATEGORY_STRUCT) {
        construction_set_status(
                outStatus,
                ZR_REFLECTION_CONSTRUCTION_STATUS_TYPE_NOT_CONSTRUCTIBLE);
        return ZR_FALSE;
    }
    prototype = construction_get_prototype(state, typeDescriptor);
    if (prototype == ZR_NULL ||
        (prototype->type != ZR_OBJECT_PROTOTYPE_TYPE_CLASS &&
         prototype->type != ZR_OBJECT_PROTOTYPE_TYPE_STRUCT)) {
        construction_set_status(
                outStatus,
                ZR_REFLECTION_CONSTRUCTION_STATUS_TYPE_NOT_CONSTRUCTIBLE);
        return ZR_FALSE;
    }
    construction_set_status(
            outStatus, ZR_REFLECTION_CONSTRUCTION_STATUS_OK);
    return ZR_TRUE;
}

/* 反射专用实例化入口：先绑定 public constructor，再隔离 VM 调用状态并返回新对象；
 * ordinary new/init 不经过此 binder。arguments 只借用至本次同步调用结束。 */
TZrBool ZrCore_Reflection_CreateInstance(
        SZrState *state,
        SZrObject *typeDescriptor,
        const SZrTypeValue *arguments,
        TZrSize argumentCount,
        SZrTypeValue *result,
        EZrReflectionConstructionStatus *outStatus) {
    SZrObjectPrototype *prototype;
    SZrFunction *constructor;
    SZrObject *instance;
    SZrTypeValue receiver;
    SZrTypeValue ignoredResult;
    SZrGcNativeCallPin instancePin;
    EZrReflectionConstructionStatus bindStatus;
    SZrReflectionConstructorInvokeRequest request;
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
    EZrThreadStatus invokeStatus;

    if (result != ZR_NULL) {
        ZrCore_Value_ResetAsNull(result);
    }
    construction_set_status(
            outStatus, ZR_REFLECTION_CONSTRUCTION_STATUS_INVALID_ARGUMENT);
    if (result == ZR_NULL || (argumentCount > 0u && arguments == ZR_NULL) ||
        !ZrCore_Reflection_RequireConstructible(
                state, typeDescriptor, outStatus)) {
        return ZR_FALSE;
    }
    prototype = construction_get_prototype(state, typeDescriptor);
    constructor = construction_bind_constructor(
            state,
            typeDescriptor,
            prototype,
            arguments,
            argumentCount,
            &bindStatus);
    construction_set_status(outStatus, bindStatus);
    if (bindStatus != ZR_REFLECTION_CONSTRUCTION_STATUS_OK) {
        return ZR_FALSE;
    }

    /* 从这里到结果交付，实例必须跨构造器分配/GC 保活；所有失败出口都需解除 pin。 */
    instance = ZrCore_Object_New(state, prototype);
    if (instance == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Object_Init(state, instance);
    instance->internalType = prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT
                                     ? ZR_OBJECT_INTERNAL_TYPE_STRUCT
                                     : ZR_OBJECT_INTERNAL_TYPE_OBJECT;
    if (!ZrCore_Gc_NativeCallPinObject(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(instance), &instancePin)) {
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsRawObject(
            state, &receiver, ZR_CAST_RAW_OBJECT_AS_SUPER(instance));
    receiver.type = ZR_VALUE_TYPE_OBJECT;
    if (constructor == ZR_NULL) {
        *result = receiver;
        ZrCore_Gc_NativeCallUnpin(state->global, &instancePin);
        construction_set_status(
                outStatus, ZR_REFLECTION_CONSTRUCTION_STATUS_OK);
        return ZR_TRUE;
    }

    ZrCore_Value_ResetAsNull(&ignoredResult);
    request.constructor = constructor;
    request.receiver = &receiver;
    request.arguments = arguments;
    request.argumentCount = argumentCount;
    request.result = &ignoredResult;
    request.invoked = ZR_FALSE;
    /* 构造器可扩展 VM 栈、压入 handler 或 AOT roots；恢复锚点而非保存裸栈地址，
     * 让反射调用作为嵌套调用返回到原调用者的 frame/handler/root 深度。 */
    savedCallInfo = state->callInfoList;
    savedExceptionHandlerStackLength = state->exceptionHandlerStackLength;
    savedRootFrame = state->aotGcRootFrameStack;
    savedRootDepth = state->aotGcRootFrameDepth;
    ZrCore_Function_StackAnchorInit(
            state, state->stackTop.valuePointer, &savedStackTopAnchor);
    if (savedCallInfo != ZR_NULL &&
        savedCallInfo->functionBase.valuePointer != ZR_NULL) {
        ZrCore_Function_StackAnchorInit(
                state,
                savedCallInfo->functionBase.valuePointer,
                &savedCallInfoBaseAnchor);
        hasSavedCallInfoBase = ZR_TRUE;
    }
    if (savedCallInfo != ZR_NULL &&
        savedCallInfo->functionTop.valuePointer != ZR_NULL) {
        ZrCore_Function_StackAnchorInit(
                state,
                savedCallInfo->functionTop.valuePointer,
                &savedCallInfoTopAnchor);
        hasSavedCallInfoTop = ZR_TRUE;
    }
    if (savedCallInfo != ZR_NULL && savedCallInfo->hasReturnDestination &&
        savedCallInfo->returnDestination != ZR_NULL) {
        ZrCore_Function_StackAnchorInit(
                state,
                savedCallInfo->returnDestination,
                &savedCallInfoReturnAnchor);
        hasSavedCallInfoReturn = ZR_TRUE;
    }
    invokeStatus = ZrCore_Exception_TryRun(
            state, construction_invoke_body, &request);
    state->aotGcRootFrameStack = savedRootFrame;
    state->aotGcRootFrameDepth = savedRootDepth;
    {
        EZrThreadStatus handlerStatus = execution_discard_exception_handlers_to_depth(
                state, savedExceptionHandlerStackLength);
        if (invokeStatus == ZR_THREAD_STATUS_FINE) {
            invokeStatus = handlerStatus;
        }
    }
    state->aotGcRootFrameStack = savedRootFrame;
    state->aotGcRootFrameDepth = savedRootDepth;
    state->stackTop.valuePointer = ZrCore_Function_StackAnchorRestore(
            state, &savedStackTopAnchor);
    if (savedCallInfo != ZR_NULL) {
        state->callInfoList = savedCallInfo;
        if (hasSavedCallInfoBase) {
            savedCallInfo->functionBase.valuePointer =
                    ZrCore_Function_StackAnchorRestore(
                            state, &savedCallInfoBaseAnchor);
        }
        if (hasSavedCallInfoTop) {
            savedCallInfo->functionTop.valuePointer =
                    ZrCore_Function_StackAnchorRestore(
                            state, &savedCallInfoTopAnchor);
        }
        if (hasSavedCallInfoReturn) {
            savedCallInfo->returnDestination =
                    ZrCore_Function_StackAnchorRestore(
                            state, &savedCallInfoReturnAnchor);
        }
    }
    /* 构造器失败不向反射 caller 泄漏半初始化实例；异常被清理并映射为统一 status。 */
    if (invokeStatus != ZR_THREAD_STATUS_FINE || !request.invoked ||
        state->threadStatus != ZR_THREAD_STATUS_FINE) {
        construction_clear_caught_exception(state);
        ZrCore_Object_DropManagedFields(state, instance);
        ZrCore_Gc_NativeCallUnpin(state->global, &instancePin);
        construction_set_status(
                outStatus,
                ZR_REFLECTION_CONSTRUCTION_STATUS_CONSTRUCTOR_THREW);
        return ZR_FALSE;
    }
    *result = receiver;
    ZrCore_Gc_NativeCallUnpin(state->global, &instancePin);
    construction_set_status(
            outStatus, ZR_REFLECTION_CONSTRUCTION_STATUS_OK);
    return ZR_TRUE;
}
