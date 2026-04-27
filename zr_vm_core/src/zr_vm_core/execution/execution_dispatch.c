//
// Dispatch loop for VM execution.
//

#include "execution/execution_internal.h"
#include "function_precall_internal.h"
#include "object/object_internal.h"
#include "object/object_super_array_internal.h"

#include <stdarg.h>
#include <stdio.h>

static ZR_FORCE_INLINE SZrRawObject *execution_refresh_forwarded_raw_object(SZrRawObject *rawObject);
static ZR_FORCE_INLINE SZrFunction *execution_refresh_forwarded_function(SZrFunction *function);
static ZR_FORCE_INLINE SZrClosure *execution_refresh_forwarded_closure(SZrClosure *closure);
static ZR_FORCE_INLINE SZrFunction *execution_try_resolve_stateless_vm_function_value_fast(
        SZrState *state,
        SZrTypeValue *value);
static ZR_FORCE_INLINE SZrFunction *execution_try_resolve_vm_metadata_function_fast(SZrState *state,
                                                                                     SZrTypeValue *value,
                                                                                     SZrRawObject **outCallableObject);
static ZR_FORCE_INLINE SZrCallInfo *execution_pre_call_known_vm_fast(SZrState *state,
                                                                     TZrStackValuePointer stackPointer,
                                                                     SZrTypeValue *callableValue,
                                                                     TZrSize argumentsCount,
                                                                     TZrSize resultCount,
                                                                     TZrStackValuePointer returnDestination,
                                                                     SZrProfileRuntime *profileRuntime,
                                                                     TZrBool recordHelpers);
static ZR_FORCE_INLINE SZrCallInfo *execution_pre_call_known_native_fast(SZrState *state,
                                                                         TZrStackValuePointer stackPointer,
                                                                         SZrTypeValue *callableValue,
                                                                         TZrSize resultCount,
                                                                         TZrStackValuePointer returnDestination,
                                                                         SZrProfileRuntime *profileRuntime,
                                                                         TZrBool recordHelpers);
static ZR_FORCE_INLINE SZrCallInfo *execution_pre_call_prepared_resolved_vm_fast(
        SZrState *state,
        TZrStackValuePointer stackPointer,
        SZrFunction *resolvedFunction,
        TZrSize argumentsCount,
        TZrSize resultCount,
        TZrStackValuePointer returnDestination);

static ZR_FORCE_INLINE TZrBool execution_can_copy_stack_value_by_bits(const SZrTypeValue *destination,
                                                                      const SZrTypeValue *source) {
    ZR_ASSERT(destination != ZR_NULL);
    ZR_ASSERT(source != ZR_NULL);
    ZR_ASSERT(source->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE ||
              (source->ownershipControl == ZR_NULL && source->ownershipWeakRef == ZR_NULL));
    ZR_ASSERT(destination->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE ||
              (destination->ownershipControl == ZR_NULL && destination->ownershipWeakRef == ZR_NULL));
    return (TZrBool)(((TZrUInt32)source->ownershipKind | (TZrUInt32)destination->ownershipKind) ==
                     (TZrUInt32)ZR_OWNERSHIP_VALUE_KIND_NONE);
}

static ZR_FORCE_INLINE void execution_copy_value_to_ret_fast_no_profile(SZrTypeValue *destination,
                                                                         const SZrTypeValue *source) {
    ZR_ASSERT(destination != ZR_NULL);
    ZR_ASSERT(source != ZR_NULL);
    *destination = *source;
}

static ZR_FORCE_INLINE void execution_copy_stack_value_to_stack_fast_no_profile(SZrState *state,
                                                                                SZrTypeValue *destination,
                                                                                const SZrTypeValue *source) {
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(destination != ZR_NULL);
    ZR_ASSERT(source != ZR_NULL);
    if (ZR_LIKELY(execution_can_copy_stack_value_by_bits(destination, source))) {
        *destination = *source;
        return;
    }

    ZrCore_Value_CopySlow(state, destination, source);
}

static ZR_FORCE_INLINE void execution_reset_stack_value_to_null_fast_no_profile(SZrState *state,
                                                                                SZrTypeValue *destination) {
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(destination != ZR_NULL);
    if (ZR_UNLIKELY(destination->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE)) {
        ZrCore_Ownership_ReleaseValue(state, destination);
        return;
    }

    ZR_ASSERT(destination->ownershipControl == ZR_NULL && destination->ownershipWeakRef == ZR_NULL);
    ZrCore_Value_ResetAsNullNoProfile(destination);
}

static ZR_FORCE_INLINE void execution_assign_stack_value_to_stack_fast_no_profile(SZrState *state,
                                                                                  SZrTypeValue *destination,
                                                                                  SZrTypeValue *source) {
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(destination != ZR_NULL);
    ZR_ASSERT(source != ZR_NULL);

    if (destination == source) {
        return;
    }

    /*
     * SET_STACK materializes expression results into their destination slots.
     * Ownership-wrapped temporaries such as %shared(...) must transfer their
     * wrapper into the destination slot instead of leaving an extra temp
     * owner alive until frame teardown.
     */
    ZrCore_Value_AssignMaterializedStackValueNoProfile(state, destination, source);
}

static ZR_FORCE_INLINE void execution_post_call_single_result_resolved_source_fast(
        SZrState *state,
        SZrCallInfo *callInfo,
        TZrStackValuePointer returnSource) {
    TZrStackValuePointer destination;
    SZrTypeValue *destinationValue;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(callInfo != ZR_NULL);
    ZR_ASSERT(state->debugHookSignal == 0u);
    ZR_ASSERT(callInfo->expectedReturnCount == 1u);
    ZR_ASSERT(callInfo->callStatus == ZR_CALL_STATUS_NONE);

    destination = callInfo->hasReturnDestination ? callInfo->returnDestination : callInfo->functionBase.valuePointer;
    ZR_ASSERT(destination != ZR_NULL);

    destinationValue = ZrCore_Stack_GetValueNoProfile(destination);
    if (returnSource == ZR_NULL) {
        ZrCore_Value_ResetAsNullNoProfile(destinationValue);
    } else if (returnSource != destination) {
        execution_copy_stack_value_to_stack_fast_no_profile(
                state,
                destinationValue,
                ZrCore_Stack_GetValueNoProfile(returnSource));
    }

    state->stackTop.valuePointer = destination + 1;
    state->callInfoList = callInfo->previous;
}

static ZR_FORCE_INLINE void execution_prepare_destination_for_direct_store_no_profile(SZrState *state,
                                                                                      SZrTypeValue *destination) {
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(destination != ZR_NULL);

    if (destination->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE) {
        ZrCore_Ownership_ReleaseValue(state, destination);
    } else {
        ZR_ASSERT(destination->ownershipControl == ZR_NULL && destination->ownershipWeakRef == ZR_NULL);
    }
}

static ZR_FORCE_INLINE void execution_store_vm_function_value_no_profile(SZrTypeValue *destination,
                                                                         SZrFunction *function) {
    ZR_ASSERT(destination != ZR_NULL);
    ZR_ASSERT(function != ZR_NULL);

    destination->type = ZR_VALUE_TYPE_FUNCTION;
    destination->value.object = ZR_CAST_RAW_OBJECT_AS_SUPER(function);
    destination->isGarbageCollectable = ZR_TRUE;
    destination->isNative = ZR_FALSE;
    destination->ownershipKind = ZR_OWNERSHIP_VALUE_KIND_NONE;
    destination->ownershipControl = ZR_NULL;
    destination->ownershipWeakRef = ZR_NULL;
}

static ZR_FORCE_INLINE void execution_copy_value_fast(SZrState *state,
                                                      SZrTypeValue *destination,
                                                      const SZrTypeValue *source,
                                                      SZrProfileRuntime *profileRuntime,
                                                      TZrBool recordHelpers) {
    if (ZR_UNLIKELY(recordHelpers)) {
        profileRuntime->helperCounts[ZR_PROFILE_HELPER_VALUE_COPY]++;
    }

    if (destination == source) {
        return;
    }

    /*
     * This helper only serves interpreter stack/ret destinations, so it does
     * not need the conservative object-field barrier constraints of the generic
     * value copy fast path.
     */
    if (ZR_LIKELY(execution_can_copy_stack_value_by_bits(destination, source))) {
        *destination = *source;
        return;
    }

    ZrCore_Value_CopySlow(state, destination, source);
}

static ZR_FORCE_INLINE SZrTypeValue *execution_stack_get_value_fast(SZrTypeValueOnStack *valueOnStack,
                                                                    SZrProfileRuntime *profileRuntime,
                                                                    TZrBool recordHelpers) {
    if (ZR_UNLIKELY(recordHelpers)) {
        profileRuntime->helperCounts[ZR_PROFILE_HELPER_STACK_GET_VALUE]++;
    }
    return ZrCore_Stack_GetValueNoProfile(valueOnStack);
}

static ZR_FORCE_INLINE SZrClosure *execution_get_current_vm_closure_no_profile(SZrState *state,
                                                                               TZrStackValuePointer base) {
    SZrTypeValue *callableValue = ZrCore_Stack_GetValueNoProfile(base - 1);
    return callableValue != ZR_NULL ? ZR_CAST_VM_CLOSURE(state, callableValue->value.object) : ZR_NULL;
}

static ZR_FORCE_INLINE SZrTypeValue *execution_get_closure_value_no_profile(SZrClosureValue *closureValue) {
    if (closureValue == ZR_NULL) {
        return ZR_NULL;
    }

    if (ZrCore_ClosureValue_IsClosed(closureValue)) {
        return &closureValue->link.closedValue;
    }

    return ZrCore_Stack_GetValueNoProfile(closureValue->value.valuePointer);
}

static ZR_FORCE_INLINE SZrCallInfo *execution_pre_call_known_vm_fast(SZrState *state,
                                                                     TZrStackValuePointer stackPointer,
                                                                     SZrTypeValue *callableValue,
                                                                     TZrSize argumentsCount,
                                                                     TZrSize resultCount,
                                                                     TZrStackValuePointer returnDestination,
                                                                     SZrProfileRuntime *profileRuntime,
                                                                     TZrBool recordHelpers) {
    SZrFunction *resolvedFunction = ZR_NULL;

    if (ZR_UNLIKELY(recordHelpers)) {
        profileRuntime->helperCounts[ZR_PROFILE_HELPER_PRECALL]++;
    }

    if (callableValue != ZR_NULL && !callableValue->isNative) {
        resolvedFunction = execution_try_resolve_stateless_vm_function_value_fast(state, callableValue);
        if (resolvedFunction == ZR_NULL) {
            resolvedFunction = execution_try_resolve_vm_metadata_function_fast(state, callableValue, ZR_NULL);
            if (callableValue->type == ZR_VALUE_TYPE_FUNCTION &&
                (resolvedFunction == ZR_NULL || resolvedFunction->closureValueLength != 0)) {
                resolvedFunction = ZR_NULL;
            }
        }
    }

    if (ZR_LIKELY(resolvedFunction != ZR_NULL)) {
        return execution_pre_call_prepared_resolved_vm_fast(
                state,
                stackPointer,
                resolvedFunction,
                argumentsCount,
                resultCount,
                returnDestination);
    }

    return ZrCore_Function_PreCallKnownVmValue(state, stackPointer, callableValue, resultCount, returnDestination);
}

static ZR_FORCE_INLINE void execution_store_resolved_vm_function_value_fast(SZrState *state,
                                                                            SZrTypeValue *destination,
                                                                            SZrFunction *function) {
    if (state == ZR_NULL || destination == ZR_NULL || function == ZR_NULL) {
        return;
    }

    execution_prepare_destination_for_direct_store_no_profile(state, destination);
    execution_store_vm_function_value_no_profile(destination, function);
}

static ZR_FORCE_INLINE SZrCallInfo *execution_pre_call_prepared_resolved_vm_fast(
        SZrState *state,
        TZrStackValuePointer stackPointer,
        SZrFunction *resolvedFunction,
        TZrSize argumentsCount,
        TZrSize resultCount,
        TZrStackValuePointer returnDestination) {
    SZrCallInfo *callInfo;

    callInfo = function_try_pre_call_prepared_resolved_vm_exact_args_steady_state_inline(
            state,
            stackPointer,
            resolvedFunction,
            argumentsCount,
            resultCount,
            returnDestination);
    if (ZR_LIKELY(callInfo != ZR_NULL)) {
        return callInfo;
    }

    return ZrCore_Function_PreCallPreparedResolvedVmFunction(state,
                                                             stackPointer,
                                                             resolvedFunction,
                                                             argumentsCount,
                                                             resultCount,
                                                             returnDestination);
}

static ZR_FORCE_INLINE TZrBool execution_try_resolve_known_vm_member_exact_single_slot_fast(
        SZrFunctionCallSiteCacheEntry *cacheEntry,
        const SZrTypeValue *receiverValue,
        SZrFunction **outResolvedFunction,
        TZrUInt32 *outArgumentCount) {
    SZrFunctionCallSitePicSlot *slot;
    SZrObject *receiverObject;

    if (outResolvedFunction != ZR_NULL) {
        *outResolvedFunction = ZR_NULL;
    }
    if (outArgumentCount != ZR_NULL) {
        *outArgumentCount = 0u;
    }
    if (cacheEntry == ZR_NULL || receiverValue == ZR_NULL || cacheEntry->picSlotCount != 1u || cacheEntry->argumentCount == 0u ||
        receiverValue->value.object == ZR_NULL ||
        (receiverValue->type != ZR_VALUE_TYPE_OBJECT && receiverValue->type != ZR_VALUE_TYPE_ARRAY)) {
        return ZR_FALSE;
    }

    slot = &cacheEntry->picSlots[0];
    if (slot->cachedFunction == ZR_NULL || slot->cachedReceiverObject == ZR_NULL ||
        slot->cachedOwnerPrototype == ZR_NULL || slot->cachedReceiverPrototype == ZR_NULL ||
        receiverValue->value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(slot->cachedReceiverObject)) {
        return ZR_FALSE;
    }

    receiverObject = ZR_CAST_OBJECT(ZR_NULL, receiverValue->value.object);
    if (receiverObject == ZR_NULL || receiverObject->internalType == ZR_OBJECT_INTERNAL_TYPE_MODULE ||
        receiverObject->prototype != slot->cachedReceiverPrototype ||
        slot->cachedReceiverPrototype->super.memberVersion != slot->cachedReceiverVersion ||
        slot->cachedOwnerPrototype->super.memberVersion != slot->cachedOwnerVersion ||
        slot->cachedFunction->closureValueLength != 0u) {
        return ZR_FALSE;
    }

    if (outResolvedFunction != ZR_NULL) {
        *outResolvedFunction = slot->cachedFunction;
    }
    if (outArgumentCount != ZR_NULL) {
        *outArgumentCount = cacheEntry->argumentCount;
    }
    cacheEntry->runtimeHitCount++;
    return ZR_TRUE;
}

static ZR_FORCE_INLINE SZrCallInfo *execution_pre_call_known_vm_member_fast(
        SZrState *state,
        const TZrInstruction *programCounter,
        SZrFunction *currentFunction,
        TZrUInt16 cacheIndex,
        SZrFunctionCallSiteCacheEntry *cacheEntry,
        TZrStackValuePointer stackPointer,
        TZrSize resultCount,
        TZrStackValuePointer returnDestination,
        SZrProfileRuntime *profileRuntime,
        TZrBool recordHelpers) {
    SZrTypeValue *callableValue;
    SZrTypeValue *receiverValue;
    SZrFunction *resolvedFunction = ZR_NULL;
    TZrUInt32 cachedArgumentCount = cacheEntry != ZR_NULL ? cacheEntry->argumentCount : 0u;

    if (ZR_UNLIKELY(recordHelpers)) {
        profileRuntime->helperCounts[ZR_PROFILE_HELPER_PRECALL]++;
    }

    callableValue = ZrCore_Stack_GetValueNoProfile(stackPointer);
    receiverValue = ZrCore_Stack_GetValueNoProfile(stackPointer + 1);
    if (receiverValue != ZR_NULL &&
        execution_try_resolve_known_vm_member_exact_single_slot_fast(
                cacheEntry, receiverValue, &resolvedFunction, &cachedArgumentCount) &&
        resolvedFunction != ZR_NULL) {
        /*
         * The direct member-call opcode skips materializing the bound callable via
         * GET_MEMBER_SLOT, but VM frames still rely on stackPointer[0] carrying a
         * function/closure value so frame metadata can be reconstructed on entry.
         */
        execution_store_resolved_vm_function_value_fast(state, callableValue, resolvedFunction);
        return execution_pre_call_prepared_resolved_vm_fast(state,
                                                            stackPointer,
                                                            resolvedFunction,
                                                            cachedArgumentCount,
                                                            resultCount,
                                                            returnDestination);
    }
    if (receiverValue != ZR_NULL &&
        execution_member_try_resolve_cached_known_vm_function_entry(state,
                                                                    currentFunction,
                                                                    cacheIndex,
                                                                    cacheEntry,
                                                                    receiverValue,
                                                                    &resolvedFunction,
                                                                    &cachedArgumentCount) &&
        resolvedFunction != ZR_NULL) {
        /*
         * The direct member-call opcode skips materializing the bound callable via
         * GET_MEMBER_SLOT, but VM frames still rely on stackPointer[0] carrying a
         * function/closure value so frame metadata can be reconstructed on entry.
         */
        execution_store_resolved_vm_function_value_fast(state, callableValue, resolvedFunction);
        return execution_pre_call_prepared_resolved_vm_fast(state,
                                                            stackPointer,
                                                            resolvedFunction,
                                                            cachedArgumentCount,
                                                            resultCount,
                                                            returnDestination);
    }

    if (callableValue == ZR_NULL || receiverValue == ZR_NULL ||
        !execution_member_get_cached(state, programCounter, currentFunction, cacheIndex, receiverValue, callableValue)) {
        return ZR_NULL;
    }

    return execution_pre_call_known_vm_fast(state,
                                            stackPointer,
                                            callableValue,
                                            cachedArgumentCount,
                                            resultCount,
                                            returnDestination,
                                            profileRuntime,
                                            ZR_FALSE);
}

static ZR_FORCE_INLINE SZrCallInfo *execution_pre_call_known_native_fast(SZrState *state,
                                                                         TZrStackValuePointer stackPointer,
                                                                         SZrTypeValue *callableValue,
                                                                         TZrSize resultCount,
                                                                         TZrStackValuePointer returnDestination,
                                                                         SZrProfileRuntime *profileRuntime,
                                                                         TZrBool recordHelpers) {
    if (ZR_UNLIKELY(recordHelpers)) {
        profileRuntime->helperCounts[ZR_PROFILE_HELPER_PRECALL]++;
    }
    return ZrCore_Function_PreCallKnownNativeValue(
            state,
            stackPointer,
            callableValue,
            resultCount,
            returnDestination);
}

static ZR_FORCE_INLINE TZrBool execution_string_values_equal_fast(SZrState *state,
                                                                  const SZrTypeValue *leftValue,
                                                                  const SZrTypeValue *rightValue) {
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(leftValue != ZR_NULL);
    ZR_ASSERT(rightValue != ZR_NULL);
    ZR_ASSERT(leftValue->type == ZR_VALUE_TYPE_STRING);
    ZR_ASSERT(rightValue->type == ZR_VALUE_TYPE_STRING);
    ZR_ASSERT(leftValue->value.object != ZR_NULL);
    ZR_ASSERT(rightValue->value.object != ZR_NULL);
    return ZrCore_String_Equal(ZR_CAST_STRING(state, leftValue->value.object),
                               ZR_CAST_STRING(state, rightValue->value.object));
}

#define ZrCore_Value_ResetAsNull ZrCore_Value_ResetAsNullNoProfile

static ZR_FORCE_INLINE SZrRawObject *execution_refresh_forwarded_raw_object(SZrRawObject *rawObject) {
    SZrRawObject *forwardedObject;

    if (rawObject == ZR_NULL) {
        return ZR_NULL;
    }

    forwardedObject = (SZrRawObject *)rawObject->garbageCollectMark.forwardingAddress;
    return forwardedObject != ZR_NULL ? forwardedObject : rawObject;
}

static ZR_FORCE_INLINE SZrFunction *execution_refresh_forwarded_function(SZrFunction *function) {
    return function != ZR_NULL
                   ? (SZrFunction *)execution_refresh_forwarded_raw_object(ZR_CAST_RAW_OBJECT_AS_SUPER(function))
                   : ZR_NULL;
}

static ZR_FORCE_INLINE SZrClosure *execution_refresh_forwarded_closure(SZrClosure *closure) {
    return closure != ZR_NULL
                   ? (SZrClosure *)execution_refresh_forwarded_raw_object(ZR_CAST_RAW_OBJECT_AS_SUPER(closure))
                   : ZR_NULL;
}

static ZR_FORCE_INLINE SZrFunction *execution_try_resolve_stateless_vm_function_value_fast(
        SZrState *state,
        SZrTypeValue *value) {
    SZrRawObject *rawObject;
    SZrFunction *function;

    if (state == ZR_NULL || value == ZR_NULL || value->type != ZR_VALUE_TYPE_FUNCTION || value->isNative ||
        value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    rawObject = execution_refresh_forwarded_raw_object(value->value.object);
    if (rawObject == ZR_NULL || rawObject->type != ZR_RAW_OBJECT_TYPE_FUNCTION) {
        return ZR_NULL;
    }

    function = ZR_CAST_FUNCTION(state, rawObject);
    if (function == ZR_NULL || function->closureValueLength != 0u) {
        return ZR_NULL;
    }

    value->value.object = ZR_CAST_RAW_OBJECT_AS_SUPER(function);
    return function;
}

static SZrString *execution_refresh_forwarded_string(SZrString *stringValue) {
    SZrRawObject *rawObject;

    if (stringValue == ZR_NULL) {
        return ZR_NULL;
    }

    rawObject = execution_refresh_forwarded_raw_object(ZR_CAST_RAW_OBJECT_AS_SUPER(stringValue));
    if (rawObject == ZR_CAST_RAW_OBJECT_AS_SUPER(stringValue)) {
        return stringValue;
    }

    return ZR_CAST_STRING(ZR_NULL, rawObject);
}

static ZR_FORCE_INLINE SZrFunction *execution_try_resolve_vm_metadata_function_fast(SZrState *state,
                                                                                     SZrTypeValue *value,
                                                                                     SZrRawObject **outCallableObject) {
    SZrRawObject *rawObject;

    if (outCallableObject != ZR_NULL) {
        *outCallableObject = ZR_NULL;
    }

    if (state == ZR_NULL || value == ZR_NULL || value->isNative || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    rawObject = execution_refresh_forwarded_raw_object(value->value.object);
    if (rawObject == ZR_NULL) {
        return ZR_NULL;
    }

    switch (value->type) {
        case ZR_VALUE_TYPE_FUNCTION: {
            SZrFunction *function = execution_refresh_forwarded_function(ZR_CAST_FUNCTION(state, rawObject));

            if (function == ZR_NULL) {
                return ZR_NULL;
            }

            rawObject = ZR_CAST_RAW_OBJECT_AS_SUPER(function);
            value->value.object = rawObject;
            if (outCallableObject != ZR_NULL) {
                *outCallableObject = rawObject;
            }
            return function;
        }

        case ZR_VALUE_TYPE_CLOSURE: {
            SZrClosure *closure = execution_refresh_forwarded_closure(ZR_CAST_VM_CLOSURE(state, rawObject));
            SZrFunction *function;

            if (closure == ZR_NULL) {
                return ZR_NULL;
            }

            function = execution_refresh_forwarded_function(closure->function);
            if (function == ZR_NULL) {
                return ZR_NULL;
            }

            rawObject = ZR_CAST_RAW_OBJECT_AS_SUPER(closure);
            value->value.object = rawObject;
            if (outCallableObject != ZR_NULL) {
                *outCallableObject = rawObject;
            }
            return function;
        }

        default:
            return ZR_NULL;
    }
}

static SZrString *execution_resolve_function_member_symbol(SZrFunction *function, TZrUInt32 memberEntryIndex) {
    SZrString *memberSymbol;

    if (function == ZR_NULL || function->memberEntries == ZR_NULL || memberEntryIndex >= function->memberEntryLength) {
        return ZR_NULL;
    }

    memberSymbol = execution_refresh_forwarded_string(function->memberEntries[memberEntryIndex].symbol);
    function->memberEntries[memberEntryIndex].symbol = memberSymbol;
    return memberSymbol;
}

static SZrString *execution_resolve_member_symbol(SZrClosure *closure, TZrUInt16 memberId) {
    return execution_resolve_function_member_symbol(closure != ZR_NULL ? closure->function : ZR_NULL, memberId);
}

static SZrString *execution_resolve_cached_member_symbol(SZrFunction *function,
                                                         TZrUInt16 cacheIndex,
                                                         EZrFunctionCallSiteCacheKind expectedKind) {
    const SZrFunctionCallSiteCacheEntry *cacheEntry = execution_get_callsite_cache_entry(function, cacheIndex, expectedKind);

    if (cacheEntry == ZR_NULL) {
        return ZR_NULL;
    }

    return execution_resolve_function_member_symbol(function, cacheEntry->memberEntryIndex);
}

static ZR_FORCE_INLINE SZrFunctionCallSiteCacheEntry *execution_member_get_cache_entry_dispatch_fast(
        SZrFunction *function,
        TZrUInt16 cacheIndex,
        EZrFunctionCallSiteCacheKind expectedKind) {
    SZrFunctionCallSiteCacheEntry *entry;

    if (ZR_UNLIKELY(
                function == ZR_NULL || function->callSiteCaches == ZR_NULL ||
                cacheIndex >= function->callSiteCacheLength)) {
        ZrCore_Profile_RecordSlowPathCurrent(ZR_PROFILE_SLOWPATH_CALLSITE_CACHE_MISS);
        return ZR_NULL;
    }

    entry = &function->callSiteCaches[cacheIndex];
    if (ZR_UNLIKELY((EZrFunctionCallSiteCacheKind)entry->kind != expectedKind)) {
        ZrCore_Profile_RecordSlowPathCurrent(ZR_PROFILE_SLOWPATH_CALLSITE_CACHE_MISS);
        return ZR_NULL;
    }

    if (ZR_UNLIKELY(execution_callsite_sanitize_enabled())) {
        garbage_collector_sanitize_callsite_cache_pic(function, cacheIndex, "runtime-callsite-lookup", entry);
    }

    return entry;
}

static TZrBool execution_raise_vm_runtime_error(SZrState *state,
                                                SZrCallInfo **ioCallInfo,
                                                const TZrChar *format,
                                                ...) {
    TZrChar errorBuffer[ZR_RUNTIME_ERROR_BUFFER_LENGTH];
    SZrString *errorString;
    SZrTypeValue payload;
    va_list args;
    SZrCallInfo *callInfo;

    if (state == ZR_NULL || ioCallInfo == ZR_NULL || *ioCallInfo == ZR_NULL || format == ZR_NULL) {
        return ZR_FALSE;
    }

    va_start(args, format);
    vsnprintf(errorBuffer, sizeof(errorBuffer), format, args);
    va_end(args);
    errorBuffer[sizeof(errorBuffer) - 1] = '\0';

    errorString = ZrCore_String_CreateFromNative(state, errorBuffer[0] != '\0' ? errorBuffer : "Runtime error");
    if (errorString == ZR_NULL) {
        if (!ZrCore_Exception_NormalizeStatus(state, ZR_THREAD_STATUS_EXCEPTION_ERROR)) {
            ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_EXCEPTION_ERROR);
        }
        ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_EXCEPTION_ERROR);
        ZR_ABORT();
    }

    callInfo = *ioCallInfo;
    execution_clear_pending_control(state);
    ZrCore_Value_InitAsRawObject(state, &payload, ZR_CAST_RAW_OBJECT_AS_SUPER(errorString));
    payload.type = ZR_VALUE_TYPE_STRING;
    payload.isGarbageCollectable = ZR_TRUE;
    payload.isNative = ZR_FALSE;

    if (!ZrCore_Exception_NormalizeThrownValue(state, &payload, callInfo, ZR_THREAD_STATUS_RUNTIME_ERROR)) {
        if (!ZrCore_Exception_NormalizeStatus(state, ZR_THREAD_STATUS_EXCEPTION_ERROR)) {
            ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_EXCEPTION_ERROR);
        }
        ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_EXCEPTION_ERROR);
        ZR_ABORT();
    }

    if (execution_unwind_exception_to_handler(state, ioCallInfo)) {
        return ZR_TRUE;
    }

    ZrCore_Exception_Throw(state, state->currentExceptionStatus);
    ZR_ABORT();
}

static ZR_FORCE_INLINE TZrBool execution_is_truthy(SZrState *state, const SZrTypeValue *value) {
    if (state == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_BOOL(value->type)) {
        return value->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE;
    }
    if (ZR_VALUE_IS_TYPE_INT(value->type)) {
        return value->value.nativeObject.nativeInt64 != 0;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        return value->value.nativeObject.nativeUInt64 != 0;
    }
    if (ZR_VALUE_IS_TYPE_FLOAT(value->type)) {
        return value->value.nativeObject.nativeDouble != 0.0;
    }
    if (ZR_VALUE_IS_TYPE_NULL(value->type)) {
        return ZR_FALSE;
    }
    if (ZR_VALUE_IS_TYPE_STRING(value->type)) {
        SZrString *str = ZR_CAST_STRING(state, value->value.object);
        TZrSize len = (str->shortStringLength < ZR_VM_LONG_STRING_FLAG) ? str->shortStringLength : str->longStringLength;
        return len > 0;
    }

    return ZR_TRUE;
}

static TZrBool execution_try_reuse_preinstalled_top_level_closure(SZrState *state,
                                                                  SZrClosure *ownerClosure,
                                                                  SZrFunction *function,
                                                                  TZrStackValuePointer base,
                                                                  SZrTypeValue *destination) {
    TZrUInt32 index;

    if (state == ZR_NULL || ownerClosure == ZR_NULL || ownerClosure->function == ZR_NULL || function == ZR_NULL ||
        base == ZR_NULL || destination == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < ownerClosure->function->exportedVariableLength; ++index) {
        const SZrFunctionExportedVariable *binding = &ownerClosure->function->exportedVariables[index];
        SZrTypeValue *existingValue;
        SZrFunction *existingFunction;

        if (binding->exportKind != ZR_MODULE_EXPORT_KIND_FUNCTION ||
            binding->readiness != ZR_MODULE_EXPORT_READY_DECLARATION ||
            binding->callableChildIndex >= ownerClosure->function->childFunctionLength ||
            &ownerClosure->function->childFunctionList[binding->callableChildIndex] != function ||
            binding->stackSlot >= ownerClosure->function->stackSize) {
            continue;
        }

        existingValue = ZrCore_Stack_GetValue(base + binding->stackSlot);
        if (existingValue == ZR_NULL) {
            continue;
        }

        existingFunction = execution_try_resolve_vm_metadata_function_fast(state, existingValue, ZR_NULL);
        if (existingFunction != function) {
            continue;
        }

        ZrCore_Value_Copy(state, destination, existingValue);
        return ZR_TRUE;
    }

    for (index = 0; index < ownerClosure->function->topLevelCallableBindingLength; ++index) {
        const SZrFunctionTopLevelCallableBinding *binding = &ownerClosure->function->topLevelCallableBindings[index];
        SZrTypeValue *existingValue;
        SZrFunction *existingFunction;

        if (binding->callableChildIndex >= ownerClosure->function->childFunctionLength ||
            &ownerClosure->function->childFunctionList[binding->callableChildIndex] != function ||
            binding->stackSlot >= ownerClosure->function->stackSize) {
            continue;
        }

        existingValue = ZrCore_Stack_GetValue(base + binding->stackSlot);
        if (existingValue == ZR_NULL) {
            continue;
        }

        existingFunction = execution_try_resolve_vm_metadata_function_fast(state, existingValue, ZR_NULL);
        if (existingFunction != function) {
            continue;
        }

        ZrCore_Value_Copy(state, destination, existingValue);
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool execution_profile_signed_arithmetic_reads_slot(const TZrInstruction *instruction, TZrUInt16 slot) {
    EZrInstructionCode opcode;

    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST_PLAIN_DEST):
            return instruction->instruction.operand.operand1[0] == slot;
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK):
            return ZR_FALSE;
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
            return instruction->instruction.operand.operand1[0] == slot ||
                   instruction->instruction.operand.operand1[1] == slot;
        default:
            return ZR_FALSE;
    }
}

static void execution_profile_record_load_typed_arithmetic_probe(
        SZrProfileRuntime *profileRuntime,
        const SZrFunction *frameFunction,
        const SZrFunction *previousFrameFunction,
        const TZrInstruction *programCounter,
        const TZrInstruction *previousProgramCounter,
        const TZrInstruction *instruction,
        const TZrInstruction *previousInstruction) {
    EZrInstructionCode previousOpcode;
    TZrUInt16 loadedSlot;

    if (profileRuntime == ZR_NULL || previousInstruction == ZR_NULL || previousProgramCounter == ZR_NULL ||
        frameFunction == ZR_NULL || previousFrameFunction != frameFunction || previousProgramCounter + 1 != programCounter) {
        return;
    }

    previousOpcode = (EZrInstructionCode)previousInstruction->instruction.operationCode;
    if (previousOpcode != ZR_INSTRUCTION_ENUM(GET_STACK) && previousOpcode != ZR_INSTRUCTION_ENUM(GET_CONSTANT)) {
        return;
    }

    loadedSlot = previousInstruction->instruction.operandExtra;
    if (!execution_profile_signed_arithmetic_reads_slot(instruction, loadedSlot)) {
        return;
    }

    if (previousOpcode == ZR_INSTRUCTION_ENUM(GET_STACK)) {
        profileRuntime->quickeningProbeCounts[ZR_PROFILE_QUICKENING_PROBE_GET_STACK_TYPED_ARITHMETIC]++;
    } else {
        profileRuntime->quickeningProbeCounts[ZR_PROFILE_QUICKENING_PROBE_GET_CONSTANT_TYPED_ARITHMETIC]++;
    }
}

