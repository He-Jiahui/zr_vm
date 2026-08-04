#include "reflection_descriptor_native_internal.h"

#include "reflection_object_internal.h"

#include "zr_vm_core/call_info.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#include <stdlib.h>
#include <string.h>

#define ZR_REFLECTION_DESCRIPTOR_MAX_PARAMETER_TYPES 1024u

typedef TZrInt64 (*FZrReflectionDescriptorNativeEntry)(SZrState *state);

static TZrInt64 descriptor_native_return_null(
        SZrState *state,
        TZrStackValuePointer functionBase) {
    if (state == ZR_NULL || functionBase == ZR_NULL) {
        return 0;
    }
    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(functionBase));
    state->stackTop.valuePointer = functionBase + 1;
    return 1;
}

static TZrInt64 descriptor_native_return_object(
        SZrState *state,
        TZrStackValuePointer functionBase,
        SZrObject *object,
        EZrValueType valueType) {
    if (object == ZR_NULL) {
        return descriptor_native_return_null(state, functionBase);
    }
    ZrCore_Value_InitAsRawObject(
            state,
            ZrCore_Stack_GetValue(functionBase),
            ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    ZrCore_Stack_GetValue(functionBase)->type = valueType;
    state->stackTop.valuePointer = functionBase + 1;
    return 1;
}

static TZrBool descriptor_native_call_frame(
        SZrState *state,
        TZrStackValuePointer *outFunctionBase,
        SZrObject **outDescriptor,
        TZrSize *outArgumentCount) {
    TZrStackValuePointer functionBase;
    SZrTypeValue *callableValue;
    SZrClosureNative *closure;
    SZrRawObject *captureOwner;
    SZrTypeValue *descriptorValue;

    *outFunctionBase = ZR_NULL;
    *outDescriptor = ZR_NULL;
    *outArgumentCount = 0u;
    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return ZR_FALSE;
    }
    functionBase = state->callInfoList->functionBase.valuePointer;
    if (functionBase == ZR_NULL || state->stackTop.valuePointer < functionBase + 1) {
        return ZR_FALSE;
    }
    callableValue = ZrCore_Stack_GetValue(functionBase);
    if (callableValue == ZR_NULL ||
        callableValue->type != ZR_VALUE_TYPE_CLOSURE ||
        !callableValue->isNative ||
        callableValue->value.object == ZR_NULL ||
        callableValue->value.object->type != ZR_RAW_OBJECT_TYPE_CLOSURE ||
        !callableValue->value.object->isNative) {
        return ZR_FALSE;
    }
    closure = ZR_CAST_NATIVE_CLOSURE(state, callableValue->value.object);
    if (closure->nativeBindingUsesReceiver != ZR_NATIVE_BINDING_RECEIVER_CAPTURED ||
        closure->closureValueCount != 1u ||
        closure->closureValuesExtend[0] != ZR_NULL) {
        return ZR_FALSE;
    }
    captureOwner = ZrCore_ClosureNative_GetCaptureOwner(closure, 0u);
    if (captureOwner == ZR_NULL ||
        captureOwner->type != ZR_RAW_OBJECT_TYPE_CLOSURE_VALUE ||
        captureOwner->isNative ||
        !ZrCore_ClosureValue_IsClosed((SZrClosureValue *) captureOwner)) {
        return ZR_FALSE;
    }
    descriptorValue = ZrCore_ClosureValue_GetValue((SZrClosureValue *) captureOwner);
    if (descriptorValue == ZR_NULL || descriptorValue->type != ZR_VALUE_TYPE_OBJECT ||
        descriptorValue->value.object == ZR_NULL ||
        descriptorValue->value.object->type != ZR_RAW_OBJECT_TYPE_OBJECT) {
        return ZR_FALSE;
    }
    *outFunctionBase = functionBase;
    *outDescriptor = ZR_CAST_OBJECT(state, descriptorValue->value.object);
    *outArgumentCount = (TZrSize)(state->stackTop.valuePointer - functionBase - 1);
    return ZR_TRUE;
}

