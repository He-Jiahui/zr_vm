//
// Runtime decorator definition-phase helpers.
//

#include "zr_vm_core/runtime_decorator.h"

#include "zr_vm_core/closure.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/reflection.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#include <string.h>

static const TZrChar *kRuntimeDecoratorMetadataFieldName = "__zr_runtime_decorator_metadata";
static const TZrChar *kRuntimeDecoratorDecoratorsFieldName = "__zr_runtime_decorator_decorators";
static const TZrChar *kRuntimeDecoratorMemberMetadataFieldName = "__zr_runtime_decorator_member_metadata";
static const TZrChar *kRuntimeDecoratorMemberDecoratorsFieldName = "__zr_runtime_decorator_member_decorators";

static SZrObject *runtime_decorator_new_plain_object(SZrState *state);

static const TZrChar *runtime_decorator_target_kind_name(EZrRuntimeDecoratorTargetKind targetKind) {
    switch (targetKind) {
        case ZR_RUNTIME_DECORATOR_TARGET_KIND_FIELD:
            return "field";
        case ZR_RUNTIME_DECORATOR_TARGET_KIND_METHOD:
            return "method";
        case ZR_RUNTIME_DECORATOR_TARGET_KIND_PROPERTY:
            return "property";
        case ZR_RUNTIME_DECORATOR_TARGET_KIND_INVALID:
        default:
            return ZR_NULL;
    }
}

static SZrString *runtime_decorator_make_string(SZrState *state, const TZrChar *text) {
    if (state == ZR_NULL || text == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(state, (TZrNativeString)text, strlen(text));
}

static TZrBool runtime_decorator_make_string_key(SZrState *state, const TZrChar *name, SZrTypeValue *outKey) {
    SZrString *nameObject;

    if (state == ZR_NULL || name == ZR_NULL || outKey == ZR_NULL) {
        return ZR_FALSE;
    }

    nameObject = runtime_decorator_make_string(state, name);
    if (nameObject == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(state, outKey, ZR_CAST_RAW_OBJECT_AS_SUPER(nameObject));
    outKey->type = ZR_VALUE_TYPE_STRING;
    return ZR_TRUE;
}

static const SZrTypeValue *runtime_decorator_get_object_field(SZrState *state,
                                                              SZrObject *object,
                                                              const TZrChar *fieldName) {
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL ||
        !runtime_decorator_make_string_key(state, fieldName, &key)) {
        return ZR_NULL;
    }

    return ZrCore_Object_GetValue(state, object, &key);
}

static SZrObject *runtime_decorator_get_object_field_as_object(SZrState *state,
                                                               SZrObject *object,
                                                               const TZrChar *fieldName,
                                                               EZrValueType expectedType) {
    const SZrTypeValue *value = runtime_decorator_get_object_field(state, object, fieldName);

    if (value == ZR_NULL || value->type != expectedType || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, value->value.object);
}

static void runtime_decorator_set_field_value(SZrState *state,
                                              SZrObject *object,
                                              const TZrChar *fieldName,
                                              const SZrTypeValue *fieldValue) {
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || fieldValue == ZR_NULL ||
        !runtime_decorator_make_string_key(state, fieldName, &key)) {
        return;
    }

    ZrCore_Object_SetValue(state, object, &key, fieldValue);
}

static void runtime_decorator_set_object_field(SZrState *state,
                                               SZrObject *object,
                                               const TZrChar *fieldName,
                                               SZrObject *fieldObject,
                                               EZrValueType valueType) {
    SZrTypeValue value;

    if (state == ZR_NULL || fieldObject == ZR_NULL) {
        return;
    }

    ZrCore_Value_InitAsRawObject(state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldObject));
    value.type = valueType;
    runtime_decorator_set_field_value(state, object, fieldName, &value);
}

static void runtime_decorator_set_string_field(SZrState *state,
                                               SZrObject *object,
                                               const TZrChar *fieldName,
                                               const TZrChar *text) {
    SZrString *stringObject;
    SZrTypeValue value;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || text == ZR_NULL) {
        return;
    }

    stringObject = runtime_decorator_make_string(state, text);
    if (stringObject == ZR_NULL) {
        return;
    }

    ZrCore_Value_InitAsRawObject(state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(stringObject));
    value.type = ZR_VALUE_TYPE_STRING;
    runtime_decorator_set_field_value(state, object, fieldName, &value);
}

static void runtime_decorator_set_bool_field(SZrState *state,
                                             SZrObject *object,
                                             const TZrChar *fieldName,
                                             TZrBool fieldValue) {
    SZrTypeValue value;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }

    ZrCore_Value_InitAsBool(state, &value, fieldValue ? ZR_TRUE : ZR_FALSE);
    runtime_decorator_set_field_value(state, object, fieldName, &value);
}