void ZrCore_Execute(SZrState *state, SZrCallInfo *callInfo) {
    SZrClosure *closure = ZR_NULL;
    TZrStackValuePointer frameFunctionBase = ZR_NULL;
    SZrRawObject *frameCallableObject = ZR_NULL;
    SZrFunction *frameFunction = ZR_NULL;
    SZrTypeValue *constants = ZR_NULL;
    TZrStackValuePointer base = ZR_NULL;
    SZrTypeValue ret;
    ZrCore_Value_ResetAsNull(&ret);
    const TZrInstruction *programCounter = ZR_NULL;
    const TZrInstruction *instructionsEnd = ZR_NULL;
    const TZrInstruction *instructionsEndFast1 = ZR_NULL;
    TZrDebugSignal trap;
    SZrProfileRuntime *profileRuntime;
    TZrBool recordInstructions;
    TZrBool recordHelpers;
    TZrBool fastDispatchMode;
    const TZrInstruction *profilePreviousProgramCounter = ZR_NULL;
    TZrInstruction profilePreviousInstruction;
    SZrFunction *profilePreviousFrameFunction = ZR_NULL;
    SZrTypeValue *opA;
    SZrTypeValue *opB;
    /*
     * registers macros
     */

    /*
     *
     */
    ZR_INSTRUCTION_DISPATCH_TABLE
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
    static void *const fastDispatchTable[ZR_INSTRUCTION_ENUM(ENUM_MAX)] = {
            [0 ... ZR_INSTRUCTION_ENUM(ENUM_MAX) - 1] = &&LZrFastInstruction_FALLBACK,
            [ZR_INSTRUCTION_ENUM(GET_STACK)] = &&LZrFastInstruction_GET_STACK,
            [ZR_INSTRUCTION_ENUM(SET_STACK)] = &&LZrFastInstruction_SET_STACK,
            [ZR_INSTRUCTION_ENUM(GET_CONSTANT)] = &&LZrFastInstruction_GET_CONSTANT,
            [ZR_INSTRUCTION_ENUM(RESET_STACK_NULL)] = &&LZrFastInstruction_RESET_STACK_NULL,
            [ZR_INSTRUCTION_ENUM(ADD_INT)] = &&LZrFastInstruction_ADD_INT,
            [ZR_INSTRUCTION_ENUM(ADD_INT_PLAIN_DEST)] = &&LZrFastInstruction_ADD_INT_PLAIN_DEST,
            [ZR_INSTRUCTION_ENUM(ADD_INT_CONST)] = &&LZrFastInstruction_ADD_INT_CONST,
            [ZR_INSTRUCTION_ENUM(ADD_INT_CONST_PLAIN_DEST)] = &&LZrFastInstruction_ADD_INT_CONST_PLAIN_DEST,
            [ZR_INSTRUCTION_ENUM(ADD_SIGNED)] = &&LZrFastInstruction_ADD_SIGNED,
            [ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST)] = &&LZrFastInstruction_ADD_SIGNED_PLAIN_DEST,
            [ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST)] = &&LZrFastInstruction_ADD_SIGNED_CONST,
            [ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST)] = &&LZrFastInstruction_ADD_SIGNED_CONST_PLAIN_DEST,
            [ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_CONST)] = &&LZrFastInstruction_ADD_SIGNED_LOAD_CONST,
            [ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_CONST)] = &&LZrFastInstruction_ADD_SIGNED_LOAD_STACK_CONST,
            [ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK)] = &&LZrFastInstruction_ADD_SIGNED_LOAD_STACK,
            [ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_LOAD_CONST)] =
                    &&LZrFastInstruction_ADD_SIGNED_LOAD_STACK_LOAD_CONST,
            [ZR_INSTRUCTION_ENUM(ADD_UNSIGNED)] = &&LZrFastInstruction_ADD_UNSIGNED,
            [ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST)] = &&LZrFastInstruction_ADD_UNSIGNED_PLAIN_DEST,
            [ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST)] = &&LZrFastInstruction_ADD_UNSIGNED_CONST,
            [ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST_PLAIN_DEST)] = &&LZrFastInstruction_ADD_UNSIGNED_CONST_PLAIN_DEST,
            [ZR_INSTRUCTION_ENUM(SUB_INT)] = &&LZrFastInstruction_SUB_INT,
            [ZR_INSTRUCTION_ENUM(SUB_INT_PLAIN_DEST)] = &&LZrFastInstruction_SUB_INT_PLAIN_DEST,
            [ZR_INSTRUCTION_ENUM(SUB_INT_CONST)] = &&LZrFastInstruction_SUB_INT_CONST,
            [ZR_INSTRUCTION_ENUM(SUB_INT_CONST_PLAIN_DEST)] = &&LZrFastInstruction_SUB_INT_CONST_PLAIN_DEST,
            [ZR_INSTRUCTION_ENUM(SUB_SIGNED)] = &&LZrFastInstruction_SUB_SIGNED,
            [ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST)] = &&LZrFastInstruction_SUB_SIGNED_PLAIN_DEST,
            [ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST)] = &&LZrFastInstruction_SUB_SIGNED_CONST,
            [ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST)] = &&LZrFastInstruction_SUB_SIGNED_CONST_PLAIN_DEST,
            [ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_CONST)] = &&LZrFastInstruction_SUB_SIGNED_LOAD_CONST,
            [ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_STACK_CONST)] = &&LZrFastInstruction_SUB_SIGNED_LOAD_STACK_CONST,
            [ZR_INSTRUCTION_ENUM(SUB_UNSIGNED)] = &&LZrFastInstruction_SUB_UNSIGNED,
            [ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_PLAIN_DEST)] = &&LZrFastInstruction_SUB_UNSIGNED_PLAIN_DEST,
            [ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST)] = &&LZrFastInstruction_SUB_UNSIGNED_CONST,
            [ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST_PLAIN_DEST)] = &&LZrFastInstruction_SUB_UNSIGNED_CONST_PLAIN_DEST,
            [ZR_INSTRUCTION_ENUM(MUL_SIGNED)] = &&LZrFastInstruction_MUL_SIGNED,
            [ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST)] = &&LZrFastInstruction_MUL_SIGNED_PLAIN_DEST,
            [ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST)] = &&LZrFastInstruction_MUL_SIGNED_CONST,
            [ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST)] = &&LZrFastInstruction_MUL_SIGNED_CONST_PLAIN_DEST,
            [ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_CONST)] = &&LZrFastInstruction_MUL_SIGNED_LOAD_CONST,
            [ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK_CONST)] = &&LZrFastInstruction_MUL_SIGNED_LOAD_STACK_CONST,
            [ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_PLAIN_DEST)] = &&LZrFastInstruction_MUL_UNSIGNED_PLAIN_DEST,
            [ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST)] = &&LZrFastInstruction_MUL_UNSIGNED_CONST,
            [ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST_PLAIN_DEST)] = &&LZrFastInstruction_MUL_UNSIGNED_CONST_PLAIN_DEST,
            [ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST)] = &&LZrFastInstruction_DIV_SIGNED_CONST,
            [ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST_PLAIN_DEST)] = &&LZrFastInstruction_DIV_SIGNED_CONST_PLAIN_DEST,
            [ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_CONST)] = &&LZrFastInstruction_DIV_SIGNED_LOAD_CONST,
            [ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_STACK_CONST)] = &&LZrFastInstruction_DIV_SIGNED_LOAD_STACK_CONST,
            [ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST)] = &&LZrFastInstruction_DIV_UNSIGNED_CONST,
            [ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST_PLAIN_DEST)] =
                    &&LZrFastInstruction_DIV_UNSIGNED_CONST_PLAIN_DEST,
            [ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST)] = &&LZrFastInstruction_MOD_SIGNED_CONST,
            [ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST_PLAIN_DEST)] = &&LZrFastInstruction_MOD_SIGNED_CONST_PLAIN_DEST,
            [ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_CONST)] = &&LZrFastInstruction_MOD_SIGNED_LOAD_CONST,
            [ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_STACK_CONST)] = &&LZrFastInstruction_MOD_SIGNED_LOAD_STACK_CONST,
            [ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST)] = &&LZrFastInstruction_MOD_UNSIGNED_CONST,
            [ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST_PLAIN_DEST)] =
                    &&LZrFastInstruction_MOD_UNSIGNED_CONST_PLAIN_DEST,
            [ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED)] = &&LZrFastInstruction_LOGICAL_LESS_EQUAL_SIGNED,
            [ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_BOOL)] = &&LZrFastInstruction_LOGICAL_EQUAL_BOOL,
            [ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_BOOL)] = &&LZrFastInstruction_LOGICAL_NOT_EQUAL_BOOL,
            [ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED)] = &&LZrFastInstruction_LOGICAL_EQUAL_SIGNED,
            [ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED_CONST)] = &&LZrFastInstruction_LOGICAL_EQUAL_SIGNED_CONST,
            [ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_SIGNED)] = &&LZrFastInstruction_LOGICAL_NOT_EQUAL_SIGNED,
            [ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_UNSIGNED)] = &&LZrFastInstruction_LOGICAL_EQUAL_UNSIGNED,
            [ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_UNSIGNED)] = &&LZrFastInstruction_LOGICAL_NOT_EQUAL_UNSIGNED,
            [ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_FLOAT)] = &&LZrFastInstruction_LOGICAL_EQUAL_FLOAT,
            [ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_FLOAT)] = &&LZrFastInstruction_LOGICAL_NOT_EQUAL_FLOAT,
            [ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_STRING)] = &&LZrFastInstruction_LOGICAL_EQUAL_STRING,
            [ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_STRING)] = &&LZrFastInstruction_LOGICAL_NOT_EQUAL_STRING,
            [ZR_INSTRUCTION_ENUM(SUPER_ARRAY_BIND_ITEMS)] = &&LZrFastInstruction_SUPER_ARRAY_BIND_ITEMS,
            [ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT)] = &&LZrFastInstruction_SUPER_ARRAY_GET_INT,
            [ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS)] = &&LZrFastInstruction_SUPER_ARRAY_GET_INT_ITEMS,
            [ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_PLAIN_DEST)] =
                    &&LZrFastInstruction_SUPER_ARRAY_GET_INT_PLAIN_DEST,
            [ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS_PLAIN_DEST)] =
                    &&LZrFastInstruction_SUPER_ARRAY_GET_INT_ITEMS_PLAIN_DEST,
            [ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT)] = &&LZrFastInstruction_SUPER_ARRAY_SET_INT,
            [ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT_ITEMS)] = &&LZrFastInstruction_SUPER_ARRAY_SET_INT_ITEMS,
            [ZR_INSTRUCTION_ENUM(SUPER_ARRAY_FILL_INT4_CONST)] = &&LZrFastInstruction_SUPER_ARRAY_FILL_INT4_CONST,
            [ZR_INSTRUCTION_ENUM(SUPER_ITER_MOVE_NEXT_JUMP_IF_FALSE)] =
                    &&LZrFastInstruction_SUPER_ITER_MOVE_NEXT_JUMP_IF_FALSE,
            [ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE)] =
                    &&LZrFastInstruction_SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE,
            [ZR_INSTRUCTION_ENUM(JUMP)] = &&LZrFastInstruction_JUMP,
            [ZR_INSTRUCTION_ENUM(JUMP_IF)] = &&LZrFastInstruction_JUMP_IF,
            [ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED)] = &&LZrFastInstruction_JUMP_IF_GREATER_SIGNED,
            [ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED)] = &&LZrFastInstruction_JUMP_IF_NOT_EQUAL_SIGNED,
            [ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED_CONST)] =
                    &&LZrFastInstruction_JUMP_IF_NOT_EQUAL_SIGNED_CONST,
            [ZR_INSTRUCTION_ENUM(FUNCTION_CALL)] = &&LZrFastInstruction_FALLBACK_NO_DESTINATION,
            [ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL)] = &&LZrFastInstruction_FALLBACK_NO_DESTINATION,
            [ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL)] = &&LZrFastInstruction_FALLBACK_NO_DESTINATION,
            [ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL_LOAD1_U8)] = &&LZrFastInstruction_FALLBACK_NO_DESTINATION,
            [ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL)] = &&LZrFastInstruction_FALLBACK_NO_DESTINATION,
            [ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_MEMBER_CALL)] = &&LZrFastInstruction_FALLBACK_NO_DESTINATION,
            [ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS)] = &&LZrFastInstruction_FALLBACK_NO_DESTINATION,
            [ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS)] = &&LZrFastInstruction_FALLBACK_NO_DESTINATION,
            [ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_CALL_NO_ARGS)] = &&LZrFastInstruction_FALLBACK_NO_DESTINATION,
            [ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT)] = &&LZrFastInstruction_GET_MEMBER_SLOT,
            [ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT)] = &&LZrFastInstruction_SET_MEMBER_SLOT,
            [ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL)] = &&LZrFastInstruction_FALLBACK_NO_DESTINATION,
            [ZR_INSTRUCTION_ENUM(KNOWN_VM_TAIL_CALL)] = &&LZrFastInstruction_FALLBACK_NO_DESTINATION,
            [ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_TAIL_CALL)] = &&LZrFastInstruction_FALLBACK_NO_DESTINATION,
            [ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS)] = &&LZrFastInstruction_FALLBACK_NO_DESTINATION,
            [ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_TAIL_CALL_NO_ARGS)] = &&LZrFastInstruction_FALLBACK_NO_DESTINATION,
            [ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_TAIL_CALL_NO_ARGS)] =
                    &&LZrFastInstruction_FALLBACK_NO_DESTINATION,
            [ZR_INSTRUCTION_ENUM(FUNCTION_RETURN)] = &&LZrFastInstruction_FALLBACK_NO_DESTINATION,
    };
#define ZR_FAST_INSTRUCTION_DISPATCH(INSTRUCTION) goto *fastDispatchTable[ZR_INSTRUCTION_OPCODE(INSTRUCTION)];
#endif
#define FETCH_DEBUG_BASE_SYNC()                                                                                         \
    do {                                                                                                               \
        if (ZR_UNLIKELY(trap != ZR_DEBUG_SIGNAL_NONE) &&                                                                \
            ZR_UNLIKELY(base != callInfo->functionBase.valuePointer + 1)) {                                            \
            base = callInfo->functionBase.valuePointer + 1;                                                            \
        }                                                                                                              \
    } while (0)
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
#define UPDATE_FAST_DISPATCH_MODE()                                                                                     \
    do {                                                                                                               \
        fastDispatchMode = (trap == ZR_DEBUG_SIGNAL_NONE && !recordInstructions) ? ZR_TRUE : ZR_FALSE;                \
    } while (0)
#else
/*
 * MSVC falls back to the switch-based dispatcher. In that mode we must force
 * the shared fetch path so `destination` is always initialized before any
 * opcode body that reads it.
 */
#define UPDATE_FAST_DISPATCH_MODE()                                                                                     \
    do {                                                                                                               \
        fastDispatchMode = ZR_FALSE;                                                                                   \
    } while (0)
#endif
#define FAST_PREPARE_DESTINATION_FROM_OFFSET(DESTINATION_OFFSET)                                                        \
    do {                                                                                                               \
        TZrUInt16 fastDestinationOffset__ = (DESTINATION_OFFSET);                                                      \
        if (ZR_UNLIKELY(fastDestinationOffset__ == ZR_INSTRUCTION_USE_RET_FLAG)) {                                     \
            destination = &ret;                                                                                         \
        } else {                                                                                                       \
            destination = &BASE(fastDestinationOffset__)->value;                                                       \
        }                                                                                                              \
    } while (0)
#define FAST_PREPARE_DESTINATION() FAST_PREPARE_DESTINATION_FROM_OFFSET(E(instruction))
#define FAST_PREPARE_STACK_DESTINATION_FROM_OFFSET(DESTINATION_OFFSET)                                                  \
    do {                                                                                                               \
        TZrUInt16 fastDestinationOffset__ = (DESTINATION_OFFSET);                                                      \
        ZR_ASSERT(fastDestinationOffset__ != ZR_INSTRUCTION_USE_RET_FLAG);                                             \
        destination = &BASE(fastDestinationOffset__)->value;                                                           \
    } while (0)
#define FAST_PREPARE_STACK_DESTINATION() FAST_PREPARE_STACK_DESTINATION_FROM_OFFSET(E(instruction))
#define FETCH_PREPARE_FAST_ONLY(N)                                                                                      \
    do {                                                                                                               \
        if ((N) == 1) {                                                                                                \
            instruction = *++programCounter;                                                                           \
        } else {                                                                                                       \
            instruction = *(programCounter += (N));                                                                    \
        }                                                                                                              \
        ZR_ASSERT(programCounter < instructionsEnd);                                                                   \
        ZR_ASSERT(base == callInfo->functionBase.valuePointer + 1);                                                    \
        ZR_ASSERT(base <= state->stackTop.valuePointer &&                                                              \
                  state->stackTop.valuePointer <= state->stackTail.valuePointer);                                      \
    } while (0)
#define FETCH_PREPARE_SHARED(N)                                                                                        \
    do {                                                                                                               \
        if (ZR_LIKELY(fastDispatchMode)) {                                                                             \
            if ((N) == 1) {                                                                                            \
                instruction = *++programCounter;                                                                       \
            } else {                                                                                                   \
                instruction = *(programCounter += (N));                                                                \
            }                                                                                                          \
        } else {                                                                                                       \
            ZR_INSTRUCTION_FETCH(instruction,                                                                          \
                                 programCounter,                                                                       \
                                 trap = ZrCore_Debug_TraceExecution(state, programCounter); UPDATE_STACK(callInfo),    \
                                 N);                                                                                   \
            if (ZR_UNLIKELY(recordInstructions)) {                                                                     \
                profileRuntime->instructionCounts[(EZrInstructionCode)ZR_INSTRUCTION_OPCODE(instruction)]++;          \
                execution_profile_record_load_typed_arithmetic_probe(profileRuntime,                                  \
                                                                     frameFunction,                                    \
                                                                     profilePreviousFrameFunction,                    \
                                                                     programCounter,                                  \
                                                                     profilePreviousProgramCounter,                   \
                                                                     &instruction,                                    \
                                                                     &profilePreviousInstruction);                    \
                profilePreviousProgramCounter = programCounter;                                                        \
                profilePreviousInstruction = instruction;                                                              \
                profilePreviousFrameFunction = frameFunction;                                                          \
            }                                                                                                          \
            FETCH_DEBUG_BASE_SYNC();                                                                                   \
        }                                                                                                              \
        ZR_ASSERT(programCounter < instructionsEnd);                                                                   \
        ZR_ASSERT(base == callInfo->functionBase.valuePointer + 1);                                                    \
        ZR_ASSERT(base <= state->stackTop.valuePointer &&                                                              \
                  state->stackTop.valuePointer <= state->stackTail.valuePointer);                                      \
        if (ZR_UNLIKELY(!fastDispatchMode)) {                                                                          \
            FAST_PREPARE_DESTINATION();                                                                                \
        }                                                                                                              \
    } while (0)
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
#define FETCH_PREPARE_OR_BREAK(N)                                                                                       \
    if (ZR_UNLIKELY((N) == 1 ? programCounter >= instructionsEndFast1 : programCounter + (N) >= instructionsEnd))      \
        break;                                                                                                          \
    FETCH_PREPARE_SHARED(N)
#define FETCH_PREPARE_OR_GOTO_DONE(N)                                                                                  \
    do {                                                                                                               \
        if (ZR_UNLIKELY((N) == 1 ? programCounter >= instructionsEndFast1 : programCounter + (N) >= instructionsEnd)) { \
            goto LZrExecutionDone;                                                                                     \
        }                                                                                                              \
        FETCH_PREPARE_SHARED(N);                                                                                       \
    } while (0)
#define DONE_FAST(N)                                                                                                   \
    do {                                                                                                               \
        if (ZR_UNLIKELY((N) == 1 ? programCounter >= instructionsEndFast1 : programCounter + (N) >= instructionsEnd)) { \
            goto LZrExecutionDone;                                                                                     \
        }                                                                                                              \
        FETCH_PREPARE_FAST_ONLY(N);                                                                                    \
        ZR_FAST_INSTRUCTION_DISPATCH(instruction)                                                                      \
    } while (0)
#define DONE_AFTER_TRAP_FAST_ONE()                                                                                     \
    do {                                                                                                               \
        if (ZR_LIKELY(trap == ZR_DEBUG_SIGNAL_NONE)) {                                                                 \
            if (ZR_UNLIKELY(programCounter >= instructionsEndFast1)) {                                                 \
                goto LZrExecutionDone;                                                                                 \
            }                                                                                                          \
            FETCH_PREPARE_FAST_ONLY(1);                                                                                \
            ZR_FAST_INSTRUCTION_DISPATCH(instruction)                                                                  \
        }                                                                                                              \
        FETCH_PREPARE_OR_GOTO_DONE(1);                                                                                 \
        ZR_INSTRUCTION_DISPATCH(instruction)                                                                           \
    } while (0)
#define DONE(N)                                                                                                        \
    do {                                                                                                               \
        if (ZR_LIKELY(fastDispatchMode)) {                                                                             \
            if (ZR_UNLIKELY((N) == 1 ? programCounter >= instructionsEndFast1 : programCounter + (N) >= instructionsEnd)) { \
                goto LZrExecutionDone;                                                                                 \
            }                                                                                                          \
            FETCH_PREPARE_FAST_ONLY(N);                                                                                \
            ZR_FAST_INSTRUCTION_DISPATCH(instruction)                                                                  \
        }                                                                                                              \
        FETCH_PREPARE_OR_GOTO_DONE(N);                                                                                 \
        ZR_INSTRUCTION_DISPATCH(instruction)                                                                           \
    } while (0)
#else
#define FETCH_PREPARE_OR_BREAK(N)                                                                                       \
    if (ZR_UNLIKELY(programCounter + (N) >= instructionsEnd))                                                           \
        break;                                                                                                          \
    FETCH_PREPARE_SHARED(N)
#define DONE(N) break
#define DONE_FAST(N) DONE(N)
#define DONE_AFTER_TRAP_FAST_ONE() DONE(1)
#endif
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
#define DONE_SKIP(N) DONE(N)
#else
#define DONE_SKIP(N) do { programCounter += ((N) - 1); } while (0); break
#endif
// extra operand
#define E(INSTRUCTION) INSTRUCTION.instruction.operandExtra
// 4 OPERANDS
#define A0(INSTRUCTION) INSTRUCTION.instruction.operand.operand0[0]
#define B0(INSTRUCTION) INSTRUCTION.instruction.operand.operand0[1]
#define C0(INSTRUCTION) INSTRUCTION.instruction.operand.operand0[2]
#define D0(INSTRUCTION) INSTRUCTION.instruction.operand.operand0[3]
// 2 OPERANDS
#define A1(INSTRUCTION) INSTRUCTION.instruction.operand.operand1[0]
#define B1(INSTRUCTION) INSTRUCTION.instruction.operand.operand1[1]
// 1 OPERAND
#define A2(INSTRUCTION) INSTRUCTION.instruction.operand.operand2[0]

#define BASE(OFFSET) (base + (OFFSET))
#define CONST(OFFSET) (constants + (OFFSET))
#define CLOSURE(OFFSET) (closure->closureValuesExtend[OFFSET])
#define ZrCore_Value_Copy(STATE, DESTINATION, SOURCE)                                                                  \
    execution_copy_value_fast((STATE), (DESTINATION), (SOURCE), profileRuntime, recordHelpers)
#define ZrCore_Stack_GetValue(VALUE_ON_STACK) execution_stack_get_value_fast((VALUE_ON_STACK), profileRuntime, recordHelpers)

#define EXECUTION_STORE_PLAIN_REUSE(REGION, DATA, TYPE)                                                                \
    do {                                                                                                               \
        TZrUInt64 rawValue__ = ZR_CAST(TZrUInt64, (DATA));                                                             \
        ZR_ASSERT(destination != ZR_NULL);                                                                             \
        ZR_ASSERT(state != ZR_NULL);                                                                                   \
        if (ZR_UNLIKELY(destination->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE)) {                                 \
            ZrCore_Ownership_ReleaseValue(state, destination);                                                         \
        } else {                                                                                                       \
            ZR_ASSERT(destination->ownershipControl == ZR_NULL && destination->ownershipWeakRef == ZR_NULL);           \
        }                                                                                                              \
        destination->type = (TYPE);                                                                                    \
        destination->value.nativeObject.nativeUInt64 = rawValue__;                                                     \
        destination->isGarbageCollectable = ZR_FALSE;                                                                  \
        destination->isNative = ZR_TRUE;                                                                               \
    } while (0)
#define EXECUTION_STORE_PLAIN_DIRECT_TO(DESTINATION_VALUE, REGION, DATA, TYPE)                                         \
    do {                                                                                                               \
        SZrTypeValue *executionPlainDestination__ = (DESTINATION_VALUE);                                               \
        ZR_ASSERT(executionPlainDestination__ != ZR_NULL);                                                             \
        ZR_ASSERT(executionPlainDestination__->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_NONE);                         \
        ZR_ASSERT(executionPlainDestination__->ownershipControl == ZR_NULL &&                                          \
                  executionPlainDestination__->ownershipWeakRef == ZR_NULL);                                           \
        ZR_ASSERT(!executionPlainDestination__->isGarbageCollectable);                                                 \
        ZR_ASSERT(executionPlainDestination__->isNative);                                                              \
        executionPlainDestination__->type = (TYPE);                                                                    \
        executionPlainDestination__->value.nativeObject.REGION = (DATA);                                               \
    } while (0)
#define EXECUTION_STORE_PLAIN_DIRECT(REGION, DATA, TYPE)                                                               \
    EXECUTION_STORE_PLAIN_DIRECT_TO(destination, REGION, DATA, TYPE)
#define ALGORITHM_1(REGION, OP, TYPE) EXECUTION_STORE_PLAIN_REUSE(REGION, OP(opA->value.nativeObject.REGION), TYPE);
#define ALGORITHM_2(REGION, OP, TYPE)                                                                                  \
    EXECUTION_STORE_PLAIN_REUSE(REGION, (opA->value.nativeObject.REGION) OP(opB->value.nativeObject.REGION), TYPE);
#define ALGORITHM_CVT_2(CVT, REGION, OP, TYPE)                                                                         \
    EXECUTION_STORE_PLAIN_REUSE(CVT, (opA->value.nativeObject.REGION) OP(opB->value.nativeObject.REGION), TYPE);
#define ALGORITHM_CONST_2(REGION, OP, TYPE, RIGHT)                                                                     \
    EXECUTION_STORE_PLAIN_REUSE(REGION, (opA->value.nativeObject.REGION) OP(RIGHT), TYPE);
#define ALGORITHM_FUNC_2(REGION, OP_FUNC, TYPE)                                                                        \
    EXECUTION_STORE_PLAIN_REUSE(REGION, OP_FUNC(opA->value.nativeObject.REGION, opB->value.nativeObject.REGION), TYPE);
#define ALGORITHM_1_DIRECT(REGION, OP, TYPE) EXECUTION_STORE_PLAIN_DIRECT(REGION, OP(opA->value.nativeObject.REGION), TYPE);
#define ALGORITHM_2_DIRECT(REGION, OP, TYPE)                                                                           \
    EXECUTION_STORE_PLAIN_DIRECT(REGION, (opA->value.nativeObject.REGION) OP(opB->value.nativeObject.REGION), TYPE);
#define ALGORITHM_CVT_2_DIRECT(CVT, REGION, OP, TYPE)                                                                  \
    EXECUTION_STORE_PLAIN_DIRECT(CVT, (opA->value.nativeObject.REGION) OP(opB->value.nativeObject.REGION), TYPE);
#define ALGORITHM_CONST_2_DIRECT(REGION, OP, TYPE, RIGHT)                                                              \
    EXECUTION_STORE_PLAIN_DIRECT(REGION, (opA->value.nativeObject.REGION) OP(RIGHT), TYPE);
#define ALGORITHM_FUNC_2_DIRECT(REGION, OP_FUNC, TYPE)                                                                 \
    EXECUTION_STORE_PLAIN_DIRECT(REGION, OP_FUNC(opA->value.nativeObject.REGION, opB->value.nativeObject.REGION), TYPE);
#define EXECUTE_GET_STACK_BODY()                                                                                       \
    do {                                                                                                               \
        SZrTypeValue *source = &BASE(A2(instruction))->value;                                                          \
        if ((destination) == &ret) {                                                                                   \
            *destination = *source;                                                                                    \
        } else {                                                                                                       \
            ZrCore_Value_Copy(state, destination, source);                                                             \
        }                                                                                                              \
    } while (0)
#define EXECUTE_SET_STACK_BODY()                                                                                       \
    do {                                                                                                               \
        SZrTypeValue *srcValue = &BASE(A2(instruction))->value;                                                        \
        execution_assign_stack_value_to_stack_fast_no_profile(                                                         \
                state, &BASE(E(instruction))->value, srcValue);                                                        \
    } while (0)
#define EXECUTE_GET_CONSTANT_BODY()                                                                                    \
    do {                                                                                                               \
        if ((destination) == &ret) {                                                                                   \
            *destination = *CONST(A2(instruction));                                                                    \
        } else {                                                                                                       \
            ZrCore_Value_Copy(state, destination, CONST(A2(instruction)));                                             \
        }                                                                                                              \
    } while (0)
#define EXECUTE_RESET_STACK_NULL_BODY()                                                                                \
    do {                                                                                                               \
        if (ZR_UNLIKELY(recordHelpers)) {                                                                              \
            profileRuntime->helperCounts[ZR_PROFILE_HELPER_VALUE_RESET_NULL]++;                                        \
        }                                                                                                              \
        execution_reset_stack_value_to_null_fast_no_profile(state, &BASE(E(instruction))->value);                     \
    } while (0)
#define EXECUTE_GET_MEMBER_SLOT_BODY()                                                                                 \
    do {                                                                                                               \
        SZrString *memberName__ = ZR_NULL;                                                                             \
        TZrNativeString memberNativeName__;                                                                            \
        opA = &BASE(A1(instruction))->value;                                                                           \
        if (opA->type != ZR_VALUE_TYPE_OBJECT && opA->type != ZR_VALUE_TYPE_ARRAY &&                                  \
            opA->type != ZR_VALUE_TYPE_STRING) {                                                                       \
            ZrCore_Debug_RunError(state, "GET_MEMBER: receiver must be an object, array, or string");                \
        } else if (!(((opA->type == ZR_VALUE_TYPE_OBJECT || opA->type == ZR_VALUE_TYPE_ARRAY) &&                      \
                      execution_member_try_dispatch_exact_receiver_pair_get_hot_fast_checked_object(                   \
                              state, currentFunction, B1(instruction), opA, destination)) ||                          \
                     execution_member_get_cached_stack_receiver(                                                        \
                             state, programCounter, currentFunction, B1(instruction), opA, destination))) {           \
            memberName__ = execution_resolve_cached_member_symbol(                                                     \
                    currentFunction, B1(instruction), ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET);                    \
            if (memberName__ == ZR_NULL) {                                                                             \
                ZrCore_Debug_RunError(state, "GET_MEMBER_SLOT: cached member lookup failed");                        \
            } else {                                                                                                   \
                memberNativeName__ = ZrCore_String_GetNativeString(memberName__);                                      \
                ZrCore_Debug_RunError(state,                                                                           \
                                      "GET_MEMBER: missing member '%s'",                                             \
                                      memberNativeName__ != ZR_NULL ? memberNativeName__ : "<unknown>");             \
            }                                                                                                          \
        }                                                                                                              \
    } while (0)
#define EXECUTE_SET_MEMBER_SLOT_BODY()                                                                                 \
    do {                                                                                                               \
        opA = &BASE(A1(instruction))->value;                                                                           \
        if (opA->type != ZR_VALUE_TYPE_OBJECT && opA->type != ZR_VALUE_TYPE_ARRAY) {                                  \
            ZrCore_Debug_RunError(state, "SET_MEMBER: receiver must be a writable object member");                   \
        } else if (!(execution_member_try_dispatch_exact_receiver_pair_set_hot_fast_checked_object(                   \
                             state, currentFunction, B1(instruction), opA, destination) ||                            \
                     execution_member_set_cached_stack_receiver(                                                       \
                             state, programCounter, currentFunction, B1(instruction), opA, destination))) {           \
            if (execution_resolve_cached_member_symbol(                                                                \
                        currentFunction, B1(instruction), ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET) == ZR_NULL) {   \
                ZrCore_Debug_RunError(state, "SET_MEMBER_SLOT: cached member store failed");                         \
            } else {                                                                                                   \
                ZrCore_Debug_RunError(state, "SET_MEMBER: receiver must be a writable object member");               \
            }                                                                                                          \
        }                                                                                                              \
    } while (0)
#define EXECUTE_GET_STACK_BODY_FAST()                                                                                  \
    do {                                                                                                               \
        TZrUInt16 destinationOffset__ = E(instruction);                                                                \
        const SZrTypeValue *source = &BASE(A2(instruction))->value;                                                    \
        if (ZR_UNLIKELY(recordHelpers)) {                                                                              \
            FAST_PREPARE_DESTINATION_FROM_OFFSET(destinationOffset__);                                                  \
            if ((destination) == &ret) {                                                                               \
                *destination = *source;                                                                                \
            } else {                                                                                                   \
                execution_copy_value_fast(state, destination, source, profileRuntime, ZR_TRUE);                        \
            }                                                                                                          \
        } else if (ZR_LIKELY(destinationOffset__ != ZR_INSTRUCTION_USE_RET_FLAG)) {                                   \
            execution_copy_stack_value_to_stack_fast_no_profile(                                                       \
                    state, &BASE(destinationOffset__)->value, source);                                                 \
        } else {                                                                                                       \
            execution_copy_value_to_ret_fast_no_profile(&ret, source);                                                 \
        }                                                                                                              \
    } while (0)
#define EXECUTE_SET_STACK_BODY_FAST()                                                                                  \
    do {                                                                                                               \
        SZrTypeValue *srcValue = &BASE(A2(instruction))->value;                                                        \
        SZrTypeValue *stackDestination = &BASE(E(instruction))->value;                                                 \
        if (ZR_UNLIKELY(recordHelpers)) {                                                                              \
            profileRuntime->helperCounts[ZR_PROFILE_HELPER_VALUE_COPY]++;                                              \
        }                                                                                                              \
        execution_assign_stack_value_to_stack_fast_no_profile(state, stackDestination, srcValue);                     \
    } while (0)
#define EXECUTE_GET_CONSTANT_BODY_FAST()                                                                               \
    do {                                                                                                               \
        TZrUInt16 destinationOffset__ = E(instruction);                                                                \
        const SZrTypeValue *source = CONST(A2(instruction));                                                           \
        if (ZR_UNLIKELY(recordHelpers)) {                                                                              \
            FAST_PREPARE_DESTINATION_FROM_OFFSET(destinationOffset__);                                                  \
            if ((destination) == &ret) {                                                                               \
                *destination = *source;                                                                                \
            } else {                                                                                                   \
                execution_copy_value_fast(state, destination, source, profileRuntime, ZR_TRUE);                        \
            }                                                                                                          \
        } else if (ZR_LIKELY(destinationOffset__ != ZR_INSTRUCTION_USE_RET_FLAG)) {                                   \
            execution_copy_stack_value_to_stack_fast_no_profile(                                                       \
                    state, &BASE(destinationOffset__)->value, source);                                                 \
        } else {                                                                                                       \
            execution_copy_value_to_ret_fast_no_profile(&ret, source);                                                 \
        }                                                                                                              \
    } while (0)
#define EXECUTE_MATERIALIZE_CONSTANT_SLOT(CONSTANT_INDEX, MATERIALIZED_SLOT)                                           \
    do {                                                                                                               \
        const SZrTypeValue *materializedSource__ = CONST(CONSTANT_INDEX);                                               \
        SZrTypeValue *materializedDestination__ = &BASE(MATERIALIZED_SLOT)->value;                                      \
        if (ZR_UNLIKELY(recordHelpers)) {                                                                              \
            execution_copy_value_fast(state,                                                                            \
                                      materializedDestination__,                                                        \
                                      materializedSource__,                                                             \
                                      profileRuntime,                                                                   \
                                      ZR_TRUE);                                                                         \
        } else {                                                                                                       \
            execution_copy_stack_value_to_stack_fast_no_profile(state,                                                  \
                                                                materializedDestination__,                             \
                                                                materializedSource__);                                  \
        }                                                                                                              \
    } while (0)
#define EXECUTE_MATERIALIZE_STACK_SLOT(SOURCE_SLOT, MATERIALIZED_SLOT)                                                 \
    do {                                                                                                               \
        const SZrTypeValue *materializedSource__ = &BASE(SOURCE_SLOT)->value;                                           \
        SZrTypeValue *materializedDestination__ = &BASE(MATERIALIZED_SLOT)->value;                                      \
        if (ZR_UNLIKELY(recordHelpers)) {                                                                              \
            execution_copy_value_fast(state,                                                                            \
                                      materializedDestination__,                                                        \
                                      materializedSource__,                                                             \
                                      profileRuntime,                                                                   \
                                      ZR_TRUE);                                                                         \
        } else {                                                                                                       \
            execution_copy_stack_value_to_stack_fast_no_profile(state,                                                  \
                                                                materializedDestination__,                             \
                                                                materializedSource__);                                  \
        }                                                                                                              \
    } while (0)
#define EXECUTE_ADD_INT_BODY()                                                                                         \
    do {                                                                                                               \
        opA = &BASE(A1(instruction))->value;                                                                           \
        opB = &BASE(B1(instruction))->value;                                                                           \
        if (ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type)) {                                     \
            if (ZR_VALUE_IS_TYPE_SIGNED_INT(opA->type)) {                                                              \
                ALGORITHM_2(nativeInt64, +, opA->type);                                                                \
            } else {                                                                                                   \
                ALGORITHM_2(nativeUInt64, +, opA->type);                                                               \
            }                                                                                                          \
        } else {                                                                                                       \
            execution_try_binary_numeric_float_fallback_or_raise(                                                      \
                    state, ZR_EXEC_NUMERIC_FALLBACK_ADD, destination, opA, opB, "ADD_INT");                           \
        }                                                                                                              \
    } while (0)
#define EXECUTE_ADD_INT_BODY_PLAIN_DEST()                                                                              \
    do {                                                                                                               \
        SZrTypeValue *plainDestination__ = &BASE(E(instruction))->value;                                               \
        const SZrTypeValue *leftValue__ = &BASE(A1(instruction))->value;                                               \
        const SZrTypeValue *rightValue__ = &BASE(B1(instruction))->value;                                              \
        if (ZR_VALUE_IS_TYPE_INT(leftValue__->type) && ZR_VALUE_IS_TYPE_INT(rightValue__->type)) {                    \
            if (ZR_VALUE_IS_TYPE_SIGNED_INT(leftValue__->type)) {                                                      \
                EXECUTION_STORE_PLAIN_DIRECT_TO(plainDestination__,                                                    \
                                                nativeInt64,                                                           \
                                                leftValue__->value.nativeObject.nativeInt64 +                         \
                                                        rightValue__->value.nativeObject.nativeInt64,                 \
                                                leftValue__->type);                                                   \
            } else {                                                                                                   \
                EXECUTION_STORE_PLAIN_DIRECT_TO(plainDestination__,                                                    \
                                                nativeUInt64,                                                          \
                                                leftValue__->value.nativeObject.nativeUInt64 +                        \
                                                        rightValue__->value.nativeObject.nativeUInt64,                \
                                                leftValue__->type);                                                   \
            }                                                                                                          \
        } else {                                                                                                       \
            destination = plainDestination__;                                                                          \
            execution_try_binary_numeric_float_fallback_or_raise(                                                      \
                    state, ZR_EXEC_NUMERIC_FALLBACK_ADD, destination, leftValue__, rightValue__, "ADD_INT");          \
        }                                                                                                              \
    } while (0)