static TZrBool descriptor_native_decode_member_query(
        SZrState *state,
        const SZrTypeValue *value,
        SZrReflectionMemberQuery *query) {
    static const TZrChar *kScopeNames[] = {"declared", "inherited", "all"};
    static const TZrChar *kAccessNames[] = {"public", "protected", "private", "all"};
    static const TZrChar *kStorageNames[] = {"instance", "static", "all"};
    SZrObject *queryObject;
    const SZrTypeValue *field;
    TZrUInt32 *enumTargets[3];
    const TZrChar *const *enumNames[3] = {kScopeNames, kAccessNames, kStorageNames};
    const TZrChar *enumFields[3] = {"scope", "access", "storage"};
    TZrUInt32 enumCounts[3] = {
            ZR_ARRAY_COUNT(kScopeNames),
            ZR_ARRAY_COUNT(kAccessNames),
            ZR_ARRAY_COUNT(kStorageNames),
    };
    TZrUInt32 enumValues[3];

    ZrCore_Reflection_MemberQueryInitDefault(query);
    if (value == ZR_NULL || value->type == ZR_VALUE_TYPE_NULL) {
        return ZR_TRUE;
    }
    if (value->type != ZR_VALUE_TYPE_OBJECT || value->value.object == ZR_NULL ||
        value->value.object->type != ZR_RAW_OBJECT_TYPE_OBJECT) {
        return ZR_FALSE;
    }
    queryObject = ZR_CAST_OBJECT(state, value->value.object);
    enumValues[0] = (TZrUInt32)query->scope;
    enumValues[1] = (TZrUInt32)query->access;
    enumValues[2] = (TZrUInt32)query->storage;
    enumTargets[0] = &enumValues[0];
    enumTargets[1] = &enumValues[1];
    enumTargets[2] = &enumValues[2];
    for (TZrUInt32 enumIndex = 0u; enumIndex < 3u; enumIndex++) {
        field = ZrCore_Reflection_ObjectGetFieldValue(
                state, queryObject, enumFields[enumIndex]);
        if (field == ZR_NULL) {
            continue;
        }
        if (ZR_VALUE_IS_TYPE_SIGNED_INT(field->type)) {
            TZrInt64 number = field->value.nativeObject.nativeInt64;
            if (number < 0 || (TZrUInt64)number >= enumCounts[enumIndex]) {
                return ZR_FALSE;
            }
            *enumTargets[enumIndex] = (TZrUInt32)number;
            continue;
        }
        if (field->type == ZR_VALUE_TYPE_STRING && field->value.object != ZR_NULL) {
            const TZrChar *text = ZrCore_String_GetNativeString(
                    ZR_CAST_STRING(state, field->value.object));
            TZrBool matched = ZR_FALSE;
            for (TZrUInt32 nameIndex = 0u;
                 nameIndex < enumCounts[enumIndex]; nameIndex++) {
                if (strcmp(text, enumNames[enumIndex][nameIndex]) == 0) {
                    *enumTargets[enumIndex] = nameIndex;
                    matched = ZR_TRUE;
                    break;
                }
            }
            if (matched) {
                continue;
            }
        }
        return ZR_FALSE;
    }
    query->scope = (EZrReflectionMemberScope)enumValues[0];
    query->access = (EZrReflectionMemberAccess)enumValues[1];
    query->storage = (EZrReflectionMemberStorage)enumValues[2];
    field = ZrCore_Reflection_ObjectGetFieldValue(
            state, queryObject, "includeCompilerGenerated");
    if (field != ZR_NULL) {
        if (field->type != ZR_VALUE_TYPE_BOOL) {
            return ZR_FALSE;
        }
        query->includeCompilerGenerated =
                field->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE;
    }
    field = ZrCore_Reflection_ObjectGetFieldValue(
            state, queryObject, "includeMetaMethods");
    if (field != ZR_NULL) {
        if (field->type != ZR_VALUE_TYPE_BOOL) {
            return ZR_FALSE;
        }
        query->includeMetaMethods =
                field->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool descriptor_native_read_parameter_type_ids(
        SZrState *state,
        const SZrTypeValue *value,
        const SZrObject ***outTypeIds,
        TZrUInt32 *outTypeIdCount) {
    SZrObject *array;
    const SZrObject **typeIds;
    TZrUInt32 count;

    *outTypeIds = ZR_NULL;
    *outTypeIdCount = 0u;
    if (value == ZR_NULL || value->type == ZR_VALUE_TYPE_NULL) {
        return ZR_TRUE;
    }
    if (value->type != ZR_VALUE_TYPE_ARRAY || value->value.object == ZR_NULL) {
        return ZR_FALSE;
    }
    array = ZR_CAST_OBJECT(state, value->value.object);
    if (array->nodeMap.elementCount >
        ZR_REFLECTION_DESCRIPTOR_MAX_PARAMETER_TYPES) {
        return ZR_FALSE;
    }
    count = (TZrUInt32)array->nodeMap.elementCount;
    if (count == 0u) {
        return ZR_TRUE;
    }
    typeIds = (const SZrObject **)malloc(sizeof(*typeIds) * count);
    if (typeIds == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrUInt32 index = 0u; index < count; index++) {
        SZrTypeValue key;
        const SZrTypeValue *entry;

        ZrCore_Value_InitAsInt(state, &key, (TZrInt64)index);
        entry = ZrCore_Object_GetValue(state, array, &key);
        if (entry == ZR_NULL || entry->type != ZR_VALUE_TYPE_OBJECT ||
            entry->value.object == ZR_NULL ||
            !ZrCore_Reflection_IsTypeIdObject(
                    state, ZR_CAST_OBJECT(state, entry->value.object))) {
            free(typeIds);
            return ZR_FALSE;
        }
        typeIds[index] = ZR_CAST_OBJECT(state, entry->value.object);
    }
    *outTypeIds = typeIds;
    *outTypeIdCount = count;
    return ZR_TRUE;
}

static TZrInt64 descriptor_native_query(
        SZrState *state,
        EZrReflectionMemberKind kind,
        TZrBool singular) {
    TZrStackValuePointer functionBase = ZR_NULL;
    SZrObject *descriptor = ZR_NULL;
    TZrSize argumentCount = 0u;
    SZrReflectionMemberQuery query;
    const SZrTypeValue *nameValue = ZR_NULL;
    const SZrTypeValue *queryValue = ZR_NULL;
    const SZrTypeValue *parameterTypesValue = ZR_NULL;
    const SZrObject **parameterTypeIds = ZR_NULL;
    TZrUInt32 parameterTypeIdCount = 0u;
    EZrReflectionQueryStatus status;
    SZrObject *result = ZR_NULL;

    if (!descriptor_native_call_frame(
                state, &functionBase, &descriptor, &argumentCount)) {
        return descriptor_native_return_null(state, functionBase);
    }
    if (singular) {
        TZrSize maximumArguments =
                kind == ZR_REFLECTION_MEMBER_KIND_METHOD ? 3u : 2u;
        if (argumentCount < 1u || argumentCount > maximumArguments) {
            return descriptor_native_return_null(state, functionBase);
        }
        nameValue = ZrCore_Stack_GetValue(functionBase + 1);
        if (nameValue == ZR_NULL || nameValue->type != ZR_VALUE_TYPE_STRING ||
            nameValue->value.object == ZR_NULL) {
            return descriptor_native_return_null(state, functionBase);
        }
        if (kind == ZR_REFLECTION_MEMBER_KIND_METHOD) {
            parameterTypesValue = argumentCount >= 2u
                                          ? ZrCore_Stack_GetValue(functionBase + 2)
                                          : ZR_NULL;
            queryValue = argumentCount >= 3u
                                 ? ZrCore_Stack_GetValue(functionBase + 3)
                                 : ZR_NULL;
        } else {
            queryValue = argumentCount >= 2u
                                 ? ZrCore_Stack_GetValue(functionBase + 2)
                                 : ZR_NULL;
        }
    } else {
        if (argumentCount > 1u) {
            return descriptor_native_return_null(state, functionBase);
        }
        queryValue = argumentCount == 1u
                             ? ZrCore_Stack_GetValue(functionBase + 1)
                             : ZR_NULL;
    }
    if (!descriptor_native_decode_member_query(state, queryValue, &query) ||
        !descriptor_native_read_parameter_type_ids(
                state, parameterTypesValue, &parameterTypeIds,
                &parameterTypeIdCount)) {
        free(parameterTypeIds);
        return descriptor_native_return_null(state, functionBase);
    }
    if (singular) {
        ZrCore_Reflection_GetMember(
                state,
                descriptor,
                ZR_CAST_STRING(state, nameValue->value.object),
                kind,
                parameterTypeIds,
                parameterTypeIdCount,
                &query,
                &result,
                &status);
    } else {
        ZrCore_Reflection_QueryMembers(
                state, descriptor, kind, &query, &result, &status);
    }
    free(parameterTypeIds);
    return descriptor_native_return_object(
            state,
            functionBase,
            result,
            singular ? ZR_VALUE_TYPE_OBJECT : ZR_VALUE_TYPE_ARRAY);
}

static TZrInt64 descriptor_get_field_native(SZrState *state) {
    return descriptor_native_query(state, ZR_REFLECTION_MEMBER_KIND_FIELD, ZR_TRUE);
}

static TZrInt64 descriptor_get_fields_native(SZrState *state) {
    return descriptor_native_query(state, ZR_REFLECTION_MEMBER_KIND_FIELD, ZR_FALSE);
}

static TZrInt64 descriptor_get_property_native(SZrState *state) {
    return descriptor_native_query(state, ZR_REFLECTION_MEMBER_KIND_PROPERTY, ZR_TRUE);
}

static TZrInt64 descriptor_get_properties_native(SZrState *state) {
    return descriptor_native_query(state, ZR_REFLECTION_MEMBER_KIND_PROPERTY, ZR_FALSE);
}

static TZrInt64 descriptor_get_method_native(SZrState *state) {
    return descriptor_native_query(state, ZR_REFLECTION_MEMBER_KIND_METHOD, ZR_TRUE);
}

static TZrInt64 descriptor_get_methods_native(SZrState *state) {
    return descriptor_native_query(state, ZR_REFLECTION_MEMBER_KIND_METHOD, ZR_FALSE);
}

static TZrInt64 descriptor_get_meta_native(SZrState *state) {
    TZrStackValuePointer functionBase = ZR_NULL;
    SZrObject *descriptor = ZR_NULL;
    TZrSize argumentCount = 0u;
    const SZrTypeValue *nameValue;
    const SZrTypeValue *metadataValue;
    const SZrTypeValue *entry;

    if (!descriptor_native_call_frame(
                state, &functionBase, &descriptor, &argumentCount) ||
        argumentCount < 1u || argumentCount > 2u) {
        return descriptor_native_return_null(state, functionBase);
    }
    nameValue = ZrCore_Stack_GetValue(functionBase + 1);
    if (nameValue == ZR_NULL || nameValue->type != ZR_VALUE_TYPE_STRING ||
        nameValue->value.object == ZR_NULL) {
        return descriptor_native_return_null(state, functionBase);
    }
    metadataValue = ZrCore_Reflection_ObjectGetFieldValue(
            state, descriptor, "metadata");
    if (metadataValue == ZR_NULL || metadataValue->type != ZR_VALUE_TYPE_OBJECT ||
        metadataValue->value.object == ZR_NULL) {
        return descriptor_native_return_null(state, functionBase);
    }
    entry = ZrCore_Reflection_ObjectGetFieldValue(
            state,
            ZR_CAST_OBJECT(state, metadataValue->value.object),
            ZrCore_String_GetNativeString(
                    ZR_CAST_STRING(state, nameValue->value.object)));
    return entry != ZR_NULL && entry->type == ZR_VALUE_TYPE_OBJECT &&
                   entry->value.object != ZR_NULL
           ? descriptor_native_return_object(
                     state,
                     functionBase,
                     ZR_CAST_OBJECT(state, entry->value.object),
                     ZR_VALUE_TYPE_OBJECT)
           : descriptor_native_return_null(state, functionBase);
}

static TZrInt64 descriptor_get_metas_native(SZrState *state) {
    TZrStackValuePointer functionBase = ZR_NULL;
    SZrObject *descriptor = ZR_NULL;
    TZrSize argumentCount = 0u;
    const SZrTypeValue *decoratorsValue;

    if (!descriptor_native_call_frame(
                state, &functionBase, &descriptor, &argumentCount) ||
        argumentCount > 1u) {
        return descriptor_native_return_null(state, functionBase);
    }
    decoratorsValue = ZrCore_Reflection_ObjectGetFieldValue(
            state, descriptor, "decorators");
    return decoratorsValue != ZR_NULL &&
                   decoratorsValue->type == ZR_VALUE_TYPE_ARRAY &&
                   decoratorsValue->value.object != ZR_NULL
           ? descriptor_native_return_object(
                     state,
                     functionBase,
                     ZR_CAST_OBJECT(state, decoratorsValue->value.object),
                     ZR_VALUE_TYPE_ARRAY)
           : descriptor_native_return_null(state, functionBase);
}

static TZrInt64 descriptor_create_instance_native(SZrState *state) {
    TZrStackValuePointer functionBase = ZR_NULL;
    SZrObject *descriptor = ZR_NULL;
    TZrSize argumentCount = 0u;
    SZrTypeValue result;
    EZrReflectionConstructionStatus status;

    if (!descriptor_native_call_frame(
                state, &functionBase, &descriptor, &argumentCount)) {
        return descriptor_native_return_null(state, functionBase);
    }
    ZrCore_Value_ResetAsNull(&result);
    if (!ZrCore_Reflection_CreateInstance(
                state,
                descriptor,
                argumentCount > 0u
                        ? ZrCore_Stack_GetValue(functionBase + 1)
                        : ZR_NULL,
                argumentCount,
                &result,
                &status)) {
        return descriptor_native_return_null(state, functionBase);
    }
    ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(functionBase), &result);
    state->stackTop.valuePointer = functionBase + 1;
    return 1;
}

static TZrBool descriptor_native_attach_method(
        SZrState *state,
        SZrObject *descriptor,
        const TZrChar *name,
        FZrReflectionDescriptorNativeEntry entry) {
    const SZrTypeValue *existing = ZrCore_Reflection_ObjectGetFieldValue(
            state, descriptor, name);
    TZrStackValuePointer rootBase;
    SZrTypeValue *closureRoot;
    SZrTypeValue *descriptorRoot;
    SZrClosureNative *closure;
    SZrClosureValue *captureOwner;
    SZrRawObject **captureOwners;
    TZrBool descriptorPinned = ZR_FALSE;
    TZrBool success;

    if (existing != ZR_NULL) {
        if (existing->type == ZR_VALUE_TYPE_CLOSURE &&
            existing->value.object != ZR_NULL &&
            existing->value.object->type == ZR_RAW_OBJECT_TYPE_CLOSURE &&
            existing->value.object->isNative) {
            closure = ZR_CAST_NATIVE_CLOSURE(state, existing->value.object);
            return (TZrBool) (closure->nativeFunction == entry &&
                              closure->nativeBindingUsesReceiver == ZR_NATIVE_BINDING_RECEIVER_CAPTURED &&
                              closure->closureValueCount == 1u &&
                              ZrCore_ClosureNative_GetCaptureValue(closure, 0u) != ZR_NULL &&
                              ZrCore_ClosureNative_GetCaptureValue(closure, 0u)->type == ZR_VALUE_TYPE_OBJECT &&
                              ZrCore_ClosureNative_GetCaptureValue(closure, 0u)->value.object ==
                                      ZR_CAST_RAW_OBJECT_AS_SUPER(descriptor));
        }
        return ZR_FALSE;
    }

    if (!ZrCore_GarbageCollector_IgnoreObjectIfNeededFast(
                state->global,
                state,
                ZR_CAST_RAW_OBJECT_AS_SUPER(descriptor),
                &descriptorPinned)) {
        return ZR_FALSE;
    }
    rootBase = state->stackTop.valuePointer;
    rootBase = ZrCore_Function_CheckStackAndGc(state, 2u, rootBase);
    closure = ZrCore_ClosureNative_New(state, 1u);
    if (closure == ZR_NULL) {
        if (descriptorPinned) {
            ZrCore_GarbageCollector_UnignoreObject(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(descriptor));
        }
        return ZR_FALSE;
    }
    closure->nativeFunction = entry;
    closure->nativeBindingUsesReceiver = ZR_NATIVE_BINDING_RECEIVER_CAPTURED;
    closureRoot = ZrCore_Stack_GetValue(rootBase);
    ZrCore_Value_InitAsRawObject(state, closureRoot, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    closureRoot->type = ZR_VALUE_TYPE_CLOSURE;
    descriptorRoot = ZrCore_Stack_GetValue(rootBase + 1);
    ZrCore_Value_InitAsRawObject(state, descriptorRoot, ZR_CAST_RAW_OBJECT_AS_SUPER(descriptor));
    state->stackTop.valuePointer = rootBase + 2;

    captureOwner = ZrCore_Closure_FindOrCreateValue(state, rootBase + 1);
    if (captureOwner == ZR_NULL) {
        state->stackTop.valuePointer = rootBase;
        if (descriptorPinned) {
            ZrCore_GarbageCollector_UnignoreObject(
                state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(descriptor));
        }
        return ZR_FALSE;
    }
    closure = ZR_CAST_NATIVE_CLOSURE(state, closureRoot->value.object);
    captureOwners = ZrCore_ClosureNative_GetCaptureOwners(closure);
    closure->closureValuesExtend[0] = ZR_NULL;
    captureOwners[0] = ZR_CAST_RAW_OBJECT_AS_SUPER(captureOwner);
    ZrCore_RawObject_Barrier(state,
                            ZR_CAST_RAW_OBJECT_AS_SUPER(closure),
                            ZR_CAST_RAW_OBJECT_AS_SUPER(captureOwner));
    ZrCore_Closure_CloseStackValue(state, rootBase + 1);

    closure = ZR_CAST_NATIVE_CLOSURE(state, closureRoot->value.object);
    descriptor = ZR_CAST_OBJECT(state, descriptorRoot->value.object);
    success = ZrCore_Reflection_ObjectSetFieldValue(state, descriptor, name, closureRoot);
    state->stackTop.valuePointer = rootBase;
    if (descriptorPinned) {
        ZrCore_GarbageCollector_UnignoreObject(
                state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(descriptor));
    }
    return success;
}

TZrBool ZrCore_Reflection_AttachDescriptorNativeMethodsInternal(
        SZrState *state,
        SZrObject *descriptor,
        EZrReflectionTypeCategory category) {
    static const struct {
        const TZrChar *name;
        FZrReflectionDescriptorNativeEntry entry;
    } kCommonMethods[] = {
            {"getField", descriptor_get_field_native},
            {"getFields", descriptor_get_fields_native},
            {"getProperty", descriptor_get_property_native},
            {"getProperties", descriptor_get_properties_native},
            {"getMethod", descriptor_get_method_native},
            {"getMethods", descriptor_get_methods_native},
            {"getMeta", descriptor_get_meta_native},
            {"getMetas", descriptor_get_metas_native},
    };

    if (state == ZR_NULL || descriptor == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0u; index < ZR_ARRAY_COUNT(kCommonMethods); index++) {
        if (!descriptor_native_attach_method(
                    state,
                    descriptor,
                    kCommonMethods[index].name,
                    kCommonMethods[index].entry)) {
            return ZR_FALSE;
        }
    }
    if (category == ZR_REFLECTION_TYPE_CATEGORY_CONCRETE_CLASS ||
        category == ZR_REFLECTION_TYPE_CATEGORY_INSTANCE_CLASS ||
        category == ZR_REFLECTION_TYPE_CATEGORY_STRUCT) {
        return descriptor_native_attach_method(
                state, descriptor, "createInstance",
                descriptor_create_instance_native);
    }
    return ZR_TRUE;
}