static SZrString *runtime_decorator_make_member_storage_key(SZrState *state,
                                                            SZrString *memberName,
                                                            EZrRuntimeDecoratorTargetKind targetKind) {
    const TZrChar *kindName;
    const TZrChar *memberNameText;
    TZrSize kindLength;
    TZrSize memberLength;
    TZrSize bufferLength;
    TZrChar *buffer;
    SZrString *result;

    if (state == ZR_NULL || memberName == ZR_NULL) {
        return ZR_NULL;
    }

    kindName = runtime_decorator_target_kind_name(targetKind);
    memberNameText = ZrCore_String_GetNativeString(memberName);
    if (kindName == ZR_NULL || memberNameText == ZR_NULL) {
        return ZR_NULL;
    }

    kindLength = strlen(kindName);
    memberLength = strlen(memberNameText);
    bufferLength = kindLength + 1 + memberLength;
    buffer = (TZrChar *)ZrCore_Memory_RawMallocWithType(state->global,
                                                        bufferLength + 1,
                                                        ZR_MEMORY_NATIVE_TYPE_OBJECT);
    if (buffer == ZR_NULL) {
        return ZR_NULL;
    }

    memcpy(buffer, kindName, kindLength);
    buffer[kindLength] = ':';
    memcpy(buffer + kindLength + 1, memberNameText, memberLength);
    buffer[bufferLength] = '\0';

    result = ZrCore_String_Create(state, buffer, bufferLength);
    ZrCore_Memory_RawFreeWithType(state->global,
                                  buffer,
                                  bufferLength + 1,
                                  ZR_MEMORY_NATIVE_TYPE_OBJECT);
    return result;
}