#define EXECUTE_ADD_INT_CONST_BODY()                                                                                   \
    do {                                                                                                               \
        const SZrTypeValue *constOpB = CONST(B1(instruction));                                                         \
        opA = &BASE(A1(instruction))->value;                                                                           \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(constOpB->type));                                                               \
        if (ZR_VALUE_IS_TYPE_INT(opA->type)) {                                                                         \
            if (ZR_VALUE_IS_TYPE_SIGNED_INT(opA->type)) {                                                              \
                ALGORITHM_CONST_2(nativeInt64, +, opA->type, constOpB->value.nativeObject.nativeInt64);               \
            } else {                                                                                                   \
                ALGORITHM_CONST_2(nativeUInt64, +, opA->type, constOpB->value.nativeObject.nativeUInt64);             \
            }                                                                                                          \
        } else {                                                                                                       \
            execution_try_binary_numeric_float_fallback_or_raise(                                                      \
                    state, ZR_EXEC_NUMERIC_FALLBACK_ADD, destination, opA, constOpB, "ADD_INT_CONST");               \
        }                                                                                                              \
    } while (0)
#define EXECUTE_ADD_INT_CONST_BODY_PLAIN_DEST()                                                                        \
    do {                                                                                                               \
        SZrTypeValue *plainDestination__ = &BASE(E(instruction))->value;                                               \
        const SZrTypeValue *constOpB = CONST(B1(instruction));                                                         \
        const SZrTypeValue *leftValue__ = &BASE(A1(instruction))->value;                                               \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(constOpB->type));                                                               \
        if (ZR_VALUE_IS_TYPE_INT(leftValue__->type)) {                                                                 \
            if (ZR_VALUE_IS_TYPE_SIGNED_INT(leftValue__->type)) {                                                      \
                EXECUTION_STORE_PLAIN_DIRECT_TO(plainDestination__,                                                    \
                                                nativeInt64,                                                           \
                                                leftValue__->value.nativeObject.nativeInt64 +                         \
                                                        constOpB->value.nativeObject.nativeInt64,                     \
                                                leftValue__->type);                                                   \
            } else {                                                                                                   \
                EXECUTION_STORE_PLAIN_DIRECT_TO(plainDestination__,                                                    \
                                                nativeUInt64,                                                          \
                                                leftValue__->value.nativeObject.nativeUInt64 +                        \
                                                        constOpB->value.nativeObject.nativeUInt64,                    \
                                                leftValue__->type);                                                   \
            }                                                                                                          \
        } else {                                                                                                       \
            destination = plainDestination__;                                                                          \
            execution_try_binary_numeric_float_fallback_or_raise(                                                      \
                    state, ZR_EXEC_NUMERIC_FALLBACK_ADD, destination, leftValue__, constOpB, "ADD_INT_CONST");       \
        }                                                                                                              \
    } while (0)
#define EXECUTE_SUB_INT_BODY()                                                                                         \
    do {                                                                                                               \
        opA = &BASE(A1(instruction))->value;                                                                           \
        opB = &BASE(B1(instruction))->value;                                                                           \
        if (ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type)) {                                     \
            ALGORITHM_2(nativeInt64, -, opA->type);                                                                    \
        } else {                                                                                                       \
            execution_try_binary_numeric_float_fallback_or_raise(                                                      \
                    state, ZR_EXEC_NUMERIC_FALLBACK_SUB, destination, opA, opB, "SUB_INT");                           \
        }                                                                                                              \
    } while (0)
#define EXECUTE_SUB_INT_BODY_PLAIN_DEST()                                                                              \
    do {                                                                                                               \
        SZrTypeValue *plainDestination__ = &BASE(E(instruction))->value;                                               \
        const SZrTypeValue *leftValue__ = &BASE(A1(instruction))->value;                                               \
        const SZrTypeValue *rightValue__ = &BASE(B1(instruction))->value;                                              \
        if (ZR_VALUE_IS_TYPE_INT(leftValue__->type) && ZR_VALUE_IS_TYPE_INT(rightValue__->type)) {                    \
            EXECUTION_STORE_PLAIN_DIRECT_TO(plainDestination__,                                                        \
                                            nativeInt64,                                                               \
                                            leftValue__->value.nativeObject.nativeInt64 -                             \
                                                    rightValue__->value.nativeObject.nativeInt64,                     \
                                            leftValue__->type);                                                       \
        } else {                                                                                                       \
            destination = plainDestination__;                                                                          \
            execution_try_binary_numeric_float_fallback_or_raise(                                                      \
                    state, ZR_EXEC_NUMERIC_FALLBACK_SUB, destination, leftValue__, rightValue__, "SUB_INT");          \
        }                                                                                                              \
    } while (0)
#define EXECUTE_SUB_INT_CONST_BODY()                                                                                   \
    do {                                                                                                               \
        const SZrTypeValue *constOpB = CONST(B1(instruction));                                                         \
        opA = &BASE(A1(instruction))->value;                                                                           \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(constOpB->type));                                                               \
        if (ZR_VALUE_IS_TYPE_INT(opA->type)) {                                                                         \
            ALGORITHM_CONST_2(nativeInt64, -, opA->type, constOpB->value.nativeObject.nativeInt64);                   \
        } else {                                                                                                       \
            execution_try_binary_numeric_float_fallback_or_raise(                                                      \
                    state, ZR_EXEC_NUMERIC_FALLBACK_SUB, destination, opA, constOpB, "SUB_INT_CONST");                \
        }                                                                                                              \
    } while (0)
#define EXECUTE_SUB_INT_CONST_BODY_PLAIN_DEST()                                                                        \
    do {                                                                                                               \
        SZrTypeValue *plainDestination__ = &BASE(E(instruction))->value;                                               \
        const SZrTypeValue *constOpB = CONST(B1(instruction));                                                         \
        const SZrTypeValue *leftValue__ = &BASE(A1(instruction))->value;                                               \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(constOpB->type));                                                               \
        if (ZR_VALUE_IS_TYPE_INT(leftValue__->type)) {                                                                 \
            EXECUTION_STORE_PLAIN_DIRECT_TO(plainDestination__,                                                        \
                                            nativeInt64,                                                               \
                                            leftValue__->value.nativeObject.nativeInt64 -                             \
                                                    constOpB->value.nativeObject.nativeInt64,                         \
                                            leftValue__->type);                                                       \
        } else {                                                                                                       \
            destination = plainDestination__;                                                                          \
            execution_try_binary_numeric_float_fallback_or_raise(                                                      \
                    state, ZR_EXEC_NUMERIC_FALLBACK_SUB, destination, leftValue__, constOpB, "SUB_INT_CONST");       \
        }                                                                                                              \
    } while (0)
#define EXECUTE_TYPED_SIGNED_BINARY_BODY(OP)                                                                           \
    do {                                                                                                               \
        opA = &BASE(A1(instruction))->value;                                                                           \
        opB = &BASE(B1(instruction))->value;                                                                           \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(opA->type) && ZR_VALUE_IS_TYPE_SIGNED_INT(opB->type));                  \
        ALGORITHM_2(nativeInt64, OP, opA->type);                                                                       \
    } while (0)
#define EXECUTE_TYPED_SIGNED_BINARY_BODY_PLAIN_DEST(OP)                                                                \
    do {                                                                                                               \
        SZrTypeValue *plainDestination__ = &BASE(E(instruction))->value;                                               \
        const SZrTypeValue *leftValue__ = &BASE(A1(instruction))->value;                                               \
        const SZrTypeValue *rightValue__ = &BASE(B1(instruction))->value;                                              \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(leftValue__->type) && ZR_VALUE_IS_TYPE_SIGNED_INT(rightValue__->type)); \
        EXECUTION_STORE_PLAIN_DIRECT_TO(plainDestination__,                                                            \
                                        nativeInt64,                                                                   \
                                        leftValue__->value.nativeObject.nativeInt64 OP                                 \
                                                rightValue__->value.nativeObject.nativeInt64,                          \
                                        leftValue__->type);                                                            \
    } while (0)
#define EXECUTE_TYPED_SIGNED_CONST_BINARY_BODY(OP)                                                                     \
    do {                                                                                                               \
        const SZrTypeValue *constOpB = CONST(B1(instruction));                                                         \
        opA = &BASE(A1(instruction))->value;                                                                           \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(opA->type) && ZR_VALUE_IS_TYPE_SIGNED_INT(constOpB->type));             \
        ALGORITHM_CONST_2(nativeInt64, OP, opA->type, constOpB->value.nativeObject.nativeInt64);                      \
    } while (0)
#define EXECUTE_TYPED_SIGNED_CONST_BINARY_BODY_PLAIN_DEST(OP)                                                          \
    do {                                                                                                               \
        SZrTypeValue *plainDestination__ = &BASE(E(instruction))->value;                                               \
        const SZrTypeValue *constOpB = CONST(B1(instruction));                                                         \
        const SZrTypeValue *leftValue__ = &BASE(A1(instruction))->value;                                               \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(leftValue__->type) && ZR_VALUE_IS_TYPE_SIGNED_INT(constOpB->type));     \
        EXECUTION_STORE_PLAIN_DIRECT_TO(plainDestination__,                                                            \
                                        nativeInt64,                                                                   \
                                        leftValue__->value.nativeObject.nativeInt64 OP                                 \
                                                constOpB->value.nativeObject.nativeInt64,                              \
                                        leftValue__->type);                                                            \
    } while (0)
#define EXECUTE_ADD_SIGNED_LOAD_CONST_BODY()                                                                           \
    do {                                                                                                               \
        TZrUInt16 constantIndex__ = instruction.instruction.operand.operand1[1];                                       \
        const SZrTypeValue *constOpB = CONST(constantIndex__);                                                         \
        EXECUTE_MATERIALIZE_CONSTANT_SLOT(constantIndex__, instruction.instruction.operand.operand0[1]);               \
        opA = &BASE(instruction.instruction.operand.operand0[0])->value;                                               \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(opA->type) && ZR_VALUE_IS_TYPE_SIGNED_INT(constOpB->type));             \
        ALGORITHM_CONST_2(nativeInt64, +, opA->type, constOpB->value.nativeObject.nativeInt64);                       \
    } while (0)
#define EXECUTE_ADD_SIGNED_LOAD_STACK_CONST_BODY()                                                                     \
    do {                                                                                                               \
        TZrUInt16 constantIndex__ = instruction.instruction.operand.operand1[1];                                       \
        const SZrTypeValue *constOpB = CONST(constantIndex__);                                                         \
        EXECUTE_MATERIALIZE_STACK_SLOT(instruction.instruction.operand.operand0[0],                                    \
                                       instruction.instruction.operand.operand0[1]);                                   \
        opA = &BASE(instruction.instruction.operand.operand0[1])->value;                                               \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(opA->type) && ZR_VALUE_IS_TYPE_SIGNED_INT(constOpB->type));             \
        ALGORITHM_CONST_2(nativeInt64, +, opA->type, constOpB->value.nativeObject.nativeInt64);                       \
    } while (0)
#define EXECUTE_ADD_SIGNED_LOAD_STACK_BODY()                                                                           \
    do {                                                                                                               \
        opA = &BASE(instruction.instruction.operand.operand0[0])->value;                                               \
        opB = &BASE(instruction.instruction.operand.operand0[1])->value;                                               \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(opA->type) && ZR_VALUE_IS_TYPE_SIGNED_INT(opB->type));                  \
        ALGORITHM_2(nativeInt64, +, opA->type);                                                                        \
    } while (0)
#define EXECUTE_ADD_SIGNED_LOAD_STACK_LOAD_CONST_BODY()                                                                \
    do {                                                                                                               \
        TZrUInt16 constantIndex__ = instruction.instruction.operand.operand1[1];                                       \
        const SZrTypeValue *constOpB = CONST(constantIndex__);                                                         \
        EXECUTE_MATERIALIZE_STACK_SLOT(instruction.instruction.operand.operand0[0],                                    \
                                       instruction.instruction.operand.operand0[1]);                                   \
        EXECUTE_MATERIALIZE_CONSTANT_SLOT(constantIndex__, instruction.instruction.operand.operand0[2]);               \
        opA = &BASE(instruction.instruction.operand.operand0[1])->value;                                               \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(opA->type) && ZR_VALUE_IS_TYPE_SIGNED_INT(constOpB->type));             \
        ALGORITHM_CONST_2(nativeInt64, +, opA->type, constOpB->value.nativeObject.nativeInt64);                       \
    } while (0)
#define EXECUTE_SUB_SIGNED_LOAD_CONST_BODY()                                                                           \
    do {                                                                                                               \
        TZrUInt16 constantIndex__ = instruction.instruction.operand.operand1[1];                                       \
        const SZrTypeValue *constOpB = CONST(constantIndex__);                                                         \
        EXECUTE_MATERIALIZE_CONSTANT_SLOT(constantIndex__, instruction.instruction.operand.operand0[1]);               \
        opA = &BASE(instruction.instruction.operand.operand0[0])->value;                                               \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(opA->type) && ZR_VALUE_IS_TYPE_SIGNED_INT(constOpB->type));             \
        ALGORITHM_CONST_2(nativeInt64, -, opA->type, constOpB->value.nativeObject.nativeInt64);                       \
    } while (0)
#define EXECUTE_SUB_SIGNED_LOAD_STACK_CONST_BODY()                                                                     \
    do {                                                                                                               \
        TZrUInt16 constantIndex__ = instruction.instruction.operand.operand1[1];                                       \
        const SZrTypeValue *constOpB = CONST(constantIndex__);                                                         \
        EXECUTE_MATERIALIZE_STACK_SLOT(instruction.instruction.operand.operand0[0],                                    \
                                       instruction.instruction.operand.operand0[1]);                                   \
        opA = &BASE(instruction.instruction.operand.operand0[1])->value;                                               \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(opA->type) && ZR_VALUE_IS_TYPE_SIGNED_INT(constOpB->type));             \
        ALGORITHM_CONST_2(nativeInt64, -, opA->type, constOpB->value.nativeObject.nativeInt64);                       \
    } while (0)
#define EXECUTE_TYPED_UNSIGNED_BINARY_BODY(OP)                                                                         \
    do {                                                                                                               \
        opA = &BASE(A1(instruction))->value;                                                                           \
        opB = &BASE(B1(instruction))->value;                                                                           \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_UNSIGNED_INT(opA->type) && ZR_VALUE_IS_TYPE_UNSIGNED_INT(opB->type));              \
        ALGORITHM_2(nativeUInt64, OP, opA->type);                                                                      \
    } while (0)
#define EXECUTE_TYPED_UNSIGNED_BINARY_BODY_PLAIN_DEST(OP)                                                              \
    do {                                                                                                               \
        SZrTypeValue *plainDestination__ = &BASE(E(instruction))->value;                                               \
        const SZrTypeValue *leftValue__ = &BASE(A1(instruction))->value;                                               \
        const SZrTypeValue *rightValue__ = &BASE(B1(instruction))->value;                                              \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue__->type) &&                                                  \
                  ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightValue__->type));                                                   \
        EXECUTION_STORE_PLAIN_DIRECT_TO(plainDestination__,                                                            \
                                        nativeUInt64,                                                                  \
                                        leftValue__->value.nativeObject.nativeUInt64 OP                                \
                                                rightValue__->value.nativeObject.nativeUInt64,                         \
                                        leftValue__->type);                                                            \
    } while (0)
#define EXECUTE_TYPED_UNSIGNED_CONST_BINARY_BODY(OP)                                                                   \
    do {                                                                                                               \
        const SZrTypeValue *constOpB = CONST(B1(instruction));                                                         \
        opA = &BASE(A1(instruction))->value;                                                                           \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_UNSIGNED_INT(opA->type) && ZR_VALUE_IS_TYPE_UNSIGNED_INT(constOpB->type));         \
        ALGORITHM_CONST_2(nativeUInt64, OP, opA->type, constOpB->value.nativeObject.nativeUInt64);                    \
    } while (0)
#define EXECUTE_TYPED_UNSIGNED_CONST_BINARY_BODY_PLAIN_DEST(OP)                                                        \
    do {                                                                                                               \
        SZrTypeValue *plainDestination__ = &BASE(E(instruction))->value;                                               \
        const SZrTypeValue *constOpB = CONST(B1(instruction));                                                         \
        const SZrTypeValue *leftValue__ = &BASE(A1(instruction))->value;                                               \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue__->type) &&                                                  \
                  ZR_VALUE_IS_TYPE_UNSIGNED_INT(constOpB->type));                                                       \
        EXECUTION_STORE_PLAIN_DIRECT_TO(plainDestination__,                                                            \
                                        nativeUInt64,                                                                  \
                                        leftValue__->value.nativeObject.nativeUInt64 OP                                \
                                                constOpB->value.nativeObject.nativeUInt64,                             \
                                        leftValue__->type);                                                            \
    } while (0)
#define EXECUTE_TYPED_EQUALITY_BOOL_BODY(NEGATE)                                                                       \
    do {                                                                                                               \
        TZrBool equalityResult__;                                                                                      \
        opA = &BASE(A1(instruction))->value;                                                                           \
        opB = &BASE(B1(instruction))->value;                                                                           \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_BOOL(opA->type) && ZR_VALUE_IS_TYPE_BOOL(opB->type));                              \
        equalityResult__ =                                                                                             \
                ((opA->value.nativeObject.nativeBool == opB->value.nativeObject.nativeBool) ? ZR_TRUE : ZR_FALSE) ^  \
                ((NEGATE) ? ZR_TRUE : ZR_FALSE);                                                                       \
        ZR_VALUE_FAST_SET(destination,                                                                                  \
                          nativeBool,                                                                                   \
                          equalityResult__,                                                                             \
                          ZR_VALUE_TYPE_BOOL);                                                                          \
    } while (0)
#define EXECUTE_TYPED_EQUALITY_SIGNED_BODY(NEGATE)                                                                     \
    do {                                                                                                               \
        TZrBool equalityResult__;                                                                                      \
        opA = &BASE(A1(instruction))->value;                                                                           \
        opB = &BASE(B1(instruction))->value;                                                                           \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(opA->type) && ZR_VALUE_IS_TYPE_SIGNED_INT(opB->type));                  \
        equalityResult__ =                                                                                             \
                ((opA->value.nativeObject.nativeInt64 == opB->value.nativeObject.nativeInt64) ? ZR_TRUE : ZR_FALSE) ^\
                ((NEGATE) ? ZR_TRUE : ZR_FALSE);                                                                       \
        ZR_VALUE_FAST_SET(destination,                                                                                  \
                          nativeBool,                                                                                   \
                          equalityResult__,                                                                             \
                          ZR_VALUE_TYPE_BOOL);                                                                          \
    } while (0)
#define EXECUTE_TYPED_EQUALITY_SIGNED_CONST_BODY(NEGATE)                                                               \
    do {                                                                                                               \
        TZrBool equalityResult__;                                                                                      \
        const SZrTypeValue *constOpB = CONST(B1(instruction));                                                         \
        opA = &BASE(A1(instruction))->value;                                                                           \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(opA->type) && ZR_VALUE_IS_TYPE_SIGNED_INT(constOpB->type));             \
        equalityResult__ =                                                                                             \
                ((opA->value.nativeObject.nativeInt64 == constOpB->value.nativeObject.nativeInt64) ? ZR_TRUE          \
                                                                                                 : ZR_FALSE) ^        \
                ((NEGATE) ? ZR_TRUE : ZR_FALSE);                                                                       \
        ZR_VALUE_FAST_SET(destination, nativeBool, equalityResult__, ZR_VALUE_TYPE_BOOL);                              \
    } while (0)
#define EXECUTE_TYPED_EQUALITY_UNSIGNED_BODY(NEGATE)                                                                   \
    do {                                                                                                               \
        TZrBool equalityResult__;                                                                                      \
        opA = &BASE(A1(instruction))->value;                                                                           \
        opB = &BASE(B1(instruction))->value;                                                                           \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_UNSIGNED_INT(opA->type) && ZR_VALUE_IS_TYPE_UNSIGNED_INT(opB->type));              \
        equalityResult__ =                                                                                             \
                ((opA->value.nativeObject.nativeUInt64 == opB->value.nativeObject.nativeUInt64) ?                     \
                         ZR_TRUE : ZR_FALSE) ^                                                                         \
                ((NEGATE) ? ZR_TRUE : ZR_FALSE);                                                                       \
        ZR_VALUE_FAST_SET(destination,                                                                                  \
                          nativeBool,                                                                                   \
                          equalityResult__,                                                                             \
                          ZR_VALUE_TYPE_BOOL);                                                                          \
    } while (0)
#define EXECUTE_TYPED_EQUALITY_FLOAT_BODY(NEGATE)                                                                      \
    do {                                                                                                               \
        TZrBool equalityResult__;                                                                                      \
        opA = &BASE(A1(instruction))->value;                                                                           \
        opB = &BASE(B1(instruction))->value;                                                                           \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_FLOAT(opA->type) && ZR_VALUE_IS_TYPE_FLOAT(opB->type));                            \
        equalityResult__ =                                                                                             \
                ((opA->value.nativeObject.nativeDouble == opB->value.nativeObject.nativeDouble) ? ZR_TRUE : ZR_FALSE) \
                ^ ((NEGATE) ? ZR_TRUE : ZR_FALSE);                                                                     \
        ZR_VALUE_FAST_SET(destination,                                                                                  \
                          nativeBool,                                                                                   \
                          equalityResult__,                                                                             \
                          ZR_VALUE_TYPE_BOOL);                                                                          \
    } while (0)
#define EXECUTE_TYPED_EQUALITY_STRING_BODY(NEGATE)                                                                     \
    do {                                                                                                               \
        TZrBool equalityResult__;                                                                                      \
        opA = &BASE(A1(instruction))->value;                                                                           \
        opB = &BASE(B1(instruction))->value;                                                                           \
        ZR_ASSERT(opA->type == ZR_VALUE_TYPE_STRING && opB->type == ZR_VALUE_TYPE_STRING);                            \
        equalityResult__ = execution_string_values_equal_fast(state, opA, opB) ^                                      \
                          ((NEGATE) ? ZR_TRUE : ZR_FALSE);                                                             \
        ZR_VALUE_FAST_SET(destination,                                                                                  \
                          nativeBool,                                                                                   \
                          equalityResult__,                                                                             \
                          ZR_VALUE_TYPE_BOOL);                                                                          \
    } while (0)
#define EXECUTE_MUL_SIGNED_BODY()                                                                                      \
    do {                                                                                                               \
        opA = &BASE(A1(instruction))->value;                                                                           \
        opB = &BASE(B1(instruction))->value;                                                                           \
        if (ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type)) {                                     \
            ALGORITHM_2(nativeInt64, *, ZR_VALUE_TYPE_INT64);                                                          \
        } else {                                                                                                       \
            execution_try_binary_numeric_float_fallback_or_raise(                                                      \
                    state, ZR_EXEC_NUMERIC_FALLBACK_MUL, destination, opA, opB, "MUL_SIGNED");                        \
        }                                                                                                              \
    } while (0)
#define EXECUTE_MUL_SIGNED_BODY_PLAIN_DEST()                                                                           \
    do {                                                                                                               \
        SZrTypeValue *plainDestination__ = &BASE(E(instruction))->value;                                               \
        const SZrTypeValue *leftValue__ = &BASE(A1(instruction))->value;                                               \
        const SZrTypeValue *rightValue__ = &BASE(B1(instruction))->value;                                              \
        if (ZR_VALUE_IS_TYPE_INT(leftValue__->type) && ZR_VALUE_IS_TYPE_INT(rightValue__->type)) {                    \
            EXECUTION_STORE_PLAIN_DIRECT_TO(plainDestination__,                                                        \
                                            nativeInt64,                                                               \
                                            leftValue__->value.nativeObject.nativeInt64 *                             \
                                                    rightValue__->value.nativeObject.nativeInt64,                     \
                                            ZR_VALUE_TYPE_INT64);                                                     \
        } else {                                                                                                       \
            destination = plainDestination__;                                                                          \
            execution_try_binary_numeric_float_fallback_or_raise(                                                      \
                    state, ZR_EXEC_NUMERIC_FALLBACK_MUL, destination, leftValue__, rightValue__, "MUL_SIGNED");      \
        }                                                                                                              \
    } while (0)
#define EXECUTE_MUL_SIGNED_CONST_BODY()                                                                                \
    do {                                                                                                               \
        const SZrTypeValue *constOpB = CONST(B1(instruction));                                                         \
        opA = &BASE(A1(instruction))->value;                                                                           \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(constOpB->type));                                                               \
        if (ZR_VALUE_IS_TYPE_INT(opA->type)) {                                                                         \
            ALGORITHM_CONST_2(nativeInt64, *, ZR_VALUE_TYPE_INT64, constOpB->value.nativeObject.nativeInt64);         \
        } else {                                                                                                       \
            execution_try_binary_numeric_float_fallback_or_raise(                                                      \
                    state,                                                                                             \
                    ZR_EXEC_NUMERIC_FALLBACK_MUL,                                                                      \
                    destination,                                                                                       \
                    opA,                                                                                               \
                    constOpB,                                                                                          \
                    "MUL_SIGNED_CONST");                                                                               \
        }                                                                                                              \
    } while (0)
#define EXECUTE_MUL_SIGNED_CONST_BODY_PLAIN_DEST()                                                                     \
    do {                                                                                                               \
        SZrTypeValue *plainDestination__ = &BASE(E(instruction))->value;                                               \
        const SZrTypeValue *constOpB = CONST(B1(instruction));                                                         \
        const SZrTypeValue *leftValue__ = &BASE(A1(instruction))->value;                                               \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(constOpB->type));                                                               \
        if (ZR_VALUE_IS_TYPE_INT(leftValue__->type)) {                                                                 \
            EXECUTION_STORE_PLAIN_DIRECT_TO(plainDestination__,                                                        \
                                            nativeInt64,                                                               \
                                            leftValue__->value.nativeObject.nativeInt64 *                             \
                                                    constOpB->value.nativeObject.nativeInt64,                         \
                                            ZR_VALUE_TYPE_INT64);                                                     \
        } else {                                                                                                       \
            destination = plainDestination__;                                                                          \
            execution_try_binary_numeric_float_fallback_or_raise(                                                      \
                    state,                                                                                             \
                    ZR_EXEC_NUMERIC_FALLBACK_MUL,                                                                      \
                    destination,                                                                                       \
                    leftValue__,                                                                                       \
                    constOpB,                                                                                          \
                    "MUL_SIGNED_CONST");                                                                               \
        }                                                                                                              \
    } while (0)
#define EXECUTE_MUL_SIGNED_LOAD_CONST_BODY()                                                                           \
    do {                                                                                                               \
        TZrUInt16 constantIndex__ = instruction.instruction.operand.operand1[1];                                       \
        const SZrTypeValue *constOpB = CONST(constantIndex__);                                                         \
        EXECUTE_MATERIALIZE_CONSTANT_SLOT(constantIndex__, instruction.instruction.operand.operand0[1]);               \
        opA = &BASE(instruction.instruction.operand.operand0[0])->value;                                               \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(constOpB->type));                                                               \
        if (ZR_VALUE_IS_TYPE_INT(opA->type)) {                                                                         \
            ALGORITHM_CONST_2(nativeInt64, *, ZR_VALUE_TYPE_INT64, constOpB->value.nativeObject.nativeInt64);         \
        } else {                                                                                                       \
            execution_try_binary_numeric_float_fallback_or_raise(                                                      \
                    state,                                                                                             \
                    ZR_EXEC_NUMERIC_FALLBACK_MUL,                                                                      \
                    destination,                                                                                       \
                    opA,                                                                                               \
                    constOpB,                                                                                          \
                    "MUL_SIGNED_LOAD_CONST");                                                                          \
        }                                                                                                              \
    } while (0)
#define EXECUTE_MUL_SIGNED_LOAD_STACK_CONST_BODY()                                                                     \
    do {                                                                                                               \
        TZrUInt16 constantIndex__ = instruction.instruction.operand.operand1[1];                                       \
        const SZrTypeValue *constOpB = CONST(constantIndex__);                                                         \
        EXECUTE_MATERIALIZE_STACK_SLOT(instruction.instruction.operand.operand0[0],                                    \
                                       instruction.instruction.operand.operand0[1]);                                   \
        opA = &BASE(instruction.instruction.operand.operand0[1])->value;                                               \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(constOpB->type));                                                               \
        if (ZR_VALUE_IS_TYPE_INT(opA->type)) {                                                                         \
            ALGORITHM_CONST_2(nativeInt64, *, ZR_VALUE_TYPE_INT64, constOpB->value.nativeObject.nativeInt64);         \
        } else {                                                                                                       \
            execution_try_binary_numeric_float_fallback_or_raise(                                                      \
                    state,                                                                                             \
                    ZR_EXEC_NUMERIC_FALLBACK_MUL,                                                                      \
                    destination,                                                                                       \
                    opA,                                                                                               \
                    constOpB,                                                                                          \
                    "MUL_SIGNED_LOAD_STACK_CONST");                                                                    \
        }                                                                                                              \
    } while (0)
#define EXECUTE_DIV_SIGNED_CONST_BODY()                                                                                \
    do {                                                                                                               \
        const SZrTypeValue *constOpB = CONST(B1(instruction));                                                         \
        opA = &BASE(A1(instruction))->value;                                                                           \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(constOpB->type));                                                               \
        if (ZR_VALUE_IS_TYPE_INT(opA->type)) {                                                                         \
            TZrInt64 divisor = constOpB->value.nativeObject.nativeInt64;                                               \
            SAVE_STATE(state, callInfo);                                                                               \
            if (ZR_UNLIKELY(divisor == 0)) {                                                                           \
                ZrCore_Debug_RunError(state, "divide by zero");                                                        \
            }                                                                                                          \
            ALGORITHM_CONST_2(nativeInt64, /, ZR_VALUE_TYPE_INT64, divisor);                                          \
        } else {                                                                                                       \
            execution_try_binary_numeric_float_fallback_or_raise(                                                      \
                    state,                                                                                             \
                    ZR_EXEC_NUMERIC_FALLBACK_DIV,                                                                      \
                    destination,                                                                                       \
                    opA,                                                                                               \
                    constOpB,                                                                                          \
                    "DIV_SIGNED_CONST");                                                                               \
        }                                                                                                              \
    } while (0)
#define EXECUTE_DIV_SIGNED_CONST_BODY_PLAIN_DEST()                                                                     \
    do {                                                                                                               \
        SZrTypeValue *plainDestination__ = &BASE(E(instruction))->value;                                               \
        const SZrTypeValue *constOpB = CONST(B1(instruction));                                                         \
        const SZrTypeValue *leftValue__ = &BASE(A1(instruction))->value;                                               \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(constOpB->type));                                                               \
        if (ZR_VALUE_IS_TYPE_INT(leftValue__->type)) {                                                                 \
            TZrInt64 divisor = constOpB->value.nativeObject.nativeInt64;                                               \
            SAVE_STATE(state, callInfo);                                                                               \
            if (ZR_UNLIKELY(divisor == 0)) {                                                                           \
                ZrCore_Debug_RunError(state, "divide by zero");                                                        \
            }                                                                                                          \
            EXECUTION_STORE_PLAIN_DIRECT_TO(plainDestination__,                                                        \
                                            nativeInt64,                                                               \
                                            leftValue__->value.nativeObject.nativeInt64 / divisor,                    \
                                            ZR_VALUE_TYPE_INT64);                                                     \
        } else {                                                                                                       \
            destination = plainDestination__;                                                                          \
            execution_try_binary_numeric_float_fallback_or_raise(                                                      \
                    state,                                                                                             \
                    ZR_EXEC_NUMERIC_FALLBACK_DIV,                                                                      \
                    destination,                                                                                       \
                    leftValue__,                                                                                       \
                    constOpB,                                                                                          \
                    "DIV_SIGNED_CONST");                                                                               \
        }                                                                                                              \
    } while (0)
#define EXECUTE_DIV_SIGNED_LOAD_CONST_BODY()                                                                           \
    do {                                                                                                               \
        TZrUInt16 constantIndex__ = instruction.instruction.operand.operand1[1];                                       \
        const SZrTypeValue *constOpB = CONST(constantIndex__);                                                         \
        EXECUTE_MATERIALIZE_CONSTANT_SLOT(constantIndex__, instruction.instruction.operand.operand0[1]);               \
        opA = &BASE(instruction.instruction.operand.operand0[0])->value;                                               \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(constOpB->type));                                                               \
        if (ZR_VALUE_IS_TYPE_INT(opA->type)) {                                                                         \
            TZrInt64 divisor = constOpB->value.nativeObject.nativeInt64;                                               \
            SAVE_STATE(state, callInfo);                                                                               \
            if (ZR_UNLIKELY(divisor == 0)) {                                                                           \
                ZrCore_Debug_RunError(state, "divide by zero");                                                        \
            }                                                                                                          \
            ALGORITHM_CONST_2(nativeInt64, /, ZR_VALUE_TYPE_INT64, divisor);                                          \
        } else {                                                                                                       \
            execution_try_binary_numeric_float_fallback_or_raise(                                                      \
                    state,                                                                                             \
                    ZR_EXEC_NUMERIC_FALLBACK_DIV,                                                                      \
                    destination,                                                                                       \
                    opA,                                                                                               \
                    constOpB,                                                                                          \
                    "DIV_SIGNED_LOAD_CONST");                                                                          \
        }                                                                                                              \
    } while (0)
#define EXECUTE_DIV_SIGNED_LOAD_STACK_CONST_BODY()                                                                     \
    do {                                                                                                               \
        TZrUInt16 constantIndex__ = instruction.instruction.operand.operand1[1];                                       \
        const SZrTypeValue *constOpB = CONST(constantIndex__);                                                         \
        EXECUTE_MATERIALIZE_STACK_SLOT(instruction.instruction.operand.operand0[0],                                    \
                                       instruction.instruction.operand.operand0[1]);                                   \
        opA = &BASE(instruction.instruction.operand.operand0[1])->value;                                               \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(constOpB->type));                                                               \
        if (ZR_VALUE_IS_TYPE_INT(opA->type)) {                                                                         \
            TZrInt64 divisor = constOpB->value.nativeObject.nativeInt64;                                               \
            SAVE_STATE(state, callInfo);                                                                               \
            if (ZR_UNLIKELY(divisor == 0)) {                                                                           \
                ZrCore_Debug_RunError(state, "divide by zero");                                                        \
            }                                                                                                          \
            ALGORITHM_CONST_2(nativeInt64, /, ZR_VALUE_TYPE_INT64, divisor);                                          \
        } else {                                                                                                       \
            execution_try_binary_numeric_float_fallback_or_raise(                                                      \
                    state,                                                                                             \
                    ZR_EXEC_NUMERIC_FALLBACK_DIV,                                                                      \
                    destination,                                                                                       \
                    opA,                                                                                               \
                    constOpB,                                                                                          \
                    "DIV_SIGNED_LOAD_STACK_CONST");                                                                    \
        }                                                                                                              \
    } while (0)
#define EXECUTE_MOD_SIGNED_CONST_BODY()                                                                                \
    do {                                                                                                               \
        const SZrTypeValue *constOpB = CONST(B1(instruction));                                                         \
        opA = &BASE(A1(instruction))->value;                                                                           \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(constOpB->type));                                                               \
        if (ZR_VALUE_IS_TYPE_INT(opA->type)) {                                                                         \
            TZrInt64 divisor = constOpB->value.nativeObject.nativeInt64;                                               \
            SAVE_STATE(state, callInfo);                                                                               \
            if (ZR_UNLIKELY(divisor == 0)) {                                                                           \
                ZrCore_Debug_RunError(state, "modulo by zero");                                                        \
            }                                                                                                          \
            if (ZR_UNLIKELY(divisor < 0)) {                                                                            \
                divisor = -divisor;                                                                                    \
            }                                                                                                          \
            ALGORITHM_CONST_2(nativeInt64, %, ZR_VALUE_TYPE_INT64, divisor);                                          \
        } else {                                                                                                       \
            execution_try_binary_numeric_float_fallback_or_raise(                                                      \
                    state,                                                                                             \
                    ZR_EXEC_NUMERIC_FALLBACK_MOD,                                                                      \
                    destination,                                                                                       \
                    opA,                                                                                               \
                    constOpB,                                                                                          \
                    "MOD_SIGNED_CONST");                                                                               \
        }                                                                                                              \
    } while (0)
