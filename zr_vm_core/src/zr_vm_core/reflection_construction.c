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

static const TZrChar *kConstructionPrototypeField =
        "__zr_reflection_prototype";
static const TZrChar *kConstructionEntryFunctionField =
        "__zr_reflection_entry_function";
static const TZrChar *kConstructionCacheFieldPrefix =
        "__zr_reflection_constructor_arity_";

enum {
    ZR_REFLECTION_CONSTRUCTION_CACHE_IMPLICIT = 0,
    ZR_REFLECTION_CONSTRUCTION_CACHE_NOT_FOUND = 1,
    ZR_REFLECTION_CONSTRUCTION_CACHE_AMBIGUOUS = 2,
};

static SZrReflectionConstructionCacheStats gConstructionCacheStats;

typedef struct SZrReflectionConstructorInvokeRequest {
    SZrFunction *constructor;
    SZrTypeValue *receiver;
    const SZrTypeValue *arguments;
    TZrSize argumentCount;
    SZrTypeValue *result;
    TZrBool invoked;
} SZrReflectionConstructorInvokeRequest;

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

void ZrCore_Reflection_DebugResetConstructionCacheStats(void) {
    memset(&gConstructionCacheStats, 0, sizeof(gConstructionCacheStats));
}

SZrReflectionConstructionCacheStats
ZrCore_Reflection_DebugGetConstructionCacheStats(void) {
    return gConstructionCacheStats;
}

static void construction_set_status(
        EZrReflectionConstructionStatus *outStatus,
        EZrReflectionConstructionStatus status) {
    if (outStatus != ZR_NULL) {
        *outStatus = status;
    }
}

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
