//
// Split from execution_dispatch.c: GET_MEMBER / SET_MEMBER helpers.
//

#include "execution/execution_internal.h"
#include "object/object_internal.h"
#include "object/object_super_array_internal.h"

#include <string.h>

static ZR_FORCE_INLINE TZrBool execution_member_callsite_sanitize_enabled(void) {
    static TZrBool initialized = ZR_FALSE;
    static TZrBool enabled = ZR_FALSE;

    if (!initialized) {
        enabled = getenv("ZR_VM_TRACE_GC_CALLSITE_SANITIZE") != ZR_NULL ? ZR_TRUE : ZR_FALSE;
        initialized = ZR_TRUE;
    }

    return enabled;
}

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

static ZR_FORCE_INLINE void execution_member_refresh_forwarded_value_copy(SZrTypeValue *value) {
    SZrRawObject *forwardedObject;

    if (value == ZR_NULL || !value->isGarbageCollectable || value->value.object == ZR_NULL) {
        return;
    }

    forwardedObject = (SZrRawObject *)value->value.object->garbageCollectMark.forwardingAddress;
    if (forwardedObject == ZR_NULL || forwardedObject == value->value.object) {
        return;
    }

    value->value.object = forwardedObject;
    value->type = (EZrValueType)forwardedObject->type;
    value->isNative = forwardedObject->isNative;
}

static ZR_FORCE_INLINE SZrProfileRuntime *execution_member_profile_runtime(SZrState *state) {
    return (state != ZR_NULL && state->global != ZR_NULL) ? state->global->profileRuntime : ZR_NULL;
}

static ZR_FORCE_INLINE void execution_member_record_helper(SZrState *state, EZrProfileHelperKind kind) {
    SZrProfileRuntime *runtime = execution_member_profile_runtime(state);

    if (ZR_UNLIKELY(runtime != ZR_NULL && runtime->recordHelpers)) {
        runtime->helperCounts[kind]++;
    }
}

static ZR_FORCE_INLINE void execution_member_copy_value_profiled(SZrState *state,
                                                                 SZrTypeValue *destination,
                                                                 const SZrTypeValue *source) {
    execution_member_record_helper(state, ZR_PROFILE_HELPER_VALUE_COPY);
    if (ZR_LIKELY(ZrCore_Value_CanFastCopyPlainValue(destination, source))) {
        *destination = *source;
        return;
    }

    ZrCore_Value_CopyNoProfile(state, destination, source);
}

