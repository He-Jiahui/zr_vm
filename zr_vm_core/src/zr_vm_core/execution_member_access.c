//
// Split from execution_dispatch.c: GET_MEMBER / SET_MEMBER helpers.
//

#include "execution_internal.h"
#include "object_internal.h"

static ZR_FORCE_INLINE SZrString *execution_member_refresh_forwarded_string(SZrString *stringValue) {
    SZrRawObject *rawObject;
    SZrRawObject *forwardedObject;

    if (stringValue == ZR_NULL) {
        return ZR_NULL;
    }

    rawObject = ZR_CAST_RAW_OBJECT_AS_SUPER(stringValue);
    forwardedObject = (SZrRawObject *)rawObject->garbageCollectMark.forwardingAddress;
    if (forwardedObject == ZR_NULL) {
        return stringValue;
    }

    return ZR_CAST_STRING(ZR_NULL, forwardedObject);
}

static ZR_FORCE_INLINE void execution_member_make_key(SZrState *state,
                                                      SZrString *memberName,
                                                      SZrTypeValue *outKey) {
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(outKey != ZR_NULL);
    memberName = execution_member_refresh_forwarded_string(memberName);
    ZR_ASSERT(memberName != ZR_NULL);
    ZrCore_Value_InitAsRawObject(state, outKey, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));
    outKey->type = ZR_VALUE_TYPE_STRING;
}

static ZR_FORCE_INLINE SZrCallInfo *execution_member_prepare_protected_call(SZrState *state,
                                                                            const TZrInstruction *programCounter) {
    SZrCallInfo *callInfo;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Profile_RecordSlowPathCurrent(ZR_PROFILE_SLOWPATH_PROTECT_E);
    callInfo = state->callInfoList;
    if (callInfo == ZR_NULL) {
        return ZR_NULL;
    }

    callInfo->context.context.programCounter = programCounter;
    state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
    return callInfo;
}

static TZrBool execution_member_try_anchor_stack_value(SZrState *state,
                                                       SZrTypeValue *value,
                                                       SZrFunctionStackAnchor *anchor) {
    TZrStackValuePointer stackSlot;

    if (state == ZR_NULL || value == ZR_NULL || anchor == ZR_NULL) {
        return ZR_FALSE;
    }

    stackSlot = ZR_CAST(TZrStackValuePointer, value);
    if (stackSlot < state->stackBase.valuePointer || stackSlot >= state->stackTail.valuePointer) {
        return ZR_FALSE;
    }

    ZrCore_Function_StackAnchorInit(state, stackSlot, anchor);
    return ZR_TRUE;
}

static SZrClosure *execution_member_current_closure(SZrState *state) {
    SZrTypeValue *closureValue;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL || state->callInfoList->functionBase.valuePointer == ZR_NULL) {
        return ZR_NULL;
    }

    closureValue = ZrCore_Stack_GetValue(state->callInfoList->functionBase.valuePointer);
    if (closureValue == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_VM_CLOSURE(state, closureValue->value.object);
}

static SZrString *execution_member_cache_resolve_name(SZrFunction *function, TZrUInt32 memberEntryIndex) {
    SZrString *memberName;

    if (function == ZR_NULL || function->memberEntries == ZR_NULL || memberEntryIndex >= function->memberEntryLength) {
        return ZR_NULL;
    }

    memberName = execution_member_refresh_forwarded_string(function->memberEntries[memberEntryIndex].symbol);
    function->memberEntries[memberEntryIndex].symbol = memberName;
    return memberName;
}