#define EXECUTE_MOD_SIGNED_CONST_BODY_PLAIN_DEST()                                                                     \
    do {                                                                                                               \
        SZrTypeValue *plainDestination__ = &BASE(E(instruction))->value;                                               \
        const SZrTypeValue *constOpB = CONST(B1(instruction));                                                         \
        const SZrTypeValue *leftValue__ = &BASE(A1(instruction))->value;                                               \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(constOpB->type));                                                               \
        if (ZR_VALUE_IS_TYPE_INT(leftValue__->type)) {                                                                 \
            TZrInt64 divisor = constOpB->value.nativeObject.nativeInt64;                                               \
            SAVE_STATE(state, callInfo);                                                                               \
            if (ZR_UNLIKELY(divisor == 0)) {                                                                           \
                ZrCore_Debug_RunError(state, "modulo by zero");                                                        \
            }                                                                                                          \
            if (ZR_UNLIKELY(divisor < 0)) {                                                                            \
                divisor = -divisor;                                                                                    \
            }                                                                                                          \
            EXECUTION_STORE_PLAIN_DIRECT_TO(plainDestination__,                                                        \
                                            nativeInt64,                                                               \
                                            leftValue__->value.nativeObject.nativeInt64 % divisor,                    \
                                            ZR_VALUE_TYPE_INT64);                                                     \
        } else {                                                                                                       \
            destination = plainDestination__;                                                                          \
            execution_try_binary_numeric_float_fallback_or_raise(                                                      \
                    state,                                                                                             \
                    ZR_EXEC_NUMERIC_FALLBACK_MOD,                                                                      \
                    destination,                                                                                       \
                    leftValue__,                                                                                       \
                    constOpB,                                                                                          \
                    "MOD_SIGNED_CONST");                                                                               \
        }                                                                                                              \
    } while (0)
#define EXECUTE_MOD_SIGNED_LOAD_CONST_BODY()                                                                           \
    do {                                                                                                               \
        TZrUInt16 constantIndex__ = instruction.instruction.operand.operand1[1];                                       \
        const SZrTypeValue *constOpB = CONST(constantIndex__);                                                         \
        EXECUTE_MATERIALIZE_CONSTANT_SLOT(constantIndex__, instruction.instruction.operand.operand0[1]);               \
        opA = &BASE(instruction.instruction.operand.operand0[0])->value;                                               \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(constOpB->type));                                                               \
        if (ZR_VALUE_IS_TYPE_INT(opA->type)) {                                                                         \
            TZrInt64 divisor = constOpB->value.nativeObject.nativeInt64;                                               \
            SAVE_STATE(state, callInfo);                                                                               \
            if (ZR_UNLIKELY(divisor == 0)) {                                                                           \
                ZrCore_Debug_RunError(state, "modulo by zero");                                                        \
            }                                                                                                          \
            if (ZR_UNLIKELY(divisor < 0)) {                                                                            \
                divisor = -divisor;                                                                                    \
            }                                                                                                          \
            ALGORITHM_CONST_2(nativeInt64, %, ZR_VALUE_TYPE_INT64, divisor);                                          \
        } else {                                                                                                       \
            execution_try_binary_numeric_float_fallback_or_raise(                                                      \
                    state,                                                                                             \
                    ZR_EXEC_NUMERIC_FALLBACK_MOD,                                                                      \
                    destination,                                                                                       \
                    opA,                                                                                               \
                    constOpB,                                                                                          \
                    "MOD_SIGNED_LOAD_CONST");                                                                          \
        }                                                                                                              \
    } while (0)
#define EXECUTE_MOD_SIGNED_LOAD_STACK_CONST_BODY()                                                                     \
    do {                                                                                                               \
        TZrUInt16 constantIndex__ = instruction.instruction.operand.operand1[1];                                       \
        const SZrTypeValue *constOpB = CONST(constantIndex__);                                                         \
        EXECUTE_MATERIALIZE_STACK_SLOT(instruction.instruction.operand.operand0[0],                                    \
                                       instruction.instruction.operand.operand0[1]);                                   \
        opA = &BASE(instruction.instruction.operand.operand0[1])->value;                                               \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(constOpB->type));                                                               \
        if (ZR_VALUE_IS_TYPE_INT(opA->type)) {                                                                         \
            TZrInt64 divisor = constOpB->value.nativeObject.nativeInt64;                                               \
            SAVE_STATE(state, callInfo);                                                                               \
            if (ZR_UNLIKELY(divisor == 0)) {                                                                           \
                ZrCore_Debug_RunError(state, "modulo by zero");                                                        \
            }                                                                                                          \
            if (ZR_UNLIKELY(divisor < 0)) {                                                                            \
                divisor = -divisor;                                                                                    \
            }                                                                                                          \
            ALGORITHM_CONST_2(nativeInt64, %, ZR_VALUE_TYPE_INT64, divisor);                                          \
        } else {                                                                                                       \
            execution_try_binary_numeric_float_fallback_or_raise(                                                      \
                    state,                                                                                             \
                    ZR_EXEC_NUMERIC_FALLBACK_MOD,                                                                      \
                    destination,                                                                                       \
                    opA,                                                                                               \
                    constOpB,                                                                                          \
                    "MOD_SIGNED_LOAD_STACK_CONST");                                                                    \
        }                                                                                                              \
    } while (0)
#define EXECUTE_MUL_UNSIGNED_CONST_BODY()                                                                              \
    do {                                                                                                               \
        const SZrTypeValue *constOpB = CONST(B1(instruction));                                                         \
        opA = &BASE(A1(instruction))->value;                                                                           \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_UNSIGNED_INT(opA->type) && ZR_VALUE_IS_TYPE_UNSIGNED_INT(constOpB->type));         \
        ALGORITHM_CONST_2(nativeUInt64, *, opA->type, constOpB->value.nativeObject.nativeUInt64);                     \
    } while (0)
#define EXECUTE_MUL_UNSIGNED_CONST_BODY_PLAIN_DEST()                                                                   \
    do {                                                                                                               \
        SZrTypeValue *plainDestination__ = &BASE(E(instruction))->value;                                               \
        const SZrTypeValue *constOpB = CONST(B1(instruction));                                                         \
        const SZrTypeValue *leftValue__ = &BASE(A1(instruction))->value;                                               \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue__->type) &&                                                  \
                  ZR_VALUE_IS_TYPE_UNSIGNED_INT(constOpB->type));                                                       \
        EXECUTION_STORE_PLAIN_DIRECT_TO(plainDestination__,                                                            \
                                        nativeUInt64,                                                                  \
                                        leftValue__->value.nativeObject.nativeUInt64 *                                 \
                                                constOpB->value.nativeObject.nativeUInt64,                             \
                                        leftValue__->type);                                                            \
    } while (0)
#define EXECUTE_DIV_UNSIGNED_CONST_BODY()                                                                              \
    do {                                                                                                               \
        const SZrTypeValue *constOpB = CONST(B1(instruction));                                                         \
        opA = &BASE(A1(instruction))->value;                                                                           \
        TZrUInt64 divisor;                                                                                             \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_UNSIGNED_INT(opA->type) && ZR_VALUE_IS_TYPE_UNSIGNED_INT(constOpB->type));         \
        divisor = constOpB->value.nativeObject.nativeUInt64;                                                           \
        SAVE_STATE(state, callInfo);                                                                                   \
        if (ZR_UNLIKELY(divisor == 0)) {                                                                               \
            ZrCore_Debug_RunError(state, "divide by zero");                                                            \
        }                                                                                                              \
        ALGORITHM_CONST_2(nativeUInt64, /, opA->type, divisor);                                                        \
    } while (0)
#define EXECUTE_DIV_UNSIGNED_CONST_BODY_PLAIN_DEST()                                                                   \
    do {                                                                                                               \
        SZrTypeValue *plainDestination__ = &BASE(E(instruction))->value;                                               \
        const SZrTypeValue *constOpB = CONST(B1(instruction));                                                         \
        const SZrTypeValue *leftValue__ = &BASE(A1(instruction))->value;                                               \
        TZrUInt64 divisor;                                                                                             \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue__->type) &&                                                  \
                  ZR_VALUE_IS_TYPE_UNSIGNED_INT(constOpB->type));                                                       \
        divisor = constOpB->value.nativeObject.nativeUInt64;                                                           \
        SAVE_STATE(state, callInfo);                                                                                   \
        if (ZR_UNLIKELY(divisor == 0)) {                                                                               \
            ZrCore_Debug_RunError(state, "divide by zero");                                                            \
        }                                                                                                              \
        EXECUTION_STORE_PLAIN_DIRECT_TO(plainDestination__,                                                            \
                                        nativeUInt64,                                                                  \
                                        leftValue__->value.nativeObject.nativeUInt64 / divisor,                        \
                                        leftValue__->type);                                                            \
    } while (0)
#define EXECUTE_MOD_UNSIGNED_CONST_BODY()                                                                              \
    do {                                                                                                               \
        const SZrTypeValue *constOpB = CONST(B1(instruction));                                                         \
        opA = &BASE(A1(instruction))->value;                                                                           \
        TZrUInt64 divisor;                                                                                             \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_UNSIGNED_INT(opA->type) && ZR_VALUE_IS_TYPE_UNSIGNED_INT(constOpB->type));         \
        divisor = constOpB->value.nativeObject.nativeUInt64;                                                           \
        SAVE_STATE(state, callInfo);                                                                                   \
        if (ZR_UNLIKELY(divisor == 0)) {                                                                               \
            ZrCore_Debug_RunError(state, "modulo by zero");                                                            \
        }                                                                                                              \
        ALGORITHM_CONST_2(nativeUInt64, %, opA->type, divisor);                                                        \
    } while (0)
#define EXECUTE_MOD_UNSIGNED_CONST_BODY_PLAIN_DEST()                                                                   \
    do {                                                                                                               \
        SZrTypeValue *plainDestination__ = &BASE(E(instruction))->value;                                               \
        const SZrTypeValue *constOpB = CONST(B1(instruction));                                                         \
        const SZrTypeValue *leftValue__ = &BASE(A1(instruction))->value;                                               \
        TZrUInt64 divisor;                                                                                             \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue__->type) &&                                                  \
                  ZR_VALUE_IS_TYPE_UNSIGNED_INT(constOpB->type));                                                       \
        divisor = constOpB->value.nativeObject.nativeUInt64;                                                           \
        SAVE_STATE(state, callInfo);                                                                                   \
        if (ZR_UNLIKELY(divisor == 0)) {                                                                               \
            ZrCore_Debug_RunError(state, "modulo by zero");                                                            \
        }                                                                                                              \
        EXECUTION_STORE_PLAIN_DIRECT_TO(plainDestination__,                                                            \
                                        nativeUInt64,                                                                  \
                                        leftValue__->value.nativeObject.nativeUInt64 % divisor,                        \
                                        leftValue__->type);                                                            \
    } while (0)
#define EXECUTE_LOGICAL_LESS_EQUAL_SIGNED_BODY()                                                                       \
    do {                                                                                                               \
        opA = &BASE(A1(instruction))->value;                                                                           \
        opB = &BASE(B1(instruction))->value;                                                                           \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));                                \
        ALGORITHM_CVT_2(nativeBool, nativeInt64, <=, ZR_VALUE_TYPE_BOOL);                                             \
    } while (0)
#define EXECUTE_JUMP_IF_GREATER_SIGNED_BODY(JUMP_IMPL, CALL_INFO)                                                     \
    do {                                                                                                               \
        const SZrTypeValue *leftValue__ = &BASE(E(instruction))->value;                                                \
        const SZrTypeValue *rightValue__ = &BASE(A1(instruction))->value;                                              \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(leftValue__->type) && ZR_VALUE_IS_TYPE_INT(rightValue__->type));               \
        if (leftValue__->value.nativeObject.nativeInt64 > rightValue__->value.nativeObject.nativeInt64) {             \
            JUMP_IMPL(CALL_INFO, instruction, 0);                                                                      \
        }                                                                                                              \
    } while (0)
#define EXECUTE_JUMP_IF_NOT_EQUAL_SIGNED_BODY(JUMP_IMPL, CALL_INFO)                                                   \
    do {                                                                                                               \
        const SZrTypeValue *leftValue__ = &BASE(E(instruction))->value;                                                \
        const SZrTypeValue *rightValue__ = &BASE(A1(instruction))->value;                                              \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(leftValue__->type) &&                                                    \
                  ZR_VALUE_IS_TYPE_SIGNED_INT(rightValue__->type));                                                    \
        if (leftValue__->value.nativeObject.nativeInt64 != rightValue__->value.nativeObject.nativeInt64) {             \
            JUMP_IMPL(CALL_INFO, instruction, 0);                                                                      \
        }                                                                                                              \
    } while (0)
#define EXECUTE_JUMP_IF_NOT_EQUAL_SIGNED_CONST_BODY(JUMP_IMPL, CALL_INFO)                                             \
    do {                                                                                                               \
        const SZrTypeValue *leftValue__ = &BASE(E(instruction))->value;                                                \
        const SZrTypeValue *constValue__ = CONST(A1(instruction));                                                     \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(leftValue__->type) &&                                                    \
                  ZR_VALUE_IS_TYPE_SIGNED_INT(constValue__->type));                                                    \
        if (leftValue__->value.nativeObject.nativeInt64 != constValue__->value.nativeObject.nativeInt64) {             \
            JUMP_IMPL(CALL_INFO, instruction, 0);                                                                      \
        }                                                                                                              \
    } while (0)
#define EXECUTE_SUPER_ARRAY_BIND_ITEMS_BODY()                                                                          \
    do {                                                                                                               \
        SZrTypeValue *itemsDestination__ = &BASE(E(instruction))->value;                                                \
        SZrTypeValue *receiverValue__ = &BASE(A2(instruction))->value;                                                  \
        SZrObject *itemsObject__ = ZR_NULL;                                                                             \
        ZR_ASSERT(E(instruction) != ZR_INSTRUCTION_USE_RET_FLAG);                                                       \
        if (ZR_UNLIKELY(!zr_super_array_try_resolve_items_cached_only_assume_fast(                                      \
                    state, receiverValue__, ZR_NULL, &itemsObject__) &&                                                 \
                !ZrCore_Object_SuperArrayResolveItemsAssumeFastSlow(                                                    \
                        state, receiverValue__, ZR_NULL, &itemsObject__))) {                                            \
            ZrCore_Debug_RunError(state, "SUPER_ARRAY_BIND_ITEMS: receiver must be an array-like object");             \
        } else {                                                                                                       \
            execution_prepare_destination_for_direct_store_no_profile(state, itemsDestination__);                       \
            ZrCore_Value_InitAsRawObject(state, itemsDestination__, ZR_CAST_RAW_OBJECT_AS_SUPER(itemsObject__));        \
        }                                                                                                              \
    } while (0)
#define EXECUTE_SUPER_ARRAY_GET_INT_BODY()                                                                             \
    do {                                                                                                               \
        SZrTypeValue *resultValue = &BASE(E(instruction))->value;                                                      \
        SZrTypeValue *receiverValue = &BASE(A1(instruction))->value;                                                   \
        const SZrTypeValue *indexValue = &BASE(B1(instruction))->value;                                                \
        ZR_ASSERT(E(instruction) != ZR_INSTRUCTION_USE_RET_FLAG);                                                      \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(indexValue->type));                                                      \
        if (ZR_UNLIKELY(!ZrCore_Object_SuperArrayGetIntByValueInlineAssumeFast(                                        \
                    state,                                                                                              \
                    receiverValue,                                                                                      \
                    indexValue->value.nativeObject.nativeInt64,                                                         \
                    resultValue))) {                                                                                    \
            ZrCore_Debug_RunError(state,                                                                               \
                                  "SUPER_ARRAY_GET_INT: receiver must be an array-like object with int index");       \
        }                                                                                                              \
    } while (0)
#define EXECUTE_SUPER_ARRAY_GET_INT_ITEMS_BODY()                                                                       \
    do {                                                                                                               \
        SZrTypeValue *resultValue__ = &BASE(E(instruction))->value;                                                    \
        const SZrTypeValue *itemsValue__ = &BASE(A1(instruction))->value;                                               \
        const SZrTypeValue *indexValue__ = &BASE(B1(instruction))->value;                                              \
        SZrObject *itemsObject__;                                                                                       \
        ZR_ASSERT(E(instruction) != ZR_INSTRUCTION_USE_RET_FLAG);                                                       \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(indexValue__->type));                                                     \
        itemsObject__ = zr_super_array_bound_items_object_from_value_assume_fast(itemsValue__);                         \
        zr_super_array_get_from_items_object_assume_fast(                                                               \
                state, itemsObject__, indexValue__->value.nativeObject.nativeInt64, resultValue__);                     \
    } while (0)
#define EXECUTE_SUPER_ARRAY_GET_INT_PLAIN_DEST_BODY_TO(RESULT_VALUE)                                                   \
    do {                                                                                                               \
        SZrTypeValue *plainDestination__ = (RESULT_VALUE);                                                             \
        SZrTypeValue *receiverValue__ = &BASE(A1(instruction))->value;                                                 \
        const SZrTypeValue *indexValue__ = &BASE(B1(instruction))->value;                                              \
        SZrObject *itemsObject__ = ZR_NULL;                                                                            \
        TZrInt64 indexInt__ = indexValue__->value.nativeObject.nativeInt64;                                            \
        ZR_ASSERT(E(instruction) != ZR_INSTRUCTION_USE_RET_FLAG);                                                      \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(indexValue__->type));                                                    \
        ZR_ASSERT(zr_super_array_value_can_overwrite_without_release(plainDestination__));                             \
        ZR_ASSERT(zr_super_array_value_is_normalized_plain(plainDestination__));                                       \
        if (ZR_LIKELY(zr_super_array_try_resolve_items_cached_only_assume_fast(                                        \
                    state, receiverValue__, ZR_NULL, &itemsObject__))) {                                               \
            zr_super_array_store_plain_get_from_items_object_assume_fast(                                              \
                    itemsObject__, (TZrUInt64)indexInt__, plainDestination__);                                         \
        } else if (ZR_UNLIKELY(!ZrCore_Object_SuperArrayGetIntByValueInlineAssumeFastPlainDestination(                 \
                                   state, receiverValue__, indexInt__, plainDestination__))) {                        \
            ZrCore_Debug_RunError(state,                                                                               \
                                  "SUPER_ARRAY_GET_INT: receiver must be an array-like object with int index");       \
        }                                                                                                              \
    } while (0)
#define EXECUTE_SUPER_ARRAY_GET_INT_PLAIN_DEST_BODY()                                                                  \
    EXECUTE_SUPER_ARRAY_GET_INT_PLAIN_DEST_BODY_TO(&BASE(E(instruction))->value)
#define EXECUTE_SUPER_ARRAY_GET_INT_ITEMS_PLAIN_DEST_BODY()                                                            \
    do {                                                                                                               \
        SZrTypeValue *plainDestination__ = &BASE(E(instruction))->value;                                                \
        const SZrTypeValue *itemsValue__ = &BASE(A1(instruction))->value;                                               \
        const SZrTypeValue *indexValue__ = &BASE(B1(instruction))->value;                                              \
        SZrObject *itemsObject__;                                                                                       \
        TZrInt64 indexInt__ = indexValue__->value.nativeObject.nativeInt64;                                             \
        ZR_ASSERT(E(instruction) != ZR_INSTRUCTION_USE_RET_FLAG);                                                       \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(indexValue__->type));                                                     \
        ZR_ASSERT(zr_super_array_value_can_overwrite_without_release(plainDestination__));                              \
        ZR_ASSERT(zr_super_array_value_is_normalized_plain(plainDestination__));                                        \
        itemsObject__ = zr_super_array_bound_items_object_from_value_assume_fast(itemsValue__);                         \
        zr_super_array_store_plain_get_from_items_object_assume_fast(                                                   \
                itemsObject__, (TZrUInt64)indexInt__, plainDestination__);                                              \
    } while (0)
#define EXECUTE_SUPER_ARRAY_SET_INT_BODY()                                                                             \
    do {                                                                                                               \
        SZrTypeValue *receiverValue = &BASE(A1(instruction))->value;                                                   \
        const SZrTypeValue *indexValue = &BASE(B1(instruction))->value;                                                \
        const SZrTypeValue *storedValue = &BASE(E(instruction))->value;                                                \
        ZR_ASSERT(E(instruction) != ZR_INSTRUCTION_USE_RET_FLAG);                                                      \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(indexValue->type));                                                      \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(storedValue->type));                                                     \
        if (ZR_UNLIKELY(!ZrCore_Object_SuperArraySetIntByValueInlineAssumeFast(                                        \
                    state,                                                                                              \
                    receiverValue,                                                                                      \
                    indexValue->value.nativeObject.nativeInt64,                                                         \
                    storedValue->value.nativeObject.nativeInt64))) {                                                    \
            ZrCore_Debug_RunError(state,                                                                               \
                                  "SUPER_ARRAY_SET_INT: receiver must be an array-like object with int index");       \
        }                                                                                                              \
    } while (0)
#define EXECUTE_SUPER_ARRAY_SET_INT_ITEMS_BODY()                                                                       \
    do {                                                                                                               \
        const SZrTypeValue *itemsValue__ = &BASE(A1(instruction))->value;                                               \
        const SZrTypeValue *indexValue__ = &BASE(B1(instruction))->value;                                              \
        const SZrTypeValue *storedValue__ = &BASE(E(instruction))->value;                                               \
        SZrObject *itemsObject__;                                                                                       \
        ZR_ASSERT(E(instruction) != ZR_INSTRUCTION_USE_RET_FLAG);                                                       \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(indexValue__->type));                                                     \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(storedValue__->type));                                                    \
        itemsObject__ = zr_super_array_bound_items_object_from_value_assume_fast(itemsValue__);                         \
        zr_super_array_set_int_in_bound_items_object_assume_fast(                                                       \
                state,                                                                                                  \
                itemsObject__,                                                                                          \
                indexValue__->value.nativeObject.nativeInt64,                                                           \
                storedValue__->value.nativeObject.nativeInt64);                                                         \
    } while (0)
#define EXECUTE_SUPER_ARRAY_ADD_INT_BODY()                                                                             \
    do {                                                                                                               \
        opA = &BASE(A1(instruction))->value;                                                                           \
        opB = &BASE(B1(instruction))->value;                                                                           \
        if (ZR_UNLIKELY(!ZrCore_Object_SuperArrayAddIntAssumeFast(state, opA, opB, destination))) {                  \
            ZrCore_Debug_RunError(state,                                                                               \
                                  "SUPER_ARRAY_ADD_INT: receiver must be an array-like object with int payload");     \
        }                                                                                                              \
    } while (0)
#define EXECUTE_SUPER_ARRAY_ADD_INT4_BODY()                                                                            \
    do {                                                                                                               \
        opB = &BASE(B1(instruction))->value;                                                                           \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(opB->type));                                                             \
        if (ZR_UNLIKELY(!ZrCore_Object_SuperArrayAddInt4ConstAssumeFast(                                               \
                    state, BASE(A1(instruction)), opB->value.nativeObject.nativeInt64))) {                            \
            ZrCore_Debug_RunError(state,                                                                               \
                                  "SUPER_ARRAY_ADD_INT4: receiver must be an array-like object with int payload");    \
        }                                                                                                              \
    } while (0)
#define EXECUTE_SUPER_ARRAY_ADD_INT4_CONST_BODY()                                                                      \
    do {                                                                                                               \
        const SZrTypeValue *constantValue = CONST(B1(instruction));                                                    \
        ZR_ASSERT(constantValue != ZR_NULL);                                                                           \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(constantValue->type));                                                   \
        if (ZR_UNLIKELY(!ZrCore_Object_SuperArrayAddInt4ConstAssumeFast(                                               \
                    state, BASE(A1(instruction)), constantValue->value.nativeObject.nativeInt64))) {                  \
            ZrCore_Debug_RunError(state,                                                                               \
                                  "SUPER_ARRAY_ADD_INT4_CONST: receiver must be an array-like object with int payload"); \
        }                                                                                                              \
    } while (0)
#define EXECUTE_SUPER_ARRAY_FILL_INT4_CONST_BODY()                                                                     \
    do {                                                                                                               \
        const SZrTypeValue *countValue = &BASE(B1(instruction))->value;                                                \
        const SZrTypeValue *constantValue = CONST(E(instruction));                                                     \
        ZR_ASSERT(countValue != ZR_NULL);                                                                              \
        ZR_ASSERT(constantValue != ZR_NULL);                                                                           \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(countValue->type));                                                      \
        ZR_ASSERT(ZR_VALUE_IS_TYPE_SIGNED_INT(constantValue->type));                                                   \
        if (ZR_UNLIKELY(!ZrCore_Object_SuperArrayFillInt4ConstAssumeFast(                                              \
                    state,                                                                                              \
                    BASE(A1(instruction)),                                                                              \
                    countValue->value.nativeObject.nativeInt64,                                                         \
                    constantValue->value.nativeObject.nativeInt64))) {                                                  \
            ZrCore_Debug_RunError(state,                                                                               \
                                  "SUPER_ARRAY_FILL_INT4_CONST: receiver must be an array-like object with int repeat count and int payload"); \
        }                                                                                                              \
    } while (0)

#define UPDATE_TRAP(CALL_INFO)                                                                                         \
    do {                                                                                                               \
        trap = (CALL_INFO)->context.context.trap;                                                                      \
        UPDATE_FAST_DISPATCH_MODE();                                                                                   \
    } while (0)
#define UPDATE_TRAP_FAST(CALL_INFO)                                                                                    \
    do {                                                                                                               \
        trap = (CALL_INFO)->context.context.trap;                                                                      \
        if (ZR_UNLIKELY(trap != ZR_DEBUG_SIGNAL_NONE)) {                                                               \
            fastDispatchMode = ZR_FALSE;                                                                               \
        }                                                                                                              \
    } while (0)
#define UPDATE_BASE(CALL_INFO) (base = (CALL_INFO)->functionBase.valuePointer + 1)
#define UPDATE_STACK(CALL_INFO)                                                                                        \
    {                                                                                                                  \
        UPDATE_BASE(CALL_INFO);                                                                                        \
    }
#define SAVE_PC(STATE, CALL_INFO) ((CALL_INFO)->context.context.programCounter = programCounter)
#define SAVE_STATE(STATE, CALL_INFO)                                                                                   \
    (SAVE_PC(STATE, CALL_INFO), ((STATE)->stackTop.valuePointer = (CALL_INFO)->functionTop.valuePointer))
    // MODIFIABLE: ERROR & STACK & HOOK
#define PROTECT_ESH(STATE, CALL_INFO, EXP)                                                                             \
    do {                                                                                                               \
        ZrCore_Profile_RecordSlowPathCurrent(ZR_PROFILE_SLOWPATH_PROTECT_ESH);                                        \
        SAVE_PC(STATE, CALL_INFO);                                                                                     \
        (STATE)->stackTop.valuePointer = (CALL_INFO)->functionTop.valuePointer;                                        \
        EXP;                                                                                                           \
        UPDATE_BASE(CALL_INFO);                                                                                        \
        UPDATE_TRAP(CALL_INFO);                                                                                        \
    } while (0)
#define PROTECT_EH(STATE, CALL_INFO, EXP)                                                                              \
    do {                                                                                                               \
        ZrCore_Profile_RecordSlowPathCurrent(ZR_PROFILE_SLOWPATH_PROTECT_EH);                                         \
        SAVE_PC(STATE, CALL_INFO);                                                                                     \
        EXP;                                                                                                           \
        UPDATE_BASE(CALL_INFO);                                                                                        \
        UPDATE_TRAP(CALL_INFO);                                                                                        \
    } while (0)
#define PROTECT_E(STATE, CALL_INFO, EXP)                                                                               \
    do {                                                                                                               \
        ZrCore_Profile_RecordSlowPathCurrent(ZR_PROFILE_SLOWPATH_PROTECT_E);                                          \
        SAVE_PC(STATE, CALL_INFO);                                                                                     \
        (STATE)->stackTop.valuePointer = (CALL_INFO)->functionTop.valuePointer;                                        \
        EXP;                                                                                                           \
        UPDATE_BASE(CALL_INFO);                                                                                        \
    } while (0)

#define RELOAD_DESTINATION_AFTER_PROTECT(CALL_INFO, INSTRUCTION)                                                       \
    do {                                                                                                               \
        UPDATE_BASE(CALL_INFO);                                                                                        \
        destination = E(INSTRUCTION) == ZR_INSTRUCTION_USE_RET_FLAG ? &ret : &BASE(E(INSTRUCTION))->value;           \
    } while (0)

#define ZrCore_Debug_RunError(STATE, ...)                                                                              \
    do {                                                                                                               \
        SAVE_PC((STATE), callInfo);                                                                                    \
        if (execution_raise_vm_runtime_error((STATE), &callInfo, __VA_ARGS__)) {                                      \
            goto LZrReturning;                                                                                         \
        }                                                                                                              \
    } while (0)

#define RESUME_AFTER_NATIVE_CALL(STATE, CALL_INFO)                                                                     \
    do {                                                                                                               \
        if ((STATE)->hasCurrentException && execution_unwind_exception_to_handler((STATE), &(CALL_INFO))) {           \
            goto LZrReturning;                                                                                         \
        }                                                                                                              \
        if ((CALL_INFO) != ZR_NULL &&                                                                                  \
            (((STATE)->stackTop.valuePointer < (CALL_INFO)->functionBase.valuePointer + 1) ||                         \
             ((STATE)->stackTop.valuePointer > (CALL_INFO)->functionTop.valuePointer))) {                             \
            (STATE)->stackTop.valuePointer = (CALL_INFO)->functionTop.valuePointer;                                    \
        }                                                                                                              \
        UPDATE_BASE(CALL_INFO);                                                                                        \
        UPDATE_TRAP(CALL_INFO);                                                                                        \
    } while (0)

#define JUMP(CALL_INFO, INSTRUCTION, OFFSET)                                                                           \
    {                                                                                                                  \
        programCounter += A2(INSTRUCTION) + (OFFSET);                                                                  \
        UPDATE_TRAP(CALL_INFO);                                                                                        \
    }
#define JUMP_FAST(CALL_INFO, INSTRUCTION, OFFSET)                                                                      \
    {                                                                                                                  \
        programCounter += A2(INSTRUCTION) + (OFFSET);                                                                  \
        UPDATE_TRAP_FAST(CALL_INFO);                                                                                   \
    }
#define JUMP1_16(CALL_INFO, INSTRUCTION, OFFSET)                                                                       \
    {                                                                                                                  \
        programCounter += (TZrInt16)B1(INSTRUCTION) + (OFFSET);                                                        \
        UPDATE_TRAP(CALL_INFO);                                                                                        \
    }
#define JUMP1_16_FAST(CALL_INFO, INSTRUCTION, OFFSET)                                                                  \
    {                                                                                                                  \
        programCounter += (TZrInt16)B1(INSTRUCTION) + (OFFSET);                                                        \
        UPDATE_TRAP_FAST(CALL_INFO);                                                                                   \
    }

LZrStart:
    trap = state->debugHookSignal;
    profileRuntime = (state != ZR_NULL && state->global != ZR_NULL) ? state->global->profileRuntime : ZR_NULL;
    recordInstructions = (profileRuntime != ZR_NULL && profileRuntime->recordInstructions) ? ZR_TRUE : ZR_FALSE;
    recordHelpers = (profileRuntime != ZR_NULL && profileRuntime->recordHelpers) ? ZR_TRUE : ZR_FALSE;
    UPDATE_FAST_DISPATCH_MODE();