static ZR_FORCE_INLINE void execution_member_record_get_and_copy_value(SZrState *state,
                                                                       SZrTypeValue *destination,
                                                                       const SZrTypeValue *source) {
    SZrProfileRuntime *runtime = execution_member_profile_runtime(state);

    if (ZR_UNLIKELY(runtime != ZR_NULL && runtime->recordHelpers)) {
        runtime->helperCounts[ZR_PROFILE_HELPER_GET_MEMBER]++;
        runtime->helperCounts[ZR_PROFILE_HELPER_VALUE_COPY]++;
    }

    if (ZR_LIKELY(ZrCore_Value_CanFastCopyPlainValue(destination, source))) {
        *destination = *source;
        return;
    }

    ZrCore_Value_CopyNoProfile(state, destination, source);
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

static ZR_FORCE_INLINE EZrFunctionCallSitePicAccessKind execution_member_resolve_pic_access_kind(
        const SZrMemberDescriptor *descriptor) {
    if (descriptor != ZR_NULL &&
        descriptor->kind == ZR_MEMBER_DESCRIPTOR_KIND_FIELD &&
        !descriptor->isStatic) {
        return ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD;
    }

    return ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_NONE;
}

static ZR_FORCE_INLINE TZrUInt8 execution_member_resolve_pic_slot_flags(
        SZrState *state,
        EZrFunctionCallSitePicAccessKind accessKind,
        SZrString *cachedMemberName) {
    SZrString *hiddenItemsName;

    if (state == ZR_NULL ||
        accessKind != ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD ||
        cachedMemberName == ZR_NULL) {
        return 0u;
    }

    hiddenItemsName = ZrCore_Object_CachedKnownFieldString(state, ZR_OBJECT_HIDDEN_ITEMS_FIELD);
    if (hiddenItemsName == ZR_NULL || ZrCore_String_Equal(cachedMemberName, hiddenItemsName)) {
        return 0u;
    }

    return ZR_FUNCTION_CALLSITE_PIC_SLOT_FLAG_NON_HIDDEN_STRING_PAIR_FAST_SET;
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
    if (closureValue == ZR_NULL ||
        closureValue->type != ZR_VALUE_TYPE_CLOSURE ||
        closureValue->isNative) {
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

static void execution_member_barrier_callsite_target(SZrState *state,
                                                     SZrFunction *function,
                                                     SZrRawObject *target);

static ZR_FORCE_INLINE SZrString *execution_member_slot_resolve_cached_name(SZrState *state,
                                                                            SZrFunction *function,
                                                                            SZrFunctionCallSitePicSlot *slot,
                                                                            TZrUInt32 memberEntryIndex) {
    SZrString *memberName;

    if (slot != ZR_NULL && slot->cachedMemberName != ZR_NULL) {
        memberName = execution_member_refresh_forwarded_string(slot->cachedMemberName);
        slot->cachedMemberName = memberName;
        if (memberName != ZR_NULL) {
            return memberName;
        }
    }

    memberName = execution_member_cache_resolve_name(function, memberEntryIndex);
    if (slot != ZR_NULL && memberName != ZR_NULL) {
        slot->cachedMemberName = memberName;
        if (state != ZR_NULL && function != ZR_NULL) {
            execution_member_barrier_callsite_target(state, function, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));
        }
    }

    return memberName;
}

static ZR_FORCE_INLINE SZrFunctionCallSiteCacheEntry *execution_member_get_cache_entry_fast(
        SZrFunction *function,
        TZrUInt16 cacheIndex,
        EZrFunctionCallSiteCacheKind expectedKind) {
    SZrFunctionCallSiteCacheEntry *entry;

    if (function == ZR_NULL || function->callSiteCaches == ZR_NULL || cacheIndex >= function->callSiteCacheLength) {
        ZrCore_Profile_RecordSlowPathCurrent(ZR_PROFILE_SLOWPATH_CALLSITE_CACHE_MISS);
        return ZR_NULL;
    }

    entry = &function->callSiteCaches[cacheIndex];
    if ((EZrFunctionCallSiteCacheKind)entry->kind != expectedKind) {
        ZrCore_Profile_RecordSlowPathCurrent(ZR_PROFILE_SLOWPATH_CALLSITE_CACHE_MISS);
        return ZR_NULL;
    }

    if (execution_member_callsite_sanitize_enabled()) {
        garbage_collector_sanitize_callsite_cache_pic(function, cacheIndex, "runtime-callsite-lookup", entry);
    }

    return entry;
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

static ZR_FORCE_INLINE SZrObject *execution_member_resolve_receiver_object(SZrState *state,
                                                                           const SZrTypeValue *receiver) {
    SZrObject *object;

    if (state == ZR_NULL || receiver == ZR_NULL ||
        (receiver->type != ZR_VALUE_TYPE_OBJECT && receiver->type != ZR_VALUE_TYPE_ARRAY) ||
        receiver->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZR_CAST_OBJECT(state, receiver->value.object);
    if (object == ZR_NULL || object->internalType == ZR_OBJECT_INTERNAL_TYPE_MODULE) {
        return ZR_NULL;
    }

    return object;
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

    (void)function;
    (void)cacheIndex;
    (void)reason;
    entry->picSlotCount = 0;
    entry->picNextInsertIndex = 0;
    for (index = 0; index < ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY; index++) {
        execution_member_clear_pic_slot(&entry->picSlots[index]);
    }
}

static void execution_member_barrier_callsite_target(SZrState *state,
                                                     SZrFunction *function,
                                                     SZrRawObject *target) {
    if (state == ZR_NULL || function == ZR_NULL || target == ZR_NULL) {
        return;
    }

    ZrCore_RawObject_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(function), target);
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

static ZR_FORCE_INLINE TZrBool execution_member_owner_on_receiver_chain(
        const SZrObjectPrototype *receiverPrototype,
        const SZrObjectPrototype *ownerPrototype) {
    while (receiverPrototype != ZR_NULL) {
        if (receiverPrototype == ownerPrototype) {
            return ZR_TRUE;
        }
        receiverPrototype = receiverPrototype->superPrototype;
    }

    return ZR_FALSE;
}

static TZrBool execution_member_find_owner_descriptorless_callable(SZrState *state,
                                                                   SZrObjectPrototype *receiverPrototype,
                                                                   SZrString *memberName,
                                                                   SZrObjectPrototype **outOwnerPrototype) {
    if (outOwnerPrototype != ZR_NULL) {
        *outOwnerPrototype = ZR_NULL;
    }
    if (state == ZR_NULL || receiverPrototype == ZR_NULL || memberName == ZR_NULL) {
        return ZR_FALSE;
    }

    while (receiverPrototype != ZR_NULL) {
        const SZrTypeValue *resolvedValue =
                object_get_own_string_value_by_name_cached_unchecked(state, &receiverPrototype->super, memberName);

        if (resolvedValue != ZR_NULL &&
            (resolvedValue->type == ZR_VALUE_TYPE_FUNCTION || resolvedValue->type == ZR_VALUE_TYPE_CLOSURE) &&
            resolvedValue->value.object != ZR_NULL) {
            if (outOwnerPrototype != ZR_NULL) {
                *outOwnerPrototype = receiverPrototype;
            }
            return ZR_TRUE;
        }

        receiverPrototype = receiverPrototype->superPrototype;
    }

    return ZR_FALSE;
}

static TZrBool execution_member_try_resolve_bound_descriptor(SZrFunction *function,
                                                             const SZrFunctionCallSiteCacheEntry *entry,
                                                             SZrObjectPrototype *receiverPrototype,
                                                             SZrObjectPrototype **outOwnerPrototype,
                                                             TZrUInt32 *outDescriptorIndex) {
    const SZrFunctionMemberEntry *memberEntry;
    SZrObjectPrototype *ownerPrototype;

    if (outOwnerPrototype != ZR_NULL) {
        *outOwnerPrototype = ZR_NULL;
    }
    if (outDescriptorIndex != ZR_NULL) {
        *outDescriptorIndex = ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE;
    }

    if (function == ZR_NULL || entry == ZR_NULL || receiverPrototype == ZR_NULL ||
        function->memberEntries == ZR_NULL || entry->memberEntryIndex >= function->memberEntryLength ||
        function->prototypeInstances == ZR_NULL) {
        return ZR_FALSE;
    }

    memberEntry = &function->memberEntries[entry->memberEntryIndex];
    if (memberEntry->entryKind != ZR_FUNCTION_MEMBER_ENTRY_KIND_BOUND_DESCRIPTOR ||
        memberEntry->prototypeIndex >= function->prototypeInstancesLength) {
        return ZR_FALSE;
    }

    ownerPrototype = function->prototypeInstances[memberEntry->prototypeIndex];
    if (ownerPrototype == ZR_NULL || ownerPrototype->memberDescriptors == ZR_NULL ||
        memberEntry->descriptorIndex >= ownerPrototype->memberDescriptorCount ||
        !execution_member_owner_on_receiver_chain(receiverPrototype, ownerPrototype)) {
        return ZR_FALSE;
    }

    if (outOwnerPrototype != ZR_NULL) {
        *outOwnerPrototype = ownerPrototype;
    }
    if (outDescriptorIndex != ZR_NULL) {
        *outDescriptorIndex = memberEntry->descriptorIndex;
    }

    return ZR_TRUE;
}

static void execution_member_store_pic_slot(SZrState *state,
                                            SZrFunction *function,
                                            TZrUInt16 cacheIndex,
                                            SZrFunctionCallSiteCacheEntry *entry,
                                            SZrObject *receiverObject,
                                            SZrObjectPrototype *receiverPrototype,
                                            SZrObjectPrototype *ownerPrototype,
                                            TZrUInt32 descriptorIndex,
                                            TZrBool isStatic) {
    TZrUInt32 slotIndex;
    SZrFunctionCallSitePicSlot *slot;
    SZrFunction *callableTarget;
    const SZrMemberDescriptor *descriptor;
    SZrString *cachedMemberName;
    SZrObject *cachedReceiverObject;
    SZrHashKeyValuePair **cachedReceiverBuckets;
    SZrHashKeyValuePair *cachedReceiverPair;
    TZrUInt32 cachedReceiverElementCount;
    EZrFunctionCallSitePicAccessKind accessKind;
    TZrUInt8 slotFlags;
    const TZrChar *traceAction;

    if (entry == ZR_NULL || receiverPrototype == ZR_NULL || ownerPrototype == ZR_NULL) {
        return;
    }

    descriptor = ZR_NULL;
    accessKind = ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_NONE;
    callableTarget = ZR_NULL;
    if (descriptorIndex != ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE) {
        if (ownerPrototype->memberDescriptors == ZR_NULL || descriptorIndex >= ownerPrototype->memberDescriptorCount) {
            return;
        }

        descriptor = &ownerPrototype->memberDescriptors[descriptorIndex];
        accessKind = execution_member_resolve_pic_access_kind(descriptor);
        callableTarget = ZrCore_Object_GetMemberCachedCallableTargetUnchecked(state, ownerPrototype, descriptorIndex);
    }
    cachedMemberName = ZR_NULL;
    cachedReceiverObject = ZR_NULL;
    cachedReceiverBuckets = ZR_NULL;
    cachedReceiverPair = ZR_NULL;
    cachedReceiverElementCount = 0u;
    slotFlags = 0u;
    if (accessKind == ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD || callableTarget != ZR_NULL) {
        if (descriptor != ZR_NULL && descriptor->name != ZR_NULL) {
            cachedMemberName = execution_member_refresh_forwarded_string(descriptor->name);
        }
        if (cachedMemberName == ZR_NULL) {
            cachedMemberName = execution_member_cache_resolve_name(function, entry->memberEntryIndex);
        }
    }
    if (descriptorIndex == ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE &&
        callableTarget == ZR_NULL &&
        receiverObject != ZR_NULL) {
        cachedMemberName = execution_member_cache_resolve_name(function, entry->memberEntryIndex);
        if (cachedMemberName != ZR_NULL) {
            const SZrTypeValue *resolvedValue =
                    object_get_own_string_value_by_name_cached_unchecked(state, &ownerPrototype->super, cachedMemberName);

            if (resolvedValue != ZR_NULL &&
                (resolvedValue->type == ZR_VALUE_TYPE_FUNCTION || resolvedValue->type == ZR_VALUE_TYPE_CLOSURE) &&
                resolvedValue->value.object != ZR_NULL &&
                object_get_own_string_value_by_name_cached_unchecked(state, receiverObject, cachedMemberName) == ZR_NULL) {
                callableTarget = ZR_CAST(SZrFunction *, resolvedValue->value.object);
                cachedReceiverObject = receiverObject;
                cachedReceiverBuckets = receiverObject->nodeMap.buckets;
                cachedReceiverElementCount = (TZrUInt32)receiverObject->nodeMap.elementCount;
            }
        }
    }
    if (accessKind == ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD && receiverObject != ZR_NULL &&
        cachedMemberName != ZR_NULL) {
        cachedReceiverPair = object_get_own_string_pair_by_name_cached_unchecked(state, receiverObject, cachedMemberName);
        if (cachedReceiverPair != ZR_NULL) {
            cachedReceiverObject = receiverObject;
            cachedReceiverBuckets = receiverObject->nodeMap.buckets;
            cachedReceiverElementCount = (TZrUInt32)receiverObject->nodeMap.elementCount;
        }
    }
    if (callableTarget != ZR_NULL && receiverObject != ZR_NULL && cachedMemberName != ZR_NULL &&
        object_get_own_string_value_by_name_cached_unchecked(state, receiverObject, cachedMemberName) == ZR_NULL) {
        cachedReceiverObject = receiverObject;
        cachedReceiverBuckets = receiverObject->nodeMap.buckets;
        cachedReceiverElementCount = (TZrUInt32)receiverObject->nodeMap.elementCount;
    }
    slotFlags = execution_member_resolve_pic_slot_flags(state, accessKind, cachedMemberName);

    if (execution_member_callsite_sanitize_enabled()) {
        garbage_collector_sanitize_callsite_cache_pic(function, cacheIndex, "runtime-member-store-before", entry);
    }

    for (slotIndex = 0; slotIndex < entry->picSlotCount; slotIndex++) {
        slot = &entry->picSlots[slotIndex];
        if (slot->cachedReceiverPrototype == receiverPrototype) {
            slot->cachedOwnerPrototype = ownerPrototype;
            slot->cachedReceiverObject = cachedReceiverObject;
            slot->cachedReceiverBuckets = cachedReceiverBuckets;
            slot->cachedReceiverPair = cachedReceiverPair;
            slot->cachedReceiverVersion = receiverPrototype->super.memberVersion;
            slot->cachedOwnerVersion = ownerPrototype->super.memberVersion;
            slot->cachedReceiverElementCount = cachedReceiverElementCount;
            slot->cachedDescriptorIndex = descriptorIndex;
            slot->cachedIsStatic = (TZrUInt8)((isStatic ? ZR_TRUE : ZR_FALSE) | slotFlags);
            slot->cachedAccessKind = (TZrUInt8)accessKind;
            slot->cachedFunction = callableTarget;
            slot->cachedMemberName = cachedMemberName;
            execution_member_barrier_callsite_target(state,
                                                     function,
                                                     ZR_CAST_RAW_OBJECT_AS_SUPER(receiverPrototype));
            execution_member_barrier_callsite_target(state,
                                                     function,
                                                     ZR_CAST_RAW_OBJECT_AS_SUPER(cachedReceiverObject));
            execution_member_barrier_callsite_target(state,
                                                     function,
                                                     ZR_CAST_RAW_OBJECT_AS_SUPER(ownerPrototype));
            execution_member_barrier_callsite_target(state,
                                                     function,
                                                     ZR_CAST_RAW_OBJECT_AS_SUPER(callableTarget));
            execution_member_barrier_callsite_target(state,
                                                     function,
                                                     ZR_CAST_RAW_OBJECT_AS_SUPER(cachedMemberName));
            garbage_collector_record_callsite_cache_pic_write(function,
                                                              cacheIndex,
                                                              slotIndex,
                                                              "member-store",
                                                              "update",
                                                              entry,
                                                              slot);
            if (execution_member_callsite_sanitize_enabled()) {
                garbage_collector_sanitize_callsite_cache_pic(function,
                                                              cacheIndex,
                                                              "runtime-member-store-after",
                                                              entry);
            }
            return;
        }
    }

    if (entry->picSlotCount < ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY) {
        slotIndex = entry->picSlotCount++;
        traceAction = "insert";
    } else {
        slotIndex = entry->picNextInsertIndex % ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY;
        traceAction = "replace";
    }
    entry->picNextInsertIndex = (slotIndex + 1u) % ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY;

    slot = &entry->picSlots[slotIndex];
    execution_member_clear_pic_slot(slot);
    slot->cachedReceiverPrototype = receiverPrototype;
    slot->cachedOwnerPrototype = ownerPrototype;
    slot->cachedReceiverObject = cachedReceiverObject;
    slot->cachedReceiverBuckets = cachedReceiverBuckets;
    slot->cachedReceiverPair = cachedReceiverPair;
    slot->cachedReceiverVersion = receiverPrototype->super.memberVersion;
    slot->cachedOwnerVersion = ownerPrototype->super.memberVersion;
    slot->cachedReceiverElementCount = cachedReceiverElementCount;
    slot->cachedDescriptorIndex = descriptorIndex;
    slot->cachedIsStatic = (TZrUInt8)((isStatic ? ZR_TRUE : ZR_FALSE) | slotFlags);
    slot->cachedAccessKind = (TZrUInt8)accessKind;
    slot->cachedFunction = callableTarget;
    slot->cachedMemberName = cachedMemberName;
    execution_member_barrier_callsite_target(state, function, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverPrototype));
    execution_member_barrier_callsite_target(state, function, ZR_CAST_RAW_OBJECT_AS_SUPER(cachedReceiverObject));
    execution_member_barrier_callsite_target(state, function, ZR_CAST_RAW_OBJECT_AS_SUPER(ownerPrototype));
    execution_member_barrier_callsite_target(state, function, ZR_CAST_RAW_OBJECT_AS_SUPER(callableTarget));
    execution_member_barrier_callsite_target(state, function, ZR_CAST_RAW_OBJECT_AS_SUPER(cachedMemberName));
    garbage_collector_record_callsite_cache_pic_write(function,
                                                      cacheIndex,
                                                      slotIndex,
                                                      "member-store",
                                                      traceAction,
                                                      entry,
                                                      slot);
    if (execution_member_callsite_sanitize_enabled()) {
        garbage_collector_sanitize_callsite_cache_pic(function, cacheIndex, "runtime-member-store-after", entry);
    }
}

static void execution_member_refresh_cache(SZrState *state,
                                           SZrFunction *function,
                                           TZrUInt16 cacheIndex,
                                           SZrFunctionCallSiteCacheEntry *entry,
                                           SZrTypeValue *receiver,
                                           SZrString *memberName) {
    SZrObject *receiverObject;
    SZrObjectPrototype *receiverPrototype;
    SZrObjectPrototype *ownerPrototype = ZR_NULL;
    TZrUInt32 descriptorIndex = ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE;

    if (state == ZR_NULL || entry == ZR_NULL || receiver == ZR_NULL || memberName == ZR_NULL) {
        return;
    }

    receiverObject = execution_member_resolve_receiver_object(state, receiver);
    receiverPrototype = execution_member_resolve_receiver_prototype(state, receiver);
    if (receiverPrototype == ZR_NULL) {
        return;
    }

    if (!execution_member_try_resolve_bound_descriptor(function,
                                                       entry,
                                                       receiverPrototype,
                                                       &ownerPrototype,
                                                       &descriptorIndex) &&
        !execution_member_find_owner_descriptor(receiverPrototype, memberName, &ownerPrototype, &descriptorIndex)) {
        if (!execution_member_find_owner_descriptorless_callable(state, receiverPrototype, memberName, &ownerPrototype)) {
            return;
        }
        descriptorIndex = ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE;
    }

    execution_member_store_pic_slot(state,
                                    function,
                                    cacheIndex,
                                    entry,
                                    receiverObject,
                                    receiverPrototype,
                                    ownerPrototype,
                                    descriptorIndex,
                                    descriptorIndex != ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE
                                            ? ownerPrototype->memberDescriptors[descriptorIndex].isStatic
                                            : ZR_FALSE);
}

static ZR_FORCE_INLINE TZrBool execution_member_cached_get_requires_result_anchor(
        const SZrMemberDescriptor *descriptor) {
    if (descriptor == ZR_NULL) {
        return ZR_TRUE;
    }

    return descriptor->kind == ZR_MEMBER_DESCRIPTOR_KIND_PROPERTY && descriptor->getterFunction != ZR_NULL;
}

static ZR_FORCE_INLINE TZrBool execution_member_cached_receiver_shape_matches(
        const SZrFunctionCallSitePicSlot *slot,
        const SZrObject *receiverObject) {
    if (slot == ZR_NULL || receiverObject == ZR_NULL || slot->cachedReceiverBuckets == ZR_NULL) {
        return ZR_FALSE;
    }

    return receiverObject->nodeMap.buckets == slot->cachedReceiverBuckets &&
           receiverObject->nodeMap.elementCount == (TZrSize)slot->cachedReceiverElementCount;
}

static ZR_FORCE_INLINE TZrBool execution_member_cached_slot_versions_match(
        const SZrFunctionCallSitePicSlot *slot) {
    if (slot == ZR_NULL || slot->cachedReceiverPrototype == ZR_NULL || slot->cachedOwnerPrototype == ZR_NULL) {
        return ZR_FALSE;
    }

    return slot->cachedReceiverPrototype->super.memberVersion == slot->cachedReceiverVersion &&
           slot->cachedOwnerPrototype->super.memberVersion == slot->cachedOwnerVersion;
}

static ZR_FORCE_INLINE SZrObject *execution_member_fast_instance_field_object(SZrState *state,
                                                                              const SZrTypeValue *receiver) {
    return execution_member_resolve_receiver_object(state, receiver);
}

static ZR_FORCE_INLINE TZrBool execution_member_try_cached_instance_field_pair_get(
        SZrState *state,
        const SZrFunctionCallSitePicSlot *slot,
        SZrTypeValue *result) {
    if (state == ZR_NULL || slot == ZR_NULL || slot->cachedReceiverPair == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    execution_member_record_helper(state, ZR_PROFILE_HELPER_GET_MEMBER);
    execution_member_copy_value_profiled(state, result, &slot->cachedReceiverPair->value);
    return ZR_TRUE;
}

static ZR_FORCE_INLINE TZrBool execution_member_can_use_exact_receiver_pair_hit(
        const SZrFunctionCallSitePicSlot *slot,
        const SZrObject *receiverObject) {
    return slot != ZR_NULL &&
           receiverObject != ZR_NULL &&
           slot->cachedReceiverObject == receiverObject &&
           (EZrFunctionCallSitePicAccessKind)slot->cachedAccessKind ==
                   ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD &&
           slot->cachedReceiverPair != ZR_NULL;
}

static ZR_FORCE_INLINE const SZrMemberDescriptor *execution_member_try_get_cached_slot_descriptor(
        const SZrFunctionCallSitePicSlot *slot) {
    if (slot == ZR_NULL || slot->cachedOwnerPrototype == ZR_NULL || slot->cachedOwnerPrototype->memberDescriptors == ZR_NULL ||
        slot->cachedDescriptorIndex == ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE ||
        slot->cachedDescriptorIndex >= slot->cachedOwnerPrototype->memberDescriptorCount) {
        return ZR_NULL;
    }

    return &slot->cachedOwnerPrototype->memberDescriptors[slot->cachedDescriptorIndex];
}

static ZR_FORCE_INLINE TZrBool execution_member_try_cached_instance_field_pair_set(
        SZrState *state,
        SZrObject *receiverObject,
        const SZrFunctionCallSitePicSlot *slot,
        const SZrTypeValue *assignedValue) {
    if (state == ZR_NULL || receiverObject == ZR_NULL || slot == ZR_NULL || slot->cachedReceiverPair == ZR_NULL ||
        assignedValue == ZR_NULL) {
        return ZR_FALSE;
    }

    execution_member_record_helper(state, ZR_PROFILE_HELPER_SET_MEMBER);
    if ((slot->cachedIsStatic & ZR_FUNCTION_CALLSITE_PIC_SLOT_FLAG_NON_HIDDEN_STRING_PAIR_FAST_SET) != 0u &&
        object_try_set_existing_string_pair_plain_value_assume_non_hidden_unchecked(
                state, receiverObject, slot->cachedReceiverPair, assignedValue)) {
        execution_member_record_helper(state, ZR_PROFILE_HELPER_VALUE_COPY);
        return ZR_TRUE;
    }
    if (object_try_set_existing_pair_plain_value_fast_unchecked(
                state, receiverObject, slot->cachedReceiverPair, assignedValue)) {
        execution_member_record_helper(state, ZR_PROFILE_HELPER_VALUE_COPY);
        return ZR_TRUE;
    }
    ZrCore_Object_SetExistingPairValueUnchecked(state, receiverObject, slot->cachedReceiverPair, assignedValue);
    return ZR_TRUE;
}

static ZR_FORCE_INLINE TZrBool execution_member_try_cached_instance_field_object_get(
        SZrState *state,
        SZrObject *object,
        SZrString *memberName,
        SZrTypeValue *result) {
    const SZrTypeValue *resolvedValue;

    if (state == ZR_NULL || object == ZR_NULL || memberName == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    resolvedValue = object_get_own_string_value_by_name_cached_unchecked(state, object, memberName);
    if (resolvedValue == ZR_NULL) {
        return ZR_FALSE;
    }

    execution_member_record_helper(state, ZR_PROFILE_HELPER_GET_MEMBER);
    execution_member_copy_value_profiled(state, result, resolvedValue);
    return ZR_TRUE;
}

static ZR_FORCE_INLINE TZrBool execution_member_try_cached_instance_field_object_set(
        SZrState *state,
        SZrObject *object,
        SZrString *memberName,
        const SZrTypeValue *assignedValue) {
    SZrHashKeyValuePair *pair;

    if (state == ZR_NULL || object == ZR_NULL || memberName == ZR_NULL || assignedValue == ZR_NULL) {
        return ZR_FALSE;
    }

    pair = object_get_own_string_pair_by_name_cached_unchecked(state, object, memberName);
    if (pair == ZR_NULL) {
        return ZR_FALSE;
    }

    execution_member_record_helper(state, ZR_PROFILE_HELPER_SET_MEMBER);
    if (object_try_set_existing_pair_plain_value_fast_unchecked(state, object, pair, assignedValue)) {
        execution_member_record_helper(state, ZR_PROFILE_HELPER_VALUE_COPY);
        return ZR_TRUE;
    }
    ZrCore_Object_SetExistingPairValueUnchecked(state, object, pair, assignedValue);
    return ZR_TRUE;
}

static ZR_FORCE_INLINE TZrBool execution_member_try_cached_callable_receiver_hit(
        SZrState *state,
        const SZrTypeValue *receiver,
        const SZrFunctionCallSitePicSlot *slot,
        SZrTypeValue *result) {
    SZrObject *receiverObject;

    if (state == ZR_NULL || receiver == ZR_NULL || slot == ZR_NULL || result == ZR_NULL ||
        slot->cachedFunction == ZR_NULL || slot->cachedReceiverObject == ZR_NULL) {
        return ZR_FALSE;
    }

    receiverObject = execution_member_resolve_receiver_object(state, receiver);
    if (receiverObject == ZR_NULL || receiverObject != slot->cachedReceiverObject) {
        return ZR_FALSE;
    }

    execution_member_record_helper(state, ZR_PROFILE_HELPER_GET_MEMBER);
    ZrCore_Value_InitAsRawObject(state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(slot->cachedFunction));
    return ZR_TRUE;
}

static ZR_FORCE_INLINE TZrBool execution_member_try_cached_receiver_fast_get(
        SZrState *state,
        SZrFunction *function,
        TZrUInt16 cacheIndex,
        SZrFunctionCallSiteCacheEntry *entry,
        SZrTypeValue *receiver,
        SZrTypeValue *result) {
    SZrObject *receiverObject;

    if (state == ZR_NULL || function == ZR_NULL || entry == ZR_NULL || receiver == ZR_NULL || result == ZR_NULL ||
        entry->picSlotCount == 0) {
        return ZR_FALSE;
    }

    receiverObject = execution_member_resolve_receiver_object(state, receiver);
    if (receiverObject == ZR_NULL) {
        return ZR_FALSE;
    }

    if (entry->picSlotCount == 1) {
        SZrFunctionCallSitePicSlot *slot = &entry->picSlots[0];
        SZrString *memberName = ZR_NULL;

        if (slot->cachedReceiverObject == receiverObject) {
            if (execution_member_can_use_exact_receiver_pair_hit(slot, receiverObject) &&
                execution_member_try_cached_instance_field_pair_get(state, slot, result)) {
                entry->runtimeHitCount++;
                return ZR_TRUE;
            }
            if ((EZrFunctionCallSitePicAccessKind)slot->cachedAccessKind ==
                ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD) {
                memberName = execution_member_slot_resolve_cached_name(state, function, slot, entry->memberEntryIndex);
                if (memberName != ZR_NULL &&
                    execution_member_try_cached_instance_field_object_get(state, receiverObject, memberName, result)) {
                    entry->runtimeHitCount++;
                    return ZR_TRUE;
                }
            }
            if (slot->cachedOwnerPrototype == ZR_NULL || slot->cachedReceiverPrototype == ZR_NULL) {
                execution_member_clear_cache_entry(function, cacheIndex, entry, "missing-owner-or-receiver");
                return ZR_FALSE;
            }
            if (!execution_member_cached_slot_versions_match(slot)) {
                execution_member_clear_cache_entry(function, cacheIndex, entry, "version-mismatch");
                return ZR_FALSE;
            }
            if (slot->cachedFunction != ZR_NULL) {
                execution_member_record_helper(state, ZR_PROFILE_HELPER_GET_MEMBER);
                ZrCore_Value_InitAsRawObject(state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(slot->cachedFunction));
                entry->runtimeHitCount++;
                return ZR_TRUE;
            }
            if ((EZrFunctionCallSitePicAccessKind)slot->cachedAccessKind ==
                        ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD &&
                slot->cachedReceiverPair != ZR_NULL &&
                execution_member_try_cached_instance_field_pair_get(state, slot, result)) {
                entry->runtimeHitCount++;
                return ZR_TRUE;
            }
            if (slot->cachedDescriptorIndex != ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE) {
                const SZrMemberDescriptor *descriptor = execution_member_try_get_cached_slot_descriptor(slot);

                if (!execution_member_cached_get_requires_result_anchor(descriptor)) {
                    TZrBool resolved = ZrCore_Object_GetMemberCachedDescriptorUnchecked(
                            state, receiver, slot->cachedOwnerPrototype, slot->cachedDescriptorIndex, result);
                    if (!resolved) {
                        execution_member_clear_cache_entry(function, cacheIndex, entry, "cached-get-failed");
                        return ZR_FALSE;
                    }

                    entry->runtimeHitCount++;
                    return ZR_TRUE;
                }
            }
        }
    }

    for (TZrUInt32 slotIndex = 0; slotIndex < entry->picSlotCount; slotIndex++) {
        SZrFunctionCallSitePicSlot *slot = &entry->picSlots[slotIndex];
        TZrBool sameReceiverObject = slot->cachedReceiverObject != ZR_NULL && slot->cachedReceiverObject == receiverObject;
        TZrBool sameReceiverShape;

        if (execution_member_can_use_exact_receiver_pair_hit(slot, receiverObject) &&
            execution_member_try_cached_instance_field_pair_get(state, slot, result)) {
            entry->runtimeHitCount++;
            return ZR_TRUE;
        }

        sameReceiverShape = sameReceiverObject ||
                            execution_member_cached_receiver_shape_matches(slot, receiverObject);
        if (!sameReceiverShape ||
            (EZrFunctionCallSitePicAccessKind)slot->cachedAccessKind !=
                    ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD ||
            slot->cachedReceiverPair == ZR_NULL) {
            continue;
        }
        if (slot->cachedOwnerPrototype == ZR_NULL || slot->cachedReceiverPrototype == ZR_NULL) {
            execution_member_clear_cache_entry(function, cacheIndex, entry, "missing-owner-or-receiver");
            return ZR_FALSE;
        }
        if (receiverObject->prototype != slot->cachedReceiverPrototype) {
            continue;
        }
        if (!execution_member_cached_slot_versions_match(slot)) {
            execution_member_clear_cache_entry(function, cacheIndex, entry, "version-mismatch");
            return ZR_FALSE;
        }
        if (execution_member_try_cached_instance_field_pair_get(state, slot, result)) {
            entry->runtimeHitCount++;
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static ZR_FORCE_INLINE TZrBool execution_member_try_cached_receiver_fast_set(
        SZrState *state,
        SZrFunction *function,
        TZrUInt16 cacheIndex,
        SZrFunctionCallSiteCacheEntry *entry,
        SZrTypeValue *receiver,
        const SZrTypeValue *assignedValue) {
    SZrObject *receiverObject;

    if (state == ZR_NULL || function == ZR_NULL || entry == ZR_NULL || receiver == ZR_NULL || assignedValue == ZR_NULL ||
        entry->picSlotCount == 0) {
        return ZR_FALSE;
    }

    receiverObject = execution_member_resolve_receiver_object(state, receiver);
    if (receiverObject == ZR_NULL) {
        return ZR_FALSE;
    }

    if (entry->picSlotCount == 1) {
        SZrFunctionCallSitePicSlot *slot = &entry->picSlots[0];
        SZrString *memberName = ZR_NULL;

        if (slot->cachedReceiverObject == receiverObject) {
            if (execution_member_can_use_exact_receiver_pair_hit(slot, receiverObject) &&
                execution_member_try_cached_instance_field_pair_set(state, receiverObject, slot, assignedValue)) {
                entry->runtimeHitCount++;
                return ZR_TRUE;
            }
            if ((EZrFunctionCallSitePicAccessKind)slot->cachedAccessKind ==
                ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD) {
                memberName = execution_member_slot_resolve_cached_name(state, function, slot, entry->memberEntryIndex);
                if (memberName != ZR_NULL &&
                    execution_member_try_cached_instance_field_object_set(
                            state, receiverObject, memberName, assignedValue)) {
                    entry->runtimeHitCount++;
                    return ZR_TRUE;
                }
            }
            if (slot->cachedOwnerPrototype == ZR_NULL || slot->cachedReceiverPrototype == ZR_NULL) {
                execution_member_clear_cache_entry(function, cacheIndex, entry, "missing-owner-or-receiver");
                return ZR_FALSE;
            }
            if (!execution_member_cached_slot_versions_match(slot)) {
                execution_member_clear_cache_entry(function, cacheIndex, entry, "version-mismatch");
                return ZR_FALSE;
            }
            if ((EZrFunctionCallSitePicAccessKind)slot->cachedAccessKind ==
                        ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD &&
                slot->cachedReceiverPair != ZR_NULL &&
                execution_member_try_cached_instance_field_pair_set(state, receiverObject, slot, assignedValue)) {
                entry->runtimeHitCount++;
                return ZR_TRUE;
            }
            if (slot->cachedDescriptorIndex != ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE) {
                SZrTypeValue stableReceiver = *receiver;
                SZrTypeValue stableAssignedValue = *assignedValue;

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
        }
    }

    for (TZrUInt32 slotIndex = 0; slotIndex < entry->picSlotCount; slotIndex++) {
        SZrFunctionCallSitePicSlot *slot = &entry->picSlots[slotIndex];
        TZrBool sameReceiverObject = slot->cachedReceiverObject != ZR_NULL && slot->cachedReceiverObject == receiverObject;

        if (execution_member_can_use_exact_receiver_pair_hit(slot, receiverObject) &&
            execution_member_try_cached_instance_field_pair_set(state, receiverObject, slot, assignedValue)) {
            entry->runtimeHitCount++;
            return ZR_TRUE;
        }

        if ((!sameReceiverObject && !execution_member_cached_receiver_shape_matches(slot, receiverObject)) ||
            (EZrFunctionCallSitePicAccessKind)slot->cachedAccessKind !=
                    ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD ||
            slot->cachedReceiverPair == ZR_NULL) {
            continue;
        }
        if (slot->cachedOwnerPrototype == ZR_NULL || slot->cachedReceiverPrototype == ZR_NULL) {
            execution_member_clear_cache_entry(function, cacheIndex, entry, "missing-owner-or-receiver");
            return ZR_FALSE;
        }
        if (receiverObject->prototype != slot->cachedReceiverPrototype) {
            continue;
        }
        if (!execution_member_cached_slot_versions_match(slot)) {
            execution_member_clear_cache_entry(function, cacheIndex, entry, "version-mismatch");
            return ZR_FALSE;
        }
        if (execution_member_try_cached_instance_field_pair_set(state, receiverObject, slot, assignedValue)) {
            entry->runtimeHitCount++;
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static ZR_FORCE_INLINE TZrBool execution_member_try_cached_instance_field_get(SZrState *state,
                                                                              SZrTypeValue *receiver,
                                                                              SZrString *memberName,
                                                                              SZrTypeValue *result) {
    SZrObject *object;

    if (state == ZR_NULL || receiver == ZR_NULL || memberName == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    object = execution_member_fast_instance_field_object(state, receiver);
    return execution_member_try_cached_instance_field_object_get(state, object, memberName, result);
}

static ZR_FORCE_INLINE TZrBool execution_member_try_cached_instance_field_set(SZrState *state,
                                                                              SZrTypeValue *receiver,
                                                                              SZrString *memberName,
                                                                              const SZrTypeValue *assignedValue) {
    SZrObject *object;

    if (state == ZR_NULL || receiver == ZR_NULL || memberName == ZR_NULL || assignedValue == ZR_NULL) {
        return ZR_FALSE;
    }

    object = execution_member_fast_instance_field_object(state, receiver);
    return execution_member_try_cached_instance_field_object_set(state, object, memberName, assignedValue);
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
    if (execution_member_try_cached_receiver_fast_get(state, function, cacheIndex, entry, receiver, result)) {
        return ZR_TRUE;
    }

    receiverPrototype = execution_member_resolve_receiver_prototype(state, receiver);
    if (receiverPrototype == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 slotIndex = 0; slotIndex < entry->picSlotCount; slotIndex++) {
        SZrFunctionCallSitePicSlot *slot = &entry->picSlots[slotIndex];
        const SZrMemberDescriptor *descriptor = ZR_NULL;
        SZrString *memberName = ZR_NULL;
        SZrTypeValue stableReceiver;
        SZrTypeValue stableResult;
        SZrFunctionStackAnchor resultAnchor;
        TZrBool hasResultAnchor;
        SZrTypeValue *targetResult = result;

        if (slot->cachedReceiverPrototype != receiverPrototype) {
            continue;
        }
        if (slot->cachedOwnerPrototype == ZR_NULL) {
            execution_member_clear_cache_entry(function, cacheIndex, entry, "missing-owner-or-descriptor");
            return ZR_FALSE;
        }
        if (slot->cachedReceiverPrototype->super.memberVersion != slot->cachedReceiverVersion ||
            slot->cachedOwnerPrototype->super.memberVersion != slot->cachedOwnerVersion) {
            execution_member_clear_cache_entry(function, cacheIndex, entry, "version-mismatch");
            return ZR_FALSE;
        }

        if (execution_member_try_cached_callable_receiver_hit(state, receiver, slot, result)) {
            entry->runtimeHitCount++;
            return ZR_TRUE;
        }
        if ((EZrFunctionCallSitePicAccessKind)slot->cachedAccessKind ==
            ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD) {
            memberName = execution_member_slot_resolve_cached_name(state, function, slot, entry->memberEntryIndex);
            if (memberName != ZR_NULL &&
                execution_member_try_cached_instance_field_get(state, receiver, memberName, result)) {
                entry->runtimeHitCount++;
                return ZR_TRUE;
            }
            if (slot->cachedDescriptorIndex == ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE) {
                execution_member_clear_cache_entry(function, cacheIndex, entry, "instance-field-miss");
                return ZR_FALSE;
            }
        }
        if (slot->cachedDescriptorIndex == ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE) {
            execution_member_clear_cache_entry(function, cacheIndex, entry, "missing-owner-or-descriptor");
            return ZR_FALSE;
        }
        if (slot->cachedFunction != ZR_NULL) {
            if (!ZrCore_Object_GetMemberCachedCallableUnchecked(state,
                                                                receiver,
                                                                slot->cachedOwnerPrototype,
                                                                slot->cachedDescriptorIndex,
                                                                slot->cachedFunction,
                                                                result)) {
                execution_member_clear_cache_entry(function, cacheIndex, entry, "cached-get-failed");
                return ZR_FALSE;
            }

            entry->runtimeHitCount++;
            return ZR_TRUE;
        }
        if (slot->cachedOwnerPrototype->memberDescriptors != ZR_NULL &&
            slot->cachedDescriptorIndex < slot->cachedOwnerPrototype->memberDescriptorCount) {
            descriptor = &slot->cachedOwnerPrototype->memberDescriptors[slot->cachedDescriptorIndex];
        }

        if (!execution_member_cached_get_requires_result_anchor(descriptor)) {
            TZrBool resolved = ZrCore_Object_GetMemberCachedDescriptorUnchecked(state,
                                                                                receiver,
                                                                                slot->cachedOwnerPrototype,
                                                                                slot->cachedDescriptorIndex,
                                                                                result);
            if (!resolved) {
                execution_member_clear_cache_entry(function, cacheIndex, entry, "cached-get-failed");
                return ZR_FALSE;
            }

            entry->runtimeHitCount++;
            return ZR_TRUE;
        }

        stableReceiver = *receiver;
        hasResultAnchor = execution_member_try_anchor_stack_value(state, result, &resultAnchor);
        if (hasResultAnchor) {
            ZrCore_Value_ResetAsNull(&stableResult);
            targetResult = &stableResult;
        }
        if (!ZrCore_Object_GetMemberCachedDescriptorUnchecked(state,
                                                              &stableReceiver,
                                                              slot->cachedOwnerPrototype,
                                                              slot->cachedDescriptorIndex,
                                                              targetResult)) {
            execution_member_clear_cache_entry(function, cacheIndex, entry, "cached-get-failed");
            return ZR_FALSE;
        }
        if (hasResultAnchor) {
            result = ZrCore_Stack_GetValue(ZrCore_Function_StackAnchorRestore(state, &resultAnchor));
            ZrCore_Value_Copy(state, result, &stableResult);
        }

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
    if (execution_member_try_cached_receiver_fast_set(state, function, cacheIndex, entry, receiver, assignedValue)) {
        return ZR_TRUE;
    }

    receiverPrototype = execution_member_resolve_receiver_prototype(state, receiver);
    if (receiverPrototype == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 slotIndex = 0; slotIndex < entry->picSlotCount; slotIndex++) {
        SZrFunctionCallSitePicSlot *slot = &entry->picSlots[slotIndex];
        SZrString *memberName = ZR_NULL;
        SZrTypeValue stableReceiver;
        SZrTypeValue stableAssignedValue;

        if (slot->cachedReceiverPrototype != receiverPrototype) {
            continue;
        }
        if (slot->cachedOwnerPrototype == ZR_NULL) {
            execution_member_clear_cache_entry(function, cacheIndex, entry, "missing-owner-or-descriptor");
            return ZR_FALSE;
        }
        if (slot->cachedReceiverPrototype->super.memberVersion != slot->cachedReceiverVersion ||
            slot->cachedOwnerPrototype->super.memberVersion != slot->cachedOwnerVersion) {
            execution_member_clear_cache_entry(function, cacheIndex, entry, "version-mismatch");
            return ZR_FALSE;
        }

        if ((EZrFunctionCallSitePicAccessKind)slot->cachedAccessKind ==
            ZR_FUNCTION_CALLSITE_PIC_ACCESS_KIND_INSTANCE_FIELD) {
            memberName = execution_member_slot_resolve_cached_name(state, function, slot, entry->memberEntryIndex);
            if (memberName != ZR_NULL &&
                execution_member_try_cached_instance_field_set(state, receiver, memberName, assignedValue)) {
                entry->runtimeHitCount++;
                return ZR_TRUE;
            }
            if (slot->cachedDescriptorIndex == ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE) {
                execution_member_clear_cache_entry(function, cacheIndex, entry, "instance-field-miss");
                return ZR_FALSE;
            }
        }
        if (slot->cachedDescriptorIndex == ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE) {
            execution_member_clear_cache_entry(function, cacheIndex, entry, "missing-owner-or-descriptor");
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

    execution_member_refresh_forwarded_value_copy(receiver);

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
        SZrTypeValue *targetResult = result;

        stableReceiver = *receiver;
        hasResultAnchor = execution_member_try_anchor_stack_value(state, result, &resultAnchor);
        if (hasResultAnchor) {
            ZrCore_Value_ResetAsNull(&stableResult);
            targetResult = &stableResult;
        }
        callInfo = execution_member_prepare_protected_call(state, programCounter);
        resolved = ZrCore_Object_GetMemberWithKeyUnchecked(
                state, &stableReceiver, memberName, &memberKey, targetResult);
        if (!resolved &&
            execution_try_materialize_global_prototypes(
                    state,
                    execution_member_current_closure(state),
                    callInfo,
                    &stableReceiver,
                    &memberKey)) {
            resolved = ZrCore_Object_GetMemberWithKeyUnchecked(
                    state, &stableReceiver, memberName, &memberKey, targetResult);
        }

        if (resolved && hasResultAnchor) {
            result = ZrCore_Stack_GetValue(ZrCore_Function_StackAnchorRestore(state, &resultAnchor));
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
    const SZrTypeValue *effectiveAssignedValue = assignedValue;
    TZrBool fastHandled = ZR_FALSE;
    TZrBool resolved = ZR_FALSE;

    if (state == ZR_NULL || receiverAndResult == ZR_NULL || memberName == ZR_NULL || assignedValue == ZR_NULL) {
        return ZR_FALSE;
    }
    if (receiverAndResult->type != ZR_VALUE_TYPE_OBJECT && receiverAndResult->type != ZR_VALUE_TYPE_ARRAY) {
        return ZR_FALSE;
    }

    execution_member_refresh_forwarded_value_copy(receiverAndResult);
    stableAssignedValue = *assignedValue;
    execution_member_refresh_forwarded_value_copy(&stableAssignedValue);
    effectiveAssignedValue = &stableAssignedValue;

    execution_member_make_key(state, memberName, &memberKey);
    if (ZrCore_Object_TrySetMemberWithKeyFastUnchecked(
                state, receiverAndResult, memberName, &memberKey, effectiveAssignedValue, &fastHandled)) {
        return ZR_TRUE;
    }
    if (fastHandled) {
        return ZR_FALSE;
    }

    stableReceiver = *receiverAndResult;
    execution_member_refresh_forwarded_value_copy(&stableReceiver);
    execution_member_prepare_protected_call(state, programCounter);
    resolved = ZrCore_Object_SetMemberWithKeyUnchecked(
            state, &stableReceiver, memberName, &memberKey, effectiveAssignedValue);
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
    SZrTypeValue stableReceiver;
    SZrTypeValue *refreshReceiver = receiver;

    if (state == ZR_NULL || function == ZR_NULL || receiver == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    execution_member_refresh_forwarded_value_copy(receiver);

    entry = execution_member_get_cache_entry_fast(function, cacheIndex, ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET);
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

    if (receiver == result) {
        stableReceiver = *receiver;
        refreshReceiver = &stableReceiver;
    }

    entry->runtimeMissCount++;
    if (!execution_member_get_by_name(state, programCounter, receiver, memberName, result)) {
        return ZR_FALSE;
    }

    execution_member_refresh_cache(state, function, cacheIndex, entry, refreshReceiver, memberName);
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
    SZrTypeValue stableAssignedValue;
    const SZrTypeValue *effectiveAssignedValue = assignedValue;

    if (state == ZR_NULL || function == ZR_NULL || receiverAndResult == ZR_NULL || assignedValue == ZR_NULL) {
        return ZR_FALSE;
    }

    execution_member_refresh_forwarded_value_copy(receiverAndResult);
    stableAssignedValue = *assignedValue;
    execution_member_refresh_forwarded_value_copy(&stableAssignedValue);
    effectiveAssignedValue = &stableAssignedValue;

    entry = execution_member_get_cache_entry_fast(function, cacheIndex, ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET);
    if (entry == ZR_NULL) {
        return ZR_FALSE;
    }

    if (execution_member_try_cached_set(state, function, cacheIndex, entry, receiverAndResult, effectiveAssignedValue)) {
        return ZR_TRUE;
    }

    memberName = execution_member_cache_resolve_name(function, entry->memberEntryIndex);
    if (memberName == ZR_NULL) {
        return ZR_FALSE;
    }

    entry->runtimeMissCount++;
    if (!execution_member_set_by_name(state, programCounter, receiverAndResult, memberName, effectiveAssignedValue)) {
        return ZR_FALSE;
    }

    execution_member_refresh_cache(state, function, cacheIndex, entry, receiverAndResult, memberName);
    return ZR_TRUE;
}
