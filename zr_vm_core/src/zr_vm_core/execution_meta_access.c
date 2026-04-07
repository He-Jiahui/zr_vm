//
// Split from execution_dispatch.c: META_GET / META_SET helpers.
//

#include "execution_internal.h"

static SZrString *execution_meta_cache_resolve_member_name(SZrFunction *function, TZrUInt32 memberEntryIndex) {
    if (function == ZR_NULL || function->memberEntries == ZR_NULL || memberEntryIndex >= function->memberEntryLength) {
        return ZR_NULL;
    }

    return function->memberEntries[memberEntryIndex].symbol;
}

static SZrObjectPrototype *execution_meta_resolve_receiver_prototype(SZrState *state, const SZrTypeValue *receiver) {
    SZrObject *object;

    if (state == ZR_NULL || receiver == ZR_NULL) {
        return ZR_NULL;
    }

    if ((receiver->type != ZR_VALUE_TYPE_OBJECT &&
         receiver->type != ZR_VALUE_TYPE_ARRAY &&
         receiver->type != ZR_VALUE_TYPE_STRING) ||
        receiver->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    object = (receiver->type == ZR_VALUE_TYPE_OBJECT || receiver->type == ZR_VALUE_TYPE_ARRAY)
                     ? ZR_CAST_OBJECT(state, receiver->value.object)
                     : ZR_NULL;

    if (object != ZR_NULL && object->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE) {
        return (SZrObjectPrototype *)object;
    }

    if (object != ZR_NULL && object->prototype != ZR_NULL) {
        return object->prototype;
    }

    if (state->global == ZR_NULL || receiver->type >= ZR_VALUE_TYPE_ENUM_MAX) {
        return ZR_NULL;
    }

    return state->global->basicTypeObjectPrototype[receiver->type];
}

const SZrFunctionCallSiteCacheEntry *execution_get_callsite_cache_entry(SZrFunction *function,
                                                                        TZrUInt16 cacheIndex,
                                                                        EZrFunctionCallSiteCacheKind expectedKind) {
    const SZrFunctionCallSiteCacheEntry *entry;

    if (function == ZR_NULL || function->callSiteCaches == ZR_NULL || cacheIndex >= function->callSiteCacheLength) {
        return ZR_NULL;
    }

    entry = &function->callSiteCaches[cacheIndex];
    if ((EZrFunctionCallSiteCacheKind)entry->kind != expectedKind) {
        return ZR_NULL;
    }

    return entry;
}

static SZrFunctionCallSiteCacheEntry *execution_meta_get_cache_entry(SZrFunction *function,
                                                                     TZrUInt16 cacheIndex,
                                                                     EZrFunctionCallSiteCacheKind expectedKind) {
    return (SZrFunctionCallSiteCacheEntry *)execution_get_callsite_cache_entry(function, cacheIndex, expectedKind);
}

static void execution_meta_clear_pic_slot(SZrFunctionCallSitePicSlot *slot) {
    if (slot == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(slot, 0, sizeof(*slot));
    slot->cachedDescriptorIndex = ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE;
}

static void execution_meta_clear_cache_entry(SZrFunctionCallSiteCacheEntry *entry) {
    TZrUInt32 index;

    if (entry == ZR_NULL) {
        return;
    }

    entry->picSlotCount = 0;
    entry->picNextInsertIndex = 0;
    for (index = 0; index < ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY; index++) {
        execution_meta_clear_pic_slot(&entry->picSlots[index]);
    }
}

static TZrUInt32 execution_meta_find_descriptor_index(SZrObjectPrototype *prototype, SZrString *memberName) {
    TZrUInt32 index;

    if (prototype == ZR_NULL || prototype->memberDescriptors == ZR_NULL || memberName == ZR_NULL) {
        return ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE;
    }

    for (index = 0; index < prototype->memberDescriptorCount; index++) {
        SZrMemberDescriptor *descriptor = &prototype->memberDescriptors[index];
        if (descriptor->name != ZR_NULL && ZrCore_String_Equal(descriptor->name, memberName)) {
            return index;
        }
    }

    return ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE;
}

static TZrBool execution_meta_resolve_call_target(SZrState *state,
                                                  SZrTypeValue *receiver,
                                                  SZrObjectPrototype **outReceiverPrototype,
                                                  SZrObjectPrototype **outOwnerPrototype,
                                                  SZrFunction **outFunction) {
    SZrObjectPrototype *receiverPrototype;
    SZrObjectPrototype *ownerPrototype;

    if (outReceiverPrototype != ZR_NULL) {
        *outReceiverPrototype = ZR_NULL;
    }
    if (outOwnerPrototype != ZR_NULL) {
        *outOwnerPrototype = ZR_NULL;
    }
    if (outFunction != ZR_NULL) {
        *outFunction = ZR_NULL;
    }

    if (state == ZR_NULL || receiver == ZR_NULL) {
        return ZR_FALSE;
    }

    receiverPrototype = execution_meta_resolve_receiver_prototype(state, receiver);
    if (receiverPrototype == ZR_NULL) {
        return ZR_FALSE;
    }

    ownerPrototype = receiverPrototype;
    while (ownerPrototype != ZR_NULL) {
        SZrMeta *meta = ownerPrototype->metaTable.metas[ZR_META_CALL];
        if (meta != ZR_NULL && meta->function != ZR_NULL) {
            if (outReceiverPrototype != ZR_NULL) {
                *outReceiverPrototype = receiverPrototype;
            }
            if (outOwnerPrototype != ZR_NULL) {
                *outOwnerPrototype = ownerPrototype;
            }
            if (outFunction != ZR_NULL) {
                *outFunction = meta->function;
            }
            return ZR_TRUE;
        }
        ownerPrototype = ownerPrototype->superPrototype;
    }

    return ZR_FALSE;
}

static void execution_meta_store_pic_slot(SZrFunctionCallSiteCacheEntry *entry,
                                          SZrObjectPrototype *receiverPrototype,
                                          SZrObjectPrototype *ownerPrototype,
                                          SZrFunction *function,
                                          TZrUInt32 descriptorIndex,
                                          TZrBool isStatic) {
    TZrUInt32 slotIndex;
    SZrFunctionCallSitePicSlot *slot;

    if (entry == ZR_NULL || receiverPrototype == ZR_NULL || function == ZR_NULL) {
        return;
    }

    for (slotIndex = 0; slotIndex < entry->picSlotCount; slotIndex++) {
        slot = &entry->picSlots[slotIndex];
        if (slot->cachedReceiverPrototype == receiverPrototype) {
            slot->cachedOwnerPrototype = ownerPrototype;
            slot->cachedFunction = function;
            slot->cachedReceiverVersion = receiverPrototype->super.memberVersion;
            slot->cachedOwnerVersion = ownerPrototype != ZR_NULL ? ownerPrototype->super.memberVersion : 0;
            slot->cachedDescriptorIndex = descriptorIndex;
            slot->cachedIsStatic = isStatic ? ZR_TRUE : ZR_FALSE;
            return;
        }
    }

    if (entry->picSlotCount < ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY) {
        slotIndex = entry->picSlotCount++;
    } else {
        slotIndex = entry->picNextInsertIndex % ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY;
    }
    entry->picNextInsertIndex = (slotIndex + 1) % ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY;

    slot = &entry->picSlots[slotIndex];
    execution_meta_clear_pic_slot(slot);
    slot->cachedReceiverPrototype = receiverPrototype;
    slot->cachedOwnerPrototype = ownerPrototype;
    slot->cachedFunction = function;
    slot->cachedReceiverVersion = receiverPrototype->super.memberVersion;
    slot->cachedOwnerVersion = ownerPrototype != ZR_NULL ? ownerPrototype->super.memberVersion : 0;
    slot->cachedDescriptorIndex = descriptorIndex;
    slot->cachedIsStatic = isStatic ? ZR_TRUE : ZR_FALSE;
}

static void execution_meta_shift_arguments_and_store_callable(SZrState *state,
                                                              TZrStackValuePointer stackPointer,
                                                              SZrFunction *function) {
    TZrStackValuePointer p;
    SZrTypeValue *value;

    if (state == ZR_NULL || stackPointer == ZR_NULL || function == ZR_NULL) {
        return;
    }

    for (p = state->stackTop.valuePointer; p > stackPointer; p--) {
        ZrCore_Stack_CopyValue(state, p, ZrCore_Stack_GetValue(p - 1));
    }
    state->stackTop.valuePointer++;
    value = ZrCore_Stack_GetValue(stackPointer);
    ZrCore_Value_InitAsRawObject(state, value, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
}

static void execution_meta_refresh_cache(SZrState *state,
                                         SZrFunctionCallSiteCacheEntry *entry,
                                         SZrTypeValue *receiver,
                                         SZrString *memberName) {
    SZrObjectPrototype *receiverPrototype = ZR_NULL;
    SZrObjectPrototype *ownerPrototype = ZR_NULL;
    SZrFunction *function = ZR_NULL;
    TZrBool isStatic = ZR_FALSE;

    if (state == ZR_NULL || entry == ZR_NULL || receiver == ZR_NULL || memberName == ZR_NULL) {
        return;
    }

    receiverPrototype = execution_meta_resolve_receiver_prototype(state, receiver);
    if (receiverPrototype == ZR_NULL) {
        return;
    }
    if (!ZrCore_Object_ResolveMemberCallable(state, receiver, memberName, &ownerPrototype, &function, &isStatic)) {
        return;
    }

    execution_meta_store_pic_slot(entry,
                                  receiverPrototype,
                                  ownerPrototype,
                                  function,
                                  execution_meta_find_descriptor_index(ownerPrototype, memberName),
                                  isStatic);
}

static TZrBool execution_meta_try_cached_call(SZrState *state,
                                              SZrFunctionCallSiteCacheEntry *entry,
                                              SZrTypeValue *receiver,
                                              const SZrTypeValue *arguments,
                                              TZrSize argumentCount,
                                              SZrTypeValue *result,
                                              TZrBool requireStaticMode,
                                              TZrBool expectedStatic) {
    SZrObjectPrototype *receiverPrototype;
    TZrUInt32 slotIndex;

    if (state == ZR_NULL || entry == ZR_NULL || receiver == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (entry->picSlotCount == 0) {
        return ZR_FALSE;
    }

    receiverPrototype = execution_meta_resolve_receiver_prototype(state, receiver);
    if (receiverPrototype == ZR_NULL) {
        return ZR_FALSE;
    }

    for (slotIndex = 0; slotIndex < entry->picSlotCount; slotIndex++) {
        SZrFunctionCallSitePicSlot *slot = &entry->picSlots[slotIndex];

        if (slot->cachedReceiverPrototype != receiverPrototype) {
            continue;
        }
        if (slot->cachedOwnerPrototype == ZR_NULL || slot->cachedFunction == ZR_NULL) {
            execution_meta_clear_cache_entry(entry);
            return ZR_FALSE;
        }
        if (slot->cachedReceiverPrototype->super.memberVersion != slot->cachedReceiverVersion ||
            slot->cachedOwnerPrototype->super.memberVersion != slot->cachedOwnerVersion) {
            execution_meta_clear_cache_entry(entry);
            return ZR_FALSE;
        }
        if (requireStaticMode &&
            ((slot->cachedIsStatic ? ZR_TRUE : ZR_FALSE) != (expectedStatic ? ZR_TRUE : ZR_FALSE))) {
            execution_meta_clear_cache_entry(entry);
            return ZR_FALSE;
        }
        if (!ZrCore_Object_InvokeResolvedFunction(state,
                                                  slot->cachedFunction,
                                                  slot->cachedIsStatic ? ZR_TRUE : ZR_FALSE,
                                                  receiver,
                                                  arguments,
                                                  argumentCount,
                                                  result)) {
            execution_meta_clear_cache_entry(entry);
            return ZR_FALSE;
        }

        entry->runtimeHitCount++;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static void execution_meta_validate_cache_static_mode(SZrFunctionCallSiteCacheEntry *entry,
                                                      TZrBool requireStaticMode,
                                                      TZrBool expectedStatic) {
    TZrUInt32 slotIndex;

    if (entry == ZR_NULL || !requireStaticMode) {
        return;
    }

    for (slotIndex = 0; slotIndex < entry->picSlotCount; slotIndex++) {
        if ((entry->picSlots[slotIndex].cachedIsStatic ? ZR_TRUE : ZR_FALSE) !=
            (expectedStatic ? ZR_TRUE : ZR_FALSE)) {
            execution_meta_clear_cache_entry(entry);
            return;
        }
    }
}

static TZrBool execution_meta_prepare_cached_call_target_internal(SZrState *state,
                                                                  SZrFunction *function,
                                                                  TZrUInt16 cacheIndex,
                                                                  TZrStackValuePointer stackPointer,
                                                                  EZrFunctionCallSiteCacheKind expectedKind,
                                                                  TZrBool allowFallbackIfNoMeta) {
    SZrFunctionCallSiteCacheEntry *entry;
    SZrTypeValue *receiver;
    SZrObjectPrototype *receiverPrototype;
    SZrObjectPrototype *ownerPrototype;
    SZrFunction *resolvedFunction;
    TZrUInt32 slotIndex;

    if (state == ZR_NULL || function == ZR_NULL || stackPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    entry = execution_meta_get_cache_entry(function, cacheIndex, expectedKind);
    if (entry == ZR_NULL) {
        return ZR_FALSE;
    }

    receiver = ZrCore_Stack_GetValue(stackPointer);
    if (receiver == ZR_NULL) {
        return ZR_FALSE;
    }

    receiverPrototype = execution_meta_resolve_receiver_prototype(state, receiver);
    if (receiverPrototype != ZR_NULL && entry->picSlotCount > 0) {
        for (slotIndex = 0; slotIndex < entry->picSlotCount; slotIndex++) {
            SZrFunctionCallSitePicSlot *slot = &entry->picSlots[slotIndex];

            if (slot->cachedReceiverPrototype != receiverPrototype) {
                continue;
            }
            if (slot->cachedOwnerPrototype == ZR_NULL || slot->cachedFunction == ZR_NULL) {
                execution_meta_clear_cache_entry(entry);
                return ZR_FALSE;
            }
            if (slot->cachedReceiverPrototype->super.memberVersion != slot->cachedReceiverVersion ||
                slot->cachedOwnerPrototype->super.memberVersion != slot->cachedOwnerVersion) {
                execution_meta_clear_cache_entry(entry);
                return ZR_FALSE;
            }

            execution_meta_shift_arguments_and_store_callable(state, stackPointer, slot->cachedFunction);
            entry->runtimeHitCount++;
            return ZR_TRUE;
        }
    }

    if (!execution_meta_resolve_call_target(state, receiver, &receiverPrototype, &ownerPrototype, &resolvedFunction)) {
        ZR_UNUSED_PARAMETER(allowFallbackIfNoMeta);
        return ZR_FALSE;
    }

    entry->runtimeMissCount++;
    execution_meta_shift_arguments_and_store_callable(state, stackPointer, resolvedFunction);
    execution_meta_store_pic_slot(entry,
                                  receiverPrototype,
                                  ownerPrototype,
                                  resolvedFunction,
                                  ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE,
                                  ZR_FALSE);
    return ZR_TRUE;
}

TZrBool execution_prepare_meta_call_target_cached(SZrState *state,
                                                  SZrFunction *function,
                                                  TZrUInt16 cacheIndex,
                                                  TZrStackValuePointer stackPointer,
                                                  EZrFunctionCallSiteCacheKind expectedKind) {
    return execution_meta_prepare_cached_call_target_internal(state,
                                                              function,
                                                              cacheIndex,
                                                              stackPointer,
                                                              expectedKind,
                                                              ZR_FALSE);
}

TZrBool execution_try_prepare_dyn_call_target_cached(SZrState *state,
                                                     SZrFunction *function,
                                                     TZrUInt16 cacheIndex,
                                                     TZrStackValuePointer stackPointer,
                                                     EZrFunctionCallSiteCacheKind expectedKind) {
    SZrTypeValue *callable;

    if (state == ZR_NULL || stackPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    callable = ZrCore_Stack_GetValue(stackPointer);
    if (callable == ZR_NULL ||
        (callable->type != ZR_VALUE_TYPE_OBJECT && callable->type != ZR_VALUE_TYPE_ARRAY)) {
        return ZR_FALSE;
    }

    return execution_meta_prepare_cached_call_target_internal(state,
                                                              function,
                                                              cacheIndex,
                                                              stackPointer,
                                                              expectedKind,
                                                              ZR_TRUE);
}

TZrBool execution_meta_get_member(SZrState *state,
                                  SZrTypeValue *receiver,
                                  SZrString *memberName,
                                  SZrTypeValue *result) {
    SZrTypeValue stableReceiver;

    if (state == ZR_NULL || receiver == ZR_NULL || memberName == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    stableReceiver = *receiver;
    return ZrCore_Object_InvokeMember(state, &stableReceiver, memberName, ZR_NULL, 0, result);
}

static TZrBool execution_meta_get_cached_member_internal(SZrState *state,
                                                         SZrFunction *function,
                                                         TZrUInt16 cacheIndex,
                                                         SZrTypeValue *receiver,
                                                         SZrTypeValue *result,
                                                         EZrFunctionCallSiteCacheKind expectedKind,
                                                         TZrBool expectedStatic) {
    SZrFunctionCallSiteCacheEntry *entry;
    SZrString *memberName;
    SZrTypeValue stableReceiver;

    if (state == ZR_NULL || function == ZR_NULL || receiver == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entry = execution_meta_get_cache_entry(function, cacheIndex, expectedKind);
    if (entry == ZR_NULL) {
        return ZR_FALSE;
    }

    if (execution_meta_try_cached_call(state, entry, receiver, ZR_NULL, 0, result, ZR_TRUE, expectedStatic)) {
        return ZR_TRUE;
    }

    memberName = execution_meta_cache_resolve_member_name(function, entry->memberEntryIndex);
    if (memberName == ZR_NULL) {
        return ZR_FALSE;
    }

    stableReceiver = *receiver;
    entry->runtimeMissCount++;
    if (!execution_meta_get_member(state, &stableReceiver, memberName, result)) {
        return ZR_FALSE;
    }

    execution_meta_refresh_cache(state, entry, &stableReceiver, memberName);
    execution_meta_validate_cache_static_mode(entry, ZR_TRUE, expectedStatic);
    return ZR_TRUE;
}

TZrBool execution_meta_get_cached_member(SZrState *state,
                                         SZrFunction *function,
                                         TZrUInt16 cacheIndex,
                                         SZrTypeValue *receiver,
                                         SZrTypeValue *result) {
    return execution_meta_get_cached_member_internal(state,
                                                     function,
                                                     cacheIndex,
                                                     receiver,
                                                     result,
                                                     ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET,
                                                     ZR_FALSE);
}

TZrBool execution_meta_get_cached_static_member(SZrState *state,
                                                SZrFunction *function,
                                                TZrUInt16 cacheIndex,
                                                SZrTypeValue *receiver,
                                                SZrTypeValue *result) {
    return execution_meta_get_cached_member_internal(state,
                                                     function,
                                                     cacheIndex,
                                                     receiver,
                                                     result,
                                                     ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET_STATIC,
                                                     ZR_TRUE);
}

TZrBool execution_meta_set_member(SZrState *state,
                                  SZrTypeValue *receiverAndResult,
                                  SZrString *memberName,
                                  const SZrTypeValue *assignedValue) {
    SZrTypeValue stableReceiver;
    SZrTypeValue stableAssignedValue;
    SZrTypeValue ignoredResult;

    if (state == ZR_NULL || receiverAndResult == ZR_NULL || memberName == ZR_NULL || assignedValue == ZR_NULL) {
        return ZR_FALSE;
    }

    stableReceiver = *receiverAndResult;
    stableAssignedValue = *assignedValue;
    ZrCore_Value_ResetAsNull(&ignoredResult);
    if (!ZrCore_Object_InvokeMember(state,
                                    &stableReceiver,
                                    memberName,
                                    &stableAssignedValue,
                                    1,
                                    &ignoredResult)) {
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, receiverAndResult, &stableReceiver);
    return ZR_TRUE;
}

static TZrBool execution_meta_set_cached_member_internal(SZrState *state,
                                                         SZrFunction *function,
                                                         TZrUInt16 cacheIndex,
                                                         SZrTypeValue *receiverAndResult,
                                                         const SZrTypeValue *assignedValue,
                                                         EZrFunctionCallSiteCacheKind expectedKind,
                                                         TZrBool expectedStatic) {
    SZrFunctionCallSiteCacheEntry *entry;
    SZrString *memberName;
    SZrTypeValue stableReceiver;
    SZrTypeValue stableAssignedValue;
    SZrTypeValue ignoredResult;

    if (state == ZR_NULL || function == ZR_NULL || receiverAndResult == ZR_NULL || assignedValue == ZR_NULL) {
        return ZR_FALSE;
    }

    entry = execution_meta_get_cache_entry(function, cacheIndex, expectedKind);
    if (entry == ZR_NULL) {
        return ZR_FALSE;
    }

    stableReceiver = *receiverAndResult;
    stableAssignedValue = *assignedValue;
    ZrCore_Value_ResetAsNull(&ignoredResult);
    if (execution_meta_try_cached_call(state,
                                       entry,
                                       &stableReceiver,
                                       &stableAssignedValue,
                                       1,
                                       &ignoredResult,
                                       ZR_TRUE,
                                       expectedStatic)) {
        ZrCore_Value_Copy(state, receiverAndResult, &stableReceiver);
        return ZR_TRUE;
    }

    memberName = execution_meta_cache_resolve_member_name(function, entry->memberEntryIndex);
    if (memberName == ZR_NULL) {
        return ZR_FALSE;
    }

    entry->runtimeMissCount++;
    if (!execution_meta_set_member(state, receiverAndResult, memberName, assignedValue)) {
        return ZR_FALSE;
    }

    execution_meta_refresh_cache(state, entry, &stableReceiver, memberName);
    execution_meta_validate_cache_static_mode(entry, ZR_TRUE, expectedStatic);
    return ZR_TRUE;
}

TZrBool execution_meta_set_cached_member(SZrState *state,
                                         SZrFunction *function,
                                         TZrUInt16 cacheIndex,
                                         SZrTypeValue *receiverAndResult,
                                         const SZrTypeValue *assignedValue) {
    return execution_meta_set_cached_member_internal(state,
                                                     function,
                                                     cacheIndex,
                                                     receiverAndResult,
                                                     assignedValue,
                                                     ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET,
                                                     ZR_FALSE);
}

TZrBool execution_meta_set_cached_static_member(SZrState *state,
                                                SZrFunction *function,
                                                TZrUInt16 cacheIndex,
                                                SZrTypeValue *receiverAndResult,
                                                const SZrTypeValue *assignedValue) {
    return execution_meta_set_cached_member_internal(state,
                                                     function,
                                                     cacheIndex,
                                                     receiverAndResult,
                                                     assignedValue,
                                                     ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET_STATIC,
                                                     ZR_TRUE);
}