static SZrObject *runtime_decorator_get_member_overlay_map(SZrState *state,
                                                           SZrObjectPrototype *prototype,
                                                           const TZrChar *fieldName,
                                                           TZrBool createIfMissing) {
    SZrObject *mapObject;

    if (state == ZR_NULL || prototype == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    mapObject = runtime_decorator_get_object_field_as_object(state, &prototype->super, fieldName, ZR_VALUE_TYPE_OBJECT);
    if (mapObject != ZR_NULL || !createIfMissing) {
        return mapObject;
    }

    mapObject = runtime_decorator_new_plain_object(state);
    if (mapObject != ZR_NULL) {
        runtime_decorator_set_object_field(state,
                                           &prototype->super,
                                           fieldName,
                                           mapObject,
                                           ZR_VALUE_TYPE_OBJECT);
    }
    return mapObject;
}

static SZrObject *runtime_decorator_get_member_overlay_entry(SZrState *state,
                                                             SZrObjectPrototype *prototype,
                                                             const TZrChar *fieldName,
                                                             SZrString *memberName,
                                                             EZrRuntimeDecoratorTargetKind targetKind,
                                                             EZrValueType expectedType) {
    SZrObject *mapObject;
    SZrString *storageKey;
    const SZrTypeValue *entryValue;
    SZrTypeValue key;

    if (state == ZR_NULL || prototype == ZR_NULL || fieldName == ZR_NULL || memberName == ZR_NULL) {
        return ZR_NULL;
    }

    mapObject = runtime_decorator_get_member_overlay_map(state, prototype, fieldName, ZR_FALSE);
    if (mapObject == ZR_NULL) {
        return ZR_NULL;
    }

    storageKey = runtime_decorator_make_member_storage_key(state, memberName, targetKind);
    if (storageKey == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(storageKey));
    key.type = ZR_VALUE_TYPE_STRING;
    entryValue = ZrCore_Object_GetValue(state, mapObject, &key);
    if (entryValue == ZR_NULL || entryValue->type != expectedType || entryValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, entryValue->value.object);
}

static void runtime_decorator_set_member_overlay_entry(SZrState *state,
                                                       SZrObjectPrototype *prototype,
                                                       const TZrChar *fieldName,
                                                       SZrString *memberName,
                                                       EZrRuntimeDecoratorTargetKind targetKind,
                                                       SZrObject *entryObject,
                                                       EZrValueType entryType) {
    SZrObject *mapObject;
    SZrString *storageKey;
    SZrTypeValue key;
    SZrTypeValue value;

    if (state == ZR_NULL || prototype == ZR_NULL || fieldName == ZR_NULL || memberName == ZR_NULL ||
        entryObject == ZR_NULL) {
        return;
    }

    mapObject = runtime_decorator_get_member_overlay_map(state, prototype, fieldName, ZR_TRUE);
    storageKey = runtime_decorator_make_member_storage_key(state, memberName, targetKind);
    if (mapObject == ZR_NULL || storageKey == ZR_NULL) {
        return;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(storageKey));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsRawObject(state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(entryObject));
    value.type = entryType;
    ZrCore_Object_SetValue(state, mapObject, &key, &value);
}

static TZrUInt32 runtime_decorator_array_length(SZrObject *array) {
    if (array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return 0;
    }

    return (TZrUInt32)array->nodeMap.elementCount;
}

static TZrBool runtime_decorator_array_push(SZrState *state, SZrObject *array, const SZrTypeValue *value) {
    SZrTypeValue key;

    if (state == ZR_NULL || array == ZR_NULL || value == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsInt(state, &key, (TZrInt64)runtime_decorator_array_length(array));
    ZrCore_Object_SetValue(state, array, &key, value);
    return ZR_TRUE;
}

static SZrObject *runtime_decorator_find_member_reflection_entry(SZrState *state,
                                                                 SZrObject *entriesArray,
                                                                 EZrRuntimeDecoratorTargetKind targetKind) {
    const TZrChar *expectedKindName;

    if (state == ZR_NULL || entriesArray == ZR_NULL) {
        return ZR_NULL;
    }

    expectedKindName = runtime_decorator_target_kind_name(targetKind);
    if (expectedKindName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < runtime_decorator_array_length(entriesArray); index++) {
        SZrTypeValue key;
        const SZrTypeValue *entryValue;
        SZrObject *entryObject;
        const SZrTypeValue *kindValue;

        ZrCore_Value_InitAsInt(state, &key, (TZrInt64)index);
        entryValue = ZrCore_Object_GetValue(state, entriesArray, &key);
        if (entryValue == ZR_NULL || entryValue->type != ZR_VALUE_TYPE_OBJECT || entryValue->value.object == ZR_NULL) {
            continue;
        }

        entryObject = ZR_CAST_OBJECT(state, entryValue->value.object);
        kindValue = runtime_decorator_get_object_field(state, entryObject, "kind");
        if (kindValue != ZR_NULL &&
            kindValue->type == ZR_VALUE_TYPE_STRING &&
            kindValue->value.object != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(ZR_CAST_STRING(state, kindValue->value.object)), expectedKindName) == 0) {
            return entryObject;
        }
    }

    return ZR_NULL;
}

static SZrObject *runtime_decorator_new_plain_object(SZrState *state) {
    SZrObject *object;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZrCore_Object_New(state, ZR_NULL);
    if (object != ZR_NULL) {
        ZrCore_Object_Init(state, object);
    }
    return object;
}

static const TZrChar *runtime_decorator_name_from_value(SZrState *state, const SZrTypeValue *decoratorValue) {
    SZrFunction *function;
    SZrObject *object;
    SZrObjectPrototype *prototype;

    if (state == ZR_NULL || decoratorValue == ZR_NULL || decoratorValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    function = ZrCore_Closure_GetMetadataFunctionFromValue(state, decoratorValue);
    if (function != ZR_NULL && function->functionName != ZR_NULL) {
        return ZrCore_String_GetNativeString(function->functionName);
    }

    if (decoratorValue->type != ZR_VALUE_TYPE_OBJECT && decoratorValue->type != ZR_VALUE_TYPE_ARRAY) {
        return ZR_NULL;
    }

    object = ZR_CAST_OBJECT(state, decoratorValue->value.object);
    if (object == ZR_NULL) {
        return ZR_NULL;
    }

    if (object->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE) {
        prototype = (SZrObjectPrototype *)object;
        return prototype->name != ZR_NULL ? ZrCore_String_GetNativeString(prototype->name) : ZR_NULL;
    }

    prototype = object->prototype;
    return (prototype != ZR_NULL && prototype->name != ZR_NULL) ? ZrCore_String_GetNativeString(prototype->name) : ZR_NULL;
}

static void runtime_decorator_append_record(SZrState *state,
                                            SZrObject *reflectionObject,
                                            const SZrTypeValue *decoratorValue) {
    SZrObject *decoratorsArray;
    SZrObject *entryObject;
    const TZrChar *decoratorName;
    SZrString *decoratorNameString;
    SZrTypeValue decoratorNameValue;
    SZrTypeValue entryValue;

    if (state == ZR_NULL || reflectionObject == ZR_NULL || decoratorValue == ZR_NULL) {
        return;
    }

    decoratorsArray = runtime_decorator_get_object_field_as_object(state, reflectionObject, "decorators", ZR_VALUE_TYPE_ARRAY);
    if (decoratorsArray == ZR_NULL) {
        return;
    }

    decoratorName = runtime_decorator_name_from_value(state, decoratorValue);
    if (decoratorName == ZR_NULL || decoratorName[0] == '\0') {
        decoratorName = "decorator";
    }

    entryObject = runtime_decorator_new_plain_object(state);
    decoratorNameString = runtime_decorator_make_string(state, decoratorName);
    if (entryObject == ZR_NULL || decoratorNameString == ZR_NULL) {
        return;
    }

    ZrCore_Value_InitAsRawObject(state, &decoratorNameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(decoratorNameString));
    decoratorNameValue.type = ZR_VALUE_TYPE_STRING;
    runtime_decorator_set_field_value(state, entryObject, "name", &decoratorNameValue);

    ZrCore_Value_InitAsRawObject(state, &entryValue, ZR_CAST_RAW_OBJECT_AS_SUPER(entryObject));
    entryValue.type = ZR_VALUE_TYPE_OBJECT;
    runtime_decorator_array_push(state, decoratorsArray, &entryValue);
}

static TZrBool runtime_decorator_call_function(SZrState *state,
                                               const SZrTypeValue *callableValue,
                                               const SZrTypeValue *argumentValue,
                                               SZrTypeValue *outResult) {
    SZrCallInfo *savedCallInfo;
    TZrStackValuePointer savedStackTop;
    TZrStackValuePointer base;
    SZrFunctionStackAnchor savedStackTopAnchor;
    SZrTypeValue copiedResult;

    if (state == ZR_NULL || callableValue == ZR_NULL || argumentValue == ZR_NULL) {
        return ZR_FALSE;
    }

    savedStackTop = state->stackTop.valuePointer;
    savedCallInfo = state->callInfoList;
    base = savedCallInfo != ZR_NULL ? savedCallInfo->functionTop.valuePointer : savedStackTop;

    ZrCore_Function_StackAnchorInit(state, savedStackTop, &savedStackTopAnchor);
    base = ZrCore_Function_ReserveScratchSlots(state, 2, base);
    if (base == ZR_NULL) {
        return ZR_FALSE;
    }
    if (savedCallInfo != ZR_NULL) {
        base = savedCallInfo->functionTop.valuePointer;
    }

    ZrCore_Stack_CopyValue(state, base, callableValue);
    ZrCore_Stack_CopyValue(state, base + 1, argumentValue);
    state->stackTop.valuePointer = base + 2;
    if (savedCallInfo != ZR_NULL && savedCallInfo->functionTop.valuePointer < state->stackTop.valuePointer) {
        savedCallInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    }

    base = ZrCore_Function_CallWithoutYieldAndRestore(state, base, 1);
    savedStackTop = ZrCore_Function_StackAnchorRestore(state, &savedStackTopAnchor);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        state->stackTop.valuePointer = savedStackTop;
        state->callInfoList = savedCallInfo;
        return ZR_FALSE;
    }

    if (outResult != ZR_NULL) {
        ZrCore_Value_ResetAsNull(&copiedResult);
        ZrCore_Value_Copy(state, &copiedResult, ZrCore_Stack_GetValue(base));
        *outResult = copiedResult;
    }

    state->stackTop.valuePointer = savedStackTop;
    state->callInfoList = savedCallInfo;
    return ZR_TRUE;
}

static TZrBool runtime_decorator_call_meta_function(SZrState *state,
                                                    SZrFunction *function,
                                                    const SZrTypeValue *selfValue,
                                                    const SZrTypeValue *argumentValue,
                                                    SZrTypeValue *outResult) {
    SZrCallInfo *savedCallInfo;
    TZrStackValuePointer savedStackTop;
    TZrStackValuePointer base;
    SZrFunctionStackAnchor savedStackTopAnchor;
    SZrTypeValue copiedResult;

    if (state == ZR_NULL || function == ZR_NULL || selfValue == ZR_NULL || argumentValue == ZR_NULL) {
        return ZR_FALSE;
    }

    savedStackTop = state->stackTop.valuePointer;
    savedCallInfo = state->callInfoList;
    base = savedCallInfo != ZR_NULL ? savedCallInfo->functionTop.valuePointer : savedStackTop;

    ZrCore_Function_StackAnchorInit(state, savedStackTop, &savedStackTopAnchor);
    base = ZrCore_Function_ReserveScratchSlots(state, 3, base);
    if (base == ZR_NULL) {
        return ZR_FALSE;
    }
    if (savedCallInfo != ZR_NULL) {
        base = savedCallInfo->functionTop.valuePointer;
    }

    ZrCore_Stack_SetRawObjectValue(state, base, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    ZrCore_Stack_CopyValue(state, base + 1, selfValue);
    ZrCore_Stack_CopyValue(state, base + 2, argumentValue);
    state->stackTop.valuePointer = base + 3;
    if (savedCallInfo != ZR_NULL && savedCallInfo->functionTop.valuePointer < state->stackTop.valuePointer) {
        savedCallInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    }

    base = ZrCore_Function_CallWithoutYieldAndRestore(state, base, 1);
    savedStackTop = ZrCore_Function_StackAnchorRestore(state, &savedStackTopAnchor);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        state->stackTop.valuePointer = savedStackTop;
        state->callInfoList = savedCallInfo;
        return ZR_FALSE;
    }

    if (outResult != ZR_NULL) {
        ZrCore_Value_ResetAsNull(&copiedResult);
        ZrCore_Value_Copy(state, &copiedResult, ZrCore_Stack_GetValue(base));
        *outResult = copiedResult;
    }

    state->stackTop.valuePointer = savedStackTop;
    state->callInfoList = savedCallInfo;
    return ZR_TRUE;
}

static TZrBool runtime_decorator_invoke_object_decorator(SZrState *state,
                                                         const SZrTypeValue *decoratorValue,
                                                         const SZrTypeValue *reflectionValue) {
    SZrObject *decoratorObject;
    SZrObjectPrototype *prototype = ZR_NULL;
    SZrFunction *decorateFunction = ZR_NULL;

    if (state == ZR_NULL || decoratorValue == ZR_NULL || reflectionValue == ZR_NULL ||
        decoratorValue->type != ZR_VALUE_TYPE_OBJECT || decoratorValue->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    decoratorObject = ZR_CAST_OBJECT(state, decoratorValue->value.object);
    if (decoratorObject == ZR_NULL) {
        return ZR_FALSE;
    }

    if (decoratorObject->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE) {
        prototype = (SZrObjectPrototype *)decoratorObject;
    } else {
        prototype = decoratorObject->prototype;
    }

    if (prototype == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrCore_Prototype_GetMetaRecursively(state->global, prototype, ZR_META_DECORATE) != ZR_NULL) {
        decorateFunction = ZrCore_Prototype_GetMetaRecursively(state->global, prototype, ZR_META_DECORATE)->function;
    }
    if (decorateFunction == ZR_NULL) {
        return ZR_FALSE;
    }

    return runtime_decorator_call_meta_function(state, decorateFunction, decoratorValue, reflectionValue, ZR_NULL);
}

static void runtime_decorator_commit_prototype_overlay(SZrState *state,
                                                       SZrObjectPrototype *prototype,
                                                       SZrObject *metadataObject,
                                                       SZrObject *decoratorsArray) {
    if (state == ZR_NULL || prototype == ZR_NULL) {
        return;
    }

    if (metadataObject != ZR_NULL) {
        runtime_decorator_set_object_field(state,
                                           &prototype->super,
                                           kRuntimeDecoratorMetadataFieldName,
                                           metadataObject,
                                           ZR_VALUE_TYPE_OBJECT);
    }
    if (decoratorsArray != ZR_NULL) {
        runtime_decorator_set_object_field(state,
                                           &prototype->super,
                                           kRuntimeDecoratorDecoratorsFieldName,
                                           decoratorsArray,
                                           ZR_VALUE_TYPE_ARRAY);
    }
}

static void runtime_decorator_commit_function_overlay(SZrState *state,
                                                      SZrFunction *function,
                                                      SZrObject *metadataObject,
                                                      SZrObject *decoratorsArray) {
    if (state == ZR_NULL || function == ZR_NULL) {
        return;
    }

    function->runtimeDecoratorMetadata = metadataObject;
    function->runtimeDecoratorDecorators = decoratorsArray;
    if (metadataObject != ZR_NULL) {
        ZrCore_RawObject_Barrier(state,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(function),
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(metadataObject));
    }
    if (decoratorsArray != ZR_NULL) {
        ZrCore_RawObject_Barrier(state,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(function),
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(decoratorsArray));
    }
}

static void runtime_decorator_commit_member_overlay(SZrState *state,
                                                    SZrObjectPrototype *prototype,
                                                    SZrString *memberName,
                                                    EZrRuntimeDecoratorTargetKind targetKind,
                                                    SZrObject *metadataObject,
                                                    SZrObject *decoratorsArray) {
    if (state == ZR_NULL || prototype == ZR_NULL || memberName == ZR_NULL) {
        return;
    }

    if (metadataObject != ZR_NULL) {
        runtime_decorator_set_member_overlay_entry(state,
                                                   prototype,
                                                   kRuntimeDecoratorMemberMetadataFieldName,
                                                   memberName,
                                                   targetKind,
                                                   metadataObject,
                                                   ZR_VALUE_TYPE_OBJECT);
    }
    if (decoratorsArray != ZR_NULL) {
        runtime_decorator_set_member_overlay_entry(state,
                                                   prototype,
                                                   kRuntimeDecoratorMemberDecoratorsFieldName,
                                                   memberName,
                                                   targetKind,
                                                   decoratorsArray,
                                                   ZR_VALUE_TYPE_ARRAY);
    }
}

ZR_CORE_API void ZrCore_RuntimeDecorator_OverlayTypeReflection(SZrState *state,
                                                               SZrObject *typeReflection,
                                                               SZrObjectPrototype *prototype) {
    const SZrTypeValue *metadataValue;
    const SZrTypeValue *decoratorsValue;

    if (state == ZR_NULL || typeReflection == ZR_NULL || prototype == ZR_NULL) {
        return;
    }

    metadataValue = runtime_decorator_get_object_field(state, &prototype->super, kRuntimeDecoratorMetadataFieldName);
    if (metadataValue != ZR_NULL && metadataValue->type == ZR_VALUE_TYPE_OBJECT && metadataValue->value.object != ZR_NULL) {
        runtime_decorator_set_object_field(state,
                                           typeReflection,
                                           "metadata",
                                           ZR_CAST_OBJECT(state, metadataValue->value.object),
                                           ZR_VALUE_TYPE_OBJECT);
    }

    decoratorsValue = runtime_decorator_get_object_field(state, &prototype->super, kRuntimeDecoratorDecoratorsFieldName);
    if (decoratorsValue != ZR_NULL && decoratorsValue->type == ZR_VALUE_TYPE_ARRAY && decoratorsValue->value.object != ZR_NULL) {
        runtime_decorator_set_object_field(state,
                                           typeReflection,
                                           "decorators",
                                           ZR_CAST_OBJECT(state, decoratorsValue->value.object),
                                           ZR_VALUE_TYPE_ARRAY);
    }
}

ZR_CORE_API void ZrCore_RuntimeDecorator_OverlayFunctionReflection(SZrState *state,
                                                                   SZrObject *reflectionObject,
                                                                   SZrFunction *function) {
    if (state == ZR_NULL || reflectionObject == ZR_NULL || function == ZR_NULL) {
        return;
    }

    if (function->runtimeDecoratorMetadata != ZR_NULL) {
        runtime_decorator_set_object_field(state,
                                           reflectionObject,
                                           "metadata",
                                           function->runtimeDecoratorMetadata,
                                           ZR_VALUE_TYPE_OBJECT);
    }

    if (function->runtimeDecoratorDecorators != ZR_NULL) {
        runtime_decorator_set_object_field(state,
                                           reflectionObject,
                                           "decorators",
                                           function->runtimeDecoratorDecorators,
                                           ZR_VALUE_TYPE_ARRAY);
    }
}

ZR_CORE_API void ZrCore_RuntimeDecorator_OverlayMemberReflection(SZrState *state,
                                                                 SZrObject *reflectionObject,
                                                                 SZrObjectPrototype *prototype,
                                                                 SZrString *memberName,
                                                                 EZrRuntimeDecoratorTargetKind targetKind) {
    SZrObject *metadataObject;
    SZrObject *decoratorsArray;

    if (state == ZR_NULL || reflectionObject == ZR_NULL || prototype == ZR_NULL || memberName == ZR_NULL ||
        targetKind == ZR_RUNTIME_DECORATOR_TARGET_KIND_INVALID) {
        return;
    }

    metadataObject = runtime_decorator_get_member_overlay_entry(state,
                                                                prototype,
                                                                kRuntimeDecoratorMemberMetadataFieldName,
                                                                memberName,
                                                                targetKind,
                                                                ZR_VALUE_TYPE_OBJECT);
    decoratorsArray = runtime_decorator_get_member_overlay_entry(state,
                                                                 prototype,
                                                                 kRuntimeDecoratorMemberDecoratorsFieldName,
                                                                 memberName,
                                                                 targetKind,
                                                                 ZR_VALUE_TYPE_ARRAY);

    if (metadataObject != ZR_NULL) {
        runtime_decorator_set_object_field(state,
                                           reflectionObject,
                                           "metadata",
                                           metadataObject,
                                           ZR_VALUE_TYPE_OBJECT);
    }
    if (decoratorsArray != ZR_NULL) {
        runtime_decorator_set_object_field(state,
                                           reflectionObject,
                                           "decorators",
                                           decoratorsArray,
                                           ZR_VALUE_TYPE_ARRAY);
    }
}

static SZrObject *runtime_decorator_build_member_reflection(SZrState *state,
                                                            const SZrTypeValue *prototypeValue,
                                                            SZrString *memberName,
                                                            EZrRuntimeDecoratorTargetKind targetKind) {
    if (state == ZR_NULL || prototypeValue == ZR_NULL || memberName == ZR_NULL ||
        targetKind == ZR_RUNTIME_DECORATOR_TARGET_KIND_INVALID) {
        return ZR_NULL;
    }

    if (prototypeValue->type != ZR_VALUE_TYPE_OBJECT || prototypeValue->value.object == ZR_NULL ||
        ZR_CAST_OBJECT(state, prototypeValue->value.object)->internalType != ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE) {
        return ZR_NULL;
    }

    return ZrCore_Reflection_BuildDecoratorTargetMemberReflection(
            state,
            (SZrObjectPrototype *)ZR_CAST_OBJECT(state, prototypeValue->value.object),
            memberName,
            (TZrUInt32)targetKind);
}

static TZrBool runtime_decorator_restore_apply_frame(SZrState *state,
                                                     const SZrFunctionStackAnchor *functionBaseAnchor,
                                                     TZrStackValuePointer *outFunctionBase,
                                                     TZrStackValuePointer *outArgBase,
                                                     SZrTypeValue **outResult,
                                                     SZrTypeValue **outTargetValue,
                                                     SZrTypeValue **outDecoratorValue) {
    TZrStackValuePointer functionBase;
    TZrStackValuePointer argBase;

    if (outFunctionBase != ZR_NULL) {
        *outFunctionBase = ZR_NULL;
    }
    if (outArgBase != ZR_NULL) {
        *outArgBase = ZR_NULL;
    }
    if (outResult != ZR_NULL) {
        *outResult = ZR_NULL;
    }
    if (outTargetValue != ZR_NULL) {
        *outTargetValue = ZR_NULL;
    }
    if (outDecoratorValue != ZR_NULL) {
        *outDecoratorValue = ZR_NULL;
    }
    if (state == ZR_NULL || functionBaseAnchor == ZR_NULL) {
        return ZR_FALSE;
    }

    functionBase = ZrCore_Function_StackAnchorRestore(state, functionBaseAnchor);
    if (functionBase == ZR_NULL) {
        return ZR_FALSE;
    }

    argBase = functionBase + 1;
    if (outFunctionBase != ZR_NULL) {
        *outFunctionBase = functionBase;
    }
    if (outArgBase != ZR_NULL) {
        *outArgBase = argBase;
    }
    if (outResult != ZR_NULL) {
        *outResult = ZrCore_Stack_GetValue(functionBase);
    }
    if (outTargetValue != ZR_NULL) {
        *outTargetValue = ZrCore_Stack_GetValue(argBase);
    }
    if (outDecoratorValue != ZR_NULL) {
        *outDecoratorValue = ZrCore_Stack_GetValue(argBase + 1);
    }
    return ZR_TRUE;
}

static TZrBool runtime_decorator_restore_apply_member_frame(SZrState *state,
                                                            const SZrFunctionStackAnchor *functionBaseAnchor,
                                                            TZrStackValuePointer *outFunctionBase,
                                                            TZrStackValuePointer *outArgBase,
                                                            SZrTypeValue **outResult,
                                                            SZrTypeValue **outPrototypeValue,
                                                            SZrTypeValue **outMemberNameValue,
                                                            SZrTypeValue **outKindValue,
                                                            SZrTypeValue **outDecoratorValue) {
    TZrStackValuePointer functionBase;
    TZrStackValuePointer argBase;

    if (outFunctionBase != ZR_NULL) {
        *outFunctionBase = ZR_NULL;
    }
    if (outArgBase != ZR_NULL) {
        *outArgBase = ZR_NULL;
    }
    if (outResult != ZR_NULL) {
        *outResult = ZR_NULL;
    }
    if (outPrototypeValue != ZR_NULL) {
        *outPrototypeValue = ZR_NULL;
    }
    if (outMemberNameValue != ZR_NULL) {
        *outMemberNameValue = ZR_NULL;
    }
    if (outKindValue != ZR_NULL) {
        *outKindValue = ZR_NULL;
    }
    if (outDecoratorValue != ZR_NULL) {
        *outDecoratorValue = ZR_NULL;
    }
    if (state == ZR_NULL || functionBaseAnchor == ZR_NULL) {
        return ZR_FALSE;
    }

    functionBase = ZrCore_Function_StackAnchorRestore(state, functionBaseAnchor);
    if (functionBase == ZR_NULL) {
        return ZR_FALSE;
    }

    argBase = functionBase + 1;
    if (outFunctionBase != ZR_NULL) {
        *outFunctionBase = functionBase;
    }
    if (outArgBase != ZR_NULL) {
        *outArgBase = argBase;
    }
    if (outResult != ZR_NULL) {
        *outResult = ZrCore_Stack_GetValue(functionBase);
    }
    if (outPrototypeValue != ZR_NULL) {
        *outPrototypeValue = ZrCore_Stack_GetValue(argBase);
    }
    if (outMemberNameValue != ZR_NULL) {
        *outMemberNameValue = ZrCore_Stack_GetValue(argBase + 1);
    }
    if (outKindValue != ZR_NULL) {
        *outKindValue = ZrCore_Stack_GetValue(argBase + 2);
    }
    if (outDecoratorValue != ZR_NULL) {
        *outDecoratorValue = ZrCore_Stack_GetValue(argBase + 3);
    }
    return ZR_TRUE;
}

ZR_CORE_API TZrInt64 ZrCore_RuntimeDecorator_ApplyNativeEntry(SZrState *state) {
    TZrStackValuePointer functionBase;
    TZrStackValuePointer argBase;
    SZrFunctionStackAnchor functionBaseAnchor;
    SZrTypeValue *result;
    SZrTypeValue *targetValue;
    SZrTypeValue *decoratorValue;
    SZrTypeValue reflectionValue;
    SZrObject *reflectionObject;
    SZrObject *metadataObject;
    SZrObject *decoratorsArray;
    SZrFunction *targetFunction;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    functionBase = state->callInfoList->functionBase.valuePointer;
    ZrCore_Function_StackAnchorInit(state, functionBase, &functionBaseAnchor);
    if (!runtime_decorator_restore_apply_frame(state,
                                               &functionBaseAnchor,
                                               &functionBase,
                                               &argBase,
                                               &result,
                                               &targetValue,
                                               &decoratorValue)) {
        return 0;
    }

    if (result != ZR_NULL && targetValue != ZR_NULL) {
        ZrCore_Value_Copy(state, result, targetValue);
    } else if (result != ZR_NULL) {
        ZrCore_Value_ResetAsNull(result);
    }

    if (targetValue == ZR_NULL || decoratorValue == ZR_NULL) {
        state->stackTop.valuePointer = functionBase + 1;
        return 1;
    }

    if (!ZrCore_Reflection_TypeOfValue(state, targetValue, &reflectionValue) ||
        reflectionValue.type != ZR_VALUE_TYPE_OBJECT || reflectionValue.value.object == ZR_NULL) {
        if (!runtime_decorator_restore_apply_frame(state,
                                                   &functionBaseAnchor,
                                                   &functionBase,
                                                   &argBase,
                                                   &result,
                                                   &targetValue,
                                                   &decoratorValue)) {
            return 0;
        }
        state->stackTop.valuePointer = functionBase + 1;
        return 1;
    }

    reflectionObject = ZR_CAST_OBJECT(state, reflectionValue.value.object);
    runtime_decorator_append_record(state, reflectionObject, decoratorValue);
    if (!runtime_decorator_restore_apply_frame(state,
                                               &functionBaseAnchor,
                                               &functionBase,
                                               &argBase,
                                               &result,
                                               &targetValue,
                                               &decoratorValue)) {
        return 0;
    }

    if (decoratorValue->type == ZR_VALUE_TYPE_FUNCTION ||
        decoratorValue->type == ZR_VALUE_TYPE_CLOSURE ||
        decoratorValue->type == ZR_VALUE_TYPE_NATIVE_POINTER) {
        if (!runtime_decorator_call_function(state, decoratorValue, &reflectionValue, ZR_NULL) &&
            state->threadStatus == ZR_THREAD_STATUS_FINE) {
            ZrCore_Debug_RunError(state, "runtime decorator function call failed");
        }
    } else if (decoratorValue->type == ZR_VALUE_TYPE_OBJECT) {
        if (!runtime_decorator_invoke_object_decorator(state, decoratorValue, &reflectionValue) &&
            state->threadStatus == ZR_THREAD_STATUS_FINE) {
            ZrCore_Debug_RunError(state, "runtime decorator object must define @decorate");
        }
    } else {
        ZrCore_Debug_RunError(state, "runtime decorator target must be a callable or decorator object");
    }

    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        return 1;
    }

    if (!runtime_decorator_restore_apply_frame(state,
                                               &functionBaseAnchor,
                                               &functionBase,
                                               &argBase,
                                               &result,
                                               &targetValue,
                                               &decoratorValue)) {
        return 0;
    }
    metadataObject = runtime_decorator_get_object_field_as_object(state, reflectionObject, "metadata", ZR_VALUE_TYPE_OBJECT);
    decoratorsArray = runtime_decorator_get_object_field_as_object(state, reflectionObject, "decorators", ZR_VALUE_TYPE_ARRAY);

    if (targetValue->type == ZR_VALUE_TYPE_OBJECT && targetValue->value.object != ZR_NULL) {
        SZrObject *targetObject = ZR_CAST_OBJECT(state, targetValue->value.object);
        if (targetObject != ZR_NULL && targetObject->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE) {
            runtime_decorator_commit_prototype_overlay(state,
                                                       (SZrObjectPrototype *)targetObject,
                                                       metadataObject,
                                                       decoratorsArray);
        }
    }

    targetFunction = ZrCore_Closure_GetMetadataFunctionFromValue(state, targetValue);
    if (targetFunction != ZR_NULL) {
        runtime_decorator_commit_function_overlay(state, targetFunction, metadataObject, decoratorsArray);
    }

    if (!runtime_decorator_restore_apply_frame(state,
                                               &functionBaseAnchor,
                                               &functionBase,
                                               &argBase,
                                               &result,
                                               &targetValue,
                                               &decoratorValue)) {
        return 0;
    }
    state->stackTop.valuePointer = functionBase + 1;
    return 1;
}

ZR_CORE_API TZrInt64 ZrCore_RuntimeDecorator_ApplyMemberNativeEntry(SZrState *state) {
    TZrStackValuePointer functionBase;
    TZrStackValuePointer argBase;
    SZrFunctionStackAnchor functionBaseAnchor;
    SZrTypeValue *result;
    SZrTypeValue *prototypeValue;
    SZrTypeValue *memberNameValue;
    SZrTypeValue *kindValue;
    SZrTypeValue *decoratorValue;
    SZrObjectPrototype *prototype;
    SZrString *memberName;
    EZrRuntimeDecoratorTargetKind targetKind;
    SZrObject *reflectionObject;
    SZrObject *metadataObject;
    SZrObject *decoratorsArray;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    functionBase = state->callInfoList->functionBase.valuePointer;
    ZrCore_Function_StackAnchorInit(state, functionBase, &functionBaseAnchor);
    if (!runtime_decorator_restore_apply_member_frame(state,
                                                      &functionBaseAnchor,
                                                      &functionBase,
                                                      &argBase,
                                                      &result,
                                                      &prototypeValue,
                                                      &memberNameValue,
                                                      &kindValue,
                                                      &decoratorValue)) {
        return 0;
    }

    if (result != ZR_NULL && prototypeValue != ZR_NULL) {
        ZrCore_Value_Copy(state, result, prototypeValue);
    } else if (result != ZR_NULL) {
        ZrCore_Value_ResetAsNull(result);
    }

    if (prototypeValue == ZR_NULL ||
        memberNameValue == ZR_NULL ||
        kindValue == ZR_NULL ||
        decoratorValue == ZR_NULL ||
        prototypeValue->type != ZR_VALUE_TYPE_OBJECT ||
         prototypeValue->value.object == ZR_NULL ||
         ZR_CAST_OBJECT(state, prototypeValue->value.object)->internalType != ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE ||
         memberNameValue->type != ZR_VALUE_TYPE_STRING ||
         memberNameValue->value.object == ZR_NULL ||
         !ZR_VALUE_IS_TYPE_INT(kindValue->type)) {
        if (!runtime_decorator_restore_apply_member_frame(state,
                                                          &functionBaseAnchor,
                                                          &functionBase,
                                                          &argBase,
                                                          &result,
                                                          &prototypeValue,
                                                          &memberNameValue,
                                                          &kindValue,
                                                          &decoratorValue)) {
            return 0;
        }
        state->stackTop.valuePointer = functionBase + 1;
        return 1;
    }

    prototype = (SZrObjectPrototype *)ZR_CAST_OBJECT(state, prototypeValue->value.object);
    memberName = ZR_CAST_STRING(state, memberNameValue->value.object);
    targetKind = (EZrRuntimeDecoratorTargetKind)kindValue->value.nativeObject.nativeInt64;
    reflectionObject = runtime_decorator_build_member_reflection(state, prototypeValue, memberName, targetKind);
    if (reflectionObject == ZR_NULL) {
        if (!runtime_decorator_restore_apply_member_frame(state,
                                                          &functionBaseAnchor,
                                                          &functionBase,
                                                          &argBase,
                                                          &result,
                                                          &prototypeValue,
                                                          &memberNameValue,
                                                          &kindValue,
                                                          &decoratorValue)) {
            return 0;
        }
        state->stackTop.valuePointer = functionBase + 1;
        return 1;
    }

    runtime_decorator_append_record(state, reflectionObject, decoratorValue);
    if (!runtime_decorator_restore_apply_member_frame(state,
                                                      &functionBaseAnchor,
                                                      &functionBase,
                                                      &argBase,
                                                      &result,
                                                      &prototypeValue,
                                                      &memberNameValue,
                                                      &kindValue,
                                                      &decoratorValue)) {
        return 0;
    }

    {
        SZrTypeValue reflectionValue;
        ZrCore_Value_InitAsRawObject(state, &reflectionValue, ZR_CAST_RAW_OBJECT_AS_SUPER(reflectionObject));
        reflectionValue.type = ZR_VALUE_TYPE_OBJECT;

        if (decoratorValue->type == ZR_VALUE_TYPE_FUNCTION ||
            decoratorValue->type == ZR_VALUE_TYPE_CLOSURE ||
            decoratorValue->type == ZR_VALUE_TYPE_NATIVE_POINTER) {
            if (!runtime_decorator_call_function(state, decoratorValue, &reflectionValue, ZR_NULL) &&
                state->threadStatus == ZR_THREAD_STATUS_FINE) {
                ZrCore_Debug_RunError(state, "runtime decorator function call failed");
            }
        } else if (decoratorValue->type == ZR_VALUE_TYPE_OBJECT) {
            if (!runtime_decorator_invoke_object_decorator(state, decoratorValue, &reflectionValue) &&
                state->threadStatus == ZR_THREAD_STATUS_FINE) {
                ZrCore_Debug_RunError(state, "runtime decorator object must define @decorate");
            }
        } else {
            ZrCore_Debug_RunError(state, "runtime decorator target must be a callable or decorator object");
        }
    }

    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        return 1;
    }

    metadataObject = runtime_decorator_get_object_field_as_object(state, reflectionObject, "metadata", ZR_VALUE_TYPE_OBJECT);
    decoratorsArray = runtime_decorator_get_object_field_as_object(state, reflectionObject, "decorators", ZR_VALUE_TYPE_ARRAY);
    runtime_decorator_commit_member_overlay(state, prototype, memberName, targetKind, metadataObject, decoratorsArray);

    if (!runtime_decorator_restore_apply_member_frame(state,
                                                      &functionBaseAnchor,
                                                      &functionBase,
                                                      &argBase,
                                                      &result,
                                                      &prototypeValue,
                                                      &memberNameValue,
                                                      &kindValue,
                                                      &decoratorValue)) {
        return 0;
    }
    state->stackTop.valuePointer = functionBase + 1;
    return 1;
}