LZrReturning: {
        SZrTypeValue *functionBaseValue = ZrCore_Stack_GetValueNoProfile(callInfo->functionBase.valuePointer);
        SZrFunction *function = execution_try_resolve_vm_metadata_function_fast(state,
                                                                                functionBaseValue,
                                                                                &frameCallableObject);

        frameFunctionBase = callInfo->functionBase.valuePointer;
        if (function == ZR_NULL || frameCallableObject == ZR_NULL) {
            ZrCore_Debug_RunError(state, "invalid VM frame callable metadata");
        }
        frameFunction = function;
        closure = (functionBaseValue != ZR_NULL &&
                   functionBaseValue->type == ZR_VALUE_TYPE_CLOSURE &&
                   !functionBaseValue->isNative)
                          ? ZR_CAST_VM_CLOSURE(state, functionBaseValue->value.object)
                          : ZR_NULL;
        constants = function->constantValueList;
        instructionsEnd = function->instructionsList + function->instructionsLength;
        instructionsEndFast1 = instructionsEnd - 1;
        programCounter = callInfo->context.context.programCounter - 1;
        base = callInfo->functionBase.valuePointer + 1;
        profilePreviousProgramCounter = ZR_NULL;
        profilePreviousFrameFunction = ZR_NULL;
        UPDATE_FAST_DISPATCH_MODE();
}
    if (ZR_UNLIKELY(trap != ZR_DEBUG_SIGNAL_NONE)) {
        // todo
    }
    for (;;) {
        TZrStackValuePointer currentFunctionBase = ZR_NULL;
        SZrTypeValue *currentFunctionBaseValue = ZR_NULL;
        SZrRawObject *currentCallableObject = ZR_NULL;
        SZrFunction *currentFunction = frameFunction;

        TZrInstruction instruction;
        SZrTypeValue *destination = &ret;
        /*
         * fetch instruction
         */
        if (callInfo != ZR_NULL && callInfo->functionBase.valuePointer != ZR_NULL) {
            currentFunctionBase = callInfo->functionBase.valuePointer;
            currentFunctionBaseValue = ZrCore_Stack_GetValueNoProfile(currentFunctionBase);
            if (currentFunctionBaseValue != ZR_NULL) {
                currentCallableObject = currentFunctionBaseValue->value.object;
            }
            if (currentFunctionBase != frameFunctionBase || currentCallableObject != frameCallableObject) {
                currentFunction = execution_try_resolve_vm_metadata_function_fast(state,
                                                                                 currentFunctionBaseValue,
                                                                                 &currentCallableObject);
                if (currentFunction == ZR_NULL || currentCallableObject == ZR_NULL) {
                    ZrCore_Debug_RunError(state, "invalid VM frame callable metadata");
                    break;
                }
                frameFunctionBase = currentFunctionBase;
                frameCallableObject = currentCallableObject;
                frameFunction = currentFunction;
                closure = (currentFunctionBaseValue->type == ZR_VALUE_TYPE_CLOSURE &&
                           !currentFunctionBaseValue->isNative)
                                  ? ZR_CAST_VM_CLOSURE(state, currentCallableObject)
                                  : ZR_NULL;
                constants = currentFunction->constantValueList;
                instructionsEnd = currentFunction->instructionsList + currentFunction->instructionsLength;
                instructionsEndFast1 = instructionsEnd - 1;
                programCounter = callInfo->context.context.programCounter - 1;
                base = currentFunctionBase + 1;
                profilePreviousProgramCounter = ZR_NULL;
                profilePreviousFrameFunction = ZR_NULL;
            }
        }
        FETCH_PREPARE_OR_BREAK(1);

#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
        if (ZR_LIKELY(fastDispatchMode)) {
            ZR_FAST_INSTRUCTION_DISPATCH(instruction)
        }
LZrFastInstruction_FALLBACK:
        FAST_PREPARE_DESTINATION();
LZrFastInstruction_FALLBACK_NO_DESTINATION:
#endif
        ZR_INSTRUCTION_DISPATCH(instruction) {
            ZR_INSTRUCTION_LABEL(NOP) {
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_GET_STACK: {
                EXECUTE_GET_STACK_BODY_FAST();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(GET_STACK) {
                EXECUTE_GET_STACK_BODY();
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_SET_STACK: {
                EXECUTE_SET_STACK_BODY_FAST();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(SET_STACK) {
                EXECUTE_SET_STACK_BODY();
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_GET_CONSTANT: {
                EXECUTE_GET_CONSTANT_BODY_FAST();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(GET_CONSTANT) {
                EXECUTE_GET_CONSTANT_BODY();
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_RESET_STACK_NULL: {
                EXECUTE_RESET_STACK_NULL_BODY();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(RESET_STACK_NULL) {
                EXECUTE_RESET_STACK_NULL_BODY();
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SET_CONSTANT) {
                // ZR_ASSERT(ZR_VALUE_IS_TYPE_UNSIGNED_INT(A2(instruction)));
                //*CONST(ret.value.nativeObject.nativeUInt64) = BASE(B1(instruction))->value;
                *CONST(A2(instruction)) = *destination;
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(GET_CLOSURE) {
                // ZR_ASSERT(ZR_VALUE_IS_TYPE_UNSIGNED_INT(A2(instruction)));
                // closure function to access
                ZrCore_Value_Copy(state, destination, ZrCore_ClosureValue_GetValue(CLOSURE(A2(instruction))));
                // BASE(B1(instruction))->value = CLOSURE(ret.value.nativeObject.nativeUInt64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SET_CLOSURE) {
                // ZR_ASSERT(ZR_VALUE_IS_TYPE_UNSIGNED_INT(A2(instruction)));
                SZrClosureValue *closureValue = CLOSURE(A2(instruction));
                SZrTypeValue *value = ZrCore_ClosureValue_GetValue(closureValue);
                SZrTypeValue *newValue = destination;
                // closure function to access
                ZrCore_Value_Copy(state, value, newValue);
                // CLOSURE(ret.value.nativeObject.nativeUInt64) = BASE(B1(instruction))->value;
                ZrCore_Value_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closureValue), newValue);
                // *CLOSURE(ret.value.nativeObject.nativeUInt64) = BASE(B1(instruction))->value;
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TO_BOOL) {
                opA = &BASE(A1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_NULL(opA->type)) {
                    ZR_VALUE_FAST_SET(destination, nativeBool, ZR_FALSE, ZR_VALUE_TYPE_BOOL);
                } else if (ZR_VALUE_IS_TYPE_BOOL(opA->type)) {
                    *destination = *opA;
                } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(opA->type)) {
                    ZR_VALUE_FAST_SET(destination,
                                      nativeBool,
                                      opA->value.nativeObject.nativeInt64 != 0,
                                      ZR_VALUE_TYPE_BOOL);
                } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(opA->type)) {
                    ZR_VALUE_FAST_SET(destination,
                                      nativeBool,
                                      opA->value.nativeObject.nativeUInt64 != 0,
                                      ZR_VALUE_TYPE_BOOL);
                } else if (ZR_VALUE_IS_TYPE_FLOAT(opA->type)) {
                    ZR_VALUE_FAST_SET(destination,
                                      nativeBool,
                                      opA->value.nativeObject.nativeDouble != 0.0,
                                      ZR_VALUE_TYPE_BOOL);
                } else {
                    SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_TO_BOOL);
                    if (meta != ZR_NULL && meta->function != ZR_NULL) {
                        // 调用元方法
                        TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                        SZrCallInfo *savedCallInfo = state->callInfoList;
                        PROTECT_E(state, callInfo, {
                            TZrStackValuePointer metaBase = ZR_NULL;
                            TZrStackValuePointer restoredStackTop = savedStackTop;
                            TZrBool metaCallSucceeded = execution_invoke_meta_call(state,
                                                                                  savedCallInfo,
                                                                                  savedStackTop,
                                                                                  callInfo->functionTop.valuePointer,
                                                                                  meta,
                                                                                  opA,
                                                                                  ZR_NULL,
                                                                                  ZR_META_CALL_UNARY_ARGUMENT_COUNT,
                                                                                  &metaBase,
                                                                                  &restoredStackTop);
                            RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                            if (metaCallSucceeded) {
                                SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                                if (returnValue->type == ZR_VALUE_TYPE_BOOL) {
                                    ZrCore_Value_Copy(state, destination, returnValue);
                                } else {
                                    // 元方法返回类型错误，使用默认转换
                                    ZR_VALUE_FAST_SET(destination, nativeBool, ZR_TRUE, ZR_VALUE_TYPE_BOOL);
                                }
                            } else {
                                // 调用失败，使用默认转换
                                ZR_VALUE_FAST_SET(destination, nativeBool, ZR_TRUE, ZR_VALUE_TYPE_BOOL);
                            }
                            state->stackTop.valuePointer = restoredStackTop;
                            state->callInfoList = savedCallInfo;
                        });
                    } else {
                        // 无元方法，使用内置转换逻辑
                        if (ZR_VALUE_IS_TYPE_STRING(opA->type)) {
                            SZrString *str = ZR_CAST_STRING(state, opA->value.object);
                            TZrSize len = (str->shortStringLength < ZR_VM_LONG_STRING_FLAG) ? str->shortStringLength : str->longStringLength;
                            ZR_VALUE_FAST_SET(destination, nativeBool, len > 0, ZR_VALUE_TYPE_BOOL);
                        } else {
                            // 对象类型，默认返回 true
                            ZR_VALUE_FAST_SET(destination, nativeBool, ZR_TRUE, ZR_VALUE_TYPE_BOOL);
                        }
                    }
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TO_INT) {
                opA = &BASE(A1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_SIGNED_INT(opA->type)) {
                    *destination = *opA;
                } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(opA->type)) {
                    ZrCore_Value_InitAsInt(state, destination, (TZrInt64)opA->value.nativeObject.nativeUInt64);
                } else {
                    SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_TO_INT);
                    if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        TZrStackValuePointer metaBase = ZR_NULL;
                        TZrStackValuePointer restoredStackTop = savedStackTop;
                        TZrBool metaCallSucceeded = execution_invoke_meta_call(state,
                                                                              savedCallInfo,
                                                                              savedStackTop,
                                                                              callInfo->functionTop.valuePointer,
                                                                              meta,
                                                                              opA,
                                                                              ZR_NULL,
                                                                              ZR_META_CALL_UNARY_ARGUMENT_COUNT,
                                                                              &metaBase,
                                                                              &restoredStackTop);
                        RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                        if (metaCallSucceeded) {
                            SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                            if (ZR_VALUE_IS_TYPE_INT(returnValue->type)) {
                                ZrCore_Value_Copy(state, destination, returnValue);
                            } else {
                                // 元方法返回类型错误，使用默认转换
                                ZrCore_Value_InitAsInt(state, destination, 0);
                            }
                        } else {
                            // 调用失败，使用默认转换
                            ZrCore_Value_InitAsInt(state, destination, 0);
                        }
                        state->stackTop.valuePointer = restoredStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                    } else {
                    // 无元方法，使用内置转换逻辑
                    if (ZR_VALUE_IS_TYPE_FLOAT(opA->type)) {
                        ZrCore_Value_InitAsInt(state, destination, (TZrInt64) opA->value.nativeObject.nativeDouble);
                    } else if (ZR_VALUE_IS_TYPE_BOOL(opA->type)) {
                        ZrCore_Value_InitAsInt(state, destination, opA->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE);
                    } else {
                        // 其他类型无法转换，返回 0
                        ZrCore_Value_InitAsInt(state, destination, 0);
                    }
                    }
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TO_UINT) {
                opA = &BASE(A1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(opA->type)) {
                    *destination = *opA;
                } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(opA->type)) {
                    ZrCore_Value_InitAsUInt(state, destination, (TZrUInt64)opA->value.nativeObject.nativeInt64);
                } else {
                    SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_TO_UINT);
                    if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        TZrStackValuePointer metaBase = ZR_NULL;
                        TZrStackValuePointer restoredStackTop = savedStackTop;
                        TZrBool metaCallSucceeded = execution_invoke_meta_call(state,
                                                                              savedCallInfo,
                                                                              savedStackTop,
                                                                              callInfo->functionTop.valuePointer,
                                                                              meta,
                                                                              opA,
                                                                              ZR_NULL,
                                                                              ZR_META_CALL_UNARY_ARGUMENT_COUNT,
                                                                              &metaBase,
                                                                              &restoredStackTop);
                        RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                        if (metaCallSucceeded) {
                            SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                            if (ZR_VALUE_IS_TYPE_INT(returnValue->type)) {
                                ZrCore_Value_Copy(state, destination, returnValue);
                            } else {
                                // 元方法返回类型错误，使用默认转换
                                ZrCore_Value_InitAsUInt(state, destination, 0);
                            }
                        } else {
                            // 调用失败，使用默认转换
                            ZrCore_Value_InitAsUInt(state, destination, 0);
                        }
                        state->stackTop.valuePointer = restoredStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                    } else {
                    // 无元方法，使用内置转换逻辑
                    if (ZR_VALUE_IS_TYPE_FLOAT(opA->type)) {
                        ZrCore_Value_InitAsUInt(state, destination, (TZrUInt64) opA->value.nativeObject.nativeDouble);
                    } else if (ZR_VALUE_IS_TYPE_BOOL(opA->type)) {
                        ZrCore_Value_InitAsUInt(state, destination, opA->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE);
                    } else {
                        // 其他类型无法转换，返回 0
                        ZrCore_Value_InitAsUInt(state, destination, 0);
                    }
                    }
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TO_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_FLOAT(opA->type)) {
                    *destination = *opA;
                } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(opA->type)) {
                    ZrCore_Value_InitAsFloat(state, destination, (TZrFloat64)opA->value.nativeObject.nativeInt64);
                } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(opA->type)) {
                    ZrCore_Value_InitAsFloat(state, destination, (TZrFloat64)opA->value.nativeObject.nativeUInt64);
                } else {
                    SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_TO_FLOAT);
                    if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        TZrStackValuePointer metaBase = ZR_NULL;
                        TZrStackValuePointer restoredStackTop = savedStackTop;
                        TZrBool metaCallSucceeded = execution_invoke_meta_call(state,
                                                                              savedCallInfo,
                                                                              savedStackTop,
                                                                              callInfo->functionTop.valuePointer,
                                                                              meta,
                                                                              opA,
                                                                              ZR_NULL,
                                                                              ZR_META_CALL_UNARY_ARGUMENT_COUNT,
                                                                              &metaBase,
                                                                              &restoredStackTop);
                        RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                        if (metaCallSucceeded) {
                            SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                            if (ZR_VALUE_IS_TYPE_FLOAT(returnValue->type)) {
                                ZrCore_Value_Copy(state, destination, returnValue);
                            } else {
                                // 元方法返回类型错误，使用默认转换
                                ZrCore_Value_InitAsFloat(state, destination, 0.0);
                            }
                        } else {
                            // 调用失败，使用默认转换
                            ZrCore_Value_InitAsFloat(state, destination, 0.0);
                        }
                        state->stackTop.valuePointer = restoredStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                    } else {
                    // 无元方法，使用内置转换逻辑
                    if (ZR_VALUE_IS_TYPE_BOOL(opA->type)) {
                        ZrCore_Value_InitAsFloat(state, destination, opA->value.nativeObject.nativeBool ? (TZrFloat64)ZR_TRUE : (TZrFloat64)ZR_FALSE);
                    } else {
                        // 其他类型无法转换，返回 0.0
                        ZrCore_Value_InitAsFloat(state, destination, 0.0);
                    }
                    }
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TO_STRING) {
                SZrString *resultString;

                opA = &BASE(A1(instruction))->value;
                resultString = ZrCore_Value_ConvertToString(state, opA);
                UPDATE_BASE(callInfo);
                destination = E(instruction) == ZR_INSTRUCTION_USE_RET_FLAG ? &ret : &BASE(E(instruction))->value;
                if (resultString != ZR_NULL) {
                    ZrCore_Value_InitAsRawObject(state, destination, ZR_CAST_RAW_OBJECT_AS_SUPER(resultString));
                    destination->type = ZR_VALUE_TYPE_STRING;
                } else {
                    // 转换失败，返回 null
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TO_STRUCT) {
                opA = &BASE(A1(instruction))->value;
                // operand1[1] 存储类型名称常量索引
                TZrUInt16 typeNameConstantIndex = B1(instruction);
                if (currentFunction != ZR_NULL && typeNameConstantIndex < currentFunction->constantValueLength) {
                    SZrTypeValue *typeNameValue = CONST(typeNameConstantIndex);
                    if (typeNameValue->type == ZR_VALUE_TYPE_STRING) {
                        SZrString *typeName = ZR_CAST_STRING(state, typeNameValue->value.object);
                        
                        // 1. 优先尝试通过元方法进行转换
                        // 如果定义了 ZR_META_TO_STRUCT，使用它；否则直接查找原型
                        // 注意：ZR_META_TO_STRUCT 可能不在标准元方法列表中，需要检查是否支持
                        // 如果opA是对象类型，尝试从对象的prototype中查找元方法
                        SZrMeta *meta = ZR_NULL;
                        if (opA->type == ZR_VALUE_TYPE_OBJECT) {
                            SZrObject *obj = ZR_CAST_OBJECT(state, opA->value.object);
                            if (obj != ZR_NULL && obj->prototype != ZR_NULL) {
                                // 尝试查找TO_STRUCT元方法（如果存在）
                                // 注意：ZR_META_TO_STRUCT可能不在标准枚举中，这里先查找原型链
                                // 如果未来添加了ZR_META_TO_STRUCT，可以在这里使用
                                // TODO: 目前暂时跳过元方法查找，直接查找原型
                            }
                        }
                        if (meta != ZR_NULL && meta->function != ZR_NULL) {
                            // 调用元方法
                            TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                            SZrCallInfo *savedCallInfo = state->callInfoList;
                            PROTECT_E(state, callInfo, {
                                TZrStackValuePointer metaBase = ZR_NULL;
                                TZrStackValuePointer restoredStackTop = savedStackTop;
                                TZrBool metaCallSucceeded = execution_invoke_meta_call(state,
                                                                                      savedCallInfo,
                                                                                      savedStackTop,
                                                                                      savedStackTop,
                                                                                      meta,
                                                                                      opA,
                                                                                      typeNameValue,
                                                                                      ZR_META_CALL_MAX_ARGUMENTS,
                                                                                      &metaBase,
                                                                                      &restoredStackTop);
                                RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                                if (metaCallSucceeded) {
                                    SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                                    ZrCore_Value_Copy(state, destination, returnValue);
                                } else {
                                    ZrCore_Value_ResetAsNull(destination);
                                }
                                state->stackTop.valuePointer = restoredStackTop;
                                state->callInfoList = savedCallInfo;
                            });
                        } else {
                            // 2. 无元方法，尝试查找类型原型并执行转换
                            SZrObjectPrototype *prototype = find_type_prototype(state, typeName, ZR_OBJECT_PROTOTYPE_TYPE_STRUCT);
                            if (prototype != ZR_NULL) {
                                // 找到原型，执行转换
                                if (!convert_to_struct(state, opA, prototype, destination)) {
                                    ZrCore_Value_ResetAsNull(destination);
                                }
                            } else {
                                // 3. 找不到原型，如果源值是对象，尝试直接复制
                                // 这允许在运行时动态创建 struct（如果类型系统支持）
                                if (ZR_VALUE_IS_TYPE_OBJECT(opA->type)) {
                                    // TODO: 暂时直接复制对象（后续需要设置正确的原型）
                                    ZrCore_Value_Copy(state, destination, opA);
                                } else {
                                    ZrCore_Value_ResetAsNull(destination);
                                }
                            }
                        }
                    } else {
                        // 类型名称常量类型错误
                        ZrCore_Value_ResetAsNull(destination);
                    }
                } else {
                    // 常量索引越界
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TO_OBJECT) {
                opA = &BASE(A1(instruction))->value;
                // operand1[1] 存储类型名称常量索引
                TZrUInt16 typeNameConstantIndex = B1(instruction);
                if (currentFunction != ZR_NULL && typeNameConstantIndex < currentFunction->constantValueLength) {
                    SZrTypeValue *typeNameValue = CONST(typeNameConstantIndex);
                    if (typeNameValue->type == ZR_VALUE_TYPE_STRING) {
                        SZrString *typeName = ZR_CAST_STRING(state, typeNameValue->value.object);
                        
                        // 1. 优先尝试通过元方法进行转换
                        // 如果定义了 ZR_META_TO_OBJECT，使用它；否则直接查找原型
                        // 注意：ZR_META_TO_OBJECT 可能不在标准元方法列表中，需要检查是否支持
                        // 如果opA是对象类型，尝试从对象的prototype中查找元方法
                        SZrMeta *meta = ZR_NULL;
                        if (opA->type == ZR_VALUE_TYPE_OBJECT) {
                            SZrObject *obj = ZR_CAST_OBJECT(state, opA->value.object);
                            if (obj != ZR_NULL && obj->prototype != ZR_NULL) {
                                // 尝试查找TO_OBJECT元方法（如果存在）
                                // 注意：ZR_META_TO_OBJECT可能不在标准枚举中，这里先查找原型链
                                // 如果未来添加了ZR_META_TO_OBJECT，可以在这里使用
                                // TODO: 目前暂时跳过元方法查找，直接查找原型
                            }
                        }
                        if (meta != ZR_NULL && meta->function != ZR_NULL) {
                            // 调用元方法
                            TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                            SZrCallInfo *savedCallInfo = state->callInfoList;
                            PROTECT_E(state, callInfo, {
                                TZrStackValuePointer metaBase = ZR_NULL;
                                TZrStackValuePointer restoredStackTop = savedStackTop;
                                TZrBool metaCallSucceeded = execution_invoke_meta_call(state,
                                                                                      savedCallInfo,
                                                                                      savedStackTop,
                                                                                      savedStackTop,
                                                                                      meta,
                                                                                      opA,
                                                                                      typeNameValue,
                                                                                      ZR_META_CALL_MAX_ARGUMENTS,
                                                                                      &metaBase,
                                                                                      &restoredStackTop);
                                RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                                if (metaCallSucceeded) {
                                    SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                                    ZrCore_Value_Copy(state, destination, returnValue);
                                } else {
                                    ZrCore_Value_ResetAsNull(destination);
                                }
                                state->stackTop.valuePointer = restoredStackTop;
                                state->callInfoList = savedCallInfo;
                            });
                        } else {
                            // 2. 无元方法，尝试查找类型原型并执行转换
                            SZrObjectPrototype *prototype = find_type_prototype(state, typeName, ZR_OBJECT_PROTOTYPE_TYPE_INVALID);
                            if (prototype != ZR_NULL) {
                                // 找到原型，执行转换
                                TZrBool converted = ZR_FALSE;

                                if (prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_CLASS) {
                                    converted = convert_to_class(state, opA, prototype, destination);
                                } else if (prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_ENUM) {
                                    converted = convert_to_enum(state, opA, prototype, destination);
                                }

                                if (!converted) {
                                    ZrCore_Value_ResetAsNull(destination);
                                }
                            } else {
                                // 3. 找不到原型，如果源值是对象，尝试直接复制
                                // 这允许在运行时动态创建 class（如果类型系统支持）
                                if (ZR_VALUE_IS_TYPE_OBJECT(opA->type)) {
                                    // TODO: 暂时直接复制对象（后续需要设置正确的原型）
                                    ZrCore_Value_Copy(state, destination, opA);
                                } else {
                                    ZrCore_Value_ResetAsNull(destination);
                                }
                            }
                        }
                    } else {
                        // 类型名称常量类型错误
                        ZrCore_Value_ResetAsNull(destination);
                    }
                } else {
                    // 常量索引越界
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(ADD) {
                SZrTypeValue builtinResult;
                TZrBool builtinNeedsTemporaryResult;

                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if (execution_try_builtin_add_exact_numeric_fast(destination, opA, opB)) {
                    // 精确数值对直接命中内联 fast path。
                } else {
                    builtinNeedsTemporaryResult = execution_builtin_add_requires_temporary_result(opA, opB);
                    if (destination == &ret || !builtinNeedsTemporaryResult) {
                        if (try_builtin_add(state, destination, opA, opB)) {
                            // 基础数值和字符串拼接直接在运行时处理。
                        } else {
                            SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_ADD);
                            if (meta != ZR_NULL && meta->function != ZR_NULL) {
                            // 调用元方法
                                TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                                SZrCallInfo *savedCallInfo = state->callInfoList;
                                PROTECT_E(state, callInfo, {
                                    TZrStackValuePointer metaBase = ZR_NULL;
                                    TZrStackValuePointer restoredStackTop = savedStackTop;
                                    TZrBool metaCallSucceeded = execution_invoke_meta_call(state,
                                                                                          savedCallInfo,
                                                                                          savedStackTop,
                                                                                          savedStackTop,
                                                                                          meta,
                                                                                          opA,
                                                                                          opB,
                                                                                          ZR_META_CALL_MAX_ARGUMENTS,
                                                                                          &metaBase,
                                                                                          &restoredStackTop);
                                    RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                                    if (metaCallSucceeded) {
                                        SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                                        ZrCore_Value_Copy(state, destination, returnValue);
                                    } else {
                                        ZrCore_Value_ResetAsNull(destination);
                                    }
                                    state->stackTop.valuePointer = restoredStackTop;
                                    state->callInfoList = savedCallInfo;
                                });
                            } else {
                                // 无元方法，返回 null
                                ZrCore_Value_ResetAsNull(destination);
                            }
                        }
                    } else {
                        ZrCore_Value_ResetAsNull(&builtinResult);
                        if (try_builtin_add(state, &builtinResult, opA, opB)) {
                            UPDATE_BASE(callInfo);
                            destination = &BASE(E(instruction))->value;
                            ZrCore_Value_Copy(state, destination, &builtinResult);
                            // 基础数值和字符串拼接直接在运行时处理。
                        } else {
                            SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_ADD);
                            if (meta != ZR_NULL && meta->function != ZR_NULL) {
                            // 调用元方法
                                TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                                SZrCallInfo *savedCallInfo = state->callInfoList;
                                PROTECT_E(state, callInfo, {
                                    TZrStackValuePointer metaBase = ZR_NULL;
                                    TZrStackValuePointer restoredStackTop = savedStackTop;
                                    TZrBool metaCallSucceeded = execution_invoke_meta_call(state,
                                                                                          savedCallInfo,
                                                                                          savedStackTop,
                                                                                          savedStackTop,
                                                                                          meta,
                                                                                          opA,
                                                                                          opB,
                                                                                          ZR_META_CALL_MAX_ARGUMENTS,
                                                                                          &metaBase,
                                                                                          &restoredStackTop);
                                    UPDATE_BASE(callInfo);
                                    destination = &BASE(E(instruction))->value;
                                    if (metaCallSucceeded) {
                                        SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                                        ZrCore_Value_Copy(state, destination, returnValue);
                                    } else {
                                        ZrCore_Value_ResetAsNull(destination);
                                    }
                                    state->stackTop.valuePointer = restoredStackTop;
                                    state->callInfoList = savedCallInfo;
                                });
                            } else {
                                // 无元方法，返回 null
                                UPDATE_BASE(callInfo);
                                destination = &BASE(E(instruction))->value;
                                ZrCore_Value_ResetAsNull(destination);
                            }
                        }
                    }
                }
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_ADD_INT: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_ADD_INT_BODY();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(ADD_INT) {
                EXECUTE_ADD_INT_BODY();
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_ADD_INT_PLAIN_DEST: {
                EXECUTE_ADD_INT_BODY_PLAIN_DEST();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(ADD_INT_PLAIN_DEST) {
                EXECUTE_ADD_INT_BODY_PLAIN_DEST();
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_ADD_INT_CONST: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_ADD_INT_CONST_BODY();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(ADD_INT_CONST) {
                EXECUTE_ADD_INT_CONST_BODY();
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_ADD_INT_CONST_PLAIN_DEST: {
                EXECUTE_ADD_INT_CONST_BODY_PLAIN_DEST();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(ADD_INT_CONST_PLAIN_DEST) {
                EXECUTE_ADD_INT_CONST_BODY_PLAIN_DEST();
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_ADD_SIGNED: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_TYPED_SIGNED_BINARY_BODY(+);
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(ADD_SIGNED) {
                EXECUTE_TYPED_SIGNED_BINARY_BODY(+);
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_ADD_SIGNED_PLAIN_DEST: {
                EXECUTE_TYPED_SIGNED_BINARY_BODY_PLAIN_DEST(+);
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(ADD_SIGNED_PLAIN_DEST) {
                EXECUTE_TYPED_SIGNED_BINARY_BODY_PLAIN_DEST(+);
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_ADD_SIGNED_CONST: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_TYPED_SIGNED_CONST_BINARY_BODY(+);
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(ADD_SIGNED_CONST) {
                EXECUTE_TYPED_SIGNED_CONST_BINARY_BODY(+);
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_ADD_SIGNED_CONST_PLAIN_DEST: {
                EXECUTE_TYPED_SIGNED_CONST_BINARY_BODY_PLAIN_DEST(+);
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(ADD_SIGNED_CONST_PLAIN_DEST) {
                EXECUTE_TYPED_SIGNED_CONST_BINARY_BODY_PLAIN_DEST(+);
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_ADD_SIGNED_LOAD_CONST: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_ADD_SIGNED_LOAD_CONST_BODY();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(ADD_SIGNED_LOAD_CONST) {
                EXECUTE_ADD_SIGNED_LOAD_CONST_BODY();
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_ADD_SIGNED_LOAD_STACK_CONST: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_ADD_SIGNED_LOAD_STACK_CONST_BODY();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(ADD_SIGNED_LOAD_STACK_CONST) {
                EXECUTE_ADD_SIGNED_LOAD_STACK_CONST_BODY();
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_ADD_SIGNED_LOAD_STACK: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_ADD_SIGNED_LOAD_STACK_BODY();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(ADD_SIGNED_LOAD_STACK) {
                EXECUTE_ADD_SIGNED_LOAD_STACK_BODY();
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_ADD_SIGNED_LOAD_STACK_LOAD_CONST: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_ADD_SIGNED_LOAD_STACK_LOAD_CONST_BODY();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(ADD_SIGNED_LOAD_STACK_LOAD_CONST) {
                EXECUTE_ADD_SIGNED_LOAD_STACK_LOAD_CONST_BODY();
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_ADD_UNSIGNED: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_TYPED_UNSIGNED_BINARY_BODY(+);
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(ADD_UNSIGNED) {
                EXECUTE_TYPED_UNSIGNED_BINARY_BODY(+);
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_ADD_UNSIGNED_PLAIN_DEST: {
                EXECUTE_TYPED_UNSIGNED_BINARY_BODY_PLAIN_DEST(+);
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(ADD_UNSIGNED_PLAIN_DEST) {
                EXECUTE_TYPED_UNSIGNED_BINARY_BODY_PLAIN_DEST(+);
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_ADD_UNSIGNED_CONST: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_TYPED_UNSIGNED_CONST_BINARY_BODY(+);
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(ADD_UNSIGNED_CONST) {
                EXECUTE_TYPED_UNSIGNED_CONST_BINARY_BODY(+);
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_ADD_UNSIGNED_CONST_PLAIN_DEST: {
                EXECUTE_TYPED_UNSIGNED_CONST_BINARY_BODY_PLAIN_DEST(+);
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(ADD_UNSIGNED_CONST_PLAIN_DEST) {
                EXECUTE_TYPED_UNSIGNED_CONST_BINARY_BODY_PLAIN_DEST(+);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(ADD_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                execution_apply_binary_numeric_float_or_raise(
                        state, ZR_EXEC_NUMERIC_FALLBACK_ADD, destination, opA, opB, "ADD_FLOAT");
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(ADD_STRING) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_STRING(opA->type) && ZR_VALUE_IS_TYPE_STRING(opB->type));
                if (!execution_try_concat_exact_strings(state, destination, opA, opB)) {
                    UPDATE_BASE(callInfo);
                    destination = E(instruction) == ZR_INSTRUCTION_USE_RET_FLAG ? &ret : &BASE(E(instruction))->value;
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUB) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if (!execution_try_builtin_sub(state, destination, opA, opB)) {
                    SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_SUB);
                    if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                        TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                        SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        TZrStackValuePointer metaBase = ZR_NULL;
                        TZrStackValuePointer restoredStackTop = savedStackTop;
                        TZrBool metaCallSucceeded = execution_invoke_meta_call(state,
                                                                              savedCallInfo,
                                                                              savedStackTop,
                                                                              savedStackTop,
                                                                              meta,
                                                                              opA,
                                                                              opB,
                                                                              ZR_META_CALL_MAX_ARGUMENTS,
                                                                              &metaBase,
                                                                              &restoredStackTop);
                        RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                        if (metaCallSucceeded) {
                            SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                            ZrCore_Value_Copy(state, destination, returnValue);
                        } else {
                            ZrCore_Value_ResetAsNull(destination);
                        }
                        state->stackTop.valuePointer = restoredStackTop;
                        state->callInfoList = savedCallInfo;
                        });
                    } else {
                    // 无元方法，返回 null
                        ZrCore_Value_ResetAsNull(destination);
                    }
                }
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_SUB_INT: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_SUB_INT_BODY();
            }
            DONE_FAST(1);
#endif
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_SUB_INT_PLAIN_DEST: {
                EXECUTE_SUB_INT_BODY_PLAIN_DEST();
            }
            DONE_FAST(1);
#endif
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_SUB_INT_CONST: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_SUB_INT_CONST_BODY();
            }
            DONE_FAST(1);
#endif
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_SUB_INT_CONST_PLAIN_DEST: {
                EXECUTE_SUB_INT_CONST_BODY_PLAIN_DEST();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(SUB_INT) {
                EXECUTE_SUB_INT_BODY();
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUB_INT_PLAIN_DEST) {
                EXECUTE_SUB_INT_BODY_PLAIN_DEST();
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUB_INT_CONST) {
                EXECUTE_SUB_INT_CONST_BODY();
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUB_INT_CONST_PLAIN_DEST) {
                EXECUTE_SUB_INT_CONST_BODY_PLAIN_DEST();
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_SUB_SIGNED: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_TYPED_SIGNED_BINARY_BODY(-);
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(SUB_SIGNED) {
                EXECUTE_TYPED_SIGNED_BINARY_BODY(-);
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_SUB_SIGNED_PLAIN_DEST: {
                EXECUTE_TYPED_SIGNED_BINARY_BODY_PLAIN_DEST(-);
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(SUB_SIGNED_PLAIN_DEST) {
                EXECUTE_TYPED_SIGNED_BINARY_BODY_PLAIN_DEST(-);
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_SUB_SIGNED_CONST: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_TYPED_SIGNED_CONST_BINARY_BODY(-);
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(SUB_SIGNED_CONST) {
                EXECUTE_TYPED_SIGNED_CONST_BINARY_BODY(-);
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_SUB_SIGNED_CONST_PLAIN_DEST: {
                EXECUTE_TYPED_SIGNED_CONST_BINARY_BODY_PLAIN_DEST(-);
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(SUB_SIGNED_CONST_PLAIN_DEST) {
                EXECUTE_TYPED_SIGNED_CONST_BINARY_BODY_PLAIN_DEST(-);
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_SUB_SIGNED_LOAD_CONST: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_SUB_SIGNED_LOAD_CONST_BODY();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(SUB_SIGNED_LOAD_CONST) {
                EXECUTE_SUB_SIGNED_LOAD_CONST_BODY();
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_SUB_SIGNED_LOAD_STACK_CONST: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_SUB_SIGNED_LOAD_STACK_CONST_BODY();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(SUB_SIGNED_LOAD_STACK_CONST) {
                EXECUTE_SUB_SIGNED_LOAD_STACK_CONST_BODY();
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_SUB_UNSIGNED: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_TYPED_UNSIGNED_BINARY_BODY(-);
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(SUB_UNSIGNED) {
                EXECUTE_TYPED_UNSIGNED_BINARY_BODY(-);
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_SUB_UNSIGNED_PLAIN_DEST: {
                EXECUTE_TYPED_UNSIGNED_BINARY_BODY_PLAIN_DEST(-);
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(SUB_UNSIGNED_PLAIN_DEST) {
                EXECUTE_TYPED_UNSIGNED_BINARY_BODY_PLAIN_DEST(-);
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_SUB_UNSIGNED_CONST: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_TYPED_UNSIGNED_CONST_BINARY_BODY(-);
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(SUB_UNSIGNED_CONST) {
                EXECUTE_TYPED_UNSIGNED_CONST_BINARY_BODY(-);
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_SUB_UNSIGNED_CONST_PLAIN_DEST: {
                EXECUTE_TYPED_UNSIGNED_CONST_BINARY_BODY_PLAIN_DEST(-);
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(SUB_UNSIGNED_CONST_PLAIN_DEST) {
                EXECUTE_TYPED_UNSIGNED_CONST_BINARY_BODY_PLAIN_DEST(-);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUB_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                execution_apply_binary_numeric_float_or_raise(
                        state, ZR_EXEC_NUMERIC_FALLBACK_SUB, destination, opA, opB, "SUB_FLOAT");
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MUL) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if (execution_try_builtin_mul_exact_numeric_fast(destination, opA, opB)) {
                    // Exact numeric pairs stay on the inline steady-state path.
                } else if (execution_try_builtin_mul_mixed_numeric_fast(destination, opA, opB)) {
                    // Mixed numeric integral/bool pairs stay off the meta fallback path.
                } else {
                    SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_MUL);
                    if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                        TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                        SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        TZrStackValuePointer metaBase = ZR_NULL;
                        TZrStackValuePointer restoredStackTop = savedStackTop;
                        TZrBool metaCallSucceeded = execution_invoke_meta_call(state,
                                                                              savedCallInfo,
                                                                              savedStackTop,
                                                                              savedStackTop,
                                                                              meta,
                                                                              opA,
                                                                              opB,
                                                                              ZR_META_CALL_MAX_ARGUMENTS,
                                                                              &metaBase,
                                                                              &restoredStackTop);
                        RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                        if (metaCallSucceeded) {
                            SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                            ZrCore_Value_Copy(state, destination, returnValue);
                        } else {
                            ZrCore_Value_ResetAsNull(destination);
                        }
                        state->stackTop.valuePointer = restoredStackTop;
                        state->callInfoList = savedCallInfo;
                        });
                    } else {
                    // 无元方法，返回 null
                        ZrCore_Value_ResetAsNull(destination);
                    }
                }
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_MUL_SIGNED: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_MUL_SIGNED_BODY();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(MUL_SIGNED) {
                EXECUTE_MUL_SIGNED_BODY();
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_MUL_SIGNED_PLAIN_DEST: {
                EXECUTE_MUL_SIGNED_BODY_PLAIN_DEST();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(MUL_SIGNED_PLAIN_DEST) {
                EXECUTE_MUL_SIGNED_BODY_PLAIN_DEST();
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_MUL_SIGNED_CONST: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_MUL_SIGNED_CONST_BODY();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(MUL_SIGNED_CONST) {
                EXECUTE_MUL_SIGNED_CONST_BODY();
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_MUL_SIGNED_CONST_PLAIN_DEST: {
                EXECUTE_MUL_SIGNED_CONST_BODY_PLAIN_DEST();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(MUL_SIGNED_CONST_PLAIN_DEST) {
                EXECUTE_MUL_SIGNED_CONST_BODY_PLAIN_DEST();
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_MUL_SIGNED_LOAD_CONST: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_MUL_SIGNED_LOAD_CONST_BODY();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(MUL_SIGNED_LOAD_CONST) {
                EXECUTE_MUL_SIGNED_LOAD_CONST_BODY();
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_MUL_SIGNED_LOAD_STACK_CONST: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_MUL_SIGNED_LOAD_STACK_CONST_BODY();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(MUL_SIGNED_LOAD_STACK_CONST) {
                EXECUTE_MUL_SIGNED_LOAD_STACK_CONST_BODY();
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MUL_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type)) {
                    ALGORITHM_2(nativeUInt64, *, ZR_VALUE_TYPE_UINT64);
                } else {
                    execution_try_binary_numeric_float_fallback_or_raise(
                            state, ZR_EXEC_NUMERIC_FALLBACK_MUL, destination, opA, opB, "MUL_UNSIGNED");
                }
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_MUL_UNSIGNED_PLAIN_DEST: {
                const SZrTypeValue *leftValue__ = &BASE(A1(instruction))->value;
                const SZrTypeValue *rightValue__ = &BASE(B1(instruction))->value;
                SZrTypeValue *plainDestination__ = &BASE(E(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue__->type) &&
                          ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightValue__->type));
                EXECUTION_STORE_PLAIN_DIRECT_TO(plainDestination__,
                                                nativeUInt64,
                                                leftValue__->value.nativeObject.nativeUInt64 *
                                                        rightValue__->value.nativeObject.nativeUInt64,
                                                leftValue__->type);
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(MUL_UNSIGNED_PLAIN_DEST) {
                const SZrTypeValue *leftValue__ = &BASE(A1(instruction))->value;
                const SZrTypeValue *rightValue__ = &BASE(B1(instruction))->value;
                SZrTypeValue *plainDestination__ = &BASE(E(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftValue__->type) &&
                          ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightValue__->type));
                EXECUTION_STORE_PLAIN_DIRECT_TO(plainDestination__,
                                                nativeUInt64,
                                                leftValue__->value.nativeObject.nativeUInt64 *
                                                        rightValue__->value.nativeObject.nativeUInt64,
                                                leftValue__->type);
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_MUL_UNSIGNED_CONST: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_MUL_UNSIGNED_CONST_BODY();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(MUL_UNSIGNED_CONST) {
                EXECUTE_MUL_UNSIGNED_CONST_BODY();
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_MUL_UNSIGNED_CONST_PLAIN_DEST: {
                EXECUTE_MUL_UNSIGNED_CONST_BODY_PLAIN_DEST();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(MUL_UNSIGNED_CONST_PLAIN_DEST) {
                EXECUTE_MUL_UNSIGNED_CONST_BODY_PLAIN_DEST();
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MUL_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                execution_apply_binary_numeric_float_or_raise(
                        state, ZR_EXEC_NUMERIC_FALLBACK_MUL, destination, opA, opB, "MUL_FLOAT");
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(NEG) {
                opA = &BASE(A1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_SIGNED_INT(opA->type)) {
                    ZR_VALUE_FAST_SET(destination,
                                      nativeInt64,
                                      -opA->value.nativeObject.nativeInt64,
                                      opA->type);
                } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(opA->type)) {
                    ZrCore_Value_InitAsInt(state, destination, -(TZrInt64)opA->value.nativeObject.nativeUInt64);
                } else if (ZR_VALUE_IS_TYPE_FLOAT(opA->type)) {
                    ZR_VALUE_FAST_SET(destination,
                                      nativeDouble,
                                      -opA->value.nativeObject.nativeDouble,
                                      opA->type);
                } else {
                    SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_NEG);
                    if (meta != ZR_NULL && meta->function != ZR_NULL) {
                        TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                        SZrCallInfo *savedCallInfo = state->callInfoList;
                        PROTECT_E(state, callInfo, {
                            TZrStackValuePointer metaBase = ZR_NULL;
                            TZrStackValuePointer restoredStackTop = savedStackTop;
                            TZrBool metaCallSucceeded = execution_invoke_meta_call(state,
                                                                                  savedCallInfo,
                                                                                  savedStackTop,
                                                                                  savedStackTop,
                                                                                  meta,
                                                                                  opA,
                                                                                  ZR_NULL,
                                                                                  ZR_META_CALL_UNARY_ARGUMENT_COUNT,
                                                                                  &metaBase,
                                                                                  &restoredStackTop);
                            RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                            if (metaCallSucceeded) {
                                SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                                ZrCore_Value_Copy(state, destination, returnValue);
                            } else {
                                ZrCore_Value_ResetAsNull(destination);
                            }
                            state->stackTop.valuePointer = restoredStackTop;
                            state->callInfoList = savedCallInfo;
                        });
                    } else {
                        ZrCore_Value_ResetAsNull(destination);
                    }
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(DIV) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if (!execution_try_builtin_div(state, destination, opA, opB)) {
                    SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_DIV);
                    if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                        TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                        SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        TZrStackValuePointer metaBase = ZR_NULL;
                        TZrStackValuePointer restoredStackTop = savedStackTop;
                        TZrBool metaCallSucceeded = execution_invoke_meta_call(state,
                                                                              savedCallInfo,
                                                                              savedStackTop,
                                                                              savedStackTop,
                                                                              meta,
                                                                              opA,
                                                                              opB,
                                                                              ZR_META_CALL_MAX_ARGUMENTS,
                                                                              &metaBase,
                                                                              &restoredStackTop);
                        RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                        if (metaCallSucceeded) {
                            SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                            ZrCore_Value_Copy(state, destination, returnValue);
                        } else {
                            ZrCore_Value_ResetAsNull(destination);
                        }
                        state->stackTop.valuePointer = restoredStackTop;
                        state->callInfoList = savedCallInfo;
                        });
                    } else {
                    // 无元方法，返回 null
                        ZrCore_Value_ResetAsNull(destination);
                    }
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(DIV_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type)) {
                    SAVE_STATE(state, callInfo); // error: divide by zero
                    if (ZR_UNLIKELY(opB->value.nativeObject.nativeInt64 == 0)) {
                        // ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_RUNTIME_ERROR);
                        ZrCore_Debug_RunError(state, "divide by zero");
                    }
                    ALGORITHM_2(nativeInt64, /, ZR_VALUE_TYPE_INT64);
                } else {
                    execution_try_binary_numeric_float_fallback_or_raise(
                            state, ZR_EXEC_NUMERIC_FALLBACK_DIV, destination, opA, opB, "DIV_SIGNED");
                }
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_DIV_SIGNED_CONST: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_DIV_SIGNED_CONST_BODY();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(DIV_SIGNED_CONST) {
                EXECUTE_DIV_SIGNED_CONST_BODY();
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_DIV_SIGNED_CONST_PLAIN_DEST: {
                EXECUTE_DIV_SIGNED_CONST_BODY_PLAIN_DEST();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(DIV_SIGNED_CONST_PLAIN_DEST) {
                EXECUTE_DIV_SIGNED_CONST_BODY_PLAIN_DEST();
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_DIV_SIGNED_LOAD_CONST: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_DIV_SIGNED_LOAD_CONST_BODY();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(DIV_SIGNED_LOAD_CONST) {
                EXECUTE_DIV_SIGNED_LOAD_CONST_BODY();
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_DIV_SIGNED_LOAD_STACK_CONST: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_DIV_SIGNED_LOAD_STACK_CONST_BODY();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(DIV_SIGNED_LOAD_STACK_CONST) {
                EXECUTE_DIV_SIGNED_LOAD_STACK_CONST_BODY();
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(DIV_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type)) {
                    SAVE_STATE(state, callInfo); // error: divide by zero
                    if (ZR_UNLIKELY(opB->value.nativeObject.nativeUInt64 == 0)) {
                        // ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_RUNTIME_ERROR);
                        ZrCore_Debug_RunError(state, "divide by zero");
                    }
                    ALGORITHM_2(nativeUInt64, /, ZR_VALUE_TYPE_UINT64);
                } else {
                    execution_try_binary_numeric_float_fallback_or_raise(
                            state, ZR_EXEC_NUMERIC_FALLBACK_DIV, destination, opA, opB, "DIV_UNSIGNED");
                }
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_DIV_UNSIGNED_CONST: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_DIV_UNSIGNED_CONST_BODY();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(DIV_UNSIGNED_CONST) {
                EXECUTE_DIV_UNSIGNED_CONST_BODY();
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_DIV_UNSIGNED_CONST_PLAIN_DEST: {
                EXECUTE_DIV_UNSIGNED_CONST_BODY_PLAIN_DEST();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(DIV_UNSIGNED_CONST_PLAIN_DEST) {
                EXECUTE_DIV_UNSIGNED_CONST_BODY_PLAIN_DEST();
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(DIV_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                execution_apply_binary_numeric_float_or_raise(
                        state, ZR_EXEC_NUMERIC_FALLBACK_DIV, destination, opA, opB, "DIV_FLOAT");
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MOD) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if ((ZR_VALUE_IS_TYPE_NUMBER(opA->type) || ZR_VALUE_IS_TYPE_BOOL(opA->type)) &&
                    (ZR_VALUE_IS_TYPE_NUMBER(opB->type) || ZR_VALUE_IS_TYPE_BOOL(opB->type))) {
                    TZrBool divisorWasZero = ZR_FALSE;

                    if (execution_try_builtin_mod_exact_integer_fast(destination, opA, opB, &divisorWasZero)) {
                        if (ZR_UNLIKELY(divisorWasZero)) {
                            SAVE_STATE(state, callInfo); // error: modulo by zero
                            ZrCore_Debug_RunError(state, "modulo by zero");
                        }
                    } else if (ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type)) {
                        TZrInt64 dividend;
                        TZrInt64 divisor;

                        SAVE_STATE(state, callInfo); // error: modulo by zero
                        dividend = ZR_VALUE_IS_TYPE_SIGNED_INT(opA->type)
                                           ? opA->value.nativeObject.nativeInt64
                                           : (TZrInt64)opA->value.nativeObject.nativeUInt64;
                        divisor = ZR_VALUE_IS_TYPE_SIGNED_INT(opB->type)
                                          ? opB->value.nativeObject.nativeInt64
                                          : (TZrInt64)opB->value.nativeObject.nativeUInt64;
                        if (ZR_UNLIKELY(divisor == 0)) {
                            ZrCore_Debug_RunError(state, "modulo by zero");
                        }
                        if (ZR_UNLIKELY(divisor < 0)) {
                            divisor = -divisor;
                        }
                        ZrCore_Value_InitAsInt(state, destination, dividend % divisor);
                    } else {
                        execution_try_binary_numeric_float_fallback_or_raise(
                                state, ZR_EXEC_NUMERIC_FALLBACK_MOD, destination, opA, opB, "MOD");
                    }
                } else {
                    SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_MOD);
                    if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                        TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                        SZrCallInfo *savedCallInfo = state->callInfoList;
                        PROTECT_E(state, callInfo, {
                            TZrStackValuePointer metaBase = ZR_NULL;
                            TZrStackValuePointer restoredStackTop = savedStackTop;
                            TZrBool metaCallSucceeded = execution_invoke_meta_call(state,
                                                                                  savedCallInfo,
                                                                                  savedStackTop,
                                                                                  savedStackTop,
                                                                                  meta,
                                                                                  opA,
                                                                                  opB,
                                                                                  ZR_META_CALL_MAX_ARGUMENTS,
                                                                                  &metaBase,
                                                                                  &restoredStackTop);
                            RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                            if (metaCallSucceeded) {
                                SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                                ZrCore_Value_Copy(state, destination, returnValue);
                            } else {
                                ZrCore_Value_ResetAsNull(destination);
                            }
                            state->stackTop.valuePointer = restoredStackTop;
                            state->callInfoList = savedCallInfo;
                        });
                    } else {
                        // 无元方法，返回 null
                        ZrCore_Value_ResetAsNull(destination);
                    }
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MOD_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type)) {
                    SAVE_STATE(state, callInfo); // error: modulo by zero
                    if (ZR_UNLIKELY(opB->value.nativeObject.nativeInt64 == 0)) {
                        ZrCore_Debug_RunError(state, "modulo by zero");
                    }
                    TZrInt64 divisor = opB->value.nativeObject.nativeInt64;
                    if (ZR_UNLIKELY(divisor < 0)) {
                        divisor = -divisor;
                    }
                    ALGORITHM_CONST_2(nativeInt64, %, ZR_VALUE_TYPE_INT64, divisor);
                } else {
                    execution_try_binary_numeric_float_fallback_or_raise(
                            state, ZR_EXEC_NUMERIC_FALLBACK_MOD, destination, opA, opB, "MOD_SIGNED");
                }
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_MOD_SIGNED_CONST: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_MOD_SIGNED_CONST_BODY();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(MOD_SIGNED_CONST) {
                EXECUTE_MOD_SIGNED_CONST_BODY();
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_MOD_SIGNED_CONST_PLAIN_DEST: {
                EXECUTE_MOD_SIGNED_CONST_BODY_PLAIN_DEST();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(MOD_SIGNED_CONST_PLAIN_DEST) {
                EXECUTE_MOD_SIGNED_CONST_BODY_PLAIN_DEST();
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_MOD_SIGNED_LOAD_CONST: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_MOD_SIGNED_LOAD_CONST_BODY();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(MOD_SIGNED_LOAD_CONST) {
                EXECUTE_MOD_SIGNED_LOAD_CONST_BODY();
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_MOD_SIGNED_LOAD_STACK_CONST: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_MOD_SIGNED_LOAD_STACK_CONST_BODY();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(MOD_SIGNED_LOAD_STACK_CONST) {
                EXECUTE_MOD_SIGNED_LOAD_STACK_CONST_BODY();
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MOD_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type)) {
                    SAVE_STATE(state, callInfo); // error: modulo by zero
                    if (opB->value.nativeObject.nativeUInt64 == 0) {
                        ZrCore_Debug_RunError(state, "modulo by zero");
                    }
                    ALGORITHM_2(nativeUInt64, %, ZR_VALUE_TYPE_UINT64);
                } else {
                    execution_try_binary_numeric_float_fallback_or_raise(
                            state, ZR_EXEC_NUMERIC_FALLBACK_MOD, destination, opA, opB, "MOD_UNSIGNED");
                }
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_MOD_UNSIGNED_CONST: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_MOD_UNSIGNED_CONST_BODY();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(MOD_UNSIGNED_CONST) {
                EXECUTE_MOD_UNSIGNED_CONST_BODY();
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_MOD_UNSIGNED_CONST_PLAIN_DEST: {
                EXECUTE_MOD_UNSIGNED_CONST_BODY_PLAIN_DEST();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(MOD_UNSIGNED_CONST_PLAIN_DEST) {
                EXECUTE_MOD_UNSIGNED_CONST_BODY_PLAIN_DEST();
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MOD_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                execution_apply_binary_numeric_float_or_raise(
                        state, ZR_EXEC_NUMERIC_FALLBACK_MOD, destination, opA, opB, "MOD_FLOAT");
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(POW) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_POW);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        TZrStackValuePointer metaBase = ZR_NULL;
                        TZrStackValuePointer restoredStackTop = savedStackTop;
                        TZrBool metaCallSucceeded = execution_invoke_meta_call(state,
                                                                              savedCallInfo,
                                                                              savedStackTop,
                                                                              savedStackTop,
                                                                              meta,
                                                                              opA,
                                                                              opB,
                                                                              ZR_META_CALL_MAX_ARGUMENTS,
                                                                              &metaBase,
                                                                              &restoredStackTop);
                        RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                        if (metaCallSucceeded) {
                            SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                            ZrCore_Value_Copy(state, destination, returnValue);
                        } else {
                            ZrCore_Value_ResetAsNull(destination);
                        }
                        state->stackTop.valuePointer = restoredStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，返回 null
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(POW_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type)) {
                    SAVE_STATE(state, callInfo); // error: power domain error
                    TZrInt64 valueA = opA->value.nativeObject.nativeInt64;
                    TZrInt64 valueB = opB->value.nativeObject.nativeInt64;
                    if (ZR_UNLIKELY(valueA == 0 && valueB <= 0)) {
                        ZrCore_Debug_RunError(state, "power domain error");
                    }
                    if (ZR_UNLIKELY(valueA < 0)) {
                        ZrCore_Debug_RunError(state, "power domain error");
                    }
                    ALGORITHM_FUNC_2(nativeInt64, ZrCore_Math_IntPower, ZR_VALUE_TYPE_INT64);
                } else {
                    execution_try_binary_numeric_float_fallback_or_raise(
                            state, ZR_EXEC_NUMERIC_FALLBACK_POW, destination, opA, opB, "POW_SIGNED");
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(POW_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type)) {
                    SAVE_STATE(state, callInfo); // error: power domain error
                    TZrUInt64 valueA = opA->value.nativeObject.nativeUInt64;
                    TZrUInt64 valueB = opB->value.nativeObject.nativeUInt64;
                    if (ZR_UNLIKELY(valueA == 0 && valueB == 0)) {
                        ZrCore_Debug_RunError(state, "power domain error");
                    }
                    ALGORITHM_FUNC_2(nativeUInt64, ZrCore_Math_UIntPower, ZR_VALUE_TYPE_UINT64);
                } else {
                    execution_try_binary_numeric_float_fallback_or_raise(
                            state, ZR_EXEC_NUMERIC_FALLBACK_POW, destination, opA, opB, "POW_UNSIGNED");
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(POW_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                execution_apply_binary_numeric_float_or_raise(
                        state, ZR_EXEC_NUMERIC_FALLBACK_POW, destination, opA, opB, "POW_FLOAT");
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SHIFT_LEFT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_SHIFT_LEFT);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        TZrStackValuePointer metaBase = ZR_NULL;
                        TZrStackValuePointer restoredStackTop = savedStackTop;
                        TZrBool metaCallSucceeded = execution_invoke_meta_call(state,
                                                                              savedCallInfo,
                                                                              savedStackTop,
                                                                              savedStackTop,
                                                                              meta,
                                                                              opA,
                                                                              opB,
                                                                              ZR_META_CALL_MAX_ARGUMENTS,
                                                                              &metaBase,
                                                                              &restoredStackTop);
                        RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                        if (metaCallSucceeded) {
                            SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                            ZrCore_Value_Copy(state, destination, returnValue);
                        } else {
                            ZrCore_Value_ResetAsNull(destination);
                        }
                        state->stackTop.valuePointer = restoredStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，返回 null
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SHIFT_LEFT_INT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, <<, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SHIFT_RIGHT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_SHIFT_RIGHT);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        TZrStackValuePointer metaBase = ZR_NULL;
                        TZrStackValuePointer restoredStackTop = savedStackTop;
                        TZrBool metaCallSucceeded = execution_invoke_meta_call(state,
                                                                              savedCallInfo,
                                                                              savedStackTop,
                                                                              savedStackTop,
                                                                              meta,
                                                                              opA,
                                                                              opB,
                                                                              ZR_META_CALL_MAX_ARGUMENTS,
                                                                              &metaBase,
                                                                              &restoredStackTop);
                        RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                        if (metaCallSucceeded) {
                            SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                            ZrCore_Value_Copy(state, destination, returnValue);
                        } else {
                            ZrCore_Value_ResetAsNull(destination);
                        }
                        state->stackTop.valuePointer = restoredStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，返回 null
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SHIFT_RIGHT_INT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, >>, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_NOT) {
                opA = &BASE(A1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_NULL(opA->type)) {
                    ZR_VALUE_FAST_SET(destination, nativeBool, ZR_TRUE, ZR_VALUE_TYPE_BOOL);
                } else if (ZR_VALUE_IS_TYPE_BOOL(opA->type)) {
                    ALGORITHM_1(nativeBool, !, ZR_VALUE_TYPE_BOOL);
                } else if (ZR_VALUE_IS_TYPE_INT(opA->type)) {
                    ZR_VALUE_FAST_SET(destination,
                                      nativeBool,
                                      opA->value.nativeObject.nativeInt64 == 0,
                                      ZR_VALUE_TYPE_BOOL);
                } else if (ZR_VALUE_IS_TYPE_FLOAT(opA->type)) {
                    ZR_VALUE_FAST_SET(destination,
                                      nativeBool,
                                      opA->value.nativeObject.nativeDouble == 0,
                                      ZR_VALUE_TYPE_BOOL);
                } else {
                    ZR_VALUE_FAST_SET(destination, nativeBool, ZR_FALSE, ZR_VALUE_TYPE_BOOL);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_AND) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_BOOL(opA->type) && ZR_VALUE_IS_TYPE_BOOL(opB->type));
                ALGORITHM_2(nativeBool, &&, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_OR) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_BOOL(opA->type) && ZR_VALUE_IS_TYPE_BOOL(opB->type));
                ALGORITHM_2(nativeBool, ||, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_GREATER_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeInt64, >, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_GREATER_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeUInt64, >, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_GREATER_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                execution_apply_binary_numeric_compare_or_raise(
                        state, ZR_EXEC_NUMERIC_COMPARE_GREATER, destination, opA, opB, "LOGICAL_GREATER_FLOAT");
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_LESS_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeInt64, <, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_LESS_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeUInt64, <, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_LESS_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                execution_apply_binary_numeric_compare_or_raise(
                        state, ZR_EXEC_NUMERIC_COMPARE_LESS, destination, opA, opB, "LOGICAL_LESS_FLOAT");
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_EQUAL) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                TZrBool result = ZrCore_Value_Equal(state, opA, opB);
                ZR_VALUE_FAST_SET(destination, nativeBool, result, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_NOT_EQUAL) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                TZrBool result = !ZrCore_Value_Equal(state, opA, opB);
                ZR_VALUE_FAST_SET(destination, nativeBool, result, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_LOGICAL_EQUAL_BOOL: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_TYPED_EQUALITY_BOOL_BODY(ZR_FALSE);
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(LOGICAL_EQUAL_BOOL) {
                EXECUTE_TYPED_EQUALITY_BOOL_BODY(ZR_FALSE);
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_LOGICAL_NOT_EQUAL_BOOL: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_TYPED_EQUALITY_BOOL_BODY(ZR_TRUE);
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(LOGICAL_NOT_EQUAL_BOOL) {
                EXECUTE_TYPED_EQUALITY_BOOL_BODY(ZR_TRUE);
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_LOGICAL_EQUAL_SIGNED: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_TYPED_EQUALITY_SIGNED_BODY(ZR_FALSE);
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(LOGICAL_EQUAL_SIGNED) {
                EXECUTE_TYPED_EQUALITY_SIGNED_BODY(ZR_FALSE);
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_LOGICAL_EQUAL_SIGNED_CONST: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_TYPED_EQUALITY_SIGNED_CONST_BODY(ZR_FALSE);
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(LOGICAL_EQUAL_SIGNED_CONST) {
                EXECUTE_TYPED_EQUALITY_SIGNED_CONST_BODY(ZR_FALSE);
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_LOGICAL_NOT_EQUAL_SIGNED: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_TYPED_EQUALITY_SIGNED_BODY(ZR_TRUE);
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(LOGICAL_NOT_EQUAL_SIGNED) {
                EXECUTE_TYPED_EQUALITY_SIGNED_BODY(ZR_TRUE);
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_LOGICAL_EQUAL_UNSIGNED: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_TYPED_EQUALITY_UNSIGNED_BODY(ZR_FALSE);
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(LOGICAL_EQUAL_UNSIGNED) {
                EXECUTE_TYPED_EQUALITY_UNSIGNED_BODY(ZR_FALSE);
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_LOGICAL_NOT_EQUAL_UNSIGNED: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_TYPED_EQUALITY_UNSIGNED_BODY(ZR_TRUE);
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(LOGICAL_NOT_EQUAL_UNSIGNED) {
                EXECUTE_TYPED_EQUALITY_UNSIGNED_BODY(ZR_TRUE);
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_LOGICAL_EQUAL_FLOAT: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_TYPED_EQUALITY_FLOAT_BODY(ZR_FALSE);
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(LOGICAL_EQUAL_FLOAT) {
                EXECUTE_TYPED_EQUALITY_FLOAT_BODY(ZR_FALSE);
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_LOGICAL_NOT_EQUAL_FLOAT: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_TYPED_EQUALITY_FLOAT_BODY(ZR_TRUE);
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(LOGICAL_NOT_EQUAL_FLOAT) {
                EXECUTE_TYPED_EQUALITY_FLOAT_BODY(ZR_TRUE);
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_LOGICAL_EQUAL_STRING: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_TYPED_EQUALITY_STRING_BODY(ZR_FALSE);
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(LOGICAL_EQUAL_STRING) {
                EXECUTE_TYPED_EQUALITY_STRING_BODY(ZR_FALSE);
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_LOGICAL_NOT_EQUAL_STRING: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_TYPED_EQUALITY_STRING_BODY(ZR_TRUE);
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(LOGICAL_NOT_EQUAL_STRING) {
                EXECUTE_TYPED_EQUALITY_STRING_BODY(ZR_TRUE);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_GREATER_EQUAL_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeInt64, >=, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_GREATER_EQUAL_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeUInt64, >=, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_GREATER_EQUAL_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                execution_apply_binary_numeric_compare_or_raise(
                        state,
                        ZR_EXEC_NUMERIC_COMPARE_GREATER_EQUAL,
                        destination,
                        opA,
                        opB,
                        "LOGICAL_GREATER_EQUAL_FLOAT");
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_LOGICAL_LESS_EQUAL_SIGNED: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_LOGICAL_LESS_EQUAL_SIGNED_BODY();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(LOGICAL_LESS_EQUAL_SIGNED) {
                EXECUTE_LOGICAL_LESS_EQUAL_SIGNED_BODY();
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_LESS_EQUAL_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeUInt64, <=, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_LESS_EQUAL_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                execution_apply_binary_numeric_compare_or_raise(
                        state,
                        ZR_EXEC_NUMERIC_COMPARE_LESS_EQUAL,
                        destination,
                        opA,
                        opB,
                        "LOGICAL_LESS_EQUAL_FLOAT");
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(BITWISE_NOT) {
                opA = &BASE(A1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type));
                ALGORITHM_1(nativeInt64, ~, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(BITWISE_AND) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, &, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(BITWISE_OR) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, |, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(BITWISE_XOR) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, ^, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(BITWISE_SHIFT_LEFT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, <<, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(BITWISE_SHIFT_RIGHT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeUInt64, >>, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUPER_FUNCTION_CALL_NO_ARGS) {
                TZrSize functionSlot = A1(instruction);
                TZrSize expectedReturnCount = 1;

                opA = &BASE(functionSlot)->value;
                ZR_ASSERT(!ZR_VALUE_IS_TYPE_NULL(opA->type) && "Function value is NULL in SUPER_FUNCTION_CALL_NO_ARGS");

                state->stackTop.valuePointer = BASE(functionSlot) + 1;
                callInfo->context.context.programCounter = programCounter + 1;
                SZrCallInfo *nextCallInfo =
                        ZrCore_Function_PreCall(state, BASE(functionSlot), expectedReturnCount, BASE(E(instruction)));
                if (nextCallInfo == ZR_NULL) {
                    RESUME_AFTER_NATIVE_CALL(state, callInfo);
                } else {
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUPER_KNOWN_VM_CALL_NO_ARGS) {
                TZrSize functionSlot = A1(instruction);
                TZrSize expectedReturnCount = 1;

                opA = &BASE(functionSlot)->value;
                ZR_ASSERT(!ZR_VALUE_IS_TYPE_NULL(opA->type) && "Function value is NULL in SUPER_KNOWN_VM_CALL_NO_ARGS");

                state->stackTop.valuePointer = BASE(functionSlot) + 1;
                callInfo->context.context.programCounter = programCounter + 1;
                SZrCallInfo *nextCallInfo = execution_pre_call_known_vm_fast(state,
                                                                             BASE(functionSlot),
                                                                             opA,
                                                                             0,
                                                                             expectedReturnCount,
                                                                             BASE(E(instruction)),
                                                                             profileRuntime,
                                                                             recordHelpers);
                if (nextCallInfo == ZR_NULL) {
                    ZrCore_Debug_RunError(state, "SUPER_KNOWN_VM_CALL_NO_ARGS: invalid known VM callable");
                } else {
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUPER_KNOWN_NATIVE_CALL_NO_ARGS) {
                TZrSize functionSlot = A1(instruction);
                TZrSize expectedReturnCount = 1;

                opA = &BASE(functionSlot)->value;
                ZR_ASSERT(!ZR_VALUE_IS_TYPE_NULL(opA->type) &&
                          "Function value is NULL in SUPER_KNOWN_NATIVE_CALL_NO_ARGS");

                state->stackTop.valuePointer = BASE(functionSlot) + 1;
                callInfo->context.context.programCounter = programCounter + 1;
                execution_pre_call_known_native_fast(state,
                                                     BASE(functionSlot),
                                                     opA,
                                                     expectedReturnCount,
                                                     BASE(E(instruction)),
                                                     profileRuntime,
                                                     recordHelpers);
                RESUME_AFTER_NATIVE_CALL(state, callInfo);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUPER_FUNCTION_TAIL_CALL_NO_ARGS) {
                TZrSize functionSlot = A1(instruction);
                TZrSize expectedReturnCount = 1;
                TZrStackValuePointer functionPointer;
                SZrCallInfo *nextCallInfo;

                opA = &BASE(functionSlot)->value;
                ZR_ASSERT(!ZR_VALUE_IS_TYPE_NULL(opA->type) &&
                          "Function value is NULL in SUPER_FUNCTION_TAIL_CALL_NO_ARGS");

                state->stackTop.valuePointer = BASE(functionSlot) + 1;
                callInfo->context.context.programCounter = programCounter + 1;
                callInfo->callStatus |= ZR_CALL_STATUS_TAIL_CALL;
                functionPointer = BASE(functionSlot);
                if (execution_try_reuse_tail_call_frame(state, callInfo, functionPointer)) {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    goto LZrStart;
                }
                nextCallInfo = ZrCore_Function_PreCall(state, functionPointer, expectedReturnCount, BASE(E(instruction)));
                if (nextCallInfo == ZR_NULL) {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    RESUME_AFTER_NATIVE_CALL(state, callInfo);
                } else {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUPER_KNOWN_VM_TAIL_CALL_NO_ARGS) {
                TZrSize functionSlot = A1(instruction);
                TZrSize expectedReturnCount = 1;
                TZrStackValuePointer functionPointer;
                SZrCallInfo *nextCallInfo;

                opA = &BASE(functionSlot)->value;
                ZR_ASSERT(!ZR_VALUE_IS_TYPE_NULL(opA->type) &&
                          "Function value is NULL in SUPER_KNOWN_VM_TAIL_CALL_NO_ARGS");

                state->stackTop.valuePointer = BASE(functionSlot) + 1;
                callInfo->context.context.programCounter = programCounter + 1;
                callInfo->callStatus |= ZR_CALL_STATUS_TAIL_CALL;
                functionPointer = BASE(functionSlot);
                if (execution_try_reuse_tail_call_frame(state, callInfo, functionPointer)) {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    goto LZrStart;
                }
                nextCallInfo = execution_pre_call_known_vm_fast(state,
                                                                functionPointer,
                                                                opA,
                                                                0,
                                                                expectedReturnCount,
                                                                BASE(E(instruction)),
                                                                profileRuntime,
                                                                recordHelpers);
                if (nextCallInfo == ZR_NULL) {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    ZrCore_Debug_RunError(state, "SUPER_KNOWN_VM_TAIL_CALL_NO_ARGS: invalid known VM callable");
                } else {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUPER_KNOWN_NATIVE_TAIL_CALL_NO_ARGS) {
                TZrSize functionSlot = A1(instruction);
                TZrSize expectedReturnCount = 1;
                TZrStackValuePointer functionPointer;

                opA = &BASE(functionSlot)->value;
                ZR_ASSERT(!ZR_VALUE_IS_TYPE_NULL(opA->type) &&
                          "Function value is NULL in SUPER_KNOWN_NATIVE_TAIL_CALL_NO_ARGS");

                state->stackTop.valuePointer = BASE(functionSlot) + 1;
                callInfo->context.context.programCounter = programCounter + 1;
                callInfo->callStatus |= ZR_CALL_STATUS_TAIL_CALL;
                functionPointer = BASE(functionSlot);
                if (execution_try_reuse_tail_call_frame(state, callInfo, functionPointer)) {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    goto LZrStart;
                }
                execution_pre_call_known_native_fast(state,
                                                     functionPointer,
                                                     opA,
                                                     expectedReturnCount,
                                                     BASE(E(instruction)),
                                                     profileRuntime,
                                                     recordHelpers);
                callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                RESUME_AFTER_NATIVE_CALL(state, callInfo);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(FUNCTION_CALL) {
                // FUNCTION_CALL 指令格式：
                // operandExtra (E) = resultSlot (返回值槽位，用于编译时；ZrCore_Function_PreCall 需要的是 expectedReturnCount)
                // operand1[0] (A1) = functionSlot (函数在栈上的槽位)
                // operand1[1] (B1) = parametersCount (参数数量，直接使用，不从栈读取)
                TZrSize functionSlot = A1(instruction);
                TZrSize parametersCount = B1(instruction);  // 参数数量直接使用，不是栈槽位
                TZrSize expectedReturnCount = 1;  // 期望 1 个返回值；ZrCore_Function_PreCall 的 resultCount 表示 expectedReturnCount；E(instruction)=resultSlot 仅编译时用
                
                opA = &BASE(functionSlot)->value;
                // 这里只保证“非空可调用目标”进入统一预调用分派。
                // ZrCore_Function_PreCall 会继续分流 function/closure/native pointer，
                // 并在其它值类型上解析 @call 元方法。
                ZR_ASSERT(!ZR_VALUE_IS_TYPE_NULL(opA->type) && "Function value is NULL in FUNCTION_CALL");
                
                // 设置栈顶指针（函数在 functionSlot，参数在 functionSlot+1 到 functionSlot+parametersCount）
                if (parametersCount > 0) {
                    state->stackTop.valuePointer = BASE(functionSlot) + parametersCount + 1;
                } else {
                    state->stackTop.valuePointer = BASE(functionSlot) + 1;
                }
                
                // save 下一条指令的地址：fetch 使用 *(PC+=1)，当前 programCounter 指向本指令，故保存 programCounter+1
                callInfo->context.context.programCounter = programCounter + 1;
                SZrCallInfo *nextCallInfo =
                        ZrCore_Function_PreCall(state, BASE(functionSlot), expectedReturnCount, BASE(E(instruction)));
                if (nextCallInfo == ZR_NULL) {
                    // NULL means native call
                    RESUME_AFTER_NATIVE_CALL(state, callInfo);
                } else {
                    // a vm call
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(KNOWN_VM_CALL) {
                TZrSize functionSlot = A1(instruction);
                TZrSize parametersCount = B1(instruction);
                TZrSize expectedReturnCount = 1;
                SZrCallInfo *nextCallInfo;

                opA = &BASE(functionSlot)->value;
                ZR_ASSERT(!ZR_VALUE_IS_TYPE_NULL(opA->type) && "Function value is NULL in KNOWN_VM_CALL");

                state->stackTop.valuePointer = BASE(functionSlot) + parametersCount + 1;
                callInfo->context.context.programCounter = programCounter + 1;
                nextCallInfo = execution_pre_call_known_vm_fast(state,
                                                                BASE(functionSlot),
                                                                opA,
                                                                parametersCount,
                                                                expectedReturnCount,
                                                                BASE(E(instruction)),
                                                                profileRuntime,
                                                                recordHelpers);
                if (nextCallInfo == ZR_NULL) {
                    ZrCore_Debug_RunError(state, "KNOWN_VM_CALL: invalid known VM callable");
                } else {
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(KNOWN_VM_MEMBER_CALL) {
                TZrSize resultSlot = E(instruction);
                TZrUInt16 cacheIndex = (TZrUInt16)A1(instruction);
                SZrFunctionCallSiteCacheEntry *cacheEntry;
                TZrSize parametersCount = 0u;
                TZrSize expectedReturnCount = 1;
                SZrCallInfo *nextCallInfo;

                callInfo->context.context.programCounter = programCounter + 1;
                cacheEntry = execution_member_get_cache_entry_dispatch_fast(
                        currentFunction, cacheIndex, ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET);
                parametersCount = cacheEntry != ZR_NULL ? cacheEntry->argumentCount : 0u;
                state->stackTop.valuePointer = BASE(resultSlot) + parametersCount + 1;
                nextCallInfo = execution_pre_call_known_vm_member_fast(state,
                                                                       programCounter,
                                                                       currentFunction,
                                                                       cacheIndex,
                                                                       cacheEntry,
                                                                       BASE(resultSlot),
                                                                       expectedReturnCount,
                                                                       BASE(E(instruction)),
                                                                       profileRuntime,
                                                                       recordHelpers);
                if (nextCallInfo == ZR_NULL) {
                    ZrCore_Debug_RunError(state, "KNOWN_VM_MEMBER_CALL: invalid cached VM member callable");
                } else {
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(KNOWN_VM_MEMBER_CALL_LOAD1_U8) {
                TZrSize resultSlot = E(instruction);
                TZrUInt16 cacheIndex = (TZrUInt16)instruction.instruction.operand.operand0[0];
                TZrUInt16 receiverSourceSlot = (TZrUInt16)instruction.instruction.operand.operand0[1];
                TZrUInt16 argumentSourceSlot = (TZrUInt16)instruction.instruction.operand.operand0[2];
                SZrFunctionCallSiteCacheEntry *cacheEntry;
                TZrSize parametersCount = 0u;
                TZrSize expectedReturnCount = 1;
                SZrCallInfo *nextCallInfo;

                execution_copy_stack_value_to_stack_fast_no_profile(
                        state, &BASE(resultSlot + 1u)->value, &BASE(receiverSourceSlot)->value);
                execution_copy_stack_value_to_stack_fast_no_profile(
                        state, &BASE(resultSlot + 2u)->value, &BASE(argumentSourceSlot)->value);
                if (ZR_UNLIKELY(recordHelpers)) {
                    profileRuntime->helperCounts[ZR_PROFILE_HELPER_VALUE_COPY] += 2u;
                }

                callInfo->context.context.programCounter = programCounter + 1;
                cacheEntry = execution_member_get_cache_entry_dispatch_fast(
                        currentFunction, cacheIndex, ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET);
                parametersCount = cacheEntry != ZR_NULL ? cacheEntry->argumentCount : 0u;
                state->stackTop.valuePointer = BASE(resultSlot) + parametersCount + 1;
                nextCallInfo = execution_pre_call_known_vm_member_fast(state,
                                                                       programCounter,
                                                                       currentFunction,
                                                                       cacheIndex,
                                                                       cacheEntry,
                                                                       BASE(resultSlot),
                                                                       expectedReturnCount,
                                                                       BASE(resultSlot),
                                                                       profileRuntime,
                                                                       recordHelpers);
                if (nextCallInfo == ZR_NULL) {
                    ZrCore_Debug_RunError(state, "KNOWN_VM_MEMBER_CALL_LOAD1_U8: invalid cached VM member callable");
                } else {
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(KNOWN_NATIVE_CALL) {
                TZrSize functionSlot = A1(instruction);
                TZrSize parametersCount = B1(instruction);
                TZrSize expectedReturnCount = 1;

                opA = &BASE(functionSlot)->value;
                ZR_ASSERT(!ZR_VALUE_IS_TYPE_NULL(opA->type) && "Function value is NULL in KNOWN_NATIVE_CALL");

                state->stackTop.valuePointer = BASE(functionSlot) + parametersCount + 1;
                callInfo->context.context.programCounter = programCounter + 1;
                execution_pre_call_known_native_fast(state,
                                                     BASE(functionSlot),
                                                     opA,
                                                     expectedReturnCount,
                                                     BASE(E(instruction)),
                                                     profileRuntime,
                                                     recordHelpers);
                RESUME_AFTER_NATIVE_CALL(state, callInfo);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(KNOWN_NATIVE_MEMBER_CALL) {
                TZrSize resultSlot = E(instruction);
                TZrUInt16 cacheIndex = (TZrUInt16)A1(instruction);
                SZrTypeValue *callableValue;
                SZrTypeValue *receiverValue;
                TZrSize parametersCount = B1(instruction);
                TZrSize expectedReturnCount = 1;

                callInfo->context.context.programCounter = programCounter + 1;
                state->stackTop.valuePointer = BASE(resultSlot) + parametersCount + 1;
                callableValue = ZrCore_Stack_GetValueNoProfile(BASE(resultSlot));
                receiverValue = ZrCore_Stack_GetValueNoProfile(BASE(resultSlot) + 1);
                if (callableValue == ZR_NULL || receiverValue == ZR_NULL ||
                    !execution_member_get_cached(
                            state, programCounter, currentFunction, cacheIndex, receiverValue, callableValue)) {
                    ZrCore_Debug_RunError(state, "KNOWN_NATIVE_MEMBER_CALL: invalid cached native member callable");
                }
                execution_pre_call_known_native_fast(state,
                                                     BASE(resultSlot),
                                                     callableValue,
                                                     expectedReturnCount,
                                                     BASE(E(instruction)),
                                                     profileRuntime,
                                                     recordHelpers);
                RESUME_AFTER_NATIVE_CALL(state, callInfo);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUPER_DYN_CALL_NO_ARGS) {
                TZrSize functionSlot = A1(instruction);
                TZrSize expectedReturnCount = 1;

                opA = &BASE(functionSlot)->value;
                ZR_ASSERT(!ZR_VALUE_IS_TYPE_NULL(opA->type) && "Function value is NULL in SUPER_DYN_CALL_NO_ARGS");

                state->stackTop.valuePointer = BASE(functionSlot) + 1;
                callInfo->context.context.programCounter = programCounter + 1;
                SZrCallInfo *nextCallInfo =
                        ZrCore_Function_PreCall(state, BASE(functionSlot), expectedReturnCount, BASE(E(instruction)));
                if (nextCallInfo == ZR_NULL) {
                    RESUME_AFTER_NATIVE_CALL(state, callInfo);
                } else {
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUPER_DYN_CALL_CACHED) {
                TZrSize functionSlot = A1(instruction);
                const SZrFunctionCallSiteCacheEntry *cacheEntry =
                        execution_get_callsite_cache_entry(currentFunction,
                                                           B1(instruction),
                                                           ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_CALL);
                TZrSize expectedReturnCount = 1;
                SZrCallInfo *nextCallInfo;

                if (cacheEntry == ZR_NULL) {
                    ZrCore_Debug_RunError(state, "SUPER_DYN_CALL_CACHED: invalid callsite cache");
                }

                state->stackTop.valuePointer = BASE(functionSlot) + cacheEntry->argumentCount + 1;
                execution_try_prepare_dyn_call_target_cached(state,
                                                             currentFunction,
                                                             B1(instruction),
                                                             BASE(functionSlot),
                                                             ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_CALL);

                callInfo->context.context.programCounter = programCounter + 1;
                nextCallInfo = ZrCore_Function_PreCall(state, BASE(functionSlot), expectedReturnCount, BASE(E(instruction)));
                if (nextCallInfo == ZR_NULL) {
                    RESUME_AFTER_NATIVE_CALL(state, callInfo);
                } else {
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUPER_DYN_TAIL_CALL_NO_ARGS) {
                TZrSize functionSlot = A1(instruction);
                TZrSize expectedReturnCount = 1;
                TZrStackValuePointer functionPointer;
                SZrCallInfo *nextCallInfo;

                opA = &BASE(functionSlot)->value;
                ZR_ASSERT(!ZR_VALUE_IS_TYPE_NULL(opA->type) &&
                          "Function value is NULL in SUPER_DYN_TAIL_CALL_NO_ARGS");

                state->stackTop.valuePointer = BASE(functionSlot) + 1;
                callInfo->context.context.programCounter = programCounter + 1;
                callInfo->callStatus |= ZR_CALL_STATUS_TAIL_CALL;
                functionPointer = BASE(functionSlot);
                if (execution_try_reuse_tail_call_frame(state, callInfo, functionPointer)) {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    goto LZrStart;
                }
                nextCallInfo = ZrCore_Function_PreCall(state, functionPointer, expectedReturnCount, BASE(E(instruction)));
                if (nextCallInfo == ZR_NULL) {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    RESUME_AFTER_NATIVE_CALL(state, callInfo);
                } else {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUPER_DYN_TAIL_CALL_CACHED) {
                TZrSize functionSlot = A1(instruction);
                const SZrFunctionCallSiteCacheEntry *cacheEntry =
                        execution_get_callsite_cache_entry(currentFunction,
                                                           B1(instruction),
                                                           ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_TAIL_CALL);
                TZrSize expectedReturnCount = 1;
                TZrStackValuePointer functionPointer;
                SZrCallInfo *nextCallInfo;

                if (cacheEntry == ZR_NULL) {
                    ZrCore_Debug_RunError(state, "SUPER_DYN_TAIL_CALL_CACHED: invalid callsite cache");
                }

                state->stackTop.valuePointer = BASE(functionSlot) + cacheEntry->argumentCount + 1;
                execution_try_prepare_dyn_call_target_cached(state,
                                                             currentFunction,
                                                             B1(instruction),
                                                             BASE(functionSlot),
                                                             ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_TAIL_CALL);

                callInfo->context.context.programCounter = programCounter + 1;
                callInfo->callStatus |= ZR_CALL_STATUS_TAIL_CALL;
                functionPointer = BASE(functionSlot);
                if (execution_try_reuse_tail_call_frame(state, callInfo, functionPointer)) {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    goto LZrStart;
                }
                nextCallInfo = ZrCore_Function_PreCall(state, functionPointer, expectedReturnCount, BASE(E(instruction)));
                if (nextCallInfo == ZR_NULL) {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    RESUME_AFTER_NATIVE_CALL(state, callInfo);
                } else {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(DYN_CALL) {
                TZrSize functionSlot = A1(instruction);
                TZrSize parametersCount = B1(instruction);
                TZrSize expectedReturnCount = 1;

                opA = &BASE(functionSlot)->value;
                ZR_ASSERT(!ZR_VALUE_IS_TYPE_NULL(opA->type) && "Function value is NULL in DYN_CALL");

                if (parametersCount > 0) {
                    state->stackTop.valuePointer = BASE(functionSlot) + parametersCount + 1;
                } else {
                    state->stackTop.valuePointer = BASE(functionSlot) + 1;
                }

                callInfo->context.context.programCounter = programCounter + 1;
                SZrCallInfo *nextCallInfo =
                        ZrCore_Function_PreCall(state, BASE(functionSlot), expectedReturnCount, BASE(E(instruction)));
                if (nextCallInfo == ZR_NULL) {
                    RESUME_AFTER_NATIVE_CALL(state, callInfo);
                } else {
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(DYN_TAIL_CALL) {
                TZrSize functionSlot = A1(instruction);
                TZrSize parametersCount = B1(instruction);
                TZrSize expectedReturnCount = 1;
                TZrStackValuePointer functionPointer;
                SZrCallInfo *nextCallInfo;

                opA = &BASE(functionSlot)->value;
                ZR_ASSERT(!ZR_VALUE_IS_TYPE_NULL(opA->type) && "Function value is NULL in DYN_TAIL_CALL");

                if (parametersCount > 0) {
                    state->stackTop.valuePointer = BASE(functionSlot) + parametersCount + 1;
                } else {
                    state->stackTop.valuePointer = BASE(functionSlot) + 1;
                }

                callInfo->context.context.programCounter = programCounter + 1;
                callInfo->callStatus |= ZR_CALL_STATUS_TAIL_CALL;
                functionPointer = BASE(functionSlot);
                if (execution_try_reuse_tail_call_frame(state, callInfo, functionPointer)) {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    goto LZrStart;
                }
                nextCallInfo = ZrCore_Function_PreCall(state, functionPointer, expectedReturnCount, BASE(E(instruction)));
                if (nextCallInfo == ZR_NULL) {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    RESUME_AFTER_NATIVE_CALL(state, callInfo);
                } else {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUPER_META_CALL_NO_ARGS) {
                TZrSize functionSlot = A1(instruction);
                TZrSize expectedReturnCount = 1;

                state->stackTop.valuePointer = BASE(functionSlot) + 1;
                if (!execution_prepare_meta_call_target(state, BASE(functionSlot))) {
                    ZrCore_Debug_RunError(state, "SUPER_META_CALL_NO_ARGS: receiver does not define @call");
                }

                callInfo->context.context.programCounter = programCounter + 1;
                SZrCallInfo *nextCallInfo =
                        ZrCore_Function_PreCall(state, BASE(functionSlot), expectedReturnCount, BASE(E(instruction)));
                if (nextCallInfo == ZR_NULL) {
                    RESUME_AFTER_NATIVE_CALL(state, callInfo);
                } else {
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUPER_META_CALL_CACHED) {
                TZrSize functionSlot = A1(instruction);
                const SZrFunctionCallSiteCacheEntry *cacheEntry =
                        execution_get_callsite_cache_entry(currentFunction,
                                                           B1(instruction),
                                                           ZR_FUNCTION_CALLSITE_CACHE_KIND_META_CALL);
                TZrSize expectedReturnCount = 1;
                SZrCallInfo *nextCallInfo;

                if (cacheEntry == ZR_NULL) {
                    ZrCore_Debug_RunError(state, "SUPER_META_CALL_CACHED: invalid callsite cache");
                }

                state->stackTop.valuePointer = BASE(functionSlot) + cacheEntry->argumentCount + 1;
                if (!execution_prepare_meta_call_target_cached(state,
                                                               currentFunction,
                                                               B1(instruction),
                                                               BASE(functionSlot),
                                                               ZR_FUNCTION_CALLSITE_CACHE_KIND_META_CALL)) {
                    ZrCore_Debug_RunError(state, "SUPER_META_CALL_CACHED: receiver does not define @call");
                }

                callInfo->context.context.programCounter = programCounter + 1;
                nextCallInfo = ZrCore_Function_PreCall(state, BASE(functionSlot), expectedReturnCount, BASE(E(instruction)));
                if (nextCallInfo == ZR_NULL) {
                    RESUME_AFTER_NATIVE_CALL(state, callInfo);
                } else {
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUPER_META_TAIL_CALL_NO_ARGS) {
                TZrSize functionSlot = A1(instruction);
                TZrSize expectedReturnCount = 1;
                TZrStackValuePointer functionPointer;
                SZrCallInfo *nextCallInfo;

                state->stackTop.valuePointer = BASE(functionSlot) + 1;
                if (!execution_prepare_meta_call_target(state, BASE(functionSlot))) {
                    ZrCore_Debug_RunError(state, "SUPER_META_TAIL_CALL_NO_ARGS: receiver does not define @call");
                }

                callInfo->context.context.programCounter = programCounter + 1;
                callInfo->callStatus |= ZR_CALL_STATUS_TAIL_CALL;
                functionPointer = BASE(functionSlot);
                if (execution_try_reuse_tail_call_frame(state, callInfo, functionPointer)) {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    goto LZrStart;
                }
                nextCallInfo = ZrCore_Function_PreCall(state, functionPointer, expectedReturnCount, BASE(E(instruction)));
                if (nextCallInfo == ZR_NULL) {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    RESUME_AFTER_NATIVE_CALL(state, callInfo);
                } else {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUPER_META_TAIL_CALL_CACHED) {
                TZrSize functionSlot = A1(instruction);
                const SZrFunctionCallSiteCacheEntry *cacheEntry =
                        execution_get_callsite_cache_entry(currentFunction,
                                                           B1(instruction),
                                                           ZR_FUNCTION_CALLSITE_CACHE_KIND_META_TAIL_CALL);
                TZrSize expectedReturnCount = 1;
                TZrStackValuePointer functionPointer;
                SZrCallInfo *nextCallInfo;

                if (cacheEntry == ZR_NULL) {
                    ZrCore_Debug_RunError(state, "SUPER_META_TAIL_CALL_CACHED: invalid callsite cache");
                }

                state->stackTop.valuePointer = BASE(functionSlot) + cacheEntry->argumentCount + 1;
                if (!execution_prepare_meta_call_target_cached(state,
                                                               currentFunction,
                                                               B1(instruction),
                                                               BASE(functionSlot),
                                                               ZR_FUNCTION_CALLSITE_CACHE_KIND_META_TAIL_CALL)) {
                    ZrCore_Debug_RunError(state, "SUPER_META_TAIL_CALL_CACHED: receiver does not define @call");
                }

                callInfo->context.context.programCounter = programCounter + 1;
                callInfo->callStatus |= ZR_CALL_STATUS_TAIL_CALL;
                functionPointer = BASE(functionSlot);
                if (execution_try_reuse_tail_call_frame(state, callInfo, functionPointer)) {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    goto LZrStart;
                }
                nextCallInfo = ZrCore_Function_PreCall(state, functionPointer, expectedReturnCount, BASE(E(instruction)));
                if (nextCallInfo == ZR_NULL) {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    RESUME_AFTER_NATIVE_CALL(state, callInfo);
                } else {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(META_CALL) {
                TZrSize functionSlot = A1(instruction);
                TZrSize parametersCount = B1(instruction);
                TZrSize expectedReturnCount = 1;

                if (parametersCount > 0) {
                    state->stackTop.valuePointer = BASE(functionSlot) + parametersCount + 1;
                } else {
                    state->stackTop.valuePointer = BASE(functionSlot) + 1;
                }

                if (!execution_prepare_meta_call_target(state, BASE(functionSlot))) {
                    ZrCore_Debug_RunError(state, "META_CALL: receiver does not define @call");
                }
                parametersCount++;

                callInfo->context.context.programCounter = programCounter + 1;
                SZrCallInfo *nextCallInfo =
                        ZrCore_Function_PreCall(state, BASE(functionSlot), expectedReturnCount, BASE(E(instruction)));
                if (nextCallInfo == ZR_NULL) {
                    RESUME_AFTER_NATIVE_CALL(state, callInfo);
                } else {
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(META_TAIL_CALL) {
                TZrSize functionSlot = A1(instruction);
                TZrSize parametersCount = B1(instruction);
                TZrSize expectedReturnCount = 1;
                TZrStackValuePointer functionPointer;
                SZrCallInfo *nextCallInfo;

                if (parametersCount > 0) {
                    state->stackTop.valuePointer = BASE(functionSlot) + parametersCount + 1;
                } else {
                    state->stackTop.valuePointer = BASE(functionSlot) + 1;
                }

                if (!execution_prepare_meta_call_target(state, BASE(functionSlot))) {
                    ZrCore_Debug_RunError(state, "META_TAIL_CALL: receiver does not define @call");
                }
                parametersCount++;

                callInfo->context.context.programCounter = programCounter + 1;
                callInfo->callStatus |= ZR_CALL_STATUS_TAIL_CALL;
                functionPointer = BASE(functionSlot);
                if (execution_try_reuse_tail_call_frame(state, callInfo, functionPointer)) {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    goto LZrStart;
                }
                nextCallInfo = ZrCore_Function_PreCall(state, functionPointer, expectedReturnCount, BASE(E(instruction)));
                if (nextCallInfo == ZR_NULL) {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    RESUME_AFTER_NATIVE_CALL(state, callInfo);
                } else {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(FUNCTION_TAIL_CALL) {
                // FUNCTION_TAIL_CALL 指令格式：
                // operandExtra (E) = resultSlot (返回值槽位，编译时；ZrCore_Function_PreCall 需要的是 expectedReturnCount)
                // operand1[0] (A1) = functionSlot (函数在栈上的槽位)
                // operand1[1] (B1) = parametersCount (参数数量，直接使用，不从栈读取)
                TZrSize functionSlot = A1(instruction);
                TZrSize parametersCount = B1(instruction);  // 参数数量直接使用，不是栈槽位
                TZrSize expectedReturnCount = 1;  // 期望 1 个返回值；E(instruction)=resultSlot 仅编译时用
                
                opA = &BASE(functionSlot)->value;
                // 与普通调用保持一致，把实际可调用性判断交给统一预调用分派，
                // 以便对象值通过 @call 元方法进入调用链。
                ZR_ASSERT(!ZR_VALUE_IS_TYPE_NULL(opA->type) && "Function value is NULL in FUNCTION_CALL");
                
                // 设置栈顶指针
                if (parametersCount > 0) {
                    state->stackTop.valuePointer = BASE(functionSlot) + parametersCount + 1;
                } else {
                    state->stackTop.valuePointer = BASE(functionSlot) + 1;
                }
                
                // 尾调用：重用当前调用帧
                // 保存下一条指令的地址：fetch 使用 *(PC+=1)，故保存 programCounter+1
                callInfo->context.context.programCounter = programCounter + 1;
                // 设置尾调用标志
                callInfo->callStatus |= ZR_CALL_STATUS_TAIL_CALL;
                // 准备调用参数（函数在BASE(functionSlot)，参数在BASE(functionSlot+1)到BASE(functionSlot+parametersCount)）
                TZrStackValuePointer functionPointer = BASE(functionSlot);
                if (execution_try_reuse_tail_call_frame(state, callInfo, functionPointer)) {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    goto LZrStart;
                }
                // 调用函数（expectedReturnCount=1，与 FUNCTION_CALL 一致）；返回值写入 BASE(E(instruction))
                SZrCallInfo *nextCallInfo =
                        ZrCore_Function_PreCall(state, functionPointer, expectedReturnCount, BASE(E(instruction)));
                if (nextCallInfo == ZR_NULL) {
                    // Native调用，清除尾调用标志
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    RESUME_AFTER_NATIVE_CALL(state, callInfo);
                } else {
                    // VM调用：对于尾调用，重用当前callInfo而不是创建新的
                    // 但ZrFunctionPreCall总是创建新的callInfo，所以我们需要调整
                    // 实际上，对于真正的尾调用优化，我们需要手动设置callInfo的字段
                    // 这里先使用简单的实现：清除尾调用标志，使用普通调用
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(KNOWN_VM_TAIL_CALL) {
                TZrSize functionSlot = A1(instruction);
                TZrSize parametersCount = B1(instruction);
                TZrSize expectedReturnCount = 1;
                TZrStackValuePointer functionPointer;
                SZrCallInfo *nextCallInfo;

                opA = &BASE(functionSlot)->value;
                ZR_ASSERT(!ZR_VALUE_IS_TYPE_NULL(opA->type) && "Function value is NULL in KNOWN_VM_TAIL_CALL");

                state->stackTop.valuePointer = BASE(functionSlot) + parametersCount + 1;
                callInfo->context.context.programCounter = programCounter + 1;
                callInfo->callStatus |= ZR_CALL_STATUS_TAIL_CALL;
                functionPointer = BASE(functionSlot);
                if (execution_try_reuse_tail_call_frame(state, callInfo, functionPointer)) {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    goto LZrStart;
                }
                nextCallInfo = execution_pre_call_known_vm_fast(state,
                                                                functionPointer,
                                                                opA,
                                                                parametersCount,
                                                                expectedReturnCount,
                                                                BASE(E(instruction)),
                                                                profileRuntime,
                                                                recordHelpers);
                if (nextCallInfo == ZR_NULL) {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    ZrCore_Debug_RunError(state, "KNOWN_VM_TAIL_CALL: invalid known VM callable");
                } else {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(KNOWN_NATIVE_TAIL_CALL) {
                TZrSize functionSlot = A1(instruction);
                TZrSize parametersCount = B1(instruction);
                TZrSize expectedReturnCount = 1;
                TZrStackValuePointer functionPointer;

                opA = &BASE(functionSlot)->value;
                ZR_ASSERT(!ZR_VALUE_IS_TYPE_NULL(opA->type) && "Function value is NULL in KNOWN_NATIVE_TAIL_CALL");

                state->stackTop.valuePointer = BASE(functionSlot) + parametersCount + 1;
                callInfo->context.context.programCounter = programCounter + 1;
                callInfo->callStatus |= ZR_CALL_STATUS_TAIL_CALL;
                functionPointer = BASE(functionSlot);
                if (execution_try_reuse_tail_call_frame(state, callInfo, functionPointer)) {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    goto LZrStart;
                }
                execution_pre_call_known_native_fast(state,
                                                     functionPointer,
                                                     opA,
                                                     expectedReturnCount,
                                                     BASE(E(instruction)),
                                                     profileRuntime,
                                                     recordHelpers);
                callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                RESUME_AFTER_NATIVE_CALL(state, callInfo);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(FUNCTION_RETURN) {
                // FUNCTION_RETURN 指令格式：
                // operandExtra (E) = 返回值数量 (returnCount)
                // operand1[0] (A1) = 返回值槽位 (resultSlot)
                // operand1[1] (B1) = 可变参数参数数量 (variableArguments, 0 表示非可变参数函数)
                TZrSize returnCount = E(instruction);
                TZrSize resultSlot = A1(instruction);
                TZrSize variableArguments = B1(instruction);

                // save its program counter
                callInfo->context.context.programCounter = programCounter;
                if (ZR_UNLIKELY(state->exceptionHandlerStackLength > 0u)) {
                    execution_discard_exception_handlers_for_callinfo_fast(state, callInfo);
                }

                if (state->stackTop.valuePointer < callInfo->functionTop.valuePointer) {
                    state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
                }
                if (currentFunction != ZR_NULL && currentFunction->returnEscapeSlotCount > 0u) {
                    for (TZrSize returnIndex = 0; returnIndex < returnCount; returnIndex++) {
                        ZrCore_Function_ApplyReturnEscape(state,
                                                          currentFunction,
                                                          (TZrUInt32)(resultSlot + returnIndex),
                                                          &BASE(resultSlot + returnIndex)->value);
                    }
                }
                if (execution_callinfo_has_pending_close_work(state, callInfo)) {
                    // The to-be-closed list only tracks close metas, not ordinary
                    // captured locals, so the frame still needs a full closure pass
                    // when pending close work exists.
                    ZrCore_Closure_CloseClosure(state,
                                                callInfo->functionBase.valuePointer + 1,
                                                ZR_THREAD_STATUS_INVALID,
                                                ZR_FALSE);
                }
                UPDATE_BASE(callInfo);

                // 如果是可变参数函数，需要调整 functionBase 指针
                // 参考 Lua: if (nparams1) ci->func.p -= ci->u.l.nextraargs + nparams1;
                if (variableArguments > 0) {
                    callInfo->functionBase.valuePointer -=
                            callInfo->context.context.variableArgumentCount + variableArguments;
                }
                if (ZR_LIKELY(state->debugHookSignal == 0u &&
                              callInfo->expectedReturnCount == 1u &&
                              callInfo->callStatus == ZR_CALL_STATUS_NONE)) {
                    TZrStackValuePointer returnSource = returnCount > 0u ? BASE(resultSlot) : ZR_NULL;
                    execution_post_call_single_result_resolved_source_fast(state, callInfo, returnSource);
                } else {
                    state->stackTop.valuePointer = BASE(resultSlot) + returnCount;
                    ZrCore_Function_PostCall(state, callInfo, returnCount);
                }
                trap = callInfo->context.context.trap;
                goto LZrReturn;
            }

        LZrReturn: {
            // return from vm
            if (callInfo->callStatus & ZR_CALL_STATUS_CREATE_FRAME) {
                return;
            } else {
                callInfo = callInfo->previous;
                goto LZrReturning;
            }
        }
            DONE(1);
            ZR_INSTRUCTION_LABEL(GETUPVAL) {
                // GETUPVAL 指令格式：
                // operandExtra (E) = destination slot
                // operand1[0] (A1) = upvalue index
                // operand1[1] (B1) = 未使用
                TZrSize upvalueIndex = A1(instruction);
                SZrClosure *currentClosure = execution_get_current_vm_closure_no_profile(state, base);
                if (ZR_UNLIKELY(upvalueIndex >= currentClosure->closureValueCount)) {
                    ZrCore_Debug_RunError(state, "upvalue index out of range");
                }
                SZrClosureValue *closureValue = currentClosure->closureValuesExtend[upvalueIndex];
                if (ZR_UNLIKELY(closureValue == ZR_NULL)) {
                    // 如果闭包值为 NULL，尝试初始化（这可能是第一次访问）
                    // 注意：这不应该发生在正常执行中，但为了测试的兼容性，我们允许这种情况
                    ZrCore_Debug_RunError(state, "upvalue is null - closure values may not be initialized");
                }
                ZrCore_Value_Copy(state, destination, execution_get_closure_value_no_profile(closureValue));
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(SETUPVAL) {
                // SETUPVAL 指令格式：
                // operandExtra (E) = source slot (destination)
                // operand1[0] (A1) = upvalue index
                // operand1[1] (B1) = 未使用
                TZrSize upvalueIndex = A1(instruction);
                SZrClosure *currentClosure = execution_get_current_vm_closure_no_profile(state, base);
                if (ZR_UNLIKELY(upvalueIndex >= currentClosure->closureValueCount)) {
                    ZrCore_Debug_RunError(state, "upvalue index out of range");
                }
                SZrClosureValue *closureValue = currentClosure->closureValuesExtend[upvalueIndex];
                if (ZR_UNLIKELY(closureValue == ZR_NULL)) {
                    ZrCore_Debug_RunError(state, "upvalue is null");
                }
                SZrTypeValue *target = execution_get_closure_value_no_profile(closureValue);
                ZrCore_Value_Copy(state, target, destination);
                ZrCore_Value_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(currentClosure->closureValuesExtend[upvalueIndex]),
                               destination);
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(GET_SUB_FUNCTION) {
                // GET_SUB_FUNCTION 指令格式：
                // operandExtra (E) = destSlot (destination已通过E(instruction)定义)
                // operand1[0] (A1) = childFunctionIndex (子函数在 childFunctionList 中的索引)
                // operand1[1] (B1) = 0 (未使用)
                // GET_SUB_FUNCTION 用于从父函数的 childFunctionList 中通过索引获取子函数并压入栈
                // 这是编译时确定的静态索引，运行时直接通过索引访问，无需名称查找
                // 注意：GET_SUB_FUNCTION 只操作函数类型（ZR_VALUE_TYPE_FUNCTION 或 ZR_VALUE_TYPE_CLOSURE）
                TZrSize childFunctionIndex = A1(instruction);
                
                // 获取父函数的 callInfo
                SZrCallInfo *parentCallInfo = callInfo->previous;
                TZrBool found = ZR_FALSE;
                SZrFunction *parentFunction = ZR_NULL;
                
                if (parentCallInfo != ZR_NULL) {
                    TZrBool isVM = ZR_CALL_INFO_IS_VM(parentCallInfo);
                    if (isVM) {
                        // 获取父函数的闭包和函数
                        SZrTypeValue *parentFunctionBaseValue = ZrCore_Stack_GetValue(parentCallInfo->functionBase.valuePointer);
                        if (parentFunctionBaseValue != ZR_NULL) {
                            // 类型检查：确保父函数是函数类型或闭包类型
                            if (parentFunctionBaseValue->type == ZR_VALUE_TYPE_FUNCTION ||
                                parentFunctionBaseValue->type == ZR_VALUE_TYPE_CLOSURE) {
                                parentFunction =
                                        execution_try_resolve_vm_metadata_function_fast(state,
                                                                                         parentFunctionBaseValue,
                                                                                         ZR_NULL);
                            } else {
                                ZrCore_Debug_RunError(state, "GET_SUB_FUNCTION: parent must be a function or closure");
                            }
                        }
                    } else {
                        // 如果不是 VM 调用，尝试从当前函数的闭包获取子函数
                        parentFunction = currentFunction;
                    }
                } else if (parentCallInfo == ZR_NULL) {
                    // 如果没有父函数的 callInfo，尝试从当前函数的闭包获取子函数
                    // 这适用于顶层函数或测试函数直接调用的情况
                    parentFunction = currentFunction;
                }
                
                // 从父函数获取子函数
                if (parentFunction != ZR_NULL) {
                    // 通过索引直接访问 childFunctionList
                    if (childFunctionIndex < parentFunction->childFunctionLength) {
                        SZrFunction *childFunction = &parentFunction->childFunctionList[childFunctionIndex];
                        if (childFunction != ZR_NULL &&
                            childFunction->super.type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
                            SZrClosureValue **parentClosureValues =
                                    closure != ZR_NULL ? closure->closureValuesExtend : ZR_NULL;
                            ZrCore_Closure_PushToStack(state, childFunction, parentClosureValues, base, BASE(E(instruction)));
                            destination->type = ZR_VALUE_TYPE_CLOSURE;
                            destination->isGarbageCollectable = ZR_TRUE;
                            destination->isNative = ZR_FALSE;
                            found = ZR_TRUE;
                        }
                    }
                }
                
                // 如果没找到，返回 null
                if (!found) {
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);


            ZR_INSTRUCTION_LABEL(GET_GLOBAL) {
                // GET_GLOBAL 指令格式：
                // operandExtra (E) = destSlot (destination已通过E(instruction)定义)
                // operand1[0] (A1) = 0 (未使用)
                // operand1[1] (B1) = 0 (未使用)
                // GET_GLOBAL 用于获取全局 zr 对象到堆栈
                SZrGlobalState *global = state->global;
                if (global != ZR_NULL && global->zrObject.type == ZR_VALUE_TYPE_OBJECT) {
                    ZrCore_Value_Copy(state, destination, &global->zrObject);
                } else {
                    // 如果 zr 对象未初始化，返回 null
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(TYPEOF) {
                SZrTypeValue stableTargetValue;
                SZrTypeValue reflectedTypeValue;
                opA = &BASE(A1(instruction))->value;
                stableTargetValue = *opA;
                ZrCore_Value_ResetAsNull(&reflectedTypeValue);
                SAVE_PC(state, callInfo);
                state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
                if (!ZrCore_Reflection_TypeOfValue(state, &stableTargetValue, &reflectedTypeValue)) {
                    ZrCore_Debug_RunError(state, "TYPEOF: failed to materialize runtime type");
                }
                if (state->stackTop.valuePointer < callInfo->functionBase.valuePointer + 1 ||
                    state->stackTop.valuePointer > callInfo->functionTop.valuePointer) {
                    state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
                }
                RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                if (destination == &ret) {
                    *destination = reflectedTypeValue;
                } else {
                    ZrCore_Value_Copy(state, destination, &reflectedTypeValue);
                }
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(GET_MEMBER) {
                SZrString *memberName = execution_resolve_function_member_symbol(currentFunction, B1(instruction));
                TZrNativeString memberNativeName;

                opA = &BASE(A1(instruction))->value;
                if (memberName == ZR_NULL) {
                    ZrCore_Debug_RunError(state, "GET_MEMBER: invalid member id");
                } else if (opA->type != ZR_VALUE_TYPE_OBJECT &&
                           opA->type != ZR_VALUE_TYPE_ARRAY &&
                           opA->type != ZR_VALUE_TYPE_STRING) {
                    ZrCore_Debug_RunError(state, "GET_MEMBER: receiver must be an object, array, or string");
                } else if (!execution_member_get_by_name(state, programCounter, opA, memberName, destination)) {
                    memberNativeName = ZrCore_String_GetNativeString(memberName);
                    ZrCore_Debug_RunError(state,
                                          "GET_MEMBER: missing member '%s'",
                                          memberNativeName != ZR_NULL ? memberNativeName : "<unknown>");
                }
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(SET_MEMBER) {
                SZrString *memberName = execution_resolve_function_member_symbol(currentFunction, B1(instruction));
                opA = &BASE(A1(instruction))->value;
                if (memberName == ZR_NULL) {
                    ZrCore_Debug_RunError(state, "SET_MEMBER: invalid member id");
                } else if (opA->type != ZR_VALUE_TYPE_OBJECT && opA->type != ZR_VALUE_TYPE_ARRAY) {
                    ZrCore_Debug_RunError(state, "SET_MEMBER: receiver must be a writable object member");
                } else if (!execution_member_set_by_name(state, programCounter, opA, memberName, destination)) {
                    ZrCore_Debug_RunError(state, "SET_MEMBER: receiver must be a writable object member");
                }
            }
            DONE(1);

#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_GET_MEMBER_SLOT: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_GET_MEMBER_SLOT_BODY();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(GET_MEMBER_SLOT) {
                EXECUTE_GET_MEMBER_SLOT_BODY();
            }
            DONE(1);

#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_SET_MEMBER_SLOT: {
                FAST_PREPARE_DESTINATION();
                EXECUTE_SET_MEMBER_SLOT_BODY();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(SET_MEMBER_SLOT) {
                EXECUTE_SET_MEMBER_SLOT_BODY();
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(META_GET) {
                SZrString *memberName = execution_resolve_function_member_symbol(currentFunction, B1(instruction));
                opA = &BASE(A1(instruction))->value;
                if (memberName == ZR_NULL) {
                    ZrCore_Debug_RunError(state, "META_GET: invalid member id");
                } else if (!execution_meta_get_member(state, opA, memberName, destination)) {
                    ZrCore_Debug_RunError(state, "META_GET: receiver must define property getter");
                }
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(SUPER_META_GET_CACHED) {
                opA = &BASE(A1(instruction))->value;
                if (!execution_meta_get_cached_member(state, currentFunction, B1(instruction), opA, destination)) {
                    ZrCore_Debug_RunError(state, "SUPER_META_GET_CACHED: receiver must define property getter");
                }
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(META_SET) {
                SZrString *memberName = execution_resolve_function_member_symbol(currentFunction, B1(instruction));
                opA = destination;
                opB = &BASE(A1(instruction))->value;
                if (memberName == ZR_NULL) {
                    ZrCore_Debug_RunError(state, "META_SET: invalid member id");
                } else if (!execution_meta_set_member(state, opA, memberName, opB)) {
                    ZrCore_Debug_RunError(state, "META_SET: receiver must define property setter");
                }
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(SUPER_META_SET_CACHED) {
                opA = destination;
                opB = &BASE(A1(instruction))->value;
                if (!execution_meta_set_cached_member(state, currentFunction, B1(instruction), opA, opB)) {
                    ZrCore_Debug_RunError(state, "SUPER_META_SET_CACHED: receiver must define property setter");
                }
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(SUPER_META_GET_STATIC_CACHED) {
                opA = &BASE(A1(instruction))->value;
                if (!execution_meta_get_cached_static_member(state, currentFunction, B1(instruction), opA, destination)) {
                    ZrCore_Debug_RunError(state, "SUPER_META_GET_STATIC_CACHED: receiver must define static property getter");
                }
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(SUPER_META_SET_STATIC_CACHED) {
                opA = destination;
                opB = &BASE(A1(instruction))->value;
                if (!execution_meta_set_cached_static_member(state, currentFunction, B1(instruction), opA, opB)) {
                    ZrCore_Debug_RunError(state, "SUPER_META_SET_STATIC_CACHED: receiver must define static property setter");
                }
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(GET_BY_INDEX) {
                SZrTypeValue stableResult;
                TZrBool resolved = ZR_FALSE;
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if (opA->type != ZR_VALUE_TYPE_OBJECT && opA->type != ZR_VALUE_TYPE_ARRAY) {
                    ZrCore_Debug_RunError(state, "GET_BY_INDEX: receiver must be an object or array");
                } else {
                    ZrCore_Value_ResetAsNull(&stableResult);
                    /*
                     * Keep the hottest readonly-inline index callback off the
                     * generic PROTECT_E wrapper. The callback already consumes
                     * stack-rooted operands directly and does not need the full
                     * generic call-preparation path on steady-state hits.
                     */
                    resolved = ZrCore_Object_TryGetByIndexReadonlyInlineFastStackOperands(
                            state, opA, opB, &stableResult);
                    if (resolved) {
                        if (ZR_UNLIKELY(recordHelpers)) {
                            profileRuntime->helperCounts[ZR_PROFILE_HELPER_GET_BY_INDEX]++;
                        }
                        UPDATE_BASE(callInfo);
                        destination = E(instruction) == ZR_INSTRUCTION_USE_RET_FLAG ? &ret : &BASE(E(instruction))->value;
                    } else if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
                        SAVE_PC(state, callInfo);
                        goto LZrReturning;
                    } else {
                        /*
                         * Preserve stack-rooted receiver/key operands so the
                         * object layer can anchor them directly on known-native
                         * index contract calls instead of first degrading them
                         * into local stable copies.
                         */
                        PROTECT_E(state, callInfo, {
                            resolved = ZrCore_Object_GetByIndexUncheckedStackOperands(
                                    state, opA, opB, &stableResult);
                        });
                        RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                    }
                    if (resolved) {
                        execution_copy_value_fast(state, destination, &stableResult, profileRuntime, recordHelpers);
                    } else {
                        ZrCore_Debug_RunError(state, "GET_BY_INDEX: receiver must be an object or array");
                    }
                }
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(SET_BY_INDEX) {
                TZrBool resolved = ZR_FALSE;
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if (opA->type != ZR_VALUE_TYPE_OBJECT && opA->type != ZR_VALUE_TYPE_ARRAY) {
                    ZrCore_Debug_RunError(state, "SET_BY_INDEX: receiver must be an object or array");
                } else {
                    resolved = ZrCore_Object_TrySetByIndexReadonlyInlineFastStackOperands(
                            state, opA, opB, destination);
                    if (resolved) {
                        if (ZR_UNLIKELY(recordHelpers)) {
                            profileRuntime->helperCounts[ZR_PROFILE_HELPER_SET_BY_INDEX]++;
                        }
                        UPDATE_BASE(callInfo);
                        destination = E(instruction) == ZR_INSTRUCTION_USE_RET_FLAG ? &ret : &BASE(E(instruction))->value;
                    } else if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
                        SAVE_PC(state, callInfo);
                        goto LZrReturning;
                    } else {
                        PROTECT_E(state, callInfo, {
                            resolved = ZrCore_Object_SetByIndexUncheckedStackOperands(
                                    state, opA, opB, destination);
                        });
                        RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                    }
                    if (!resolved) {
                        ZrCore_Debug_RunError(state, "SET_BY_INDEX: receiver must be an object or array");
                    }
                }
            }
            DONE(1);

#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_SUPER_ARRAY_BIND_ITEMS: {
                EXECUTE_SUPER_ARRAY_BIND_ITEMS_BODY();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(SUPER_ARRAY_BIND_ITEMS) {
                EXECUTE_SUPER_ARRAY_BIND_ITEMS_BODY();
            }
            DONE(1);

#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_SUPER_ARRAY_GET_INT: {
                EXECUTE_SUPER_ARRAY_GET_INT_BODY();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(SUPER_ARRAY_GET_INT) {
                EXECUTE_SUPER_ARRAY_GET_INT_BODY();
            }
            DONE(1);

#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_SUPER_ARRAY_GET_INT_ITEMS: {
                EXECUTE_SUPER_ARRAY_GET_INT_ITEMS_BODY();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(SUPER_ARRAY_GET_INT_ITEMS) {
                EXECUTE_SUPER_ARRAY_GET_INT_ITEMS_BODY();
            }
            DONE(1);

#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_SUPER_ARRAY_GET_INT_PLAIN_DEST: {
                EXECUTE_SUPER_ARRAY_GET_INT_PLAIN_DEST_BODY();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(SUPER_ARRAY_GET_INT_PLAIN_DEST) {
                EXECUTE_SUPER_ARRAY_GET_INT_PLAIN_DEST_BODY();
            }
            DONE(1);

#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_SUPER_ARRAY_GET_INT_ITEMS_PLAIN_DEST: {
                EXECUTE_SUPER_ARRAY_GET_INT_ITEMS_PLAIN_DEST_BODY();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(SUPER_ARRAY_GET_INT_ITEMS_PLAIN_DEST) {
                EXECUTE_SUPER_ARRAY_GET_INT_ITEMS_PLAIN_DEST_BODY();
            }
            DONE(1);

#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_SUPER_ARRAY_SET_INT: {
                EXECUTE_SUPER_ARRAY_SET_INT_BODY();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(SUPER_ARRAY_SET_INT) {
                EXECUTE_SUPER_ARRAY_SET_INT_BODY();
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_SUPER_ARRAY_SET_INT_ITEMS: {
                EXECUTE_SUPER_ARRAY_SET_INT_ITEMS_BODY();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(SUPER_ARRAY_SET_INT_ITEMS) {
                EXECUTE_SUPER_ARRAY_SET_INT_ITEMS_BODY();
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_SUPER_ARRAY_FILL_INT4_CONST: {
                EXECUTE_SUPER_ARRAY_FILL_INT4_CONST_BODY();
            }
            DONE_FAST(1);
#endif
            ZR_INSTRUCTION_LABEL(SUPER_ARRAY_ADD_INT) {
                EXECUTE_SUPER_ARRAY_ADD_INT_BODY();
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUPER_ARRAY_ADD_INT4) {
                EXECUTE_SUPER_ARRAY_ADD_INT4_BODY();
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUPER_ARRAY_ADD_INT4_CONST) {
                EXECUTE_SUPER_ARRAY_ADD_INT4_CONST_BODY();
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUPER_ARRAY_FILL_INT4_CONST) {
                EXECUTE_SUPER_ARRAY_FILL_INT4_CONST_BODY();
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(ITER_INIT) {
                SZrTypeValue stableReceiver;
                SZrTypeValue stableResult;
                TZrBool resolved = ZR_FALSE;
                opA = &BASE(A1(instruction))->value;
                stableReceiver = *opA;
                ZrCore_Value_ResetAsNull(&stableResult);
                PROTECT_E(state, callInfo, {
                    resolved = ZrCore_Object_IterInit(state, &stableReceiver, &stableResult);
                });
                RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                if (resolved) {
                    ZrCore_Value_Copy(state, destination, &stableResult);
                } else {
                    ZrCore_Debug_RunError(state, "ITER_INIT: receiver does not satisfy iterable contract");
                }
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(ITER_MOVE_NEXT) {
                SZrTypeValue stableReceiver;
                SZrTypeValue stableResult;
                TZrBool resolved = ZR_FALSE;
                opA = &BASE(A1(instruction))->value;
                if (ZrCore_Object_TryIterMoveNextCachedRawIntArrayFast(state, opA, destination)) {
                    DONE(1);
                }
                stableReceiver = *opA;
                ZrCore_Value_ResetAsNull(&stableResult);
                PROTECT_E(state, callInfo, {
                    resolved = ZrCore_Object_IterMoveNext(state, &stableReceiver, &stableResult);
                });
                RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                if (resolved) {
                    ZrCore_Value_Copy(state, destination, &stableResult);
                } else {
                    ZrCore_Debug_RunError(state, "ITER_MOVE_NEXT: receiver does not satisfy iterator contract");
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(DYN_ITER_INIT) {
                SZrTypeValue stableReceiver;
                SZrTypeValue stableResult;
                TZrBool resolved = ZR_FALSE;
                opA = &BASE(A1(instruction))->value;
                stableReceiver = *opA;
                ZrCore_Value_ResetAsNull(&stableResult);
                PROTECT_E(state, callInfo, {
                    resolved = ZrCore_Object_IterInit(state, &stableReceiver, &stableResult);
                });
                RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                if (resolved) {
                    ZrCore_Value_Copy(state, destination, &stableResult);
                } else {
                    ZrCore_Debug_RunError(state, "DYN_ITER_INIT: receiver does not satisfy dynamic iterable contract");
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(DYN_ITER_MOVE_NEXT) {
                SZrTypeValue stableReceiver;
                SZrTypeValue stableResult;
                TZrBool resolved = ZR_FALSE;
                opA = &BASE(A1(instruction))->value;
                if (ZrCore_Object_TryIterMoveNextCachedRawIntArrayFast(state, opA, destination)) {
                    DONE(1);
                }
                stableReceiver = *opA;
                ZrCore_Value_ResetAsNull(&stableResult);
                PROTECT_E(state, callInfo, {
                    resolved = ZrCore_Object_IterMoveNext(state, &stableReceiver, &stableResult);
                });
                RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                if (resolved) {
                    ZrCore_Value_Copy(state, destination, &stableResult);
                } else {
                    ZrCore_Debug_RunError(state, "DYN_ITER_MOVE_NEXT: receiver does not satisfy dynamic iterator contract");
                }
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_SUPER_ITER_MOVE_NEXT_JUMP_IF_FALSE: {
                FAST_PREPARE_DESTINATION();
                TZrInt16 jumpOffset = (TZrInt16)B1(instruction);
                SZrTypeValue stableReceiver;
                SZrTypeValue stableResult;
                TZrBool resolved = ZR_FALSE;

                opA = &BASE(A1(instruction))->value;
                if (ZrCore_Object_TryIterMoveNextCachedRawIntArrayFast(state, opA, destination)) {
                    if (!execution_is_truthy(state, destination)) {
                        programCounter += jumpOffset;
                        UPDATE_TRAP(callInfo);
                    }
                    DONE_SKIP(2);
                }
                stableReceiver = *opA;
                ZrCore_Value_ResetAsNull(&stableResult);
                PROTECT_E(state, callInfo, {
                    resolved = ZrCore_Object_IterMoveNext(state, &stableReceiver, &stableResult);
                });
                RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                if (resolved) {
                    ZrCore_Value_Copy(state, destination, &stableResult);
                } else {
                    ZrCore_Debug_RunError(state,
                                          "SUPER_ITER_MOVE_NEXT_JUMP_IF_FALSE: receiver does not satisfy iterator "
                                          "contract");
                }

                if (!execution_is_truthy(state, destination)) {
                    programCounter += jumpOffset;
                    UPDATE_TRAP(callInfo);
                }
            }
            DONE_SKIP(2);
#endif
            ZR_INSTRUCTION_LABEL(SUPER_ITER_MOVE_NEXT_JUMP_IF_FALSE) {
                TZrInt16 jumpOffset = (TZrInt16)B1(instruction);
                SZrTypeValue stableReceiver;
                SZrTypeValue stableResult;
                TZrBool resolved = ZR_FALSE;

                opA = &BASE(A1(instruction))->value;
                if (ZrCore_Object_TryIterMoveNextCachedRawIntArrayFast(state, opA, destination)) {
                    if (!execution_is_truthy(state, destination)) {
                        programCounter += jumpOffset;
                        UPDATE_TRAP(callInfo);
                    }
                    DONE_SKIP(2);
                }
                stableReceiver = *opA;
                ZrCore_Value_ResetAsNull(&stableResult);
                PROTECT_E(state, callInfo, {
                    resolved = ZrCore_Object_IterMoveNext(state, &stableReceiver, &stableResult);
                });
                RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                if (resolved) {
                    ZrCore_Value_Copy(state, destination, &stableResult);
                } else {
                    ZrCore_Debug_RunError(state,
                                          "SUPER_ITER_MOVE_NEXT_JUMP_IF_FALSE: receiver does not satisfy iterator "
                                          "contract");
                }

                if (!execution_is_truthy(state, destination)) {
                    programCounter += jumpOffset;
                    UPDATE_TRAP(callInfo);
                }
            }
            DONE_SKIP(2);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE: {
                FAST_PREPARE_DESTINATION();
                TZrInt16 jumpOffset = (TZrInt16)B1(instruction);
                SZrTypeValue stableReceiver;
                SZrTypeValue stableResult;
                TZrBool resolved = ZR_FALSE;

                opA = &BASE(A1(instruction))->value;
                if (ZrCore_Object_TryIterMoveNextCachedRawIntArrayFast(state, opA, destination)) {
                    if (!execution_is_truthy(state, destination)) {
                        programCounter += jumpOffset;
                        UPDATE_TRAP(callInfo);
                    }
                    DONE_SKIP(2);
                }
                stableReceiver = *opA;
                ZrCore_Value_ResetAsNull(&stableResult);
                PROTECT_E(state, callInfo, {
                    resolved = ZrCore_Object_IterMoveNext(state, &stableReceiver, &stableResult);
                });
                RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                if (resolved) {
                    ZrCore_Value_Copy(state, destination, &stableResult);
                } else {
                    ZrCore_Debug_RunError(state,
                                          "SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE: receiver does not satisfy dynamic "
                                          "iterator contract");
                }

                if (!execution_is_truthy(state, destination)) {
                    programCounter += jumpOffset;
                    UPDATE_TRAP(callInfo);
                }
            }
            DONE_SKIP(2);
#endif
            ZR_INSTRUCTION_LABEL(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE) {
                TZrInt16 jumpOffset = (TZrInt16)B1(instruction);
                SZrTypeValue stableReceiver;
                SZrTypeValue stableResult;
                TZrBool resolved = ZR_FALSE;

                opA = &BASE(A1(instruction))->value;
                if (ZrCore_Object_TryIterMoveNextCachedRawIntArrayFast(state, opA, destination)) {
                    if (!execution_is_truthy(state, destination)) {
                        programCounter += jumpOffset;
                        UPDATE_TRAP(callInfo);
                    }
                    DONE_SKIP(2);
                }
                stableReceiver = *opA;
                ZrCore_Value_ResetAsNull(&stableResult);
                PROTECT_E(state, callInfo, {
                    resolved = ZrCore_Object_IterMoveNext(state, &stableReceiver, &stableResult);
                });
                RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                if (resolved) {
                    ZrCore_Value_Copy(state, destination, &stableResult);
                } else {
                    ZrCore_Debug_RunError(state,
                                          "SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE: receiver does not satisfy dynamic "
                                          "iterator contract");
                }

                if (!execution_is_truthy(state, destination)) {
                    programCounter += jumpOffset;
                    UPDATE_TRAP(callInfo);
                }
            }
            DONE_SKIP(2);

            ZR_INSTRUCTION_LABEL(ITER_CURRENT) {
                SZrTypeValue stableReceiver;
                SZrTypeValue stableResult;
                TZrBool resolved = ZR_FALSE;
                opA = &BASE(A1(instruction))->value;
                if (ZrCore_Object_TryIterCurrentCachedMemberFast(state, opA, destination)) {
                    DONE(1);
                }
                stableReceiver = *opA;
                ZrCore_Value_ResetAsNull(&stableResult);
                PROTECT_E(state, callInfo, {
                    resolved = ZrCore_Object_IterCurrent(state, &stableReceiver, &stableResult);
                });
                RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                if (resolved) {
                    ZrCore_Value_Copy(state, destination, &stableResult);
                } else {
                    ZrCore_Debug_RunError(state, "ITER_CURRENT: receiver does not satisfy iterator contract");
                }
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_JUMP: { JUMP_FAST(callInfo, instruction, 0); }
            DONE_AFTER_TRAP_FAST_ONE();
#endif
            ZR_INSTRUCTION_LABEL(JUMP) { JUMP(callInfo, instruction, 0); }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_JUMP_IF: {
                SZrTypeValue *condValue = &BASE(E(instruction))->value;
                TZrBool condition = execution_is_truthy(state, condValue);

                if (!condition) {
                    JUMP_FAST(callInfo, instruction, 0);
                }
            }
            DONE_AFTER_TRAP_FAST_ONE();
#endif
            ZR_INSTRUCTION_LABEL(JUMP_IF) {
                // JUMP_IF 指令格式：
                // operandExtra (E) = condSlot (条件值的栈槽)
                // operand2[0] (A2) = offset (相对跳转偏移量)
                // 如果条件为假，跳转到 else 分支；如果条件为真，继续执行 then 分支
                SZrTypeValue *condValue = &BASE(E(instruction))->value;
                TZrBool condition = execution_is_truthy(state, condValue);
                
                // 如果条件为假，跳转到 else 分支
                if (!condition) {
                    JUMP(callInfo, instruction, 0);
                }
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_JUMP_IF_GREATER_SIGNED: {
                EXECUTE_JUMP_IF_GREATER_SIGNED_BODY(JUMP1_16_FAST, callInfo);
            }
            DONE_AFTER_TRAP_FAST_ONE();
#endif
            ZR_INSTRUCTION_LABEL(JUMP_IF_GREATER_SIGNED) {
                // JUMP_IF_GREATER_SIGNED 指令格式：
                // operandExtra (E) = leftSlot
                // operand1[0] (A1) = rightSlot
                // operand1[1] (B1) = 16-bit 相对跳转偏移量
                // 如果 left > right，则跳转
                EXECUTE_JUMP_IF_GREATER_SIGNED_BODY(JUMP1_16, callInfo);
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_JUMP_IF_NOT_EQUAL_SIGNED: {
                EXECUTE_JUMP_IF_NOT_EQUAL_SIGNED_BODY(JUMP1_16_FAST, callInfo);
            }
            DONE_AFTER_TRAP_FAST_ONE();
#endif
            ZR_INSTRUCTION_LABEL(JUMP_IF_NOT_EQUAL_SIGNED) {
                // operandExtra (E) = leftSlot
                // operand1[0] (A1) = rightSlot
                // operand1[1] (B1) = 16-bit relative jump offset
                // Fused LOGICAL_EQUAL_SIGNED + JUMP_IF: jump when equality is false.
                EXECUTE_JUMP_IF_NOT_EQUAL_SIGNED_BODY(JUMP1_16, callInfo);
            }
            DONE(1);
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
LZrFastInstruction_JUMP_IF_NOT_EQUAL_SIGNED_CONST: {
                EXECUTE_JUMP_IF_NOT_EQUAL_SIGNED_CONST_BODY(JUMP1_16_FAST, callInfo);
            }
            DONE_AFTER_TRAP_FAST_ONE();
#endif
            ZR_INSTRUCTION_LABEL(JUMP_IF_NOT_EQUAL_SIGNED_CONST) {
                // operandExtra (E) = leftSlot
                // operand1[0] (A1) = signed int constant index
                // operand1[1] (B1) = 16-bit relative jump offset
                // Fused LOGICAL_EQUAL_SIGNED_CONST + JUMP_IF: jump when equality is false.
                EXECUTE_JUMP_IF_NOT_EQUAL_SIGNED_CONST_BODY(JUMP1_16, callInfo);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(CREATE_CLOSURE) {
                // CREATE_CLOSURE 指令格式：
                // operandExtra (E) = destSlot (destination已通过E(instruction)定义)
                // operand1[0] (A1) = functionConstantIndex
                // operand1[1] (B1) = closureVarCount
                TZrSize functionConstantIndex = A1(instruction);
                SZrTypeValue *functionConstant = CONST(functionConstantIndex);
                // 从常量池获取函数对象
                // 注意：编译器将SZrFunction*存储为ZR_VALUE_TYPE_CLOSURE类型，但value.object实际指向SZrFunction*
                SZrFunction *function = ZR_NULL;
                if (functionConstant->type == ZR_VALUE_TYPE_CLOSURE ||
                    functionConstant->type == ZR_VALUE_TYPE_FUNCTION) {
                    // 从raw object获取实际的函数对象
                    SZrRawObject *rawObject = functionConstant->value.object;
                    if (rawObject != ZR_NULL && rawObject->type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
                        function = ZR_CAST(SZrFunction *, rawObject);
                    }
                }
                if (function != ZR_NULL) {
                    SZrClosureValue **parentClosureValues =
                            closure != ZR_NULL ? closure->closureValuesExtend : ZR_NULL;
                    if (!execution_try_reuse_preinstalled_top_level_closure(state,
                                                                            closure,
                                                                            function,
                                                                            base,
                                                                            destination)) {
                        execution_prepare_destination_for_direct_store_no_profile(state, destination);
                        ZrCore_Closure_PushToStack(state, function, parentClosureValues, base, BASE(E(instruction)));
                        destination->type = ZR_VALUE_TYPE_CLOSURE;
                        destination->isGarbageCollectable = ZR_TRUE;
                        destination->isNative = ZR_FALSE;
                    }
                } else {
                    // 类型错误或函数为NULL
                    execution_prepare_destination_for_direct_store_no_profile(state, destination);
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(CREATE_OBJECT) {
                // 创建空对象
                SZrObject *object = ZrCore_Object_New(state, ZR_NULL);
                execution_prepare_destination_for_direct_store_no_profile(state, destination);
                if (object != ZR_NULL) {
                    ZrCore_Object_Init(state, object);
                    ZrCore_Value_InitAsRawObject(state, destination, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
                } else {
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(CREATE_ARRAY) {
                // 创建空数组对象
                SZrObject *array = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
                execution_prepare_destination_for_direct_store_no_profile(state, destination);
                if (array != ZR_NULL) {
                    ZrCore_Object_Init(state, array);
                    ZrCore_Value_InitAsRawObject(state, destination, ZR_CAST_RAW_OBJECT_AS_SUPER(array));
                } else {
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(OWN_UNIQUE) {
                opA = &BASE(A1(instruction))->value;
                if (!ZrCore_Ownership_UniqueValue(state, destination, opA)) {
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(OWN_BORROW) {
                opA = &BASE(A1(instruction))->value;
                if (!ZrCore_Ownership_BorrowValue(state, destination, opA)) {
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(OWN_LOAN) {
                opA = &BASE(A1(instruction))->value;
                if (!ZrCore_Ownership_LoanValue(state, destination, opA)) {
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(OWN_SHARE) {
                opA = &BASE(A1(instruction))->value;
                if (!ZrCore_Ownership_ShareValue(state, destination, opA)) {
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(OWN_WEAK) {
                opA = &BASE(A1(instruction))->value;
                if (!ZrCore_Ownership_WeakValue(state, destination, opA)) {
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(OWN_DETACH) {
                opA = &BASE(A1(instruction))->value;
                if (!ZrCore_Ownership_DetachValue(state, destination, opA)) {
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(OWN_UPGRADE) {
                opA = &BASE(A1(instruction))->value;
                if (!ZrCore_Ownership_UpgradeValue(state, destination, opA)) {
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(OWN_RELEASE) {
                opA = &BASE(A1(instruction))->value;
                ZrCore_Ownership_ReleaseValue(state, opA);
                execution_prepare_destination_for_direct_store_no_profile(state, destination);
                ZrCore_Value_ResetAsNull(destination);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MARK_TO_BE_CLOSED) {
                TZrSize closeSlot = E(instruction);
                TZrStackValuePointer closePointer = BASE(closeSlot);
                ZrCore_Closure_ToBeClosedValueClosureNew(state, closePointer);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(CLOSE_SCOPE) {
                close_scope_cleanup_registrations(state, E(instruction));
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TRY) {
                if (!execution_push_exception_handler(state, callInfo, E(instruction))) {
                    if (!ZrCore_Exception_NormalizeStatus(state, ZR_THREAD_STATUS_MEMORY_ERROR)) {
                        ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_MEMORY_ERROR);
                    }
                    ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_MEMORY_ERROR);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(END_TRY) {
                SZrVmExceptionHandlerState *handlerState = execution_find_handler_state(state, callInfo, E(instruction));
                SZrFunction *handlerFunction = ZR_NULL;
                const SZrFunctionExceptionHandlerInfo *handlerInfo =
                        execution_lookup_exception_handler_info(state, handlerState, &handlerFunction);

                if (handlerState != ZR_NULL) {
                    if (handlerInfo != ZR_NULL && handlerInfo->hasFinally) {
                        handlerState->phase = ZR_VM_EXCEPTION_HANDLER_PHASE_FINALLY;
                    } else {
                        execution_pop_exception_handler(state, handlerState);
                    }
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(THROW) {
                SZrTypeValue payload;

                SAVE_PC(state, callInfo);
                execution_clear_pending_control(state);
                payload = BASE(E(instruction))->value;
                if (!ZrCore_Exception_NormalizeThrownValue(state,
                                                          &payload,
                                                          callInfo,
                                                          ZR_THREAD_STATUS_RUNTIME_ERROR)) {
                    if (!ZrCore_Exception_NormalizeStatus(state, ZR_THREAD_STATUS_EXCEPTION_ERROR)) {
                        ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_EXCEPTION_ERROR);
                    }
                    ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_EXCEPTION_ERROR);
                }

                if (execution_unwind_exception_to_handler(state, &callInfo)) {
                    goto LZrReturning;
                }

                ZrCore_Exception_Throw(state, state->currentExceptionStatus);
                ZR_ABORT();
            }
            ZR_INSTRUCTION_LABEL(CATCH) {
                if (state->hasCurrentException) {
                    ZrCore_Value_Copy(state, destination, &state->currentException);
                    ZrCore_Exception_ClearCurrent(state);
                } else {
                    ZrCore_Value_ResetAsNull(destination);
                }
                execution_clear_pending_control(state);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(END_FINALLY) {
                SZrVmExceptionHandlerState *handlerState = execution_find_handler_state(state, callInfo, E(instruction));
                SZrCallInfo *resumeCallInfo;
                TZrStackValuePointer targetSlot;

                if (handlerState != ZR_NULL) {
                    execution_pop_exception_handler(state, handlerState);
                }

                switch (state->pendingControl.kind) {
                    case ZR_VM_PENDING_CONTROL_NONE:
                        break;
                    case ZR_VM_PENDING_CONTROL_EXCEPTION:
                        resumeCallInfo = state->pendingControl.callInfo != ZR_NULL
                                                 ? state->pendingControl.callInfo
                                                 : callInfo;
                        callInfo = resumeCallInfo;
                        if (execution_unwind_exception_to_handler(state, &callInfo)) {
                            goto LZrReturning;
                        }
                        ZrCore_Exception_Throw(state, state->currentExceptionStatus);
                        break;
                    case ZR_VM_PENDING_CONTROL_RETURN:
                    case ZR_VM_PENDING_CONTROL_BREAK:
                    case ZR_VM_PENDING_CONTROL_CONTINUE:
                        resumeCallInfo = state->pendingControl.callInfo != ZR_NULL
                                                 ? state->pendingControl.callInfo
                                                 : callInfo;
                        callInfo = resumeCallInfo;
                        if (execution_resume_pending_via_outer_finally(state, &callInfo)) {
                            goto LZrReturning;
                        }

                        if (state->pendingControl.kind == ZR_VM_PENDING_CONTROL_RETURN &&
                            state->pendingControl.hasValue &&
                            resumeCallInfo != ZR_NULL &&
                            resumeCallInfo->functionBase.valuePointer != ZR_NULL) {
                            targetSlot = resumeCallInfo->functionBase.valuePointer + 1 + state->pendingControl.valueSlot;
                            ZrCore_Value_Copy(state, &targetSlot->value, &state->pendingControl.value);
                        }

                        if (execution_jump_to_instruction_offset(state,
                                                                 &callInfo,
                                                                 resumeCallInfo,
                                                                 state->pendingControl.targetInstructionOffset)) {
                            execution_clear_pending_control(state);
                            goto LZrReturning;
                        }

                        execution_clear_pending_control(state);
                        if (!ZrCore_Exception_NormalizeStatus(state, ZR_THREAD_STATUS_EXCEPTION_ERROR)) {
                            ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_EXCEPTION_ERROR);
                        }
                        ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_EXCEPTION_ERROR);
                        break;
                    default:
                        execution_clear_pending_control(state);
                        break;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SET_PENDING_RETURN) {
                execution_set_pending_control(state,
                                              ZR_VM_PENDING_CONTROL_RETURN,
                                              callInfo,
                                              (TZrMemoryOffset)A2(instruction),
                                              E(instruction),
                                              &BASE(E(instruction))->value);
                if (execution_resume_pending_via_outer_finally(state, &callInfo)) {
                    goto LZrReturning;
                }
                if (execution_jump_to_instruction_offset(state,
                                                         &callInfo,
                                                         callInfo,
                                                         state->pendingControl.targetInstructionOffset)) {
                    execution_clear_pending_control(state);
                    goto LZrReturning;
                }
                execution_clear_pending_control(state);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SET_PENDING_BREAK) {
                execution_set_pending_control(state,
                                              ZR_VM_PENDING_CONTROL_BREAK,
                                              callInfo,
                                              (TZrMemoryOffset)A2(instruction),
                                              0,
                                              ZR_NULL);
                if (execution_resume_pending_via_outer_finally(state, &callInfo)) {
                    goto LZrReturning;
                }
                if (execution_jump_to_instruction_offset(state,
                                                         &callInfo,
                                                         callInfo,
                                                         state->pendingControl.targetInstructionOffset)) {
                    execution_clear_pending_control(state);
                    goto LZrReturning;
                }
                execution_clear_pending_control(state);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SET_PENDING_CONTINUE) {
                execution_set_pending_control(state,
                                              ZR_VM_PENDING_CONTROL_CONTINUE,
                                              callInfo,
                                              (TZrMemoryOffset)A2(instruction),
                                              0,
                                              ZR_NULL);
                if (execution_resume_pending_via_outer_finally(state, &callInfo)) {
                    goto LZrReturning;
                }
                if (execution_jump_to_instruction_offset(state,
                                                         &callInfo,
                                                         callInfo,
                                                         state->pendingControl.targetInstructionOffset)) {
                    execution_clear_pending_control(state);
                    goto LZrReturning;
                }
                execution_clear_pending_control(state);
            }
            DONE(1);
            ZR_INSTRUCTION_DEFAULT() {
                // todo: error unreachable
                char message[ZR_RUNTIME_DIAGNOSTIC_BUFFER_LENGTH];
                sprintf(message, "Not implemented op code:%d at offset %d\n", instruction.instruction.operationCode,
                        (int) (instructionsEnd - programCounter));
                ZrCore_Debug_RunError(state, message);
            }
            DONE(1);
        }
    }

LZrExecutionDone:
    ;

#undef DONE
#undef DONE_FAST
#undef FETCH_PREPARE_OR_BREAK
#undef FETCH_PREPARE_FAST_ONLY
#undef FETCH_PREPARE_SHARED
#undef FETCH_DEBUG_BASE_SYNC
#undef FAST_PREPARE_DESTINATION_FROM_OFFSET
#undef FAST_PREPARE_DESTINATION
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
#undef DONE_AFTER_TRAP_FAST_ONE
#undef FETCH_PREPARE_OR_GOTO_DONE
#undef ZR_FAST_INSTRUCTION_DISPATCH
#endif
#undef EXECUTE_GET_STACK_BODY
#undef EXECUTE_GET_STACK_BODY_FAST
#undef EXECUTE_SET_STACK_BODY
#undef EXECUTE_SET_STACK_BODY_FAST
#undef EXECUTE_GET_CONSTANT_BODY
#undef EXECUTE_GET_CONSTANT_BODY_FAST
#undef EXECUTE_RESET_STACK_NULL_BODY
#undef EXECUTE_GET_MEMBER_SLOT_BODY
#undef EXECUTE_SET_MEMBER_SLOT_BODY
#undef EXECUTE_MATERIALIZE_CONSTANT_SLOT
#undef EXECUTE_MATERIALIZE_STACK_SLOT
#undef EXECUTE_ADD_INT_BODY
#undef EXECUTE_ADD_INT_BODY_PLAIN_DEST
#undef EXECUTE_ADD_INT_CONST_BODY
#undef EXECUTE_ADD_INT_CONST_BODY_PLAIN_DEST
#undef EXECUTE_TYPED_SIGNED_BINARY_BODY
#undef EXECUTE_TYPED_SIGNED_BINARY_BODY_PLAIN_DEST
#undef EXECUTE_TYPED_SIGNED_CONST_BINARY_BODY
#undef EXECUTE_TYPED_SIGNED_CONST_BINARY_BODY_PLAIN_DEST
#undef EXECUTE_ADD_SIGNED_LOAD_CONST_BODY
#undef EXECUTE_ADD_SIGNED_LOAD_STACK_CONST_BODY
#undef EXECUTE_ADD_SIGNED_LOAD_STACK_BODY
#undef EXECUTE_ADD_SIGNED_LOAD_STACK_LOAD_CONST_BODY
#undef EXECUTE_SUB_SIGNED_LOAD_CONST_BODY
#undef EXECUTE_SUB_SIGNED_LOAD_STACK_CONST_BODY
#undef EXECUTE_TYPED_UNSIGNED_BINARY_BODY
#undef EXECUTE_TYPED_UNSIGNED_BINARY_BODY_PLAIN_DEST
#undef EXECUTE_TYPED_UNSIGNED_CONST_BINARY_BODY
#undef EXECUTE_TYPED_UNSIGNED_CONST_BINARY_BODY_PLAIN_DEST
#undef EXECUTE_TYPED_EQUALITY_BOOL_BODY
#undef EXECUTE_TYPED_EQUALITY_SIGNED_BODY
#undef EXECUTE_TYPED_EQUALITY_SIGNED_CONST_BODY
#undef EXECUTE_TYPED_EQUALITY_UNSIGNED_BODY
#undef EXECUTE_TYPED_EQUALITY_FLOAT_BODY
#undef EXECUTE_TYPED_EQUALITY_STRING_BODY
#undef EXECUTE_SUB_INT_BODY
#undef EXECUTE_SUB_INT_BODY_PLAIN_DEST
#undef EXECUTE_SUB_INT_CONST_BODY
#undef EXECUTE_SUB_INT_CONST_BODY_PLAIN_DEST
#undef EXECUTE_MUL_SIGNED_BODY
#undef EXECUTE_MUL_SIGNED_BODY_PLAIN_DEST
#undef EXECUTE_MUL_SIGNED_CONST_BODY
#undef EXECUTE_MUL_SIGNED_CONST_BODY_PLAIN_DEST
#undef EXECUTE_MUL_SIGNED_LOAD_CONST_BODY
#undef EXECUTE_MUL_SIGNED_LOAD_STACK_CONST_BODY
#undef EXECUTE_MUL_UNSIGNED_CONST_BODY
#undef EXECUTE_MUL_UNSIGNED_CONST_BODY_PLAIN_DEST
#undef EXECUTE_DIV_SIGNED_CONST_BODY
#undef EXECUTE_DIV_SIGNED_CONST_BODY_PLAIN_DEST
#undef EXECUTE_DIV_SIGNED_LOAD_CONST_BODY
#undef EXECUTE_DIV_SIGNED_LOAD_STACK_CONST_BODY
#undef EXECUTE_DIV_UNSIGNED_CONST_BODY
#undef EXECUTE_DIV_UNSIGNED_CONST_BODY_PLAIN_DEST
#undef EXECUTE_MOD_SIGNED_CONST_BODY
#undef EXECUTE_MOD_SIGNED_CONST_BODY_PLAIN_DEST
#undef EXECUTE_MOD_SIGNED_LOAD_CONST_BODY
#undef EXECUTE_MOD_SIGNED_LOAD_STACK_CONST_BODY
#undef EXECUTE_MOD_UNSIGNED_CONST_BODY
#undef EXECUTE_MOD_UNSIGNED_CONST_BODY_PLAIN_DEST
#undef EXECUTE_LOGICAL_LESS_EQUAL_SIGNED_BODY
#undef EXECUTE_JUMP_IF_GREATER_SIGNED_BODY
#undef EXECUTE_JUMP_IF_NOT_EQUAL_SIGNED_BODY
#undef EXECUTE_JUMP_IF_NOT_EQUAL_SIGNED_CONST_BODY
#undef EXECUTE_SUPER_ARRAY_BIND_ITEMS_BODY
#undef EXECUTE_SUPER_ARRAY_GET_INT_BODY
#undef EXECUTE_SUPER_ARRAY_GET_INT_ITEMS_BODY
#undef EXECUTE_SUPER_ARRAY_GET_INT_PLAIN_DEST_BODY
#undef EXECUTE_SUPER_ARRAY_GET_INT_ITEMS_PLAIN_DEST_BODY
#undef EXECUTE_SUPER_ARRAY_SET_INT_BODY
#undef EXECUTE_SUPER_ARRAY_SET_INT_ITEMS_BODY
#undef EXECUTE_SUPER_ARRAY_ADD_INT_BODY
#undef EXECUTE_SUPER_ARRAY_ADD_INT4_BODY
#undef EXECUTE_SUPER_ARRAY_ADD_INT4_CONST_BODY
#undef EXECUTE_SUPER_ARRAY_FILL_INT4_CONST_BODY
#undef ZrCore_Function_PreCall
#undef ZrCore_Stack_GetValue
#undef ZrCore_Value_Copy
#undef ZrCore_Debug_RunError
}