static SZrObjectPrototype *execution_member_resolve_receiver_prototype(SZrState *state,
                                                                       const SZrTypeValue *receiver) {
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

static void execution_member_clear_pic_slot(SZrFunctionCallSitePicSlot *slot) {
    if (slot == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(slot, 0, sizeof(*slot));
    slot->cachedDescriptorIndex = ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE;
}

static void execution_member_clear_cache_entry(SZrFunction *function,
                                               TZrUInt16 cacheIndex,
                                               SZrFunctionCallSiteCacheEntry *entry,
                                               const TZrChar *reason) {
    TZrUInt32 index;

    if (entry == ZR_NULL) {
        return;
    }

    entry->picSlotCount = 0;
    entry->picNextInsertIndex = 0;
    for (index = 0; index < ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY; index++) {
        execution_member_clear_pic_slot(&entry->picSlots[index]);
    }
}

static TZrBool execution_member_find_owner_descriptor(SZrObjectPrototype *receiverPrototype,
                                                      SZrString *memberName,
                                                      SZrObjectPrototype **outOwnerPrototype,
                                                      TZrUInt32 *outDescriptorIndex) {
    SZrObjectPrototype *ownerPrototype = receiverPrototype;

    if (outOwnerPrototype != ZR_NULL) {
        *outOwnerPrototype = ZR_NULL;
    }
    if (outDescriptorIndex != ZR_NULL) {
        *outDescriptorIndex = ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE;
    }
    if (receiverPrototype == ZR_NULL || memberName == ZR_NULL) {
        return ZR_FALSE;
    }

    while (ownerPrototype != ZR_NULL) {
        for (TZrUInt32 descriptorIndex = 0; descriptorIndex < ownerPrototype->memberDescriptorCount; descriptorIndex++) {
            SZrMemberDescriptor *descriptor = &ownerPrototype->memberDescriptors[descriptorIndex];
            descriptor->name = execution_member_refresh_forwarded_string(descriptor->name);
            if (descriptor->name != ZR_NULL && ZrCore_String_Equal(descriptor->name, memberName)) {
                if (outOwnerPrototype != ZR_NULL) {
                    *outOwnerPrototype = ownerPrototype;
                }
                if (outDescriptorIndex != ZR_NULL) {
                    *outDescriptorIndex = descriptorIndex;
                }
                return ZR_TRUE;
            }
        }
        ownerPrototype = ownerPrototype->superPrototype;
    }

    return ZR_FALSE;
}

static void execution_member_store_pic_slot(SZrFunction *function,
                                            TZrUInt16 cacheIndex,
                                            SZrFunctionCallSiteCacheEntry *entry,
                                            SZrObjectPrototype *receiverPrototype,
                                            SZrObjectPrototype *ownerPrototype,
                                            TZrUInt32 descriptorIndex,
                                            TZrBool isStatic) {
    TZrUInt32 slotIndex;
    SZrFunctionCallSitePicSlot *slot;

    if (entry == ZR_NULL || receiverPrototype == ZR_NULL || ownerPrototype == ZR_NULL ||
        descriptorIndex == ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE) {
        return;
    }

    for (slotIndex = 0; slotIndex < entry->picSlotCount; slotIndex++) {
        slot = &entry->picSlots[slotIndex];
        if (slot->cachedReceiverPrototype == receiverPrototype) {
            slot->cachedOwnerPrototype = ownerPrototype;
            slot->cachedReceiverVersion = receiverPrototype->super.memberVersion;
            slot->cachedOwnerVersion = ownerPrototype->super.memberVersion;
            slot->cachedDescriptorIndex = descriptorIndex;
            slot->cachedIsStatic = isStatic ? ZR_TRUE : ZR_FALSE;
            slot->cachedFunction = ZR_NULL;
            return;
        }
    }

    if (entry->picSlotCount < ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY) {
        slotIndex = entry->picSlotCount++;
    } else {
        slotIndex = entry->picNextInsertIndex % ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY;
    }
    entry->picNextInsertIndex = (slotIndex + 1u) % ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY;

    slot = &entry->picSlots[slotIndex];
    execution_member_clear_pic_slot(slot);
    slot->cachedReceiverPrototype = receiverPrototype;
    slot->cachedOwnerPrototype = ownerPrototype;
    slot->cachedReceiverVersion = receiverPrototype->super.memberVersion;
    slot->cachedOwnerVersion = ownerPrototype->super.memberVersion;
    slot->cachedDescriptorIndex = descriptorIndex;
    slot->cachedIsStatic = isStatic ? ZR_TRUE : ZR_FALSE;
}

static void execution_member_refresh_cache(SZrState *state,
                                           SZrFunction *function,
                                           TZrUInt16 cacheIndex,
                                           SZrFunctionCallSiteCacheEntry *entry,
                                           SZrTypeValue *receiver,
                                           SZrString *memberName) {
    SZrObjectPrototype *receiverPrototype;
    SZrObjectPrototype *ownerPrototype = ZR_NULL;
    TZrUInt32 descriptorIndex = ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE;

    if (state == ZR_NULL || entry == ZR_NULL || receiver == ZR_NULL || memberName == ZR_NULL) {
        return;
    }

    receiverPrototype = execution_member_resolve_receiver_prototype(state, receiver);
    if (receiverPrototype == ZR_NULL ||
        !execution_member_find_owner_descriptor(receiverPrototype, memberName, &ownerPrototype, &descriptorIndex)) {
        return;
    }

    execution_member_store_pic_slot(function,
                                    cacheIndex,
                                    entry,
                                    receiverPrototype,
                                    ownerPrototype,
                                    descriptorIndex,
                                    ownerPrototype->memberDescriptors[descriptorIndex].isStatic);
}

static TZrBool execution_member_try_cached_get(SZrState *state,
                                               SZrFunction *function,
                                               TZrUInt16 cacheIndex,
                                               SZrFunctionCallSiteCacheEntry *entry,
                                               SZrTypeValue *receiver,
                                               SZrTypeValue *result) {
    SZrObjectPrototype *receiverPrototype;

    if (state == ZR_NULL || entry == ZR_NULL || receiver == ZR_NULL || result == ZR_NULL || entry->picSlotCount == 0) {
        return ZR_FALSE;
    }

    receiverPrototype = execution_member_resolve_receiver_prototype(state, receiver);
    if (receiverPrototype == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 slotIndex = 0; slotIndex < entry->picSlotCount; slotIndex++) {
        SZrFunctionCallSitePicSlot *slot = &entry->picSlots[slotIndex];
        SZrTypeValue stableReceiver;
        SZrTypeValue stableResult;
        SZrFunctionStackAnchor resultAnchor;
        TZrBool hasResultAnchor;

        if (slot->cachedReceiverPrototype != receiverPrototype) {
            continue;
        }
        if (slot->cachedOwnerPrototype == ZR_NULL ||
            slot->cachedDescriptorIndex == ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE) {
            execution_member_clear_cache_entry(function, cacheIndex, entry, "missing-owner-or-descriptor");
            return ZR_FALSE;
        }
        if (slot->cachedReceiverPrototype->super.memberVersion != slot->cachedReceiverVersion ||
            slot->cachedOwnerPrototype->super.memberVersion != slot->cachedOwnerVersion) {
            execution_member_clear_cache_entry(function, cacheIndex, entry, "version-mismatch");
            return ZR_FALSE;
        }

        stableReceiver = *receiver;
        ZrCore_Value_ResetAsNull(&stableResult);
        hasResultAnchor = execution_member_try_anchor_stack_value(state, result, &resultAnchor);
        if (!ZrCore_Object_GetMemberCachedDescriptorUnchecked(state,
                                                              &stableReceiver,
                                                              slot->cachedOwnerPrototype,
                                                              slot->cachedDescriptorIndex,
                                                              &stableResult)) {
            execution_member_clear_cache_entry(function, cacheIndex, entry, "cached-get-failed");
            return ZR_FALSE;
        }
        if (hasResultAnchor) {
            result = ZrCore_Stack_GetValue(ZrCore_Function_StackAnchorRestore(state, &resultAnchor));
        }
        ZrCore_Value_Copy(state, result, &stableResult);

        entry->runtimeHitCount++;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool execution_member_try_cached_set(SZrState *state,
                                               SZrFunction *function,
                                               TZrUInt16 cacheIndex,
                                               SZrFunctionCallSiteCacheEntry *entry,
                                               SZrTypeValue *receiver,
                                               const SZrTypeValue *assignedValue) {
    SZrObjectPrototype *receiverPrototype;

    if (state == ZR_NULL || entry == ZR_NULL || receiver == ZR_NULL || assignedValue == ZR_NULL ||
        entry->picSlotCount == 0) {
        return ZR_FALSE;
    }

    receiverPrototype = execution_member_resolve_receiver_prototype(state, receiver);
    if (receiverPrototype == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 slotIndex = 0; slotIndex < entry->picSlotCount; slotIndex++) {
        SZrFunctionCallSitePicSlot *slot = &entry->picSlots[slotIndex];
        SZrTypeValue stableReceiver;
        SZrTypeValue stableAssignedValue;

        if (slot->cachedReceiverPrototype != receiverPrototype) {
            continue;
        }
        if (slot->cachedOwnerPrototype == ZR_NULL ||
            slot->cachedDescriptorIndex == ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE) {
            execution_member_clear_cache_entry(function, cacheIndex, entry, "missing-owner-or-descriptor");
            return ZR_FALSE;
        }
        if (slot->cachedReceiverPrototype->super.memberVersion != slot->cachedReceiverVersion ||
            slot->cachedOwnerPrototype->super.memberVersion != slot->cachedOwnerVersion) {
            execution_member_clear_cache_entry(function, cacheIndex, entry, "version-mismatch");
            return ZR_FALSE;
        }

        stableReceiver = *receiver;
        stableAssignedValue = *assignedValue;
        if (!ZrCore_Object_SetMemberCachedDescriptorUnchecked(state,
                                                              &stableReceiver,
                                                              slot->cachedOwnerPrototype,
                                                              slot->cachedDescriptorIndex,
                                                              &stableAssignedValue)) {
            execution_member_clear_cache_entry(function, cacheIndex, entry, "cached-set-failed");
            return ZR_FALSE;
        }

        entry->runtimeHitCount++;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

TZrBool execution_member_get_by_name(SZrState *state,
                                     const TZrInstruction *programCounter,
                                     SZrTypeValue *receiver,
                                     SZrString *memberName,
                                     SZrTypeValue *result) {
    SZrTypeValue memberKey;
    SZrTypeValue stableReceiver;
    SZrTypeValue stableResult;
    SZrFunctionStackAnchor resultAnchor;
    TZrBool fastHandled = ZR_FALSE;
    TZrBool hasResultAnchor = ZR_FALSE;
    TZrBool allowGlobalPrototypeRetry = ZR_FALSE;
    TZrBool shouldTrySlowPath = ZR_TRUE;
    TZrBool resolved = ZR_FALSE;

    if (state == ZR_NULL || receiver == ZR_NULL || memberName == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    if (receiver->type != ZR_VALUE_TYPE_OBJECT &&
        receiver->type != ZR_VALUE_TYPE_ARRAY &&
        receiver->type != ZR_VALUE_TYPE_STRING) {
        return ZR_FALSE;
    }

    execution_member_make_key(state, memberName, &memberKey);
    allowGlobalPrototypeRetry =
            (receiver->type == ZR_VALUE_TYPE_OBJECT &&
             state->global != ZR_NULL &&
             state->global->zrObject.type == ZR_VALUE_TYPE_OBJECT &&
             state->global->zrObject.value.object == receiver->value.object);
    if (receiver->type != ZR_VALUE_TYPE_STRING &&
        ZrCore_Object_TryGetMemberWithKeyFastUnchecked(
                state, receiver, memberName, &memberKey, result, &fastHandled)) {
        resolved = ZR_TRUE;
    } else if (fastHandled) {
        shouldTrySlowPath = allowGlobalPrototypeRetry;
    }

    if (!resolved && shouldTrySlowPath) {
        SZrCallInfo *callInfo;

        stableReceiver = *receiver;
        ZrCore_Value_ResetAsNull(&stableResult);
        hasResultAnchor = execution_member_try_anchor_stack_value(state, result, &resultAnchor);
        callInfo = execution_member_prepare_protected_call(state, programCounter);
        resolved = ZrCore_Object_GetMemberWithKeyUnchecked(
                state, &stableReceiver, memberName, &memberKey, &stableResult);
        if (!resolved &&
            execution_try_materialize_global_prototypes(
                    state,
                    execution_member_current_closure(state),
                    callInfo,
                    &stableReceiver,
                    &memberKey)) {
            resolved = ZrCore_Object_GetMemberWithKeyUnchecked(
                    state, &stableReceiver, memberName, &memberKey, &stableResult);
        }

        if (resolved) {
            if (hasResultAnchor) {
                result = ZrCore_Stack_GetValue(ZrCore_Function_StackAnchorRestore(state, &resultAnchor));
            }
            ZrCore_Value_Copy(state, result, &stableResult);
        }
    }

    return resolved;
}

TZrBool execution_member_set_by_name(SZrState *state,
                                     const TZrInstruction *programCounter,
                                     SZrTypeValue *receiverAndResult,
                                     SZrString *memberName,
                                     const SZrTypeValue *assignedValue) {
    SZrTypeValue memberKey;
    SZrTypeValue stableReceiver;
    SZrTypeValue stableAssignedValue;
    TZrBool fastHandled = ZR_FALSE;
    TZrBool resolved = ZR_FALSE;

    if (state == ZR_NULL || receiverAndResult == ZR_NULL || memberName == ZR_NULL || assignedValue == ZR_NULL) {
        return ZR_FALSE;
    }
    if (receiverAndResult->type != ZR_VALUE_TYPE_OBJECT && receiverAndResult->type != ZR_VALUE_TYPE_ARRAY) {
        return ZR_FALSE;
    }

    execution_member_make_key(state, memberName, &memberKey);
    if (ZrCore_Object_TrySetMemberWithKeyFastUnchecked(
                state, receiverAndResult, memberName, &memberKey, assignedValue, &fastHandled)) {
        return ZR_TRUE;
    }
    if (fastHandled) {
        return ZR_FALSE;
    }

    stableReceiver = *receiverAndResult;
    stableAssignedValue = *assignedValue;
    execution_member_prepare_protected_call(state, programCounter);
    resolved = ZrCore_Object_SetMemberWithKeyUnchecked(
            state, &stableReceiver, memberName, &memberKey, &stableAssignedValue);
    return resolved;
}

TZrBool execution_member_get_cached(SZrState *state,
                                    const TZrInstruction *programCounter,
                                    SZrFunction *function,
                                    TZrUInt16 cacheIndex,
                                    SZrTypeValue *receiver,
                                    SZrTypeValue *result) {
    SZrFunctionCallSiteCacheEntry *entry;
    SZrString *memberName;

    if (state == ZR_NULL || function == ZR_NULL || receiver == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    entry = (SZrFunctionCallSiteCacheEntry *)execution_get_callsite_cache_entry(
            function,
            cacheIndex,
            ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET);
    if (entry == ZR_NULL) {
        return ZR_FALSE;
    }

    if (execution_member_try_cached_get(state, function, cacheIndex, entry, receiver, result)) {
        return ZR_TRUE;
    }

    memberName = execution_member_cache_resolve_name(function, entry->memberEntryIndex);
    if (memberName == ZR_NULL) {
        return ZR_FALSE;
    }

    entry->runtimeMissCount++;
    if (!execution_member_get_by_name(state, programCounter, receiver, memberName, result)) {
        return ZR_FALSE;
    }

    execution_member_refresh_cache(state, function, cacheIndex, entry, receiver, memberName);
    return ZR_TRUE;
}

TZrBool execution_member_set_cached(SZrState *state,
                                    const TZrInstruction *programCounter,
                                    SZrFunction *function,
                                    TZrUInt16 cacheIndex,
                                    SZrTypeValue *receiverAndResult,
                                    const SZrTypeValue *assignedValue) {
    SZrFunctionCallSiteCacheEntry *entry;
    SZrString *memberName;

    if (state == ZR_NULL || function == ZR_NULL || receiverAndResult == ZR_NULL || assignedValue == ZR_NULL) {
        return ZR_FALSE;
    }

    entry = (SZrFunctionCallSiteCacheEntry *)execution_get_callsite_cache_entry(
            function,
            cacheIndex,
            ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET);
    if (entry == ZR_NULL) {
        return ZR_FALSE;
    }

    if (execution_member_try_cached_set(state, function, cacheIndex, entry, receiverAndResult, assignedValue)) {
        return ZR_TRUE;
    }

    memberName = execution_member_cache_resolve_name(function, entry->memberEntryIndex);
    if (memberName == ZR_NULL) {
        return ZR_FALSE;
    }

    entry->runtimeMissCount++;
    if (!execution_member_set_by_name(state, programCounter, receiverAndResult, memberName, assignedValue)) {
        return ZR_FALSE;
    }

    execution_member_refresh_cache(state, function, cacheIndex, entry, receiverAndResult, memberName);
    return ZR_TRUE;
}
