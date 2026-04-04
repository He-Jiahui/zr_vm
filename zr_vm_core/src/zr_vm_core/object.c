//
// Created by HeJiahui on 2025/6/22.
//
#include "zr_vm_core/object.h"

#include "zr_vm_core/call_info.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include <string.h>

static TZrBool object_node_map_is_ready(const SZrObject *object) {
    return object != ZR_NULL && object->nodeMap.isValid && object->nodeMap.buckets != ZR_NULL &&
           object->nodeMap.capacity > 0;
}

static TZrBool ensure_managed_field_capacity(SZrState *state, SZrObjectPrototype *prototype, TZrUInt32 minimumCapacity) {
    SZrManagedFieldInfo *newFields;
    TZrUInt32 newCapacity;
    TZrSize newSize;

    if (state == ZR_NULL || prototype == ZR_NULL) {
        return ZR_FALSE;
    }

    if (prototype->managedFieldCapacity >= minimumCapacity) {
        return ZR_TRUE;
    }

    newCapacity = prototype->managedFieldCapacity > 0 ? prototype->managedFieldCapacity : ZR_RUNTIME_OBJECT_PROTOTYPE_INITIAL_CAPACITY;
    while (newCapacity < minimumCapacity) {
        newCapacity *= ZR_RUNTIME_OBJECT_PROTOTYPE_GROWTH_FACTOR;
    }

    newSize = newCapacity * sizeof(SZrManagedFieldInfo);
    newFields = (SZrManagedFieldInfo *)ZrCore_Memory_RawMallocWithType(state->global,
                                                                 newSize,
                                                                 ZR_MEMORY_NATIVE_TYPE_OBJECT);
    if (newFields == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(newFields, 0, newSize);
    if (prototype->managedFields != ZR_NULL && prototype->managedFieldCount > 0) {
        memcpy(newFields,
               prototype->managedFields,
               prototype->managedFieldCount * sizeof(SZrManagedFieldInfo));
        ZrCore_Memory_RawFreeWithType(state->global,
                                prototype->managedFields,
                                prototype->managedFieldCapacity * sizeof(SZrManagedFieldInfo),
                                ZR_MEMORY_NATIVE_TYPE_OBJECT);
    }

    prototype->managedFields = newFields;
    prototype->managedFieldCapacity = newCapacity;
    return ZR_TRUE;
}

static const SZrManagedFieldInfo *object_prototype_find_managed_field(SZrObjectPrototype *prototype,
                                                                      SZrString *memberName,
                                                                      TZrBool includeInherited) {
    while (prototype != ZR_NULL) {
        for (TZrUInt32 index = 0; index < prototype->managedFieldCount; index++) {
            SZrManagedFieldInfo *fieldInfo = &prototype->managedFields[index];
            if (fieldInfo->name != ZR_NULL && memberName != ZR_NULL &&
                ZrCore_String_Equal(fieldInfo->name, memberName)) {
                return fieldInfo;
            }
        }

        if (!includeInherited) {
            break;
        }
        prototype = prototype->superPrototype;
    }

    return ZR_NULL;
}

static TZrBool object_make_string_key(SZrState *state, SZrString *name, SZrTypeValue *outKey) {
    if (state == ZR_NULL || name == ZR_NULL || outKey == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(state, outKey, ZR_CAST_RAW_OBJECT_AS_SUPER(name));
    outKey->type = ZR_VALUE_TYPE_STRING;
    return ZR_TRUE;
}

static TZrBool object_make_hidden_key(SZrState *state, const TZrChar *name, SZrTypeValue *outKey) {
    SZrString *stringObject;

    if (state == ZR_NULL || name == ZR_NULL || outKey == ZR_NULL) {
        return ZR_FALSE;
    }

    stringObject = ZrCore_String_Create(state, (TZrNativeString)name, strlen(name));
    if (stringObject == ZR_NULL) {
        return ZR_FALSE;
    }

    return object_make_string_key(state, stringObject, outKey);
}

static TZrBool object_hidden_set(SZrState *state, SZrObject *object, const TZrChar *name, const SZrTypeValue *value) {
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || value == ZR_NULL || !object_make_hidden_key(state, name, &key)) {
        return ZR_FALSE;
    }

    ZrCore_Object_SetValue(state, object, &key, value);
    return ZR_TRUE;
}

static TZrBool object_hidden_get(SZrState *state, SZrObject *object, const TZrChar *name, SZrTypeValue *outValue) {
    SZrTypeValue key;
    const SZrTypeValue *value;

    if (state == ZR_NULL || object == ZR_NULL || outValue == ZR_NULL || !object_make_hidden_key(state, name, &key)) {
        return ZR_FALSE;
    }

    value = ZrCore_Object_GetValue(state, object, &key);
    if (value == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_ResetAsNull(outValue);
    ZrCore_Value_Copy(state, outValue, value);
    return ZR_TRUE;
}

static TZrStackValuePointer object_resolve_call_scratch_base(TZrStackValuePointer stackTop,
                                                             const SZrCallInfo *callInfo) {
    TZrStackValuePointer base = stackTop;

    if (callInfo != ZR_NULL && callInfo->functionTop.valuePointer > base) {
        base = callInfo->functionTop.valuePointer;
    }

    return base;
}

static TZrBool object_pin_value_object(SZrState *state, const SZrTypeValue *value, TZrBool *addedByCaller) {
    SZrRawObject *object;

    if (addedByCaller != ZR_NULL) {
        *addedByCaller = ZR_FALSE;
    }

    if (state == ZR_NULL || state->global == ZR_NULL || value == ZR_NULL || !ZrCore_Value_IsGarbageCollectable(value)) {
        return ZR_TRUE;
    }

    object = ZrCore_Value_GetRawObject(value);
    if (object == ZR_NULL) {
        return ZR_TRUE;
    }

    if (ZrCore_GarbageCollector_IsObjectIgnored(state->global, object)) {
        return ZR_TRUE;
    }

    if (!ZrCore_GarbageCollector_IgnoreObject(state, object)) {
        return ZR_FALSE;
    }

    if (addedByCaller != ZR_NULL) {
        *addedByCaller = ZR_TRUE;
    }
    return ZR_TRUE;
}

static void object_unpin_value_object(SZrGlobalState *global, const SZrTypeValue *value, TZrBool addedByCaller) {
    SZrRawObject *object;

    if (!addedByCaller || global == ZR_NULL || value == ZR_NULL || !ZrCore_Value_IsGarbageCollectable(value)) {
        return;
    }

    object = ZrCore_Value_GetRawObject(value);
    if (object != ZR_NULL) {
        ZrCore_GarbageCollector_UnignoreObject(global, object);
    }
}

static TZrBool object_call_value(SZrState *state,
                                 const SZrTypeValue *callable,
                                 const SZrTypeValue *receiver,
                                 const SZrTypeValue *arguments,
                                 TZrSize argumentCount,
                                 SZrTypeValue *result) {
    SZrTypeValue stableCallable;
    SZrTypeValue stableReceiver;
    SZrTypeValue inlineArguments[ZR_RUNTIME_OBJECT_CALL_INLINE_ARGUMENT_CAPACITY];
    SZrTypeValue *stableArguments = ZR_NULL;
    TZrBool inlineArgumentPinAdded[ZR_RUNTIME_OBJECT_CALL_INLINE_ARGUMENT_CAPACITY];
    TZrBool *argumentPinAdded = ZR_NULL;
    TZrBool freeStableArguments = ZR_FALSE;
    TZrBool freeArgumentPinAdded = ZR_FALSE;
    TZrBool callablePinAdded = ZR_FALSE;
    TZrBool receiverPinAdded = ZR_FALSE;
    TZrSize stableArgumentsBytes = 0;
    TZrSize argumentPinAddedBytes = 0;
    TZrStackValuePointer savedStackTop;
    SZrCallInfo *savedCallInfo;
    TZrSize totalArguments;
    TZrSize scratchSlots;
    TZrStackValuePointer base;
    TZrStackValuePointer resultStackSlot;
    SZrFunctionStackAnchor savedStackTopAnchor;
    SZrFunctionStackAnchor baseAnchor;
    SZrFunctionStackAnchor callInfoBaseAnchor;
    SZrFunctionStackAnchor callInfoTopAnchor;
    SZrFunctionStackAnchor callInfoReturnAnchor;
    SZrFunctionStackAnchor resultAnchor;
    TZrBool hasAnchoredReturnDestination = ZR_FALSE;
    TZrBool hasResultAnchor = ZR_FALSE;
    TZrSize index;

    if (state == ZR_NULL || callable == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    stableCallable = *callable;
    memset(inlineArgumentPinAdded, 0, sizeof(inlineArgumentPinAdded));
    if (receiver != ZR_NULL) {
        stableReceiver = *receiver;
    }
    if (argumentCount > 0) {
        if (arguments == ZR_NULL) {
            return ZR_FALSE;
        }

        if (argumentCount <= ZR_RUNTIME_OBJECT_CALL_INLINE_ARGUMENT_CAPACITY) {
            stableArguments = inlineArguments;
            argumentPinAdded = inlineArgumentPinAdded;
        } else {
            stableArgumentsBytes = argumentCount * sizeof(SZrTypeValue);
            stableArguments = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(state->global,
                                                                              stableArgumentsBytes,
                                                                              ZR_MEMORY_NATIVE_TYPE_OBJECT);
            if (stableArguments == ZR_NULL) {
                return ZR_FALSE;
            }
            freeStableArguments = ZR_TRUE;

            argumentPinAddedBytes = argumentCount * sizeof(TZrBool);
            argumentPinAdded = (TZrBool *)ZrCore_Memory_RawMallocWithType(state->global,
                                                                          argumentPinAddedBytes,
                                                                          ZR_MEMORY_NATIVE_TYPE_OBJECT);
            if (argumentPinAdded == ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(state->global,
                                              stableArguments,
                                              stableArgumentsBytes,
                                              ZR_MEMORY_NATIVE_TYPE_OBJECT);
                return ZR_FALSE;
            }
            freeArgumentPinAdded = ZR_TRUE;
        }

        memset(argumentPinAdded, 0, argumentCount * sizeof(TZrBool));
        for (index = 0; index < argumentCount; index++) {
            stableArguments[index] = arguments[index];
        }
    }

    if (!object_pin_value_object(state, &stableCallable, &callablePinAdded)) {
        if (freeArgumentPinAdded) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          argumentPinAdded,
                                          argumentPinAddedBytes,
                                          ZR_MEMORY_NATIVE_TYPE_OBJECT);
        }
        if (freeStableArguments) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          stableArguments,
                                          stableArgumentsBytes,
                                          ZR_MEMORY_NATIVE_TYPE_OBJECT);
        }
        return ZR_FALSE;
    }

    if (receiver != ZR_NULL && !object_pin_value_object(state, &stableReceiver, &receiverPinAdded)) {
        object_unpin_value_object(state->global, &stableCallable, callablePinAdded);
        if (freeArgumentPinAdded) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          argumentPinAdded,
                                          argumentPinAddedBytes,
                                          ZR_MEMORY_NATIVE_TYPE_OBJECT);
        }
        if (freeStableArguments) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          stableArguments,
                                          stableArgumentsBytes,
                                          ZR_MEMORY_NATIVE_TYPE_OBJECT);
        }
        return ZR_FALSE;
    }

    for (index = 0; index < argumentCount; index++) {
        if (!object_pin_value_object(state, &stableArguments[index], &argumentPinAdded[index])) {
            while (index > 0) {
                index--;
                object_unpin_value_object(state->global, &stableArguments[index], argumentPinAdded[index]);
            }
            object_unpin_value_object(state->global, receiver != ZR_NULL ? &stableReceiver : ZR_NULL, receiverPinAdded);
            object_unpin_value_object(state->global, &stableCallable, callablePinAdded);
            if (freeArgumentPinAdded) {
                ZrCore_Memory_RawFreeWithType(state->global,
                                              argumentPinAdded,
                                              argumentPinAddedBytes,
                                              ZR_MEMORY_NATIVE_TYPE_OBJECT);
            }
            if (freeStableArguments) {
                ZrCore_Memory_RawFreeWithType(state->global,
                                              stableArguments,
                                              stableArgumentsBytes,
                                              ZR_MEMORY_NATIVE_TYPE_OBJECT);
            }
            return ZR_FALSE;
        }
    }

    ZrCore_Value_ResetAsNull(result);
    savedStackTop = state->stackTop.valuePointer;
    savedCallInfo = state->callInfoList;
    totalArguments = argumentCount + (receiver != ZR_NULL ? 1 : 0);
    scratchSlots = 1 + totalArguments;
    base = object_resolve_call_scratch_base(savedStackTop, savedCallInfo);
    resultStackSlot = ZR_CAST(TZrStackValuePointer, result);

    if (resultStackSlot >= state->stackBase.valuePointer && resultStackSlot < state->stackTail.valuePointer) {
        ZrCore_Function_StackAnchorInit(state, resultStackSlot, &resultAnchor);
        hasResultAnchor = ZR_TRUE;
    }

    ZrCore_Function_StackAnchorInit(state, savedStackTop, &savedStackTopAnchor);
    ZrCore_Function_StackAnchorInit(state, base, &baseAnchor);
    if (savedCallInfo != ZR_NULL) {
        ZrCore_Function_StackAnchorInit(state, savedCallInfo->functionBase.valuePointer, &callInfoBaseAnchor);
        ZrCore_Function_StackAnchorInit(state, savedCallInfo->functionTop.valuePointer, &callInfoTopAnchor);
        hasAnchoredReturnDestination =
            (TZrBool)(savedCallInfo->hasReturnDestination && savedCallInfo->returnDestination != ZR_NULL);
        if (hasAnchoredReturnDestination) {
            ZrCore_Function_StackAnchorInit(state, savedCallInfo->returnDestination, &callInfoReturnAnchor);
        }
    }

    ZrCore_Function_ReserveScratchSlots(state, scratchSlots, base);
    savedStackTop = ZrCore_Function_StackAnchorRestore(state, &savedStackTopAnchor);
    base = ZrCore_Function_StackAnchorRestore(state, &baseAnchor);
    if (savedCallInfo != ZR_NULL) {
        savedCallInfo->functionBase.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoBaseAnchor);
        savedCallInfo->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoTopAnchor);
        if (hasAnchoredReturnDestination) {
            savedCallInfo->returnDestination = ZrCore_Function_StackAnchorRestore(state, &callInfoReturnAnchor);
        }
        base = object_resolve_call_scratch_base(savedStackTop, savedCallInfo);
    }

    ZrCore_Stack_CopyValue(state, base, &stableCallable);
    if (receiver != ZR_NULL) {
        ZrCore_Stack_CopyValue(state, base + 1, &stableReceiver);
    }
    for (index = 0; index < argumentCount; index++) {
        ZrCore_Stack_CopyValue(state,
                               base + 1 + (receiver != ZR_NULL ? 1 : 0) + index,
                               &stableArguments[index]);
    }

    state->stackTop.valuePointer = base + scratchSlots;
    if (savedCallInfo != ZR_NULL && savedCallInfo->functionTop.valuePointer < state->stackTop.valuePointer) {
        savedCallInfo->functionTop.valuePointer = state->stackTop.valuePointer;
        ZrCore_Function_StackAnchorInit(state, savedCallInfo->functionBase.valuePointer, &callInfoBaseAnchor);
        ZrCore_Function_StackAnchorInit(state, savedCallInfo->functionTop.valuePointer, &callInfoTopAnchor);
        if (hasAnchoredReturnDestination) {
            ZrCore_Function_StackAnchorInit(state, savedCallInfo->returnDestination, &callInfoReturnAnchor);
        }
    }

    for (index = argumentCount; index > 0; index--) {
        object_unpin_value_object(state->global,
                                  &stableArguments[index - 1],
                                  argumentPinAdded[index - 1]);
    }
    object_unpin_value_object(state->global,
                              receiver != ZR_NULL ? &stableReceiver : ZR_NULL,
                              receiverPinAdded);
    object_unpin_value_object(state->global, &stableCallable, callablePinAdded);

    base = ZrCore_Function_CallWithoutYieldAndRestore(state, base, 1);
    savedStackTop = ZrCore_Function_StackAnchorRestore(state, &savedStackTopAnchor);
    if (savedCallInfo != ZR_NULL) {
        savedCallInfo->functionBase.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoBaseAnchor);
        savedCallInfo->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoTopAnchor);
        if (hasAnchoredReturnDestination) {
            savedCallInfo->returnDestination = ZrCore_Function_StackAnchorRestore(state, &callInfoReturnAnchor);
        }
    }
    if (hasResultAnchor) {
        result = ZrCore_Stack_GetValue(ZrCore_Function_StackAnchorRestore(state, &resultAnchor));
    }

    if (state->threadStatus == ZR_THREAD_STATUS_FINE) {
        SZrTypeValue *stackResult = ZrCore_Stack_GetValue(base);
        ZrCore_Value_Copy(state, result, stackResult);
        state->stackTop.valuePointer = savedStackTop;
        state->callInfoList = savedCallInfo;
        if (freeStableArguments) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          stableArguments,
                                          stableArgumentsBytes,
                                          ZR_MEMORY_NATIVE_TYPE_OBJECT);
        }
        if (freeArgumentPinAdded) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          argumentPinAdded,
                                          argumentPinAddedBytes,
                                          ZR_MEMORY_NATIVE_TYPE_OBJECT);
        }
        return ZR_TRUE;
    }

    state->stackTop.valuePointer = savedStackTop;
    state->callInfoList = savedCallInfo;
    if (freeStableArguments) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      stableArguments,
                                      stableArgumentsBytes,
                                      ZR_MEMORY_NATIVE_TYPE_OBJECT);
    }
    if (freeArgumentPinAdded) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      argumentPinAdded,
                                      argumentPinAddedBytes,
                                      ZR_MEMORY_NATIVE_TYPE_OBJECT);
    }
    return ZR_FALSE;
}

static SZrObjectPrototype *object_value_resolve_prototype(SZrState *state, const SZrTypeValue *value) {
    SZrObject *object;

    if (state == ZR_NULL || value == ZR_NULL) {
        return ZR_NULL;
    }

    if ((value->type != ZR_VALUE_TYPE_OBJECT && value->type != ZR_VALUE_TYPE_ARRAY) || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZR_CAST_OBJECT(state, value->value.object);
    if (object != ZR_NULL && object->prototype != ZR_NULL) {
        return object->prototype;
    }

    if (state->global == ZR_NULL || value->type >= ZR_VALUE_TYPE_ENUM_MAX) {
        return ZR_NULL;
    }

    return state->global->basicTypeObjectPrototype[value->type];
}

static const SZrTypeValue *object_get_own_value(SZrState *state, SZrObject *object, const SZrTypeValue *key) {
    SZrHashKeyValuePair *pair;

    if (state == ZR_NULL || object == ZR_NULL || key == ZR_NULL || !object_node_map_is_ready(object)) {
        return ZR_NULL;
    }

    pair = ZrCore_HashSet_Find(state, &object->nodeMap, key);
    return pair != ZR_NULL ? &pair->value : ZR_NULL;
}

static const SZrTypeValue *object_get_prototype_value(SZrState *state,
                                                      SZrObjectPrototype *prototype,
                                                      const SZrTypeValue *key,
                                                      TZrBool includeInherited) {
    while (prototype != ZR_NULL) {
        const SZrTypeValue *value = object_get_own_value(state, &prototype->super, key);
        if (value != ZR_NULL) {
            return value;
        }

        if (!includeInherited) {
            break;
        }
        prototype = prototype->superPrototype;
    }

    return ZR_NULL;
}

static TZrBool object_make_callable_value(SZrState *state, SZrFunction *function, SZrTypeValue *result) {
    if (state == ZR_NULL || function == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    return ZR_TRUE;
}

static TZrBool object_call_function_with_receiver(SZrState *state,
                                                  SZrFunction *function,
                                                  SZrTypeValue *receiver,
                                                  const SZrTypeValue *arguments,
                                                  TZrSize argumentCount,
                                                  SZrTypeValue *result) {
    SZrTypeValue callableValue;

    if (!object_make_callable_value(state, function, &callableValue)) {
        return ZR_FALSE;
    }

    return object_call_value(state, &callableValue, receiver, arguments, argumentCount, result);
}

static TZrBool object_allows_dynamic_member_write(const SZrObject *object) {
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    return object->prototype == ZR_NULL || object->prototype->dynamicMemberCapable;
}

static TZrBool object_can_use_direct_index_fallback(const SZrObject *object) {
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    return object->prototype == ZR_NULL || object->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY;
}

static TZrBool object_get_index_length(SZrState *state,
                                       SZrTypeValue *receiver,
                                       SZrTypeValue *result) {
    SZrObject *object;
    SZrObjectPrototype *prototype;

    if (state == ZR_NULL || receiver == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (receiver->type != ZR_VALUE_TYPE_OBJECT && receiver->type != ZR_VALUE_TYPE_ARRAY) {
        return ZR_FALSE;
    }

    object = ZR_CAST_OBJECT(state, receiver->value.object);
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    prototype = object_value_resolve_prototype(state, receiver);
    if (prototype != ZR_NULL && prototype->indexContract.getLengthFunction != ZR_NULL) {
        return object_call_function_with_receiver(state,
                                                  prototype->indexContract.getLengthFunction,
                                                  receiver,
                                                  ZR_NULL,
                                                  0,
                                                  result);
    }

    if (!object_can_use_direct_index_fallback(object)) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsInt(state, result, (TZrInt64)object->nodeMap.elementCount);
    return ZR_TRUE;
}


SZrObject *ZrCore_Object_New(SZrState *state, SZrObjectPrototype *prototype) {
    SZrRawObject *rawObject = ZrCore_RawObject_New(state, ZR_VALUE_TYPE_OBJECT, sizeof(SZrObject), ZR_FALSE);
    SZrObject *object = ZR_CAST_OBJECT(state, rawObject);
    object->prototype = prototype;
    object->internalType = ZR_OBJECT_INTERNAL_TYPE_OBJECT;
    object->memberVersion = 0;
    ZrCore_HashSet_Construct(&object->nodeMap);
    return object;
}

SZrObject *ZrCore_Object_NewCustomized(struct SZrState *state, TZrSize size, EZrObjectInternalType internalType) {
    // 根据 internalType 选择正确的值类型
    EZrValueType valueType = ZR_VALUE_TYPE_OBJECT;
    if (internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        valueType = ZR_VALUE_TYPE_ARRAY;
    }
    SZrRawObject *rawObject = ZrCore_RawObject_New(state, valueType, size, ZR_FALSE);
    SZrObject *object = ZR_CAST_OBJECT(state, rawObject);
    object->prototype = (internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY && state != ZR_NULL && state->global != ZR_NULL)
                                ? state->global->basicTypeObjectPrototype[ZR_VALUE_TYPE_ARRAY]
                                : ZR_NULL;
    object->internalType = internalType;
    object->memberVersion = 0;
    ZrCore_HashSet_Construct(&object->nodeMap);
    return object;
}

void ZrCore_Object_Init(struct SZrState *state, SZrObject *object) {
    ZrCore_HashSet_Init(state, &object->nodeMap, ZR_OBJECT_TABLE_INITIAL_SIZE_LOG2);
}

TZrBool ZrCore_Object_CompareWithAddress(struct SZrState *state, SZrObject *object1, SZrObject *object2) {
    ZR_UNUSED_PARAMETER(state);
    return object1 == object2;
}


void ZrCore_Object_SetValue(struct SZrState *state, SZrObject *object, const SZrTypeValue *key, const SZrTypeValue *value) {
    ZR_ASSERT(object != ZR_NULL);
    if (key == ZR_NULL) {
        ZrCore_Log_Error(state, "attempt to set value with null key");
        return;
    }
    if (!object_node_map_is_ready(object)) {
        if (state == ZR_NULL) {
            return;
        }
        ZrCore_Object_Init(state, object);
        if (!object_node_map_is_ready(object)) {
            ZrCore_Log_Error(state, "failed to initialize object storage");
            return;
        }
    }
    SZrHashSet *nodeMap = &object->nodeMap;
    SZrHashKeyValuePair *pair = ZrCore_HashSet_Find(state, nodeMap, key);
    if (pair == ZR_NULL) {
        pair = ZrCore_HashSet_Add(state, nodeMap, key);
        if (pair == ZR_NULL) {
            ZrCore_Log_Error(state, "failed to allocate object storage entry");
            return;
        }
    }
    ZrCore_Value_Copy(state, &pair->value, value);
    object->memberVersion++;
}

const SZrTypeValue *ZrCore_Object_GetValue(struct SZrState *state, SZrObject *object, const SZrTypeValue *key) {
    SZrHashKeyValuePair *pair = ZR_NULL;

    ZR_ASSERT(object != ZR_NULL);
    if (key == ZR_NULL) {
        ZrCore_Log_Error(state, "attempt to get value with null key");
        return ZR_NULL;
    }
    if (object_node_map_is_ready(object)) {
        pair = ZrCore_HashSet_Find(state, &object->nodeMap, key);
    }
    if (pair != ZR_NULL) {
        return &pair->value;
    }

    if (object->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE) {
        SZrObjectPrototype *prototype = (SZrObjectPrototype *)object;
        prototype = prototype->superPrototype;
        while (prototype != ZR_NULL) {
            if (object_node_map_is_ready(&prototype->super)) {
                pair = ZrCore_HashSet_Find(state, &prototype->super.nodeMap, key);
            } else {
                pair = ZR_NULL;
            }
            if (pair != ZR_NULL) {
                return &pair->value;
            }
            prototype = prototype->superPrototype;
        }
        return ZR_NULL;
    }

    SZrObjectPrototype *prototype = object->prototype;
    while (prototype != ZR_NULL) {
        pair = ZrCore_HashSet_Find(state, &prototype->super.nodeMap, key);
        if (pair != ZR_NULL) {
            return &pair->value;
        }
        prototype = prototype->superPrototype;
    }

    return ZR_NULL;
}

// 创建基础 ObjectPrototype
SZrObjectPrototype *ZrCore_ObjectPrototype_New(SZrState *state, SZrString *name, EZrObjectPrototypeType type) {
    if (state == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 创建 ObjectPrototype 对象
    SZrObject *objectBase = ZrCore_Object_NewCustomized(state, sizeof(SZrObjectPrototype), ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE);
    if (objectBase == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrObjectPrototype *prototype = (SZrObjectPrototype *)objectBase;
    
    // 初始化哈希集
    ZrCore_HashSet_Init(state, &prototype->super.nodeMap, ZR_OBJECT_TABLE_INITIAL_SIZE_LOG2);
    
    // 初始化 ObjectPrototype 特定字段
    prototype->name = name;
    prototype->type = type;
    prototype->superPrototype = ZR_NULL;
    prototype->memberDescriptors = ZR_NULL;
    prototype->memberDescriptorCount = 0;
    prototype->memberDescriptorCapacity = 0;
    memset(&prototype->indexContract, 0, sizeof(prototype->indexContract));
    memset(&prototype->iterableContract, 0, sizeof(prototype->iterableContract));
    memset(&prototype->iteratorContract, 0, sizeof(prototype->iteratorContract));
    prototype->protocolMask = 0;
    prototype->dynamicMemberCapable = ZR_FALSE;
    prototype->reserved0 = 0;
    prototype->reserved1 = 0;
    prototype->managedFields = ZR_NULL;
    prototype->managedFieldCount = 0;
    prototype->managedFieldCapacity = 0;
    
    // 初始化 metaTable
    ZrCore_MetaTable_Construct(&prototype->metaTable);
    
    // 标记为永久对象（避免被 GC 回收）
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
    
    return prototype;
}

// 创建 StructPrototype
SZrStructPrototype *ZrCore_StructPrototype_New(SZrState *state, SZrString *name) {
    if (state == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 创建 StructPrototype 对象
    SZrObject *objectBase = ZrCore_Object_NewCustomized(state, sizeof(SZrStructPrototype), ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE);
    if (objectBase == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrStructPrototype *prototype = (SZrStructPrototype *)objectBase;
    
    // 初始化哈希集
    ZrCore_HashSet_Init(state, &prototype->super.super.nodeMap, ZR_OBJECT_TABLE_INITIAL_SIZE_LOG2);
    
    // 初始化 ObjectPrototype 特定字段
    prototype->super.name = name;
    prototype->super.type = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT;
    prototype->super.superPrototype = ZR_NULL;
    prototype->super.memberDescriptors = ZR_NULL;
    prototype->super.memberDescriptorCount = 0;
    prototype->super.memberDescriptorCapacity = 0;
    memset(&prototype->super.indexContract, 0, sizeof(prototype->super.indexContract));
    memset(&prototype->super.iterableContract, 0, sizeof(prototype->super.iterableContract));
    memset(&prototype->super.iteratorContract, 0, sizeof(prototype->super.iteratorContract));
    prototype->super.protocolMask = 0;
    prototype->super.dynamicMemberCapable = ZR_FALSE;
    prototype->super.reserved0 = 0;
    prototype->super.reserved1 = 0;
    prototype->super.managedFields = ZR_NULL;
    prototype->super.managedFieldCount = 0;
    prototype->super.managedFieldCapacity = 0;
    
    // 初始化 metaTable
    ZrCore_MetaTable_Construct(&prototype->super.metaTable);
    
    // 初始化 keyOffsetMap
    ZrCore_HashSet_Construct(&prototype->keyOffsetMap);
    ZrCore_HashSet_Init(state, &prototype->keyOffsetMap, ZR_OBJECT_TABLE_INITIAL_SIZE_LOG2);
    
    // 标记为永久对象（避免被 GC 回收）
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
    
    return prototype;
}

// 设置继承关系
void ZrCore_ObjectPrototype_SetSuper(SZrState *state, SZrObjectPrototype *prototype, SZrObjectPrototype *superPrototype) {
    ZR_UNUSED_PARAMETER(state);
    if (prototype == ZR_NULL) {
        return;
    }
    prototype->superPrototype = superPrototype;
    prototype->super.memberVersion++;
}

// 初始化元表
void ZrCore_ObjectPrototype_InitMetaTable(SZrState *state, SZrObjectPrototype *prototype) {
    ZR_UNUSED_PARAMETER(state);
    if (prototype == ZR_NULL) {
        return;
    }
    ZrCore_MetaTable_Construct(&prototype->metaTable);
}

// 向 StructPrototype 添加字段
void ZrCore_StructPrototype_AddField(SZrState *state, SZrStructPrototype *prototype, SZrString *fieldName, TZrSize offset) {
    if (state == ZR_NULL || prototype == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }
    
    // 创建键值（字段名）
    SZrTypeValue key;
    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldName));
    key.type = ZR_VALUE_TYPE_STRING;
    
    // 创建值（偏移量）
    SZrTypeValue value;
    ZrCore_Value_InitAsUInt(state, &value, (TZrUInt64)offset);
    
    // 添加到 keyOffsetMap
    SZrHashKeyValuePair *pair = ZrCore_HashSet_Find(state, &prototype->keyOffsetMap, &key);
    if (pair == ZR_NULL) {
        pair = ZrCore_HashSet_Add(state, &prototype->keyOffsetMap, &key);
        if (pair == ZR_NULL) {
            return;
        }
    }
    ZrCore_Value_Copy(state, &pair->value, &value);
}

// 向 Prototype 添加元函数
void ZrCore_ObjectPrototype_AddMeta(SZrState *state, SZrObjectPrototype *prototype, EZrMetaType metaType, SZrFunction *function) {
    if (state == ZR_NULL || prototype == ZR_NULL || function == ZR_NULL) {
        return;
    }
    
    if (metaType >= ZR_META_ENUM_MAX) {
        return;
    }
    
    // 创建 Meta 对象
    SZrGlobalState *global = state->global;
    SZrMeta *meta = (SZrMeta *)ZrCore_Memory_RawMallocWithType(global, sizeof(SZrMeta), ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    if (meta == ZR_NULL) {
        return;
    }
    
    meta->metaType = metaType;
    meta->function = function;
    
    // 添加到 metaTable
    prototype->metaTable.metas[metaType] = meta;
}

void ZrCore_ObjectPrototype_AddManagedField(SZrState *state,
                                      SZrObjectPrototype *prototype,
                                      SZrString *fieldName,
                                      TZrUInt32 fieldOffset,
                                      TZrUInt32 fieldSize,
                                      TZrUInt32 ownershipQualifier,
                                      TZrBool callsClose,
                                      TZrBool callsDestructor,
                                      TZrUInt32 declarationOrder) {
    SZrManagedFieldInfo *fieldInfo;

    if (state == ZR_NULL || prototype == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }

    if (!ensure_managed_field_capacity(state, prototype, prototype->managedFieldCount + 1)) {
        return;
    }

    fieldInfo = &prototype->managedFields[prototype->managedFieldCount++];
    fieldInfo->name = fieldName;
    fieldInfo->fieldOffset = fieldOffset;
    fieldInfo->fieldSize = fieldSize;
    fieldInfo->ownershipQualifier = ownershipQualifier;
    fieldInfo->callsClose = callsClose;
    fieldInfo->callsDestructor = callsDestructor;
    fieldInfo->declarationOrder = declarationOrder;
}

const SZrMemberDescriptor *ZrCore_ObjectPrototype_FindMemberDescriptor(SZrObjectPrototype *prototype,
                                                                       SZrString *memberName,
                                                                       TZrBool includeInherited) {
    while (prototype != ZR_NULL) {
        for (TZrUInt32 index = 0; index < prototype->memberDescriptorCount; index++) {
            SZrMemberDescriptor *descriptor = &prototype->memberDescriptors[index];
            if (descriptor->name != ZR_NULL && memberName != ZR_NULL &&
                ZrCore_String_Equal(descriptor->name, memberName)) {
                return descriptor;
            }
        }

        if (!includeInherited) {
            break;
        }
        prototype = prototype->superPrototype;
    }

    return ZR_NULL;
}

TZrBool ZrCore_ObjectPrototype_AddMemberDescriptor(struct SZrState *state,
                                                   SZrObjectPrototype *prototype,
                                                   const SZrMemberDescriptor *descriptor) {
    SZrMemberDescriptor *newEntries;
    TZrUInt32 newCapacity;
    TZrSize bytes;

    if (state == ZR_NULL || prototype == ZR_NULL || descriptor == ZR_NULL) {
        return ZR_FALSE;
    }

    if (prototype->memberDescriptorCount >= prototype->memberDescriptorCapacity) {
        newCapacity = prototype->memberDescriptorCapacity > 0
                              ? prototype->memberDescriptorCapacity * ZR_RUNTIME_OBJECT_PROTOTYPE_GROWTH_FACTOR
                              : ZR_RUNTIME_OBJECT_PROTOTYPE_INITIAL_CAPACITY;
        bytes = sizeof(SZrMemberDescriptor) * newCapacity;
        newEntries = (SZrMemberDescriptor *)ZrCore_Memory_RawMallocWithType(state->global,
                                                                             bytes,
                                                                             ZR_MEMORY_NATIVE_TYPE_OBJECT);
        if (newEntries == ZR_NULL) {
            return ZR_FALSE;
        }

        memset(newEntries, 0, bytes);
        if (prototype->memberDescriptors != ZR_NULL && prototype->memberDescriptorCount > 0) {
            memcpy(newEntries,
                   prototype->memberDescriptors,
                   sizeof(SZrMemberDescriptor) * prototype->memberDescriptorCount);
            ZrCore_Memory_RawFreeWithType(state->global,
                                          prototype->memberDescriptors,
                                          sizeof(SZrMemberDescriptor) * prototype->memberDescriptorCapacity,
                                          ZR_MEMORY_NATIVE_TYPE_OBJECT);
        }

        prototype->memberDescriptors = newEntries;
        prototype->memberDescriptorCapacity = newCapacity;
    }

    prototype->memberDescriptors[prototype->memberDescriptorCount++] = *descriptor;
    prototype->super.memberVersion++;
    return ZR_TRUE;
}

TZrBool ZrCore_Object_GetMember(struct SZrState *state,
                                SZrTypeValue *receiver,
                                struct SZrString *memberName,
                                SZrTypeValue *result) {
    SZrObject *object;
    SZrObjectPrototype *prototype;
    SZrTypeValue key;
    const SZrTypeValue *resolvedValue;
    const SZrMemberDescriptor *descriptor;
    TZrBool isPrototypeReceiver;

    if (state == ZR_NULL || receiver == ZR_NULL || memberName == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (receiver->type != ZR_VALUE_TYPE_OBJECT && receiver->type != ZR_VALUE_TYPE_ARRAY) {
        return ZR_FALSE;
    }

    object = ZR_CAST_OBJECT(state, receiver->value.object);
    if (object == ZR_NULL || !object_make_string_key(state, memberName, &key)) {
        return ZR_FALSE;
    }

    resolvedValue = object_get_own_value(state, object, &key);
    if (resolvedValue != ZR_NULL) {
        ZrCore_Value_Copy(state, result, resolvedValue);
        return ZR_TRUE;
    }

    prototype = object->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE
                         ? (SZrObjectPrototype *)object
                         : object_value_resolve_prototype(state, receiver);
    descriptor = prototype != ZR_NULL
                         ? ZrCore_ObjectPrototype_FindMemberDescriptor(prototype, memberName, ZR_TRUE)
                         : ZR_NULL;
    isPrototypeReceiver = object->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE ? ZR_TRUE : ZR_FALSE;

    if (descriptor != ZR_NULL) {
        if (descriptor->isStatic && !isPrototypeReceiver) {
            return ZR_FALSE;
        }

        if (descriptor->kind == ZR_MEMBER_DESCRIPTOR_KIND_PROPERTY &&
            descriptor->getterFunction != ZR_NULL) {
            return object_call_function_with_receiver(state,
                                                     descriptor->getterFunction,
                                                     receiver,
                                                     ZR_NULL,
                                                     0,
                                                     result);
        }

        if (descriptor->kind == ZR_MEMBER_DESCRIPTOR_KIND_PROPERTY &&
            descriptor->contractRole == ZR_MEMBER_CONTRACT_ROLE_INDEX_LENGTH) {
            return object_get_index_length(state, receiver, result);
        }
    }

    resolvedValue = prototype != ZR_NULL ? object_get_prototype_value(state, prototype, &key, ZR_TRUE) : ZR_NULL;
    if (resolvedValue != ZR_NULL) {
        if (descriptor == ZR_NULL || !descriptor->isStatic || isPrototypeReceiver) {
            ZrCore_Value_Copy(state, result, resolvedValue);
            return ZR_TRUE;
        }
        return ZR_FALSE;
    }

    ZrCore_Value_ResetAsNull(result);
    return ZR_FALSE;
}

TZrBool ZrCore_Object_SetMember(struct SZrState *state,
                                SZrTypeValue *receiver,
                                struct SZrString *memberName,
                                const SZrTypeValue *value) {
    SZrObject *object;
    SZrObjectPrototype *prototype;
    SZrTypeValue key;
    const SZrTypeValue *existingValue;
    const SZrMemberDescriptor *descriptor;
    const SZrManagedFieldInfo *managedField;
    const SZrTypeValue *prototypeValue;
    TZrBool isPrototypeReceiver;

    if (state == ZR_NULL || receiver == ZR_NULL || memberName == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (receiver->type != ZR_VALUE_TYPE_OBJECT && receiver->type != ZR_VALUE_TYPE_ARRAY) {
        return ZR_FALSE;
    }

    object = ZR_CAST_OBJECT(state, receiver->value.object);
    if (object == ZR_NULL || !object_make_string_key(state, memberName, &key)) {
        return ZR_FALSE;
    }

    prototype = object->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE
                         ? (SZrObjectPrototype *)object
                         : object_value_resolve_prototype(state, receiver);
    existingValue = object_get_own_value(state, object, &key);
    descriptor = prototype != ZR_NULL
                         ? ZrCore_ObjectPrototype_FindMemberDescriptor(prototype, memberName, ZR_TRUE)
                         : ZR_NULL;
    managedField = prototype != ZR_NULL
                           ? object_prototype_find_managed_field(prototype, memberName, ZR_TRUE)
                           : ZR_NULL;
    prototypeValue = prototype != ZR_NULL ? object_get_prototype_value(state, prototype, &key, ZR_TRUE) : ZR_NULL;
    isPrototypeReceiver = object->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE ? ZR_TRUE : ZR_FALSE;

    if (descriptor != ZR_NULL) {
        if (descriptor->isStatic && !isPrototypeReceiver) {
            return ZR_FALSE;
        }

        if (descriptor->kind == ZR_MEMBER_DESCRIPTOR_KIND_PROPERTY &&
            descriptor->setterFunction != ZR_NULL) {
            SZrTypeValue ignoredResult;
            return object_call_function_with_receiver(state,
                                                     descriptor->setterFunction,
                                                     receiver,
                                                     value,
                                                     1,
                                                     &ignoredResult);
        }

        if (descriptor->kind == ZR_MEMBER_DESCRIPTOR_KIND_FIELD ||
            descriptor->kind == ZR_MEMBER_DESCRIPTOR_KIND_STATIC_MEMBER) {
            if (!descriptor->isWritable) {
                return ZR_FALSE;
            }

            ZrCore_Object_SetValue(state,
                                   descriptor->isStatic && prototype != ZR_NULL ? &prototype->super : object,
                                   &key,
                                   value);
            return ZR_TRUE;
        }
    }

    if (managedField != ZR_NULL || existingValue != ZR_NULL) {
        ZrCore_Object_SetValue(state, object, &key, value);
        return ZR_TRUE;
    }

    if (prototypeValue != ZR_NULL) {
        return ZR_FALSE;
    }

    if (!object_allows_dynamic_member_write(object)) {
        return ZR_FALSE;
    }

    ZrCore_Object_SetValue(state, object, &key, value);
    return ZR_TRUE;
}

TZrBool ZrCore_Object_InvokeMember(struct SZrState *state,
                                   SZrTypeValue *receiver,
                                   struct SZrString *memberName,
                                   const SZrTypeValue *arguments,
                                   TZrSize argumentCount,
                                   SZrTypeValue *result) {
    SZrObject *object;
    SZrObjectPrototype *prototype;
    const SZrMemberDescriptor *descriptor;
    SZrTypeValue callableValue;

    if (state == ZR_NULL || receiver == ZR_NULL || memberName == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (receiver->type != ZR_VALUE_TYPE_OBJECT && receiver->type != ZR_VALUE_TYPE_ARRAY) {
        return ZR_FALSE;
    }

    object = ZR_CAST_OBJECT(state, receiver->value.object);
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    prototype = object->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE
                         ? (SZrObjectPrototype *)object
                         : object_value_resolve_prototype(state, receiver);
    descriptor = prototype != ZR_NULL
                         ? ZrCore_ObjectPrototype_FindMemberDescriptor(prototype, memberName, ZR_TRUE)
                         : ZR_NULL;

    ZrCore_Value_ResetAsNull(&callableValue);
    if (!ZrCore_Object_GetMember(state, receiver, memberName, &callableValue)) {
        return ZR_FALSE;
    }

    return object_call_value(state,
                             &callableValue,
                             (descriptor != ZR_NULL && descriptor->isStatic) ? ZR_NULL : receiver,
                             arguments,
                             argumentCount,
                             result);
}

TZrBool ZrCore_Object_ResolveMemberCallable(struct SZrState *state,
                                            SZrTypeValue *receiver,
                                            struct SZrString *memberName,
                                            struct SZrObjectPrototype **outOwnerPrototype,
                                            struct SZrFunction **outFunction,
                                            TZrBool *outIsStatic) {
    SZrObject *object;
    SZrObjectPrototype *prototype;
    SZrTypeValue key;
    TZrBool isPrototypeReceiver;

    if (outOwnerPrototype != ZR_NULL) {
        *outOwnerPrototype = ZR_NULL;
    }
    if (outFunction != ZR_NULL) {
        *outFunction = ZR_NULL;
    }
    if (outIsStatic != ZR_NULL) {
        *outIsStatic = ZR_FALSE;
    }

    if (state == ZR_NULL || receiver == ZR_NULL || memberName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (receiver->type != ZR_VALUE_TYPE_OBJECT && receiver->type != ZR_VALUE_TYPE_ARRAY) {
        return ZR_FALSE;
    }

    object = ZR_CAST_OBJECT(state, receiver->value.object);
    if (object == ZR_NULL || !object_make_string_key(state, memberName, &key)) {
        return ZR_FALSE;
    }

    isPrototypeReceiver = object->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE ? ZR_TRUE : ZR_FALSE;
    prototype = isPrototypeReceiver ? (SZrObjectPrototype *)object : object_value_resolve_prototype(state, receiver);
    while (prototype != ZR_NULL) {
        const SZrMemberDescriptor *descriptor =
                ZrCore_ObjectPrototype_FindMemberDescriptor(prototype, memberName, ZR_FALSE);
        const SZrTypeValue *resolvedValue = object_get_own_value(state, &prototype->super, &key);

        if (descriptor != ZR_NULL && descriptor->isStatic && !isPrototypeReceiver) {
            return ZR_FALSE;
        }

        if (resolvedValue != ZR_NULL &&
            (resolvedValue->type == ZR_VALUE_TYPE_FUNCTION || resolvedValue->type == ZR_VALUE_TYPE_CLOSURE) &&
            resolvedValue->value.object != ZR_NULL) {
            if (outOwnerPrototype != ZR_NULL) {
                *outOwnerPrototype = prototype;
            }
            if (outFunction != ZR_NULL) {
                *outFunction = ZR_CAST(SZrFunction *, resolvedValue->value.object);
            }
            if (outIsStatic != ZR_NULL) {
                *outIsStatic = (descriptor != ZR_NULL && descriptor->isStatic) ? ZR_TRUE : ZR_FALSE;
            }
            return ZR_TRUE;
        }

        prototype = prototype->superPrototype;
    }

    return ZR_FALSE;
}

TZrBool ZrCore_Object_InvokeResolvedFunction(struct SZrState *state,
                                             struct SZrFunction *function,
                                             TZrBool isStatic,
                                             SZrTypeValue *receiver,
                                             const SZrTypeValue *arguments,
                                             TZrSize argumentCount,
                                             SZrTypeValue *result) {
    if (state == ZR_NULL || function == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    return object_call_function_with_receiver(state,
                                              function,
                                              isStatic ? ZR_NULL : receiver,
                                              arguments,
                                              argumentCount,
                                              result);
}

TZrBool ZrCore_Object_GetByIndex(struct SZrState *state,
                                 SZrTypeValue *receiver,
                                 const SZrTypeValue *key,
                                 SZrTypeValue *result) {
    SZrObject *object;
    SZrObjectPrototype *prototype;
    const SZrTypeValue *resolvedValue;

    if (state == ZR_NULL || receiver == ZR_NULL || key == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (receiver->type != ZR_VALUE_TYPE_OBJECT && receiver->type != ZR_VALUE_TYPE_ARRAY) {
        return ZR_FALSE;
    }

    object = ZR_CAST_OBJECT(state, receiver->value.object);
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    prototype = object_value_resolve_prototype(state, receiver);
    if (prototype != ZR_NULL && prototype->indexContract.getByIndexFunction != ZR_NULL) {
        return object_call_function_with_receiver(state,
                                                  prototype->indexContract.getByIndexFunction,
                                                  receiver,
                                                  key,
                                                  1,
                                                  result);
    }

    if (!object_can_use_direct_index_fallback(object)) {
        return ZR_FALSE;
    }

    resolvedValue = object_get_own_value(state, object, key);
    if (resolvedValue == ZR_NULL) {
        ZrCore_Value_ResetAsNull(result);
    } else {
        ZrCore_Value_Copy(state, result, resolvedValue);
    }
    return ZR_TRUE;
}

TZrBool ZrCore_Object_SetByIndex(struct SZrState *state,
                                 SZrTypeValue *receiver,
                                 const SZrTypeValue *key,
                                 const SZrTypeValue *value) {
    SZrObject *object;
    SZrObjectPrototype *prototype;

    if (state == ZR_NULL || receiver == ZR_NULL || key == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (receiver->type != ZR_VALUE_TYPE_OBJECT && receiver->type != ZR_VALUE_TYPE_ARRAY) {
        return ZR_FALSE;
    }

    object = ZR_CAST_OBJECT(state, receiver->value.object);
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    prototype = object_value_resolve_prototype(state, receiver);
    if (prototype != ZR_NULL && prototype->indexContract.setByIndexFunction != ZR_NULL) {
        SZrTypeValue arguments[2];
        SZrTypeValue ignoredResult;
        arguments[0] = *key;
        arguments[1] = *value;
        return object_call_function_with_receiver(state,
                                                  prototype->indexContract.setByIndexFunction,
                                                  receiver,
                                                  arguments,
                                                  2,
                                                  &ignoredResult);
    }

    if (!object_can_use_direct_index_fallback(object)) {
        return ZR_FALSE;
    }

    ZrCore_Object_SetValue(state, object, key, value);
    return ZR_TRUE;
}

TZrBool ZrCore_Object_IterInit(struct SZrState *state,
                               SZrTypeValue *iterableValue,
                               SZrTypeValue *result) {
    SZrObject *iteratorObject;
    SZrObjectPrototype *prototype;
    SZrTypeValue indexValue;
    SZrTypeValue currentValue;
    SZrTypeValue currentValidValue;

    if (state == ZR_NULL || iterableValue == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (iterableValue->type != ZR_VALUE_TYPE_ARRAY && iterableValue->type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_FALSE;
    }

    prototype = object_value_resolve_prototype(state, iterableValue);
    if (iterableValue->type != ZR_VALUE_TYPE_ARRAY &&
        prototype != ZR_NULL &&
        prototype->iterableContract.iterInitFunction != ZR_NULL) {
        if (object_call_function_with_receiver(state,
                                              prototype->iterableContract.iterInitFunction,
                                              iterableValue,
                                              ZR_NULL,
                                              0,
                                              result)) {
            return ZR_TRUE;
        }
    }

    if (iterableValue->type != ZR_VALUE_TYPE_ARRAY) {
        return ZR_FALSE;
    }

    iteratorObject = ZrCore_Object_New(state, ZR_NULL);
    if (iteratorObject == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Object_Init(state, iteratorObject);

    ZrCore_Value_InitAsInt(state, &indexValue, -1);
    ZrCore_Value_ResetAsNull(&currentValue);
    ZrCore_Value_InitAsBool(state, &currentValidValue, ZR_FALSE);

    if (!object_hidden_set(state, iteratorObject, "__zr_iter_source", iterableValue) ||
        !object_hidden_set(state, iteratorObject, "__zr_iter_index", &indexValue) ||
        !object_hidden_set(state, iteratorObject, "__zr_iter_current", &currentValue) ||
        !object_hidden_set(state, iteratorObject, "__zr_iter_has_current", &currentValidValue)) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(iteratorObject));
    result->type = ZR_VALUE_TYPE_OBJECT;
    return ZR_TRUE;
}

TZrBool ZrCore_Object_IterMoveNext(struct SZrState *state,
                                   SZrTypeValue *iteratorValue,
                                   SZrTypeValue *result) {
    SZrObject *iteratorObject;
    SZrObjectPrototype *prototype;
    SZrTypeValue sourceValue;
    SZrTypeValue indexValue;
    SZrTypeValue nextIndexValue;
    SZrTypeValue currentValue;
    SZrTypeValue hasCurrentValue;
    const SZrTypeValue *resolvedValue;
    SZrObject *sourceObject;

    if (state == ZR_NULL || iteratorValue == ZR_NULL || result == ZR_NULL ||
        iteratorValue->type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_FALSE;
    }

    iteratorObject = ZR_CAST_OBJECT(state, iteratorValue->value.object);
    if (iteratorObject == ZR_NULL) {
        return ZR_FALSE;
    }
    prototype = object_value_resolve_prototype(state, iteratorValue);
    if (prototype != ZR_NULL && prototype->iteratorContract.moveNextFunction != ZR_NULL) {
        if (object_call_function_with_receiver(state,
                                              prototype->iteratorContract.moveNextFunction,
                                              iteratorValue,
                                              ZR_NULL,
                                              0,
                                              result)) {
            return ZR_TRUE;
        }
    }

    if (!object_hidden_get(state, iteratorObject, "__zr_iter_source", &sourceValue) ||
        !object_hidden_get(state, iteratorObject, "__zr_iter_index", &indexValue)) {
        return ZR_FALSE;
    }

    if (sourceValue.type != ZR_VALUE_TYPE_ARRAY && sourceValue.type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsInt(state, &nextIndexValue, indexValue.value.nativeObject.nativeInt64 + 1);
    sourceObject = ZR_CAST_OBJECT(state, sourceValue.value.object);
    resolvedValue = ZrCore_Object_GetValue(state, sourceObject, &nextIndexValue);
    if (resolvedValue == ZR_NULL) {
        ZrCore_Value_ResetAsNull(&currentValue);
        ZrCore_Value_InitAsBool(state, &hasCurrentValue, ZR_FALSE);
        object_hidden_set(state, iteratorObject, "__zr_iter_current", &currentValue);
        object_hidden_set(state, iteratorObject, "__zr_iter_has_current", &hasCurrentValue);
        ZrCore_Value_InitAsBool(state, result, ZR_FALSE);
        return ZR_TRUE;
    }

    // Value copy is ownership-aware, so the destination must be initialized first.
    ZrCore_Value_ResetAsNull(&currentValue);
    ZrCore_Value_Copy(state, &currentValue, resolvedValue);
    ZrCore_Value_InitAsBool(state, &hasCurrentValue, ZR_TRUE);
    object_hidden_set(state, iteratorObject, "__zr_iter_index", &nextIndexValue);
    object_hidden_set(state, iteratorObject, "__zr_iter_current", &currentValue);
    object_hidden_set(state, iteratorObject, "__zr_iter_has_current", &hasCurrentValue);
    ZrCore_Value_InitAsBool(state, result, ZR_TRUE);
    return ZR_TRUE;
}

TZrBool ZrCore_Object_IterCurrent(struct SZrState *state,
                                  SZrTypeValue *iteratorValue,
                                  SZrTypeValue *result) {
    SZrObject *iteratorObject;
    SZrObjectPrototype *prototype;
    SZrTypeValue currentValue;

    if (state == ZR_NULL || iteratorValue == ZR_NULL || result == ZR_NULL ||
        iteratorValue->type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_FALSE;
    }

    iteratorObject = ZR_CAST_OBJECT(state, iteratorValue->value.object);
    if (iteratorObject == ZR_NULL) {
        return ZR_FALSE;
    }

    prototype = object_value_resolve_prototype(state, iteratorValue);
    if (prototype != ZR_NULL) {
        if (prototype->iteratorContract.currentFunction != ZR_NULL) {
            if (object_call_function_with_receiver(state,
                                                  prototype->iteratorContract.currentFunction,
                                                  iteratorValue,
                                                  ZR_NULL,
                                                  0,
                                                  result)) {
                return ZR_TRUE;
            }
        }

        if (prototype->iteratorContract.currentMemberName != ZR_NULL) {
            if (ZrCore_Object_GetMember(state,
                                        iteratorValue,
                                        prototype->iteratorContract.currentMemberName,
                                        result)) {
                return ZR_TRUE;
            }
        }
    }

    if (!object_hidden_get(state, iteratorObject, "__zr_iter_current", &currentValue)) {
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, result, &currentValue);
    return ZR_TRUE;
}

void ZrCore_ObjectPrototype_SetIndexContract(SZrObjectPrototype *prototype,
                                             const SZrIndexContract *contract) {
    if (prototype == ZR_NULL) {
        return;
    }

    if (contract == ZR_NULL) {
        memset(&prototype->indexContract, 0, sizeof(prototype->indexContract));
        return;
    }

    prototype->indexContract = *contract;
}

void ZrCore_ObjectPrototype_SetIterableContract(SZrObjectPrototype *prototype,
                                                const SZrIterableContract *contract) {
    if (prototype == ZR_NULL) {
        return;
    }

    if (contract == ZR_NULL) {
        memset(&prototype->iterableContract, 0, sizeof(prototype->iterableContract));
        return;
    }

    prototype->iterableContract = *contract;
}

void ZrCore_ObjectPrototype_SetIteratorContract(SZrObjectPrototype *prototype,
                                                const SZrIteratorContract *contract) {
    if (prototype == ZR_NULL) {
        return;
    }

    if (contract == ZR_NULL) {
        memset(&prototype->iteratorContract, 0, sizeof(prototype->iteratorContract));
        return;
    }

    prototype->iteratorContract = *contract;
}

void ZrCore_ObjectPrototype_AddProtocol(SZrObjectPrototype *prototype, EZrProtocolId protocolId) {
    if (prototype == ZR_NULL || protocolId <= ZR_PROTOCOL_ID_NONE) {
        return;
    }

    prototype->protocolMask |= ZR_PROTOCOL_BIT(protocolId);
}

TZrBool ZrCore_ObjectPrototype_ImplementsProtocol(SZrObjectPrototype *prototype, EZrProtocolId protocolId) {
    if (prototype == ZR_NULL || protocolId <= ZR_PROTOCOL_ID_NONE) {
        return ZR_FALSE;
    }

    return (prototype->protocolMask & ZR_PROTOCOL_BIT(protocolId)) != 0;
}

TZrBool ZrCore_Object_IsInstanceOfPrototype(SZrObject *object, SZrObjectPrototype *prototype) {
    SZrObjectPrototype *currentPrototype;

    if (object == ZR_NULL || prototype == ZR_NULL) {
        return ZR_FALSE;
    }

    currentPrototype = object->prototype;
    while (currentPrototype != ZR_NULL) {
        if (currentPrototype == prototype) {
            return ZR_TRUE;
        }
        currentPrototype = currentPrototype->superPrototype;
    }

    return ZR_FALSE;
}
